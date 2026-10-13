#pragma once

#include "WorldStateProtocol.h"
#include "WorldStateTransport.h"
#include "WorldStateSoundQueue.h"

#include <cstdint>
#include <deque>
#include <vector>

namespace RTE {

	/// Receives state snapshots and forwards sampled local input commands to the host.
	/// The main simulation loop applies replica snapshots; host-side input injection is
	/// still separate from transport validation.
class WorldStateClientSession {
public:
	struct ReceivedTerrainPatch {
		WorldStateProtocol::TerrainPatch Patch;
		std::uint32_t Sequence = 0;
	};
		bool Connect(const char* address, unsigned short port);
		void Disconnect();
	void Update();
	bool SendInputCommand(const WorldStateProtocol::InputCommand& command);
	bool SendPurchaseRequest(const WorldStateProtocol::PurchaseRequest& request) { return m_Connected && m_Transport.SendPurchaseRequest(request); }
	std::vector<WorldStateProtocol::PurchaseResult> DrainPurchaseResults();
	const WorldStateProtocol::PurchasePolicy& GetPurchasePolicy() const { return m_PurchasePolicy; }
	std::uint32_t GetPurchasePolicySequence() const { return m_PolicySequence; }
	int GetAssignedPlayerSlot() const { return m_AssignedPlayerSlot; }
	bool HasPlayerAssignment() const { return m_HasPlayerAssignment; }
		bool IsConnected() const { return m_Connected; }
		const std::string& GetConnectionError() const { return m_ConnectionError; }
		bool HasSnapshot() const { return m_HasSnapshot; }
		std::uint32_t GetReceivedSnapshotCount() const { return m_ReceivedSnapshotCount; }
	const WorldStateProtocol::Snapshot& GetLatestSnapshot() const { return m_LatestSnapshot; }
	std::vector<ReceivedTerrainPatch> DrainTerrainPatches(std::uint32_t sceneRevision);
	std::vector<WorldStateSoundProtocol::Event> DrainSoundEvents(std::uint32_t sceneRevision);

	private:
		WorldStateTransport m_Transport;
	WorldStateProtocol::Snapshot m_LatestSnapshot;
	std::deque<WorldStateProtocol::PurchaseResult> m_PurchaseResults;
	WorldStateProtocol::PurchasePolicy m_PurchasePolicy;
	std::uint32_t m_PolicySequence=0;
	std::deque<ReceivedTerrainPatch> m_TerrainPatches;
	WorldStateSoundQueue m_SoundQueue;
	std::uint32_t m_LastSequence = 0;
	std::uint32_t m_LastTerrainSequence = 0;
	std::uint32_t m_ReceivedSnapshotCount = 0;
	std::uint32_t m_InputCommandSequence = 0;
	int m_AssignedPlayerSlot = -1;
	bool m_HasPlayerAssignment = false;
		std::string m_ConnectionError;
		bool m_Connected = false;
		bool m_HasSnapshot = false;
	};

} // namespace RTE
