#include "WorldStateServerSession.h"

#include "PerformanceMan.h"
#include "AudioMan.h"
#include "MessageIdentifiers.h"
#include "Activity.h"
#include "ActivityMan.h"
#include "GameActivity.h"
#include "GAScripted.h"
#include "ACraft.h"
#include "PresetMan.h"
#include "ThreadMan.h"
#include "BuyMenuGUI.h"
#include "SceneEditorGUI.h"
#include "MetaMan.h"
#include "NetworkMessages.h"
#include "SceneMan.h"
#include "SLTerrain.h"
#include "UInputMan.h"
#include "System/UTF8.h"
#include "System/WorldStateTerrainTest.h"
#include "WorldStateSoundCapture.h"
#include "TimerMan.h"
#include "SoundPlaybackClock.h"
#include "WorldStateAttachments.h"
#include "Actor.h"
#include "MovableMan.h"
#include <limits>

#include <algorithm>
#include <utility>

using namespace RTE;

bool WorldStateServerSession::Start(const std::string& bindAddress, unsigned short port, unsigned short maxPlayers, const std::string& logPath, bool truncateLog, bool reserveLocalPlayer) {
	Stop();
	m_Log.open(UTF8::PathFromString(logPath), std::ios::out | (truncateLog ? std::ios::trunc : std::ios::app));
	if (!m_Log || !m_Transport.StartServer(port, maxPlayers, bindAddress.c_str())) {
		Log("ERROR: could not bind world-state server socket");
		Stop();
		return false;
	}
	g_SceneMan.SetWorldStateTerrainTrackingEnabled(true);
	m_LastBroadcastTick = 0;
	m_Sequence = 0;
	m_SnapshotBroadcastCount = 0;
	m_SnapshotCaptureWindowMicroseconds = 0;
	m_SnapshotTransportWindowMicroseconds = 0;
	m_SnapshotEncodeWindowMicroseconds = 0;
	m_SnapshotCompressWindowMicroseconds = 0;
	m_SnapshotQueueWindowMicroseconds = 0;
	m_SnapshotPayloadWindowBytes = 0;
	m_SnapshotCaptureWindowSamples = 0;
	m_SnapshotPayloadWindowSamples = 0;
	m_LastSnapshotObjectCount = 0;
	m_MaxSnapshotPayloadBytes = 0;
	m_MaxSnapshotTransportMicroseconds = 0;
	m_MaxSnapshotEncodeMicroseconds = 0;
	m_MaxSnapshotCompressMicroseconds = 0;
	m_MaxSnapshotQueueMicroseconds = 0;
	m_InputCommandCount = 0;
	m_LastTerrainSceneRevision = 0;
	m_TerrainPatchBroadcastCount = 0;
	m_PendingTerrainPatches.clear();
	m_LastInputSequenceByClient.clear();
	m_LastInputTimeByClient.clear();
	m_LastPurchaseByClient.clear();
	m_LastPolicyByClient.clear(); m_LastPolicyTick=0; m_PolicySequence=0;
	m_LastPurchaseTimeByClient.clear();
	m_PlayerSlotByClient.clear();
	m_ClientAddresses.clear();
	m_ReportedPlayerSlots.clear();
	m_ResetVotesByClient.clear();
	m_RestartVotesByClient.clear();
	m_PreviousResetInputByClient.clear();
	m_PreviousRestartInputByClient.clear();
	m_PendingInputPackets.clear();
	m_LastActivityVoteAction = {};
	m_InputSlotsInUse.fill(false);
	// Graphical hosts keep local PlayerOne independent from network peers.
	// Headless hosts can assign PlayerOne to a remote connection.
	m_ReserveLocalPlayer = reserveLocalPlayer;
	m_InputSlotsInUse[Players::PlayerOne] = reserveLocalPlayer;
	m_ConnectedClients = 0;
	GetWorldStateSoundCapture().Begin();
	for (std::uint8_t bus = 0; bus < 3; ++bus) {
		WorldStateSoundProtocol::Event pitch; pitch.Type = WorldStateSoundProtocol::Action::GlobalPitch;
		pitch.Bus = static_cast<std::uint8_t>(1U << bus); pitch.Rate = g_AudioMan.GetBusPitches()[bus];
		GetWorldStateSoundCapture().Global(pitch);
	}
	WorldStateSoundProtocol::Event pause; pause.Type = WorldStateSoundProtocol::Action::BusPause;
	pause.Paused = g_AudioMan.AreIngameSoundsPaused(); GetWorldStateSoundCapture().Global(pause);
	m_SoundMixBaselinePending = true; m_LastSoundSceneRevision = 0;
	m_PendingSoundBaselines.clear();
	m_DebugTerrainMutationSmokeEnabled = false;
	m_DebugTerrainMutationSmokeAttempted = false;
	m_DebugTerrainMutationProbeLocated = false;
	m_DebugTerrainMutationBaselineSent = false;
	m_DebugTerrainMutationSmokePassed = false;
	m_DebugTerrainMutationPatchSent = false;
	m_LoggedWaitingForWorldState = false;
	m_TerrainBaselinePending = false;
	m_DebugEmptyWorldSnapshotSmokeEnabled = false;
	Log("INFO: world-state host listening on " + bindAddress + ":" + std::to_string(m_Transport.GetBoundPort()) +
	    " (max clients " + std::to_string(maxPlayers) + ")");
	return true;
}

