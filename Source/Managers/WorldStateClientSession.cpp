#include "WorldStateClientSession.h"

#include "MessageIdentifiers.h"
#include "NetworkMessages.h"
#include "WorldStateCompression.h"

#include <utility>

using namespace RTE;

bool WorldStateClientSession::Connect(const char* address, unsigned short port) {
	Disconnect();
	return m_Transport.StartClient(address, port);
}

void WorldStateClientSession::Disconnect() {
	m_Transport.Stop();
	m_Connected = false;
	m_HasSnapshot = false;
	m_LastSequence = 0;
	m_LastTerrainSequence = 0;
	m_TerrainPatches.clear();
	m_ReceivedSnapshotCount = 0;
	m_InputCommandSequence = 0;
	m_AssignedPlayerSlot = -1;
	m_HasPlayerAssignment = false;
	m_LatestSnapshot = {};
}

bool WorldStateClientSession::SendInputCommand(const WorldStateProtocol::InputCommand& command) {
	return m_Connected && m_Transport.SendInputCommand(command, ++m_InputCommandSequence);
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
		if (packet.Identifier == ID_DISCONNECTION_NOTIFICATION || packet.Identifier == ID_CONNECTION_LOST) {
			m_Connected = false;
			continue;
		}
		if (packet.Identifier != ID_CCR_WORLD_STATE) {
			continue;
		}
		WorldStateProtocol::TerrainPatch terrainPatch;
		std::uint32_t terrainSequence = 0;
		if (WorldStateProtocol::DecodeTerrainPatch(packet.Payload, terrainPatch, &terrainSequence)) {
			if (m_LastTerrainSequence == 0 || static_cast<std::int32_t>(terrainSequence - m_LastTerrainSequence) > 0) {
				m_LastTerrainSequence = terrainSequence;
				m_TerrainPatches.push_back({std::move(terrainPatch), terrainSequence});
			}
			continue;
		}
		WorldStateProtocol::ClientAssignment assignment;
		if (WorldStateProtocol::DecodeClientAssignment(packet.Payload, assignment)) {
			m_AssignedPlayerSlot = assignment.PlayerSlot;
			m_HasPlayerAssignment = true;
			continue;
		}
		std::vector<std::uint8_t> decompressedPacket;
		std::span<const std::uint8_t> snapshotPayload;
		if (!WorldStateCompression::DecodeFromWire(packet.Payload, decompressedPacket, snapshotPayload)) {
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
