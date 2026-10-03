#pragma once

#include <string>

namespace RTE {

	/// Optional Discord Rich Presence integration using Discord's local IPC transport.
	class DiscordPresence {
	public:
		static void Initialize();
		static void SetEnabled(bool enabled);
		static void SetActivity(const std::string& details, const std::string& state);
		static void Shutdown();
	};

} // namespace RTE