void WorldStateServerSession::Stop() {
	m_LastConstructionByClient.clear();
	m_ConstructionOwners.clear(); m_ConstructionOwnerRevision = 0;
	m_LastConstructionTimeByClient.clear();
	m_PendingSoundBaselines.clear();
	GetWorldStateSoundCapture().End();
	g_SceneMan.SetWorldStateTerrainTrackingEnabled(false);
	m_PendingTerrainPatches.clear();
	m_LastTerrainSceneRevision = 0;
	for (const auto& [clientAddress, player] : m_PlayerSlotByClient) {
		g_UInputMan.ClearNetworkInputState(player);
	}
	m_PlayerSlotByClient.clear();
	m_ClientAddresses.clear();
	m_ReportedPlayerSlots.clear();
	m_LastInputSequenceByClient.clear();
	m_LastInputTimeByClient.clear();
	m_ResetVotesByClient.clear();
	m_RestartVotesByClient.clear();
	m_PreviousResetInputByClient.clear();
	m_PreviousRestartInputByClient.clear();
	m_PendingInputPackets.clear();
	m_TerrainBaselinePending = false;
	m_InputSlotsInUse.fill(false);
	if (m_SnapshotCaptureWindowSamples > 0 || m_SnapshotPayloadWindowSamples > 0) {
		const double averageCaptureMilliseconds = m_SnapshotCaptureWindowSamples > 0
		                                             ? static_cast<double>(m_SnapshotCaptureWindowMicroseconds) / m_SnapshotCaptureWindowSamples / 1000.0
		                                             : 0.0;
		const double averagePayloadBytes = m_SnapshotPayloadWindowSamples > 0 ? static_cast<double>(m_SnapshotPayloadWindowBytes) / m_SnapshotPayloadWindowSamples : 0.0;
		const double averageTransportMilliseconds = m_SnapshotPayloadWindowSamples > 0
		                                               ? static_cast<double>(m_SnapshotTransportWindowMicroseconds) / m_SnapshotPayloadWindowSamples / 1000.0
		                                               : 0.0;
		const double averageEncodeMilliseconds = m_SnapshotPayloadWindowSamples > 0
		                                            ? static_cast<double>(m_SnapshotEncodeWindowMicroseconds) / m_SnapshotPayloadWindowSamples / 1000.0
		                                            : 0.0;
		const double averageCompressMilliseconds = m_SnapshotPayloadWindowSamples > 0
		                                               ? static_cast<double>(m_SnapshotCompressWindowMicroseconds) / m_SnapshotPayloadWindowSamples / 1000.0
		                                               : 0.0;
		const double averageQueueMilliseconds = m_SnapshotPayloadWindowSamples > 0
		                                           ? static_cast<double>(m_SnapshotQueueWindowMicroseconds) / m_SnapshotPayloadWindowSamples / 1000.0
		                                           : 0.0;
		Log("INFO: final snapshot capture average over " + std::to_string(m_SnapshotCaptureWindowSamples) + " samples=" +
		    std::to_string(averageCaptureMilliseconds) + " ms; objects=" + std::to_string(m_LastSnapshotObjectCount) +
		    "; average encode/compress/queue=" + std::to_string(averageEncodeMilliseconds) + "/" + std::to_string(averageCompressMilliseconds) + "/" +
		    std::to_string(averageQueueMilliseconds) + " ms (total " + std::to_string(averageTransportMilliseconds) + "); peak stages=" +
		    std::to_string(m_MaxSnapshotEncodeMicroseconds / 1000.0) + "/" + std::to_string(m_MaxSnapshotCompressMicroseconds / 1000.0) + "/" +
		    std::to_string(m_MaxSnapshotQueueMicroseconds / 1000.0) + " ms (total " + std::to_string(m_MaxSnapshotTransportMicroseconds / 1000.0) + " ms)" +
		    "; average compressed payload bytes over " + std::to_string(m_SnapshotPayloadWindowSamples) + " broadcasts=" + std::to_string(averagePayloadBytes) +
		    "; peak compressed payload bytes=" +
		    std::to_string(m_MaxSnapshotPayloadBytes) + "; estimated snapshot payload bandwidth/client=" +
		    std::to_string(averagePayloadBytes * 20.0 * 8.0 / 1000.0) + " kbit/s at 20 Hz (excluding RakNet/IP/UDP overhead)");
		m_SnapshotCaptureWindowMicroseconds = 0;
		m_SnapshotTransportWindowMicroseconds = 0;
		m_SnapshotEncodeWindowMicroseconds = 0;
		m_SnapshotCompressWindowMicroseconds = 0;
		m_SnapshotQueueWindowMicroseconds = 0;
		m_SnapshotPayloadWindowBytes = 0;
		m_SnapshotCaptureWindowSamples = 0;
		m_SnapshotPayloadWindowSamples = 0;
		m_MaxSnapshotPayloadBytes = 0;
		m_MaxSnapshotTransportMicroseconds = 0;
		m_MaxSnapshotEncodeMicroseconds = 0;
		m_MaxSnapshotCompressMicroseconds = 0;
		m_MaxSnapshotQueueMicroseconds = 0;
	}
	if (m_Transport.IsStarted()) {
		m_Transport.Stop();
		Log("INFO: world-state host stopped");
	}
	if (m_Log.is_open()) {
		m_Log.close();
	}
	m_ConnectedClients = 0;
}

void WorldStateServerSession::EnableDebugTerrainMutationSmoke() {
	m_DebugTerrainMutationSmokeEnabled = true;
}

std::vector<int> WorldStateServerSession::GetAssignedPlayerSlots() const {
	std::vector<int> playerSlots;
	playerSlots.reserve(m_PlayerSlotByClient.size());
	for (const auto& clientPlayer : m_PlayerSlotByClient) {
		playerSlots.push_back(clientPlayer.second);
	}
	std::sort(playerSlots.begin(), playerSlots.end());
	return playerSlots;
}

std::size_t WorldStateServerSession::ConfigureActivityPlayers(Activity& activity, bool beforeStart) {
	std::size_t configuredPlayers = 0;
	std::vector<int> addedPlayers;
	auto* gameActivity = dynamic_cast<GameActivity*>(&activity);
	const auto assignedSlots = GetAssignedPlayerSlots();
	if (beforeStart) {
		for (int player = Players::PlayerOne; player < Players::MaxPlayerCount; ++player) {
			if (m_ReserveLocalPlayer && player == Players::PlayerOne) continue;
			if (activity.PlayerActive(player) && activity.PlayerHuman(player) &&
			    std::find(assignedSlots.begin(), assignedSlots.end(), player) == assignedSlots.end()) activity.DeactivatePlayer(player);
		}
	}
	for (const int playerSlot : assignedSlots) {
		if (playerSlot < (m_ReserveLocalPlayer ? Players::PlayerTwo : Players::PlayerOne) || playerSlot >= Players::MaxPlayerCount ||
		    (activity.PlayerActive(playerSlot) && activity.PlayerHuman(playerSlot))) {
			continue;
		}
		if (activity.GetHumanCount() >= activity.GetMaxPlayerSupport()) {
			continue;
		}

		int playerTeam = activity.GetTeamOfPlayer(playerSlot);
		if (playerTeam < Activity::Teams::TeamOne || playerTeam >= Activity::Teams::MaxTeamCount) {
			playerTeam = Activity::Teams::TeamOne;
		}
		// Honor TeamOfPlayerN from the Activity. Inventing an inactive team can
		// put clients outside the teams understood by a scripted game mode.

		if (gameActivity && gameActivity->TeamIsCPU(playerTeam)) {
			gameActivity->SetTeamIsCPU(playerTeam, false);
		}
		activity.AddPlayer(playerSlot, true, playerTeam, 0.0F);
		addedPlayers.push_back(playerSlot);
		++configuredPlayers;
	}

	if (configuredPlayers > 0 || beforeStart) {
		activity.SetupPlayers();
	}
	if (!beforeStart && gameActivity) {
		for (int player : addedPlayers) {
			gameActivity->InitializeHumanPlayer(player);
			if (auto* scripted = dynamic_cast<GAScripted*>(gameActivity)) {
				const std::string playerArgument = std::to_string(player);
				if (scripted->RunLuaFunction("OnPlayerJoined", {}, {playerArgument}) < 0) {
					Log("ERROR: OnPlayerJoined failed for player slot " + playerArgument);
				}
			}
		}
	}
	if (beforeStart && gameActivity && activity.GetTeamCount() < activity.GetMinTeamsRequired()) {
		for (int team = Activity::Teams::TeamOne; team < Activity::Teams::MaxTeamCount; ++team) {
			if (!activity.TeamActive(team)) {
				gameActivity->SetCPUTeam(team);
				break;
			}
		}
	}
	return configuredPlayers;
}

void WorldStateServerSession::RefreshClientAssignments(std::uint32_t simulationTick) {
	const Activity* activity = g_ActivityMan.GetActivity();
	for (const auto& [clientAddress, peerAddress] : m_ClientAddresses) {
		const auto reserved = m_PlayerSlotByClient.find(clientAddress);
		int player = Players::NoPlayer;
		if (reserved != m_PlayerSlotByClient.end() &&
		    (m_DebugEmptyWorldSnapshotSmokeEnabled || (activity && activity->PlayerActive(reserved->second) && activity->PlayerHuman(reserved->second)))) {
			player = reserved->second;
		}
		if (m_ReportedPlayerSlots.at(clientAddress) == player) {
			continue;
		}
		WorldStateProtocol::ClientAssignment assignment;
		assignment.PlayerSlot = static_cast<std::int8_t>(player);
		if (!m_Transport.SendClientAssignment(peerAddress, assignment, simulationTick)) {
			Log("ERROR: could not update client player-slot assignment for " + clientAddress);
			continue;
		}
		m_ReportedPlayerSlots[clientAddress] = player;
		m_ResetVotesByClient.erase(clientAddress);
		m_RestartVotesByClient.erase(clientAddress);
		m_PreviousResetInputByClient.erase(clientAddress);
		m_PreviousRestartInputByClient.erase(clientAddress);
		if (reserved != m_PlayerSlotByClient.end()) {
			g_UInputMan.ClearNetworkInputState(reserved->second);
		}
		Log("INFO: client assignment updated for " + clientAddress + " to " +
		    (player == Players::NoPlayer ? std::string("spectator") : "player slot " + std::to_string(player)));
	}
}

