#pragma once

#include "WorldStateProtocol.h"

#include <cstdint>
#include <string>

namespace RTE {

	/// Captures the current authoritative simulation state in the world-state wire model.
	/// Object IDs are taken from the engine's unique, server-local runtime IDs.
	class WorldStateSnapshotBuilder {
	public:
		WorldStateProtocol::Snapshot Capture(std::uint32_t tick);

	private:
		std::uint32_t m_SceneRevision = 0;
		std::string m_LastScenePreset;
		std::string m_LastActivityPreset;
	};

} // namespace RTE
