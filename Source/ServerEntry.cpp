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
bool VerifyConstructionBoundary(RTE::WorldStateClientSession& client) {
	using namespace RTE::WorldStateProtocol;
	ConstructionRequest request;
	request.RequestId = 1; request.SceneRevision = client.GetLatestSnapshot().SceneRevision;
	request.Action = ConstructionAction::SetReady;
	const bool editing = client.GetLatestSnapshot().ActivityState == 2;
	const bool spectator = client.GetAssignedPlayerSlot() < 0;
	ConstructionResult original;
	auto checkReply = [&](const ConstructionRequest& command, bool accepted, const std::string& reason, ConstructionResult* saved = nullptr) {
		client.DrainConstructionResults();
		if (!client.SendConstructionRequest(command)) return false;
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
		while (std::chrono::steady_clock::now() < deadline) {
			client.Update();
			if (client.HasSnapshot()) {
				const auto revision = client.GetLatestSnapshot().SceneRevision;
				client.DrainSoundEvents(revision); client.DrainTerrainPatches(revision); client.DrainPurchaseResults();
			}
			client.SendInputCommand({});
			for (const auto& result : client.DrainConstructionResults()) {
				if (result.RequestId != command.RequestId) continue;
				if (result.Accepted != accepted || result.Reason != reason || result.SceneRevision != request.SceneRevision ||
				    result.ChargedCost != 0 || result.ObjectId != 0) {
					std::cerr << "Construction response mismatch id=" << result.RequestId << " accepted=" << result.Accepted << " reason=" << result.Reason << '\n';
					return false;
				}
				if (saved) *saved = result;
				std::this_thread::sleep_for(std::chrono::milliseconds(350));
				return true;
			}
			if (!client.GetConnectionError().empty()) return false;
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
		std::cerr << "Construction response timed out\n";
		return false;
	};
	const std::string denied = spectator ? "spectator" : "not_editing";
	if (!checkReply(request, editing && !spectator, editing && !spectator ? "" : denied, &original)) return false;
	if (!checkReply(request, original.Accepted, original.Reason)) return false;
	request.Ready = true;
	if (!checkReply(request, false, spectator ? "spectator" : "conflicting_request")) return false;
	request.Ready = false;
	if (!checkReply(request, original.Accepted, original.Reason)) return false;
	ConstructionRequest stale = request; stale.RequestId = 2; ++stale.SceneRevision;
	if (!checkReply(stale, false, spectator ? "spectator" : "stale_scene")) return false;
	request.RequestId = 3; request.Ready = true;
	if (!checkReply(request, false, spectator ? "spectator" : (editing ? "brain_not_placed" : "not_editing"))) return false;
	request.RequestId = 1; request.Ready = false;
	return checkReply(request, false, spectator ? "spectator" : "stale_request");
}

bool VerifyBrainConstructionStart(RTE::WorldStateClientSession& client) {
	using namespace RTE::WorldStateProtocol;
	ConstructionRequest request;
	request.RequestId = 1; request.SceneRevision = client.GetLatestSnapshot().SceneRevision;
	request.Action = ConstructionAction::PlaceBrain;
	request.X = 100.0F + 100.0F * client.GetAssignedPlayerSlot(); request.Y = 30;
	auto send = [&](const ConstructionRequest& command, bool accepted, const std::string& reason, ConstructionResult* saved = nullptr, bool paid = false) {
		client.DrainConstructionResults();
		if (!client.SendConstructionRequest(command)) return false;
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
		while (std::chrono::steady_clock::now() < deadline) {
			client.Update(); client.SendInputCommand({});
			if (client.HasSnapshot()) {
				const auto revision = client.GetLatestSnapshot().SceneRevision;
				client.DrainSoundEvents(revision); client.DrainTerrainPatches(revision); client.DrainPurchaseResults();
			}
			for (const auto& reply : client.DrainConstructionResults()) {
				if (reply.RequestId != command.RequestId) continue;
				if (reply.Accepted != accepted || reply.Reason != reason || reply.SceneRevision != request.SceneRevision ||
				    (paid ? (reply.ChargedCost <= 0 || reply.ObjectId == 0) : reply.ChargedCost != 0)) {
					std::cerr << "Brain response mismatch id=" << reply.RequestId << " accepted=" << reply.Accepted << " reason=" << reply.Reason << '\n'; return false;
				}
				if (saved) *saved = reply;
				std::this_thread::sleep_for(std::chrono::milliseconds(350)); return true;
			}
			if (!client.GetConnectionError().empty()) return false;
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
		std::cerr << "Brain construction reply timed out\n"; return false;
	};
	if (!send(request, true, "") || !send(request, true, "")) return false;
	const auto previewDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
	bool residentObserved = false;
	while (std::chrono::steady_clock::now() < previewDeadline) {
		client.Update(); client.SendInputCommand({});
		const int slot = client.GetAssignedPlayerSlot();
		const auto& state = client.GetConstructionState();
		if (client.HasConstructionState() && state.SceneRevision == request.SceneRevision && state.ActivityState == 2 && slot >= 0) {
			const auto& player = state.Players[slot];
			residentObserved = player.Active && !player.Ready && player.Brain && player.Brain->PositionX == request.X &&
			    player.Brain->PositionY == request.Y && !player.Brain->ClassName.empty() && !player.Brain->PresetName.empty();
			if (residentObserved) break;
		}
		if (!client.GetConnectionError().empty()) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	if (!residentObserved) { std::cerr << "Resident brain state not received before readiness\n"; return false; }
	ConstructionRequest invalid = request; invalid.RequestId = 2; invalid.X = -1;
	if (!send(invalid, false, "outside_scene")) return false;
	ConstructionRequest object;
	object.RequestId = 3; object.SceneRevision = request.SceneRevision; object.Action = ConstructionAction::PlaceObject;
	object.X = 400.0F + 100.0F * client.GetAssignedPlayerSlot(); object.Y = 30;
	object.Preset = {"HDFirearm", "Ronin.rte", "AK-47"};
	const int team = client.GetLatestSnapshot().Players[client.GetAssignedPlayerSlot()].Team;
	const float initialFunds = client.GetLatestSnapshot().TeamFunds[team];
	const bool affordable = initialFunds >= 160;
	ConstructionResult placed, replay;
	if (!send(object, affordable, affordable ? "" : "insufficient_funds", &placed, affordable) ||
	    !send(object, affordable, affordable ? "" : "insufficient_funds", &replay, affordable) || placed != replay) return false;
	const float expectedFunds = initialFunds - placed.ChargedCost;
	const auto fundsDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
	bool fundsObserved = false;
	while (std::chrono::steady_clock::now() < fundsDeadline) {
		client.Update(); client.SendInputCommand({});
		const auto& world = client.GetLatestSnapshot();
		const auto matching = std::count_if(world.Objects.begin(), world.Objects.end(), [&](const auto& entry) {
			return entry.ClassName == object.Preset.ClassName && entry.ModuleName == object.Preset.ModuleName && entry.PresetName == object.Preset.PresetName && entry.Team == team;
		});
		if (world.TeamFunds[team] == expectedFunds && matching == (affordable ? 1 : 0)) {
			for (const auto& entry : world.Objects) if (affordable && entry.ClassName == object.Preset.ClassName && entry.ModuleName == object.Preset.ModuleName && entry.PresetName == object.Preset.PresetName && entry.Team == team)
				std::cout << "construction_object_position=" << entry.PositionX << ',' << entry.PositionY << " requested=" << object.X << ',' << object.Y << '\n';
			fundsObserved = true; break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	if (!fundsObserved) { std::cerr << "Construction charged funds mismatch\n"; return false; }
	object.RequestId = 4; object.Preset.PresetName = "Missing construction preset";
	if (!send(object, false, "invalid_preset")) return false;
	object.RequestId = 5; object.Preset = {"Actor", "Base.rte", "Brain Case"};
	if (!send(object, false, "prohibited_item")) return false;
	object.RequestId = 6; object.Preset = {"HDFirearm", "Ronin.rte", "AK-47"}; object.X = -1;
	if (!send(object, false, "outside_scene")) return false;
	ConstructionRequest removal;
	removal.RequestId = 7; removal.SceneRevision = request.SceneRevision; removal.Action = ConstructionAction::RemoveObject;
	removal.ObjectId = affordable ? placed.ObjectId : std::numeric_limits<std::uint64_t>::max();
	ConstructionResult removed, removedReplay;
	if (!send(removal, affordable, affordable ? "" : "object_not_owned", &removed) ||
	    !send(removal, affordable, affordable ? "" : "object_not_owned", &removedReplay) || removed != removedReplay ||
	    (affordable && removed.ObjectId != placed.ObjectId)) return false;
	removal.RequestId = 8;
	if (!send(removal, false, "object_not_owned")) return false;
	const auto removeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
	bool deletionObserved = false;
	while (std::chrono::steady_clock::now() < removeDeadline) {
		client.Update(); client.SendInputCommand({});
		const auto& world = client.GetLatestSnapshot();
		const bool present = std::any_of(world.Objects.begin(), world.Objects.end(), [&](const auto& entry) {
			return entry.ClassName == "HDFirearm" && entry.ModuleName == "Ronin.rte" && entry.PresetName == "AK-47" && entry.Team == team;
		});
		if (!present && world.TeamFunds[team] == expectedFunds) { deletionObserved = true; break; }
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	if (!deletionObserved) { std::cerr << "Construction removal/funds mismatch\n"; return false; }
	std::cout << "construction_removal=passed removed=" << affordable << '\n';
	std::cout << "construction_objects=passed placed=" << affordable << " charged=" << placed.ChargedCost << '\n';
	request = {}; request.RequestId = 9; request.SceneRevision = client.GetLatestSnapshot().SceneRevision;
	request.Action = ConstructionAction::SetReady; request.Ready = true;
	return send(request, true, "");
}

int RunClientProbe(const std::vector<std::string>& arguments) {
	bool requireText = false, inputSilence = false, requireObjectives = false, requireControl = false;
	unsigned int probeSeconds = 3;
	unsigned int minimumRevision = 1;
	unsigned int expectedHumans = 0;
	bool constructionCheck = false, constructionPassed = false;
	bool brainStart = false, brainPlaced = false;
	if (arguments.size() < 4 || arguments[2].empty()) {
		std::cerr << "Usage: CortexCommandServer --probe-client ADDRESS PORT [--screen-text] [--probe-seconds 3..3600] [--input-silence]\n"; return 2;
	}
	for (std::size_t index = 4; index < arguments.size(); ++index) {
		if (arguments[index] == "--screen-text") requireText = true;
		else if (arguments[index] == "--objectives") requireObjectives = true;
		else if (arguments[index] == "--require-control") requireControl = true;
		else if (arguments[index] == "--construction-check") constructionCheck = true;
		else if (arguments[index] == "--brain-start") { brainStart = true; requireControl = true; }
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
			if (number.ec != std::errc{} || number.ptr != value.data() + value.size() || minimumRevision < 1 || minimumRevision > 65535) {
				std::cerr << "Minimum scene revision must be 1..65535.\n"; return 2;
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
			if (brainStart && !brainPlaced) {
				const auto& before = client.GetLatestSnapshot();
				const auto humans = std::count_if(before.Players.begin(), before.Players.end(), [](const auto& player) { return player.ActiveHuman; });
				if (client.GetAssignedPlayerSlot() < 0 || humans != 2) { client.SendInputCommand({}); std::this_thread::sleep_for(std::chrono::milliseconds(20)); continue; }
				if (before.ActivityState != 2 || !VerifyBrainConstructionStart(client)) { client.Disconnect(); return 1; }
				brainPlaced = true;
			}
			if (constructionCheck && !constructionPassed) {
				// The initial assignment is a spectator placeholder until Activity activation.
				if (client.GetAssignedPlayerSlot() < 0) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); continue; }
				if (!VerifyConstructionBoundary(client)) { client.Disconnect(); return 1; }
				constructionPassed = true;
			}
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
			if ((!snapshot.Objects.empty() || (constructionCheck && snapshot.ActivityState == 2)) && !snapshot.ScenePreset.empty() && !snapshot.ActivityPreset.empty()) {
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
					if (brainStart && (snapshot.ActivityState != 4 || !client.HasConstructionState() ||
					    client.GetConstructionState().SceneRevision != snapshot.SceneRevision || client.GetConstructionState().ActivityState != 4 ||
					    std::any_of(client.GetConstructionState().Players.begin(), client.GetConstructionState().Players.end(), [](const auto& player) { return player.Brain || player.Ready; }))) continue;
					std::cout << "result=passed snapshots=" << client.GetReceivedSnapshotCount() << " objects=" << snapshot.Objects.size()
					          << " tick_delta=" << snapshot.Tick - firstTick << " input_queued=" << commands
					          << " probe_seconds=" << probeSeconds << " silence_windows=" << silenceWindows
					          << " objectives_received=" << receivedObjectives
					          << " controlled_actor=" << controlledActor << " scene_revision=" << snapshot.SceneRevision
					          << " humans=" << humans
					          << " construction_checks=" << (constructionPassed ? 7 : 0)
					          << " brain_start=" << brainPlaced
					          << " resident_state_received=" << brainPlaced
					          << " activity_state=" << static_cast<int>(snapshot.ActivityState)
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
