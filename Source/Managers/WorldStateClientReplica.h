#pragma once

#include "WorldStateProtocol.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace RTE {

	/// Applies authoritative top-level MovableObject state on the simulation thread.
	/// Missing mod presets are skipped; existing .rte objects keep their normal ownership.
	class WorldStateClientReplica {
	public:
		struct ApplyResult {
			std::uint32_t Updated = 0;
			std::uint32_t Spawned = 0;
			std::uint32_t Removed = 0;
			std::uint32_t MissingPresets = 0;
			std::uint32_t HumanEquipmentStatesApplied = 0;
			std::uint32_t InventoryStatesApplied = 0, InventoryObjectsSpawned = 0;
			bool InventoryMatches=false;
			std::uint32_t InventoryLiveObjects=0, InventoryCachedObjects=0;
			std::vector<std::string> InventoryMismatchDetails;
			std::uint32_t PositionErrorsMeasured = 0;
			double TotalPositionErrorBeforeCorrection = 0.0;
			double MeanPositionErrorBeforeCorrection = 0.0;
			double MaxPositionErrorBeforeCorrection = 0.0;
			std::uint32_t RotationInterpolationsScheduled = 0;
			double MaxRotationInterpolationDeltaRadians = 0.0;
			std::vector<std::string> MissingPresetDetails;
		};

		ApplyResult Apply(const WorldStateProtocol::Snapshot& snapshot, bool authoritativeWorld = false);
		std::uint32_t AdvanceInterpolation();
		bool AdjustCameraForInterpolation(std::uint64_t networkId, class Vector& camera) const;
		bool BindPlayerView(std::uint64_t networkId, int team, int localPlayer = 0) const;
		bool ApplyTerrainPatch(const WorldStateProtocol::TerrainPatch& patch);
		void Forget();

	private:
		void ApplyInventory(const WorldStateProtocol::Snapshot& snapshot, ApplyResult& result);
		struct Replica {
			class MovableObject* Object = nullptr;
			bool ClientOwned = false;
			float AuthoritativeX = 0.0F;
			float AuthoritativeY = 0.0F;
			float InterpolationStartX = 0.0F;
			float InterpolationStartY = 0.0F;
			float InterpolationDeltaX = 0.0F;
			float InterpolationDeltaY = 0.0F;
			float InterpolationStartRotation = 0.0F;
			float InterpolationDeltaRotation = 0.0F;
			std::uint32_t InterpolationElapsedTicks = 0;
			std::uint32_t InterpolationDurationTicks = 0;
		};

		std::unordered_map<std::uint64_t, Replica> m_Objects;
		std::unordered_map<std::uint64_t,class MovableObject*> m_InventoryObjects; // Borrowed: Actor inventories own these objects.
		std::uint32_t m_LastSnapshotTick = 0;
	};

} // namespace RTE
