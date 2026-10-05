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
			std::uint32_t PositionErrorsMeasured = 0;
			double TotalPositionErrorBeforeCorrection = 0.0;
			double MeanPositionErrorBeforeCorrection = 0.0;
			double MaxPositionErrorBeforeCorrection = 0.0;
			std::vector<std::string> MissingPresetDetails;
		};

		ApplyResult Apply(const WorldStateProtocol::Snapshot& snapshot);
		std::uint32_t AdvanceInterpolation();
		bool ApplyTerrainPatch(const WorldStateProtocol::TerrainPatch& patch);
		void Forget() { m_Objects.clear(); m_LastSnapshotTick = 0; }

	private:
		struct Replica {
			class MovableObject* Object = nullptr;
			bool ClientOwned = false;
			float InterpolationStartX = 0.0F;
			float InterpolationStartY = 0.0F;
			float InterpolationDeltaX = 0.0F;
			float InterpolationDeltaY = 0.0F;
			std::uint32_t InterpolationElapsedTicks = 0;
			std::uint32_t InterpolationDurationTicks = 0;
		};

		std::unordered_map<std::uint64_t, Replica> m_Objects;
		std::uint32_t m_LastSnapshotTick = 0;
	};

} // namespace RTE
