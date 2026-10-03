#pragma once

#include "Constants.h"
#include "WorldStateSnapshotBuilder.h"
#include "WorldStateTransport.h"

#include <cstdint>
#include <array>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace RTE {

	/// Optional in-process authoritative snapshot host. It shares the normal simulation;
	/// it is not a headless server.
	class WorldStateServerSession {
	public:
		bool Start(const std::string& bindAddress, unsigned short port, unsigned short maxPlayers, const std::string& logPath, bool truncateLog = false);
		void Stop();
		void Update(std::uint32_t simulationTick);
		bool IsStarted() const { return m_Transport.IsStarted(); }
		unsigned short GetBoundPort() const { return m_Transport.GetBoundPort(); }
		unsigned short GetConnectedClientCount() const { return m_ConnectedClients; }
	std::uint32_t GetSnapshotBroadcastCount() const { return m_SnapshotBroadcastCount; }
	std::uint32_t GetInputCommandCount() const { return m_InputCommandCount; }

	private:
		void Log(const std::string& message);

		WorldStateTransport m_Transport;
		WorldStateSnapshotBuilder m_SnapshotBuilder;
		std::ofstream m_Log;
		std::uint32_t m_LastBroadcastTick = 0;
		std::uint32_t m_Sequence = 0;
	std::uint32_t m_SnapshotBroadcastCount = 0;
	std::uint32_t m_InputCommandCount = 0;
	unsigned short m_ConnectedClients = 0;
	std::unordered_map<std::string, std::uint32_t> m_LastInputSequenceByClient;
	std::unordered_map<std::string, int> m_PlayerSlotByClient;
	std::array<bool, Players::MaxPlayerCount> m_InputSlotsInUse{};
	};

} // namespace RTE
