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
			std::vector<std::string> MissingPresetDetails;
		};

		ApplyResult Apply(const WorldStateProtocol::Snapshot& snapshot);
		void Forget() { m_Objects.clear(); }

	private:
		struct Replica {
			class MovableObject* Object = nullptr;
			bool ClientOwned = false;
		};

		std::unordered_map<std::uint64_t, Replica> m_Objects;
	};

} // namespace RTE
