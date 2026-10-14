#include "WorldStateClientSession.h"

#include "MessageIdentifiers.h"
#include "NetworkMessages.h"
#include "WorldStateCompression.h"

#include <utility>

using namespace RTE;

namespace {
	// Inspect only the stable envelope; do not parse a foreign-version body.
	bool ReadEnvelopeVersion(std::span<const std::uint8_t> packet, std::uint16_t& version) {
		using namespace WorldStateProtocol;
		if (packet.size() < c_HeaderSize || packet.size() > c_MaxPacketSize) return false;
		std::size_t offset = 0;
		std::uint64_t magic = 0, wireVersion = 0, type = 0, reserved = 0, sequence = 0, size = 0;
		if (!Detail::ReadUnsigned(packet, offset, 4, magic) || magic != c_Magic ||
		    !Detail::ReadUnsigned(packet, offset, 2, wireVersion) || !wireVersion ||
		    !Detail::ReadUnsigned(packet, offset, 1, type) || !type ||
		    !Detail::ReadUnsigned(packet, offset, 1, reserved) || reserved != 0 ||
		    !Detail::ReadUnsigned(packet, offset, 4, sequence) ||
		    !Detail::ReadUnsigned(packet, offset, 4, size) || size != packet.size() - c_HeaderSize) return false;
		version = static_cast<std::uint16_t>(wireVersion);
		return true;
	}
}

bool WorldStateClientSession::Connect(const char* address, unsigned short port) {
	Disconnect();
	if (m_Transport.StartClient(address, port)) return true;
	m_ConnectionError = "Unable to start client transport. Check the server address.";
	return false;
}

void WorldStateClientSession::Disconnect() {
	m_Transport.Stop();
	m_ConnectionError.clear();
	m_Connected = false;
	m_HasSnapshot = false;
	m_LastSequence = 0;
	m_LastTerrainSequence = 0;
	m_TerrainPatches.clear();
	m_SoundQueue.Clear();
	m_PurchaseResults.clear();
	m_ConstructionResults.clear();
	m_ConstructionState = {}; m_ConstructionSequence = 0;
	m_ConstructionCatalog = {}; m_ConstructionCatalogSequence = 0;
	m_PurchasePolicy={}; m_PolicySequence=0;
	m_ReceivedSnapshotCount = 0;
	m_InputCommandSequence = 0;
	m_AssignedPlayerSlot = -1;
	m_HasPlayerAssignment = false;
	m_LatestSnapshot = {};
}

bool WorldStateClientSession::SendInputCommand(const WorldStateProtocol::InputCommand& command) {
	return m_Connected && m_Transport.SendInputCommand(command, ++m_InputCommandSequence);
}

std::vector<WorldStateProtocol::PurchaseResult> WorldStateClientSession::DrainPurchaseResults() {
	std::vector<WorldStateProtocol::PurchaseResult> results;
	while (!m_PurchaseResults.empty()) {
		results.push_back(std::move(m_PurchaseResults.front()));
		m_PurchaseResults.pop_front();
	}
	return results;
}

std::vector<WorldStateProtocol::ConstructionResult> WorldStateClientSession::DrainConstructionResults() {
	std::vector<WorldStateProtocol::ConstructionResult> results;
	results.reserve(m_ConstructionResults.size());
	while (!m_ConstructionResults.empty()) { results.push_back(std::move(m_ConstructionResults.front())); m_ConstructionResults.pop_front(); }
	return results;
}

std::vector<WorldStateClientSession::ReceivedTerrainPatch> WorldStateClientSession::DrainTerrainPatches(std::uint32_t sceneRevision) {
	std::vector<ReceivedTerrainPatch> patches;
	patches.reserve(m_TerrainPatches.size());
	const std::size_t queuedCount = m_TerrainPatches.size();
	for (std::size_t index = 0; index < queuedCount; ++index) {
		ReceivedTerrainPatch patch = std::move(m_TerrainPatches.front());
		m_TerrainPatches.pop_front();
		if (patch.Patch.SceneRevision == sceneRevision) {
			patches.emplace_back(std::move(patch));
		} else if (static_cast<std::int32_t>(sceneRevision - patch.Patch.SceneRevision) < 0) {
			m_TerrainPatches.emplace_back(std::move(patch));
		}
	}
	return patches;
}

std::vector<WorldStateSoundProtocol::Event> WorldStateClientSession::DrainSoundEvents(std::uint32_t sceneRevision) {
	return m_SoundQueue.Drain(sceneRevision);
}

