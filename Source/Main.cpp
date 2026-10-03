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
#include "MetaMan.h"
#include "WindowMan.h"
#include "GLResourceMan.h"
#include "CameraMan.h"
#include "ActivityMan.h"
#include "MovableMan.h"
#include "PrimitiveMan.h"
#include "ThreadMan.h"
#include "LuaMan.h"
#include "MusicMan.h"
#include "System.h"
#include "System/UTF8.h"
#include "System/WorldStateProtocol.h"
#include "WorldStateTransport.h"
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
#include <fstream>
#include <filesystem>
#include <list>
#include <span>
#include <sstream>
#include <string_view>
#include <thread>
#include <unordered_set>

extern "C" {
FILE __iob_func[3] = {*stdin, *stdout, *stderr};
}

using namespace RTE;

namespace {
	struct DebugRunState {
		bool Enabled = false;
		bool StressStarted = false;
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

	bool ParseDebugRunArguments(int argc, char** argv) {
		DebugRunState& state = GetDebugRunState();
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

		state.OutputDirectory = std::filesystem::path(System::GetWorkingDirectory()) / System::GetScreenshotDirectory() / "DebugRuns";
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
		const bool utf8DecodePassed = UTF8::CountCodepoints(utf8Probe) == 3 && UTF8::NextBoundary(utf8Probe, 1) == 3 && UTF8::PreviousBoundary(utf8Probe, 3) == 1 && UTF8::CountCodepoints("\xF0\x28\x8C\x28") == 4;
		state.Log << "utf8_decoder_smoke=" << (utf8DecodePassed ? "passed" : "failed") << '\n' << std::flush;
		if (!utf8DecodePassed) {
			return false;
		}
		WorldStateProtocol::Snapshot networkSnapshot;
		networkSnapshot.Tick = 1234;
		networkSnapshot.SceneRevision = 7;
		networkSnapshot.ActivityPreset = "Tutorial Mission";
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
		networkActor.Team = 1;
		networkActor.Flags = 3;
		networkSnapshot.Objects.push_back(networkActor);
		std::vector<std::uint8_t> networkPacket;
		WorldStateProtocol::Snapshot decodedSnapshot;
		std::uint32_t decodedSequence = 0;
		const bool networkRoundTripPassed = WorldStateProtocol::EncodeSnapshot(networkSnapshot, 99, networkPacket) &&
		                                   WorldStateProtocol::DecodeSnapshot(networkPacket, decodedSnapshot, &decodedSequence) &&
		                                   decodedSequence == 99 && decodedSnapshot.Tick == networkSnapshot.Tick &&
		                                   decodedSnapshot.SceneRevision == networkSnapshot.SceneRevision && decodedSnapshot.Objects.size() == 1 &&
		                                   decodedSnapshot.Objects.front().NetworkId == networkActor.NetworkId &&
		                                   decodedSnapshot.Objects.front().PresetName == networkActor.PresetName &&
		                                   decodedSnapshot.Objects.front().PositionX == networkActor.PositionX &&
		                                   decodedSnapshot.Objects.front().Team == networkActor.Team && decodedSnapshot.Objects.front().Flags == networkActor.Flags;
		std::vector<std::uint8_t> truncatedNetworkPacket = networkPacket;
		if (!truncatedNetworkPacket.empty()) truncatedNetworkPacket.pop_back();
		std::vector<std::uint8_t> unsupportedVersionPacket = networkPacket;
		if (unsupportedVersionPacket.size() > 4) unsupportedVersionPacket[4] = static_cast<std::uint8_t>(WorldStateProtocol::c_Version + 1);
		WorldStateProtocol::Snapshot duplicateIdSnapshot = networkSnapshot;
		duplicateIdSnapshot.Objects.push_back(networkActor);
		std::vector<std::uint8_t> duplicateIdPacket;
		const bool malformedNetworkRejected = !networkPacket.empty() &&
		                                     !WorldStateProtocol::DecodeSnapshot(truncatedNetworkPacket, decodedSnapshot) &&
		                                     !WorldStateProtocol::DecodeSnapshot(unsupportedVersionPacket, decodedSnapshot) &&
		                                     !WorldStateProtocol::EncodeSnapshot(duplicateIdSnapshot, 100, duplicateIdPacket);
		state.Log << "world_state_protocol_smoke=" << (networkRoundTripPassed && malformedNetworkRejected ? "passed" : "failed") << '\n' << std::flush;
		if (!networkRoundTripPassed || !malformedNetworkRejected) {
			return false;
		}
		const std::string legacyCyrillicProbe("\xCF\xF0\xE8\xE2\xE5\xF2", 6);
		const bool legacyEncodingPassed = UTF8::PreserveLegacyWindows1251(legacyCyrillicProbe) == "Привет" && UTF8::PreserveLegacyWindows1251("Already UTF-8: Привет") == "Already UTF-8: Привет";
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
		state.Log << "mode=automated-gameplay-smoke\nupdates=" << state.UpdateLimit << "\nscene=Tutorial Bunker\n" << std::flush;
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
		if (state.SimulationUpdates % 60 == 0) {
			state.Log << "simulation_update=" << state.SimulationUpdates << " actors=" << g_MovableMan.GetActorCount()
			          << " particles=" << g_MovableMan.GetParticleCount() << '\n' << std::flush;
		}
	}

