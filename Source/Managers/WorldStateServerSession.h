#pragma once

#include "Constants.h"
#include "WorldStateSnapshotBuilder.h"
#include "WorldStateTransport.h"

#include <cstdint>
#include <cstddef>
#include <array>
#include <chrono>
#include <deque>
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
		void EnableDebugTerrainMutationSmoke();
		bool DidDebugTerrainMutationSmokePass() const { return m_DebugTerrainMutationSmokePassed && m_DebugTerrainMutationPatchSent; }
		bool IsStarted() const { return m_Transport.IsStarted(); }
		unsigned short GetBoundPort() const { return m_Transport.GetBoundPort(); }
		unsigned short GetConnectedClientCount() const { return m_ConnectedClients; }
		std::uint32_t GetSnapshotBroadcastCount() const { return m_SnapshotBroadcastCount; }
		std::uint32_t GetInputCommandCount() const { return m_InputCommandCount; }

	private:
		void Log(const std::string& message);
		void ProcessActivityVotes();
		void QueueTerrainChanges(std::uint32_t sceneRevision);
		void SendPendingTerrainPatches();

		WorldStateTransport m_Transport;
		WorldStateSnapshotBuilder m_SnapshotBuilder;
		std::ofstream m_Log;
		std::uint32_t m_LastBroadcastTick = 0;
		std::uint32_t m_Sequence = 0;
		std::uint32_t m_SnapshotBroadcastCount = 0;
		std::uint64_t m_SnapshotCaptureWindowMicroseconds = 0;
		std::uint64_t m_SnapshotPayloadWindowBytes = 0;
		std::uint32_t m_SnapshotCaptureWindowSamples = 0;
		std::uint32_t m_SnapshotPayloadWindowSamples = 0;
		std::size_t m_LastSnapshotObjectCount = 0;
		std::size_t m_MaxSnapshotPayloadBytes = 0;
		std::uint32_t m_InputCommandCount = 0;
		std::uint32_t m_LastTerrainSceneRevision = 0;
		std::uint32_t m_TerrainPatchBroadcastCount = 0;
		std::deque<WorldStateProtocol::TerrainPatch> m_PendingTerrainPatches;
		unsigned short m_ConnectedClients = 0;
		std::unordered_map<std::string, std::uint32_t> m_LastInputSequenceByClient;
		std::unordered_map<std::string, int> m_PlayerSlotByClient;
		std::unordered_map<std::string, bool> m_ResetVotesByClient;
		std::unordered_map<std::string, bool> m_RestartVotesByClient;
		std::unordered_map<std::string, bool> m_PreviousResetInputByClient;
		std::unordered_map<std::string, bool> m_PreviousRestartInputByClient;
		std::chrono::steady_clock::time_point m_LastActivityVoteAction{};
		std::array<bool, Players::MaxPlayerCount> m_InputSlotsInUse{};
		bool m_DebugTerrainMutationSmokeEnabled = false;
		bool m_DebugTerrainMutationSmokeAttempted = false;
		bool m_DebugTerrainMutationProbeLocated = false;
		bool m_DebugTerrainMutationBaselineSent = false;
		bool m_DebugTerrainMutationSmokePassed = false;
		bool m_DebugTerrainMutationPatchSent = false;
		int m_DebugTerrainMutationPixelX = 0;
		int m_DebugTerrainMutationPixelY = 0;
	};

} // namespace RTE
