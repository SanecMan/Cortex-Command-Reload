/*          ______   ______   ______  ______  ______  __  __       ______   ______   __    __   __    __   ______   __   __   _____
           /\  ___\ /\  __ \ /\  == \/\__  _\/\  ___\/\_\_\_\     /\  ___\ /\  __ \ /\ "-./  \ /\ "-./  \ /\  __ \ /\ "-.\ \ /\  __-.
           \ \ \____\ \ \/\ \\ \  __<\/_/\ \/\ \  __\\/_/\_\/_    \ \ \____\ \ \/\ \\ \ \-./\ \\ \ \-./\ \\ \  __ \\ \ \-.  \\ \ \/\ \
            \ \_____\\ \_____\\ \_\ \_\ \ \_\ \ \_____\/\_\/\_\    \ \_____\\ \_____\\ \_\ \ \_\\ \_\ \ \_\\ \_\ \_\\ \_\\"\_\\ \____-
             \/_____/ \/_____/ \/_/ /_/  \/_/  \/_____/\/_/\/_/     \/_____/ \/_____/ \/_/  \/_/ \/_/  \/_/ \/_/\/_/ \/_/ \/_/ \/____/
   ______   ______   __    __   __    __   __  __   __   __   __   ______  __  __       ______  ______   ______      __   ______   ______   ______
  /\  ___\ /\  __ \ /\ "-./  \ /\ "-./  \ /\ \/\ \ /\ "-.\ \ /\ \ /\__  _\/\ \_\ \     /\  == \/\  == \ /\  __ \    /\ \ /\  ___\ /\  ___\ /\__  _\
  \ \ \____\ \ \/\ \\ \ \-./\ \\ \ \-./\ \\ \ \_\ \\ \ \-.  \\ \ \\/_/\ \/\ \____ \    \ \  _-/\ \  __< \ \ \/\ \  _\_\ \\ \  __\ \ \ \____\/_/\ \/
   \ \_____\\ \_____\\ \_\ \ \_\\ \_\ \ \_\\ \_____\\ \_\\"\_\\ \_\  \ \_\ \/\_____\    \ \_\   \ \_\ \_\\ \_____\/\_____\\ \_____\\ \_____\  \ \_\
    \/_____/ \/_____/ \/_/  \/_/ \/_/  \/_/ \/_____/ \/_/ \/_/ \/_/   \/_/  \/_____/     \/_/    \/_/ /_/ \/_____/\/_____/ \/_____/ \/_____/   \/_/

/////\\\\\/////\\\\\/////\\\\\/////\\\\\/////\\\\\/////\\\\\/////\\\\\/////\\\\\/////\\\\\/////\\\\\/////\\\\\/////\\\\\/////\\\\\/////\\\\\/////\\\\\*/

/// <summary>
/// Main driver implementation of the Retro Terrain Engine.
/// Data Realms, LLC - http://www.datarealms.com
/// Cortex Command Community Project - https://github.com/cortex-command-community
/// Cortex Command Community Project Discord - https://discord.gg/TSU6StNQUG
/// </summary>

#include "allegro.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include "GUI.h"
#include "GUIInputWrapper.h"
#include "AllegroScreen.h"
#include "AllegroBitmap.h"

#include "MainMenuGUI.h"
#include "ScenarioGUI.h"
#include "PauseMenuGUI.h"
#include "TitleScreen.h"
#include "LoadingScreen.h"

#include "MenuMan.h"
#include "ConsoleMan.h"
#include "SettingsMan.h"
#include "PresetMan.h"
#include "UInputMan.h"
#include "PerformanceMan.h"
#include "FrameMan.h"
#include "PostProcessMan.h"
#include "SceneMan.h"
#include "Scene.h"
#include "SLTerrain.h"
#include "MetaMan.h"
#include "WindowMan.h"
#include "GLResourceMan.h"
#include "CameraMan.h"
#include "ActivityMan.h"
#include "MovableMan.h"
#include "MovableObject.h"
#include "PrimitiveMan.h"
#include "ThreadMan.h"
#include "LuaMan.h"
#include "MusicMan.h"
#include "System.h"
#include "System/UTF8.h"
#include "System/WorldStateProtocol.h"
#include "System/WorldStateTerrainTest.h"
#include "WorldStateCompression.h"
#include "WorldStateTransport.h"
#include "WorldStateSnapshotBuilder.h"
#include "WorldStateClientSession.h"
#include "WorldStateClientReplica.h"
#include "WorldStateServerSession.h"
#include "NetworkMessages.h"
#include "DiscordPresence.h"

#include "RenderTarget.h"
#include "tracy/Tracy.hpp"

#include "imgui_impl_sdl3.h"

#ifdef _WIN32
#include "windows.h"
#include <psapi.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string_view>
#include <thread>

extern "C" {
FILE __iob_func[3] = {*stdin, *stdout, *stderr};
}

using namespace RTE;

namespace {
	bool worldStateServerRequested = false;
	unsigned short worldStateServerPort = 8000;
	unsigned short worldStateServerMaxPlayers = 8;
	std::string worldStateServerBindAddress = "0.0.0.0";
	std::unique_ptr<WorldStateServerSession> worldStateServer;
	std::uint32_t worldStateServerSimulationTick = 0;
	std::string worldStateClientAddress;
	unsigned short worldStateClientPort = 8000;
	std::unique_ptr<WorldStateClientSession> worldStateClient;
	WorldStateClientReplica worldStateClientReplica;
	std::ofstream worldStateClientLog;
	std::uint32_t worldStateClientLastObservedSnapshotCount = 0;
	std::uint32_t worldStateClientLastLoggedSnapshotCount = 0;
	std::uint32_t worldStateClientAppliedSceneRevision = 0;
	std::uint32_t worldStateClientRejectedSceneRevision = 0;

	bool QueueWorldStateActivity(const WorldStateProtocol::Snapshot& snapshot, std::string& failureReason) {
		if (snapshot.ActivityClassName.empty() || snapshot.ActivityPreset.empty() || snapshot.ScenePreset.empty()) {
			failureReason = "server snapshot does not identify an Activity and Scene";
			return false;
		}

		const int activityModule = snapshot.ActivityModuleName.empty() ? -1 : g_PresetMan.GetModuleID(snapshot.ActivityModuleName);
		const Entity* activityPreset = g_PresetMan.GetEntityPreset(snapshot.ActivityClassName, snapshot.ActivityPreset, activityModule);
		Entity* activityClone = activityPreset ? activityPreset->Clone() : nullptr;
		Activity* activity = dynamic_cast<Activity*>(activityClone);
		if (!activity) {
			delete activityClone;
			failureReason = "server Activity preset is unavailable locally: " + snapshot.ActivityModuleName + "/" + snapshot.ActivityClassName + "/" + snapshot.ActivityPreset;
			return false;
		}

		const int sceneResult = snapshot.SceneModuleName.empty() ? g_SceneMan.SetSceneToLoad(snapshot.ScenePreset)
		                                                       : g_SceneMan.SetSceneToLoad(snapshot.ScenePreset, snapshot.SceneModuleName);
		if (sceneResult < 0) {
			delete activity;
			failureReason = "server Scene preset is unavailable locally: " + snapshot.SceneModuleName + "/" + snapshot.ScenePreset;
			return false;
		}

		g_ActivityMan.SetStartActivity(activity);
		g_ActivityMan.SetRestartActivity();
		return true;
	}

	struct DebugRunState {
		bool Enabled = false;
		bool StressStarted = false;
		bool Passed = false;
		bool HostSessionTestPassed = true;
		bool OverlayCaptureEnabled = false;
		bool PreflightOnly = false;
		bool RequestHostWorldStateTransition = false;
		bool RequireWorldStateTransition = false;
		bool RequestHostTerrainMutationSmoke = false;
		bool RequireRemoteTerrainMutationSmoke = false;
		bool RemoteTerrainMutationObserved = false;
		bool TerrainMutationProbeLocated = false;
		int TerrainMutationProbeX = 0;
		int TerrainMutationProbeY = 0;
		bool WorldStateTransitionObserved = false;
		bool HostWorldStateTransitionQueued = false;
		bool PostTransitionStressBatchSpawned = false;
		int WorldStateTransitionObservedUpdate = 0;
		std::string ExpectedTransitionActivity = "Skirmish Defense";
		std::string ExpectedTransitionScene = "Ketanot Hills";
		int UpdateLimit = 600;
		int SimulationUpdates = 0;
		int RenderedFrames = 0;
		int InitialActorCount = 0;
		int InitialParticleCount = 0;
		double TotalFrameTimeMilliseconds = 0.0;
		double TotalRenderTimeMilliseconds = 0.0;
		bool InitialScreenshotCaptured = false;
		bool MidpointScreenshotCaptured = false;
		bool FinalScreenshotCaptured = false;
		std::filesystem::path OutputDirectory;
		std::string OutputDirectoryName = "DebugRuns";
		std::ofstream Log;
		std::chrono::steady_clock::time_point StartTime;
		std::chrono::steady_clock::time_point ModuleLoadStartTime;
		std::chrono::steady_clock::time_point ModuleLoadEndTime;
		std::chrono::steady_clock::time_point SimulationStartTime;
	};

	DebugRunState& GetDebugRunState() {
		static DebugRunState state;
		return state;
	}

