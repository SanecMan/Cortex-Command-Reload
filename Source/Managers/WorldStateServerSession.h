#pragma once

#include "WorldStateSnapshotBuilder.h"
#include "WorldStateTransport.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace RTE {

	/// Optional in-process authoritative snapshot host. It shares the normal simulation;
	/// it is not a headless server and does not yet apply client input commands.
	class WorldStateServerSession {
	public:
		bool Start(const std::string& bindAddress, unsigned short port, unsigned short maxPlayers, const std::string& logPath);
		void Stop();
		void Update(std::uint32_t simulationTick);
		bool IsStarted() const { return m_Transport.IsStarted(); }

	private:
		void Log(const std::string& message);

		WorldStateTransport m_Transport;
		WorldStateSnapshotBuilder m_SnapshotBuilder;
		std::ofstream m_Log;
		std::uint32_t m_LastBroadcastTick = 0;
		std::uint32_t m_Sequence = 0;
		unsigned short m_ConnectedClients = 0;
	};

} // namespace RTE
