#pragma once

#include "RakPeerInterface.h"
#include "PacketPriority.h"

// RakNet includes Windows.h, whose GetClassName macro collides with Entity's method.
#undef GetClassName

#include "RakNetTypes.h"
#include "WorldStateProtocol.h"

#include <cstdint>
#include <span>
#include <vector>

namespace RTE {

	/// Small RakNet transport for the versioned world-state protocol.
	/// It deliberately owns its peer and does not depend on renderer/client managers.
	class WorldStateTransport {
	public:
		struct ReceivedPacket {
			unsigned char Identifier = 0;
			RakNet::SystemAddress Sender = RakNet::UNASSIGNED_SYSTEM_ADDRESS;
			std::vector<std::uint8_t> Payload;
		};

		WorldStateTransport() = default;
		~WorldStateTransport();
		WorldStateTransport(const WorldStateTransport&) = delete;
		WorldStateTransport& operator=(const WorldStateTransport&) = delete;

		bool StartServer(unsigned short port, unsigned short maxPlayers, const char* bindAddress = "0.0.0.0");
		bool StartClient(const char* address, unsigned short port);
		void Stop();
		bool IsStarted() const { return m_Peer != nullptr; }
		unsigned short GetBoundPort() const;
		unsigned short GetNumberOfConnections() const { return m_Peer ? m_Peer->NumberOfConnections() : 0; }

		bool SendWorldState(const RakNet::AddressOrGUID& target, std::span<const std::uint8_t> packet);
		bool BroadcastWorldState(std::span<const std::uint8_t> packet);
	bool SendSnapshot(const RakNet::AddressOrGUID& target, const WorldStateProtocol::Snapshot& snapshot, std::uint32_t sequence);
	bool BroadcastSnapshot(const WorldStateProtocol::Snapshot& snapshot, std::uint32_t sequence);
	bool SendInputCommand(const WorldStateProtocol::InputCommand& command, std::uint32_t sequence);
		void Poll(std::vector<ReceivedPacket>& packets);

	private:
		bool Send(const RakNet::AddressOrGUID& target, std::span<const std::uint8_t> packet, bool broadcast, PacketReliability reliability = RELIABLE_ORDERED);

		RakNet::RakPeerInterface* m_Peer = nullptr;
	};

} // namespace RTE
