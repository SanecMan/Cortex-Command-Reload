#pragma once

#include "WorldStateProtocol.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace RTE {

	/// Captures the current authoritative simulation state in the world-state wire model.
	/// Object IDs are taken from the engine's unique, server-local runtime IDs.
	class WorldStateSnapshotBuilder {
	public:
		WorldStateProtocol::Snapshot Capture(std::uint32_t tick);

	private:
		std::uint32_t m_SceneRevision = 0;
		std::uintptr_t m_LastActivityAddress = 0;
		std::uintptr_t m_LastSceneAddress = 0;
		std::string m_LastScenePreset;
		std::string m_LastActivityPreset;
		std::unordered_map<long, std::uint64_t> m_NetworkIdsByRuntimeId;
		std::uint64_t m_NextNetworkId = 1;
	};

} // namespace RTE