void WorldStateServerSession::PollNetwork(std::uint32_t simulationTick) {
	if (!m_Transport.IsStarted()) {
		return;
	}
	std::vector<WorldStateTransport::ReceivedPacket> packets;
	m_Transport.Poll(packets);
	for (const WorldStateTransport::ReceivedPacket& packet : packets) {
		if (packet.Identifier == ID_NEW_INCOMING_CONNECTION) {
			++m_ConnectedClients;
			m_SoundMixBaselinePending = true;
			// Defer terrain access until the next simulation update, but remember to
			// rebuild the batch for this newly connected peer.
			m_PendingTerrainPatches.clear();
			m_TerrainBaselinePending = true;
			const std::string clientAddress(packet.Sender.ToString(true));
			m_PendingSoundBaselines.push_back(clientAddress);
			m_LastInputSequenceByClient.erase(clientAddress);
			m_LastInputTimeByClient.erase(clientAddress);
			m_LastPurchaseByClient.erase(clientAddress);
			m_LastPolicyByClient.erase(clientAddress);
			m_LastConstructionByClient.erase(clientAddress);
			std::erase_if(m_ConstructionOwners, [&](const auto& entry) { return entry.second.Client == clientAddress; });
			m_LastConstructionTimeByClient.erase(clientAddress);
			m_LastPurchaseTimeByClient.erase(clientAddress);
			m_ResetVotesByClient.erase(clientAddress);
			m_RestartVotesByClient.erase(clientAddress);
			m_PreviousResetInputByClient.erase(clientAddress);
			m_PreviousRestartInputByClient.erase(clientAddress);
			int assignedPlayer = Players::NoPlayer;
			for (int player = m_ReserveLocalPlayer ? Players::PlayerTwo : Players::PlayerOne; player < Players::MaxPlayerCount; ++player) {
				if (!m_InputSlotsInUse[player]) {
					m_InputSlotsInUse[player] = true;
					m_PlayerSlotByClient[clientAddress] = player;
					assignedPlayer = player;
					break;
				}
			}
			WorldStateProtocol::ClientAssignment assignment;
			// A reserved slot becomes playable only after the Activity activates it.
			const int reportedPlayer = m_DebugEmptyWorldSnapshotSmokeEnabled ? assignedPlayer : Players::NoPlayer;
			m_ClientAddresses[clientAddress] = packet.Sender;
			m_ReportedPlayerSlots[clientAddress] = reportedPlayer;
			assignment.PlayerSlot = static_cast<std::int8_t>(reportedPlayer);
			if (!m_Transport.SendClientAssignment(packet.Sender, assignment, simulationTick)) {
				Log("ERROR: could not send client player-slot assignment to " + clientAddress);
			}
			Log("INFO: client connected from " + clientAddress + " (" + std::to_string(m_ConnectedClients) + " connected, input slot " +
			    (assignedPlayer == Players::NoPlayer ? std::string("none") : std::to_string(assignedPlayer)) + ")");
		} else if (packet.Identifier == ID_DISCONNECTION_NOTIFICATION || packet.Identifier == ID_CONNECTION_LOST) {
			m_ConnectedClients = m_ConnectedClients > 0 ? static_cast<unsigned short>(m_ConnectedClients - 1) : 0;
			const std::string clientAddress(packet.Sender.ToString(true));
			m_ClientAddresses.erase(clientAddress);
			std::erase(m_PendingSoundBaselines, clientAddress);
			m_LastPolicyByClient.erase(clientAddress);
			m_LastPurchaseByClient.erase(clientAddress);
			m_LastPurchaseTimeByClient.erase(clientAddress);
			m_LastConstructionByClient.erase(clientAddress);
			std::erase_if(m_ConstructionOwners, [&](const auto& entry) { return entry.second.Client == clientAddress; });
			m_LastConstructionTimeByClient.erase(clientAddress);
			m_ReportedPlayerSlots.erase(clientAddress);
			m_LastInputSequenceByClient.erase(clientAddress);
			m_LastInputTimeByClient.erase(clientAddress);
			m_ResetVotesByClient.erase(clientAddress);
			m_RestartVotesByClient.erase(clientAddress);
			m_PreviousResetInputByClient.erase(clientAddress);
			m_PreviousRestartInputByClient.erase(clientAddress);
			if (auto slot = m_PlayerSlotByClient.find(clientAddress); slot != m_PlayerSlotByClient.end()) {
				g_UInputMan.ClearNetworkInputState(slot->second);
				m_InputSlotsInUse[slot->second] = false;
				m_PlayerSlotByClient.erase(slot);
			}
			std::erase_if(m_PendingInputPackets, [&clientAddress](const WorldStateTransport::ReceivedPacket& pendingPacket) {
				return pendingPacket.Sender.ToString(true) == clientAddress;
			});
			Log("INFO: client disconnected from " + clientAddress + " (" + std::to_string(m_ConnectedClients) + " connected)");
		} else if (packet.Identifier == ID_CCR_WORLD_STATE) {
			constexpr std::size_t c_MaxPendingInputPackets = 512;
			if (m_PendingInputPackets.size() == c_MaxPendingInputPackets) {
				m_PendingInputPackets.pop_front();
			}
			m_PendingInputPackets.push_back(packet);
		}
	}
}

