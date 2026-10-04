#include "WorldStateServerSession.h"

#include "MessageIdentifiers.h"
#include "ActivityMan.h"
#include "NetworkMessages.h"
#include "SceneMan.h"
#include "SLTerrain.h"
#include "UInputMan.h"

#include <algorithm>
#include <utility>

using namespace RTE;

bool WorldStateServerSession::Start(const std::string& bindAddress, unsigned short port, unsigned short maxPlayers, const std::string& logPath, bool truncateLog) {
	Stop();
	m_Log.open(logPath, std::ios::out | (truncateLog ? std::ios::trunc : std::ios::app));
	if (!m_Log || !m_Transport.StartServer(port, maxPlayers, bindAddress.c_str())) {
		Log("ERROR: could not bind world-state server socket");
		Stop();
		return false;
	}
	g_SceneMan.SetWorldStateTerrainTrackingEnabled(true);
	m_LastBroadcastTick = 0;
	m_Sequence = 0;
	m_SnapshotBroadcastCount = 0;
	m_InputCommandCount = 0;
	m_LastTerrainSceneRevision = 0;
	m_TerrainPatchBroadcastCount = 0;
	m_PendingTerrainPatches.clear();
	m_LastInputSequenceByClient.clear();
	m_PlayerSlotByClient.clear();
	m_ResetVotesByClient.clear();
	m_RestartVotesByClient.clear();
	m_PreviousResetInputByClient.clear();
	m_PreviousRestartInputByClient.clear();
	m_LastActivityVoteAction = {};
	m_InputSlotsInUse.fill(false);
	// This session runs inside the playable host client. Keep its local PlayerOne
	// input independent from network peers; use PlayerTwo through PlayerFour remotely.
	m_InputSlotsInUse[Players::PlayerOne] = true;
	m_ConnectedClients = 0;
	Log("INFO: world-state host listening on " + bindAddress + ":" + std::to_string(m_Transport.GetBoundPort()) +
	    " (max clients " + std::to_string(maxPlayers) + ")");
	return true;
}

void WorldStateServerSession::Stop() {
	g_SceneMan.SetWorldStateTerrainTrackingEnabled(false);
	m_PendingTerrainPatches.clear();
	m_LastTerrainSceneRevision = 0;
	for (const auto& [clientAddress, player] : m_PlayerSlotByClient) {
		g_UInputMan.ClearNetworkInputState(player);
	}
	m_PlayerSlotByClient.clear();
	m_LastInputSequenceByClient.clear();
	m_ResetVotesByClient.clear();
	m_RestartVotesByClient.clear();
	m_PreviousResetInputByClient.clear();
	m_PreviousRestartInputByClient.clear();
	m_InputSlotsInUse.fill(false);
	if (m_Transport.IsStarted()) {
		m_Transport.Stop();
		Log("INFO: world-state host stopped");
	}
	if (m_Log.is_open()) {
		m_Log.close();
	}
	m_ConnectedClients = 0;
}

