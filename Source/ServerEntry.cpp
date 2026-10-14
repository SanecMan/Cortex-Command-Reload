// This frontend uses Windows code-page definitions excluded by legacy NONLS flags.
#ifdef NONLS
#undef NONLS
#endif
#include "System/ApplicationEntry.h"
#include "System/UTF8.h"
#include "Managers/WorldStateClientSession.h"
#ifdef _WIN32
#include <Windows.h>
#include <winnls.h>
#endif
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <charconv>
#include <chrono>
#include <thread>

namespace {
// Separate-process transport diagnostic. It loads no content or devices and
// does not stand in for a graphical client's replica/gameplay validation.
int RunClientProbe(const std::vector<std::string>& arguments) {
	bool requireText = false, inputSilence = false, requireObjectives = false, requireControl = false;
	unsigned int probeSeconds = 3;
	unsigned int minimumRevision = 2;
	unsigned int expectedHumans = 0;
	if (arguments.size() < 4 || arguments[2].empty()) {
		std::cerr << "Usage: CortexCommandServer --probe-client ADDRESS PORT [--screen-text] [--probe-seconds 3..3600] [--input-silence]\n"; return 2;
	}
	for (std::size_t index = 4; index < arguments.size(); ++index) {
		if (arguments[index] == "--screen-text") requireText = true;
		else if (arguments[index] == "--objectives") requireObjectives = true;
		else if (arguments[index] == "--require-control") requireControl = true;
		else if (arguments[index] == "--expected-humans" && index + 1 < arguments.size()) {
			const auto& value = arguments[++index];
			const auto number = std::from_chars(value.data(), value.data() + value.size(), expectedHumans);
			if (number.ec != std::errc{} || number.ptr != value.data() + value.size() || expectedHumans < 1 || expectedHumans > 4) {
				std::cerr << "Expected human count must be 1..4.\n"; return 2;
			}
		}
		else if (arguments[index] == "--minimum-revision" && index + 1 < arguments.size()) {
			const auto& value = arguments[++index];
			const auto number = std::from_chars(value.data(), value.data() + value.size(), minimumRevision);
			if (number.ec != std::errc{} || number.ptr != value.data() + value.size() || minimumRevision < 2 || minimumRevision > 65535) {
				std::cerr << "Minimum scene revision must be 2..65535.\n"; return 2;
			}
		}
		else if (arguments[index] == "--input-silence") inputSilence = true;
		else if (arguments[index] == "--probe-seconds" && index + 1 < arguments.size()) {
			const auto& value = arguments[++index];
			const auto number = std::from_chars(value.data(), value.data() + value.size(), probeSeconds);
			if (number.ec != std::errc{} || number.ptr != value.data() + value.size() || probeSeconds < 3 || probeSeconds > 3600) {
				std::cerr << "Probe duration must be an integer from 3 to 3600.\n"; return 2;
			}
		} else { std::cerr << "Unknown or incomplete probe option.\n"; return 2; }
	}
	unsigned int port = 0;
	const auto& text = arguments[3];
	const auto parsed = std::from_chars(text.data(), text.data() + text.size(), port);
	if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || port == 0 || port > 65535) {
		std::cerr << "Probe port must be an integer from 1 to 65535.\n"; return 2;
	}
	RTE::WorldStateClientSession client;
	if (!client.Connect(arguments[2].c_str(), static_cast<unsigned short>(port))) {
		std::cerr << client.GetConnectionError() << '\n'; return 1;
	}
	const auto started = std::chrono::steady_clock::now();
	std::uint32_t firstTick = 0, commands = 0;
	bool receivedWorld = false, receivedText = false, receivedObjectives = false, wasSilent = false;
	unsigned int silenceWindows = 0;
	while (std::chrono::steady_clock::now() - started < std::chrono::seconds(probeSeconds + 30)) {
		client.Update();
		if (client.HasSnapshot()) {
			// A transport-only probe still consumes presentation streams. It has
			// no renderer/audio device, but must not accumulate unbounded events.
			const auto revision = client.GetLatestSnapshot().SceneRevision;
			client.DrainSoundEvents(revision);
			client.DrainTerrainPatches(revision);
			client.DrainPurchaseResults();
		}
		if (!client.GetConnectionError().empty()) {
			std::cerr << client.GetConnectionError() << '\n'; client.Disconnect(); return 1;
		}
		if (client.IsConnected() && client.HasPlayerAssignment() && client.HasSnapshot()) {
			const auto& snapshot = client.GetLatestSnapshot();
			const int slot = client.GetAssignedPlayerSlot();
			const auto controlledActor = slot >= 0 ? snapshot.Players[slot].ControlledActorId : 0;
			const auto humans = std::count_if(snapshot.Players.begin(), snapshot.Players.end(), [](const auto& player) { return player.ActiveHuman; });
			receivedObjectives = receivedObjectives || std::any_of(snapshot.Objectives.begin(), snapshot.Objectives.end(), [](const auto& objective) {
				return objective.Description == "Цель сети: Ёжик!" && objective.Team == 0 && objective.Arrow == 0;
			});
			receivedText = receivedText || std::any_of(snapshot.Players.begin(), snapshot.Players.end(), [](const auto& player) {
				return player.ActiveHuman && !player.ScreenText.empty();
			});
			if (!snapshot.Objects.empty() && !snapshot.ScenePreset.empty() && !snapshot.ActivityPreset.empty()) {
				if (!receivedWorld) { firstTick = snapshot.Tick; receivedWorld = true; }
				RTE::WorldStateProtocol::InputCommand command;
				const auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started).count();
				const bool silent = inputSilence && elapsedSeconds % 6 >= 4;
				if (silent && !wasSilent) ++silenceWindows;
				wasSilent = silent;
				if (!silent) command.ClientTick = ++commands; // Neutral input; no gameplay mutation requested.
				if (!silent && !client.SendInputCommand(command)) {
					std::cerr << "Probe failed to queue neutral input.\n"; client.Disconnect(); return 1;
				}
				if (client.GetReceivedSnapshotCount() >= 10 && static_cast<std::int32_t>(snapshot.Tick - firstTick) >= 60 &&
				    std::chrono::steady_clock::now() - started >= std::chrono::seconds(probeSeconds) && (!requireText || receivedText) && (!requireObjectives || receivedObjectives) &&
				    (!requireControl || (controlledActor != 0 && snapshot.SceneRevision >= minimumRevision)) && (!expectedHumans || humans == expectedHumans)) {
					std::cout << "result=passed snapshots=" << client.GetReceivedSnapshotCount() << " objects=" << snapshot.Objects.size()
					          << " tick_delta=" << snapshot.Tick - firstTick << " input_queued=" << commands
					          << " probe_seconds=" << probeSeconds << " silence_windows=" << silenceWindows
					          << " objectives_received=" << receivedObjectives
					          << " controlled_actor=" << controlledActor << " scene_revision=" << snapshot.SceneRevision
					          << " humans=" << humans
					          << " assigned_slot=" << client.GetAssignedPlayerSlot() << " hud_received=" << receivedText << '\n';
					client.Disconnect(); return 0;
				}
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	std::cerr << "Probe timed out waiting for a populated advancing world.\n";
	client.Disconnect(); return 1;
}

int RunServer(std::vector<std::string> arguments) {
	if (arguments.size() > 1 && arguments[1] == "--probe-client") return RunClientProbe(arguments);
	const bool selfTest = std::find(arguments.begin(), arguments.end(), "--self-test") != arguments.end();
	if (selfTest) {
		if (std::find(arguments.begin(), arguments.end(), "--server") != arguments.end()) {
			std::cerr << "Use --self-test without --server.\n"; return 2;
		}
		std::erase(arguments, "--self-test");
		arguments.insert(arguments.begin() + 1, {"-debug-run", "60", "-debug-run-headless-simulation", "-debug-run-output", "DedicatedServerSelfTest"});
	} else arguments.emplace_back("--server");
	std::vector<char*> pointers;
	for (std::string& argument : arguments) pointers.push_back(argument.data());
	pointers.push_back(nullptr);
	return RunCortexCommand(static_cast<int>(arguments.size()), pointers.data());
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
	SetConsoleOutputCP(CP_UTF8);
	std::vector<std::string> arguments;
	arguments.reserve(static_cast<std::size_t>(argc) + 1);
	for (int index = 0; index < argc; ++index) {
		const std::wstring_view wide(argv[index]);
		std::u16string utf16;
		for (const wchar_t character : wide) utf16.push_back(static_cast<char16_t>(character));
		arguments.push_back(RTE::UTF8::EncodeUTF16(utf16));
	}
	return RunServer(std::move(arguments));
}
#else
int main(int argc, char** argv) {
	std::vector<std::string> arguments;
	for (int index = 0; index < argc; ++index) arguments.emplace_back(argv[index]);
	return RunServer(std::move(arguments));
}
#endif