void WorldStateServerSession::ProcessConstruction(const WorldStateTransport::ReceivedPacket& packet, const WorldStateProtocol::ConstructionRequest& request) {
	const std::string client(packet.Sender.ToString(true));
	if (!m_ClientAddresses.contains(client)) return;
	WorldStateProtocol::ConstructionResult result;
	result.RequestId = request.RequestId;
	result.SceneRevision = m_SnapshotBuilder.RefreshSceneRevision();
	if (m_ConstructionOwnerRevision != result.SceneRevision) { m_ConstructionOwners.clear(); m_ConstructionOwnerRevision = result.SceneRevision; }
	const auto previous = m_LastConstructionByClient.find(client);
	const auto slot = m_ReportedPlayerSlots.find(client);
	auto* activity = dynamic_cast<GameActivity*>(g_ActivityMan.GetActivity());
	const auto now = std::chrono::steady_clock::now();
	if (slot == m_ReportedPlayerSlots.end() || slot->second < 0) result.Reason = "spectator";
	else if (request.SceneRevision != result.SceneRevision) result.Reason = "stale_scene";
	else if (previous != m_LastConstructionByClient.end() && request.RequestId == previous->second.Request.RequestId) {
		if (request == previous->second.Request) { m_Transport.SendConstructionResult(packet.Sender, previous->second.Result); return; }
		result.Reason = "conflicting_request";
	} else if (previous != m_LastConstructionByClient.end() && request.RequestId < previous->second.Request.RequestId) result.Reason = "stale_request";
	else {
		const auto lastTime = m_LastConstructionTimeByClient.find(client);
		if (lastTime != m_LastConstructionTimeByClient.end() && now - lastTime->second < std::chrono::milliseconds(250)) result.Reason = "rate_limit";
		else if (!activity || activity->GetActivityState() != Activity::Editing) result.Reason = "not_editing";
		else {
			g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
			g_ThreadMan.GetBackgroundThreadPool().wait_for_tasks();
			if (request.Action == WorldStateProtocol::ConstructionAction::PlaceObject) {
				std::erase_if(m_ConstructionOwners, [&](const auto& entry) {
					const auto* object = entry.first > static_cast<std::uint64_t>(std::numeric_limits<long>::max()) ? nullptr : g_MovableMan.FindObjectByUniqueID(static_cast<long>(entry.first));
					return !object || object->IsSetToDelete();
				});
			}
			if (request.Action == WorldStateProtocol::ConstructionAction::SetReady) {
				result.Accepted = activity->SetNetworkConstructionReady(slot->second, request.Ready, result.Reason);
			} else if (slot->second >= Players::MaxPlayerCount || !activity->PlayerActive(slot->second) || !activity->PlayerHuman(slot->second)) {
				result.Reason = "inactive_player";
			} else if (request.Action == WorldStateProtocol::ConstructionAction::RemoveObject) {
				const auto owner = m_ConstructionOwners.find(request.ObjectId);
				if (owner == m_ConstructionOwners.end() || owner->second.Client != client || owner->second.Revision != result.SceneRevision ||
				    owner->second.Team != activity->GetTeamOfPlayer(slot->second)) result.Reason = "object_not_owned";
				else {
					auto* object = request.ObjectId > static_cast<std::uint64_t>(std::numeric_limits<long>::max()) ? nullptr :
					    g_MovableMan.FindObjectByUniqueID(static_cast<long>(request.ObjectId));
					if (!object || object->IsSetToDelete()) result.Reason = "object_missing";
					else if (object->GetTeam() != owner->second.Team || object->GetRootParent() != object ||
					    !(g_MovableMan.IsActor(object) || g_MovableMan.IsDevice(object) || g_MovableMan.IsParticle(object))) result.Reason = "object_attached";
					else { object->SetToDelete(true); result.Accepted = true; result.ObjectId = request.ObjectId; }
					if (result.Accepted || result.Reason == "object_missing") m_ConstructionOwners.erase(owner);
					if (result.Accepted) activity->SetNetworkConstructionReady(slot->second, false, result.Reason);
				}
			} else if (auto* editor = activity->GetEditorGUI(slot->second)) {
				if (request.Action == WorldStateProtocol::ConstructionAction::PlaceObject) {
					const int module = g_PresetMan.GetModuleID(request.Preset.ModuleName);
					const auto* preset = module < 0 ? nullptr : dynamic_cast<const SceneObject*>(g_PresetMan.GetEntityPreset(request.Preset.ClassName, request.Preset.PresetName, module));
					if (m_ConstructionOwners.size() >= 4096) result.Reason = "construction_limit";
					else if (!preset || preset->GetClassName() != request.Preset.ClassName || preset->GetModuleName() != request.Preset.ModuleName || preset->GetPresetName() != request.Preset.PresetName)
						result.Reason = "invalid_preset";
					else result.Accepted = editor->PlaceNetworkObject(preset, Vector(request.X, request.Y), request.Rotation, request.HFlipped,
					    result.ChargedCost, result.ObjectId, result.Reason);
					if (result.Accepted && result.ObjectId) m_ConstructionOwners[result.ObjectId] = {client, result.SceneRevision, activity->GetTeamOfPlayer(slot->second)};
				} else result.Accepted = editor->InstallNetworkBrain(Vector(request.X, request.Y), request.Rotation, request.HFlipped, result.Reason);
				if (result.Accepted) result.Accepted = activity->SetNetworkConstructionReady(slot->second, false, result.Reason);
			} else result.Reason = "editor_unavailable";
		}
		m_LastConstructionTimeByClient[client] = now;
	}
	// Do not replace the cached original response on ID reuse or stale requests.
	if (previous == m_LastConstructionByClient.end() || request.RequestId > previous->second.Request.RequestId)
		m_LastConstructionByClient[client] = {request, result};
	if (!m_Transport.SendConstructionResult(packet.Sender, result)) Log("ERROR: construction result send failed client=" + client);
	Log("INFO: construction id=" + std::to_string(result.RequestId) + " accepted=" + std::to_string(result.Accepted) + " reason=" + result.Reason + " client=" + client);
}

void WorldStateServerSession::ProcessPurchase(const WorldStateTransport::ReceivedPacket& packet, const WorldStateProtocol::PurchaseRequest& request) {
	const std::string client(packet.Sender.ToString(true));
	if (!m_ClientAddresses.contains(client)) return;
	const auto previous = m_LastPurchaseByClient.find(client);
	if (previous != m_LastPurchaseByClient.end() && request.RequestId == previous->second.RequestId) {
		m_Transport.SendPurchaseResult(packet.Sender, previous->second);
		Log("INFO: purchase replay id=" + std::to_string(request.RequestId) + " client=" + client);
		return;
	}
	WorldStateProtocol::PurchaseResult result;
	result.RequestId = request.RequestId;
	result.SceneRevision = m_SnapshotBuilder.RefreshSceneRevision();
	if (previous != m_LastPurchaseByClient.end() && request.RequestId < previous->second.RequestId) {
		result.Reason = "stale_request";
		m_Transport.SendPurchaseResult(packet.Sender, result);
		return;
	}
	const auto now = std::chrono::steady_clock::now();
	const auto lastTime = m_LastPurchaseTimeByClient.find(client);
	const auto slot = m_ReportedPlayerSlots.find(client);
	auto* activity = dynamic_cast<GameActivity*>(g_ActivityMan.GetActivity());
	if (lastTime != m_LastPurchaseTimeByClient.end() && now - lastTime->second < std::chrono::milliseconds(250)) result.Reason = "rate_limit";
	else if (slot == m_ReportedPlayerSlots.end() || slot->second < 0) result.Reason = "spectator";
	else if (request.SceneRevision != result.SceneRevision) result.Reason = "stale_scene";
	else if (!activity) result.Reason = "delivery_unavailable";
	else {
		auto resolve = [](const WorldStateProtocol::PurchasePreset& identity) -> const SceneObject* {
			const int module = g_PresetMan.GetModuleID(identity.ModuleName);
			if (module < 0) return nullptr;
			const auto* preset = dynamic_cast<const SceneObject*>(g_PresetMan.GetEntityPreset(identity.ClassName, identity.PresetName, module));
			return preset && preset->GetModuleName() == identity.ModuleName && preset->GetClassName() == identity.ClassName ? preset : nullptr;
		};
		const auto* craft = dynamic_cast<const ACraft*>(resolve(request.Craft));
		std::vector<const SceneObject*> cargo;
		cargo.reserve(request.Cargo.size());
		for (const auto& identity : request.Cargo) cargo.push_back(resolve(identity));
		if (!craft || std::find(cargo.begin(), cargo.end(), nullptr) != cargo.end()) result.Reason = "invalid_preset";
		else {
			// Native cloning can load Lua scripts; complete the asynchronous GC before entering that path.
			g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
			result.Accepted = activity->CreateValidatedDelivery(slot->second, craft, cargo, Vector(request.LandingX, request.LandingY),
			    request.AIMode, result.ChargedCost, result.Reason);
		}
	}
	if (lastTime == m_LastPurchaseTimeByClient.end() || now - lastTime->second >= std::chrono::milliseconds(250)) m_LastPurchaseTimeByClient[client] = now;
	m_LastPurchaseByClient[client] = result;
	if (!m_Transport.SendPurchaseResult(packet.Sender, result)) Log("ERROR: purchase result send failed client=" + client);
	Log("INFO: purchase id=" + std::to_string(result.RequestId) + " accepted=" + std::to_string(result.Accepted) + " charged=" +
	    std::to_string(result.ChargedCost) + " reason=" + result.Reason + " client=" + client);
}