void WorldStateClientSession::Update() {
	if (!m_Transport.IsStarted()) {
		m_Connected = false;
		return;
	}
	std::vector<WorldStateTransport::ReceivedPacket> packets;
	m_Transport.Poll(packets);
	for (const WorldStateTransport::ReceivedPacket& packet : packets) {
		if (packet.Identifier == ID_CONNECTION_REQUEST_ACCEPTED) {
			m_Connected = true;
			continue;
		}
		switch (packet.Identifier) {
			case ID_ALREADY_CONNECTED: m_ConnectionError = "The server rejected a duplicate peer identity. Reconnect to create a new peer."; break;
			case ID_CONNECTION_ATTEMPT_FAILED: m_ConnectionError = "The server did not accept the connection."; break;
			case ID_NO_FREE_INCOMING_CONNECTIONS: m_ConnectionError = "The server is full."; break;
			case ID_CONNECTION_BANNED: m_ConnectionError = "The server rejected this client (banned)."; break;
			case ID_INVALID_PASSWORD: m_ConnectionError = "The server requires a different password."; break;
			case ID_INCOMPATIBLE_PROTOCOL_VERSION: m_ConnectionError = "The server uses an incompatible network protocol."; break;
			case ID_DISCONNECTION_NOTIFICATION: m_ConnectionError = "The server closed the connection."; break;
			case ID_CONNECTION_LOST: m_ConnectionError = "Connection to the server was lost."; break;
			default: break;
		}
		if (!m_ConnectionError.empty()) {
			m_Connected = false;
			m_SoundQueue.Clear();
			m_Transport.Stop();
			// Keep the last authoritative snapshot frozen until explicit Disconnect or
			// Connect. Clearing it here would resume local simulation of the remote world.
			return;
		}
		if (packet.Identifier != ID_CCR_WORLD_STATE) {
			continue;
		}
		std::vector<std::uint8_t> decompressedPacket;
		std::span<const std::uint8_t> snapshotPayload;
		if (!WorldStateCompression::DecodeFromWire(packet.Payload, decompressedPacket, snapshotPayload)) continue;
		std::uint16_t serverVersion = 0;
		if (ReadEnvelopeVersion(snapshotPayload, serverVersion) && serverVersion != WorldStateProtocol::c_Version) {
			m_ConnectionError = "Incompatible world-state protocol: server " + std::to_string(serverVersion) +
				", client " + std::to_string(WorldStateProtocol::c_Version) + ". Update client and server together.";
			m_Connected = false;
			m_SoundQueue.Clear();
			m_Transport.Stop();
			return; // Retain any authoritative snapshot, just as on transport failure.
		}
		WorldStateProtocol::TerrainPatch terrainPatch;
		std::vector<WorldStateSoundProtocol::Event> soundEvents;
		std::uint32_t soundRevision = 0, soundSequence = 0;
		if (WorldStateSoundProtocol::DecodePacket(snapshotPayload, soundRevision, soundSequence, soundEvents)) {
			if (!m_SoundQueue.Push(soundRevision, soundSequence, std::move(soundEvents))) {
				m_ConnectionError = "The sound event queue overflowed. Reconnect to synchronize the session.";
				m_Connected = false;
				m_SoundQueue.Clear();
				m_Transport.Stop();
				return;
			}
			continue;
		}
		WorldStateProtocol::PurchaseResult purchase;
		WorldStateProtocol::ConstructionResult construction;
		WorldStateProtocol::ConstructionState constructionState;
		WorldStateProtocol::ConstructionCatalog constructionCatalog;
		std::uint32_t constructionSequence = 0;
		if (WorldStateProtocol::DecodeConstructionCatalog(snapshotPayload, constructionCatalog, constructionSequence)) {
			if (!m_ConstructionCatalogSequence || static_cast<std::int32_t>(constructionSequence - m_ConstructionCatalogSequence) > 0) {
				m_ConstructionCatalog = std::move(constructionCatalog); m_ConstructionCatalogSequence = constructionSequence;
			}
			continue;
		}
		if (WorldStateProtocol::DecodeConstructionState(snapshotPayload, constructionState, constructionSequence)) {
			if (!m_ConstructionSequence || static_cast<std::int32_t>(constructionSequence - m_ConstructionSequence) > 0) {
				m_ConstructionState = std::move(constructionState); m_ConstructionSequence = constructionSequence;
			}
			continue;
		}
		if (WorldStateProtocol::DecodeConstructionResult(snapshotPayload, construction)) {
			if (m_ConstructionResults.size() >= 64) m_ConstructionResults.pop_front();
			m_ConstructionResults.push_back(std::move(construction));
			continue;
		}
		WorldStateProtocol::PurchasePolicy policy;
		std::uint32_t policySequence=0;
		if (WorldStateProtocol::DecodePurchasePolicy(snapshotPayload,policy,&policySequence)) {
			if (!m_PolicySequence || static_cast<std::int32_t>(policySequence-m_PolicySequence)>0) { m_PolicySequence=policySequence; m_PurchasePolicy=std::move(policy); }
			continue;
		}
		if (WorldStateProtocol::DecodePurchaseResult(snapshotPayload, purchase)) {
			if (m_PurchaseResults.size() >= 64) m_PurchaseResults.pop_front();
			m_PurchaseResults.push_back(std::move(purchase));
			continue;
		}
		std::uint32_t terrainSequence = 0;
		if (WorldStateProtocol::DecodeTerrainPatch(snapshotPayload, terrainPatch, &terrainSequence)) {
			if (m_LastTerrainSequence == 0 || static_cast<std::int32_t>(terrainSequence - m_LastTerrainSequence) > 0) {
				m_LastTerrainSequence = terrainSequence;
				m_TerrainPatches.push_back({std::move(terrainPatch), terrainSequence});
			}
			continue;
		}
		WorldStateProtocol::ClientAssignment assignment;
		if (WorldStateProtocol::DecodeClientAssignment(snapshotPayload, assignment)) {
			m_AssignedPlayerSlot = assignment.PlayerSlot;
			m_HasPlayerAssignment = true;
			continue;
		}
		WorldStateProtocol::Snapshot snapshot;
		std::uint32_t sequence = 0;
		if (!WorldStateProtocol::DecodeSnapshot(snapshotPayload, snapshot, &sequence)) {
			continue;
		}
		if (m_HasSnapshot && static_cast<std::int32_t>(sequence - m_LastSequence) <= 0) {
			continue;
		}
		m_LastSequence = sequence;
		m_LatestSnapshot = std::move(snapshot);
		m_HasSnapshot = true;
		++m_ReceivedSnapshotCount;
	}
}