	WorldStateProtocol::Snapshot CaptureDebugWorldStateSnapshot() {
		WorldStateProtocol::Snapshot snapshot;
		snapshot.Tick = static_cast<std::uint32_t>(GetDebugRunState().SimulationUpdates);
		if (const Activity* activity = g_ActivityMan.GetActivity()) {
			snapshot.ActivityPreset = activity->GetPresetName();
		}
		if (const Scene* scene = g_SceneMan.GetScene()) {
			snapshot.ScenePreset = scene->GetPresetName();
		}

		std::list<SceneObject*> objects;
		g_MovableMan.GetAllActors(false, objects);
		g_MovableMan.GetAllItems(false, objects);
		g_MovableMan.GetAllParticles(false, objects);
		std::unordered_set<std::uint64_t> includedObjectIds;
		includedObjectIds.reserve(objects.size());
		snapshot.Objects.reserve(objects.size());
		for (const SceneObject* sceneObject : objects) {
			const MovableObject* movableObject = dynamic_cast<const MovableObject*>(sceneObject);
			if (!movableObject) {
				continue;
			}
			const std::uint64_t networkId = static_cast<std::uint64_t>(movableObject->GetUniqueID());
			if (networkId == 0 || !includedObjectIds.insert(networkId).second) {
				continue;
			}
			WorldStateProtocol::ObjectState object;
			object.NetworkId = networkId;
			object.ClassName = movableObject->GetClass().GetName();
			object.ModuleName = movableObject->GetModuleName();
			object.PresetName = movableObject->GetPresetName();
			object.PositionX = movableObject->GetPos().GetX();
			object.PositionY = movableObject->GetPos().GetY();
			object.VelocityX = movableObject->GetVel().GetX();
			object.VelocityY = movableObject->GetVel().GetY();
			object.Rotation = movableObject->GetRotAngle();
			object.AngularVelocity = movableObject->GetAngularVel();
			if (const auto* actor = dynamic_cast<const Actor*>(movableObject)) {
				object.Health = actor->GetHealth();
			}
			object.Team = static_cast<std::int16_t>(movableObject->GetTeam());
			snapshot.Objects.push_back(std::move(object));
		}
		return snapshot;
	}

	bool VerifyWorldStateTransportLoopback(std::span<const std::uint8_t> packet, const WorldStateProtocol::Snapshot& expectedSnapshot, std::ofstream& log) {
		bool loopbackPassed = false;
		bool serverStarted = false;
		bool clientStarted = false;
		std::size_t sendCount = 0;
		std::size_t serverPacketCount = 0;
		std::size_t clientPacketCount = 0;
		std::uint16_t boundPort = 0;
		{
			WorldStateTransport serverTransport;
			WorldStateTransport clientTransport;
			serverStarted = serverTransport.StartServer(0, 1, "127.0.0.1");
			if (serverStarted) {
				boundPort = serverTransport.GetBoundPort();
				clientStarted = boundPort != 0 && clientTransport.StartClient("127.0.0.1", boundPort);
				if (clientStarted) {
					const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
					while (std::chrono::steady_clock::now() < deadline && !loopbackPassed) {
						std::vector<WorldStateTransport::ReceivedPacket> serverPackets;
						std::vector<WorldStateTransport::ReceivedPacket> clientPackets;
						serverTransport.Poll(serverPackets);
						clientTransport.Poll(clientPackets);
						serverPacketCount += serverPackets.size();
						clientPacketCount += clientPackets.size();
						for (const WorldStateTransport::ReceivedPacket& received : serverPackets) {
							if (received.Identifier == ID_NEW_INCOMING_CONNECTION && serverTransport.SendWorldState(received.Sender, packet)) {
								++sendCount;
							}
						}
		for (const WorldStateTransport::ReceivedPacket& received : clientPackets) {
			if (received.Identifier != ID_CCR_WORLD_STATE) {
				continue;
			}
			WorldStateProtocol::Snapshot transportedSnapshot;
							loopbackPassed = WorldStateProtocol::DecodeSnapshot(received.Payload, transportedSnapshot) &&
							                 transportedSnapshot.Tick == expectedSnapshot.Tick &&
							                 transportedSnapshot.ScenePreset == expectedSnapshot.ScenePreset &&
							                 transportedSnapshot.Objects.size() == expectedSnapshot.Objects.size() &&
							                 (transportedSnapshot.Objects.empty() || transportedSnapshot.Objects.front().PresetName == expectedSnapshot.Objects.front().PresetName);
							break;
						}
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
					}
				}
			}
		}
		log << "world_state_transport_setup=server:" << serverStarted << ",port:" << boundPort
		    << ",client:" << clientStarted << ",server_packets:" << serverPacketCount
		    << ",client_packets:" << clientPacketCount << ",sends:" << sendCount
		    << ",loopback:" << (loopbackPassed ? "passed" : "failed") << '\n' << std::flush;
		return loopbackPassed;
	}