void WorldStateServerSession::PublishPurchasePolicies(std::uint32_t tick) {
	if (m_LastPolicyTick && tick-m_LastPolicyTick<15) return;
	m_LastPolicyTick=tick;
	auto* activity=dynamic_cast<GameActivity*>(g_ActivityMan.GetActivity());
	if (!activity) return;
	const auto revision=m_SnapshotBuilder.RefreshSceneRevision();
	for (const auto& [client,address]:m_ClientAddresses) {
		const int slot=m_ReportedPlayerSlots.at(client);
		if (slot<0) { m_LastPolicyByClient.erase(client); continue; }
		const BuyMenuGUI* menu=activity->GetBuyGUI(slot);
		const int team=activity->GetTeamOfPlayer(slot);
		if (!menu || team<0 || team>=Activity::MaxTeamCount) continue;
		WorldStateProtocol::PurchasePolicy policy;
		policy.SceneRevision=revision; policy.PlayerSlot=static_cast<std::int8_t>(slot);
		policy.Team=static_cast<std::int8_t>(team);
		if (MetaPlayer* meta=g_MetaMan.GetMetaPlayerOfInGamePlayer(slot); g_MetaMan.GameInProgress() && meta) {
			policy.PricingModule=g_PresetMan.GetDataModuleName(meta->GetNativeTechModule());
			policy.ForeignMultiplier=meta->GetForeignCostMultiplier(); policy.NativeMultiplier=meta->GetNativeCostMultiplier();
		}
		policy.OnlyOwned=menu->GetOnlyShowOwnedItems(); policy.EnforceMass=menu->EnforceMaxMassConstraint(); policy.EnforcePassengers=menu->EnforceMaxPassengersConstraint();
		for (const auto& entry:menu->GetAllowedItems()) policy.Allowed.push_back(entry.first);
		for (const auto& entry:menu->GetAlwaysAllowedItems()) policy.AlwaysAllowed.push_back(entry.first);
		for (const auto& entry:menu->GetProhibitedItems()) policy.Prohibited.push_back(entry.first);
		for (const auto& entry:menu->GetOwnedItems()) policy.Owned.emplace(entry.first,entry.second);
		for (const Box* box:activity->GetLZArea(team).GetBoxes()) {
			const Vector corner=box->GetCorner();
			policy.LandingBoxes.push_back({corner.m_X,corner.m_Y,box->GetWidth(),box->GetHeight()});
		}
		if (const auto previous=m_LastPolicyByClient.find(client); previous!=m_LastPolicyByClient.end() && previous->second==policy) continue;
		if (++m_PolicySequence==0) ++m_PolicySequence;
		if (m_Transport.SendPurchasePolicy(address,policy,m_PolicySequence)) {
			m_LastPolicyByClient[client]=policy;
			Log("INFO: purchase policy sequence="+std::to_string(m_PolicySequence)+" slot="+std::to_string(slot)+" stock_entries="+std::to_string(policy.Owned.size()));
		} else Log("ERROR: purchase policy encoding/send failed client="+client);
	}
}

bool WorldStateServerSession::SendSoundEvents(std::span<const WorldStateSoundProtocol::Event> events) {
	if (!IsStarted() || !m_ConnectedClients || events.empty()) return false;
	if (++m_Sequence == 0) ++m_Sequence;
	return m_Transport.BroadcastSoundEvents(m_SnapshotBuilder.RefreshSceneRevision(), m_Sequence, events);
}

