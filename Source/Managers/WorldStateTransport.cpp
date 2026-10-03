#include "WorldStateTransport.h"

#include "NetworkMessages.h"
#include "WorldStateProtocol.h"

#include "MessageIdentifiers.h"
#include "PacketPriority.h"

using namespace RTE;

WorldStateTransport::~WorldStateTransport() {
	Stop();
}

bool WorldStateTransport::StartServer(unsigned short port, unsigned short maxPlayers, const char* bindAddress) {
	Stop();
	if (maxPlayers == 0) {
		return false;
	}

	m_Peer = RakNet::RakPeerInterface::GetInstance();
	RakNet::SocketDescriptor socket(port, bindAddress);
	socket.socketFamily = AF_INET;
	if (m_Peer->Startup(maxPlayers, &socket, 1) != RakNet::RAKNET_STARTED) {
		Stop();
		return false;
	}
	m_Peer->SetMaximumIncomingConnections(maxPlayers);
	return true;
}

bool WorldStateTransport::StartClient(const char* address, unsigned short port) {
	Stop();
	if (!address || address[0] == '\0') {
		return false;
	}

	m_Peer = RakNet::RakPeerInterface::GetInstance();
	RakNet::SocketDescriptor socket(0, "0.0.0.0");
	socket.socketFamily = AF_INET;
	if (m_Peer->Startup(1, &socket, 1) != RakNet::RAKNET_STARTED) {
		Stop();
		return false;
	}
	if (m_Peer->Connect(address, port, nullptr, 0) != RakNet::CONNECTION_ATTEMPT_STARTED) {
		Stop();
		return false;
	}
	return true;
}

void WorldStateTransport::Stop() {
	if (m_Peer) {
		m_Peer->Shutdown(100);
		RakNet::RakPeerInterface::DestroyInstance(m_Peer);
		m_Peer = nullptr;
	}
}

unsigned short WorldStateTransport::GetBoundPort() const {
	return m_Peer ? m_Peer->GetMyBoundAddress().GetPort() : 0;
}

bool WorldStateTransport::SendWorldState(const RakNet::AddressOrGUID& target, std::span<const std::uint8_t> packet) {
	return Send(target, packet, false);
}

bool WorldStateTransport::BroadcastWorldState(std::span<const std::uint8_t> packet) {
	return Send(RakNet::UNASSIGNED_SYSTEM_ADDRESS, packet, true);
}

bool WorldStateTransport::SendSnapshot(const RakNet::AddressOrGUID& target, const WorldStateProtocol::Snapshot& snapshot, std::uint32_t sequence) {
	std::vector<std::uint8_t> packet;
	return WorldStateProtocol::EncodeSnapshot(snapshot, sequence, packet) && Send(target, packet, false, UNRELIABLE_SEQUENCED);
}

bool WorldStateTransport::BroadcastSnapshot(const WorldStateProtocol::Snapshot& snapshot, std::uint32_t sequence) {
	std::vector<std::uint8_t> packet;
	return WorldStateProtocol::EncodeSnapshot(snapshot, sequence, packet) && Send(RakNet::UNASSIGNED_SYSTEM_ADDRESS, packet, true, UNRELIABLE_SEQUENCED);
}

bool WorldStateTransport::Send(const RakNet::AddressOrGUID& target, std::span<const std::uint8_t> packet, bool broadcast, PacketReliability reliability) {
	if (!m_Peer || packet.empty() || packet.size() > WorldStateProtocol::c_MaxPacketSize) {
		return false;
	}
	std::vector<char> wirePacket;
	wirePacket.reserve(packet.size() + 1);
	wirePacket.push_back(static_cast<char>(ID_CCR_WORLD_STATE));
	wirePacket.insert(wirePacket.end(), packet.begin(), packet.end());
	return m_Peer->Send(wirePacket.data(), static_cast<int>(wirePacket.size()), HIGH_PRIORITY, reliability, 0, target, broadcast);
}

void WorldStateTransport::Poll(std::vector<ReceivedPacket>& packets) {
	if (!m_Peer) {
		return;
	}
	for (RakNet::Packet* packet = m_Peer->Receive(); packet; m_Peer->DeallocatePacket(packet), packet = m_Peer->Receive()) {
		if (packet->length < 1 || packet->length > WorldStateProtocol::c_MaxPacketSize + 1) {
			continue;
		}
		ReceivedPacket received;
		received.Identifier = packet->data[0];
		received.Sender = packet->systemAddress;
		received.Payload.assign(packet->data + 1, packet->data + packet->length);
		packets.push_back(std::move(received));
	}
}
