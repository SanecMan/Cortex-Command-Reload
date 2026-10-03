#pragma once

#include "WorldStateProtocol.h"
#include "WorldStateTransport.h"

#include <cstdint>

namespace RTE {

	/// Receives and validates authoritative snapshots. It does not yet reconcile them
	/// into MovableMan; that requires lifecycle and mod-object mapping policy.
	class WorldStateClientSession {
	public:
		bool Connect(const char* address, unsigned short port);
		void Disconnect();
		void Update();
		bool IsConnected() const { return m_Connected; }
		bool HasSnapshot() const { return m_HasSnapshot; }
		std::uint32_t GetReceivedSnapshotCount() const { return m_ReceivedSnapshotCount; }
		const WorldStateProtocol::Snapshot& GetLatestSnapshot() const { return m_LatestSnapshot; }

	private:
		WorldStateTransport m_Transport;
		WorldStateProtocol::Snapshot m_LatestSnapshot;
		std::uint32_t m_LastSequence = 0;
		std::uint32_t m_ReceivedSnapshotCount = 0;
		bool m_Connected = false;
		bool m_HasSnapshot = false;
	};

} // namespace RTE