void WorldStateServerSession::Update(std::uint32_t simulationTick) {
	if (!m_Transport.IsStarted()) {
		return;
	}
	PollNetwork(simulationTick);
	if (m_ConnectedClients && g_SceneMan.GetScene()) {
		const auto revision = m_SnapshotBuilder.RefreshSceneRevision();
		if (revision != m_LastSoundSceneRevision) {
			m_PendingSoundBaselines.clear();
			for (const auto& [address, peer] : m_ClientAddresses) m_PendingSoundBaselines.push_back(address);
		}
		if (m_SoundMixBaselinePending || revision != m_LastSoundSceneRevision) {
			const auto mix = GetWorldStateSoundCapture().GetMixState();
			if (!SendSoundEvents(mix)) { Log("ERROR: sound mix baseline send failed"); Stop(); return; }
			m_LastSoundSceneRevision = revision; m_SoundMixBaselinePending = false;
		}
	}
	for (;;) {
		bool overflow = false;
		auto sounds = GetWorldStateSoundCapture().Drain(overflow);
		if (overflow) {
			Log("ERROR: sound capture overflow; stopping network session to avoid lost lifecycle commands");
			Stop(); return;
		}
		if (sounds.empty()) break;
		if (m_ConnectedClients && !SendSoundEvents(sounds)) {
			Log("ERROR: captured sound batch send failed; stopping network session");
			Stop(); return;
		}
	}
	if (!m_PendingSoundBaselines.empty() && g_SceneMan.GetScene()) {
		auto baseline = GetWorldStateSoundCapture().GetMixState();
		const double now = SoundPlaybackClockMilliseconds();
		auto loops = GetWorldStateSoundCapture().GetActiveLoops(now);
		baseline.insert(baseline.end(), loops.begin(), loops.end());
		for (const auto& address : m_PendingSoundBaselines) {
			const auto peer = m_ClientAddresses.find(address); if (peer == m_ClientAddresses.end()) continue;
			for (std::size_t offset = 0; offset < baseline.size(); offset += 100) {
				if (++m_Sequence == 0) ++m_Sequence;
				const auto batch = std::span<const WorldStateSoundProtocol::Event>(baseline).subspan(offset, std::min(std::size_t{100}, baseline.size() - offset));
				if (!m_Transport.SendSoundEvents(peer->second, m_LastSoundSceneRevision, m_Sequence, batch)) {
					Log("ERROR: active sound baseline send failed"); Stop(); return;
				}
			}
			Log("INFO: active sound baseline sent to " + address + " loops=" + std::to_string(loops.size()));
		}
		m_PendingSoundBaselines.clear();
	}
	for (const auto& [clientAddress, player] : m_PlayerSlotByClient) {
		g_UInputMan.ClearNetworkInputImpulse(player);
	}
	if (m_TerrainBaselinePending) {
		if (g_SceneMan.GetScene() && g_SceneMan.GetScene()->GetTerrain()) {
			g_SceneMan.RegisterTerrainChange(0, 0, g_SceneMan.GetSceneWidth(), g_SceneMan.GetSceneHeight());
			m_TerrainBaselinePending = false;
		}
	}
	if (m_ConnectedClients > 0 && g_ActivityMan.IsInActivity() && g_ActivityMan.GetActivity()) {
		const std::size_t configuredPlayers = ConfigureActivityPlayers(*g_ActivityMan.GetActivity(), false);
		if (configuredPlayers > 0) {
			Log("INFO: added " + std::to_string(configuredPlayers) + " connected client(s) to the running Activity");
		}
	}
	RefreshClientAssignments(simulationTick);
	while (!m_PendingInputPackets.empty()) {
		WorldStateTransport::ReceivedPacket packet = std::move(m_PendingInputPackets.front());
		m_PendingInputPackets.pop_front();
		WorldStateProtocol::InputCommand command;
		std::uint32_t sequence = 0;
		const std::string clientAddress(packet.Sender.ToString(true));
		WorldStateProtocol::PurchaseRequest purchase;
		WorldStateProtocol::ConstructionRequest construction;
		if (WorldStateProtocol::DecodeConstructionRequest(packet.Payload, construction)) {
			ProcessConstruction(packet, construction);
			continue;
		}
		if (WorldStateProtocol::DecodePurchaseRequest(packet.Payload, purchase)) {
			ProcessPurchase(packet, purchase);
			continue;
		}
		auto previous = m_LastInputSequenceByClient.find(clientAddress);
		if (!WorldStateProtocol::DecodeInputCommand(packet.Payload, command, &sequence) ||
		    (previous != m_LastInputSequenceByClient.end() && static_cast<std::int32_t>(sequence - previous->second) <= 0)) {
			Log("WARNING: rejected invalid or stale client input command from " + clientAddress);
			continue;
		}
		auto playerSlot = m_PlayerSlotByClient.find(clientAddress);
		const auto reportedSlot = m_ReportedPlayerSlots.find(clientAddress);
		if (playerSlot == m_PlayerSlotByClient.end() || reportedSlot == m_ReportedPlayerSlots.end() || reportedSlot->second != playerSlot->second) {
			continue;
		}
		m_LastInputSequenceByClient[clientAddress] = sequence;
		m_LastInputTimeByClient[clientAddress] = std::chrono::steady_clock::now();
		const bool previousReset = m_PreviousResetInputByClient[clientAddress];
		const bool previousRestart = m_PreviousRestartInputByClient[clientAddress];
		if (command.ResetActivityVote && !previousReset) {
			m_ResetVotesByClient[clientAddress] = true;
		}
		if (command.RestartActivityVote && !previousRestart) {
			m_RestartVotesByClient[clientAddress] = true;
		}
		m_PreviousResetInputByClient[clientAddress] = command.ResetActivityVote;
		m_PreviousRestartInputByClient[clientAddress] = command.RestartActivityVote;
		if (!m_DebugEmptyWorldSnapshotSmokeEnabled) {
			const Activity* activity = g_ActivityMan.GetActivity();
			if (!activity || !activity->PlayerActive(playerSlot->second) || !activity->PlayerHuman(playerSlot->second)) {
				Log("WARNING: rejected gameplay input from " + clientAddress + " because its player slot is not active in the current Activity");
				continue;
			}
		}
		++m_InputCommandCount;
		constexpr float c_AnalogScale = 1.0F / 32767.0F;
		const Vector mouseMovement(static_cast<float>(command.MouseDeltaX), static_cast<float>(command.MouseDeltaY));
		const Vector analogMove(command.AnalogMoveX * c_AnalogScale, command.AnalogMoveY * c_AnalogScale);
		const Vector analogAim(command.AnalogAimX * c_AnalogScale, command.AnalogAimY * c_AnalogScale);
		g_UInputMan.SetNetworkInputState(playerSlot->second, command.HeldElements, mouseMovement, command.MouseWheelDelta,
		                                command.MouseButtonsHeld, analogMove, analogAim);
		if (m_InputCommandCount == 1 || m_InputCommandCount % 120 == 0) {
			Log("INFO: applied " + std::to_string(m_InputCommandCount) + " client input commands to player slot " +
			    std::to_string(playerSlot->second) + "; latest held mask=" + std::to_string(command.HeldElements));
		}
	}
	const auto inputNow = std::chrono::steady_clock::now();
	for (auto it = m_LastInputTimeByClient.begin(); it != m_LastInputTimeByClient.end();) {
		if (inputNow - it->second < std::chrono::seconds(1)) { ++it; continue; }
		if (const auto slot = m_PlayerSlotByClient.find(it->first); slot != m_PlayerSlotByClient.end()) {
			// Keep the slot in network mode: a silent client must not fall back
			// to the host's local keyboard/controller input.
			g_UInputMan.SetNetworkInputState(slot->second, 0, Vector(), 0, 0, Vector(), Vector());
			m_PreviousResetInputByClient.erase(it->first);
			m_PreviousRestartInputByClient.erase(it->first);
			Log("WARNING: neutralized stale network input for player slot " + std::to_string(slot->second));
		}
		it = m_LastInputTimeByClient.erase(it);
	}
	ProcessActivityVotes();
	PublishPurchasePolicies(simulationTick);
	if (m_DebugTerrainMutationSmokeEnabled && !m_DebugTerrainMutationProbeLocated && !m_DebugTerrainMutationSmokeAttempted && m_ConnectedClients > 0) {
		SLTerrain* terrain = g_SceneMan.GetScene() ? g_SceneMan.GetScene()->GetTerrain() : nullptr;
		if (terrain) {
			m_DebugTerrainMutationProbeLocated = FindWorldStateTerrainTestPixel(*terrain, m_DebugTerrainMutationPixelX, m_DebugTerrainMutationPixelY);
			if (m_DebugTerrainMutationProbeLocated) {
				Log("INFO: debug terrain mutation probe x=" + std::to_string(m_DebugTerrainMutationPixelX) + " y=" +
				    std::to_string(m_DebugTerrainMutationPixelY) + " material=" +
				    std::to_string(terrain->GetMaterialPixel(m_DebugTerrainMutationPixelX, m_DebugTerrainMutationPixelY)));
			} else {
				m_DebugTerrainMutationSmokeAttempted = true;
				Log("ERROR: debug terrain mutation smoke could not locate solid probe pixel");
			}
		}
	}

	// The simulation timer currently runs at 60 updates per second. Send 20 full snapshots/sec.
	// Avoid walking every movable object and encoding a full snapshot while nobody is connected.
	if (m_Transport.GetNumberOfConnections() == 0) {
		return;
	}
	if (simulationTick - m_LastBroadcastTick < 3) {
		return;
	}
	m_LastBroadcastTick = simulationTick;
	g_PerformanceMan.StartPerformanceMeasurement(PerformanceMan::WorldStateSnapshot);
	const auto snapshotCaptureStarted = std::chrono::steady_clock::now();
	const WorldStateProtocol::Snapshot snapshot = m_SnapshotBuilder.Capture(simulationTick);
	WorldStateProtocol::ConstructionState constructionState;
	constructionState.SceneRevision = snapshot.SceneRevision;
	constructionState.ActivityState = snapshot.ActivityState;
	if (const auto* activity = dynamic_cast<const GameActivity*>(g_ActivityMan.GetActivity())) {
		for (int player = 0; player < Players::MaxPlayerCount; ++player) {
			auto& target = constructionState.Players[player];
			target.Active = activity->PlayerActive(player) && activity->PlayerHuman(player);
			if (!target.Active) continue;
			target.Team = static_cast<std::int16_t>(activity->GetTeamOfPlayer(player));
			if (snapshot.ActivityState != Activity::Editing) continue;
			target.Ready = activity->IsNetworkConstructionReady(player);
			const auto* brain = g_SceneMan.GetScene() ? dynamic_cast<const Actor*>(g_SceneMan.GetScene()->GetResidentBrain(player)) : nullptr;
			if (!brain) continue;
			target.Brain.emplace();
			auto& state = *target.Brain;
			state.NetworkId = static_cast<std::uint64_t>(brain->GetUniqueID());
			state.ClassName = brain->GetClassName(); state.ModuleName = brain->GetModuleName(); state.PresetName = brain->GetPresetName();
			state.Flags = WorldStateProtocol::c_ObjectFlagActor; state.Team = target.Team;
			state.PositionX = brain->GetPos().GetX(); state.PositionY = brain->GetPos().GetY();
			state.Rotation = brain->GetRotAngle(); state.HFlipped = brain->IsHFlipped();
			state.SpriteFrame = static_cast<std::uint16_t>(std::min(brain->GetFrame(), 65535U));
			state.Health = brain->GetHealth(); state.AimAngle = brain->GetAimAngle(false); state.ActorStatus = static_cast<std::uint8_t>(brain->GetStatus());
			if (!WorldStateAttachments::Capture(*brain, state)) { target.Brain.reset(); Log("WARNING: construction brain attachment capture failed"); }
		}
	}
	if (++m_Sequence == 0) ++m_Sequence;
	const auto constructionSequence = m_Sequence;
	if ((snapshot.ActivityClassName.empty() || snapshot.ActivityPreset.empty() || snapshot.ScenePreset.empty()) && !m_DebugEmptyWorldSnapshotSmokeEnabled) {
		g_PerformanceMan.StopPerformanceMeasurement(PerformanceMan::WorldStateSnapshot);
		if (!m_LoggedWaitingForWorldState) {
			Log("INFO: waiting for an active Activity and Scene before sending the first world snapshot");
			m_LoggedWaitingForWorldState = true;
		}
		return;
	}
	m_SnapshotCaptureWindowMicroseconds += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - snapshotCaptureStarted).count());
	++m_SnapshotCaptureWindowSamples;
	m_LastSnapshotObjectCount = snapshot.Objects.size();
	g_PerformanceMan.StopPerformanceMeasurement(PerformanceMan::WorldStateSnapshot);
	if (!m_Transport.BroadcastConstructionState(constructionState, constructionSequence)) Log("ERROR: construction state send failed");
	WorldStateTransport::SnapshotTransmissionMetrics transmissionMetrics;
	if (!m_Transport.BroadcastSnapshot(snapshot, ++m_Sequence, &transmissionMetrics)) {
		Log("ERROR: failed to encode or broadcast world snapshot");
	} else {
		++m_SnapshotBroadcastCount;
		m_SnapshotPayloadWindowBytes += transmissionMetrics.PayloadBytes;
		m_SnapshotEncodeWindowMicroseconds += transmissionMetrics.EncodeMicroseconds;
		m_SnapshotCompressWindowMicroseconds += transmissionMetrics.CompressMicroseconds;
		m_SnapshotQueueWindowMicroseconds += transmissionMetrics.QueueMicroseconds;
		const std::uint64_t transportMicroseconds = transmissionMetrics.EncodeMicroseconds + transmissionMetrics.CompressMicroseconds + transmissionMetrics.QueueMicroseconds;
		m_SnapshotTransportWindowMicroseconds += transportMicroseconds;
		++m_SnapshotPayloadWindowSamples;
		m_MaxSnapshotPayloadBytes = std::max(m_MaxSnapshotPayloadBytes, transmissionMetrics.PayloadBytes);
		m_MaxSnapshotTransportMicroseconds = std::max(m_MaxSnapshotTransportMicroseconds, transportMicroseconds);
		m_MaxSnapshotEncodeMicroseconds = std::max(m_MaxSnapshotEncodeMicroseconds, transmissionMetrics.EncodeMicroseconds);
		m_MaxSnapshotCompressMicroseconds = std::max(m_MaxSnapshotCompressMicroseconds, transmissionMetrics.CompressMicroseconds);
		m_MaxSnapshotQueueMicroseconds = std::max(m_MaxSnapshotQueueMicroseconds, transmissionMetrics.QueueMicroseconds);
		if (m_SnapshotBroadcastCount % 100 == 0 && m_SnapshotCaptureWindowSamples > 0 && m_SnapshotPayloadWindowSamples > 0) {
			const double averageCaptureMilliseconds = static_cast<double>(m_SnapshotCaptureWindowMicroseconds) / m_SnapshotCaptureWindowSamples / 1000.0;
			const double averagePayloadBytes = static_cast<double>(m_SnapshotPayloadWindowBytes) / m_SnapshotPayloadWindowSamples;
			const double averageTransportMilliseconds = static_cast<double>(m_SnapshotTransportWindowMicroseconds) / m_SnapshotPayloadWindowSamples / 1000.0;
			const double averageEncodeMilliseconds = static_cast<double>(m_SnapshotEncodeWindowMicroseconds) / m_SnapshotPayloadWindowSamples / 1000.0;
			const double averageCompressMilliseconds = static_cast<double>(m_SnapshotCompressWindowMicroseconds) / m_SnapshotPayloadWindowSamples / 1000.0;
			const double averageQueueMilliseconds = static_cast<double>(m_SnapshotQueueWindowMicroseconds) / m_SnapshotPayloadWindowSamples / 1000.0;
			Log("INFO: snapshot capture average over " + std::to_string(m_SnapshotCaptureWindowSamples) + " samples=" +
			    std::to_string(averageCaptureMilliseconds) + " ms; objects=" + std::to_string(snapshot.Objects.size()) +
			    "; average encode/compress/queue=" + std::to_string(averageEncodeMilliseconds) + "/" + std::to_string(averageCompressMilliseconds) + "/" +
			    std::to_string(averageQueueMilliseconds) + " ms (total " + std::to_string(averageTransportMilliseconds) + "); peak stages=" +
			    std::to_string(m_MaxSnapshotEncodeMicroseconds / 1000.0) + "/" + std::to_string(m_MaxSnapshotCompressMicroseconds / 1000.0) + "/" +
			    std::to_string(m_MaxSnapshotQueueMicroseconds / 1000.0) + " ms; peak total=" +
			    std::to_string(m_MaxSnapshotTransportMicroseconds / 1000.0) + " ms" +
			    "; average compressed payload bytes over " + std::to_string(m_SnapshotPayloadWindowSamples) + " broadcasts=" + std::to_string(averagePayloadBytes) +
			    "; peak compressed payload bytes=" +
			    std::to_string(m_MaxSnapshotPayloadBytes) + "; estimated snapshot payload bandwidth/client=" +
			    std::to_string(averagePayloadBytes * 20.0 * 8.0 / 1000.0) + " kbit/s at 20 Hz (excluding RakNet/IP/UDP overhead)");
			m_SnapshotCaptureWindowMicroseconds = 0;
			m_SnapshotTransportWindowMicroseconds = 0;
			m_SnapshotEncodeWindowMicroseconds = 0;
			m_SnapshotCompressWindowMicroseconds = 0;
			m_SnapshotQueueWindowMicroseconds = 0;
			m_SnapshotPayloadWindowBytes = 0;
			m_SnapshotCaptureWindowSamples = 0;
			m_SnapshotPayloadWindowSamples = 0;
			m_MaxSnapshotPayloadBytes = 0;
			m_MaxSnapshotTransportMicroseconds = 0;
			m_MaxSnapshotEncodeMicroseconds = 0;
			m_MaxSnapshotCompressMicroseconds = 0;
			m_MaxSnapshotQueueMicroseconds = 0;
		}
	}
	if (snapshot.SceneRevision != m_LastTerrainSceneRevision) {
		m_PendingTerrainPatches.clear();
		m_LastTerrainSceneRevision = snapshot.SceneRevision;
	}
	QueueTerrainChanges(snapshot.SceneRevision);
	SendPendingTerrainPatches();
	if (m_DebugTerrainMutationSmokeEnabled && !m_DebugTerrainMutationSmokeAttempted && m_ConnectedClients > 0 && m_DebugTerrainMutationProbeLocated &&
	    m_DebugTerrainMutationBaselineSent) {
		m_DebugTerrainMutationSmokeAttempted = true;
		SLTerrain* terrain = g_SceneMan.GetScene() ? g_SceneMan.GetScene()->GetTerrain() : nullptr;
		int pixelX = 0;
		int pixelY = 0;
		bool mutated = false;
		int originalMaterial = g_MaterialAir;
		if (terrain && m_DebugTerrainMutationProbeLocated && m_DebugTerrainMutationBaselineSent) {
			pixelX = m_DebugTerrainMutationPixelX;
			pixelY = m_DebugTerrainMutationPixelY;
			originalMaterial = terrain->GetMaterialPixel(pixelX, pixelY);
			float retardation = 0.0F;
			const bool penetrated = g_SceneMan.TryPenetrate(pixelX, pixelY, Vector(1000000.0F, 0.0F), Vector(30.0F, 0.0F), retardation, 1.0F, 0, 0, 0, 0.0F);
			mutated = penetrated && terrain->GetMaterialPixel(pixelX, pixelY) == g_MaterialAir;
		}
		m_DebugTerrainMutationSmokePassed = mutated;
		Log(std::string("INFO: debug terrain mutation smoke=") + (mutated ? "passed" : "failed") +
		    " x=" + std::to_string(pixelX) + " y=" + std::to_string(pixelY) +
		    " original_material=" + std::to_string(originalMaterial) + " material_after=" +
		    std::to_string(terrain && pixelX < terrain->GetWidth() && pixelY < terrain->GetHeight() ? terrain->GetMaterialPixel(pixelX, pixelY) : -1));
	}
}

