#include "WorldStateServerSession.h"

#include "MessageIdentifiers.h"
#include "NetworkMessages.h"

using namespace RTE;

bool WorldStateServerSession::Start(const std::string& bindAddress, unsigned short port, unsigned short maxPlayers, const std::string& logPath) {
	Stop();
	m_Log.open(logPath, std::ios::out | std::ios::app);
	if (!m_Log || !m_Transport.StartServer(port, maxPlayers, bindAddress.c_str())) {
		Log("ERROR: could not bind world-state server socket");
		Stop();
		return false;
	}
	m_LastBroadcastTick = 0;
	m_Sequence = 0;
	m_ConnectedClients = 0;
	Log("INFO: world-state host listening on " + bindAddress + ":" + std::to_string(m_Transport.GetBoundPort()) +
	    " (max clients " + std::to_string(maxPlayers) + ")");
	return true;
}

void WorldStateServerSession::Stop() {
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
	std::vector<WorldStateTransport::ReceivedPacket> packets;
	m_Transport.Poll(packets);
	for (const WorldStateTransport::ReceivedPacket& packet : packets) {
		if (packet.Identifier == ID_NEW_INCOMING_CONNECTION) {
			++m_ConnectedClients;
			Log("INFO: client connected from " + std::string(packet.Sender.ToString(true)) + " (" + std::to_string(m_ConnectedClients) + " connected)");
		} else if (packet.Identifier == ID_DISCONNECTION_NOTIFICATION || packet.Identifier == ID_CONNECTION_LOST) {
			m_ConnectedClients = m_ConnectedClients > 0 ? static_cast<unsigned short>(m_ConnectedClients - 1) : 0;
			Log("INFO: client disconnected from " + std::string(packet.Sender.ToString(true)) + " (" + std::to_string(m_ConnectedClients) + " connected)");
		} else if (packet.Identifier == ID_CCR_WORLD_STATE) {
			Log("WARNING: client world-state/input packet rejected; client command handling is not enabled yet");
		}
	}

	// The simulation timer currently runs at 60 updates per second. Send 20 full snapshots/sec.
	if (simulationTick - m_LastBroadcastTick < 3) {
		return;
	}
	m_LastBroadcastTick = simulationTick;
	const WorldStateProtocol::Snapshot snapshot = m_SnapshotBuilder.Capture(simulationTick);
	if (!m_Transport.BroadcastSnapshot(snapshot, ++m_Sequence)) {
		Log("ERROR: failed to encode or broadcast world snapshot");
	}
}

void WorldStateServerSession::Log(const std::string& message) {
	if (m_Log.is_open()) {
		m_Log << message << '\n';
		m_Log.flush();
	}
}