	void FinishDebugRun(bool success) {
		DebugRunState& state = GetDebugRunState();
		if (!state.Enabled || System::IsSetToQuit()) {
			return;
		}
		const WorldStateProtocol::Snapshot worldSnapshot = CaptureDebugWorldStateSnapshot();
		std::vector<std::uint8_t> worldSnapshotPacket;
		WorldStateProtocol::Snapshot decodedWorldSnapshot;
		const bool worldSnapshotPassed = WorldStateProtocol::EncodeSnapshot(worldSnapshot, state.SimulationUpdates, worldSnapshotPacket) &&
		                                 WorldStateProtocol::DecodeSnapshot(worldSnapshotPacket, decodedWorldSnapshot) &&
		                                 decodedWorldSnapshot.Objects.size() == worldSnapshot.Objects.size();
		state.Log << "captured_world_snapshot_objects=" << worldSnapshot.Objects.size()
		          << " bytes=" << worldSnapshotPacket.size()
		          << " result=" << (worldSnapshotPassed ? "passed" : "failed") << '\n' << std::flush;
		const bool liveSnapshotTransportPassed = worldSnapshotPassed && VerifyWorldStateTransportLoopback(worldSnapshotPacket, decodedWorldSnapshot, state.Log);
		success = success && worldSnapshotPassed && liveSnapshotTransportPassed;
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
		for (const auto& [name, counter] : std::array<std::pair<const char*, PerformanceMan::PerformanceCounters>, 8>{ {
		         {"simulation", PerformanceMan::SimTotal}, {"ai", PerformanceMan::ActorsAI}, {"actor_travel", PerformanceMan::ActorsTravel},
		         {"actor_update", PerformanceMan::ActorsUpdate}, {"particle_travel", PerformanceMan::ParticlesTravel}, {"particle_update", PerformanceMan::ParticlesUpdate},
		         {"activity", PerformanceMan::ActivityUpdate}, {"lua_scripts", PerformanceMan::ScriptsUpdate}
	         } }) {
			state.Log << "average_" << name << "_ms=" << static_cast<double>(g_PerformanceMan.GetAveragePerformanceTime(counter)) / 1000.0 << '\n';
		}
		state.Log.flush();
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
			const std::string imageName = "DebugRuns/debug-run-" + std::string(label) + ".png";
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
			FinishDebugRun(state.StressStarted);
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

	if (g_AudioMan.Initialize()) {
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

			g_PerformanceMan.StartPerformanceMeasurement(PerformanceMan::SimTotal);

			g_LuaMan.Update();

			g_UInputMan.Update();

			g_FrameMan.Update();

			g_MovableMan.CompleteQueuedMOIDDrawings();

			g_ConsoleMan.Update();
			g_ActivityMan.Update();

			if (g_SceneMan.GetScene()) {
				g_SceneMan.GetScene()->Update();
			}

			g_LuaMan.ClearScriptTimings();
			g_MovableMan.Update();
			g_PerformanceMan.UpdateSortedScriptTimings(g_LuaMan.GetScriptTimings());

			g_AudioMan.Update();
			g_MusicMan.Update();

			g_ActivityMan.LateUpdateGlobalScripts();
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
				g_TimerMan.PauseSim(true);

				if (!g_ActivityMan.ActivitySetToRestart()) {
					g_MenuMan.HandleTransitionIntoMenuLoop();
					RunMenuLoop();
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

		g_FrameMan.Draw();
		g_WindowMan.DrawPostProcessBuffer();
		g_WindowMan.UploadFrame();

		drawTotalTime = g_TimerMan.GetAbsoluteTime() - drawStartTime;
		if (System::IsDebugRun()) {
			DebugRunState& debugRunState = GetDebugRunState();
			debugRunState.TotalFrameTimeMilliseconds += static_cast<double>(updateTotalTime + drawTotalTime) / 1000.0;
			debugRunState.TotalRenderTimeMilliseconds += static_cast<double>(drawTotalTime) / 1000.0;
		}
		g_PerformanceMan.UpdateMSPF(updateTotalTime, drawTotalTime);
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

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD );

	SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
	SDL_SetHint("SDL_ALLOW_TOPMOST", "0");
	SDL_HideCursor();

	if (std::filesystem::exists("Base.rte/gamecontrollerdb.txt")) {
		SDL_AddGamepadMappingsFromFile("Base.rte/gamecontrollerdb.txt");
	}

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

	InitializeManagers();
	if (!StartDebugRun()) {
		DestroyManagers();
		allegro_exit();
		SDL_Quit();
		return EXIT_FAILURE;
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
	if (debugRun) {
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

		if (!g_ActivityMan.Initialize() && !debugRun) {
			RunMenuLoop();
		}

		RunGameLoop();
	}

	g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
	g_ThreadMan.GetBackgroundThreadPool().wait_for_tasks();
	DiscordPresence::Shutdown();

	DestroyManagers();

	allegro_exit();
	SDL_Quit();

	return EXIT_SUCCESS;
}

#ifdef _WIN32
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) { return main(__argc, __argv); }
#endif