void WorldStateServerSession::QueueTerrainChanges(std::uint32_t sceneRevision) {
	if (sceneRevision == 0 || !g_SceneMan.GetScene() || !g_SceneMan.GetScene()->GetTerrain()) {
		return;
	}
	SLTerrain* terrain = g_SceneMan.GetScene()->GetTerrain();
	for (const TerrainDirtyGrid::Rectangle& region : g_SceneMan.DrainWorldStateTerrainChanges()) {
		const int regionRight = region.X + region.Width;
		const int regionBottom = region.Y + region.Height;
		for (int y = region.Y; y < regionBottom; y += 64) {
			for (int x = region.X; x < regionRight; x += 64) {
				WorldStateProtocol::TerrainPatch patch;
				patch.SceneRevision = sceneRevision;
				patch.X = static_cast<std::uint32_t>(x);
				patch.Y = static_cast<std::uint32_t>(y);
				patch.Width = static_cast<std::uint32_t>(std::min(64, regionRight - x));
				patch.Height = static_cast<std::uint32_t>(std::min(64, regionBottom - y));
				const std::size_t pixelCount = static_cast<std::size_t>(patch.Width) * patch.Height;
				for (WorldStateProtocol::TerrainLayer layer : {WorldStateProtocol::TerrainLayer::Material,
				                                               WorldStateProtocol::TerrainLayer::Foreground,
				                                               WorldStateProtocol::TerrainLayer::Background}) {
					patch.Layer = layer;
					patch.Pixels.resize(pixelCount);
					for (std::uint32_t row = 0; row < patch.Height; ++row) {
						for (std::uint32_t column = 0; column < patch.Width; ++column) {
							const int pixelX = x + static_cast<int>(column);
							const int pixelY = y + static_cast<int>(row);
							int value = 0;
							switch (layer) {
							case WorldStateProtocol::TerrainLayer::Material: value = terrain->GetMaterialPixel(pixelX, pixelY); break;
							case WorldStateProtocol::TerrainLayer::Foreground: value = terrain->GetFGColorPixel(pixelX, pixelY); break;
							case WorldStateProtocol::TerrainLayer::Background: value = terrain->GetBGColorPixel(pixelX, pixelY); break;
							}
							patch.Pixels[static_cast<std::size_t>(row) * patch.Width + column] = static_cast<std::uint8_t>(value);
						}
					}
					m_PendingTerrainPatches.emplace_back(std::move(patch));
				}
			}
		}
	}
}