	std::uint64_t GetProcessResidentMemoryBytes() {
#ifdef _WIN32
		PROCESS_MEMORY_COUNTERS_EX counters{};
		if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
			return static_cast<std::uint64_t>(counters.WorkingSetSize);
		}
#elif defined(__linux__)
		std::ifstream statm("/proc/self/statm");
		std::uint64_t totalPages = 0;
		std::uint64_t residentPages = 0;
		if (statm >> totalPages >> residentPages) {
			const long pageSize = sysconf(_SC_PAGESIZE);
			if (pageSize > 0) {
				return residentPages * static_cast<std::uint64_t>(pageSize);
			}
		}
#endif
		return 0;
	}

	bool VerifyWorldStateTransportLoopback(const WorldStateProtocol::Snapshot& expectedSnapshot, std::ofstream& log);

	SDL_AssertState SDLCALL DebugRunSDLAssertionHandler(const SDL_AssertData* assertion, void*) {
		DebugRunState& state = GetDebugRunState();
		if (state.Log.is_open()) {
			state.Log << "sdl_assert=" << (assertion->filename ? assertion->filename : "unknown") << ':' << assertion->linenum
			          << " function=" << (assertion->function ? assertion->function : "unknown")
			          << " condition=" << (assertion->condition ? assertion->condition : "unknown") << '\n' << std::flush;
		}
		const std::string_view filename = assertion->filename ? assertion->filename : "";
		const std::string_view function = assertion->function ? assertion->function : "";
		if (filename.ends_with("SDL_hid.c") && function == "WIN_QuitDeviceNotification") {
			return SDL_ASSERTION_IGNORE;
		}
		return SDL_ASSERTION_ABORT;
	}

	bool ParseDebugRunArguments(int argc, char** argv) {
		DebugRunState& state = GetDebugRunState();
		for (int i = 1; i < argc; ++i) {
			const std::string_view argument(argv[i]);
			if (argument == "-debug-overlay") state.OverlayCaptureEnabled = true;
			if (argument == "-debug-run-preflight-only") state.PreflightOnly = true;
			if (argument == "-debug-run-world-state-transition") state.RequestHostWorldStateTransition = true;
			if (argument == "-debug-run-require-world-state-transition") state.RequireWorldStateTransition = true;
			if (argument == "-debug-run-world-state-terrain-mutation") state.RequestHostTerrainMutationSmoke = true;
			if (argument == "-debug-run-require-world-state-terrain-mutation") state.RequireRemoteTerrainMutationSmoke = true;
			if (argument == "-debug-run-output" && i + 1 < argc) state.OutputDirectoryName = argv[++i];
		}
		for (int i = 1; i < argc; ++i) {
			if (std::string_view(argv[i]) != "-debug-run") {
				continue;
			}
			state.Enabled = true;
			if (i + 1 < argc) {
				int requestedUpdates = 0;
				const std::string_view nextArgument(argv[i + 1]);
				const auto [end, error] = std::from_chars(nextArgument.data(), nextArgument.data() + nextArgument.size(), requestedUpdates);
				if (error == std::errc{} && end == nextArgument.data() + nextArgument.size() && requestedUpdates > 0) {
					state.UpdateLimit = std::clamp(requestedUpdates, 60, 36000);
					++i;
				}
			}
			return true;
		}
		return false;
	}

	bool StartDebugRun() {
		DebugRunState& state = GetDebugRunState();
		if (!state.Enabled) {
			return true;
		}

		const std::filesystem::path outputName(state.OutputDirectoryName);
		if (outputName.empty() || outputName.has_parent_path() || outputName.filename() != outputName || outputName == "." || outputName == ".." ||
		    state.OutputDirectoryName.find_first_of("/\\:") != std::string::npos) {
			std::cerr << "Debug run output must be a single directory name under the screenshots directory.\n";
			return false;
		}
		state.OutputDirectory = std::filesystem::path(System::GetWorkingDirectory()) / System::GetScreenshotDirectory() / outputName;
		std::error_code filesystemError;
		const std::filesystem::file_status existingPathStatus = std::filesystem::symlink_status(state.OutputDirectory, filesystemError);
		if (!filesystemError && std::filesystem::is_symlink(existingPathStatus)) {
			std::cerr << "Debug run output path must not be a symlink: " << state.OutputDirectory.string() << '\n';
			return false;
		}
		filesystemError.clear();
		std::filesystem::create_directories(state.OutputDirectory, filesystemError);
		if (filesystemError) {
			std::cerr << "Unable to create debug run output directory: " << filesystemError.message() << '\n';
			return false;
		}
		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(state.OutputDirectory, filesystemError)) {
			if (filesystemError) {
				break;
			}
			const std::string name = entry.path().filename().string();
			const bool isDebugArtifact = name == "DebugRun.log" || name == "LogLoading.txt" || name == "LogLoadingWarning.txt" || name == "LogConsole.txt" || name.starts_with("debug-run-");
			if (isDebugArtifact && (entry.is_regular_file(filesystemError) || entry.is_symlink(filesystemError))) {
				filesystemError.clear();
				std::filesystem::remove(entry.path(), filesystemError);
			}
			filesystemError.clear();
		}
		state.Log.open(state.OutputDirectory / "DebugRun.log", std::ios::out | std::ios::trunc);
		if (!state.Log) {
			std::cerr << "Unable to open DebugRun.log for writing.\n";
			return false;
		}
		const std::string utf8Probe = "Aя🙂";
		std::string utf8EncodedProbe;
		UTF8::AppendCodepoint(utf8EncodedProbe, 0x1F642);
		std::uint32_t decodedEmoji = 0;
		std::size_t decodedEmojiBytes = 0;
		const bool utf8DecodePassed = UTF8::CountCodepoints(utf8Probe) == 3 && UTF8::NextBoundary(utf8Probe, 1) == 3 &&
		                              UTF8::PreviousBoundary(utf8Probe, 3) == 1 && UTF8::CountCodepoints("\xF0\x28\x8C\x28") == 4 &&
		                              utf8EncodedProbe == "🙂" && UTF8::Decode(utf8EncodedProbe, 0, decodedEmoji, decodedEmojiBytes) &&
		                              decodedEmoji == 0x1F642 && decodedEmojiBytes == 4;
		state.Log << "utf8_decoder_smoke=" << (utf8DecodePassed ? "passed" : "failed") << '\n' << std::flush;
		if (!utf8DecodePassed) {
			return false;
		}
		std::u16string unmatchedSurrogate = u"bad ";
		unmatchedSurrogate.push_back(static_cast<char16_t>(0xD800));
		unmatchedSurrogate += u" surrogate";
		const bool utf16BridgePassed = UTF8::EncodeUTF16(u"Сбой 🙂") == "Сбой 🙂" && UTF8::EncodeUTF16(unmatchedSurrogate) == "bad � surrogate";
		state.Log << "utf8_utf16_bridge_smoke=" << (utf16BridgePassed ? "passed" : "failed") << '\n' << std::flush;
		if (!utf16BridgePassed) {
			return false;
		}
		const std::string unicodePath = UTF8::PathToString(state.OutputDirectory / std::filesystem::u8path("тест-🙂.tmp"));
		constexpr std::string_view fileProbe = "UTF-8 file path round trip";
		FILE* unicodeFile = UTF8::OpenFile(unicodePath, "wb");
		bool unicodeFileWritten = unicodeFile && std::fwrite(fileProbe.data(), 1, fileProbe.size(), unicodeFile) == fileProbe.size();
		if (unicodeFile && std::fclose(unicodeFile) != 0) {
			unicodeFileWritten = false;
		}
		unicodeFile = unicodeFileWritten ? UTF8::OpenFile(unicodePath, "rb") : nullptr;
		std::array<char, fileProbe.size()> fileContents{};
		const bool unicodeFileRead = unicodeFile && std::fread(fileContents.data(), 1, fileContents.size(), unicodeFile) == fileContents.size() &&
		                             std::string_view(fileContents.data(), fileContents.size()) == fileProbe;
		if (unicodeFile) {
			std::fclose(unicodeFile);
		}
		const bool unicodePathResolved = unicodeFileWritten && System::PathExistsCaseSensitive(unicodePath);
		std::error_code unicodeFileCleanupError;
		std::filesystem::remove(UTF8::PathFromString(unicodePath), unicodeFileCleanupError);
		const bool unicodeFilePathPassed = unicodeFileWritten && unicodeFileRead && unicodePathResolved && !unicodeFileCleanupError;
		state.Log << "utf8_file_path_smoke=" << (unicodeFilePathPassed ? "passed" : "failed") << '\n' << std::flush;
		if (!unicodeFilePathPassed) {
			return false;
		}
		WorldStateProtocol::Snapshot networkSnapshot;
		networkSnapshot.Tick = 1234;
		networkSnapshot.SceneRevision = 7;
		networkSnapshot.ActivityClassName = "GATutorial";
		networkSnapshot.ActivityPreset = "Tutorial Mission";
		networkSnapshot.ActivityModuleName = "Base.rte";
		networkSnapshot.SceneModuleName = "Base.rte";
		networkSnapshot.ScenePreset = "Tutorial Bunker";
		WorldStateProtocol::ObjectState networkActor;
		networkActor.NetworkId = 0x1020304050607080ULL;
		networkActor.ClassName = "AHuman";
		networkActor.ModuleName = "Base.rte";
		networkActor.PresetName = "Пример актора";
		networkActor.PositionX = 123.5F;
		networkActor.PositionY = 42.25F;
		networkActor.VelocityX = -2.0F;
		networkActor.VelocityY = 0.75F;
		networkActor.Rotation = 1.25F;
		networkActor.AngularVelocity = -0.5F;
		networkActor.Health = 83.0F;
		networkActor.SpriteFrame = 3;
		networkActor.Team = 1;
		networkActor.Flags = WorldStateProtocol::c_ObjectFlagActor;
		networkActor.HFlipped = true;
		networkSnapshot.Objects.push_back(networkActor);
		WorldStateProtocol::ObjectState transientPixel;
		transientPixel.NetworkId = 0x8877665544332211ULL;
		transientPixel.ClassName = "MOPixel";
		transientPixel.PresetName = "None";
		transientPixel.PositionX = 5.0F;
		transientPixel.PositionY = 6.0F;
		transientPixel.PixelMaterialId = 1;
		transientPixel.PixelColorIndex = 42;
		transientPixel.PixelMass = 0.25F;
		transientPixel.PixelLifetime = 350;
		transientPixel.PixelSharpness = 0.8F;
		transientPixel.Flags = WorldStateProtocol::c_ObjectFlagTransientPixel;
		networkSnapshot.Objects.push_back(transientPixel);
		std::vector<std::uint8_t> networkPacket;
		WorldStateProtocol::Snapshot decodedSnapshot;
		std::uint32_t decodedSequence = 0;
		const bool networkRoundTripPassed = WorldStateProtocol::EncodeSnapshot(networkSnapshot, 99, networkPacket) &&
		                                   WorldStateProtocol::DecodeSnapshot(networkPacket, decodedSnapshot, &decodedSequence) &&
			                                   decodedSequence == 99 && decodedSnapshot.Tick == networkSnapshot.Tick &&
			                                   decodedSnapshot.SceneRevision == networkSnapshot.SceneRevision && decodedSnapshot.Objects.size() == 2 &&
			                                   decodedSnapshot.ActivityClassName == networkSnapshot.ActivityClassName &&
			                                   decodedSnapshot.ActivityPreset == networkSnapshot.ActivityPreset &&
			                                   decodedSnapshot.ActivityModuleName == networkSnapshot.ActivityModuleName &&
			                                   decodedSnapshot.SceneModuleName == networkSnapshot.SceneModuleName &&
			                                   decodedSnapshot.ScenePreset == networkSnapshot.ScenePreset &&
		                                   decodedSnapshot.Objects.front().NetworkId == networkActor.NetworkId &&
		                                   decodedSnapshot.Objects.front().PresetName == networkActor.PresetName &&
		                                   decodedSnapshot.Objects.front().PositionX == networkActor.PositionX &&
		                                   decodedSnapshot.Objects.front().Team == networkActor.Team && decodedSnapshot.Objects.front().Flags == networkActor.Flags &&
		                                   decodedSnapshot.Objects.front().SpriteFrame == networkActor.SpriteFrame && decodedSnapshot.Objects.front().HFlipped == networkActor.HFlipped &&
		                                   decodedSnapshot.Objects.back().PixelMaterialId == transientPixel.PixelMaterialId &&
		                                   decodedSnapshot.Objects.back().PixelColorIndex == transientPixel.PixelColorIndex &&
		                                   decodedSnapshot.Objects.back().PixelMass == transientPixel.PixelMass &&
		                                   decodedSnapshot.Objects.back().PixelLifetime == transientPixel.PixelLifetime &&
		                                   decodedSnapshot.Objects.back().PixelSharpness == transientPixel.PixelSharpness;
		std::vector<std::uint8_t> truncatedNetworkPacket = networkPacket;
		if (!truncatedNetworkPacket.empty()) truncatedNetworkPacket.pop_back();
		std::vector<std::uint8_t> unsupportedVersionPacket = networkPacket;
		if (unsupportedVersionPacket.size() > 4) unsupportedVersionPacket[4] = static_cast<std::uint8_t>(WorldStateProtocol::c_Version + 1);
		WorldStateProtocol::Snapshot duplicateIdSnapshot = networkSnapshot;
		duplicateIdSnapshot.Objects.push_back(networkActor);
		std::vector<std::uint8_t> duplicateIdPacket;
		WorldStateProtocol::Snapshot invalidTeamSnapshot = networkSnapshot;
		invalidTeamSnapshot.Objects.front().Team = 32767;
		WorldStateProtocol::Snapshot invalidFlagsSnapshot = networkSnapshot;
		invalidFlagsSnapshot.Objects.front().Flags = 3;
		const bool malformedNetworkRejected = !networkPacket.empty() &&
		                                     !WorldStateProtocol::DecodeSnapshot(truncatedNetworkPacket, decodedSnapshot) &&
		                                     !WorldStateProtocol::DecodeSnapshot(unsupportedVersionPacket, decodedSnapshot) &&
		                                     !WorldStateProtocol::EncodeSnapshot(duplicateIdSnapshot, 100, duplicateIdPacket) &&
		                                     !WorldStateProtocol::EncodeSnapshot(invalidTeamSnapshot, 100, duplicateIdPacket) &&
		                                     !WorldStateProtocol::EncodeSnapshot(invalidFlagsSnapshot, 100, duplicateIdPacket);
		state.Log << "world_state_protocol_smoke=" << (networkRoundTripPassed && malformedNetworkRejected ? "passed" : "failed") << '\n' << std::flush;
		if (!networkRoundTripPassed || !malformedNetworkRejected) {
			return false;
		}
		WorldStateProtocol::InputCommand inputCommand;
		inputCommand.ClientTick = 345;
		inputCommand.HeldElements = (std::uint64_t{1} << InputElements::INPUT_FIRE) | (std::uint64_t{1} << InputElements::INPUT_L_UP);
		inputCommand.MouseDeltaX = -125;
		inputCommand.MouseDeltaY = 72;
		inputCommand.MouseWheelDelta = 1;
		inputCommand.MouseButtonsHeld = static_cast<std::uint8_t>(1U << MouseButtons::MOUSE_LEFT);
		inputCommand.AnalogMoveX = 16384;
		inputCommand.AnalogMoveY = -8192;
		inputCommand.AnalogAimX = -32767;
		inputCommand.AnalogAimY = 32767;
		inputCommand.RestartActivityVote = true;
		std::vector<std::uint8_t> inputPacket;
		WorldStateProtocol::InputCommand decodedInputCommand;
		std::uint32_t decodedInputSequence = 0;
		const bool inputCommandRoundTripPassed = WorldStateProtocol::EncodeInputCommand(inputCommand, 18, inputPacket) &&
		                                        WorldStateProtocol::DecodeInputCommand(inputPacket, decodedInputCommand, &decodedInputSequence) &&
		                                        decodedInputSequence == 18 && decodedInputCommand.ClientTick == inputCommand.ClientTick &&
		                                        decodedInputCommand.HeldElements == inputCommand.HeldElements &&
		                                        decodedInputCommand.MouseDeltaX == inputCommand.MouseDeltaX &&
		                                        decodedInputCommand.MouseDeltaY == inputCommand.MouseDeltaY &&
		                                        decodedInputCommand.MouseWheelDelta == inputCommand.MouseWheelDelta &&
		                                        decodedInputCommand.MouseButtonsHeld == inputCommand.MouseButtonsHeld &&
		                                        decodedInputCommand.AnalogMoveX == inputCommand.AnalogMoveX &&
		                                        decodedInputCommand.AnalogMoveY == inputCommand.AnalogMoveY &&
		                                        decodedInputCommand.AnalogAimX == inputCommand.AnalogAimX &&
		                                        decodedInputCommand.AnalogAimY == inputCommand.AnalogAimY &&
		                                        decodedInputCommand.RestartActivityVote && !decodedInputCommand.ResetActivityVote;
		WorldStateProtocol::ClientAssignment assignment{Players::PlayerTwo};
		std::vector<std::uint8_t> assignmentPacket;
		WorldStateProtocol::ClientAssignment decodedAssignment;
		std::uint32_t assignmentSequence = 0;
		const bool assignmentRoundTripPassed = WorldStateProtocol::EncodeClientAssignment(assignment, 20, assignmentPacket) &&
		                                      WorldStateProtocol::DecodeClientAssignment(assignmentPacket, decodedAssignment, &assignmentSequence) &&
		                                      decodedAssignment.PlayerSlot == Players::PlayerTwo && assignmentSequence == 20;
		WorldStateProtocol::InputCommand invalidInputCommand = inputCommand;
		invalidInputCommand.MouseDeltaX = WorldStateProtocol::c_MaxMouseDelta + 1;
		std::vector<std::uint8_t> invalidInputPacket;
		const bool invalidInputRejected = !WorldStateProtocol::EncodeInputCommand(invalidInputCommand, 19, invalidInputPacket) &&
		                                 !inputPacket.empty() && !WorldStateProtocol::DecodeInputCommand(std::span(inputPacket).first(inputPacket.size() - 1), decodedInputCommand);
		WorldStateProtocol::TerrainPatch terrainPatch;
		terrainPatch.SceneRevision = 7;
		terrainPatch.X = 128;
		terrainPatch.Y = 64;
		terrainPatch.Width = 2;
		terrainPatch.Height = 2;
		terrainPatch.Layer = WorldStateProtocol::TerrainLayer::Foreground;
		terrainPatch.Pixels = {3, 17, 128, 255};
		std::vector<std::uint8_t> terrainPacket;
		WorldStateProtocol::TerrainPatch decodedTerrainPatch;
		std::uint32_t terrainSequence = 0;
		const bool terrainPatchRoundTripPassed = WorldStateProtocol::EncodeTerrainPatch(terrainPatch, 21, terrainPacket) &&
		                                        WorldStateProtocol::DecodeTerrainPatch(terrainPacket, decodedTerrainPatch, &terrainSequence) &&
		                                        terrainSequence == 21 && decodedTerrainPatch.SceneRevision == terrainPatch.SceneRevision &&
		                                        decodedTerrainPatch.X == terrainPatch.X && decodedTerrainPatch.Y == terrainPatch.Y &&
		                                        decodedTerrainPatch.Width == terrainPatch.Width && decodedTerrainPatch.Height == terrainPatch.Height &&
		                                        decodedTerrainPatch.Layer == terrainPatch.Layer && decodedTerrainPatch.Pixels == terrainPatch.Pixels;
		WorldStateProtocol::TerrainPatch invalidTerrainPatch = terrainPatch;
		invalidTerrainPatch.Pixels.pop_back();
		const bool invalidTerrainPatchRejected = !WorldStateProtocol::EncodeTerrainPatch(invalidTerrainPatch, 22, terrainPacket) &&
		                                        !WorldStateProtocol::DecodeTerrainPatch(std::span(terrainPacket).first(terrainPacket.size() - 1), decodedTerrainPatch);
		state.Log << "world_state_input_codec_smoke=" << (inputCommandRoundTripPassed && invalidInputRejected ? "passed" : "failed") << '\n' << std::flush;
		state.Log << "world_state_assignment_codec_smoke=" << (assignmentRoundTripPassed ? "passed" : "failed") << '\n' << std::flush;
		state.Log << "world_state_terrain_patch_codec_smoke=" << (terrainPatchRoundTripPassed && invalidTerrainPatchRejected ? "passed" : "failed") << '\n' << std::flush;
		TerrainDirtyGrid dirtyGrid;
		const bool dirtyGridConfigured = dirtyGrid.Configure(130, 130, 64, false);
		dirtyGrid.Mark(4, 5, 2, 2);
		dirtyGrid.Mark(60, 60, 20, 20);
		const std::vector<TerrainDirtyGrid::Rectangle> coalescedTerrainChanges = dirtyGrid.Drain();
		const bool dirtyGridCoalescingPassed = dirtyGridConfigured && coalescedTerrainChanges.size() == 1 &&
		                                      coalescedTerrainChanges.front().X == 0 && coalescedTerrainChanges.front().Y == 0 &&
		                                      coalescedTerrainChanges.front().Width == 128 && coalescedTerrainChanges.front().Height == 128 &&
		                                      dirtyGrid.Drain().empty();
		TerrainDirtyGrid wrappingDirtyGrid;
		const bool wrappingDirtyGridConfigured = wrappingDirtyGrid.Configure(192, 130, 64, true);
		wrappingDirtyGrid.Mark(-4, 4, 8, 8);
		wrappingDirtyGrid.Mark(4, 128, 8, 8);
		const std::vector<TerrainDirtyGrid::Rectangle> wrappedTerrainChanges = wrappingDirtyGrid.Drain();
		const bool dirtyGridWrappingPassed = wrappingDirtyGridConfigured && wrappedTerrainChanges.size() == 3 &&
		                                    wrappedTerrainChanges[0].X == 0 && wrappedTerrainChanges[0].Y == 0 && wrappedTerrainChanges[0].Width == 64 && wrappedTerrainChanges[0].Height == 64 &&
		                                    wrappedTerrainChanges[1].X == 128 && wrappedTerrainChanges[1].Y == 0 && wrappedTerrainChanges[1].Width == 64 && wrappedTerrainChanges[1].Height == 64 &&
		                                    wrappedTerrainChanges[2].X == 0 && wrappedTerrainChanges[2].Y == 128 && wrappedTerrainChanges[2].Width == 64 && wrappedTerrainChanges[2].Height == 2;
		state.Log << "world_state_terrain_dirty_grid_smoke=" << (dirtyGridCoalescingPassed && dirtyGridWrappingPassed ? "passed" : "failed")
		          << " coalesced=" << coalescedTerrainChanges.size() << " wrapped=" << wrappedTerrainChanges.size();
		for (const TerrainDirtyGrid::Rectangle& rectangle : wrappedTerrainChanges) {
			state.Log << " [" << rectangle.X << ',' << rectangle.Y << ',' << rectangle.Width << ',' << rectangle.Height << ']';
		}
		state.Log << '\n' << std::flush;
		if (!inputCommandRoundTripPassed || !invalidInputRejected || !assignmentRoundTripPassed || !terrainPatchRoundTripPassed || !invalidTerrainPatchRejected ||
		    !dirtyGridCoalescingPassed || !dirtyGridWrappingPassed ||
		    WorldStateProtocol::c_InputElementCount != InputElements::INPUT_COUNT) {
			return false;
		}
		if (!VerifyWorldStateTransportLoopback(networkSnapshot, state.Log)) {
			return false;
		}
		const std::string legacyCyrillicProbe("\xCF\xF0\xE8\xE2\xE5\xF2", 6);
		const bool legacyEncodingPassed = UTF8::PreserveLegacyWindows1251(legacyCyrillicProbe) == "Привет" &&
		                                  UTF8::PreserveLegacyWindows1251("ASCII content is unchanged") == "ASCII content is unchanged" &&
		                                  UTF8::PreserveLegacyWindows1251("Already UTF-8: Привет") == "Already UTF-8: Привет";
		state.Log << "legacy_windows_1251_smoke=" << (legacyEncodingPassed ? "passed" : "failed") << '\n' << std::flush;
		if (!legacyEncodingPassed) {
			return false;
		}
		std::istringstream bomStream("\xEF\xBB\xBF" "ModuleName = UTF-8");
		UTF8::SkipByteOrderMark(bomStream);
		const bool utf8BOMPassed = bomStream.peek() == 'M';
		state.Log << "utf8_bom_smoke=" << (utf8BOMPassed ? "passed" : "failed") << '\n' << std::flush;
		if (!utf8BOMPassed) {
			return false;
		}
		state.StartTime = std::chrono::steady_clock::now();
		state.Log << "mode=" << (state.PreflightOnly ? "preflight-only" : "automated-gameplay-smoke")
		          << "\nupdates=" << state.UpdateLimit << "\nscene=Tutorial Bunker\n" << std::flush;
		return true;
	}

	void SpawnDebugStressBatch() {
		DebugRunState& state = GetDebugRunState();
		std::ostringstream script;
		script << "for i = 1, 16 do "
		          "local actor = CreateAHuman('Fat Culled Clone', 'Base.rte'); "
		          "if actor then actor.Team = 0; actor.Pos = SceneMan:MovePointToGround(Vector(180 + i * 8, 0), 0, 3); MovableMan:AddActor(actor); end; "
		          "end; "
		          "for i = 1, 256 do "
		          "local particle = CreateMOPixel('Explosion Flame Glow', 'Base.rte'); "
		          "if particle then particle.Pos = Vector(220 + math.random(-100, 100), 260 + math.random(-50, 50)); particle.Vel = Vector(math.random(-30, 30), math.random(-30, 30)); MovableMan:AddParticle(particle); end; "
		          "end";
		const int result = g_LuaMan.GetMasterScriptState().RunScriptString(script.str());
		const int actorCount = g_MovableMan.GetActorCount();
		const int particleCount = g_MovableMan.GetParticleCount();
		state.Log << "stress_batch_update=" << state.SimulationUpdates << " result=" << result
		          << " actors=" << actorCount << " particles=" << particleCount << '\n' << std::flush;
		state.StressStarted = state.StressStarted || (result >= 0 && actorCount > state.InitialActorCount && particleCount > state.InitialParticleCount);
	}

	void AdvanceDebugRunSimulation() {
		DebugRunState& state = GetDebugRunState();
		if (!state.Enabled || System::IsSetToQuit()) {
			return;
		}
		++state.SimulationUpdates;
		if (state.SimulationUpdates == 1) {
			state.InitialActorCount = g_MovableMan.GetActorCount();
			state.InitialParticleCount = g_MovableMan.GetParticleCount();
		}
		if (state.SimulationUpdates == 1 || state.SimulationUpdates == state.UpdateLimit / 2) {
			SpawnDebugStressBatch();
		}
		if (state.RequestHostWorldStateTransition && worldStateServerRequested && !state.HostWorldStateTransitionQueued && state.SimulationUpdates == 600) {
			WorldStateProtocol::Snapshot targetActivity;
			targetActivity.ActivityClassName = "GAScripted";
			targetActivity.ActivityPreset = "Skirmish Defense";
			targetActivity.ActivityModuleName = "Base.rte";
			targetActivity.SceneModuleName = "Base.rte";
			targetActivity.ScenePreset = "Ketanot Hills";
			std::string failureReason;
			state.HostWorldStateTransitionQueued = QueueWorldStateActivity(targetActivity, failureReason);
			state.Log << "debug_host_activity_transition=" << (state.HostWorldStateTransitionQueued ? "queued" : "failed")
			          << (failureReason.empty() ? "" : " reason=" + failureReason) << '\n' << std::flush;
		}
		const bool hostTransitionRestarted = state.RequestHostWorldStateTransition && state.HostWorldStateTransitionQueued && state.SimulationUpdates > 600;
		const bool clientTransitionRestarted = state.RequireWorldStateTransition && state.WorldStateTransitionObserved &&
		                                      state.SimulationUpdates > state.WorldStateTransitionObservedUpdate;
		if (!state.PostTransitionStressBatchSpawned && (hostTransitionRestarted || clientTransitionRestarted)) {
			state.PostTransitionStressBatchSpawned = true;
			SpawnDebugStressBatch();
		}
		if (state.SimulationUpdates % 60 == 0) {
			state.Log << "simulation_update=" << state.SimulationUpdates << " actors=" << g_MovableMan.GetActorCount()
			          << " particles=" << g_MovableMan.GetParticleCount() << '\n' << std::flush;
		}
	}

	WorldStateProtocol::Snapshot CaptureDebugWorldStateSnapshot() {
		static WorldStateSnapshotBuilder snapshotBuilder;
		return snapshotBuilder.Capture(static_cast<std::uint32_t>(GetDebugRunState().SimulationUpdates));
	}

	bool VerifyWorldStateTransportLoopback(const WorldStateProtocol::Snapshot& expectedSnapshot, std::ofstream& log) {
		bool loopbackPassed = false;
		bool serverStarted = false;
		bool clientsStarted = false;
		constexpr std::size_t expectedClientCount = 4;
		std::size_t sendCount = 0;
		std::size_t serverPacketCount = 0;
		std::size_t clientPacketCount = 0;
		std::size_t acceptedClientCount = 0;
		std::size_t receivedClientCount = 0;
		std::size_t receivedTerrainPatchCount = 0;
		std::uint16_t boundPort = 0;
		{
			WorldStateTransport serverTransport;
			std::array<WorldStateClientSession, expectedClientCount> clients;
			std::array<bool, expectedClientCount> clientReceivedSnapshot{};
			std::array<bool, expectedClientCount> clientReceivedTerrainPatch{};
			WorldStateProtocol::TerrainPatch expectedTerrainPatch;
			expectedTerrainPatch.SceneRevision = expectedSnapshot.SceneRevision;
			expectedTerrainPatch.X = 4;
			expectedTerrainPatch.Y = 5;
			expectedTerrainPatch.Width = 2;
			expectedTerrainPatch.Height = 2;
			expectedTerrainPatch.Layer = WorldStateProtocol::TerrainLayer::Foreground;
			expectedTerrainPatch.Pixels = {3, 5, 7, 9};
			serverStarted = serverTransport.StartServer(0, static_cast<unsigned short>(expectedClientCount), "127.0.0.1");
			if (serverStarted) {
				boundPort = serverTransport.GetBoundPort();
				clientsStarted = boundPort != 0;
				for (WorldStateClientSession& client : clients) {
					clientsStarted = clientsStarted && client.Connect("127.0.0.1", boundPort);
				}
				if (clientsStarted) {
					bool broadcastSent = false;
					bool terrainBroadcastSent = false;
					const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
					while (std::chrono::steady_clock::now() < deadline &&
					       (receivedClientCount < expectedClientCount || receivedTerrainPatchCount < expectedClientCount)) {
						std::vector<WorldStateTransport::ReceivedPacket> serverPackets;
						serverTransport.Poll(serverPackets);
						serverPacketCount += serverPackets.size();
						for (const WorldStateTransport::ReceivedPacket& received : serverPackets) {
							if (received.Identifier == ID_NEW_INCOMING_CONNECTION) {
								++acceptedClientCount;
							}
						}
						if (!broadcastSent && acceptedClientCount == expectedClientCount) {
							broadcastSent = serverTransport.BroadcastSnapshot(expectedSnapshot, expectedSnapshot.Tick);
							sendCount += broadcastSent ? expectedClientCount : 0;
						}
						if (broadcastSent && !terrainBroadcastSent) {
							terrainBroadcastSent = serverTransport.BroadcastTerrainPatch(expectedTerrainPatch, expectedSnapshot.Tick + 1);
							sendCount += terrainBroadcastSent ? expectedClientCount : 0;
						}
						for (std::size_t clientIndex = 0; clientIndex < clients.size(); ++clientIndex) {
							WorldStateClientSession& client = clients[clientIndex];
							client.Update();
							if (client.HasSnapshot() && !clientReceivedSnapshot[clientIndex]) {
								const WorldStateProtocol::Snapshot& transportedSnapshot = client.GetLatestSnapshot();
								clientReceivedSnapshot[clientIndex] = transportedSnapshot.Tick == expectedSnapshot.Tick &&
								    transportedSnapshot.SceneRevision == expectedSnapshot.SceneRevision &&
								    transportedSnapshot.ActivityClassName == expectedSnapshot.ActivityClassName && transportedSnapshot.ActivityPreset == expectedSnapshot.ActivityPreset &&
								    transportedSnapshot.ActivityModuleName == expectedSnapshot.ActivityModuleName && transportedSnapshot.SceneModuleName == expectedSnapshot.SceneModuleName &&
									    transportedSnapshot.ScenePreset == expectedSnapshot.ScenePreset && transportedSnapshot.Objects.size() == expectedSnapshot.Objects.size() &&
								    (transportedSnapshot.Objects.empty() || transportedSnapshot.Objects.front().PresetName == expectedSnapshot.Objects.front().PresetName);
								if (clientReceivedSnapshot[clientIndex]) {
									++receivedClientCount;
									++clientPacketCount;
								}
							}
							for (const WorldStateClientSession::ReceivedTerrainPatch& receivedPatch : client.DrainTerrainPatches(expectedTerrainPatch.SceneRevision)) {
								const WorldStateProtocol::TerrainPatch& terrainPatch = receivedPatch.Patch;
								clientReceivedTerrainPatch[clientIndex] = receivedPatch.Sequence == expectedSnapshot.Tick + 1 &&
								    terrainPatch.X == expectedTerrainPatch.X && terrainPatch.Y == expectedTerrainPatch.Y && terrainPatch.Width == expectedTerrainPatch.Width &&
								    terrainPatch.Height == expectedTerrainPatch.Height && terrainPatch.Layer == expectedTerrainPatch.Layer && terrainPatch.Pixels == expectedTerrainPatch.Pixels;
								if (clientReceivedTerrainPatch[clientIndex]) {
									++receivedTerrainPatchCount;
									++clientPacketCount;
								}
							}
						}
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
					}
					loopbackPassed = broadcastSent && terrainBroadcastSent && receivedClientCount == expectedClientCount && receivedTerrainPatchCount == expectedClientCount;
				}
			}
		}
		log << "world_state_transport_setup=server:" << serverStarted << ",port:" << boundPort
		    << ",clients_started:" << clientsStarted << ",clients_accepted:" << acceptedClientCount << ",clients_received:" << receivedClientCount << ",server_packets:" << serverPacketCount
		    << ",client_packets:" << clientPacketCount << ",terrain_received:" << receivedTerrainPatchCount << ",sends:" << sendCount
		    << ",loopback:" << (loopbackPassed ? "passed" : "failed") << '\n' << std::flush;
		return loopbackPassed;
	}

	bool VerifyWorldStateHostSessionLoopback(WorldStateServerSession& serverSession, std::ofstream& log) {
		constexpr std::size_t expectedClientCount = 4;
		const unsigned short port = serverSession.GetBoundPort();
		serverSession.Update(++worldStateServerSimulationTick);
		const bool idleServerSkippedSnapshot = serverSession.GetSnapshotBroadcastCount() == 0;
		std::array<WorldStateClientSession, expectedClientCount> clients;
		bool started = port != 0;
		for (WorldStateClientSession& client : clients) {
			started = started && client.Connect("127.0.0.1", port);
		}
		if (!started) {
			log << "world_state_host_session_smoke=failed reason=client_start\n" << std::flush;
			return false;
		}

		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (std::chrono::steady_clock::now() < deadline &&
		       (serverSession.GetConnectedClientCount() < expectedClientCount ||
		        std::any_of(clients.begin(), clients.end(), [](const WorldStateClientSession& client) { return !client.HasPlayerAssignment(); }) ||
		        std::any_of(clients.begin(), clients.end(), [](const WorldStateClientSession& client) { return !client.HasSnapshot(); }))) {
			serverSession.Update(++worldStateServerSimulationTick);
			for (WorldStateClientSession& client : clients) {
				client.Update();
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		const bool allConnectedAndReceived = serverSession.GetConnectedClientCount() == expectedClientCount &&
		                                    std::all_of(clients.begin(), clients.end(), [](const WorldStateClientSession& client) {
		                                    return client.IsConnected() && client.HasPlayerAssignment() && client.HasSnapshot() && client.GetLatestSnapshot().Tick > 0;
		                                    });
		std::array<int, expectedClientCount> assignedSlots{};
		std::transform(clients.begin(), clients.end(), assignedSlots.begin(), [](const WorldStateClientSession& client) {
			return client.GetAssignedPlayerSlot();
		});
		std::sort(assignedSlots.begin(), assignedSlots.end());
		const bool playerAssignmentsPassed = assignedSlots == std::array<int, expectedClientCount>{Players::NoPlayer, Players::PlayerTwo, Players::PlayerThree, Players::PlayerFour};
		const std::size_t clientsReceivedSnapshotCount = std::count_if(clients.begin(), clients.end(), [](const WorldStateClientSession& client) {
			return client.GetReceivedSnapshotCount() > 0;
		});
		bool networkInputApplied = false;
		int networkInputPlayer = Players::NoPlayer;
		if (allConnectedAndReceived) {
			WorldStateProtocol::InputCommand inputCommand;
			inputCommand.ClientTick = 1;
			inputCommand.HeldElements = std::uint64_t{1} << InputElements::INPUT_FIRE;
			inputCommand.MouseButtonsHeld = static_cast<std::uint8_t>(1U << MouseButtons::MOUSE_LEFT);
			inputCommand.AnalogAimX = 16384;
			const std::uint32_t initialInputCount = serverSession.GetInputCommandCount();
			const bool inputSent = clients.front().SendInputCommand(inputCommand);
			const auto inputDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
			while (inputSent && serverSession.GetInputCommandCount() == initialInputCount && std::chrono::steady_clock::now() < inputDeadline) {
				serverSession.Update(++worldStateServerSimulationTick);
				clients.front().Update();
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
			if (serverSession.GetInputCommandCount() > initialInputCount) {
				for (int player = Players::PlayerOne; player < Players::MaxPlayerCount; ++player) {
					if (g_UInputMan.IsNetworkInputActive(player) && g_UInputMan.ElementHeld(player, InputElements::INPUT_FIRE) &&
					    g_UInputMan.MouseButtonHeld(MouseButtons::MOUSE_LEFT, player) && g_UInputMan.AnalogAimValues(player).GetX() > 0.49F) {
						networkInputPlayer = player;
						break;
					}
				}
			}
			inputCommand.ClientTick = 2;
			inputCommand.HeldElements = 0;
			inputCommand.MouseButtonsHeld = 0;
			inputCommand.AnalogAimX = 0;
			const bool releaseSent = clients.front().SendInputCommand(inputCommand);
			const auto releaseDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
			while (releaseSent && serverSession.GetInputCommandCount() <= initialInputCount + 1 && std::chrono::steady_clock::now() < releaseDeadline) {
				serverSession.Update(++worldStateServerSimulationTick);
				clients.front().Update();
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
			networkInputApplied = networkInputPlayer == clients.front().GetAssignedPlayerSlot() && serverSession.GetInputCommandCount() >= initialInputCount + 2 &&
			                      g_UInputMan.ElementReleased(networkInputPlayer, InputElements::INPUT_FIRE) &&
			                      g_UInputMan.MouseButtonReleased(MouseButtons::MOUSE_LEFT, networkInputPlayer);
		}
		for (WorldStateClientSession& client : clients) {
			client.Disconnect();
		}
		const auto disconnectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (serverSession.GetConnectedClientCount() > 0 && std::chrono::steady_clock::now() < disconnectDeadline) {
			serverSession.Update(++worldStateServerSimulationTick);
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		const bool remainedAvailableAfterDisconnect = serverSession.IsStarted() && serverSession.GetConnectedClientCount() == 0;
		const bool passed = idleServerSkippedSnapshot && allConnectedAndReceived && playerAssignmentsPassed && networkInputApplied && remainedAvailableAfterDisconnect;
		log << "world_state_host_session_smoke=" << (passed ? "passed" : "failed")
		    << " clients_accepted=" << expectedClientCount
		    << " clients_received=" << clientsReceivedSnapshotCount
		    << " assignments=" << playerAssignmentsPassed
		    << " idle_snapshot_count=" << (idleServerSkippedSnapshot ? 0 : serverSession.GetSnapshotBroadcastCount())
		    << " input_applied=" << networkInputApplied << " input_slot=" << networkInputPlayer
		    << " server_alive_after_disconnect=" << remainedAvailableAfterDisconnect << '\n' << std::flush;
		return passed;
	}

	void FinishDebugRun(bool success) {
		DebugRunState& state = GetDebugRunState();
		if (!state.Enabled || System::IsSetToQuit()) {
			return;
		}
		const WorldStateProtocol::Snapshot worldSnapshot = CaptureDebugWorldStateSnapshot();
		const WorldStateProtocol::Snapshot repeatedWorldSnapshot = CaptureDebugWorldStateSnapshot();
		bool worldIdentityStable = worldSnapshot.SceneRevision == repeatedWorldSnapshot.SceneRevision &&
		                          worldSnapshot.ActivityClassName == repeatedWorldSnapshot.ActivityClassName &&
		                          worldSnapshot.ActivityPreset == repeatedWorldSnapshot.ActivityPreset &&
		                          worldSnapshot.ActivityModuleName == repeatedWorldSnapshot.ActivityModuleName &&
		                          worldSnapshot.SceneModuleName == repeatedWorldSnapshot.SceneModuleName &&
		                          worldSnapshot.ScenePreset == repeatedWorldSnapshot.ScenePreset &&
		                          worldSnapshot.Objects.size() == repeatedWorldSnapshot.Objects.size();
		for (std::size_t index = 0; worldIdentityStable && index < worldSnapshot.Objects.size(); ++index) {
			worldIdentityStable = worldSnapshot.Objects[index].NetworkId == repeatedWorldSnapshot.Objects[index].NetworkId;
		}
		state.Log << "world_state_identity_smoke=" << (worldIdentityStable ? "passed" : "failed") << '\n' << std::flush;
		bool terrainPatchApplyPassed = false;
		if (Scene* scene = g_SceneMan.GetScene(); scene && scene->GetTerrain() && worldSnapshot.SceneRevision != 0 &&
		    g_SceneMan.GetSceneWidth() > 0 && g_SceneMan.GetSceneHeight() > 0) {
			SLTerrain* terrain = scene->GetTerrain();
			const int testX = g_SceneMan.GetSceneWidth() / 2;
			const int testY = g_SceneMan.GetSceneHeight() / 2;
			const std::array<std::pair<WorldStateProtocol::TerrainLayer, std::uint8_t>, 3> originalPixels = {{
			    {WorldStateProtocol::TerrainLayer::Material, static_cast<std::uint8_t>(terrain->GetMaterialPixel(testX, testY))},
			    {WorldStateProtocol::TerrainLayer::Foreground, static_cast<std::uint8_t>(terrain->GetFGColorPixel(testX, testY))},
			    {WorldStateProtocol::TerrainLayer::Background, static_cast<std::uint8_t>(terrain->GetBGColorPixel(testX, testY))}}};
			auto applyPixel = [&](WorldStateProtocol::TerrainLayer layer, std::uint8_t pixelValue) {
				WorldStateProtocol::TerrainPatch patch;
				patch.SceneRevision = worldSnapshot.SceneRevision;
				patch.X = static_cast<std::uint32_t>(testX);
				patch.Y = static_cast<std::uint32_t>(testY);
				patch.Width = 1;
				patch.Height = 1;
				patch.Layer = layer;
				patch.Pixels = {pixelValue};
				return worldStateClientReplica.ApplyTerrainPatch(patch);
			};
			bool allLayersAppliedAndRestored = true;
			for (const auto& [layer, originalPixel] : originalPixels) {
				const std::uint8_t changedPixel = static_cast<std::uint8_t>(originalPixel ^ 1U);
				const bool applyPassed = applyPixel(layer, changedPixel);
				int observedChangedPixel = 0;
				switch (layer) {
				case WorldStateProtocol::TerrainLayer::Material: observedChangedPixel = terrain->GetMaterialPixel(testX, testY); break;
				case WorldStateProtocol::TerrainLayer::Foreground: observedChangedPixel = terrain->GetFGColorPixel(testX, testY); break;
				case WorldStateProtocol::TerrainLayer::Background: observedChangedPixel = terrain->GetBGColorPixel(testX, testY); break;
				}
				const bool changedPixelObserved = applyPassed && observedChangedPixel == changedPixel;
				const bool restorePassed = applyPixel(layer, originalPixel);
				int observedRestoredPixel = 0;
				switch (layer) {
				case WorldStateProtocol::TerrainLayer::Material: observedRestoredPixel = terrain->GetMaterialPixel(testX, testY); break;
				case WorldStateProtocol::TerrainLayer::Foreground: observedRestoredPixel = terrain->GetFGColorPixel(testX, testY); break;
				case WorldStateProtocol::TerrainLayer::Background: observedRestoredPixel = terrain->GetBGColorPixel(testX, testY); break;
				}
				allLayersAppliedAndRestored = allLayersAppliedAndRestored && changedPixelObserved && restorePassed && observedRestoredPixel == originalPixel;
			}
			terrainPatchApplyPassed = allLayersAppliedAndRestored;
		}
		state.Log << "world_state_terrain_patch_apply_smoke=" << (terrainPatchApplyPassed ? "passed" : "unavailable-or-failed") << '\n' << std::flush;
		WorldStateClientReplica replicaSmoke;
		const WorldStateClientReplica::ApplyResult firstApply = replicaSmoke.Apply(worldSnapshot);
		WorldStateProtocol::Snapshot emptyReplicaSnapshot = worldSnapshot;
		emptyReplicaSnapshot.Objects.clear();
		const WorldStateClientReplica::ApplyResult removalApply = replicaSmoke.Apply(emptyReplicaSnapshot);
		const WorldStateClientReplica::ApplyResult restoredApply = replicaSmoke.Apply(worldSnapshot);
		const WorldStateClientReplica::ApplyResult repeatedApply = replicaSmoke.Apply(worldSnapshot);
		const bool replicaSmokePassed = firstApply.Updated + firstApply.Spawned == worldSnapshot.Objects.size() && firstApply.MissingPresets == 0 &&
		                               removalApply.Removed == firstApply.Spawned && removalApply.Updated == 0 && removalApply.Spawned == 0 &&
		                               restoredApply.Updated + restoredApply.Spawned == worldSnapshot.Objects.size() && restoredApply.MissingPresets == 0 &&
		                               repeatedApply.Updated == worldSnapshot.Objects.size() && repeatedApply.Spawned == 0 && repeatedApply.Removed == 0 && repeatedApply.MissingPresets == 0;
		state.Log << "world_state_replica_smoke=" << (replicaSmokePassed ? "passed" : "failed") << " objects=" << worldSnapshot.Objects.size()
		          << " first_updated=" << firstApply.Updated << " first_spawned=" << firstApply.Spawned << " removed=" << removalApply.Removed
		          << " repeat_updated=" << repeatedApply.Updated << '\n' << std::flush;
		WorldStateProtocol::Snapshot transientPixelSnapshot = worldSnapshot;
		transientPixelSnapshot.Objects.clear();
		WorldStateProtocol::ObjectState transientPixel;
		transientPixel.NetworkId = std::numeric_limits<std::uint64_t>::max();
		transientPixel.ClassName = "MOPixel";
		transientPixel.PresetName = "None";
		transientPixel.PositionX = 100000.0F;
		transientPixel.PositionY = 100000.0F;
		transientPixel.PixelMaterialId = g_MaterialAir;
		transientPixel.PixelColorIndex = 42;
		transientPixel.PixelMass = 0.25F;
		transientPixel.PixelLifetime = 350;
		transientPixel.PixelSharpness = 0.8F;
		transientPixel.Flags = WorldStateProtocol::c_ObjectFlagTransientPixel;
		transientPixelSnapshot.Objects.push_back(transientPixel);
		WorldStateClientReplica transientPixelReplica;
		const WorldStateClientReplica::ApplyResult transientPixelSpawn = transientPixelReplica.Apply(transientPixelSnapshot);
		WorldStateProtocol::Snapshot removeTransientPixel = transientPixelSnapshot;
		removeTransientPixel.Objects.clear();
		const WorldStateClientReplica::ApplyResult transientPixelRemoval = transientPixelReplica.Apply(removeTransientPixel);
		const bool transientPixelReplicaPassed = transientPixelSpawn.Spawned == 1 && transientPixelSpawn.MissingPresets == 0 && transientPixelRemoval.Removed == 1;
		state.Log << "transient_mopixel_replica_smoke=" << (transientPixelReplicaPassed ? "passed" : "failed")
		          << " spawned=" << transientPixelSpawn.Spawned << " removed=" << transientPixelRemoval.Removed
		          << " missing=" << transientPixelSpawn.MissingPresets << '\n' << std::flush;
		const int inputSmokePlayer = Players::MaxPlayerCount - 1;
		const std::uint64_t inputSmokeMask = std::uint64_t{1} << InputElements::INPUT_FIRE;
		const Vector inputSmokeMouseMovement(3.0F, -4.0F);
		const Vector inputSmokeAnalogMove(0.5F, -0.25F);
		const Vector inputSmokeAnalogAim(-1.0F, 1.0F);
		const std::uint8_t inputSmokeMouseButtons = static_cast<std::uint8_t>(1U << MouseButtons::MOUSE_LEFT);
		g_UInputMan.SetNetworkInputState(inputSmokePlayer, inputSmokeMask, inputSmokeMouseMovement, -1, inputSmokeMouseButtons,
		                                inputSmokeAnalogMove, inputSmokeAnalogAim);
		const bool networkInputPressPassed = g_UInputMan.ElementHeld(inputSmokePlayer, InputElements::INPUT_FIRE) &&
		                                    g_UInputMan.ElementPressed(inputSmokePlayer, InputElements::INPUT_FIRE) &&
		                                    g_UInputMan.MouseButtonHeld(MouseButtons::MOUSE_LEFT, inputSmokePlayer) &&
		                                    g_UInputMan.MouseButtonPressed(MouseButtons::MOUSE_LEFT, inputSmokePlayer) &&
		                                    g_UInputMan.MouseWheelMovedByPlayer(inputSmokePlayer) == -1 &&
		                                    g_UInputMan.GetMouseMovement(inputSmokePlayer).GetX() == inputSmokeMouseMovement.GetX() &&
		                                    g_UInputMan.GetMouseMovement(inputSmokePlayer).GetY() == inputSmokeMouseMovement.GetY() &&
		                                    g_UInputMan.AnalogMoveValues(inputSmokePlayer).GetX() == inputSmokeAnalogMove.GetX() &&
		                                    g_UInputMan.AnalogAimValues(inputSmokePlayer).GetY() == inputSmokeAnalogAim.GetY();
		g_UInputMan.SetNetworkInputState(inputSmokePlayer, 0, Vector(), 0, 0, Vector(), Vector());
		const bool networkInputReleasePassed = g_UInputMan.ElementReleased(inputSmokePlayer, InputElements::INPUT_FIRE) &&
		                                      g_UInputMan.MouseButtonReleased(MouseButtons::MOUSE_LEFT, inputSmokePlayer);
		g_UInputMan.ClearNetworkInputState(inputSmokePlayer);
		const bool networkInputApplicationPassed = networkInputPressPassed && networkInputReleasePassed;
		state.Log << "world_state_input_application_smoke=" << (networkInputApplicationPassed ? "passed" : "failed") << '\n' << std::flush;
		std::vector<std::uint8_t> worldSnapshotPacket;
		WorldStateProtocol::Snapshot decodedWorldSnapshot;
		const bool worldSnapshotPassed = WorldStateProtocol::EncodeSnapshot(worldSnapshot, state.SimulationUpdates, worldSnapshotPacket) &&
		                                 WorldStateProtocol::DecodeSnapshot(worldSnapshotPacket, decodedWorldSnapshot) &&
		                                 decodedWorldSnapshot.ActivityClassName == worldSnapshot.ActivityClassName &&
		                                 decodedWorldSnapshot.ActivityPreset == worldSnapshot.ActivityPreset &&
		                                 decodedWorldSnapshot.ActivityModuleName == worldSnapshot.ActivityModuleName &&
		                                 decodedWorldSnapshot.SceneModuleName == worldSnapshot.SceneModuleName &&
		                                 decodedWorldSnapshot.ScenePreset == worldSnapshot.ScenePreset &&
		                                 decodedWorldSnapshot.Objects.size() == worldSnapshot.Objects.size();
		std::vector<std::uint8_t> snapshotWirePacket;
		std::vector<std::uint8_t> decompressedWorldSnapshotPacket;
		std::span<const std::uint8_t> decompressedWorldSnapshotView;
		bool snapshotWasCompressed = false;
		WorldStateProtocol::Snapshot compressedRoundTripSnapshot;
		const bool snapshotCompressionPassed = worldSnapshotPassed &&
		                                      WorldStateCompression::EncodeForWire(worldSnapshotPacket, snapshotWirePacket, snapshotWasCompressed) &&
		                                      WorldStateCompression::DecodeFromWire(snapshotWirePacket, decompressedWorldSnapshotPacket, decompressedWorldSnapshotView) &&
		                                      WorldStateProtocol::DecodeSnapshot(decompressedWorldSnapshotView, compressedRoundTripSnapshot) &&
		                                      compressedRoundTripSnapshot.Objects.size() == worldSnapshot.Objects.size() &&
		                                      compressedRoundTripSnapshot.ActivityPreset == worldSnapshot.ActivityPreset &&
		                                      compressedRoundTripSnapshot.ScenePreset == worldSnapshot.ScenePreset;
		state.Log << "world_snapshot_compression_smoke=" << (snapshotCompressionPassed ? "passed" : "failed")
		          << " compressed=" << (snapshotWasCompressed ? "yes" : "no") << " plain_bytes=" << worldSnapshotPacket.size()
		          << " wire_bytes=" << snapshotWirePacket.size() << '\n' << std::flush;
		const bool liveSnapshotTransportPassed = worldSnapshotPassed && snapshotCompressionPassed && VerifyWorldStateTransportLoopback(worldSnapshot, state.Log);
		state.Log << "captured_world_snapshot_objects=" << worldSnapshot.Objects.size()
		          << " bytes=" << worldSnapshotPacket.size()
		          << " result=" << (worldSnapshotPassed ? "passed" : "failed") << '\n' << std::flush;
		state.Log << "live_world_state_transport=" << (liveSnapshotTransportPassed ? "passed" : "failed") << '\n' << std::flush;
		const bool utf8GlyphRenderPassed = g_FrameMan.DidDebugUTF8GlyphProbePass();
		const bool worldStateTransitionPassed = !state.RequireWorldStateTransition || state.WorldStateTransitionObserved;
		state.Log << "world_state_activity_transition=" << (worldStateTransitionPassed ? "passed" : "failed")
		          << " observed=" << state.WorldStateTransitionObserved << " host_queued=" << state.HostWorldStateTransitionQueued << '\n' << std::flush;
		const bool remoteTerrainMutationPassed = !state.RequireRemoteTerrainMutationSmoke || state.RemoteTerrainMutationObserved;
		if (state.RequireRemoteTerrainMutationSmoke) {
			state.Log << "world_state_remote_terrain_mutation=" << (remoteTerrainMutationPassed ? "passed" : "failed")
			          << " observed=" << state.RemoteTerrainMutationObserved << " x=" << state.TerrainMutationProbeX << " y=" << state.TerrainMutationProbeY << '\n' << std::flush;
		}
		const bool hostTerrainMutationPassed = !state.RequestHostTerrainMutationSmoke || (worldStateServer && worldStateServer->DidDebugTerrainMutationSmokePass());
		if (state.RequestHostTerrainMutationSmoke) {
			state.Log << "world_state_host_terrain_mutation=" << (hostTerrainMutationPassed ? "passed" : "failed") << '\n' << std::flush;
		}
		state.Log << "utf8_unicode_font_loaded=" << (g_FrameMan.DidDebugUTF8GlyphFontLoad() ? "yes" : "no") << '\n' << std::flush;
		const std::string unicodeFontPath = g_PresetMan.GetFullModulePath("Base.rte/GUIs/Fonts/Roboto-Medium.ttf");
		std::filesystem::path unicodeFontFilePath = std::filesystem::u8path(unicodeFontPath);
		if (unicodeFontFilePath.is_relative()) {
			unicodeFontFilePath = std::filesystem::u8path(System::GetWorkingDirectory()) / unicodeFontFilePath;
		}
		std::ifstream unicodeFontProbe(unicodeFontFilePath, std::ios::binary);
		state.Log << "utf8_unicode_font_path=" << unicodeFontFilePath.generic_string() << " exists=" << (unicodeFontProbe ? "yes" : "no") << '\n' << std::flush;
		state.Log << "utf8_glyph_render_smoke=" << (utf8GlyphRenderPassed ? "passed" : "failed") << '\n' << std::flush;
		const MovableMan::SceneStats sceneStats = g_MovableMan.CollectSceneStats();
		const bool performanceCountersPassed = g_MovableMan.GetMovableObjectCount() > 0 && sceneStats.Actors > 0 && MovableObject::GetSceneSpawnEvents() > 0;
		state.Log << "performance_counters_smoke=" << (performanceCountersPassed ? "passed" : "failed")
		          << " actors=" << sceneStats.Actors << " items=" << sceneStats.Items << " mos_rotating=" << sceneStats.MOSRotating
		          << " mos_particle=" << sceneStats.MOSParticles << " mo_pixel=" << sceneStats.MOPixels << " gibs=" << sceneStats.Gibs
		          << " spawned=" << MovableObject::GetSceneSpawnEvents() << " deleted=" << MovableObject::GetSceneDeleteEvents()
		          << " overlay_level=" << g_PerformanceMan.GetOverlayLevel() << " fps_limit=" << g_SettingsMan.GetFPSLimit() << '\n' << std::flush;
		state.Log << "performance_settings=" << "preset:" << g_SettingsMan.GetPerformancePreset()
		          << ",particles:" << g_SettingsMan.GetParticleLimitLevel() << ",gibs:" << g_SettingsMan.GetGibLimitLevel()
		          << ",debris_life:" << g_SettingsMan.GetDebrisLifetimeLevel() << ",particle_life:" << g_SettingsMan.GetParticleLifetimePercent()
		          << ",max_mos:" << g_SettingsMan.GetMaximumMovableObjects() << ",background:" << g_SettingsMan.GetBackgroundEffectsLevel()
		          << ",screen_fx:" << g_SettingsMan.GetScreenEffectsLevel() << ",gore:" << g_SettingsMan.GetGoreDensityPercent()
		          << ",vsync:" << g_WindowMan.GetVSyncEnabled() << '\n' << std::flush;
		success = success && performanceCountersPassed;
		success = success && utf8GlyphRenderPassed && worldStateTransitionPassed && remoteTerrainMutationPassed && hostTerrainMutationPassed && state.HostSessionTestPassed && worldIdentityStable && replicaSmokePassed && transientPixelReplicaPassed && terrainPatchApplyPassed &&
		          networkInputApplicationPassed && worldSnapshotPassed && snapshotCompressionPassed && liveSnapshotTransportPassed;
		const double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - state.StartTime).count();
		const double simulationSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - state.SimulationStartTime).count();
		state.Log << "result=" << (success ? "passed" : "failed") << "\nupdates_done=" << state.SimulationUpdates
		          << "\nrendered_frames=" << state.RenderedFrames << "\nelapsed_seconds=" << elapsedSeconds
		          << "\nsimulation_seconds=" << simulationSeconds
		          << "\nmodule_load_seconds=" << std::chrono::duration<double>(state.ModuleLoadEndTime - state.ModuleLoadStartTime).count()
		          << "\naverage_frame_ms=" << (state.RenderedFrames > 0 ? simulationSeconds * 1000.0 / state.RenderedFrames : 0.0)
		          << "\naverage_frame_work_ms=" << (state.RenderedFrames > 0 ? state.TotalFrameTimeMilliseconds / state.RenderedFrames : 0.0)
		          << "\naverage_render_ms=" << (state.RenderedFrames > 0 ? state.TotalRenderTimeMilliseconds / state.RenderedFrames : 0.0)
		          << "\nsimulation_updates_per_second=" << (simulationSeconds > 0.0 ? state.SimulationUpdates / simulationSeconds : 0.0)
		          << "\nprocess_resident_memory_bytes=" << GetProcessResidentMemoryBytes()
		          << "\nactors=" << g_MovableMan.GetActorCount() << "\nparticles=" << g_MovableMan.GetParticleCount() << '\n';
		for (const auto& [name, counter] : std::array<std::pair<const char*, PerformanceMan::PerformanceCounters>, 9>{ {
		         {"simulation", PerformanceMan::SimTotal}, {"ai", PerformanceMan::ActorsAI}, {"actor_travel", PerformanceMan::ActorsTravel},
		         {"actor_update", PerformanceMan::ActorsUpdate}, {"particle_travel", PerformanceMan::ParticlesTravel}, {"particle_update", PerformanceMan::ParticlesUpdate},
		         {"activity", PerformanceMan::ActivityUpdate}, {"lua_scripts", PerformanceMan::ScriptsUpdate}, {"network_snapshot_capture", PerformanceMan::WorldStateSnapshot}
	         } }) {
			state.Log << "average_" << name << "_ms=" << static_cast<double>(g_PerformanceMan.GetAveragePerformanceTime(counter)) / 1000.0 << '\n';
		}
		state.Log.flush();
		state.Passed = success;
		g_ConsoleMan.SaveAllText((state.OutputDirectory / "LogConsole.txt").string());
		for (const char* logName : {"LogLoading.txt", "LogLoadingWarning.txt"}) {
			const std::filesystem::path source = std::filesystem::path(System::GetWorkingDirectory()) / logName;
			if (std::filesystem::exists(source)) {
				std::error_code copyError;
				std::filesystem::copy_file(source, state.OutputDirectory / logName, std::filesystem::copy_options::overwrite_existing, copyError);
			}
		}
		state.Log.close();
		System::SetQuit();
	}

	void CaptureDebugRunFrame() {
		DebugRunState& state = GetDebugRunState();
		if (!state.Enabled || System::IsSetToQuit()) {
			return;
		}
		++state.RenderedFrames;
		const auto capture = [&](const char* label) {
			const std::string imageName = state.OutputDirectory.filename().string() + "/debug-run-" + std::string(label) + ".png";
			const int screenshotResult = g_FrameMan.SaveScreenToPNGBlocking(imageName);
			state.Log << "screenshot=" << imageName << " result=" << screenshotResult << '\n' << std::flush;
		};
		if (!state.InitialScreenshotCaptured && state.SimulationUpdates >= 1) {
			state.InitialScreenshotCaptured = true;
			capture("initial");
		}
		if (!state.MidpointScreenshotCaptured && state.SimulationUpdates >= state.UpdateLimit / 2) {
			state.MidpointScreenshotCaptured = true;
			capture("stress");
		}
		if (!state.FinalScreenshotCaptured && state.SimulationUpdates >= state.UpdateLimit) {
			state.FinalScreenshotCaptured = true;
			capture("final");
		}
		if (state.SimulationUpdates >= state.UpdateLimit) {
			FinishDebugRun(state.StressStarted && (!state.RequestHostWorldStateTransition || state.HostWorldStateTransitionQueued));
		}
	}
}

/// <summary>
/// Initializes all the essential managers.
/// </summary>
void InitializeManagers() {
	ThreadMan::Construct();
	TimerMan::Construct();
	PresetMan::Construct();
	SettingsMan::Construct();
	WindowMan::Construct();
	GLResourceMan::Construct();
	LuaMan::Construct();
	FrameMan::Construct();
	PerformanceMan::Construct();
	PostProcessMan::Construct();
	PrimitiveMan::Construct();
	AudioMan::Construct();
	GUISound::Construct();
	MusicMan::Construct();
	UInputMan::Construct();
	ConsoleMan::Construct();
	SceneMan::Construct();
	MovableMan::Construct();
	MetaMan::Construct();
	MenuMan::Construct();
	CameraMan::Construct();
	ActivityMan::Construct();
	LoadingScreen::Construct();

	g_ThreadMan.Initialize();
	g_SettingsMan.Initialize();
	g_WindowMan.Initialize();
	g_GLResourceMan.Initialize();

	g_LuaMan.Initialize();
	g_TimerMan.Initialize();
	g_FrameMan.Initialize();
	g_PostProcessMan.Initialize();
	g_PerformanceMan.Initialize();

	if (!System::IsDebugRun() && g_AudioMan.Initialize()) {
		g_GUISound.Initialize();
		g_MusicMan.Initialize();
	}

	g_UInputMan.Initialize();
	g_ConsoleMan.Initialize();
	g_SceneMan.Initialize();
	g_MovableMan.Initialize();
	g_MetaMan.Initialize();
	g_MenuMan.Initialize();

	// Overwrite Settings.ini after all the managers are created to fully populate the file. Up until this moment Settings.ini is populated only with minimal required properties to run.
	// If Settings.ini already exists and is fully populated, this will deal with overwriting it to apply any overrides performed by the managers at boot (e.g resolution validation).
	if (g_SettingsMan.SettingsNeedOverwrite()) {
		g_SettingsMan.UpdateSettingsFile();
	}
}

/// <summary>
/// Destroys all the managers and frees all loaded data before termination.
/// </summary>
void DestroyManagers() {
	g_MetaMan.Destroy();
	g_PerformanceMan.Destroy();
	g_MovableMan.Destroy();
	g_SceneMan.Destroy();
	g_ActivityMan.Destroy();
	g_GUISound.Destroy();
	g_AudioMan.Destroy();
	g_MusicMan.Destroy();
	g_PresetMan.Destroy();
	g_UInputMan.Destroy();
	g_PostProcessMan.Destroy();
	g_FrameMan.Destroy();
	g_TimerMan.Destroy();
	g_LuaMan.Destroy();
	ContentFile::FreeAllLoaded();
	g_ConsoleMan.Destroy();
	g_GLResourceMan.Destroy();
	g_WindowMan.Destroy();

#ifdef DEBUG_BUILD
	Entity::ClassInfo::DumpPoolMemoryInfo(Writer("MemCleanupInfo.txt"));
#endif
}

/// <summary>
/// Command-line argument handling.
/// </summary>
/// <param name="argCount">Argument count.</param>
/// <param name="argValue">Argument values.</param>
void HandleMainArgs(int argCount, char** argValue) {
	// Discard the first argument because it's always the executable path/name
	argCount--;
	argValue++;
	if (argCount == 0) {
		return;
	}
	bool launchModeSet = false;
	bool singleModuleSet = false;

	for (int i = 0; i < argCount;) {
		std::string currentArg = argValue[i];
		bool lastArg = i + 1 == argCount;
		if (currentArg == "-world-state-client" && i + 2 < argCount) {
			unsigned int port = 0;
			const std::string_view portArgument(argValue[i + 2]);
			const auto parsed = std::from_chars(portArgument.data(), portArgument.data() + portArgument.size(), port);
			if (parsed.ec == std::errc{} && parsed.ptr == portArgument.data() + portArgument.size() && port > 0 && port <= 65535) {
				worldStateClientAddress = argValue[i + 1];
				worldStateClientPort = static_cast<unsigned short>(port);
				i += 2;
			}
		}
		if (currentArg == "-world-state-server") {
			worldStateServerRequested = true;
			if (!lastArg) {
				unsigned int port = 0;
				const std::string_view portArgument(argValue[i + 1]);
				const auto parsed = std::from_chars(portArgument.data(), portArgument.data() + portArgument.size(), port);
				if (parsed.ec == std::errc{} && parsed.ptr == portArgument.data() + portArgument.size() && port > 0 && port <= 65535) {
					worldStateServerPort = static_cast<unsigned short>(port);
					++i;
				}
			}
		}
		if (!lastArg && currentArg == "-world-state-bind") {
			worldStateServerBindAddress = argValue[++i];
		}
		if (!lastArg && currentArg == "-world-state-players") {
			unsigned int players = 0;
			const std::string_view playerArgument(argValue[i + 1]);
			const auto parsed = std::from_chars(playerArgument.data(), playerArgument.data() + playerArgument.size(), players);
			if (parsed.ec == std::errc{} && parsed.ptr == playerArgument.data() + playerArgument.size() && players > 0 && players <= 64) {
				worldStateServerMaxPlayers = static_cast<unsigned short>(players);
			}
			++i;
		}

		if (currentArg == "-cout") {
			System::EnableLoggingToCLI();
		}

		if (currentArg == "-ext-validate") {
			System::EnableExternalModuleValidationMode();
		}

		if (!lastArg && !singleModuleSet && currentArg == "-module") {
			std::string moduleToLoad = argValue[++i];
			if (moduleToLoad.find(System::GetModulePackageExtension()) == moduleToLoad.length() - System::GetModulePackageExtension().length()) {
				g_PresetMan.SetSingleModuleToLoad(moduleToLoad);
				singleModuleSet = true;
			}
		}
		if (!launchModeSet) {
			if (!lastArg && currentArg == "-editor") {
				g_ActivityMan.SetEditorToLaunch(argValue[++i]);
				launchModeSet = true;
			}
		}
		++i;
	}
	if (launchModeSet) {
		g_SettingsMan.SetSkipIntro(true);
	}
}

/// <summary>
/// Polls the SDL event queue and passes events to be handled by the relevant managers.
/// </summary>
void PollSDLEvents() {
	SDL_Event sdlEvent;
	while (SDL_PollEvent(&sdlEvent)) {
		switch (sdlEvent.type) {
			case SDL_EVENT_QUIT :
				System::SetQuit(true);
				return;
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				System::SetQuit(true);
				return;
			case SDL_EVENT_KEY_UP :
			case SDL_EVENT_KEY_DOWN :
			case SDL_EVENT_TEXT_INPUT :
			case SDL_EVENT_MOUSE_MOTION :
			case SDL_EVENT_MOUSE_BUTTON_UP :
			case SDL_EVENT_MOUSE_BUTTON_DOWN :
			case SDL_EVENT_MOUSE_WHEEL :
			case SDL_EVENT_GAMEPAD_AXIS_MOTION :
			case SDL_EVENT_GAMEPAD_BUTTON_DOWN :
			case SDL_EVENT_GAMEPAD_BUTTON_UP :
			case SDL_EVENT_JOYSTICK_AXIS_MOTION :
			case SDL_EVENT_JOYSTICK_BUTTON_DOWN :
			case SDL_EVENT_JOYSTICK_BUTTON_UP :
			case SDL_EVENT_JOYSTICK_ADDED :
			case SDL_EVENT_JOYSTICK_REMOVED :
				g_UInputMan.HandleInputEvent(sdlEvent);
				break;
			default:
				break;
		}
		ImGui_ImplSDL3_ProcessEvent(&sdlEvent);
		if (sdlEvent.type >= SDL_EVENT_WINDOW_FIRST && sdlEvent.type <= SDL_EVENT_WINDOW_LAST) {
			g_WindowMan.QueueWindowEvent(sdlEvent);
		}
	}
}

/// <summary>
/// Game menus loop.
/// </summary>
void RunMenuLoop() {
	g_MenuMan.SetIsInMenuScreen(true);
	g_UInputMan.DisableKeys(false);
	g_UInputMan.TrapMousePos(false);

	while (!System::IsSetToQuit()) {
		const auto frameWallStart = std::chrono::steady_clock::now();
		DiscordPresence::SetActivity("At the main menu", "Cortex Command Reload");
		g_WindowMan.ClearBackbuffer();
		PollSDLEvents();

		g_WindowMan.Update();

		g_UInputMan.Update();
		g_TimerMan.Update();
		g_TimerMan.UpdateSim();
		g_AudioMan.Update();
		g_MusicMan.Update();

		if (g_WindowMan.ResolutionChanged()) {
			g_MenuMan.Reinitialize();
			g_ConsoleMan.Destroy();
			g_ConsoleMan.Initialize();
			g_LoadingScreen.CreateLoadingSplash();
			g_WindowMan.CompleteResolutionChange();
		}

		if (g_MenuMan.Update()) {
			g_UInputMan.EndFrame();
			break;
		}

		g_ConsoleMan.Update();

		g_UInputMan.EndFrame();
		g_WindowMan.GetScreenBuffer()->Begin();
		g_MenuMan.Draw();
		g_ConsoleMan.Draw(g_FrameMan.GetBackBuffer32());
		g_WindowMan.GetScreenBuffer()->End();
		g_WindowMan.UploadFrame();
		if (g_SettingsMan.GetFPSLimit() > 0) {
			const auto frameInterval = std::chrono::duration<double>(1.0 / g_SettingsMan.GetFPSLimit());
			std::this_thread::sleep_until(frameWallStart + std::chrono::duration_cast<std::chrono::steady_clock::duration>(frameInterval));
		}
	}

	g_MenuMan.SetIsInMenuScreen(false);
}

/// <summary>
/// Game simulation loop.
/// </summary>
void RunGameLoop() {
	if (System::IsSetToQuit()) {
		return;
	}
	g_TimerMan.PauseSim(false);

	if (g_ActivityMan.ActivitySetToRestart()) {
		g_LoadingScreen.DrawLoadingSplash();
		g_WindowMan.UploadFrame();
		if (!g_ActivityMan.RestartActivity()) {
			// This doesn't work.
			// Somewhat related to https://github.com/cortex-command-community/Cortex-Command-Community-Project-Source/issues/472
			// Deal with later.
			// g_MenuMan.GetTitleScreen()->SetTitleTransitionState(TitleScreen::TitleTransition::ScrollingFadeIn);
		}
	}

	long long updateStartTime = 0;
	long long updateTotalTime = 0;
	long long updateEndAndDrawStartTime = 0;
	long long drawStartTime = 0;
	long long drawTotalTime = 0;

	while (!System::IsSetToQuit()) {
		const auto frameWallStart = std::chrono::steady_clock::now();
		if (const Activity* activity = g_ActivityMan.GetActivity(); g_ActivityMan.IsInActivity() && activity) {
			DiscordPresence::SetActivity("Playing " + activity->GetPresetName(), "Cortex Command Reload");
		} else {
			DiscordPresence::SetActivity("At the main menu", "Cortex Command Reload");
		}
		bool serverUpdated = false;
		updateStartTime = g_TimerMan.GetAbsoluteTime();

		PollSDLEvents();
		g_WindowMan.Update();
		g_WindowMan.ClearBackbuffer();

		g_TimerMan.Update();

		// Simulation update, as many times as the fixed update step allows in the span since last frame draw.
		while (g_TimerMan.TimeForSimUpdate()) {
			ZoneScopedN("Simulation Update");

			serverUpdated = false;

			g_PerformanceMan.NewPerformanceSample();
			g_PerformanceMan.UpdateMSPSU();
			g_TimerMan.UpdateSim();
			if (worldStateClient && worldStateClient->IsConnected()) worldStateClient->Update();
			if (worldStateServer && worldStateServer->IsStarted()) {
				worldStateServer->Update(++worldStateServerSimulationTick);
			}
			bool worldStateSceneTransitionQueued = false;
			if (worldStateClient && worldStateClient->IsConnected() && worldStateClient->HasSnapshot()) {
				const WorldStateProtocol::Snapshot& snapshot = worldStateClient->GetLatestSnapshot();
				if (snapshot.SceneRevision != worldStateClientAppliedSceneRevision && snapshot.SceneRevision != worldStateClientRejectedSceneRevision) {
					std::string failureReason;
					if (QueueWorldStateActivity(snapshot, failureReason)) {
						worldStateClientAppliedSceneRevision = snapshot.SceneRevision;
						worldStateClientReplica.Forget();
						worldStateSceneTransitionQueued = true;
						DebugRunState& debugRunState = GetDebugRunState();
						debugRunState.WorldStateTransitionObserved = !debugRunState.RequireWorldStateTransition ||
						    (snapshot.ActivityPreset == debugRunState.ExpectedTransitionActivity && snapshot.ScenePreset == debugRunState.ExpectedTransitionScene);
						if (debugRunState.RequireWorldStateTransition && debugRunState.WorldStateTransitionObserved) {
							debugRunState.WorldStateTransitionObservedUpdate = debugRunState.SimulationUpdates + 1;
						}
						if (worldStateClientLog.is_open()) {
							worldStateClientLog << "activity_transition_queued=true revision=" << snapshot.SceneRevision << " activity=" << snapshot.ActivityModuleName << '/'
							                    << snapshot.ActivityClassName << '/' << snapshot.ActivityPreset << " scene=" << snapshot.SceneModuleName << '/' << snapshot.ScenePreset << '\n'
							                    << std::flush;
						}
					} else {
						worldStateClientRejectedSceneRevision = snapshot.SceneRevision;
						g_ConsoleMan.PrintString("ERROR: Disconnecting from server because its Activity/Scene is not available locally: " + failureReason);
						if (worldStateClientLog.is_open()) {
							worldStateClientLog << "ERROR: rejected server Activity/Scene revision " << snapshot.SceneRevision << ": " << failureReason << '\n' << std::flush;
						}
						worldStateClient->Disconnect();
					}
				}
			}

			g_PerformanceMan.StartPerformanceMeasurement(PerformanceMan::SimTotal);
			g_LuaMan.ClearScriptTimings();

			g_PerformanceMan.StartPerformanceMeasurement(PerformanceMan::LuaManagerUpdate);
			g_LuaMan.Update();
			g_PerformanceMan.StopPerformanceMeasurement(PerformanceMan::LuaManagerUpdate);

			g_PerformanceMan.StartPerformanceMeasurement(PerformanceMan::InputUpdate);
			g_UInputMan.Update();
			g_PerformanceMan.StopPerformanceMeasurement(PerformanceMan::InputUpdate);
			if (worldStateClient && worldStateClient->IsConnected()) {
				WorldStateProtocol::InputCommand inputCommand;
				inputCommand.ClientTick = static_cast<std::uint32_t>(g_TimerMan.GetSimUpdateCount());
				for (int element = 0; element < InputElements::INPUT_COUNT; ++element) {
					if (g_UInputMan.ElementHeld(Players::PlayerOne, element)) {
						inputCommand.HeldElements |= std::uint64_t{1} << element;
					}
				}
				const Vector mouseMovement = g_UInputMan.GetMouseMovement(Players::PlayerOne);
				inputCommand.MouseDeltaX = std::clamp(mouseMovement.GetFloorIntX(), -WorldStateProtocol::c_MaxMouseDelta, WorldStateProtocol::c_MaxMouseDelta);
				inputCommand.MouseDeltaY = std::clamp(mouseMovement.GetFloorIntY(), -WorldStateProtocol::c_MaxMouseDelta, WorldStateProtocol::c_MaxMouseDelta);
				auto QuantizeAnalog = [](float value) {
					return static_cast<std::int16_t>(std::lround(std::clamp(value, -1.0F, 1.0F) * 32767.0F));
				};
				const Vector analogMove = g_UInputMan.AnalogMoveValues(Players::PlayerOne);
				const Vector analogAim = g_UInputMan.AnalogAimValues(Players::PlayerOne);
				inputCommand.AnalogMoveX = QuantizeAnalog(analogMove.GetX());
				inputCommand.AnalogMoveY = QuantizeAnalog(analogMove.GetY());
				inputCommand.AnalogAimX = QuantizeAnalog(analogAim.GetX());
				inputCommand.AnalogAimY = QuantizeAnalog(analogAim.GetY());
				for (int button = 0; button < std::min<int>(MouseButtons::MAX_MOUSE_BUTTONS, 8); ++button) {
					if (g_UInputMan.MouseButtonHeld(button, Players::PlayerOne)) {
						inputCommand.MouseButtonsHeld |= static_cast<std::uint8_t>(1U << button);
					}
				}
				inputCommand.MouseWheelDelta = static_cast<std::int16_t>(std::clamp(g_UInputMan.MouseWheelMovedByPlayer(Players::PlayerOne),
				                                                                            -static_cast<int>(WorldStateProtocol::c_MaxMouseWheelDelta),
				                                                                            static_cast<int>(WorldStateProtocol::c_MaxMouseWheelDelta)));
				inputCommand.ResetActivityVote = g_UInputMan.KeyHeld(SDL_SCANCODE_BACKSPACE);
				inputCommand.RestartActivityVote = g_UInputMan.KeyHeld(SDL_SCANCODE_BACKSLASH);
				worldStateClient->SendInputCommand(inputCommand);
			}

			g_FrameMan.Update();

			g_MovableMan.CompleteQueuedMOIDDrawings();
			// Applying a snapshot can insert, remove, and mutate objects; wait until the
			// asynchronous MOID pass from the previous update has stopped using them.
			if (worldStateClient && worldStateClient->HasSnapshot() && !worldStateSceneTransitionQueued) {
				const std::uint32_t snapshotCount = worldStateClient->GetReceivedSnapshotCount();
				if (snapshotCount != worldStateClientLastObservedSnapshotCount) {
					const WorldStateClientReplica::ApplyResult applied = worldStateClientReplica.Apply(worldStateClient->GetLatestSnapshot());
					worldStateClientLastObservedSnapshotCount = snapshotCount;
					if (worldStateClientLog.is_open() &&
					    (worldStateClientLastLoggedSnapshotCount == 0 || snapshotCount - worldStateClientLastLoggedSnapshotCount >= 20)) {
						worldStateClientLog << "snapshot=" << snapshotCount << " updated=" << applied.Updated << " spawned=" << applied.Spawned << " removed=" << applied.Removed
						                    << " missing_presets=" << applied.MissingPresets << " position_error_samples=" << applied.PositionErrorsMeasured
						                    << " mean_position_error_before_correction_px=" << applied.MeanPositionErrorBeforeCorrection
						                    << " max_position_error_before_correction_px=" << applied.MaxPositionErrorBeforeCorrection << '\n'
						                    << std::flush;
						for (const std::string& detail : applied.MissingPresetDetails) {
							worldStateClientLog << "missing=" << detail << '\n';
						}
						worldStateClientLog.flush();
						worldStateClientLastLoggedSnapshotCount = snapshotCount;
					}
				}
			}

			g_ConsoleMan.Update();
			g_ActivityMan.Update();

			if (g_SceneMan.GetScene()) {
				g_SceneMan.GetScene()->Update();
			}
			if (worldStateClient && worldStateClient->IsConnected() && worldStateClient->HasSnapshot() && !worldStateSceneTransitionQueued) {
				const WorldStateProtocol::Snapshot& snapshot = worldStateClient->GetLatestSnapshot();
				Scene* localScene = g_SceneMan.GetScene();
				DebugRunState& debugRunState = GetDebugRunState();
				if (localScene && localScene->GetPresetName() == snapshot.ScenePreset && localScene->GetModuleName() == snapshot.SceneModuleName) {
					SLTerrain* localTerrain = localScene->GetTerrain();
					if (debugRunState.RequireRemoteTerrainMutationSmoke && !debugRunState.TerrainMutationProbeLocated && localTerrain) {
						debugRunState.TerrainMutationProbeLocated = FindWorldStateTerrainTestPixel(*localTerrain, debugRunState.TerrainMutationProbeX,
						                                                                    debugRunState.TerrainMutationProbeY);
						if (debugRunState.TerrainMutationProbeLocated) {
							debugRunState.Log << "world_state_terrain_mutation_probe=x:" << debugRunState.TerrainMutationProbeX
							                  << ",y:" << debugRunState.TerrainMutationProbeY << ",material:"
							                  << localTerrain->GetMaterialPixel(debugRunState.TerrainMutationProbeX, debugRunState.TerrainMutationProbeY) << '\n' << std::flush;
						}
					}
					const std::vector<WorldStateClientSession::ReceivedTerrainPatch> terrainPatches = worldStateClient->DrainTerrainPatches(snapshot.SceneRevision);
					std::uint32_t appliedTerrainPatches = 0;
					for (const WorldStateClientSession::ReceivedTerrainPatch& receivedPatch : terrainPatches) {
						const WorldStateProtocol::TerrainPatch& patch = receivedPatch.Patch;
						bool targetBecameAir = false;
						if (debugRunState.RequireRemoteTerrainMutationSmoke && debugRunState.TerrainMutationProbeLocated && localTerrain &&
						    patch.Layer == WorldStateProtocol::TerrainLayer::Material && debugRunState.TerrainMutationProbeX >= static_cast<int>(patch.X) &&
						    debugRunState.TerrainMutationProbeY >= static_cast<int>(patch.Y) && debugRunState.TerrainMutationProbeX < static_cast<int>(patch.X + patch.Width) &&
						    debugRunState.TerrainMutationProbeY < static_cast<int>(patch.Y + patch.Height)) {
							const std::size_t patchIndex = static_cast<std::size_t>(debugRunState.TerrainMutationProbeY - static_cast<int>(patch.Y)) * patch.Width +
							                               static_cast<std::size_t>(debugRunState.TerrainMutationProbeX - static_cast<int>(patch.X));
							const int localMaterialBefore = localTerrain->GetMaterialPixel(debugRunState.TerrainMutationProbeX, debugRunState.TerrainMutationProbeY);
							targetBecameAir = localMaterialBefore > g_MaterialCavity && patchIndex < patch.Pixels.size() && patch.Pixels[patchIndex] == g_MaterialAir;
							if (worldStateClientLog.is_open()) {
								worldStateClientLog << "terrain_probe_patch_received sequence=" << receivedPatch.Sequence << " before=" << localMaterialBefore
								                    << " patch=" << (patchIndex < patch.Pixels.size() ? static_cast<int>(patch.Pixels[patchIndex]) : -1)
								                    << "\n";
							}
						}
						if (worldStateClientReplica.ApplyTerrainPatch(patch)) {
							++appliedTerrainPatches;
							if (targetBecameAir && localTerrain->GetMaterialPixel(debugRunState.TerrainMutationProbeX, debugRunState.TerrainMutationProbeY) == g_MaterialAir) {
								debugRunState.RemoteTerrainMutationObserved = true;
								debugRunState.Log << "world_state_remote_terrain_mutation_observed=passed sequence=" << receivedPatch.Sequence
								                  << " x=" << debugRunState.TerrainMutationProbeX << " y=" << debugRunState.TerrainMutationProbeY << '\n' << std::flush;
							}
							if (debugRunState.RequireRemoteTerrainMutationSmoke && patch.Layer == WorldStateProtocol::TerrainLayer::Material &&
							    debugRunState.TerrainMutationProbeLocated && debugRunState.TerrainMutationProbeX >= static_cast<int>(patch.X) &&
							    debugRunState.TerrainMutationProbeY >= static_cast<int>(patch.Y) && debugRunState.TerrainMutationProbeX < static_cast<int>(patch.X + patch.Width) &&
							    debugRunState.TerrainMutationProbeY < static_cast<int>(patch.Y + patch.Height) && worldStateClientLog.is_open()) {
								worldStateClientLog << "terrain_probe_patch_applied sequence=" << receivedPatch.Sequence << " after="
								                    << localTerrain->GetMaterialPixel(debugRunState.TerrainMutationProbeX, debugRunState.TerrainMutationProbeY) << '\n' << std::flush;
							}
						} else if (worldStateClientLog.is_open()) {
							worldStateClientLog << "WARNING: rejected terrain patch sequence=" << receivedPatch.Sequence << " revision=" << receivedPatch.Patch.SceneRevision << '\n';
						}
					}
					if (appliedTerrainPatches && worldStateClientLog.is_open()) {
						worldStateClientLog << "terrain_patches_applied=" << appliedTerrainPatches << " revision=" << snapshot.SceneRevision << '\n' << std::flush;
					}
				}
			}

			g_MovableMan.Update();

			g_AudioMan.Update();
			g_MusicMan.Update();

			g_ActivityMan.LateUpdateGlobalScripts();
			if (g_PerformanceMan.GetOverlayLevel() == 2) g_PerformanceMan.UpdateSortedScriptTimings(g_LuaMan.GetScriptTimings());
			AdvanceDebugRunSimulation();

			// This is to support hot reloading entities in SceneEditorGUI. It's a bit hacky to put it in Main like this, but PresetMan has no update in which to clear the value, and I didn't want to set up a listener for the job.
			// It's in this spot to allow it to be set by UInputMan update and ConsoleMan update, and read from ActivityMan update.
			g_PresetMan.ClearReloadEntityPresetCalledThisUpdate();

			g_PerformanceMan.StopPerformanceMeasurement(PerformanceMan::SimTotal);
			g_UInputMan.EndFrame();

			if (!g_ActivityMan.IsInActivity()) {
				if (System::IsDebugRun()) {
					FinishDebugRun(false);
					break;
				}
				if (worldStateServerRequested) {
					// Keep the host loop alive after an Activity ends so clients stay connected.
				} else {
					g_TimerMan.PauseSim(true);

					if (!g_ActivityMan.ActivitySetToRestart()) {
						g_MenuMan.HandleTransitionIntoMenuLoop();
						RunMenuLoop();
					}
				}
			}
			if (g_ActivityMan.ActivitySetToRestart()) {
				g_LoadingScreen.DrawLoadingSplash();
				g_WindowMan.UploadFrame();
				if (!g_ActivityMan.RestartActivity()) {
					break;
				}
			}
			if (g_ActivityMan.ActivitySetToResume()) {
				g_ActivityMan.ResumeActivity();
				g_PerformanceMan.ResetSimUpdateTimer();
				updateStartTime = g_TimerMan.GetAbsoluteTime();
			}
		}

		updateEndAndDrawStartTime = g_TimerMan.GetAbsoluteTime();
		updateTotalTime = updateEndAndDrawStartTime - updateStartTime;
		drawStartTime = updateEndAndDrawStartTime;

		g_PerformanceMan.SetConnectedNetworkPlayers(worldStateServer ? worldStateServer->GetConnectedClientCount() : (worldStateClient && worldStateClient->IsConnected() ? 1 : 0));
		g_FrameMan.Draw();
		g_WindowMan.DrawPostProcessBuffer();
		g_WindowMan.UploadFrame();

		drawTotalTime = g_TimerMan.GetAbsoluteTime() - drawStartTime;
		if (System::IsDebugRun()) {
			DebugRunState& debugRunState = GetDebugRunState();
			debugRunState.TotalFrameTimeMilliseconds += static_cast<double>(updateTotalTime + drawTotalTime) / 1000.0;
			debugRunState.TotalRenderTimeMilliseconds += static_cast<double>(drawTotalTime) / 1000.0;
		}
		if (!System::IsDebugRun() && g_SettingsMan.GetFPSLimit() > 0) {
			const auto frameInterval = std::chrono::duration<double>(1.0 / g_SettingsMan.GetFPSLimit());
			std::this_thread::sleep_until(frameWallStart + std::chrono::duration_cast<std::chrono::steady_clock::duration>(frameInterval));
		}
		const auto actualFrameTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - frameWallStart).count();
		g_PerformanceMan.UpdateMSPF(updateTotalTime, drawTotalTime, actualFrameTime);
		CaptureDebugRunFrame();
	}
}

/// <summary>
/// Self-invoking lambda that installs exception handlers before Main is executed.
/// </summary>
static const bool RTESetExceptionHandlers = []() {
	RTEError::SetExceptionHandlers();
	return true;
}();

/// <summary>
/// Implementation of the main function.
/// </summary>
int main(int argc, char** argv) {
	const bool debugRun = ParseDebugRunArguments(argc, argv);
	System::SetDebugRun(debugRun);
	install_allegro(SYSTEM_NONE, &errno, std::atexit);
	loadpng_init();

#ifdef WIN32
	// Stops framespiking from our child threads being sat on for too long
	// TODO: use a better thread system that'll do what we want ASAP instead of letting the OS schedule all over us
	// Disabled for now because windows is great and this means when the game lags out it freezes the entire computer. Which we wouldn't expect with anything but REALTIME priority.
	// Because apparently high priority class is preferred over "processing mouse input"?!
	// SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif // WIN32

	// argv[0] actually unreliable for exe path and name, because of course, why would it be, why would anything be simple and make sense.
	// Just use it anyway until some dumb edge case pops up and it becomes a problem.
	System::Initialize(argv[0]);
	SeedRNG();

	if (!StartDebugRun()) {
		allegro_exit();
		return EXIT_FAILURE;
	}
	if (debugRun && GetDebugRunState().PreflightOnly) {
		DebugRunState& state = GetDebugRunState();
		state.Passed = true;
		state.Log << "result=passed\n" << std::flush;
		state.Log.close();
		allegro_exit();
		return EXIT_SUCCESS;
	}
	if (debugRun) {
		SDL_SetAssertionHandler(DebugRunSDLAssertionHandler, nullptr);
	}

	const SDL_InitFlags sdlInitFlags = SDL_INIT_VIDEO | SDL_INIT_EVENTS | (debugRun ? 0 : SDL_INIT_GAMEPAD);
	const bool sdlInitialized = SDL_Init(sdlInitFlags);
	if (debugRun) {
		GetDebugRunState().Log << "stage=sdl_initialized result=" << sdlInitialized << " error=" << SDL_GetError() << '\n' << std::flush;
	}
	if (!sdlInitialized) {
		std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
		if (debugRun) {
			GetDebugRunState().Log << "result=failed reason=sdl_initialization\n" << std::flush;
		}
		allegro_exit();
		return EXIT_FAILURE;
	}
	SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
	SDL_SetHint("SDL_ALLOW_TOPMOST", "0");
	SDL_HideCursor();
	if (!debugRun && std::filesystem::exists("Base.rte/gamecontrollerdb.txt")) {
		SDL_AddGamepadMappingsFromFile("Base.rte/gamecontrollerdb.txt");
	}

	InitializeManagers();
	if (debugRun && GetDebugRunState().OverlayCaptureEnabled) {
		g_PerformanceMan.SetOverlayLevel(2);
	}
	if (debugRun) {
		GetDebugRunState().Log << "stage=managers_initialized\n" << std::flush;
	}
	DiscordPresence::SetEnabled(g_SettingsMan.DiscordPresenceEnabled());
	DiscordPresence::Initialize();
	if (debugRun) {
		GetDebugRunState().Log << "stage=discord_initialized\n" << std::flush;
	}

	HandleMainArgs(argc, argv);
	if (debugRun) {
		GetDebugRunState().Log << "stage=arguments_handled\n" << std::flush;
	}

	if (debugRun) {
		g_SettingsMan.MeasureModuleLoadTime(true);
		GetDebugRunState().ModuleLoadStartTime = std::chrono::steady_clock::now();
	}
	g_PresetMan.LoadAllDataModules();
	if (debugRun) {
		GetDebugRunState().ModuleLoadEndTime = std::chrono::steady_clock::now();
		GetDebugRunState().SimulationStartTime = GetDebugRunState().ModuleLoadEndTime;
		GetDebugRunState().Log << "stage=modules_loaded\n" << std::flush;
	}
	if (!worldStateClientAddress.empty()) {
		const std::filesystem::path clientLogPath = debugRun ? (GetDebugRunState().OutputDirectory / "WorldStateClient.log") : std::filesystem::path("WorldStateClient.log");
		worldStateClientLog.open(clientLogPath, std::ios::out | std::ios::trunc);
		worldStateClient = std::make_unique<WorldStateClientSession>();
		if (!worldStateClient->Connect(worldStateClientAddress.c_str(), worldStateClientPort)) {
			std::cerr << "[NETWORK] Failed to start client transport\n";
			DestroyManagers();
			allegro_exit();
			SDL_Quit();
			return EXIT_FAILURE;
		}
		const auto connectionDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
		while (!worldStateClient->HasSnapshot() && std::chrono::steady_clock::now() < connectionDeadline) {
			worldStateClient->Update();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		if (!worldStateClient->HasSnapshot()) {
			std::cerr << "[NETWORK] Connected but no valid world snapshot arrived within 15 seconds\n";
			worldStateClient.reset();
			DestroyManagers();
			allegro_exit();
			SDL_Quit();
			return EXIT_FAILURE;
		}
		const WorldStateProtocol::Snapshot& snapshot = worldStateClient->GetLatestSnapshot();
		std::string failureReason;
		if (!QueueWorldStateActivity(snapshot, failureReason)) {
			std::cerr << "[NETWORK] " << failureReason << "; verify installed .rte modules\n";
			worldStateClient.reset();
			DestroyManagers();
			allegro_exit();
			SDL_Quit();
			return EXIT_FAILURE;
		}
		worldStateClientAppliedSceneRevision = snapshot.SceneRevision;
		if (worldStateClientLog.is_open()) {
			worldStateClientLog << "connected=true\nactivity=" << snapshot.ActivityClassName << '/' << snapshot.ActivityPreset << "\nscene=" << snapshot.SceneModuleName << '/' << snapshot.ScenePreset
			                    << "\nsnapshot_objects=" << snapshot.Objects.size() << "\n" << std::flush;
		}
		std::cout << "[NETWORK] Loaded server Activity '" << snapshot.ActivityPreset << "' and Scene '" << snapshot.ScenePreset << "' from module '"
		          << snapshot.SceneModuleName << "'\n";
	}
	if (debugRun && worldStateClientAddress.empty()) {
		g_ActivityMan.SetStartTutorialActivity();
		g_ActivityMan.SetRestartActivity();
	} else if (worldStateServerRequested) {
		// Prototype host mode needs a real Activity to simulate. Tutorial provides a
		// known built-in starting point until server config can select Activity/Scene.
		g_ActivityMan.SetStartTutorialActivity();
		g_ActivityMan.SetRestartActivity();
	}

	if (!System::IsInExternalModuleValidationMode()) {
		// Load the different input device icons. This can't be done during UInputMan::Create() because the icon presets don't exist so we need to do this after modules are loaded.
		g_UInputMan.LoadDeviceIcons();

		if (g_ConsoleMan.LoadWarningsExist()) {
			g_ConsoleMan.PrintString("WARNING: Encountered non-fatal errors during module loading!\nSee \"LogLoadingWarning.txt\" for information.");
			g_ConsoleMan.SaveLoadWarningLog("LogLoadingWarning.txt");
			// Open the console so the user is aware there are loading warnings.
			g_ConsoleMan.SetEnabled(true);
		} else {
			// Delete an existing log if there are no warnings so there's less junk in the root folder.
			if (std::filesystem::exists(System::GetWorkingDirectory() + "LogLoadingWarning.txt")) {
				std::remove("LogLoadingWarning.txt");
			}
		}

		const bool activityInitialized = g_ActivityMan.Initialize();
		if (!activityInitialized && !debugRun) {
			if (!worldStateServerRequested && worldStateClientAddress.empty()) {
				RunMenuLoop();
			} else if (!g_ActivityMan.ActivitySetToRestart()) {
				DestroyManagers();
				allegro_exit();
				SDL_Quit();
				return EXIT_FAILURE;
			}
		}
		if (worldStateServerRequested) {
			worldStateServer = std::make_unique<WorldStateServerSession>();
			const unsigned short serverPort = debugRun ? 0 : worldStateServerPort;
			const std::string serverLogPath = debugRun ? (GetDebugRunState().OutputDirectory / "WorldStateServer.log").string() : "WorldStateServer.log";
			const std::string bindAddress = debugRun ? "0.0.0.0" : worldStateServerBindAddress;
			if (!worldStateServer->Start(bindAddress, serverPort, worldStateServerMaxPlayers, serverLogPath, debugRun)) {
				worldStateServer.reset();
				DestroyManagers();
				allegro_exit();
				SDL_Quit();
				return EXIT_FAILURE;
			}
			if (debugRun && !VerifyWorldStateHostSessionLoopback(*worldStateServer, GetDebugRunState().Log)) {
				GetDebugRunState().HostSessionTestPassed = false;
			}
			if (debugRun && GetDebugRunState().RequestHostTerrainMutationSmoke) {
				worldStateServer->EnableDebugTerrainMutationSmoke();
			}
		}

		RunGameLoop();
	}

	g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
	g_ThreadMan.GetBackgroundThreadPool().wait_for_tasks();
	if (worldStateServer) {
		worldStateServer->Stop();
		worldStateServer.reset();
	}
	if (worldStateClientLog.is_open()) {
		if (worldStateClient) {
			worldStateClientLog << "snapshots_received_total=" << worldStateClient->GetReceivedSnapshotCount() << '\n';
		}
		worldStateClientLog.close();
	}
	if (worldStateClient) {
		worldStateClient->Disconnect();
		worldStateClient.reset();
	}
	DiscordPresence::Shutdown();

	DestroyManagers();

	allegro_exit();
	SDL_Quit();

	return debugRun && !GetDebugRunState().Passed ? EXIT_FAILURE : EXIT_SUCCESS;
}

#ifdef _WIN32
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) { return main(__argc, __argv); }
#endif