void WorldStateServerSession::Update(std::uint32_t simulationTick) {
	if (!m_Transport.IsStarted()) {
		return;
	}
	for (const auto& [clientAddress, player] : m_PlayerSlotByClient) {
		g_UInputMan.ClearNetworkInputImpulse(player);
	}
	std::vector<WorldStateTransport::ReceivedPacket> packets;
	m_Transport.Poll(packets);
	for (const WorldStateTransport::ReceivedPacket& packet : packets) {
		if (packet.Identifier == ID_NEW_INCOMING_CONNECTION) {
			++m_ConnectedClients;
			if (g_SceneMan.GetScene() && g_SceneMan.GetScene()->GetTerrain()) {
				g_SceneMan.RegisterTerrainChange(0, 0, g_SceneMan.GetSceneWidth(), g_SceneMan.GetSceneHeight());
			}
			const std::string clientAddress(packet.Sender.ToString(true));
			m_LastInputSequenceByClient.erase(clientAddress);
			m_ResetVotesByClient.erase(clientAddress);
			m_RestartVotesByClient.erase(clientAddress);
			m_PreviousResetInputByClient.erase(clientAddress);
			m_PreviousRestartInputByClient.erase(clientAddress);
			int assignedPlayer = Players::NoPlayer;
			for (int player = Players::PlayerTwo; player < Players::MaxPlayerCount; ++player) {
				if (!m_InputSlotsInUse[player]) {
					m_InputSlotsInUse[player] = true;
					m_PlayerSlotByClient[clientAddress] = player;
					assignedPlayer = player;
					break;
				}
			}
			WorldStateProtocol::ClientAssignment assignment;
			assignment.PlayerSlot = assignedPlayer == Players::NoPlayer ? -1 : static_cast<std::int8_t>(assignedPlayer);
			if (!m_Transport.SendClientAssignment(packet.Sender, assignment, simulationTick)) {
				Log("ERROR: could not send client player-slot assignment to " + clientAddress);
			}
			Log("INFO: client connected from " + clientAddress + " (" + std::to_string(m_ConnectedClients) + " connected, input slot " +
			    (assignedPlayer == Players::NoPlayer ? std::string("none") : std::to_string(assignedPlayer)) + ")");
		} else if (packet.Identifier == ID_DISCONNECTION_NOTIFICATION || packet.Identifier == ID_CONNECTION_LOST) {
			m_ConnectedClients = m_ConnectedClients > 0 ? static_cast<unsigned short>(m_ConnectedClients - 1) : 0;
			const std::string clientAddress(packet.Sender.ToString(true));
			m_LastInputSequenceByClient.erase(clientAddress);
			m_ResetVotesByClient.erase(clientAddress);
			m_RestartVotesByClient.erase(clientAddress);
			m_PreviousResetInputByClient.erase(clientAddress);
			m_PreviousRestartInputByClient.erase(clientAddress);
			if (auto slot = m_PlayerSlotByClient.find(clientAddress); slot != m_PlayerSlotByClient.end()) {
				g_UInputMan.ClearNetworkInputState(slot->second);
				m_InputSlotsInUse[slot->second] = false;
				m_PlayerSlotByClient.erase(slot);
			}
			Log("INFO: client disconnected from " + clientAddress + " (" + std::to_string(m_ConnectedClients) + " connected)");
		} else if (packet.Identifier == ID_CCR_WORLD_STATE) {
			WorldStateProtocol::InputCommand command;
			std::uint32_t sequence = 0;
			const std::string clientAddress(packet.Sender.ToString(true));
			auto previous = m_LastInputSequenceByClient.find(clientAddress);
			if (!WorldStateProtocol::DecodeInputCommand(packet.Payload, command, &sequence) ||
			    (previous != m_LastInputSequenceByClient.end() && static_cast<std::int32_t>(sequence - previous->second) <= 0)) {
				Log("WARNING: rejected invalid or stale client input command from " + clientAddress);
				continue;
			}
			auto playerSlot = m_PlayerSlotByClient.find(clientAddress);
			if (playerSlot == m_PlayerSlotByClient.end()) {
				Log("WARNING: rejected gameplay input from spectator " + clientAddress + " (no free player slot)");
				continue;
			}
			m_LastInputSequenceByClient[clientAddress] = sequence;
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
	}
	ProcessActivityVotes();

	// The simulation timer currently runs at 60 updates per second. Send 20 full snapshots/sec.
	// Avoid walking every movable object and encoding a full snapshot while nobody is connected.
	if (m_Transport.GetNumberOfConnections() == 0) {
		return;
	}
	if (simulationTick - m_LastBroadcastTick < 3) {
		return;
	}
	m_LastBroadcastTick = simulationTick;
	const WorldStateProtocol::Snapshot snapshot = m_SnapshotBuilder.Capture(simulationTick);
	if (!m_Transport.BroadcastSnapshot(snapshot, ++m_Sequence)) {
		Log("ERROR: failed to encode or broadcast world snapshot");
	} else {
		++m_SnapshotBroadcastCount;
	}
	if (snapshot.SceneRevision != m_LastTerrainSceneRevision) {
		m_PendingTerrainPatches.clear();
		m_LastTerrainSceneRevision = snapshot.SceneRevision;
	}
	QueueTerrainChanges(snapshot.SceneRevision);
	SendPendingTerrainPatches();
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
		if (!m_Transport.BroadcastTerrainPatch(m_PendingTerrainPatches.front(), ++m_Sequence)) {
			Log("ERROR: failed to encode or broadcast terrain patch");
			return;
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
	const bool resetUnanimous = m_ResetVotesByClient.size() == m_PlayerSlotByClient.size() &&
	    std::all_of(m_PlayerSlotByClient.begin(), m_PlayerSlotByClient.end(), [this](const auto& client) {
		    const auto vote = m_ResetVotesByClient.find(client.first);
		    return vote != m_ResetVotesByClient.end() && vote->second;
	    });
	const bool restartUnanimous = m_RestartVotesByClient.size() == m_PlayerSlotByClient.size() &&
	    std::all_of(m_PlayerSlotByClient.begin(), m_PlayerSlotByClient.end(), [this](const auto& client) {
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