void WorldStateServerSession::SendPendingTerrainPatches() {
	constexpr std::size_t c_MaxPatchesPerUpdate = 16;
	std::size_t sentThisUpdate = 0;
	while (!m_PendingTerrainPatches.empty() && sentThisUpdate < c_MaxPatchesPerUpdate) {
		const WorldStateProtocol::TerrainPatch& patch = m_PendingTerrainPatches.front();
		bool probeBaselinePatch = false;
		std::uint8_t probeBaselineMaterial = 0;
		bool probeMutationPatch = false;
		std::uint8_t probeMutationMaterial = 0;
		if (m_DebugTerrainMutationSmokeEnabled && m_DebugTerrainMutationProbeLocated && !m_DebugTerrainMutationBaselineSent &&
		    patch.Layer == WorldStateProtocol::TerrainLayer::Material &&
		    m_DebugTerrainMutationPixelX >= static_cast<int>(patch.X) && m_DebugTerrainMutationPixelY >= static_cast<int>(patch.Y) &&
		    m_DebugTerrainMutationPixelX < static_cast<int>(patch.X + patch.Width) && m_DebugTerrainMutationPixelY < static_cast<int>(patch.Y + patch.Height)) {
			const std::size_t pixelIndex = static_cast<std::size_t>(m_DebugTerrainMutationPixelY - static_cast<int>(patch.Y)) * patch.Width +
			                               static_cast<std::size_t>(m_DebugTerrainMutationPixelX - static_cast<int>(patch.X));
			if (pixelIndex < patch.Pixels.size() && patch.Pixels[pixelIndex] > g_MaterialCavity) {
				probeBaselinePatch = true;
				probeBaselineMaterial = patch.Pixels[pixelIndex];
			}
		}
		if (m_DebugTerrainMutationSmokePassed && !m_DebugTerrainMutationPatchSent && patch.Layer == WorldStateProtocol::TerrainLayer::Material &&
		    m_DebugTerrainMutationPixelX >= static_cast<int>(patch.X) && m_DebugTerrainMutationPixelY >= static_cast<int>(patch.Y) &&
		    m_DebugTerrainMutationPixelX < static_cast<int>(patch.X + patch.Width) && m_DebugTerrainMutationPixelY < static_cast<int>(patch.Y + patch.Height)) {
			const std::size_t pixelIndex = static_cast<std::size_t>(m_DebugTerrainMutationPixelY - static_cast<int>(patch.Y)) * patch.Width +
			                               static_cast<std::size_t>(m_DebugTerrainMutationPixelX - static_cast<int>(patch.X));
			if (pixelIndex < patch.Pixels.size()) {
				probeMutationPatch = true;
				probeMutationMaterial = patch.Pixels[pixelIndex];
			}
		}
		const std::uint32_t sequence = ++m_Sequence;
		if (!m_Transport.BroadcastTerrainPatch(patch, sequence)) {
			Log("ERROR: failed to encode or broadcast terrain patch");
			return;
		}
		if (probeMutationPatch) {
			m_DebugTerrainMutationPatchSent = probeMutationMaterial == g_MaterialAir;
			Log("INFO: debug terrain mutation patch sent sequence=" + std::to_string(sequence) + " x=" +
			    std::to_string(m_DebugTerrainMutationPixelX) + " y=" + std::to_string(m_DebugTerrainMutationPixelY) +
			    " material=" + std::to_string(probeMutationMaterial) + " current=" + (m_DebugTerrainMutationPatchSent ? "yes" : "no"));
		}
		if (probeBaselinePatch) {
			m_DebugTerrainMutationBaselineSent = true;
			Log("INFO: debug terrain mutation baseline pixel sent x=" + std::to_string(m_DebugTerrainMutationPixelX) + " y=" +
			    std::to_string(m_DebugTerrainMutationPixelY) + " material=" + std::to_string(probeBaselineMaterial));
		}
		m_PendingTerrainPatches.pop_front();
		++sentThisUpdate;
		++m_TerrainPatchBroadcastCount;
	}
}

void WorldStateServerSession::ProcessActivityVotes() {
	if (!g_ActivityMan.IsInActivity() || !g_ActivityMan.GetActivity() ||
	    g_ActivityMan.GetActivity()->GetPresetName() == "Multiplayer Lobby" || m_PlayerSlotByClient.empty()) {
		return;
	}
	const auto now = std::chrono::steady_clock::now();
	if (m_LastActivityVoteAction != std::chrono::steady_clock::time_point{} && now - m_LastActivityVoteAction < std::chrono::seconds(3)) {
		return;
	}
	const bool hasPlayers = std::any_of(m_ReportedPlayerSlots.begin(), m_ReportedPlayerSlots.end(), [](const auto& client) {
		return client.second != Players::NoPlayer;
	});
	const bool resetUnanimous = hasPlayers &&
	    std::all_of(m_ReportedPlayerSlots.begin(), m_ReportedPlayerSlots.end(), [this](const auto& client) {
		    if (client.second == Players::NoPlayer) return true;
		    const auto vote = m_ResetVotesByClient.find(client.first);
		    return vote != m_ResetVotesByClient.end() && vote->second;
	    });
	const bool restartUnanimous = hasPlayers &&
	    std::all_of(m_ReportedPlayerSlots.begin(), m_ReportedPlayerSlots.end(), [this](const auto& client) {
		    if (client.second == Players::NoPlayer) return true;
		    const auto vote = m_RestartVotesByClient.find(client.first);
		    return vote != m_RestartVotesByClient.end() && vote->second;
	    });
	if (!resetUnanimous && !restartUnanimous) {
		return;
	}
	for (auto& [client, vote] : m_ResetVotesByClient) {
		vote = false;
	}
	for (auto& [client, vote] : m_RestartVotesByClient) {
		vote = false;
	}
	m_LastActivityVoteAction = now;
	if (resetUnanimous) {
		Log("INFO: connected clients unanimously voted to end the current Activity");
		g_ActivityMan.EndActivity();
		g_ActivityMan.SetRestartActivity();
		g_ActivityMan.SetInActivity(false);
	} else {
		Log("INFO: connected clients unanimously voted to restart the current Activity");
		g_ActivityMan.SetRestartActivity();
	}
}

void WorldStateServerSession::Log(const std::string& message) {
	if (m_Log.is_open()) {
		m_Log << message << '\n';
		m_Log.flush();
	}
}
