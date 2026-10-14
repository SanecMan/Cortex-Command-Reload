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
#include "GUIReader.h"
#include "AllegroScreen.h"
#include "AllegroBitmap.h"
#include "GUIFont.h"
#include "GUITextPanel.h"

#include "MainMenuGUI.h"
#include "ScenarioGUI.h"
#include "PauseMenuGUI.h"
#include "TitleScreen.h"
#include "LoadingScreen.h"
#include "MOPixel.h"
#include "System/ServerConfiguration.h"
#include "System/ServerConsole.h"

#include "MenuMan.h"
#include "ConsoleMan.h"
#include "SettingsMan.h"
#include "PresetMan.h"
#include "DataModule.h"
#include "SoundContainer.h"
#include "SoundSet.h"
#include "SoundPlaybackTimeline.h"
#include "WorldStateSoundCapture.h"
#include "WorldStateSoundPlayback.h"
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
#include "RenderTarget.h"
#include "BigTexture.h"
#include "SceneLayer.h"
#include "CameraMan.h"
#include "ActivityMan.h"
#include "MovableMan.h"
#include "MovableObject.h"
#include "Entities/Activity.h"
#include "Entities/AHuman.h"
#include "Entities/HDFirearm.h"
#include "Entities/Magazine.h"
#include "Entities/SceneObject.h"
#include "Activities/GameActivity.h"
#include "PrimitiveMan.h"
#include "ThreadMan.h"
#include "LuaMan.h"
#include "MusicMan.h"
#include "System.h"
#include "System/Reader.h"
#include "System/UTF8.h"
#include "System/UTF8Zip.h"
#include "System/WorldStateProtocol.h"
#include "System/WorldStateTerrainTest.h"
#include "System/DeliveryValidationTest.h"
#include "System/WorldStatePurchaseTest.h"
#include "System/WorldStateBuyMenu.h"
#include "System/WorldStateConstructionMenu.h"
#include "System/WorldStateInventoryTest.h"
#include "System/WorldStateAttachmentTest.h"
#include "WorldStateCompression.h"
#include "WorldStateTransport.h"
#include "WorldStateSnapshotBuilder.h"
#include "WorldStateClientSession.h"
#include "WorldStateClientReplica.h"
#include "WorldStateAttachments.h"
#include "WorldStateServerSession.h"
#include "NetworkMessages.h"
#include "DiscordPresence.h"

#include "RenderTarget.h"
#include "tracy/Tracy.hpp"

#include "imgui_impl_sdl3.h"

#ifdef _WIN32
#include "windows.h"
#include <psapi.h>
#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")
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
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

extern "C" {
FILE __iob_func[3] = {*stdin, *stdout, *stderr};
}

using namespace RTE;

namespace {
	bool worldStateServerRequested = false;
	bool worldStateServerConfigurationFailed = false;
	unsigned short worldStateServerPort = 8000;
	bool worldStateServerPortSpecified = false;
	unsigned short worldStateServerMaxPlayers = 8;
	std::string worldStateServerBindAddress = "0.0.0.0";
	std::string worldStateServerActivityClass;
	std::string worldStateServerActivityPreset;
	std::string worldStateServerTeamTech;
	std::string worldStateServerScenePreset;
	std::unique_ptr<WorldStateServerSession> worldStateServer;
	std::uint32_t worldStateServerSimulationTick = 0;
	std::string worldStateClientAddress;
	unsigned short worldStateClientPort = 8000;
	std::unique_ptr<WorldStateClientSession> worldStateClient;
	std::unique_ptr<WorldStateBuyMenu> worldStateBuyMenu;
	std::unique_ptr<WorldStateConstructionMenu> worldStateConstructionMenu;
	WorldStateClientReplica worldStateClientReplica;
	WorldStateSoundPlayback worldStateSoundPlayback;
	std::ofstream worldStateClientLog;
	std::uint32_t worldStateClientLastObservedSnapshotCount = 0;
	std::uint32_t worldStateClientLastLoggedSnapshotCount = 0;
	std::uint32_t worldStateClientAppliedSceneRevision = 0;
	std::uint32_t worldStateClientRejectedSceneRevision = 0;
	std::uint64_t worldStateClientReplicaSimulationTicks = 0;
	std::uint64_t worldStateClientLocalSimulationTicks = 0;

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

		if (worldStateServer) {
			worldStateServer->ConfigureActivityPlayers(*activity, true);
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

	std::unique_ptr<WorldStateClientSession> pendingMenuClient;
	std::filesystem::path menuClientLogPath = "WorldStateClient.log";
	std::chrono::steady_clock::time_point menuConnectionDeadline;
	bool menuConnectionActivated = false;

	void CancelMenuConnection() {
		if (pendingMenuClient) pendingMenuClient->Disconnect();
		pendingMenuClient.reset();
	}

	bool BeginMenuConnection(const std::string& address, unsigned short port, std::string& status) {
		MainMenuGUI::RetryServerAddress = address;
		MainMenuGUI::RetryServerPort = port;
		CancelMenuConnection();
		pendingMenuClient = std::make_unique<WorldStateClientSession>();
		if (!pendingMenuClient->Connect(address.c_str(), port)) {
			CancelMenuConnection();
			status = "Unable to start connection. Check the address.";
			return false;
		}
		menuConnectionDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
		status = "Connecting to " + address + ":" + std::to_string(port) + "...";
		return true;
	}

	int PollMenuConnection(std::string& status) {
		if (!pendingMenuClient) { status = "Connection cancelled."; return -1; }
		pendingMenuClient->Update();
		if (!pendingMenuClient->GetConnectionError().empty()) {
			status = pendingMenuClient->GetConnectionError();
			CancelMenuConnection();
			return -1;
		}
		if (pendingMenuClient->IsConnected() && pendingMenuClient->HasSnapshot()) {
			const auto& snapshot = pendingMenuClient->GetLatestSnapshot();
			if (!QueueWorldStateActivity(snapshot, status)) {
				CancelMenuConnection();
				return -1;
			}
			if (worldStateClient) worldStateClient->Disconnect();
			worldStateClientReplica.Forget();
			g_FrameMan.SetClientGUIRenderer({});
			worldStateBuyMenu.reset(); worldStateConstructionMenu.reset();
			worldStateClientAppliedSceneRevision = snapshot.SceneRevision;
			worldStateClientRejectedSceneRevision = 0;
			worldStateClientLastObservedSnapshotCount = worldStateClientLastLoggedSnapshotCount = 0;
			worldStateClientReplicaSimulationTicks = worldStateClientLocalSimulationTicks = 0;
			if (worldStateClientLog.is_open()) worldStateClientLog.close();
			worldStateClientLog.open(menuClientLogPath, std::ios::out | std::ios::trunc);
			worldStateClientLog << "connection_source=main_menu\nconnected=true\nactivity=" << snapshot.ActivityPreset
			                    << "\nscene=" << snapshot.ScenePreset << '\n' << std::flush;
			worldStateClient = std::move(pendingMenuClient);
			menuConnectionActivated = true;
			MainMenuGUI::DisconnectedNetworkSession = false;
			status = "Connected. Loading server scene...";
			return 1;
		}
		if (std::chrono::steady_clock::now() >= menuConnectionDeadline) {
			status = "No valid world snapshot within 15 seconds. Check address, port and server version.";
			CancelMenuConnection();
			return -1;
		}
		status = pendingMenuClient->IsConnected() ? "Connected. Waiting for world state..." : "Connecting... (Back or Escape to cancel)";
		return 0;
	}

	bool ConfigureWorldStateHostActivity(std::string& failureReason) {
		const bool hasActivityClass = !worldStateServerActivityClass.empty();
		const bool hasActivityPreset = !worldStateServerActivityPreset.empty();
		const bool hasScenePreset = !worldStateServerScenePreset.empty();
		if (hasActivityClass != hasActivityPreset || (hasActivityClass && !hasScenePreset)) {
			failureReason = "world-state server Activity requires both class and preset, and a custom Scene preset";
			return false;
		}
		if (!hasActivityClass && !hasScenePreset) {
			g_ActivityMan.SetStartTutorialActivity();
			g_ActivityMan.SetRestartActivity();
			return true;
		}

		const std::string activityClass = hasActivityClass ? worldStateServerActivityClass : "GATutorial";
		const std::string activityPresetName = hasActivityClass ? worldStateServerActivityPreset : "Tutorial Mission";
		const Entity* activityPreset = g_PresetMan.GetEntityPreset(activityClass, activityPresetName);
		Entity* activityClone = activityPreset ? activityPreset->Clone() : nullptr;
		Activity* activity = dynamic_cast<Activity*>(activityClone);
		if (!activity) {
			delete activityClone;
			failureReason = "world-state server Activity preset is unavailable: " + activityClass + "/" + activityPresetName;
			return false;
		}
		if (!hasActivityClass) {
			if (GameActivity* gameActivity = dynamic_cast<GameActivity*>(activity)) {
				gameActivity->SetStartingGold(10000);
			}
		}
		if (!worldStateServerTeamTech.empty()) {
			auto* gameActivity = dynamic_cast<GameActivity*>(activity);
			const int module = g_PresetMan.GetModuleID(worldStateServerTeamTech);
			if (!gameActivity || module < 0 || !g_PresetMan.GetDataModule(module)->IsFaction()) {
				delete activity; failureReason = "server TeamTech requires a loaded faction module: " + worldStateServerTeamTech; return false;
			}
			for (int team = Activity::TeamOne; team < Activity::MaxTeamCount; ++team) gameActivity->SetTeamTech(team, worldStateServerTeamTech);
		}

		if (g_SceneMan.SetSceneToLoad(worldStateServerScenePreset) < 0) {
			delete activity;
			failureReason = "world-state server Scene preset is unavailable: " + worldStateServerScenePreset;
			return false;
		}
		g_ActivityMan.SetStartActivity(activity);
		g_ActivityMan.SetRestartActivity();
		return true;
	}

	struct DebugRunState {
		bool NetworkScreenTextVerified = false;
		bool NetworkObjectivesVerified = false;
		bool NetworkObjectivesCleared = false;
		bool EnableDebugAudio = false;
		std::uint64_t NetworkSoundFixture = 0, NetworkSoundVoice = 0;
		bool NetworkSoundPlayVerified = false, NetworkSoundStopVerified = false;
		bool NetworkSoundFinishRequested = false, NetworkSoundFinishVerified = false;
		bool NetworkSoundFadeRequested = false, NetworkSoundFadeMidpoint = false, NetworkSoundFadeFrozen = false, NetworkSoundFadeEnded = false;
		bool ReloadHudInitialCaptured = false, ReloadHudChangedCaptured = false;
		bool ShotActorReady = false, ShotFlashCaptured = false, ShotFlashExpired = false;
		std::uint32_t ShotFlashSequence = 0;
		int ShotFlashRounds = 0;
		int ShotFlashCaptureUpdate = 0;
		float NetworkSoundFadePausedVolume = -1;
		double NetworkSoundFadePauseTime = 0;
		bool Enabled = false;
		bool StressStarted = false;
		bool PerformanceCountersObserved = false;
		bool HasProtocolSmokeSnapshot = false;
		bool Passed = false;
		bool LuaFilesystemProbePassed = false;
		bool HostSessionTestPassed = true;
		bool OverlayCaptureEnabled = false;
		bool PreflightOnly = false;
		bool CPUGraphicsPreflight = false;
		bool HeadlessContentPreflight = false;
		bool HeadlessSimulationPreflight = false;
		bool HeadlessServerPreflight = false;
		bool DeliveryValidationTest = false;
		bool RequestPurchaseTest = false;
		bool RequestBuyMenuTest = false;
		bool RequestConstructionTest = false, ConstructionTestPassed = false;
		bool RequestDeploymentTest = false;
		std::uint64_t ConstructionDeploymentActorId = 0;
		int ConstructionDeploymentPistols = 0;
		float ConstructionDeploymentFunds = 0;
		int ConstructionTestStage = 0;
		bool ConstructionExpectedAffordable = false;
		float ConstructionInitialFunds = 0;
		float ConstructionObjectCost = 0;
		float ConstructionTerrainFunds = 0;
		std::uint64_t ConstructionActionUpdate = 0;
		bool RequirePurchasePolicyTest = false, PurchasePolicyTestObserved = false, RequirePurchasePricingTest = false;
		bool RequestInventoryTest=false, InventoryInitialObserved=false, InventoryMutationObserved=false, InventoryReuseObserved=false;
		int BuyMenuTestStage = 0;
		bool BuyMenuTestPassed = false, BuyMenuCatalogCaptured = false, BuyMenuLandingCaptured = false;
		float BuyMenuInitialFunds = 0, BuyMenuDisplayedCost = 0;
		WorldStatePurchaseTest PurchaseTest;
		ServerConfiguration HeadlessConfig{.ListenAddress = "127.0.0.1", .Port = 0};
		std::string ErrorProbe;
		bool LoadAllOptionalModules = false;
		bool RequestHostWorldStateTransition = false;
		bool RequireWorldStateTransition = false;
		bool RequestHostTerrainMutationSmoke = false;
		bool RequireRemoteTerrainMutationSmoke = false;
		bool RequestHostEquipmentMutationSmoke = false;
		bool HostControlSmoke = false;
		bool ClientControlSmoke = false;
		bool HostControlPassed = false;
		int HostControlPassedUpdate = 0;
		bool ClientControlPassed = false;
		bool ReplicaWorldExact = true;
		bool ReplicaActivityStateExact = true;
		bool ReplicaTeamFundsExact = true;
		AHuman* ControlTestActor = nullptr;
		float ControlStartX = 0;
		int ControlStartRounds = 0;
		std::uint64_t ControlBaselineId = 0;
		int ControlInputTicks = 0;
		bool RequireRemoteEquipmentMutationSmoke = false;
		bool HostEquipmentMutationApplied = false;
		bool RemoteEquipmentMutationObserved = false;
		int EquipmentMutationScheduledUpdate = 0;
		int EquipmentMutationChangedMagazines = 0;
		std::unordered_map<std::uint64_t, int> PreviousMagazineRoundsByNetworkId;
		bool PauseSimulationAfterClient = false;
		bool RequireClientDisconnectAfterPause = false;
		bool SawClientForPauseTest = false;
		bool SimulationPausedByDebugRun = false;
		bool SawClientBeforeDebugDisconnect = false;
		bool ClientDisconnectWhilePausedObserved = false;
		bool RemoteTerrainMutationObserved = false;
		bool TerrainMutationProbeLocated = false;
		int TerrainMutationProbeX = 0;
		int TerrainMutationProbeY = 0;
		bool WorldStateTransitionObserved = false;
		bool WorldStateTransitionApplied = false;
		bool HostWorldStateTransitionQueued = false;
		bool HostWorldStateTransitionApplied = false;
		bool HostActivityRosterPassed = true;
		bool PostTransitionStressBatchSpawned = false;
		int WorldStateTransitionObservedUpdate = 0;
		int WorldStateTransitionAppliedUpdate = 0;
		int HostWorldStateTransitionAppliedUpdate = 0;
		WorldStateProtocol::Snapshot ProtocolSmokeSnapshot;
		int HostWorldStateTransitionUpdate = 600;
		int PauseSimulationAfterClientUpdate = 0;
		std::string ExpectedTransitionActivity = "Skirmish Defense";
		std::string ExpectedTransitionScene = "Ketanot Hills";
		bool RequireReconnectMenuTest = false;
		bool ReconnectMenuQueued = false;
		bool ReconnectWorldApplied = false;
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

	WorldStateProtocol::Snapshot CaptureDebugWorldStateSnapshot();

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
			if (argument == "-debug-run-audio") state.EnableDebugAudio = true;
			if (argument == "-debug-overlay") state.OverlayCaptureEnabled = true;
			if (argument == "-debug-run-preflight-only") state.PreflightOnly = true;
			if (argument == "-debug-run-world-state-purchase") state.RequestPurchaseTest = true;
			if (argument == "-debug-run-world-state-buy-menu") state.RequestBuyMenuTest = true;
			if (argument == "-debug-run-world-state-construction") state.RequestConstructionTest = true;
			if (argument == "-debug-run-world-state-deployment") { state.RequestConstructionTest = true; state.RequestDeploymentTest = true; }
			if (argument == "-debug-run-world-state-purchase-policy") { state.RequestBuyMenuTest=true; state.RequirePurchasePolicyTest=true; }
			if (argument == "-debug-run-world-state-purchase-pricing") { state.RequestBuyMenuTest=true; state.RequirePurchasePolicyTest=true; state.RequirePurchasePricingTest=true; }
			if (argument == "-debug-run-world-state-inventory") state.RequestInventoryTest=true;
			if (argument == "-debug-run-cpu-graphics-preflight") { state.CPUGraphicsPreflight = true; state.PreflightOnly = true; }
			if (argument == "-debug-run-headless-content") { state.HeadlessContentPreflight = true; state.CPUGraphicsPreflight = true; state.PreflightOnly = true; }
			if (argument == "-debug-run-headless-simulation") { state.HeadlessSimulationPreflight = true; state.HeadlessContentPreflight = true; state.CPUGraphicsPreflight = true; state.PreflightOnly = true; }
			if (argument == "-debug-run-delivery-validation") {
				state.DeliveryValidationTest = true;
				state.HeadlessSimulationPreflight = state.HeadlessContentPreflight = state.CPUGraphicsPreflight = state.PreflightOnly = true;
			}
			if (argument == "-debug-run-headless-server") {
				state.HeadlessServerPreflight = true;
				state.HeadlessSimulationPreflight = state.HeadlessContentPreflight = state.CPUGraphicsPreflight = state.PreflightOnly = true;
			}
			if (argument == "-debug-run-error-probe" && i + 1 < argc) state.ErrorProbe = argv[++i];
			if (argument == "-debug-run-all-mods") state.LoadAllOptionalModules = true;
			if (argument == "-debug-run-world-state-transition") state.RequestHostWorldStateTransition = true;
			if (argument == "-debug-run-require-world-state-transition") state.RequireWorldStateTransition = true;
			if (argument == "-debug-run-pause-after-client") state.PauseSimulationAfterClient = true;
			if (argument == "-debug-run-require-client-disconnect-after-pause") state.RequireClientDisconnectAfterPause = true;
		if (argument == "-debug-run-world-state-transition-update" && i + 1 < argc) {
			int transitionUpdate = 0;
			const std::string_view transitionUpdateArgument(argv[++i]);
			const auto [end, error] = std::from_chars(transitionUpdateArgument.data(), transitionUpdateArgument.data() + transitionUpdateArgument.size(), transitionUpdate);
			if (error == std::errc{} && end == transitionUpdateArgument.data() + transitionUpdateArgument.size() && transitionUpdate > 0) {
				state.HostWorldStateTransitionUpdate = std::clamp(transitionUpdate, 1, 36000);
			}
		}
			if (argument == "-debug-run-world-state-terrain-mutation") state.RequestHostTerrainMutationSmoke = true;
			if (argument == "-debug-run-require-world-state-terrain-mutation") state.RequireRemoteTerrainMutationSmoke = true;
			if (argument == "-debug-run-world-state-equipment-mutation") state.RequestHostEquipmentMutationSmoke = true;
			if (argument == "-debug-run-world-state-control") { state.HostControlSmoke = true; state.RequestHostWorldStateTransition = true; }
			if (argument == "-debug-run-require-world-state-control") { state.ClientControlSmoke = true; state.RequireWorldStateTransition = true; }
			if (argument == "-debug-run-require-world-state-equipment-mutation") state.RequireRemoteEquipmentMutationSmoke = true;
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

		const std::filesystem::path outputName = UTF8::PathFromString(state.OutputDirectoryName);
		if (outputName.empty() || outputName.has_parent_path() || outputName.filename() != outputName || outputName == "." || outputName == ".." ||
		    state.OutputDirectoryName.find_first_of("/\\:") != std::string::npos) {
			std::cerr << "Debug run output must be a single directory name under the screenshots directory.\n";
			return false;
		}
		state.OutputDirectory = UTF8::PathFromString(System::GetWorkingDirectory()) / System::GetScreenshotDirectory() / UTF8::PathFromString(state.OutputDirectoryName);
		std::error_code filesystemError;
		const std::filesystem::file_status existingPathStatus = std::filesystem::symlink_status(state.OutputDirectory, filesystemError);
		if (!filesystemError && std::filesystem::is_symlink(existingPathStatus)) {
			std::cerr << "Debug run output path must not be a symlink: " << UTF8::PathToString(state.OutputDirectory) << '\n';
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
			const std::string name = UTF8::PathToString(entry.path().filename());
			const bool isDebugArtifact = name == "DebugFailure.log" || name == "DebugRun.log" || name == "LogLoading.txt" || name == "LogLoadingWarning.txt" || name == "LogConsole.txt" || name.starts_with("debug-run-");
			if (isDebugArtifact && (entry.is_regular_file(filesystemError) || entry.is_symlink(filesystemError))) {
				filesystemError.clear();
				std::filesystem::remove(entry.path(), filesystemError);
			}
			filesystemError.clear();
		}
		state.Log.open(state.OutputDirectory / "DebugRun.log", std::ios::out | std::ios::trunc);
		RTEError::SetDiagnosticOutputDirectory(UTF8::PathToString(state.OutputDirectory));
		if (!state.Log) {
			std::cerr << "Unable to open DebugRun.log for writing.\n";
			return false;
		}
		const std::string utf8Probe = "Aя🙂";
		const bool purchaseCodecPassed=TestPurchaseCodec();
		const bool policyCodecPassed=WorldStateProtocol::TestPurchasePolicyCodec();
		const bool inventoryCodecPassed=TestWorldStateInventoryCodec();
		const bool attachmentCodecPassed=TestWorldStateAttachmentCodec();
		state.Log << "world_state_attachment_codec=" << (attachmentCodecPassed ? "passed" : "failed") << '\n' << std::flush;
		if (!attachmentCodecPassed) return false;
		state.Log << "world_state_inventory_codec=" << (inventoryCodecPassed ? "passed" : "failed") << '\n' << std::flush;
		if (!inventoryCodecPassed) return false;
		state.Log << "world_state_purchase_policy_codec=" << (policyCodecPassed ? "passed" : "failed") << '\n' << std::flush;
		if (!policyCodecPassed) return false;
		state.Log << "world_state_purchase_codec=" << (purchaseCodecPassed ? "passed" : "failed") << '\n' << std::flush;
		if (!purchaseCodecPassed) return false;
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
		const std::string unicodePath = UTF8::PathToString(state.OutputDirectory / std::filesystem::u8path("тест-путь.tmp"));
		const bool utf8PathRoundTripPassed = UTF8::PathToString(UTF8::PathFromString(unicodePath)) == unicodePath;
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
		bool unicodeReaderOpened = false;
		{
			Reader unicodePathReader(unicodePath, false, nullptr, true, true);
			unicodeReaderOpened = unicodePathReader.ReaderOK();
		}
		bool unicodeGUIReaderOpened = false;
		{
			GUIReader unicodeGUIReader;
			unicodeGUIReader.Create(unicodePath);
			unicodeGUIReaderOpened = unicodeGUIReader.ReaderOK();
		}
		const std::string legacyFileName = "legacy-\xF2\xE5\xF1\xF2-\xEF\xF3\xF2\xFC.tmp";
		const std::string legacyPath = UTF8::PathToString(state.OutputDirectory) + "/" + legacyFileName;
		const std::string legacyPathUTF8 = UTF8::PreserveLegacyWindows1251Path(legacyPath);
		std::ofstream legacyFile(UTF8::PathFromString(legacyPathUTF8), std::ios::binary | std::ios::trunc);
		const bool legacyFileWritten = legacyFile.is_open() && static_cast<bool>(legacyFile << "legacy path reader smoke");
		legacyFile.close();
		bool legacyReaderOpened = false;
		{
			Reader legacyPathReader(legacyPath, false, nullptr, true, true);
			legacyReaderOpened = legacyPathReader.ReaderOK();
		}
		bool legacyGUIReaderOpened = false;
		{
			GUIReader legacyPathReader;
			legacyPathReader.Create(legacyPath);
			legacyGUIReaderOpened = legacyPathReader.ReaderOK();
		}
		std::error_code unicodeFileCleanupError;
		const bool unicodeFileRemoved = std::filesystem::remove(UTF8::PathFromString(unicodePath), unicodeFileCleanupError);
		std::error_code legacyFileExistsError;
		const bool legacyFileExistedBeforeCleanup = std::filesystem::exists(UTF8::PathFromString(legacyPathUTF8), legacyFileExistsError);
		std::error_code legacyFileCleanupError;
		const bool legacyFileRemoved = std::filesystem::remove(UTF8::PathFromString(legacyPathUTF8), legacyFileCleanupError);
		std::error_code legacyFileExistsAfterCleanupError;
		const bool legacyFileCleaned = !std::filesystem::exists(UTF8::PathFromString(legacyPathUTF8), legacyFileExistsAfterCleanupError);
		const bool unicodeFilePathPassed = utf8PathRoundTripPassed && unicodeFileWritten && unicodeFileRead && unicodePathResolved && unicodeReaderOpened && unicodeGUIReaderOpened &&
		                                   legacyFileWritten && legacyReaderOpened && legacyGUIReaderOpened && legacyFileExistedBeforeCleanup && legacyFileCleaned &&
		                                   !legacyFileExistsError && !legacyFileCleanupError && !legacyFileExistsAfterCleanupError &&
		                                   unicodeFileRemoved && !unicodeFileCleanupError;
		state.Log << "utf8_file_path_smoke=" << (unicodeFilePathPassed ? "passed" : "failed")
		          << " path_round_trip=" << utf8PathRoundTripPassed << " written=" << unicodeFileWritten << " read=" << unicodeFileRead << " resolved=" << unicodePathResolved
		          << " reader_opened=" << unicodeReaderOpened << " gui_reader_opened=" << unicodeGUIReaderOpened
		          << " legacy_written=" << legacyFileWritten << " legacy_reader_opened=" << legacyReaderOpened << " legacy_gui_reader_opened=" << legacyGUIReaderOpened
		          << " legacy_existed=" << legacyFileExistedBeforeCleanup << " legacy_removed=" << legacyFileRemoved << " legacy_cleaned=" << legacyFileCleaned
		          << " legacy_cleanup_error=" << legacyFileCleanupError.value() << " removed=" << unicodeFileRemoved << " cleanup_error=" << unicodeFileCleanupError.value()
		          << '\n' << std::flush;
		if (!unicodeFilePathPassed) {
			return false;
		}

		const std::string unicodeArchivePath = UTF8::PathToString(state.OutputDirectory / std::filesystem::u8path("debug-run-проверка-архива.zip"));
		constexpr std::string_view archiveEntryContents = "UTF-8 archive path round trip";
		zipFile unicodeArchiveWriter = UTF8::OpenZipForWriting(unicodeArchivePath, APPEND_STATUS_CREATE);
		zip_fileinfo archiveEntryInfo{};
		// This small probe needs no ZIP64 entry; the common API exists in both minizip variants.
		const int archiveEntryOpened = unicodeArchiveWriter ? zipOpenNewFileInZip(unicodeArchiveWriter, "entry.txt", &archiveEntryInfo, nullptr, 0, nullptr, 0,
		                                                                         nullptr, Z_DEFLATED, -1)
		                                                  : ZIP_PARAMERROR;
		const int archiveEntryWritten = archiveEntryOpened == ZIP_OK
		                                    ? zipWriteInFileInZip(unicodeArchiveWriter, archiveEntryContents.data(), static_cast<std::uint32_t>(archiveEntryContents.size()))
		                                    : ZIP_PARAMERROR;
		const int archiveEntryClosed = archiveEntryOpened == ZIP_OK ? zipCloseFileInZip(unicodeArchiveWriter) : ZIP_PARAMERROR;
		const int archiveClosed = unicodeArchiveWriter ? zipClose(unicodeArchiveWriter, nullptr) : ZIP_PARAMERROR;
		unzFile unicodeArchiveReader = (archiveEntryWritten == ZIP_OK && archiveEntryClosed == ZIP_OK && archiveClosed == ZIP_OK)
		                                   ? UTF8::OpenZipForReading(unicodeArchivePath)
		                                   : nullptr;
		const bool archiveEntryFound = unicodeArchiveReader && unzGoToFirstFile(unicodeArchiveReader) == UNZ_OK && unzOpenCurrentFile(unicodeArchiveReader) == UNZ_OK;
		std::array<char, archiveEntryContents.size()> archiveContents{};
		const int archiveBytesRead = archiveEntryFound ? unzReadCurrentFile(unicodeArchiveReader, archiveContents.data(), static_cast<std::uint32_t>(archiveContents.size())) : -1;
		const bool unicodeArchiveRoundTripPassed = archiveBytesRead == static_cast<int>(archiveContents.size()) &&
		                                          std::string_view(archiveContents.data(), archiveContents.size()) == archiveEntryContents;
		if (unicodeArchiveReader) {
			if (archiveEntryFound) unzCloseCurrentFile(unicodeArchiveReader);
			unzClose(unicodeArchiveReader);
		}
		std::error_code archiveCleanupError;
		const bool unicodeArchiveRemoved = std::filesystem::remove(UTF8::PathFromString(unicodeArchivePath), archiveCleanupError);
		const bool unicodeArchivePathPassed = archiveEntryOpened == ZIP_OK && archiveEntryWritten == ZIP_OK && archiveEntryClosed == ZIP_OK && archiveClosed == ZIP_OK &&
		                                      archiveEntryFound && unicodeArchiveRoundTripPassed && unicodeArchiveRemoved && !archiveCleanupError;
		state.Log << "utf8_zip_path_smoke=" << (unicodeArchivePathPassed ? "passed" : "failed") << " writer_open=" << (unicodeArchiveWriter != nullptr)
		          << " entry_open=" << (archiveEntryOpened == ZIP_OK) << " entry_written=" << (archiveEntryWritten == ZIP_OK)
		          << " archive_open=" << (unicodeArchiveReader != nullptr) << " entry_found=" << archiveEntryFound
		          << " round_trip=" << unicodeArchiveRoundTripPassed << " removed=" << unicodeArchiveRemoved << " cleanup_error=" << archiveCleanupError.value()
		          << '\n' << std::flush;
		if (!unicodeArchivePathPassed) {
			return false;
		}

		WorldStateProtocol::Snapshot networkSnapshot;
		networkSnapshot.Tick = 1234;
		networkSnapshot.SceneRevision = 7;
		networkSnapshot.ActivityState = Activity::Editing;
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
		networkActor.HasEquippedDevice = true;
		networkActor.EquippedDeviceClassName = "HDFirearm";
		networkActor.EquippedDeviceModuleName = "Base.rte";
		networkActor.EquippedDevicePresetName = "Тестовое оружие";
		networkActor.HasEquippedDeviceMagazine = true;
		networkActor.EquippedDeviceMagazineClassName = "Magazine";
		networkActor.EquippedDeviceMagazineModuleName = "Base.rte";
		networkActor.EquippedDeviceMagazinePresetName = "Тестовый магазин";
		networkActor.EquippedDeviceMagazineRounds = 17;
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
		networkSnapshot.Players[1] = {networkActor.NetworkId, 320.5F, 128.0F, networkActor.Team, true};
		std::vector<std::uint8_t> networkPacket;
		WorldStateProtocol::Snapshot decodedSnapshot;
		std::uint32_t decodedSequence = 0;
		WorldStateProtocol::Snapshot infiniteAmmoSnapshot = networkSnapshot;
		infiniteAmmoSnapshot.Objects.front().EquippedDeviceMagazineRounds = -7;
		std::vector<std::uint8_t> infiniteAmmoPacket;
		WorldStateProtocol::Snapshot decodedInfiniteAmmoSnapshot;
		const bool infiniteAmmoRoundTripPassed = WorldStateProtocol::EncodeSnapshot(infiniteAmmoSnapshot, 100, infiniteAmmoPacket) &&
		                                        WorldStateProtocol::DecodeSnapshot(infiniteAmmoPacket, decodedInfiniteAmmoSnapshot) &&
		                                        decodedInfiniteAmmoSnapshot.Objects.front().EquippedDeviceMagazineRounds == -7;
		const bool networkRoundTripPassed = WorldStateProtocol::EncodeSnapshot(networkSnapshot, 99, networkPacket) &&
		                                   WorldStateProtocol::DecodeSnapshot(networkPacket, decodedSnapshot, &decodedSequence) &&
		                                   decodedSnapshot.Players == networkSnapshot.Players &&
		                                   decodedSnapshot.ActivityState == networkSnapshot.ActivityState &&
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
		                                   decodedSnapshot.Objects.front().HasEquippedDevice &&
		                                   decodedSnapshot.Objects.front().EquippedDeviceClassName == networkActor.EquippedDeviceClassName &&
		                                   decodedSnapshot.Objects.front().EquippedDeviceModuleName == networkActor.EquippedDeviceModuleName &&
		                                   decodedSnapshot.Objects.front().EquippedDevicePresetName == networkActor.EquippedDevicePresetName &&
		                                   decodedSnapshot.Objects.front().HasEquippedDeviceMagazine &&
		                                   decodedSnapshot.Objects.front().EquippedDeviceMagazineClassName == networkActor.EquippedDeviceMagazineClassName &&
		                                   decodedSnapshot.Objects.front().EquippedDeviceMagazineModuleName == networkActor.EquippedDeviceMagazineModuleName &&
		                                   decodedSnapshot.Objects.front().EquippedDeviceMagazinePresetName == networkActor.EquippedDeviceMagazinePresetName &&
		                                   decodedSnapshot.Objects.front().EquippedDeviceMagazineRounds == networkActor.EquippedDeviceMagazineRounds &&
		                                   decodedSnapshot.Objects.back().PixelMaterialId == transientPixel.PixelMaterialId &&
		                                   decodedSnapshot.Objects.back().PixelColorIndex == transientPixel.PixelColorIndex &&
		                                   decodedSnapshot.Objects.back().PixelMass == transientPixel.PixelMass &&
		                                   decodedSnapshot.Objects.back().PixelLifetime == transientPixel.PixelLifetime &&
		                                   decodedSnapshot.Objects.back().PixelSharpness == transientPixel.PixelSharpness && infiniteAmmoRoundTripPassed;
		std::vector<std::uint8_t> truncatedNetworkPacket = networkPacket;
		bool activityStateCodecPassed = true;
		for (int activityState = WorldStateProtocol::c_NoActivity; activityState <= WorldStateProtocol::c_MaxActivityState; ++activityState) {
			auto stateProbe = networkSnapshot;
			stateProbe.ActivityState = static_cast<std::int8_t>(activityState);
			std::vector<std::uint8_t> statePacket;
			WorldStateProtocol::Snapshot decodedState;
			activityStateCodecPassed = activityStateCodecPassed && WorldStateProtocol::EncodeSnapshot(stateProbe, 1, statePacket) &&
			    WorldStateProtocol::DecodeSnapshot(statePacket, decodedState) && decodedState.ActivityState == activityState;
		}
		auto invalidState = networkSnapshot;
		invalidState.ActivityState = 127;
		std::vector<std::uint8_t> invalidStatePacket;
		activityStateCodecPassed = activityStateCodecPassed && !WorldStateProtocol::EncodeSnapshot(invalidState, 1, invalidStatePacket);
		invalidStatePacket = networkPacket;
		if (invalidStatePacket.size() > WorldStateProtocol::c_HeaderSize + 8) invalidStatePacket[WorldStateProtocol::c_HeaderSize + 8] = 127;
		activityStateCodecPassed = activityStateCodecPassed && !WorldStateProtocol::DecodeSnapshot(invalidStatePacket, decodedSnapshot);
		state.Log << "world_state_activity_state_codec=" << (activityStateCodecPassed ? "passed" : "failed") << '\n' << std::flush;
		WorldStateProtocol::Snapshot fundsProbe = networkSnapshot;
		fundsProbe.TeamFunds = {12345.25F, -17.5F, 0.0F, 1000000.0F};
		std::vector<std::uint8_t> fundsPacket;
		WorldStateProtocol::Snapshot decodedFunds;
		bool fundsCodecPassed = WorldStateProtocol::EncodeSnapshot(fundsProbe, 1, fundsPacket) &&
		    WorldStateProtocol::DecodeSnapshot(fundsPacket, decodedFunds) && decodedFunds.TeamFunds == fundsProbe.TeamFunds;
		for (std::size_t team = 0; team < WorldStateProtocol::c_TeamCount; ++team) {
			for (float invalidFunds : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()}) {
				WorldStateProtocol::Snapshot invalidFundsProbe = fundsProbe;
				invalidFundsProbe.TeamFunds[team] = invalidFunds;
				std::vector<std::uint8_t> invalidFundsPacket;
				fundsCodecPassed = fundsCodecPassed && !WorldStateProtocol::EncodeSnapshot(invalidFundsProbe, 1, invalidFundsPacket);
				invalidFundsPacket = fundsPacket;
				std::vector<std::uint8_t> invalidFloatBytes;
				WorldStateProtocol::Detail::WriteFloat(invalidFloatBytes, invalidFunds);
				const std::size_t fundsOffset = WorldStateProtocol::c_HeaderSize + 9 + team * sizeof(float);
				if (invalidFundsPacket.size() >= fundsOffset + sizeof(float)) std::copy(invalidFloatBytes.begin(), invalidFloatBytes.end(), invalidFundsPacket.begin() + fundsOffset);
				fundsCodecPassed = fundsCodecPassed && !WorldStateProtocol::DecodeSnapshot(invalidFundsPacket, decodedFunds);
			}
		}
		state.Log << "world_state_team_funds_codec=" << (fundsCodecPassed ? "passed" : "failed") << '\n' << std::flush;
		auto textProbe = networkSnapshot;
		textProbe.Players[1].ScreenText = "HUD: Привет, Ёжик!\nActivity message";
		textProbe.Players[1].TextBlinkInterval = 250;
		textProbe.Players[1].TextCentered = true;
		std::vector<std::uint8_t> textPacket;
		WorldStateProtocol::Snapshot decodedText;
		bool textCodecPassed = WorldStateProtocol::EncodeSnapshot(textProbe, 1, textPacket) &&
		    WorldStateProtocol::DecodeSnapshot(textPacket, decodedText) && decodedText.Players == textProbe.Players;
		for (const std::string& invalidText : {std::string("\xFF", 1), std::string(WorldStateProtocol::c_MaxScreenTextBytes + 1, 'x')}) {
			auto invalid = textProbe; invalid.Players[1].ScreenText = invalidText;
			std::vector<std::uint8_t> rejected;
			textCodecPassed = textCodecPassed && !WorldStateProtocol::EncodeSnapshot(invalid, 1, rejected);
		}
		auto invalidBlink = textProbe;
		invalidBlink.Players[1].TextBlinkInterval = std::numeric_limits<std::uint32_t>::max();
		std::vector<std::uint8_t> rejectedTextPacket;
		textCodecPassed = textCodecPassed && !WorldStateProtocol::EncodeSnapshot(invalidBlink, 1, rejectedTextPacket);
		const auto textStart = std::search(textPacket.begin(), textPacket.end(), textProbe.Players[1].ScreenText.begin(), textProbe.Players[1].ScreenText.end(),
		    [](std::uint8_t byte, char character) { return byte == static_cast<unsigned char>(character); });
		if (textStart == textPacket.end()) textCodecPassed = false;
		else {
			const auto index = static_cast<std::size_t>(textStart - textPacket.begin());
			auto invalid = textPacket; invalid[index] = 0xFF;
			textCodecPassed = textCodecPassed && !WorldStateProtocol::DecodeSnapshot(invalid, decodedText);
			invalid = textPacket; invalid[index + textProbe.Players[1].ScreenText.size() + 4] = 2;
			textCodecPassed = textCodecPassed && !WorldStateProtocol::DecodeSnapshot(invalid, decodedText);
		}
		state.Log << "world_state_screen_text_codec=" << (textCodecPassed ? "passed" : "failed") << '\n' << std::flush;
		fundsCodecPassed = fundsCodecPassed && textCodecPassed;
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
		WorldStateProtocol::Snapshot invalidEquipmentSnapshot = networkSnapshot;
		WorldStateProtocol::Snapshot invalidPlayerSnapshot = networkSnapshot;
		invalidPlayerSnapshot.Players[1].ControlledActorId = transientPixel.NetworkId;
		WorldStateProtocol::Snapshot invalidCameraSnapshot = networkSnapshot;
		invalidCameraSnapshot.Players[1].CameraX = std::numeric_limits<float>::quiet_NaN();
		invalidEquipmentSnapshot.Objects.back().HasEquippedDevice = true;
		invalidEquipmentSnapshot.Objects.back().EquippedDeviceClassName = "HDFirearm";
		invalidEquipmentSnapshot.Objects.back().EquippedDevicePresetName = "Invalid non-actor equipment";
		const bool malformedNetworkRejected = activityStateCodecPassed && fundsCodecPassed && !networkPacket.empty() &&
		                                     !WorldStateProtocol::DecodeSnapshot(truncatedNetworkPacket, decodedSnapshot) &&
		                                     !WorldStateProtocol::DecodeSnapshot(unsupportedVersionPacket, decodedSnapshot) &&
		                                     !WorldStateProtocol::EncodeSnapshot(duplicateIdSnapshot, 100, duplicateIdPacket) &&
		                                     !WorldStateProtocol::EncodeSnapshot(invalidTeamSnapshot, 100, duplicateIdPacket) &&
		                                     !WorldStateProtocol::EncodeSnapshot(invalidFlagsSnapshot, 100, duplicateIdPacket) &&
		                                     !WorldStateProtocol::EncodeSnapshot(invalidEquipmentSnapshot, 100, duplicateIdPacket) &&
		                                     !WorldStateProtocol::EncodeSnapshot(invalidPlayerSnapshot, 100, duplicateIdPacket) &&
		                                     !WorldStateProtocol::EncodeSnapshot(invalidCameraSnapshot, 100, duplicateIdPacket);
		state.Log << "world_state_protocol_smoke=" << (networkRoundTripPassed && malformedNetworkRejected ? "passed" : "failed")
		          << " infinite_ammo=" << (infiniteAmmoRoundTripPassed ? "passed" : "failed") << '\n' << std::flush;
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
		if (worldStateClient) {
			const MovableMan::SceneStats stats = g_MovableMan.CollectSceneStats();
			state.StressStarted = worldStateClient->IsConnected() && worldStateClient->HasSnapshot() && stats.Actors > 0 &&
			                      g_MovableMan.GetMovableObjectCount() > 0;
			state.Log << "replica_stress_probe actors=" << stats.Actors << " objects=" << g_MovableMan.GetMovableObjectCount()
			          << " result=" << (state.StressStarted ? "passed" : "waiting") << '\n' << std::flush;
			return;
		}
		// This fixture runs after MovableMan can schedule asynchronous Lua GC.
		// Native cloning through CreateAHuman requires an unlocked script state.
		g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
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

	int ApplyDebugWorldStateEquipmentMutation() {
		std::list<SceneObject*> actors;
		g_MovableMan.GetAllActors(false, actors);
		int changedMagazines = 0;
		for (SceneObject* sceneObject : actors) {
			auto* human = dynamic_cast<AHuman*>(sceneObject);
			auto* firearm = human ? dynamic_cast<HDFirearm*>(human->GetEquippedItem()) : nullptr;
			Magazine* magazine = firearm ? firearm->GetMagazine() : nullptr;
			if (!magazine) {
				continue;
			}

			const int currentRounds = magazine->GetRoundCount();
			// Infinite ammo is a persistent sentinel for this test, so an AI reload
			// between 20 Hz snapshots cannot erase the observed state transition.
			constexpr int c_DebugInfiniteAmmo = -1;
			const int targetRounds = c_DebugInfiniteAmmo;
			if (currentRounds != targetRounds) {
				magazine->SetRoundCount(targetRounds);
				++changedMagazines;
			}
		}
		return changedMagazines;
	}

	void AdvanceDebugRunSimulation() {
		DebugRunState& state = GetDebugRunState();
		if (!state.Enabled || System::IsSetToQuit()) {
			return;
		}
		++state.SimulationUpdates;
		if (state.HostControlSmoke && state.HostWorldStateTransitionApplied && worldStateServer &&
		    worldStateServer->GetConnectedClientCount() > 0 && !state.HostControlPassed) {
			Activity* activity = g_ActivityMan.GetActivity();
			const auto slots = worldStateServer->GetAssignedPlayerSlots();
			if (activity && !slots.empty() && !state.ControlTestActor) {
				// The normal update has just launched GC and MOID work. Native
				// cloning must not race those tasks when acquiring a Lua state.
				g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
				const Entity* actorPreset = g_PresetMan.GetEntityPreset("AHuman", "Fat Culled Clone", "Base.rte");
				const Entity* gunPreset = g_PresetMan.GetEntityPreset("HDFirearm", "Pistol", "Base.rte");
				AHuman* actor = actorPreset ? dynamic_cast<AHuman*>(actorPreset->Clone()) : nullptr;
				HDFirearm* gun = gunPreset ? dynamic_cast<HDFirearm*>(gunPreset->Clone()) : nullptr;
				if (actor && gun) {
					const int slot = slots.front();
					actor->SetTeam(activity->GetTeamOfPlayer(slot));
					actor->SetPos(g_SceneMan.MovePointToGround(Vector(400, 0), 0, 3));
					actor->AddInventoryItem(gun);
					actor->EquipFirearm();
					if (g_MovableMan.AddMO(actor)) {
						activity->SetPlayerBrain(actor, slot);
						// Keep the other human team alive so Skirmish does not end
						// before remote input arrives after client scene loading.
						for (int player = Players::PlayerOne; player < Players::MaxPlayerCount; ++player) {
							if (player == slot || !activity->PlayerHuman(player) || !activity->PlayerActive(player) ||
							    g_MovableMan.IsActor(activity->GetPlayerBrain(player))) continue;
							AHuman* brain = dynamic_cast<AHuman*>(actorPreset->Clone());
							if (!brain) continue;
							brain->SetTeam(activity->GetTeamOfPlayer(player));
							brain->SetPos(g_SceneMan.MovePointToGround(Vector(250 + player * 40, 0), 0, 3));
							brain->SetHealth(1000);
							if (g_MovableMan.AddMO(brain)) activity->SetPlayerBrain(brain, player);
							else delete brain;
						}
						activity->SetActivityState(Activity::Running);
						if (activity->SwitchToActor(actor, slot, actor->GetTeam())) {
							state.ControlTestActor = actor;
							state.ControlStartX = actor->GetPos().GetX();
							state.ControlStartRounds = gun->GetMagazine() ? gun->GetMagazine()->GetRoundCount() : 0;
							state.Log << "control_fixture_created slot=" << slot << " rounds=" << state.ControlStartRounds << '\n' << std::flush;
						}
					} else { delete actor; }
				} else { delete actor; delete gun; }
			}
			if (state.ControlTestActor && g_MovableMan.IsActor(state.ControlTestActor)) {
				const auto* gun = dynamic_cast<HDFirearm*>(state.ControlTestActor->GetEquippedItem());
				const int rounds = gun && gun->GetMagazine() ? gun->GetMagazine()->GetRoundCount() : state.ControlStartRounds;
				const float moved = std::abs(g_SceneMan.ShortestDistance(Vector(state.ControlStartX, 0), Vector(state.ControlTestActor->GetPos().GetX(), 0)).GetX());
				if (moved > 8 && rounds >= 0 && rounds < state.ControlStartRounds) {
					state.HostControlPassed = true;
					state.HostControlPassedUpdate = state.SimulationUpdates;
					state.Log << "world_state_host_control=passed moved=" << moved << " rounds=" << rounds << '\n' << std::flush;
				}
			}
		}
		if (state.RequestHostEquipmentMutationSmoke && worldStateServer && worldStateServer->GetConnectedClientCount() > 0 &&
		    !state.HostEquipmentMutationApplied) {
			if (state.EquipmentMutationScheduledUpdate == 0) {
				state.EquipmentMutationScheduledUpdate = state.SimulationUpdates + 120;
				state.Log << "world_state_host_equipment_mutation_scheduled=true at_update=" << state.EquipmentMutationScheduledUpdate << '\n' << std::flush;
			} else if (state.SimulationUpdates >= state.EquipmentMutationScheduledUpdate) {
				state.EquipmentMutationChangedMagazines = ApplyDebugWorldStateEquipmentMutation();
				state.HostEquipmentMutationApplied = state.EquipmentMutationChangedMagazines > 0;
				state.Log << "world_state_host_equipment_mutation=" << (state.HostEquipmentMutationApplied ? "passed" : "retry")
				          << " changed_magazines=" << state.EquipmentMutationChangedMagazines << " at_update=" << state.SimulationUpdates << '\n' << std::flush;
				if (!state.HostEquipmentMutationApplied) {
					state.EquipmentMutationScheduledUpdate = state.SimulationUpdates + 60;
				}
			}
		}
		if (state.SimulationUpdates == 1) {
			state.InitialActorCount = g_MovableMan.GetActorCount();
			state.InitialParticleCount = g_MovableMan.GetParticleCount();
		}
		if (state.SimulationUpdates == 1 || state.SimulationUpdates == state.UpdateLimit / 2) {
			SpawnDebugStressBatch();
		}
		if (!state.HasProtocolSmokeSnapshot) {
			const auto hasActor = [](const WorldStateProtocol::Snapshot& snapshot) {
				return std::any_of(snapshot.Objects.begin(), snapshot.Objects.end(), [](const auto& object) { return object.Flags == WorldStateProtocol::c_ObjectFlagActor; });
			};
			if (worldStateClient && worldStateClient->HasSnapshot() && hasActor(worldStateClient->GetLatestSnapshot())) {
				state.ProtocolSmokeSnapshot = worldStateClient->GetLatestSnapshot();
				state.HasProtocolSmokeSnapshot = true;
			} else if (g_MovableMan.GetMovableObjectCount() > 0) {
				state.ProtocolSmokeSnapshot = CaptureDebugWorldStateSnapshot();
				state.HasProtocolSmokeSnapshot = hasActor(state.ProtocolSmokeSnapshot);
			}
		}
		if (state.RequestHostWorldStateTransition && worldStateServerRequested && worldStateServer &&
		    worldStateServer->GetConnectedClientCount() > 0 && !state.HostWorldStateTransitionQueued &&
		    state.SimulationUpdates >= state.HostWorldStateTransitionUpdate) {
			WorldStateProtocol::Snapshot targetActivity;
			targetActivity.ActivityClassName = "GAScripted";
			targetActivity.ActivityPreset = "Skirmish Defense";
			targetActivity.ActivityModuleName = "Base.rte";
			targetActivity.SceneModuleName = "Base.rte";
			targetActivity.ScenePreset = "Ketanot Hills";
			std::string failureReason;
			state.HostWorldStateTransitionQueued = QueueWorldStateActivity(targetActivity, failureReason);
			state.Log << "debug_host_activity_transition=" << (state.HostWorldStateTransitionQueued ? "queued" : "failed")
			          << " at_update=" << state.SimulationUpdates
			          << (failureReason.empty() ? "" : " reason=" + failureReason) << '\n' << std::flush;
		}
		const Activity* currentActivity = g_ActivityMan.GetActivity();
		const Scene* currentScene = g_SceneMan.GetScene();
		const bool targetWorldLoaded = g_ActivityMan.IsInActivity() && currentActivity && currentScene &&
		                               currentActivity->GetPresetName() == state.ExpectedTransitionActivity &&
		                               currentScene->GetPresetName() == state.ExpectedTransitionScene;
		if (targetWorldLoaded && state.RequestHostWorldStateTransition && state.HostWorldStateTransitionQueued &&
		    !state.HostWorldStateTransitionApplied) {
			state.HostWorldStateTransitionApplied = true;
			state.HostWorldStateTransitionAppliedUpdate = state.SimulationUpdates;
			const std::vector<int> assignedPlayerSlots = worldStateServer ? worldStateServer->GetAssignedPlayerSlots() : std::vector<int>{};
			state.HostActivityRosterPassed = !assignedPlayerSlots.empty();
			for (const int playerSlot : assignedPlayerSlots) {
				if (playerSlot < Players::PlayerOne || playerSlot >= Players::MaxPlayerCount || !currentActivity->PlayerActive(playerSlot) ||
				    !currentActivity->PlayerHuman(playerSlot)) {
					state.HostActivityRosterPassed = false;
				}
			}
			state.Log << "world_state_host_player_roster=" << (state.HostActivityRosterPassed ? "passed" : "failed")
			          << " assigned_slots=" << assignedPlayerSlots.size() << " human_players=" << static_cast<int>(currentActivity->GetHumanCount()) << '\n' << std::flush;
			state.Log << "debug_host_activity_transition=applied at_update=" << state.SimulationUpdates << '\n' << std::flush;
		}
		if (targetWorldLoaded && state.RequireWorldStateTransition && state.WorldStateTransitionObserved && !state.WorldStateTransitionApplied) {
			state.WorldStateTransitionApplied = true;
			state.WorldStateTransitionAppliedUpdate = state.SimulationUpdates;
			state.Log << "debug_client_activity_transition=applied at_update=" << state.SimulationUpdates << '\n' << std::flush;
		}
		constexpr int c_TransitionSettleUpdates = 180;
		const bool hostTransitionRestarted = state.RequestHostWorldStateTransition && state.HostWorldStateTransitionApplied &&
		                                     state.SimulationUpdates >= state.HostWorldStateTransitionAppliedUpdate + c_TransitionSettleUpdates;
		const bool clientTransitionRestarted = state.RequireWorldStateTransition && state.WorldStateTransitionApplied &&
		                                      state.SimulationUpdates >= state.WorldStateTransitionAppliedUpdate + c_TransitionSettleUpdates;
		if (!state.PostTransitionStressBatchSpawned && (hostTransitionRestarted || clientTransitionRestarted)) {
			state.PostTransitionStressBatchSpawned = true;
			SpawnDebugStressBatch();
		}
		if (state.SimulationUpdates % 60 == 0) {
			state.Log << "simulation_update=" << state.SimulationUpdates << " actors=" << g_MovableMan.GetActorCount()
			          << " particles=" << g_MovableMan.GetParticleCount() << '\n' << std::flush;
		}
		if (!state.PerformanceCountersObserved && g_MovableMan.GetActorCount() > 0 && MovableObject::GetSceneSpawnEvents() > 0) {
			state.PerformanceCountersObserved = true;
			state.Log << "performance_counters_observed=passed at_update=" << state.SimulationUpdates
			          << " actors=" << g_MovableMan.GetActorCount() << " scene_spawns=" << MovableObject::GetSceneSpawnEvents() << '\n' << std::flush;
		}
		if (state.PauseSimulationAfterClient && worldStateServer && worldStateServer->GetConnectedClientCount() > 0) {
			if (!state.SawClientForPauseTest) {
				state.SawClientForPauseTest = true;
				state.PauseSimulationAfterClientUpdate = state.SimulationUpdates + 30;
				state.Log << "pause_after_client_scheduled=true at_update=" << state.PauseSimulationAfterClientUpdate << '\n' << std::flush;
			}
			if (!state.SimulationPausedByDebugRun && state.SimulationUpdates >= state.PauseSimulationAfterClientUpdate) {
				g_TimerMan.PauseSim(true);
				state.SimulationPausedByDebugRun = true;
				state.Log << "simulation_paused_after_client=true at_update=" << state.SimulationUpdates << '\n' << std::flush;
			}
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
		std::size_t receivedSoundCount = 0;
		WorldStateTransport::SnapshotTransmissionMetrics snapshotTransmissionMetrics;
		std::uint16_t boundPort = 0;
		{
			WorldStateTransport serverTransport;
			std::array<WorldStateClientSession, expectedClientCount> clients;
			std::array<bool, expectedClientCount> clientReceivedSnapshot{};
			std::array<bool, expectedClientCount> clientReceivedTerrainPatch{};
			std::array<bool, expectedClientCount> clientReceivedSound{};
			WorldStateSoundProtocol::Event sound;
			sound.Type = WorldStateSoundProtocol::Action::Play;
			sound.Container = 1; sound.Voice = 2;
			sound.Module = "Base.rte"; sound.Resource = "Sounds/UI/Confirm.flac";
			const std::vector<WorldStateSoundProtocol::Event> expectedSounds{sound};
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
					bool soundBroadcastSent = false;
					const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
					while (std::chrono::steady_clock::now() < deadline &&
					       (receivedClientCount < expectedClientCount || receivedTerrainPatchCount < expectedClientCount || receivedSoundCount < expectedClientCount)) {
						std::vector<WorldStateTransport::ReceivedPacket> serverPackets;
						serverTransport.Poll(serverPackets);
						serverPacketCount += serverPackets.size();
						for (const WorldStateTransport::ReceivedPacket& received : serverPackets) {
							if (received.Identifier == ID_NEW_INCOMING_CONNECTION) {
								++acceptedClientCount;
							}
						}
						if (!broadcastSent && acceptedClientCount == expectedClientCount) {
							broadcastSent = serverTransport.BroadcastSnapshot(expectedSnapshot, expectedSnapshot.Tick, &snapshotTransmissionMetrics);
							sendCount += broadcastSent ? expectedClientCount : 0;
						}
						if (broadcastSent && !terrainBroadcastSent) {
							terrainBroadcastSent = serverTransport.BroadcastTerrainPatch(expectedTerrainPatch, expectedSnapshot.Tick + 1);
							sendCount += terrainBroadcastSent ? expectedClientCount : 0;
						}
						if (broadcastSent && !soundBroadcastSent) {
							soundBroadcastSent = serverTransport.BroadcastSoundEvents(expectedSnapshot.SceneRevision, expectedSnapshot.Tick + 2, expectedSounds) &&
							    serverTransport.BroadcastSoundEvents(expectedSnapshot.SceneRevision, expectedSnapshot.Tick + 2, expectedSounds);
						}
						for (std::size_t clientIndex = 0; clientIndex < clients.size(); ++clientIndex) {
							WorldStateClientSession& client = clients[clientIndex];
							client.Update();
							if (!clientReceivedSound[clientIndex]) {
								if (!client.DrainSoundEvents(0).empty()) { soundBroadcastSent = false; break; }
								const auto events = client.DrainSoundEvents(expectedSnapshot.SceneRevision);
								if (events == expectedSounds && client.DrainSoundEvents(expectedSnapshot.SceneRevision).empty()) {
									clientReceivedSound[clientIndex] = true; ++receivedSoundCount;
								}
							}
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
					loopbackPassed = broadcastSent && terrainBroadcastSent && soundBroadcastSent && receivedSoundCount == expectedClientCount &&
					    receivedClientCount == expectedClientCount && receivedTerrainPatchCount == expectedClientCount;
				}
			}
		}
		log << "world_state_transport_setup=server:" << serverStarted << ",port:" << boundPort
		    << ",clients_started:" << clientsStarted << ",clients_accepted:" << acceptedClientCount << ",clients_received:" << receivedClientCount << ",server_packets:" << serverPacketCount
		    << ",client_packets:" << clientPacketCount << ",terrain_received:" << receivedTerrainPatchCount << ",sends:" << sendCount
		    << ",compressed_snapshot_payload_bytes:" << snapshotTransmissionMetrics.PayloadBytes << ",estimated_snapshot_payload_bandwidth_kbit_s_per_client:" << (snapshotTransmissionMetrics.PayloadBytes * 20.0 * 8.0 / 1000.0)
		    << ",sound_clients_received:" << receivedSoundCount
		    << ",snapshot_encode_us:" << snapshotTransmissionMetrics.EncodeMicroseconds << ",snapshot_compress_us:" << snapshotTransmissionMetrics.CompressMicroseconds
		    << ",snapshot_queue_us:" << snapshotTransmissionMetrics.QueueMicroseconds
		    << ",loopback:" << (loopbackPassed ? "passed" : "failed") << '\n' << std::flush;
		return loopbackPassed;
	}

	bool VerifyWorldStateHostSessionLoopback(WorldStateServerSession& serverSession, std::ofstream& log, bool requireLiveWorld = false) {
		constexpr std::size_t expectedClientCount = 4;
		const unsigned short port = serverSession.GetBoundPort();
		serverSession.Update(++worldStateServerSimulationTick);
		const bool idleServerSkippedSnapshot = serverSession.GetSnapshotBroadcastCount() == 0;
		serverSession.EnableDebugEmptyWorldSnapshotSmoke();
		std::array<WorldStateClientSession, expectedClientCount> clients;
		bool started = port != 0;
		for (WorldStateClientSession& client : clients) {
			started = started && client.Connect("127.0.0.1", port);
		}
		if (!started) {
			serverSession.DisableDebugEmptyWorldSnapshotSmoke();
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
		const bool liveWorldReceived = std::all_of(clients.begin(), clients.end(), [](const WorldStateClientSession& client) {
			return client.HasSnapshot() && !client.GetLatestSnapshot().Objects.empty() && !client.GetLatestSnapshot().ScenePreset.empty();
		});
		std::transform(clients.begin(), clients.end(), assignedSlots.begin(), [](const WorldStateClientSession& client) {
			return client.GetAssignedPlayerSlot();
		});
		std::sort(assignedSlots.begin(), assignedSlots.end());
		const bool playerAssignmentsPassed = assignedSlots == std::array<int, expectedClientCount>{Players::NoPlayer, Players::PlayerTwo, Players::PlayerThree, Players::PlayerFour};
		const std::size_t clientsReceivedSnapshotCount = std::count_if(clients.begin(), clients.end(), [](const WorldStateClientSession& client) {
			return client.GetReceivedSnapshotCount() > 0;
		});
		bool networkInputApplied = false;
		bool networkInputFreshness = false;
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
			if (networkInputApplied) {
				auto sendAndApply = [&](bool fire) {
					++inputCommand.ClientTick;
					inputCommand.HeldElements = fire ? std::uint64_t{1} << InputElements::INPUT_FIRE : 0;
					inputCommand.MouseButtonsHeld = fire ? static_cast<std::uint8_t>(1U << MouseButtons::MOUSE_LEFT) : 0;
					const auto before = serverSession.GetInputCommandCount();
					if (!clients.front().SendInputCommand(inputCommand)) return false;
					const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
					while (serverSession.GetInputCommandCount() == before && std::chrono::steady_clock::now() < deadline) {
						serverSession.Update(++worldStateServerSimulationTick);
						for (auto& peer : clients) peer.Update();
						std::this_thread::sleep_for(std::chrono::milliseconds(5));
					}
					return serverSession.GetInputCommandCount() > before;
				};
				const bool heldApplied = sendAndApply(true) && g_UInputMan.ElementHeld(networkInputPlayer, InputElements::INPUT_FIRE);
				const auto quietBegin = std::chrono::steady_clock::now();
				while (heldApplied && g_UInputMan.ElementHeld(networkInputPlayer, InputElements::INPUT_FIRE) &&
				       std::chrono::steady_clock::now() - quietBegin < std::chrono::seconds(3)) {
					serverSession.Update(++worldStateServerSimulationTick);
					for (auto& peer : clients) peer.Update();
					std::this_thread::sleep_for(std::chrono::milliseconds(5));
				}
				const bool neutralized = heldApplied && g_UInputMan.IsNetworkInputActive(networkInputPlayer) &&
				    !g_UInputMan.ElementHeld(networkInputPlayer, InputElements::INPUT_FIRE) &&
				    !g_UInputMan.MouseButtonHeld(MouseButtons::MOUSE_LEFT, networkInputPlayer) &&
				    std::chrono::steady_clock::now() - quietBegin >= std::chrono::milliseconds(900);
				const bool recovered = sendAndApply(true) && g_UInputMan.ElementHeld(networkInputPlayer, InputElements::INPUT_FIRE);
				const bool released = sendAndApply(false) && !g_UInputMan.ElementHeld(networkInputPlayer, InputElements::INPUT_FIRE);
				networkInputFreshness = neutralized && recovered && released && serverSession.GetConnectedClientCount() == expectedClientCount;
				log << "world_state_input_freshness=" << (networkInputFreshness ? "passed" : "failed") << '\n' << std::flush;
			}
		}
		for (WorldStateClientSession& client : clients) {
			client.Disconnect();
		}
		const auto disconnectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (serverSession.GetConnectedClientCount() > 0 && std::chrono::steady_clock::now() < disconnectDeadline) {
			serverSession.Update(++worldStateServerSimulationTick);
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		serverSession.DisableDebugEmptyWorldSnapshotSmoke();
		const bool remainedAvailableAfterDisconnect = serverSession.IsStarted() && serverSession.GetConnectedClientCount() == 0;
		const bool passed = idleServerSkippedSnapshot && allConnectedAndReceived && playerAssignmentsPassed && networkInputApplied && networkInputFreshness && remainedAvailableAfterDisconnect && (!requireLiveWorld || liveWorldReceived);
		log << "world_state_host_session_smoke=" << (passed ? "passed" : "failed")
		    << " clients_accepted=" << expectedClientCount
		    << " clients_received=" << clientsReceivedSnapshotCount
		    << " assignments=" << playerAssignmentsPassed
		    << " live_world_received=" << liveWorldReceived
		    << " idle_snapshot_count=" << (idleServerSkippedSnapshot ? 0 : serverSession.GetSnapshotBroadcastCount())
		    << " input_applied=" << networkInputApplied << " input_slot=" << networkInputPlayer
		    << " server_alive_after_disconnect=" << remainedAvailableAfterDisconnect << '\n' << std::flush;
		return passed;
	}

	bool VerifyWorldStateClientFailureLoopback(const WorldStateProtocol::Snapshot& snapshot, std::ofstream& log) {
		WorldStateTransport server;
		WorldStateClientSession client, overflow;
		bool ready = server.StartServer(0, 1, "127.0.0.1");
		auto receive = [&]() {
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
			while (!(client.IsConnected() && client.HasSnapshot()) && client.GetConnectionError().empty() && std::chrono::steady_clock::now() < deadline) {
				std::vector<WorldStateTransport::ReceivedPacket> packets;
				server.Poll(packets);
				server.BroadcastSnapshot(snapshot, 1);
				client.Update();
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
			log << "client_failure_receive connected=" << client.IsConnected() << " snapshot=" << client.HasSnapshot()
			    << " count=" << client.GetReceivedSnapshotCount() << " port=" << server.GetBoundPort() << " error=" << client.GetConnectionError() << '\n' << std::flush;
			return client.IsConnected() && client.HasSnapshot();
		};
		ready = ready && client.Connect("127.0.0.1", server.GetBoundPort()) && receive();
		bool refused = false, disconnected = false, reset = false, reconnected = false;
		if (ready && overflow.Connect("127.0.0.1", server.GetBoundPort())) {
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
			while (overflow.GetConnectionError().empty() && std::chrono::steady_clock::now() < deadline) {
				std::vector<WorldStateTransport::ReceivedPacket> packets;
				server.Poll(packets); overflow.Update();
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
			refused = overflow.GetConnectionError() == "The server is full." && !overflow.IsConnected() && !overflow.HasSnapshot();
		}
		server.Stop();
		if (ready) {
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
			while (client.GetConnectionError().empty() && std::chrono::steady_clock::now() < deadline) {
				client.Update(); std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
			disconnected = !client.IsConnected() && !client.GetConnectionError().empty() && client.HasSnapshot();
			if (server.StartServer(0, 1, "127.0.0.1") && client.Connect("127.0.0.1", server.GetBoundPort())) {
				reset = client.GetConnectionError().empty() && !client.HasSnapshot() && !client.HasPlayerAssignment() && client.GetReceivedSnapshotCount() == 0;
				reconnected = reset && receive();
			}
		}
		bool versionRefused = reconnected, compressedVersionRefused = reconnected, versionRetry = false;
		for (int trial = 0; trial < 2 && reconnected; ++trial) {
			if (trial != 0) {
				server.Stop();
				const bool started = server.StartServer(0, 1, "127.0.0.1");
				const unsigned short port = server.GetBoundPort();
				const bool connected = started && client.Connect("127.0.0.1", port);
				const bool received = connected && receive();
				log << "client_failure_version_reconnect trial=" << trial << " started=" << started << " port=" << port
				    << " connect_started=" << connected << " received=" << received << " connected=" << client.IsConnected()
				    << " snapshot=" << client.HasSnapshot() << " error=" << client.GetConnectionError() << '\n' << std::flush;
				if (!received) {
					compressedVersionRefused = false;
					break;
				}
			}
			std::vector<std::uint8_t> foreign, wire;
			const std::uint16_t version = WorldStateProtocol::c_Version + (trial == 0 ? -1 : 1);
			bool encoded = WorldStateProtocol::EncodeSnapshot(snapshot, 2, foreign);
			if (encoded) { foreign[4] = static_cast<std::uint8_t>(version); foreign[5] = static_cast<std::uint8_t>(version >> 8); }
			bool compressed = false;
			if (trial == 1) encoded = encoded && WorldStateCompression::EncodeForWire(foreign, wire, compressed) && compressed;
			else wire = foreign;
			const auto count = client.GetReceivedSnapshotCount();
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
			while (encoded && client.GetConnectionError().empty() && std::chrono::steady_clock::now() < deadline) {
				std::vector<WorldStateTransport::ReceivedPacket> packets;
				server.Poll(packets); server.BroadcastWorldState(wire); client.Update();
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
			const bool rejected = encoded && !client.IsConnected() && client.HasSnapshot() &&
				client.GetReceivedSnapshotCount() == count &&
				client.GetConnectionError().find("Incompatible world-state protocol: server " + std::to_string(version)) == 0;
			log << "client_failure_version_trial=" << trial << " encoded=" << encoded << " compressed=" << compressed
			    << " rejected=" << rejected << " snapshots_before=" << count << " snapshots_after=" << client.GetReceivedSnapshotCount()
			    << " error=" << client.GetConnectionError() << '\n' << std::flush;
			(trial == 0 ? versionRefused : compressedVersionRefused) = rejected;
		}
		if (versionRefused && compressedVersionRefused) {
			server.Stop();
			versionRetry = server.StartServer(0, 1, "127.0.0.1") && client.Connect("127.0.0.1", server.GetBoundPort()) &&
				client.GetConnectionError().empty() && !client.HasSnapshot() && receive();
		}
		const bool passed = ready && refused && disconnected && reset && reconnected && versionRefused && compressedVersionRefused && versionRetry;
		log << "world_state_client_failure_smoke=" << (passed ? "passed" : "failed")
			<< " capacity_refused=" << refused << " disconnected_frozen=" << disconnected
			<< " reconnect_reset=" << reset << " reconnected=" << reconnected
			<< " version_refused=" << versionRefused << " compressed_version_refused=" << compressedVersionRefused
			<< " version_retry=" << versionRetry << '\n' << std::flush;
		return passed;
	}

	bool VerifyHeadlessSoundLengths(std::ofstream& log) {
		using Type = SoundContainer::LengthOfSoundType;
		const auto path = GetDebugRunState().OutputDirectory / UTF8::PathFromString("SoundLength_Ёжик.wav");
		std::vector<std::uint8_t> wave;
		auto id = [&](const char* text) { wave.insert(wave.end(), text, text + 4); };
		auto number = [&](std::uint64_t value, std::size_t bytes) { WorldStateProtocol::Detail::WriteUnsigned(wave, value, bytes); };
		id("RIFF"); number(1636, 4); id("WAVE"); id("fmt "); number(16, 4);
		number(1, 2); number(1, 2); number(8000, 4); number(16000, 4); number(2, 2); number(16, 2);
		id("data"); number(1600, 4); wave.resize(1644, 0); // 800 PCM frames = 100 ms.
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file.write(reinterpret_cast<const char*>(wave.data()), static_cast<std::streamsize>(wave.size())); file.close();
		SoundSet sounds;
		sounds.AddSound(UTF8::PathToString(path), Vector(), 0, 0, false);
		SoundContainer container; container.SetTopLevelSoundSet(sounds);
		SoundContainer empty;
		const bool emptySafe = empty.GetLength(Type::Any) == 0 && empty.GetLength(Type::NextPlayed) == 0;
		const float any = container.GetLength(Type::Any), next = container.GetLength(Type::NextPlayed);
		std::array<float, 2> parallel{};
		std::thread first([&] { parallel[0] = container.GetLength(Type::Any); });
		std::thread second([&] { parallel[1] = container.GetLength(Type::Any); });
		first.join(); second.join();
		auto alternativePath = path; alternativePath.replace_extension(".flac");
		SoundSet alternativeSounds; alternativeSounds.AddSound(UTF8::PathToString(alternativePath), Vector(), 0, 0, false);
		SoundContainer alternative; alternative.SetTopLevelSoundSet(alternativeSounds);
		const float alternativeLength = alternative.GetLength(Type::Any);
		SoundSet missingSounds; missingSounds.AddSound(UTF8::PathToString(path) + ".missing", Vector(), 0, 0, false);
		SoundContainer missing; missing.SetTopLevelSoundSet(missingSounds);
		const float missingLength = missing.GetLength(Type::Any);
		const Entity* preset = g_PresetMan.GetEntityPreset("SoundContainer", "Confirm", "Base.rte");
		std::unique_ptr<SoundContainer> confirm(preset ? dynamic_cast<SoundContainer*>(preset->Clone()) : nullptr);
		const float flacLength = confirm ? confirm->GetLength(Type::Any) : 0;
		const bool soundless = !g_AudioMan.IsAudioEnabled() && !container.IsBeingPlayed();
		GetWorldStateSoundCapture().Begin();
		bool playbackPassed = TestSoundPlaybackTimeline() && confirm && confirm->Play() && confirm->IsBeingPlayed();
		if (playbackPassed) playbackPassed = confirm->SeekNetworkPlayback(100) && confirm->IsBeingPlayed();
		if (playbackPassed) {
			confirm->SetSoundOverlapMode(SoundContainer::IGNORE_PLAY);
			playbackPassed = !confirm->Play();
			confirm->SetSoundOverlapMode(SoundContainer::RESTART);
			playbackPassed = confirm->Play() && confirm->IsBeingPlayed() && playbackPassed;
			confirm->SetPaused(true); confirm->SetPitch(2);
			confirm->SetPosition(Vector(12, 4)); confirm->SetVolume(0.5F);
			playbackPassed = confirm->IsPaused() && confirm->IsBeingPlayed() && playbackPassed;
			confirm->FadeOut(0);
			playbackPassed = !confirm->IsBeingPlayed() && playbackPassed;
			confirm->SetPaused(false); confirm->SetSoundOverlapMode(SoundContainer::OVERLAP); confirm->SetLoopSetting(-1);
			for (int index = 0; index < c_MaxPlayingSoundsPerContainer; ++index) playbackPassed = confirm->Play() && playbackPassed;
			playbackPassed = !confirm->Play() && confirm->IsBeingPlayed() && confirm->GetPlayingChannels()->empty() && playbackPassed;
			g_AudioMan.SetGlobalPitch(0.5F, true, true); g_AudioMan.PauseIngameSounds(true); g_AudioMan.FinishIngameLoopingSounds();
			g_AudioMan.PauseIngameSounds(false); g_AudioMan.StopAll(); g_AudioMan.SetGlobalPitch(1, true, true);
			playbackPassed = !confirm->IsBeingPlayed() && confirm->Play() && playbackPassed;
			SoundContainer copy(*confirm);
			playbackPassed = !copy.IsBeingPlayed() && confirm->Stop() && !confirm->IsBeingPlayed() && playbackPassed;
			playbackPassed = confirm->Play() && confirm->Restart() && playbackPassed;
			confirm->Reset(); playbackPassed = !confirm->IsBeingPlayed() && playbackPassed;
		}
		bool captureOverflow = false;
		std::vector<WorldStateSoundProtocol::Event> captured;
		for (;;) {
			auto batch = GetWorldStateSoundCapture().Drain(captureOverflow);
			if (batch.empty()) break;
			for (auto& event : batch) captured.push_back(std::move(event));
		}
		GetWorldStateSoundCapture().End();
		std::array<std::size_t, 12> actionCounts{};
		bool capturePassed = !captureOverflow;
		for (const auto& event : captured) {
			++actionCounts[static_cast<std::size_t>(event.Type)];
			if (event.Type == WorldStateSoundProtocol::Action::Play)
				capturePassed = capturePassed && event.Module == "Base.rte" && !event.Resource.empty() && event.Container && event.Voice;
		}
		for (auto action : {WorldStateSoundProtocol::Action::Play, WorldStateSoundProtocol::Action::Stop,
		    WorldStateSoundProtocol::Action::Volume, WorldStateSoundProtocol::Action::Pitch,
		    WorldStateSoundProtocol::Action::Pause, WorldStateSoundProtocol::Action::Fade, WorldStateSoundProtocol::Action::FinishBusLoops})
			capturePassed = capturePassed && actionCounts[static_cast<std::size_t>(action)] > 0;
		WorldStateSoundPlayback replicaSound;
		WorldStateSoundProtocol::Event replicaPlay;
		replicaPlay.Type = WorldStateSoundProtocol::Action::Play; replicaPlay.Container = 123; replicaPlay.Voice = 456;
		replicaPlay.Module = "Base.rte"; replicaPlay.Resource = "Sounds/GUIs/SlicePicked.flac";
		replicaPlay.Immobile = true; replicaPlay.Loops = -1; replicaPlay.OffsetX = 3; replicaPlay.OffsetY = -4;
		bool replicaPassed = replicaSound.Apply(1, 0, std::vector{replicaPlay}) == 1 && replicaSound.HasVoice(456);
		WorldStateSoundProtocol::Event uiPlay = replicaPlay; uiPlay.Container = 124; uiPlay.Voice = 457; uiPlay.Bus = 1; uiPlay.Rate = 2;
		replicaPassed = replicaSound.Apply(1, 0, std::vector{uiPlay}) == 1 && replicaPassed;
		WorldStateSoundProtocol::Event groupPitch; groupPitch.Type = WorldStateSoundProtocol::Action::GlobalPitch; groupPitch.Bus = 1; groupPitch.Rate = 0.5F;
		replicaSound.Apply(1, 0, std::vector{groupPitch});
		replicaPassed = replicaSound.GetVoicePitch(456) == 0.5F && replicaSound.GetVoicePitch(457) == 2 && replicaPassed;
		WorldStateSoundProtocol::Event groupPause; groupPause.Type = WorldStateSoundProtocol::Action::BusPause; groupPause.Paused = true;
		replicaSound.Apply(1, 0, std::vector{groupPause});
		replicaPassed = replicaSound.IsVoicePaused(456) && !replicaSound.IsVoicePaused(457) && replicaPassed;
		WorldStateSoundProtocol::Event explicitPause; explicitPause.Type = WorldStateSoundProtocol::Action::Pause; explicitPause.Container = 123; explicitPause.Paused = true;
		replicaSound.Apply(1, 0, std::vector{explicitPause}); groupPause.Paused = false;
		replicaSound.Apply(1, 0, std::vector{groupPause}); replicaPassed = replicaSound.IsVoicePaused(456) && replicaPassed;
		explicitPause.Paused = false; replicaSound.Apply(1, 0, std::vector{explicitPause});
		replicaPassed = !replicaSound.IsVoicePaused(456) && replicaPassed;
		WorldStateSoundProtocol::Event finishBus; finishBus.Type = WorldStateSoundProtocol::Action::FinishBusLoops; finishBus.Bus = 0;
		replicaPassed = replicaSound.Apply(1, 0, std::vector{finishBus}) == 1 && replicaSound.HasVoice(456) &&
		    replicaSound.IsVoiceFinishing(456) && !replicaSound.IsVoiceFinishing(457) && replicaPassed;
		WorldStateSoundProtocol::Event uiStop; uiStop.Container = 124; replicaSound.Apply(1, 0, std::vector{uiStop});
		WorldStateSoundProtocol::Event replicaStop; replicaStop.Container = 123;
		replicaPassed = replicaSound.Apply(1, 0, std::vector{replicaStop}) == 1 && replicaSound.Size() == 0 && replicaPassed;
		replicaPassed = replicaSound.Apply(1, 0, std::vector{replicaPlay}) == 1 && replicaPassed;
		replicaSound.Apply(2, 0, {}); replicaPassed = replicaSound.Size() == 0 && replicaPassed;
		replicaPlay.Player = 3; replicaPassed = replicaSound.Apply(2, 0, std::vector{replicaPlay}) == 0 && replicaPassed;
		replicaPlay.Player = -1; replicaPlay.Resource = "../Sounds.ini";
		replicaPassed = replicaSound.Apply(2, 0, std::vector{replicaPlay}) == 0 && replicaPassed;
		SoundContainer replicatedPitch; replicatedPitch.SetNetworkPitch(16);
		replicaPassed = replicatedPitch.GetPitch() == 16 && replicaPassed;
		replicatedPitch.SetPitch(16); replicaPassed = replicatedPitch.GetPitch() == 8 && replicaPassed;
		replicaSound.Clear();
		WorldStateSoundPlayback fadingReplica;
		auto fadedPlay = uiPlay; fadedPlay.Container = 125; fadedPlay.Voice = 458; fadedPlay.Bus = 0;
		fadedPlay.Paused = true; fadedPlay.FadeMilliseconds = 100; fadedPlay.FadeGain = 0.5F; fadedPlay.Gain = 0.8F;
		fadingReplica.Apply(1, 0, std::vector{fadedPlay}, 0);
		replicaPassed = std::abs(fadingReplica.GetVoiceVolume(458) - 0.4F) < 0.001F && replicaPassed;
		fadingReplica.Apply(1, 0, {}, 50);
		replicaPassed = std::abs(fadingReplica.GetVoiceVolume(458) - 0.2F) < 0.001F && replicaPassed;
		groupPause.Paused = true; fadingReplica.Apply(1, 0, std::vector{groupPause}, 50);
		WorldStateSoundProtocol::Event fadeVolume; fadeVolume.Type = WorldStateSoundProtocol::Action::Volume; fadeVolume.Container = 125; fadeVolume.Gain = 0.4F;
		fadingReplica.Apply(1, 0, std::vector{fadeVolume}, 500);
		replicaPassed = std::abs(fadingReplica.GetVoiceVolume(458) - 0.1F) < 0.001F && replicaPassed;
		groupPause.Paused = false; fadingReplica.Apply(1, 0, std::vector{groupPause}, 500);
		fadingReplica.Apply(1, 0, {}, 550); replicaPassed = !fadingReplica.HasVoice(458) && replicaPassed;
		fadingReplica.Clear();
		capturePassed = replicaPassed && capturePassed;
		log << "headless_sound_replica=" << (replicaPassed ? "passed" : "failed") << '\n';
		log << "headless_sound_capture=" << (capturePassed ? "passed" : "failed") << " events=" << captured.size() << '\n';
		std::error_code error; const bool removed = std::filesystem::remove(path, error) && !error;
		const bool passed = capturePassed && playbackPassed && file && emptySafe && soundless && any == 100 && next == 100 && parallel[0] == 100 && parallel[1] == 100 &&
		    alternativeLength == 100 && missingLength == 0 && flacLength > 0 && std::isfinite(flacLength) && removed;
		log << "headless_sound_length_smoke=" << (passed ? "passed" : "failed") << " wav_ms=" << any << " next_ms=" << next
		    << " alternative_ms=" << alternativeLength << " missing_ms=" << missingLength << " flac_ms=" << flacLength
		    << " empty_safe=" << emptySafe << " soundless=" << soundless << " playback=" << playbackPassed << " cleanup=" << removed << '\n' << std::flush;
		return passed;
	}

	void FinishDebugRun(bool success) {
		DebugRunState& state = GetDebugRunState();
		if (!state.Enabled || System::IsSetToQuit()) {
			return;
		}
		// Replica smoke checks below can clone script-bearing objects. Finish the
		// update's async Lua GC and MOID work before those native clone operations.
		g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
		g_ThreadMan.GetBackgroundThreadPool().wait_for_tasks();
		const WorldStateProtocol::Snapshot worldSnapshot = state.HasProtocolSmokeSnapshot ? state.ProtocolSmokeSnapshot : CaptureDebugWorldStateSnapshot();
		const WorldStateProtocol::Snapshot repeatedWorldSnapshot = state.HasProtocolSmokeSnapshot ? state.ProtocolSmokeSnapshot : CaptureDebugWorldStateSnapshot();
		const bool clientFailurePassed = VerifyWorldStateClientFailureLoopback(worldSnapshot, state.Log);
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
		std::uint32_t replayCallbacks = 0;
		const bool objectCallbacksPassed = WorldStateClientReplica::TestObjectScriptCallbacks(state.OutputDirectory, replayCallbacks);
		state.Log << "world_state_object_callback_smoke=" << (objectCallbacksPassed ? "passed" : "failed") << " callbacks=" << replayCallbacks << '\n' << std::flush;
		const bool backgroundEquipmentPassed = WorldStateClientReplica::TestBackgroundEquipment();
		const bool attachmentsPassed = WorldStateAttachments::TestNative();
		state.Log << "world_state_attachments_smoke=" << (attachmentsPassed ? "passed" : "failed") << '\n' << std::flush;
		state.Log << "world_state_background_equipment_smoke=" << (backgroundEquipmentPassed ? "passed" : "failed") << '\n' << std::flush;
		const WorldStateClientReplica::ApplyResult firstApply = replicaSmoke.Apply(worldSnapshot);
		Vector cameraProbe(320.0F, 240.0F);
		const auto cameraActor = std::find_if(worldSnapshot.Objects.begin(), worldSnapshot.Objects.end(), [](const auto& object) { return object.Flags == WorldStateProtocol::c_ObjectFlagActor; });
		const std::uint64_t cameraActorId = cameraActor == worldSnapshot.Objects.end() ? 0 : cameraActor->NetworkId;
		const bool cameraInitiallyResolved = replicaSmoke.AdjustCameraForInterpolation(cameraActorId, cameraProbe);
		Activity* viewActivity = g_ActivityMan.GetActivity();
		Actor* previousViewActor = viewActivity ? viewActivity->GetControlledActor(Players::PlayerOne) : nullptr;
		const int previousViewTeam = viewActivity ? viewActivity->GetTeamOfPlayer(Players::PlayerOne) : Activity::TeamOne;
		const bool viewBound = viewActivity && replicaSmoke.BindPlayerView(cameraActorId, Activity::TeamOne) &&
		    viewActivity->GetControlledActor(Players::PlayerOne) &&
		    viewActivity->GetControlledActor(Players::PlayerOne)->IsPlayerControlled() &&
		    viewActivity->GetControlledActor(Players::PlayerOne)->GetController()->IsDisabled();
		WorldStateProtocol::Snapshot emptyReplicaSnapshot = worldSnapshot;
		emptyReplicaSnapshot.Objects.clear();
		const WorldStateClientReplica::ApplyResult removalApply = replicaSmoke.Apply(emptyReplicaSnapshot);
		const bool cameraRemoved = !replicaSmoke.AdjustCameraForInterpolation(cameraActorId, cameraProbe);
		const bool removedViewCleared = viewActivity && !viewActivity->GetControlledActor(Players::PlayerOne);
		const WorldStateClientReplica::ApplyResult restoredApply = replicaSmoke.Apply(worldSnapshot);
		const WorldStateClientReplica::ApplyResult repeatedApply = replicaSmoke.Apply(worldSnapshot);
		WorldStateProtocol::Snapshot interpolationTarget = worldSnapshot;
		interpolationTarget.Tick += 3;
		for (auto& object : interpolationTarget.Objects) if (object.NetworkId == cameraActorId) {
			object.PositionX += 30.0F;
			object.Rotation += c_TwoPI - 0.2F;
		}
		const WorldStateClientReplica::ApplyResult interpolationApply = replicaSmoke.Apply(interpolationTarget);
		const std::uint32_t interpolatedObjectCount = replicaSmoke.AdvanceInterpolation();
		cameraProbe = Vector(320.0F, 240.0F);
		const bool cameraInterpolationPassed = cameraInitiallyResolved && cameraRemoved &&
		    replicaSmoke.AdjustCameraForInterpolation(cameraActorId, cameraProbe) &&
		    std::abs(cameraProbe.GetX() - 300.0F) < 0.01F && std::abs(cameraProbe.GetY() - 240.0F) < 0.01F;
		state.Log << "world_state_camera_replica_smoke=" << (cameraInterpolationPassed ? "passed" : "failed")
		          << " lifecycle=" << (cameraInitiallyResolved && cameraRemoved) << " x=" << cameraProbe.GetX() << " y=" << cameraProbe.GetY() << '\n' << std::flush;
		const bool viewRebound = replicaSmoke.BindPlayerView(cameraActorId, Activity::TeamOne);
		replicaSmoke.Forget();
		const bool forgottenViewCleared = viewActivity && !viewActivity->GetControlledActor(Players::PlayerOne);
		const bool viewLifetimePassed = viewBound && removedViewCleared && viewRebound && forgottenViewCleared;
		state.Log << "world_state_player_view_smoke=" << (viewLifetimePassed ? "passed" : "failed")
		          << " bound=" << viewBound << " removal_cleared=" << removedViewCleared
		          << " forget_cleared=" << forgottenViewCleared << '\n' << std::flush;
		if (viewActivity) {
			viewActivity->SetReplicaControlledActor(previousViewActor, Players::PlayerOne, previousViewTeam);
			if (previousViewActor && g_MovableMan.IsActor(previousViewActor)) previousViewActor->GetController()->SetDisabled(false);
		}
		const auto isHumanObject = [](const WorldStateProtocol::ObjectState& object) { return object.ClassName == "AHuman"; };
		const std::uint32_t expectedHumanEquipmentStates = static_cast<std::uint32_t>(std::count_if(worldSnapshot.Objects.begin(), worldSnapshot.Objects.end(), isHumanObject));
		const bool interpolationSmokePassed = !interpolationTarget.Objects.empty() && interpolationApply.MissingPresets == 0 &&
		                                     interpolatedObjectCount == interpolationTarget.Objects.size() &&
		                                     interpolationApply.RotationInterpolationsScheduled == interpolationTarget.Objects.size() &&
		                                     std::abs(interpolationApply.MaxRotationInterpolationDeltaRadians - 0.2) < 0.01;
		const bool replicaSmokePassed = firstApply.Updated + firstApply.Spawned == worldSnapshot.Objects.size() && firstApply.MissingPresets == 0 &&
		                               firstApply.HumanEquipmentStatesApplied == expectedHumanEquipmentStates &&
		                               removalApply.Removed == firstApply.Spawned && removalApply.Updated == 0 && removalApply.Spawned == 0 &&
		                               restoredApply.Updated + restoredApply.Spawned == worldSnapshot.Objects.size() && restoredApply.MissingPresets == 0 &&
		                               restoredApply.HumanEquipmentStatesApplied == expectedHumanEquipmentStates &&
		                               repeatedApply.Updated == worldSnapshot.Objects.size() && repeatedApply.Spawned == 0 && repeatedApply.Removed == 0 && repeatedApply.MissingPresets == 0 &&
		                               repeatedApply.HumanEquipmentStatesApplied == expectedHumanEquipmentStates &&
		                               interpolationApply.Updated == worldSnapshot.Objects.size() && interpolationSmokePassed && cameraInterpolationPassed && viewLifetimePassed;
		state.Log << "world_state_replica_smoke=" << (replicaSmokePassed ? "passed" : "failed") << " objects=" << worldSnapshot.Objects.size()
		          << " first_updated=" << firstApply.Updated << " first_spawned=" << firstApply.Spawned << " removed=" << removalApply.Removed
		          << " repeat_updated=" << repeatedApply.Updated << " human_equipment=" << firstApply.HumanEquipmentStatesApplied << '/'
		          << expectedHumanEquipmentStates << " interpolated=" << interpolatedObjectCount << '\n' << std::flush;
		state.Log << "world_state_rotation_interpolation_smoke=" << (interpolationSmokePassed ? "passed" : "failed")
		          << " scheduled=" << interpolationApply.RotationInterpolationsScheduled
		          << " max_shortest_delta_rad=" << interpolationApply.MaxRotationInterpolationDeltaRadians << '\n' << std::flush;
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
		const auto sameHumanEquipment = [](const WorldStateProtocol::ObjectState& left, const WorldStateProtocol::ObjectState& right) {
			return left.HasEquippedDevice == right.HasEquippedDevice && left.EquippedDeviceClassName == right.EquippedDeviceClassName &&
			       left.EquippedDeviceModuleName == right.EquippedDeviceModuleName && left.EquippedDevicePresetName == right.EquippedDevicePresetName &&
			       left.HasEquippedDeviceMagazine == right.HasEquippedDeviceMagazine &&
			       left.EquippedDeviceMagazineClassName == right.EquippedDeviceMagazineClassName &&
			       left.EquippedDeviceMagazineModuleName == right.EquippedDeviceMagazineModuleName &&
			       left.EquippedDeviceMagazinePresetName == right.EquippedDeviceMagazinePresetName &&
			       left.EquippedDeviceMagazineRounds == right.EquippedDeviceMagazineRounds;
		};
		bool worldSnapshotPassed = WorldStateProtocol::EncodeSnapshot(worldSnapshot, state.SimulationUpdates, worldSnapshotPacket) &&
		                                 WorldStateProtocol::DecodeSnapshot(worldSnapshotPacket, decodedWorldSnapshot) &&
		                                 decodedWorldSnapshot.ActivityClassName == worldSnapshot.ActivityClassName &&
		                                 decodedWorldSnapshot.ActivityPreset == worldSnapshot.ActivityPreset &&
		                                 decodedWorldSnapshot.ActivityState == worldSnapshot.ActivityState &&
		                                 decodedWorldSnapshot.ActivityModuleName == worldSnapshot.ActivityModuleName &&
		                                 decodedWorldSnapshot.SceneModuleName == worldSnapshot.SceneModuleName &&
		                                 decodedWorldSnapshot.ScenePreset == worldSnapshot.ScenePreset &&
		                                 decodedWorldSnapshot.Players == worldSnapshot.Players &&
		                                 decodedWorldSnapshot.Objects.size() == worldSnapshot.Objects.size();
		for (std::size_t index = 0; worldSnapshotPassed && index < worldSnapshot.Objects.size(); ++index) {
			worldSnapshotPassed = sameHumanEquipment(worldSnapshot.Objects[index], decodedWorldSnapshot.Objects[index]);
		}
		std::vector<std::uint8_t> snapshotWirePacket;
		std::vector<std::uint8_t> decompressedWorldSnapshotPacket;
		std::span<const std::uint8_t> decompressedWorldSnapshotView;
		bool snapshotWasCompressed = false;
		WorldStateProtocol::Snapshot compressedRoundTripSnapshot;
		bool snapshotCompressionPassed = worldSnapshotPassed &&
		                                 WorldStateCompression::EncodeForWire(worldSnapshotPacket, snapshotWirePacket, snapshotWasCompressed) &&
		                                 WorldStateCompression::DecodeFromWire(snapshotWirePacket, decompressedWorldSnapshotPacket, decompressedWorldSnapshotView) &&
		                                 WorldStateProtocol::DecodeSnapshot(decompressedWorldSnapshotView, compressedRoundTripSnapshot) &&
		                                 compressedRoundTripSnapshot.Objects.size() == worldSnapshot.Objects.size() &&
		                                 compressedRoundTripSnapshot.ActivityPreset == worldSnapshot.ActivityPreset &&
		                                 compressedRoundTripSnapshot.ScenePreset == worldSnapshot.ScenePreset;
		for (std::size_t index = 0; snapshotCompressionPassed && index < worldSnapshot.Objects.size(); ++index) {
			snapshotCompressionPassed = sameHumanEquipment(worldSnapshot.Objects[index], compressedRoundTripSnapshot.Objects[index]);
		}
		state.Log << "world_snapshot_compression_smoke=" << (snapshotCompressionPassed ? "passed" : "failed")
		          << " compressed=" << (snapshotWasCompressed ? "yes" : "no") << " plain_bytes=" << worldSnapshotPacket.size()
		          << " wire_bytes=" << snapshotWirePacket.size() << '\n' << std::flush;
		const bool liveSnapshotTransportPassed = worldSnapshotPassed && snapshotCompressionPassed && VerifyWorldStateTransportLoopback(worldSnapshot, state.Log);
		state.Log << "captured_world_snapshot_objects=" << worldSnapshot.Objects.size()
		          << " bytes=" << worldSnapshotPacket.size()
		          << " result=" << (worldSnapshotPassed ? "passed" : "failed") << '\n' << std::flush;
		state.Log << "live_world_state_transport=" << (liveSnapshotTransportPassed ? "passed" : "failed") << '\n' << std::flush;
		const bool utf8GlyphRenderPassed = g_FrameMan.DidDebugUTF8GlyphProbePass();
		const bool worldStateTransitionPassed = (!state.RequireWorldStateTransition || state.WorldStateTransitionObserved) &&
		                                       (!state.RequestHostWorldStateTransition || state.HostWorldStateTransitionApplied) &&
		                                       (!state.RequestHostWorldStateTransition || state.HostActivityRosterPassed) &&
		                                       (!state.RequireWorldStateTransition || state.WorldStateTransitionApplied);
		state.Log << "world_state_activity_transition=" << (worldStateTransitionPassed ? "passed" : "failed")
		          << " observed=" << state.WorldStateTransitionObserved << " host_queued=" << state.HostWorldStateTransitionQueued
		          << " host_applied=" << state.HostWorldStateTransitionApplied << " client_applied=" << state.WorldStateTransitionApplied << '\n' << std::flush;
		const bool remoteTerrainMutationPassed = !state.RequireRemoteTerrainMutationSmoke || state.RemoteTerrainMutationObserved;
		if (state.RequireRemoteTerrainMutationSmoke) {
			state.Log << "world_state_remote_terrain_mutation=" << (remoteTerrainMutationPassed ? "passed" : "failed")
			          << " observed=" << state.RemoteTerrainMutationObserved << " x=" << state.TerrainMutationProbeX << " y=" << state.TerrainMutationProbeY << '\n' << std::flush;
		}
		const bool hostTerrainMutationPassed = !state.RequestHostTerrainMutationSmoke || (worldStateServer && worldStateServer->DidDebugTerrainMutationSmokePass());
		if (state.RequestHostTerrainMutationSmoke) {
			state.Log << "world_state_host_terrain_mutation=" << (hostTerrainMutationPassed ? "passed" : "failed") << '\n' << std::flush;
		}
		const bool hostEquipmentMutationPassed = !state.RequestHostEquipmentMutationSmoke || state.HostEquipmentMutationApplied;
		const bool remoteEquipmentMutationPassed = !state.RequireRemoteEquipmentMutationSmoke || state.RemoteEquipmentMutationObserved;
		if (state.RequestHostEquipmentMutationSmoke) {
			state.Log << "world_state_host_equipment_mutation=" << (hostEquipmentMutationPassed ? "passed" : "failed")
			          << " changed_magazines=" << state.EquipmentMutationChangedMagazines << '\n' << std::flush;
		}
		if (state.RequireRemoteEquipmentMutationSmoke) {
			state.Log << "world_state_remote_equipment_mutation=" << (remoteEquipmentMutationPassed ? "passed" : "failed")
			          << " observed=" << state.RemoteEquipmentMutationObserved << '\n' << std::flush;
		}
		state.Log << "utf8_unicode_font_loaded=" << (g_FrameMan.DidDebugUTF8GlyphFontLoad() ? "yes" : "no") << '\n' << std::flush;
		const std::string unicodeFontPath = g_PresetMan.GetFullModulePath("Base.rte/GUIs/Fonts/Roboto-Medium.ttf");
		std::filesystem::path unicodeFontFilePath = std::filesystem::u8path(unicodeFontPath);
		if (unicodeFontFilePath.is_relative()) {
			unicodeFontFilePath = std::filesystem::u8path(System::GetWorkingDirectory()) / unicodeFontFilePath;
		}
		std::ifstream unicodeFontProbe(unicodeFontFilePath, std::ios::binary);
		state.Log << "utf8_unicode_font_path=" << UTF8::PathToString(unicodeFontFilePath) << " exists=" << (unicodeFontProbe ? "yes" : "no") << '\n' << std::flush;
		state.Log << "utf8_glyph_render_smoke=" << (utf8GlyphRenderPassed ? "passed" : "failed") << '\n' << std::flush;
		const MovableMan::SceneStats sceneStats = g_MovableMan.CollectSceneStats();
		const bool performanceCountersPassed = state.PerformanceCountersObserved ||
		                                       (g_MovableMan.GetMovableObjectCount() > 0 && sceneStats.Actors > 0 && MovableObject::GetSceneSpawnEvents() > 0);
		state.Log << "performance_counters_smoke=" << (performanceCountersPassed ? "passed" : "failed")
		          << " observed_during_run=" << state.PerformanceCountersObserved
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
		success = success && state.LuaFilesystemProbePassed;
		if (worldStateClient) {
			state.Log << "world_state_replica_membership=" << (state.ReplicaWorldExact ? "passed" : "failed") << '\n' << std::flush;
			state.Log << "world_state_activity_presentation=" << (state.ReplicaActivityStateExact ? "passed" : "failed") << '\n' << std::flush;
			state.Log << "world_state_team_funds_presentation=" << (state.ReplicaTeamFundsExact ? "passed" : "failed") << '\n' << std::flush;
			success = success && state.ReplicaTeamFundsExact;
			success = success && state.ReplicaActivityStateExact;
			success = success && state.ReplicaWorldExact;
		}
		if (state.HostControlSmoke || state.ClientControlSmoke) {
			const bool controlPassed = (!state.HostControlSmoke || state.HostControlPassed) && (!state.ClientControlSmoke || state.ClientControlPassed);
			state.Log << "world_state_control_smoke=" << (controlPassed ? "passed" : "failed") << '\n' << std::flush;
			success = success && controlPassed;
		}
		if (state.RequireClientDisconnectAfterPause) {
			state.Log << "network_disconnect_while_paused=" << (state.ClientDisconnectWhilePausedObserved ? "passed" : "failed")
			          << " paused=" << state.SimulationPausedByDebugRun << " client_seen=" << state.SawClientBeforeDebugDisconnect << '\n' << std::flush;
			success = success && state.ClientDisconnectWhilePausedObserved;
		}
		if (state.RequireReconnectMenuTest) {
			state.Log << "world_state_menu_reconnect=" << (state.ReconnectWorldApplied ? "passed" : "failed") << '\n' << std::flush;
			success = success && state.ReconnectWorldApplied;
		}
		success = success && utf8GlyphRenderPassed && worldStateTransitionPassed && remoteTerrainMutationPassed && hostTerrainMutationPassed && hostEquipmentMutationPassed && remoteEquipmentMutationPassed &&
		          state.HostSessionTestPassed && clientFailurePassed && worldIdentityStable && replicaSmokePassed && backgroundEquipmentPassed && attachmentsPassed && objectCallbacksPassed && transientPixelReplicaPassed && terrainPatchApplyPassed &&
		          networkInputApplicationPassed && worldSnapshotPassed && snapshotCompressionPassed && liveSnapshotTransportPassed;
		if (state.RequestConstructionTest) {
			state.Log << "world_state_construction_ui=" << (state.ConstructionTestPassed ? "passed" : "failed") << '\n' << std::flush;
			success = success && state.ConstructionTestPassed;
		}
		const double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - state.StartTime).count();
		if (state.RequestInventoryTest) {
			const bool inventoryPassed=state.InventoryInitialObserved && state.InventoryMutationObserved && state.InventoryReuseObserved;
			state.Log << "world_state_inventory=" << (inventoryPassed ? "passed" : "failed") << '\n' << std::flush;
			success=success && inventoryPassed;
		}
		if (state.RequestBuyMenuTest) {
			if (state.RequirePurchasePolicyTest) {
				const bool passed=state.PurchasePolicyTestObserved || (worldStateBuyMenu && worldStateBuyMenu->IsSpectator());
				state.Log << "world_state_purchase_policy=" << (passed ? "passed" : "failed") << '\n' << std::flush;
				success=success && passed;
			}
			state.Log << "world_state_buy_menu=" << (state.BuyMenuTestPassed ? "passed" : "failed") << " stage=" << state.BuyMenuTestStage
			    << " catalog_captured=" << state.BuyMenuCatalogCaptured << " landing_captured=" << state.BuyMenuLandingCaptured << '\n' << std::flush;
			success=success && state.BuyMenuTestPassed;
		}
		if (state.RequestPurchaseTest) {
			const bool passed=state.PurchaseTest.Passed && !state.PurchaseTest.Failed;
			state.Log << "world_state_purchase=" << (passed ? "passed" : "failed") << " spectator=" << state.PurchaseTest.Spectator
			    << " stage=" << state.PurchaseTest.Stage << " saw_craft=" << state.PurchaseTest.SawCraft << '\n' << std::flush;
			success=success && passed;
		}
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
		g_ConsoleMan.SaveAllText(UTF8::PathToString(state.OutputDirectory / "LogConsole.txt"));
		for (const char* logName : {"LogLoading.txt", "LogLoadingWarning.txt"}) {
			const std::filesystem::path source = UTF8::PathFromString(System::GetWorkingDirectory()) / UTF8::PathFromString(logName);
			if (std::filesystem::exists(source)) {
				std::error_code copyError;
				std::filesystem::copy_file(source, state.OutputDirectory / logName, std::filesystem::copy_options::overwrite_existing, copyError);
			}
		}
		state.Log.close();
		System::SetQuit();
	}

	void CheckDebugRunClientDisconnectWhilePaused() {
		DebugRunState& state = GetDebugRunState();
		if (!state.Enabled || !state.RequireClientDisconnectAfterPause || !worldStateServer || System::IsSetToQuit()) {
			return;
		}
		if (worldStateServer->GetConnectedClientCount() > 0) {
			state.SawClientBeforeDebugDisconnect = true;
		}
		if (state.SimulationPausedByDebugRun && state.SawClientBeforeDebugDisconnect && worldStateServer->GetConnectedClientCount() == 0) {
			state.ClientDisconnectWhilePausedObserved = true;
			state.Log << "network_disconnect_detected_while_simulation_paused=true\n" << std::flush;
			const std::string imageName = UTF8::PathToString(state.OutputDirectory.filename()) + "/debug-run-final.png";
			const int screenshotResult = g_FrameMan.SaveScreenToPNGBlocking(imageName);
			state.Log << "screenshot=" << imageName << " result=" << screenshotResult << '\n' << std::flush;
			FinishDebugRun(true);
		}
	}

	void CaptureDebugRunFrame() {
		DebugRunState& state = GetDebugRunState();
		if (!state.Enabled || System::IsSetToQuit()) {
			return;
		}
		++state.RenderedFrames;
		const auto capture = [&](const char* label) {
		const std::string imageName = UTF8::PathToString(state.OutputDirectory.filename()) + "/debug-run-" + std::string(label) + ".png";
			const int screenshotResult = g_FrameMan.SaveScreenToPNGBlocking(imageName);
			state.Log << "screenshot=" << imageName << " result=" << screenshotResult << '\n' << std::flush;
			return screenshotResult;
		};
		if (worldStateClient && worldStateClient->IsConnected() && worldStateClient->HasSnapshot() && g_ActivityMan.GetActivity()) {
			auto* actor = dynamic_cast<AHuman*>(g_ActivityMan.GetActivity()->GetControlledActor(Players::PlayerOne));
			auto* gun = actor ? dynamic_cast<HDFirearm*>(actor->GetEquippedItem()) : nullptr;
			if (actor && actor->GetHealth() == 61.875F && gun) {
				if (!state.ShotActorReady && gun->GetNetworkShotSequence() == 0) {
					state.ShotActorReady = true; state.Log << "world_state_shot_actor_ready=passed\n" << std::flush;
				}
				if (!state.ShotFlashCaptured && gun->GetNetworkShotSequence() != 0 && !gun->FiredFrame() && gun->MuzzleFlashVisible()) {
					if (capture("recent-shot") != 0) { FinishDebugRun(false); return; }
					state.ShotFlashCaptured = true; state.ShotFlashSequence = gun->GetNetworkShotSequence(); state.ShotFlashRounds = gun->GetRoundInMagCount();
					state.ShotFlashCaptureUpdate = state.SimulationUpdates;
					state.Log << "world_state_recent_shot=passed sequence=" << state.ShotFlashSequence << " age_ms=" << gun->GetNetworkShotAgeMS() << " native_fire_frame=0\n" << std::flush;
				}
				if (state.ShotFlashCaptured && !state.ShotFlashExpired && state.SimulationUpdates >= state.ShotFlashCaptureUpdate + 2 && !gun->MuzzleFlashVisible()) {
					if (gun->GetNetworkShotSequence() != state.ShotFlashSequence || gun->GetRoundInMagCount() != state.ShotFlashRounds) {
						state.Log << "world_state_recent_shot_expired=failed sequence=" << gun->GetNetworkShotSequence() << " expected=" << state.ShotFlashSequence
						    << " rounds=" << gun->GetRoundInMagCount() << " expected_rounds=" << state.ShotFlashRounds << '\n' << std::flush;
						FinishDebugRun(false); return;
					}
					if (capture("recent-shot-expired") != 0) { FinishDebugRun(false); return; }
					state.ShotFlashExpired = true; state.Log << "world_state_recent_shot_expired=passed\n" << std::flush;
				}
			}
			if (actor && actor->GetHealth() == 61.125F && gun && gun->IsReloading()) {
				const float progress = gun->GetReloadProgress();
				if (!state.ReloadHudInitialCaptured && progress >= 0.34F && progress <= 0.38F) {
					if (capture("reload-hud-35") != 0) { FinishDebugRun(false); return; } state.ReloadHudInitialCaptured = true;
					state.Log << "world_state_reload_hud_initial=passed progress=" << progress << '\n' << std::flush;
				}
				if (state.ReloadHudInitialCaptured && !state.ReloadHudChangedCaptured && progress >= 0.74F && progress <= 0.78F) {
					if (capture("reload-hud-75") != 0) { FinishDebugRun(false); return; } state.ReloadHudChangedCaptured = true;
					state.Log << "world_state_reload_hud_changed=passed progress=" << progress << '\n' << std::flush;
				}
			}
		}
		if (state.RequestBuyMenuTest && worldStateBuyMenu && state.BuyMenuTestStage==1 && state.SimulationUpdates>=120 && !state.BuyMenuCatalogCaptured) {
			capture("buy-catalog"); state.BuyMenuCatalogCaptured=true;
		}
		if (state.RequestBuyMenuTest && state.BuyMenuTestStage==2 && state.SimulationUpdates>=220 && !state.BuyMenuLandingCaptured) {
			capture("buy-landing"); state.BuyMenuLandingCaptured=true;
		}
		if (!state.InitialScreenshotCaptured && state.SimulationUpdates >= 1) {
			state.InitialScreenshotCaptured = true;
			capture("initial");
		}
		if (!state.MidpointScreenshotCaptured && state.SimulationUpdates >= state.UpdateLimit / 2) {
			state.MidpointScreenshotCaptured = true;
			capture("stress");
		}
		const bool requiredHostTransitionComplete = !state.RequestHostWorldStateTransition || state.HostWorldStateTransitionApplied;
		const bool requiredClientTransitionComplete = !state.RequireWorldStateTransition || state.WorldStateTransitionApplied;
		const bool requiredTransitionsComplete = requiredHostTransitionComplete && requiredClientTransitionComplete &&
		    (!state.HostControlSmoke || (state.HostControlPassed && state.SimulationUpdates >= state.HostControlPassedUpdate + 180)) &&
		    (!state.ClientControlSmoke || state.ClientControlPassed) &&
		    (!state.RequestInventoryTest || (state.InventoryInitialObserved && state.InventoryMutationObserved && state.InventoryReuseObserved) ||
		     std::chrono::steady_clock::now()-state.SimulationStartTime>std::chrono::seconds(60));
		if (!state.FinalScreenshotCaptured && state.SimulationUpdates >= state.UpdateLimit && requiredTransitionsComplete) {
			state.FinalScreenshotCaptured = true;
			capture("final");
		}
		if (state.SimulationUpdates >= state.UpdateLimit && requiredTransitionsComplete) {
			const bool hostTransitionSettled = !state.RequestHostWorldStateTransition ||
			                                  state.SimulationUpdates >= state.HostWorldStateTransitionAppliedUpdate + 180;
			const bool clientTransitionSettled = !state.RequireWorldStateTransition ||
			                                    state.SimulationUpdates >= state.WorldStateTransitionAppliedUpdate + 180;
			if (hostTransitionSettled && clientTransitionSettled) {
				const bool setupSmokePassed = state.StressStarted || (worldStateClient && state.PerformanceCountersObserved);
				state.Log << "scenario_setup_smoke=" << (setupSmokePassed ? "passed" : "failed")
				          << " local_stress=" << state.StressStarted << " replicated_objects_observed="
				          << (worldStateClient && state.PerformanceCountersObserved) << '\n' << std::flush;
				FinishDebugRun(setupSmokePassed);
			}
		}
		if (!System::IsSetToQuit() && (state.RequireWorldStateTransition || state.RequestHostWorldStateTransition) &&
		    state.SimulationUpdates >= state.UpdateLimit &&
		    std::chrono::steady_clock::now() - state.SimulationStartTime > std::chrono::seconds(300)) {
			state.Log << "debug_world_state_transition_timeout=300s host_queued=" << state.HostWorldStateTransitionQueued
			          << " client_observed=" << state.WorldStateTransitionObserved << '\n' << std::flush;
			FinishDebugRun(false);
		}
	}
}

/// <summary>
/// Initializes all the essential managers.
/// </summary>
void ConstructManagers() {
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
}

bool InitializeHeadlessContent(const ServerConfiguration& config, const std::string& loadingLog, bool includeOptional = false) {
	g_ThreadMan.Initialize();
	g_LuaMan.Initialize();
	g_PerformanceMan.Initialize();
	g_SceneMan.Initialize();
	g_MovableMan.Initialize();
	return g_LoadingScreen.CreateLogOnly(loadingLog) && g_PresetMan.LoadAllDataModules(config.IncludeMods || includeOptional);
}

void StepHeadlessSimulation(WorldStateServerSession* server, std::uint32_t tick) {
	g_PerformanceMan.NewPerformanceSample();
	g_PerformanceMan.UpdateMSPSU();
	g_PerformanceMan.StartPerformanceMeasurement(PerformanceMan::SimTotal);
	if (server) server->Update(tick);
	g_LuaMan.Update();
	g_FrameMan.Update();
	g_MovableMan.CompleteQueuedMOIDDrawings();
	g_ActivityMan.Update();
	g_SceneMan.GetScene()->Update();
	g_MovableMan.Update(false);
	g_ActivityMan.LateUpdateGlobalScripts();
	g_PerformanceMan.StopPerformanceMeasurement(PerformanceMan::SimTotal);
}

int RunPersistentHeadlessServer(const ServerConfiguration& config) {
	const auto logDirectory = UTF8::PathFromString(config.LogDirectory);
	std::error_code error;
	std::filesystem::create_directories(logDirectory, error);
	if (error) { std::cerr << "[SERVER][ERROR] Cannot create log directory: " << error.message() << '\n'; return EXIT_FAILURE; }
	std::ofstream serverLog(logDirectory / "Server.log", std::ios::app);
	if (!serverLog) { std::cerr << "[SERVER][ERROR] Cannot open Server.log\n"; return EXIT_FAILURE; }
	RTEError::SetDiagnosticOutputDirectory(UTF8::PathToString(logDirectory));
	auto log = [&](const std::string& message) {
		const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		serverLog << '[' << seconds << "][SERVER] " << message << '\n' << std::flush;
		std::cout << "[SERVER] " << message << '\n' << std::flush;
	};
	ServerConsole console;
	log("INFO starting name=" + config.Name);
	if (!SDL_Init(0)) { log("ERROR SDL core initialization failed"); return EXIT_FAILURE; }
	ConstructManagers();
	g_TimerMan.Initialize();
	g_GLResourceMan.Initialize(false);
	const bool frameReady = g_FrameMan.Initialize() == 0;
	bool success = frameReady && InitializeHeadlessContent(config, UTF8::PathToString(logDirectory / "LogLoading.txt"));
	WorldStateServerSession session;
	if (success) {
		worldStateServerActivityClass = config.ActivityClass;
		worldStateServerActivityPreset = config.ActivityPreset;
		worldStateServerScenePreset = config.ScenePreset;
		worldStateServerTeamTech = config.TeamTech;
		std::string activityError;
		success = ConfigureWorldStateHostActivity(activityError) && g_ActivityMan.RestartActivity();
		if (!activityError.empty()) log("ERROR " + activityError);
		if (success && !config.TeamTech.empty()) {
			if (const auto* activity = dynamic_cast<const GameActivity*>(g_ActivityMan.GetActivity())) {
				for (int team = Activity::TeamOne; team < Activity::MaxTeamCount; ++team)
					log("INFO team_tech team=" + std::to_string(team) + " value=" + activity->GetTeamTech(team));
			}
		}
		if (success) {
			g_ActivityMan.ResumeActivity();
			success = session.Start(config.ListenAddress, config.Port, config.Players, UTF8::PathToString(logDirectory / "WorldStateServer.log"), false, false);
		}
	}
	std::unique_ptr<SoundContainer> diagnosticSound;
	AHuman* diagnosticReloadActor = nullptr;
	bool diagnosticObjectives = false;
	bool restartAwaitingPlayers = false;
	auto applyDiagnosticObjectives = [&] {
		if (auto* activity = dynamic_cast<GameActivity*>(g_ActivityMan.GetActivity())) {
			Vector position = g_CameraMan.GetScrollTarget(0);
			if (auto* actor = activity->GetControlledActor(0)) position = actor->GetPos();
			activity->ClearObjectivePoints();
			activity->AddObjectivePoint("Цель сети: Ёжик!", position + Vector(0, -40), 0, GameActivity::ARROWDOWN);
			activity->AddObjectivePoint("Enemy objective", position + Vector(60, 0), 1, GameActivity::ARROWLEFT);
		}
	};
	std::uint32_t tick = 0;
	if (success) log("INFO ready port=" + std::to_string(session.GetBoundPort()) + " video_initialized=" + std::to_string(SDL_WasInit(SDL_INIT_VIDEO) != 0));
	else log("ERROR startup failed");
	while (success && !ServerConsole::StopRequested.load() && !System::IsSetToQuit()) {
		for (const std::string& command : console.Poll()) {
			if (command == "quit" || command == "exit" || command == "stop") ServerConsole::StopRequested.store(true);
			else if (command == "status") log("INFO status tick=" + std::to_string(tick) + " clients=" + std::to_string(session.GetConnectedClientCount()) + " actors=" + std::to_string(g_MovableMan.GetActorCount()));
			else if (command == "debug-reload-hud-create" || command == "debug-shot-create") {
				const bool shotFixture = command == "debug-shot-create";
				g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
				Activity* activity = g_ActivityMan.GetActivity();
				const Entity* body = g_PresetMan.GetEntityPreset("AHuman", "Fat Culled Clone", "Base.rte");
				const Entity* weapon = g_PresetMan.GetEntityPreset("HDFirearm", "Auto Pistol", "Coalition.rte");
				std::unique_ptr<AHuman> actor(body ? dynamic_cast<AHuman*>(body->Clone()) : nullptr);
				std::unique_ptr<HDFirearm> gun(weapon ? dynamic_cast<HDFirearm*>(weapon->Clone()) : nullptr);
				if (!activity || !activity->PlayerActive(0) || !actor || !gun || (diagnosticReloadActor && g_MovableMan.IsActor(diagnosticReloadActor))) {
					log("ERROR debug_reload_hud=create_failed"); continue;
				}
				actor->SetTeam(activity->GetTeamOfPlayer(0)); actor->SetHealth(shotFixture ? 61.875F : 61.125F); actor->SetHUDVisible(true);
				actor->SetPos(g_SceneMan.MovePointToGround(Vector(400, 0), 0, 3)); actor->SetGlobalAccScalar(0); actor->SetPinStrength(1000000);
				if (shotFixture) actor->SetPos(actor->GetPos() + Vector(0, -40));
				actor->AddInventoryItem(gun.get()); gun.release(); actor->EquipFirearm();
				auto* equipped = dynamic_cast<HDFirearm*>(actor->GetEquippedItem());
				if (!equipped || !g_MovableMan.AddMO(actor.get())) { log("ERROR debug_reload_hud=equip_failed"); continue; }
				diagnosticReloadActor = actor.release(); activity->SetPlayerBrain(diagnosticReloadActor, 0);
				if (!activity->SwitchToActor(diagnosticReloadActor, 0, diagnosticReloadActor->GetTeam())) { log("ERROR debug_reload_hud=control_failed"); continue; }
				if (shotFixture) log("INFO debug_shot=created");
				else {
					equipped->SetBaseReloadTime(600000); equipped->Reload(); equipped->SetReloadProgress(0.35F);
					log(equipped->IsReloading() ? "INFO debug_reload_hud=created" : "ERROR debug_reload_hud=reload_failed");
				}
			} else if (command == "debug-shot-fire") {
				g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
				auto* gun = diagnosticReloadActor && g_MovableMan.IsActor(diagnosticReloadActor) ? dynamic_cast<HDFirearm*>(diagnosticReloadActor->GetEquippedItem()) : nullptr;
				if (!gun) { log("ERROR debug_shot=missing_gun"); continue; }
				const auto before = gun->GetNetworkShotSequence(); const int rounds = gun->GetRoundInMagCount();
				gun->SetActivationDelay(0); gun->SetDeactivationDelay(0); gun->SetFullAuto(false);
				gun->Activate(); gun->Update(); gun->Deactivate(); gun->Update();
				// Keep ambient controller input from adding another authoritative shot
				// during the expiry check; this disposable fixture fires once per minute.
				gun->SetRateOfFire(1);
				log(gun->GetNetworkShotSequence() != before && gun->GetRoundInMagCount() == rounds - 1 && !gun->FiredFrame() ? "INFO debug_shot=fired_between_snapshots" : "ERROR debug_shot=fire_failed");
			} else if (command == "debug-reload-hud-mutate") {
				auto* gun = diagnosticReloadActor && g_MovableMan.IsActor(diagnosticReloadActor) ? dynamic_cast<HDFirearm*>(diagnosticReloadActor->GetEquippedItem()) : nullptr;
				if (gun && gun->IsReloading()) { gun->SetReloadProgress(0.75F); log("INFO debug_reload_hud=changed"); }
				else log("ERROR debug_reload_hud=mutation_failed");
			} else if (command == "debug-sound") {
				if (!diagnosticSound) {
					const Entity* preset = g_PresetMan.GetEntityPreset("SoundContainer", "Confirm", "Base.rte");
					diagnosticSound.reset(preset ? dynamic_cast<SoundContainer*>(preset->Clone()) : nullptr);
					if (diagnosticSound) { diagnosticSound->SetLoopSetting(-1); diagnosticSound->SetBusRouting(SoundContainer::SFX); diagnosticSound->SetVolume(1); diagnosticSound->SetImmobile(true); }
				}
				const bool played = diagnosticSound && diagnosticSound->Play();
				log(played ? "INFO debug_sound=playing" : "ERROR debug_sound=unavailable");
			} else if (command == "debug-sound-mix-pause") {
				g_AudioMan.PauseIngameSounds(true);
				g_AudioMan.SetGlobalPitch(0.5F, true, false);
				log("INFO debug_sound_mix=paused");
			} else if (command == "debug-sound-mix-resume") {
				g_AudioMan.PauseIngameSounds(false);
				g_AudioMan.SetGlobalPitch(1, true, false);
				log("INFO debug_sound_mix=resumed");
			} else if (command == "debug-sound-fade") {
				if (diagnosticSound) diagnosticSound->FadeOut(4000);
				log("INFO debug_sound=fading");
			} else if (command == "debug-sound-volume-half") {
				if (diagnosticSound) diagnosticSound->SetVolume(0.5F);
				log("INFO debug_sound_volume=half");
			} else if (command == "debug-sound-finish") {
				g_AudioMan.FinishIngameLoopingSounds();
				log("INFO debug_sound=finishing");
			} else if (command == "debug-sound-stop") {
				if (diagnosticSound) diagnosticSound->Stop();
				log("INFO debug_sound=stopped");
			} else if (command == "debug-objectives") {
				diagnosticObjectives = true; applyDiagnosticObjectives(); log("INFO debug_objectives=created");
			} else if (command == "debug-objectives-clear") {
				diagnosticObjectives = false;
				if (auto* activity = dynamic_cast<GameActivity*>(g_ActivityMan.GetActivity())) activity->ClearObjectivePoints();
				log("INFO debug_objectives=cleared");
			} else if (command == "debug-screen-text") {
				if (Activity* activity = g_ActivityMan.GetActivity()) activity->ResetMessageTimer();
				g_FrameMan.ClearScreenText(0);
				g_FrameMan.SetScreenText("Network HUD: Привет, Ёжик!", 0, 250, 60000, true);
				log("INFO debug_screen_text=created");
			} else if (command == "debug-screen-text-clear") {
				g_FrameMan.ClearScreenText(0); log("INFO debug_screen_text=cleared");
			}
			else if (command == "debug-inventory-create") log(CreateWorldStateInventoryFixture() ? "INFO debug_inventory=create_passed" : "ERROR debug_inventory=create_failed");
			else if (command == "debug-inventory-mutate") log(MutateWorldStateInventoryFixture() ? "INFO debug_inventory=mutation_passed" : "ERROR debug_inventory=mutation_failed");
			else if (command == "debug-purchase-policy" || command == "debug-purchase-pricing") {
				auto* activity=dynamic_cast<GameActivity*>(g_ActivityMan.GetActivity());
				BuyMenuGUI* menu=activity ? activity->GetBuyGUI(0) : nullptr;
				if (menu) {
					menu->ClearAllowedItems(); menu->ClearAlwaysAllowedItems(); menu->ClearProhibitedItems(); menu->ClearOwnedItems();
					for (const char* name : {"Base.rte/Rocket MK2","Coalition.rte/Soldier Light","Coalition.rte/Assault Rifle","Coalition.rte/Auto Pistol"}) menu->AddAllowedItem(name);
					menu->AddAlwaysAllowedItem("Base.rte/Brain Case"); menu->AddProhibitedItem("Base.rte/SMG");
					menu->SetOwnedItemsAmount("Base.rte/Rocket MK2",1); menu->SetOwnedItemsAmount("Coalition.rte/Soldier Light",2);
					if (command == "debug-purchase-pricing") {
						if (g_MetaMan.GetPlayerCount()!=0) { log("ERROR debug_purchase_pricing=metagame_already_present"); continue; }
						// Use the normal metagame data reader for a disposable, non-UI economic fixture.
						const std::string metaData="MetaMan\n\tGameState = "+std::to_string(MetaMan::RUNACTIVITIES)+
						    "\n\tAddPlayer = MetaPlayer\n\t\tName = Network pricing fixture\n\t\tTeam = 0\n\t\tInGamePlayer = 0\n\t\tNativeTechModule = Ronin.rte\n\t\tNativeCostMultiplier = 0.5\n\t\tForeignCostMultiplier = 2\n";
						Reader metaReader(std::make_unique<std::istringstream>(metaData),"Base.rte/debug-purchase-pricing.ini");
						if (g_MetaMan.Serializable::Create(metaReader,true,false)<0 || !g_MetaMan.GameInProgress() || !g_MetaMan.GetMetaPlayerOfInGamePlayer(0)) {
							log("ERROR debug_purchase_pricing=reader_failed"); continue;
						}
						menu->SetOwnedItemsAmount("Base.rte/Rocket MK2",0);
						log("INFO debug_purchase_pricing=applied");
					}
					menu->SetOnlyShowOwnedItems(false); menu->SetEnforceMaxMassConstraint(false); menu->SetEnforceMaxPassengersConstraint(false);
					Scene::Area area; area.AddBox(Box(Vector(100,0),300,static_cast<float>(g_SceneMan.GetSceneHeight())));
					activity->SetLZArea(activity->GetTeamOfPlayer(0),area);
					log("INFO debug_purchase_policy=applied");
				} else log("ERROR debug_purchase_policy=unavailable");
			}
			else if (command == "restart") { g_ActivityMan.SetRestartActivity(); log("INFO activity_restart=requested"); }
			else if (command == "help") log("INFO commands: status, restart, funds TEAM VALUE, help, quit");
			else if (command.starts_with("funds ")) {
				std::istringstream input(command.substr(6));
				int team = -1;
				float funds = 0;
				std::string trailing;
				if (input >> team >> funds && !(input >> trailing) && team >= 0 && team < Activity::MaxTeamCount && std::isfinite(funds)) {
					g_ActivityMan.GetActivity()->SetTeamFunds(funds, team);
					log("INFO funds team=" + std::to_string(team) + " value=" + std::to_string(funds));
				} else log("WARNING usage: funds TEAM(0..3) FINITE_VALUE");
			}
			else log("WARNING unknown console command: " + command);
		}
		session.PollNetwork(tick);
		if (restartAwaitingPlayers && !session.GetAssignedPlayerSlots().empty()) {
			restartAwaitingPlayers = false;
			g_ActivityMan.SetRestartActivity();
		}
		if (g_ActivityMan.ActivitySetToRestart()) {
			if (session.GetAssignedPlayerSlots().empty()) {
				g_ActivityMan.SetRestartActivity(false);
				restartAwaitingPlayers = true;
				log("INFO activity_restart=waiting_for_players");
				continue;
			}
			g_MovableMan.CompleteQueuedMOIDDrawings();
			g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
			g_ThreadMan.GetBackgroundThreadPool().wait_for_tasks();
			if (auto* planned = g_ActivityMan.GetStartActivity()) session.ConfigureActivityPlayers(*planned, true);
			success = g_ActivityMan.RestartActivity();
			if (success) { g_ActivityMan.ResumeActivity(); log("INFO activity_restart=completed humans=" + std::to_string(g_ActivityMan.GetActivity()->GetHumanCount())); }
			else break;
		}
		g_TimerMan.Update();
		if (!g_TimerMan.TimeForSimUpdate()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue; }
		g_TimerMan.UpdateSim();
		if (diagnosticObjectives) applyDiagnosticObjectives();
		StepHeadlessSimulation(&session, ++tick);
	}
	diagnosticSound.reset();
	session.Stop();
	g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
	g_ThreadMan.GetBackgroundThreadPool().wait_for_tasks();
	g_AudioMan.Destroy();
	g_ConsoleMan.SaveAllText(UTF8::PathToString(logDirectory / "LogConsole.txt"));
	g_MovableMan.Destroy();
	g_SceneMan.Destroy();
	g_ActivityMan.Destroy();
	g_PresetMan.Destroy();
	g_LuaMan.Destroy();
	g_LoadingScreen.Destroy();
	g_FrameMan.Destroy();
	ContentFile::FreeAllLoaded();
	g_GLResourceMan.Destroy();
	g_WindowMan.Destroy();
	SDL_Quit();
	log("INFO stopped tick=" + std::to_string(tick) + " success=" + std::to_string(success));
	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

void InitializeManagers() {
	auto logDebugInitializationStage = [](const char* stage) {
		if (System::IsDebugRun()) {
			DebugRunState& state = GetDebugRunState();
			state.Log << "stage=manager_init_" << stage << '\n' << std::flush;
		}
	};
	logDebugInitializationStage("construct_begin");
	ConstructManagers();
	logDebugInitializationStage("construct_complete");

	g_ThreadMan.Initialize();
	logDebugInitializationStage("threads");
	g_SettingsMan.Initialize();
	logDebugInitializationStage("settings");
	g_WindowMan.Initialize();
	logDebugInitializationStage("window");
	g_GLResourceMan.Initialize();
	logDebugInitializationStage("gl_resources");

	g_LuaMan.Initialize();
	logDebugInitializationStage("lua");
	g_TimerMan.Initialize();
	logDebugInitializationStage("timer");
	g_FrameMan.Initialize();
	logDebugInitializationStage("frame");
	g_PostProcessMan.Initialize();
	logDebugInitializationStage("post_process");
	g_PerformanceMan.Initialize();
	logDebugInitializationStage("performance");

	if ((!System::IsDebugRun() || GetDebugRunState().EnableDebugAudio) && g_AudioMan.Initialize()) {
		g_GUISound.Initialize();
		g_MusicMan.Initialize();
	}

	g_UInputMan.Initialize();
	logDebugInitializationStage("input");
	g_ConsoleMan.Initialize();
	logDebugInitializationStage("console");
	g_SceneMan.Initialize();
	logDebugInitializationStage("scene");
	g_MovableMan.Initialize();
	logDebugInitializationStage("movable");
	g_MetaMan.Initialize();
	logDebugInitializationStage("meta");
	g_MenuMan.Initialize();
	logDebugInitializationStage("menu");

	// Overwrite Settings.ini after all the managers are created to fully populate the file. Up until this moment Settings.ini is populated only with minimal required properties to run.
	// If Settings.ini already exists and is fully populated, this will deal with overwriting it to apply any overrides performed by the managers at boot (e.g resolution validation).
	if (g_SettingsMan.SettingsNeedOverwrite()) {
		g_SettingsMan.UpdateSettingsFile();
	}
	logDebugInitializationStage("complete");
}

/// <summary>
/// Destroys all the managers and frees all loaded data before termination.
/// </summary>
void DestroyManagers() {
	g_MovableMan.CompleteQueuedMOIDDrawings();
	g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
	// Stop FMOD while every channel owner and the console still exist.
	if (System::IsDebugRun() && !GetDebugRunState().Log.is_open()) {
		GetDebugRunState().Log.open(GetDebugRunState().OutputDirectory / "DebugRun.log", std::ios::app);
	}
	if (System::IsDebugRun()) GetDebugRunState().Log << "audio_shutdown_begin\n" << std::flush;
	g_AudioMan.Destroy();
	if (System::IsDebugRun()) GetDebugRunState().Log << "audio_shutdown_complete\n" << std::flush;
	g_MetaMan.Destroy();
	g_PerformanceMan.Destroy();
	g_MovableMan.Destroy();
	g_SceneMan.Destroy();
	g_ActivityMan.Destroy();
	g_GUISound.Destroy();
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
		if ((currentArg == "-world-state-client" || (System::IsDebugRun() && currentArg == "-debug-run-menu-connect")) && i + 2 < argCount) {
			unsigned int port = 0;
			const std::string_view portArgument(argValue[i + 2]);
			const auto parsed = std::from_chars(portArgument.data(), portArgument.data() + portArgument.size(), port);
			if (parsed.ec == std::errc{} && parsed.ptr == portArgument.data() + portArgument.size() && port > 0 && port <= 65535) {
				if (currentArg == "-debug-run-menu-connect") {
					MainMenuGUI::DebugConnectionAddress = argValue[i + 1];
					MainMenuGUI::DebugConnectionPort = static_cast<unsigned short>(port);
				} else {
					worldStateClientAddress = argValue[i + 1];
					worldStateClientPort = static_cast<unsigned short>(port);
					MainMenuGUI::RetryServerAddress = worldStateClientAddress;
					MainMenuGUI::RetryServerPort = worldStateClientPort;
				}
				i += 2;
			}
		}
		if (System::IsDebugRun() && currentArg == "-debug-run-disconnect-menu") MainMenuGUI::DebugDisconnectMenuTest = true;
		if (System::IsDebugRun() && currentArg == "-debug-run-reconnect-menu") {
			MainMenuGUI::DebugDisconnectMenuTest = MainMenuGUI::DebugReconnectMenuTest = true;
			GetDebugRunState().RequireReconnectMenuTest = true;
		}
		if (currentArg == "-world-state-server") {
			worldStateServerRequested = true;
			if (!lastArg) {
				unsigned int port = 0;
				const std::string_view portArgument(argValue[i + 1]);
				const auto parsed = std::from_chars(portArgument.data(), portArgument.data() + portArgument.size(), port);
				if (parsed.ec == std::errc{} && parsed.ptr == portArgument.data() + portArgument.size() && port > 0 && port <= 65535) {
					worldStateServerPort = static_cast<unsigned short>(port);
					worldStateServerPortSpecified = true;
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
		if (!lastArg && currentArg == "-world-state-activity" && i + 2 < argCount) {
			worldStateServerActivityClass = argValue[i + 1];
			worldStateServerActivityPreset = argValue[i + 2];
			i += 2;
		}
		if (!lastArg && currentArg == "-world-state-scene") {
			worldStateServerScenePreset = argValue[++i];
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
	menuConnectionActivated = false;
	MainMenuGUI::BeginConnection = BeginMenuConnection;
	MainMenuGUI::PollConnection = PollMenuConnection;
	MainMenuGUI::CancelConnection = CancelMenuConnection;
	const bool menuConnectSmoke = System::IsDebugRun() && !MainMenuGUI::DebugConnectionAddress.empty();
	const bool menuSmoke = menuConnectSmoke || (System::IsDebugRun() && MainMenuGUI::DebugDisconnectMenuTest);
	const auto menuSmokeStart = std::chrono::steady_clock::now();
	bool debugReconnectStarted = false;
	menuClientLogPath = System::IsDebugRun() ? GetDebugRunState().OutputDirectory / "WorldStateClient.log" : std::filesystem::path("WorldStateClient.log");
	if (menuSmoke) g_ConsoleMan.SetEnabled(false);
	if (menuConnectSmoke) g_MenuMan.PrepareDebugMainMenu();
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
		if (menuSmoke && !MainMenuGUI::DebugMenuCapture.empty()) {
			const auto image = GetDebugRunState().OutputDirectoryName + "/" + MainMenuGUI::DebugMenuCapture + ".png";
			const int result = g_FrameMan.SaveScreenToPNGBlocking(image);
			GetDebugRunState().Log << "menu_capture=" << MainMenuGUI::DebugMenuCapture << " result=" << result << '\n' << std::flush;
			MainMenuGUI::DebugMenuCapture.clear();
			if (MainMenuGUI::DebugDisconnectMenuTest && MainMenuGUI::DisconnectedNetworkSession) {
				const bool passed = result == 0 && worldStateClient && !worldStateClient->IsConnected() && worldStateClient->HasSnapshot() && worldStateClientLocalSimulationTicks == 0;
				GetDebugRunState().Log << "disconnected_server_menu=" << (passed ? "passed" : "failed") << '\n' << std::flush;
				if (!MainMenuGUI::DebugReconnectMenuTest || !passed) FinishDebugRun(passed);
			}
		}
		if (System::IsDebugRun() && MainMenuGUI::DebugReconnectMenuTest && !debugReconnectStarted &&
		    std::filesystem::exists(GetDebugRunState().OutputDirectory / "Reconnect.ready")) {
			debugReconnectStarted = true;
			g_MenuMan.DebugRetryDisconnectedServer();
			GetDebugRunState().Log << "menu_reconnect_requested=true\n" << std::flush;
		}
		if (menuSmoke && std::chrono::steady_clock::now() - menuSmokeStart > std::chrono::seconds(40)) {
			GetDebugRunState().Log << "menu_connection=failed reason=timeout\n" << std::flush;
			FinishDebugRun(false);
		}
		if (g_SettingsMan.GetFPSLimit() > 0) {
			const auto frameInterval = std::chrono::duration<double>(1.0 / g_SettingsMan.GetFPSLimit());
			std::this_thread::sleep_until(frameWallStart + std::chrono::duration_cast<std::chrono::steady_clock::duration>(frameInterval));
		}
	}

	if (menuConnectSmoke) {
		GetDebugRunState().Log << "menu_validation_and_cancel=" << (MainMenuGUI::DebugMenuStage >= 3 ? "passed" : "failed")
			<< "\nmenu_connection=" << (menuConnectionActivated ? "passed" : "failed") << '\n' << std::flush;
		MainMenuGUI::DebugConnectionAddress.clear();
	}
	if (System::IsDebugRun() && MainMenuGUI::DebugReconnectMenuTest) {
		auto& state = GetDebugRunState();
		state.ReconnectMenuQueued = menuConnectionActivated && worldStateClient && worldStateClient->IsConnected() && worldStateClient->HasSnapshot();
		state.Log << "menu_reconnect_queued=" << (state.ReconnectMenuQueued ? "passed" : "failed") << '\n' << std::flush;
		state.UpdateLimit = state.SimulationUpdates + 120;
		MainMenuGUI::DebugDisconnectMenuTest = MainMenuGUI::DebugReconnectMenuTest = false;
		std::error_code markerError;
		std::filesystem::remove(state.OutputDirectory / "Reconnect.ready", markerError);
	}
	CancelMenuConnection();
	// Starting local content from a paused network game must stop snapshot replication.
	if (!menuConnectionActivated && g_ActivityMan.ActivitySetToRestart() && worldStateClient) {
		worldStateClient->Disconnect();
		worldStateSoundPlayback.Clear();
		worldStateClient.reset();
		MainMenuGUI::DisconnectedNetworkSession = false;
		worldStateClientReplica.Forget();
		g_FrameMan.SetClientGUIRenderer({});
		worldStateBuyMenu.reset(); worldStateConstructionMenu.reset();
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
		// Keep transport/lifecycle polling responsive even when the fixed simulation
		// step is paused or an Activity is sitting in an editor-only state. Gameplay
		// commands and world snapshots are still applied only on simulation ticks.
		if (worldStateClient) worldStateClient->Update();
		if (worldStateClient && worldStateClient->HasSnapshot() && !worldStateClient->GetConnectionError().empty() && !MainMenuGUI::DisconnectedNetworkSession) {
			MainMenuGUI::DisconnectedNetworkSession = true;
			worldStateSoundPlayback.Clear();
			const std::string reason = worldStateClient->GetConnectionError();
			if (worldStateClientLog.is_open()) worldStateClientLog << "connection_lost=" << reason << '\n' << std::flush;
			if (System::IsDebugRun() && !GetDebugRunState().RequireReconnectMenuTest && !MainMenuGUI::DebugDisconnectMenuTest) {
				GetDebugRunState().Log << "unexpected_network_disconnect=" << reason << '\n' << std::flush;
				FinishDebugRun(false); return;
			}
			g_TimerMan.PauseSim(true);
			// Pause the clock without running Activity pause callbacks on the replica.
			g_MenuMan.ShowDisconnectedServer(reason);
			RunMenuLoop();
			if (System::IsSetToQuit()) return;
			// RunGameLoop is the application's persistent gameplay loop. Returning
			// after Connect would tear down the application before loading its Activity.
			if (g_ActivityMan.ActivitySetToRestart()) {
				g_LoadingScreen.DrawLoadingSplash();
				g_WindowMan.UploadFrame();
				if (!g_ActivityMan.RestartActivity()) {
					if (System::IsDebugRun()) FinishDebugRun(false);
					return;
				}
			}
			g_TimerMan.PauseSim(false);
			g_PerformanceMan.ResetSimUpdateTimer();
			continue;
		}
		if (worldStateClient && System::IsDebugRun() && GetDebugRunState().RequestPurchaseTest)
			GetDebugRunState().PurchaseTest.Update(*worldStateClient, GetDebugRunState().Log);
		if (worldStateServer && worldStateServer->IsStarted()) worldStateServer->PollNetwork(worldStateServerSimulationTick);
		CheckDebugRunClientDisconnectWhilePaused();

		// Simulation update, as many times as the fixed update step allows in the span since last frame draw.
		while (g_TimerMan.TimeForSimUpdate()) {
			ZoneScopedN("Simulation Update");

			serverUpdated = false;

			g_PerformanceMan.NewPerformanceSample();
			g_PerformanceMan.UpdateMSPSU();
			g_TimerMan.UpdateSim();
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
						worldStateSoundPlayback.Clear();
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
			const bool worldStateReplicaClient = worldStateClient && worldStateClient->HasSnapshot();
			if (worldStateReplicaClient) {
				++worldStateClientReplicaSimulationTicks;
			} else if (worldStateClient && worldStateClient->IsConnected()) {
				++worldStateClientLocalSimulationTicks;
			}

			g_PerformanceMan.StartPerformanceMeasurement(PerformanceMan::SimTotal);
			g_LuaMan.ClearScriptTimings();

			g_PerformanceMan.StartPerformanceMeasurement(PerformanceMan::LuaManagerUpdate);
			if (worldStateReplicaClient) {
				g_LuaMan.UpdateWithoutScripts();
			} else {
				g_LuaMan.Update();
			}
			g_PerformanceMan.StopPerformanceMeasurement(PerformanceMan::LuaManagerUpdate);

			g_PerformanceMan.StartPerformanceMeasurement(PerformanceMan::InputUpdate);
			g_UInputMan.Update();
			g_PerformanceMan.StopPerformanceMeasurement(PerformanceMan::InputUpdate);
			if (worldStateClient && worldStateClient->IsConnected() && worldStateClient->HasPlayerAssignment() &&
			    worldStateClient->GetAssignedPlayerSlot() >= Players::PlayerOne) {
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
				if ((worldStateBuyMenu && worldStateBuyMenu->BlocksGameplayInput()) || worldStateClient->GetLatestSnapshot().ActivityState == 2 || g_UInputMan.KeyPressed(SDLK_F6)) {
					const std::uint32_t tick=inputCommand.ClientTick;
					inputCommand={}; inputCommand.ClientTick=tick;
				}
				DebugRunState& controlDebug = GetDebugRunState();
				if (controlDebug.Enabled && controlDebug.ClientControlSmoke) {
					inputCommand.HeldElements = 0;
					inputCommand.MouseButtonsHeld = 0;
					inputCommand.MouseDeltaX = inputCommand.MouseDeltaY = 0;
					inputCommand.AnalogMoveX = inputCommand.AnalogMoveY = 0;
					inputCommand.AnalogAimX = inputCommand.AnalogAimY = 0;
					if (controlDebug.ControlBaselineId && !controlDebug.ClientControlPassed) {
						inputCommand.HeldElements = std::uint64_t{1} << InputElements::INPUT_L_RIGHT;
						if (++controlDebug.ControlInputTicks % 30 < 10)
							inputCommand.HeldElements |= std::uint64_t{1} << InputElements::INPUT_FIRE;
					}
				}
				worldStateClient->SendInputCommand(inputCommand);
			}

			g_FrameMan.Update();

			g_MovableMan.CompleteQueuedMOIDDrawings();
			// Applying a snapshot can insert, remove, and mutate objects; wait until the
			// asynchronous MOID pass from the previous update has stopped using them.
			if (worldStateClient && worldStateClient->HasSnapshot() && !worldStateSceneTransitionQueued) {
				const std::uint32_t snapshotCount = worldStateClient->GetReceivedSnapshotCount();
				if (snapshotCount != worldStateClientLastObservedSnapshotCount) {
					const WorldStateProtocol::Snapshot& receivedSnapshot = worldStateClient->GetLatestSnapshot();
					DebugRunState& debugRunState = GetDebugRunState();
					if (debugRunState.ClientControlSmoke && !debugRunState.ClientControlPassed) {
						const int slot = worldStateClient->GetAssignedPlayerSlot();
						if (slot >= Players::PlayerOne && slot < Players::MaxPlayerCount) {
							const auto& player = receivedSnapshot.Players[slot];
							for (const auto& object : receivedSnapshot.Objects) {
								if (!player.ControlledActorId || object.NetworkId != player.ControlledActorId || !object.HasEquippedDeviceMagazine) continue;
								if (!debugRunState.ControlBaselineId) {
									debugRunState.ControlBaselineId = object.NetworkId;
									debugRunState.ControlStartX = object.PositionX;
									debugRunState.ControlStartRounds = object.EquippedDeviceMagazineRounds;
									debugRunState.Log << "control_client_baseline id=" << object.NetworkId << " rounds=" << object.EquippedDeviceMagazineRounds << '\n' << std::flush;
								}
								const float moved = std::abs(g_SceneMan.ShortestDistance(Vector(debugRunState.ControlStartX, 0), Vector(object.PositionX, 0)).GetX());
								if (object.NetworkId == debugRunState.ControlBaselineId && moved > 8 &&
								    object.EquippedDeviceMagazineRounds >= 0 && object.EquippedDeviceMagazineRounds < debugRunState.ControlStartRounds &&
								    worldStateClientReplica.BindPlayerView(object.NetworkId, player.Team)) {
									debugRunState.ClientControlPassed = true;
									debugRunState.Log << "world_state_client_control=passed moved=" << moved << " rounds=" << object.EquippedDeviceMagazineRounds << '\n' << std::flush;
								}
							}
						}
					}
					if (debugRunState.RequireRemoteEquipmentMutationSmoke && !debugRunState.RemoteEquipmentMutationObserved) {
						std::unordered_map<std::uint64_t, int> currentMagazineRounds;
						currentMagazineRounds.reserve(receivedSnapshot.Objects.size());
						for (const WorldStateProtocol::ObjectState& object : receivedSnapshot.Objects) {
							if (!object.HasEquippedDeviceMagazine) {
								continue;
							}
							currentMagazineRounds.emplace(object.NetworkId, object.EquippedDeviceMagazineRounds);
							const auto previous = debugRunState.PreviousMagazineRoundsByNetworkId.find(object.NetworkId);
							if (previous != debugRunState.PreviousMagazineRoundsByNetworkId.end() && previous->second != object.EquippedDeviceMagazineRounds) {
								debugRunState.RemoteEquipmentMutationObserved = true;
								GetDebugRunState().Log << "world_state_remote_equipment_mutation_observed=passed network_id=" << object.NetworkId
								                      << " previous_rounds=" << previous->second << " current_rounds=" << object.EquippedDeviceMagazineRounds
								                      << " tick=" << receivedSnapshot.Tick << '\n' << std::flush;
								break;
							}
						}
						debugRunState.PreviousMagazineRoundsByNetworkId = std::move(currentMagazineRounds);
					}
					const WorldStateClientReplica::ApplyResult applied = worldStateClientReplica.Apply(receivedSnapshot, true);
					if (System::IsDebugRun() && !debugRunState.NetworkObjectivesVerified && !receivedSnapshot.Objectives.empty() &&
					    receivedSnapshot.Objectives.front().Description == "Цель сети: Ёжик!") {
						const auto* activity = dynamic_cast<const GameActivity*>(g_ActivityMan.GetActivity());
						bool matches = activity && activity->GetObjectivePoints().size() == receivedSnapshot.Objectives.size();
						if (matches) {
							auto native = activity->GetObjectivePoints().begin();
							for (const auto& objective : receivedSnapshot.Objectives) {
								matches = matches && native->m_Description == objective.Description && native->m_ScenePos == Vector(objective.X, objective.Y) &&
								    native->m_Team == objective.Team && native->m_ArrowDir == objective.Arrow;
								++native;
							}
						}
						debugRunState.NetworkObjectivesVerified = matches;
						debugRunState.Log << "world_state_objectives_received=" << (matches ? "passed" : "failed") << '\n' << std::flush;
					}
					if (System::IsDebugRun() && debugRunState.NetworkObjectivesVerified && !debugRunState.NetworkObjectivesCleared &&
					    std::none_of(receivedSnapshot.Objectives.begin(), receivedSnapshot.Objectives.end(), [](const auto& objective) { return objective.Description == "Цель сети: Ёжик!"; })) {
						const auto* activity = dynamic_cast<const GameActivity*>(g_ActivityMan.GetActivity());
						const bool cleared = activity && std::none_of(activity->GetObjectivePoints().begin(), activity->GetObjectivePoints().end(),
						    [](const auto& objective) { return objective.m_Description == "Цель сети: Ёжик!"; });
						debugRunState.NetworkObjectivesCleared = cleared;
						debugRunState.Log << "world_state_objectives_cleared=" << (cleared ? "passed" : "failed") << '\n' << std::flush;
					}
					if (debugRunState.RequestInventoryTest && applied.InventoryMatches && applied.MissingPresets==0 && applied.AttachmentStatesChecked>0 && applied.AttachmentMismatches==0) {
						const int observed=ObserveWorldStateInventoryFixture(receivedSnapshot);
						if (observed==1 && !debugRunState.InventoryInitialObserved) {
							debugRunState.InventoryInitialObserved=true;
							debugRunState.Log << "world_state_inventory_initial=passed\n" << std::flush;
						}
						if (observed==2) {
							debugRunState.InventoryMutationObserved=true;
							if (applied.InventoryObjectsSpawned==0) debugRunState.InventoryReuseObserved=true;
						}
					}
					g_MovableMan.CommitPendingAdditions();
					if (debugRunState.Enabled) {
						Activity* activity = g_ActivityMan.GetActivity();
						debugRunState.ReplicaActivityStateExact = debugRunState.ReplicaActivityStateExact && activity &&
						    activity->GetActivityState() == receivedSnapshot.ActivityState;
						for (int team = 0; team < Activity::MaxTeamCount; ++team) debugRunState.ReplicaTeamFundsExact = debugRunState.ReplicaTeamFundsExact &&
						    activity && activity->GetTeamFunds(team) == receivedSnapshot.TeamFunds[team];
					}
					const int liveReplicaObjects = g_MovableMan.GetMovableObjectCount();
					if (debugRunState.Enabled && applied.MissingPresets == 0 &&
					    static_cast<std::size_t>(liveReplicaObjects) != receivedSnapshot.Objects.size()) {
						debugRunState.ReplicaWorldExact = false;
						debugRunState.Log << "replica_membership_mismatch live=" << liveReplicaObjects
						                  << " expected=" << receivedSnapshot.Objects.size() << '\n' << std::flush;
						std::unordered_map<std::string, int> identities;
						for (const auto& object : receivedSnapshot.Objects) ++identities[object.ClassName + "/" + object.ModuleName + "/" + object.PresetName];
						std::list<SceneObject*> local;
						g_MovableMan.GetAllActors(false, local); g_MovableMan.GetAllItems(false, local); g_MovableMan.GetAllParticles(false, local);
						for (const auto* object : local) --identities[object->GetClassName() + "/" + object->GetModuleName() + "/" + object->GetPresetName()];
						for (const auto& [identity, count] : identities) if (count != 0) debugRunState.Log << "replica_identity_difference=" << identity << " count=" << count << '\n';
					}
					if (debugRunState.RequireReconnectMenuTest && debugRunState.ReconnectMenuQueued && !debugRunState.ReconnectWorldApplied &&
					    receivedSnapshot.ActivityPreset == "Skirmish Defense" && receivedSnapshot.ScenePreset == "Ketanot Hills" && !receivedSnapshot.Objects.empty() &&
					    g_ActivityMan.GetActivity() && g_ActivityMan.GetActivity()->GetPresetName() == receivedSnapshot.ActivityPreset &&
					    g_SceneMan.GetScene() && g_SceneMan.GetScene()->GetPresetName() == receivedSnapshot.ScenePreset &&
					    applied.MissingPresets == 0 && static_cast<std::size_t>(liveReplicaObjects) == receivedSnapshot.Objects.size() &&
					    worldStateClient->IsConnected() && worldStateClient->HasPlayerAssignment() && worldStateClientLocalSimulationTicks == 0) {
						debugRunState.ReconnectWorldApplied = true;
						debugRunState.Log << "menu_reconnect_world=passed activity=" << receivedSnapshot.ActivityPreset << " scene=" << receivedSnapshot.ScenePreset << '\n' << std::flush;
					}
					g_MovableMan.UpdateDrawMOIDs();
					worldStateClientLastObservedSnapshotCount = snapshotCount;
					if (worldStateClientLog.is_open() &&
					    (worldStateClientLastLoggedSnapshotCount == 0 || snapshotCount - worldStateClientLastLoggedSnapshotCount >= 20)) {
						worldStateClientLog << "snapshot=" << snapshotCount << " updated=" << applied.Updated << " spawned=" << applied.Spawned << " removed=" << applied.Removed
						                    << " scene_revision=" << receivedSnapshot.SceneRevision << " objectives=" << receivedSnapshot.Objectives.size()
						                    << " player_slot=" << worldStateClient->GetAssignedPlayerSlot()
						                    << " controlled_actor=" << (worldStateClient->GetAssignedPlayerSlot() >= 0 ? receivedSnapshot.Players[worldStateClient->GetAssignedPlayerSlot()].ControlledActorId : 0)
						                    << " live_objects=" << liveReplicaObjects << " expected_objects=" << receivedSnapshot.Objects.size()
						                    << " activity_state=" << static_cast<int>(receivedSnapshot.ActivityState)
						                    << " funds_team0=" << std::to_string(receivedSnapshot.TeamFunds[0])
						                    << " funds_team1=" << std::to_string(receivedSnapshot.TeamFunds[1])
						                    << " missing_presets=" << applied.MissingPresets << " human_equipment_applied=" << applied.HumanEquipmentStatesApplied
						                    << " inventory_applied=" << applied.InventoryStatesApplied << " inventory_spawned=" << applied.InventoryObjectsSpawned << " inventory_matches=" << applied.InventoryMatches
						                    << " attachment_states_checked=" << applied.AttachmentStatesChecked << " attachment_mismatches=" << applied.AttachmentMismatches
						                    << " inventory_live=" << applied.InventoryLiveObjects << " inventory_cached=" << applied.InventoryCachedObjects
						                    << " position_error_samples=" << applied.PositionErrorsMeasured
						                    << " mean_position_error_before_correction_px=" << applied.MeanPositionErrorBeforeCorrection
						                    << " max_position_error_before_correction_px=" << applied.MaxPositionErrorBeforeCorrection << '\n'
						                    << std::flush;
						for (const std::string& detail : applied.MissingPresetDetails) {
							worldStateClientLog << "missing=" << detail << '\n';
						}
						for (const std::string& detail : applied.InventoryMismatchDetails) worldStateClientLog << "inventory_mismatch=" << detail << '\n';
						worldStateClientLog.flush();
						worldStateClientLastLoggedSnapshotCount = snapshotCount;
					}
				}
			}
			if (worldStateReplicaClient && !worldStateSceneTransitionQueued) {
				worldStateClientReplica.AdvanceInterpolation();
				const auto& soundSnapshot = worldStateClient->GetLatestSnapshot();
				const auto soundEvents = worldStateClient->DrainSoundEvents(soundSnapshot.SceneRevision);
				const auto soundsApplied = worldStateSoundPlayback.Apply(soundSnapshot.SceneRevision, worldStateClient->GetAssignedPlayerSlot(), soundEvents);
				if (System::IsDebugRun()) {
					auto& state = GetDebugRunState();
					for (const auto& event : soundEvents) {
						if (event.Type == WorldStateSoundProtocol::Action::Play && event.Module == "Base.rte" &&
						    event.Resource == "Sounds/GUIs/SlicePicked.flac" && (event.Loops == -1 || (event.Loops == 0 && event.CursorMilliseconds > 0)))
							state.Log << "world_state_sound_fixture_event voice=" << event.Voice
							          << " cursor_ms=" << event.CursorMilliseconds << '\n' << std::flush;
						if (!state.NetworkSoundPlayVerified && event.Type == WorldStateSoundProtocol::Action::Play &&
						    event.Module == "Base.rte" && event.Resource == "Sounds/GUIs/SlicePicked.flac" && (event.Loops == -1 || (event.Loops == 0 && event.CursorMilliseconds > 0)) &&
						    worldStateSoundPlayback.HasVoice(event.Voice)) {
							state.NetworkSoundFixture = event.Container; state.NetworkSoundVoice = event.Voice; state.NetworkSoundPlayVerified = true;
							state.Log << "world_state_sound_play=passed audio_enabled=" << g_AudioMan.IsAudioEnabled() << '\n' << std::flush;
							if (event.CursorMilliseconds > 0)
								state.Log << "world_state_sound_late_join=passed cursor_ms=" << event.CursorMilliseconds
								          << " paused=" << worldStateSoundPlayback.IsVoicePaused(event.Voice) << '\n' << std::flush;
							if (event.Loops == 0 && worldStateSoundPlayback.IsVoiceFinishing(event.Voice)) {
								state.NetworkSoundFinishRequested = true;
								state.Log << "world_state_sound_finish_received=passed source=baseline\n" << std::flush;
							}
							if (event.FadeMilliseconds) {
								state.NetworkSoundFadeRequested = true;
								const float volume = worldStateSoundPlayback.GetVoiceVolume(event.Voice);
								const bool restored = std::abs(volume - event.Gain * event.FadeGain) < 0.001F && event.FadeGain > 0 && event.FadeGain < 1;
								state.Log << "world_state_sound_fade_restored=" << (restored ? "passed" : "failed") << " gain=" << event.FadeGain << " volume=" << volume << '\n' << std::flush;
							}
							if (g_AudioMan.IsAudioEnabled()) {
								const bool lifetime = g_AudioMan.VerifySoundContainerLifetime();
								state.Log << "audio_owner_lifetime=" << (lifetime ? "passed" : "failed") << '\n' << std::flush;
							}
						}
						if (state.NetworkSoundPlayVerified && event.Type == WorldStateSoundProtocol::Action::BusPause && event.Bus == 0) {
							const bool applied = worldStateSoundPlayback.HasVoice(state.NetworkSoundVoice) && worldStateSoundPlayback.IsVoicePaused(state.NetworkSoundVoice) == event.Paused;
							state.Log << (applied ? "world_state_sound_bus_pause=" : "world_state_sound_bus_pause_failed=") << event.Paused << '\n' << std::flush;
						}
						if (state.NetworkSoundPlayVerified && event.Type == WorldStateSoundProtocol::Action::FinishBusLoops && event.Bus == 0) {
							state.NetworkSoundFinishRequested = worldStateSoundPlayback.IsVoiceFinishing(state.NetworkSoundVoice);
							state.Log << "world_state_sound_finish_received=" << (state.NetworkSoundFinishRequested ? "passed" : "failed") << " source=command\n" << std::flush;
						}
						if (state.NetworkSoundPlayVerified && event.Type == WorldStateSoundProtocol::Action::Fade && event.Container == state.NetworkSoundFixture) {
							state.NetworkSoundFadeRequested = true;
								state.Log << "world_state_sound_fade_received=passed\n" << std::flush;
						}
						if (state.NetworkSoundFadeRequested && event.Type == WorldStateSoundProtocol::Action::Volume && event.Container == state.NetworkSoundFixture && event.Gain == 0.5F) {
							const float volume = worldStateSoundPlayback.GetVoiceVolume(state.NetworkSoundVoice);
							const bool changed = state.NetworkSoundFadePausedVolume > 0 && std::abs(volume - state.NetworkSoundFadePausedVolume * 0.5F) < 0.002F;
							state.Log << "world_state_sound_fade_volume=" << (changed ? "passed" : "failed") << " volume=" << volume << '\n' << std::flush;
							state.NetworkSoundFadePausedVolume = volume;
						}
						if (state.NetworkSoundPlayVerified && !state.NetworkSoundStopVerified && event.Type == WorldStateSoundProtocol::Action::Stop &&
						    event.Container == state.NetworkSoundFixture && !worldStateSoundPlayback.HasContainer(event.Container)) {
							state.NetworkSoundStopVerified = true;
							state.Log << "world_state_sound_stop=passed\n" << std::flush;
						}
					}
					if (state.NetworkSoundFinishRequested && !state.NetworkSoundFinishVerified && !worldStateSoundPlayback.HasVoice(state.NetworkSoundVoice)) {
						state.NetworkSoundFinishVerified = true;
						state.Log << "world_state_sound_finish_completed=passed\n" << std::flush;
					}
					if (state.NetworkSoundFadeRequested) {
						if (!worldStateSoundPlayback.HasVoice(state.NetworkSoundVoice) && !state.NetworkSoundFadeEnded) {
							state.NetworkSoundFadeEnded = true; state.Log << "world_state_sound_fade_completed=passed\n" << std::flush;
						} else if (worldStateSoundPlayback.HasVoice(state.NetworkSoundVoice)) {
							const float volume = worldStateSoundPlayback.GetVoiceVolume(state.NetworkSoundVoice);
							const bool paused = worldStateSoundPlayback.IsVoicePaused(state.NetworkSoundVoice);
							if (!paused && !state.NetworkSoundFadeMidpoint && volume > 0.25F && volume < 0.75F) {
								state.NetworkSoundFadeMidpoint = true; state.Log << "world_state_sound_fade_midpoint=passed volume=" << volume << '\n' << std::flush;
							}
							if (paused && state.NetworkSoundFadePausedVolume < 0) { state.NetworkSoundFadePausedVolume = volume; state.NetworkSoundFadePauseTime = SoundPlaybackClockMilliseconds(); }
							if (paused && !state.NetworkSoundFadeFrozen && SoundPlaybackClockMilliseconds() - state.NetworkSoundFadePauseTime > 200 &&
							    std::abs(volume - state.NetworkSoundFadePausedVolume) < 0.001F) {
								state.NetworkSoundFadeFrozen = true; state.Log << "world_state_sound_fade_frozen=passed volume=" << volume << '\n' << std::flush;
							}
						}
					}
				}
				if (!soundEvents.empty() && worldStateClientLog.is_open())
					worldStateClientLog << "sound_events_received=" << soundEvents.size() << " applied=" << soundsApplied << " active=" << worldStateSoundPlayback.Size() << '\n';
				const int slot = worldStateClient->GetAssignedPlayerSlot();
				const bool activeView = slot >= Players::PlayerOne && slot < Players::MaxPlayerCount &&
				    worldStateClient->GetLatestSnapshot().Players[slot].ActiveHuman;
				if (!activeView) {
					g_FrameMan.ApplyNetworkScreenText("", 0, false);
					if (Activity* activity = g_ActivityMan.GetActivity(); activity && activity->GetControlledActor(Players::PlayerOne))
						activity->ClearReplicaControlledActor(Players::PlayerOne);
				}
				if (slot >= Players::PlayerOne && slot < Players::MaxPlayerCount) {
					const auto& snapshot = worldStateClient->GetLatestSnapshot();
					const auto& player = snapshot.Players[slot];
					if (player.ActiveHuman) {
						if (snapshot.ActivityState == 2) g_FrameMan.ApplyNetworkScreenText("F7: objects | B: brain placement | F6: ready", 0, false);
						else g_FrameMan.ApplyNetworkScreenText(player.ScreenText, static_cast<int>(player.TextBlinkInterval), player.TextCentered);
						if (System::IsDebugRun() && !GetDebugRunState().NetworkScreenTextVerified && player.ScreenText == "Network HUD: Привет, Ёжик!") {
							const bool applied = g_FrameMan.GetScreenText() == player.ScreenText &&
							    g_FrameMan.GetScreenTextBlinkInterval() == 250 && g_FrameMan.IsScreenTextCentered();
							GetDebugRunState().Log << "world_state_screen_text_received=" << (applied ? "passed" : "failed") << " slot=" << slot << '\n' << std::flush;
							GetDebugRunState().NetworkScreenTextVerified = applied;
						}
						worldStateClientReplica.BindPlayerView(player.ControlledActorId, player.Team);
						Vector camera(player.CameraX, player.CameraY);
						worldStateClientReplica.AdjustCameraForInterpolation(player.ControlledActorId, camera);
						g_CameraMan.SetScrollTarget(camera, 0.1F, 0);
					}
				}
			}

			g_ConsoleMan.Update();
			if (!worldStateReplicaClient) {
				g_ActivityMan.Update();
			}

			if (!worldStateReplicaClient && g_SceneMan.GetScene()) {
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

			if (!worldStateReplicaClient) {
				g_MovableMan.Update();
			} else if (!worldStateSceneTransitionQueued && g_SceneMan.GetScene() && g_TimerMan.DrawnSimUpdate()) {
				g_PostProcessMan.ClearScenePostEffects();
				g_SceneMan.ClearMOColorLayer();
				g_MovableMan.Draw(g_SceneMan.GetMOColorBitmap());
			}

			g_AudioMan.Update();
			if (worldStateReplicaClient && !worldStateSceneTransitionQueued && !GetDebugRunState().RequestPurchaseTest) {
				if (!worldStateBuyMenu) {
					worldStateBuyMenu=std::make_unique<WorldStateBuyMenu>();
					g_FrameMan.SetClientGUIRenderer([](BITMAP* target, int screen, const Vector& offset) {
						if (worldStateConstructionMenu && worldStateConstructionMenu->IsActive()) worldStateConstructionMenu->Draw(target, screen, offset);
						else if (worldStateBuyMenu) worldStateBuyMenu->Draw(target, screen, offset);
					});
				}
				DebugRunState& uiTest=GetDebugRunState();
				if (!worldStateConstructionMenu) worldStateConstructionMenu = std::make_unique<WorldStateConstructionMenu>();
				worldStateConstructionMenu->Update(*worldStateClient, !uiTest.RequestConstructionTest);
				if (!worldStateConstructionMenu->IsActive()) worldStateBuyMenu->Update(*worldStateClient, !uiTest.RequestBuyMenuTest);
				if (uiTest.RequestConstructionTest) {
					const auto& snapshot = worldStateClient->GetLatestSnapshot();
					const int slot = worldStateClient->GetAssignedPlayerSlot();
					const auto humans = std::count_if(snapshot.Players.begin(), snapshot.Players.end(), [](const auto& player) { return player.ActiveHuman; });
					const auto deployment = std::find_if(snapshot.Objects.begin(), snapshot.Objects.end(), [](const auto& object) {
						return object.ClassName == "AHuman" && object.ModuleName == "Coalition.rte" && object.PresetName == "Soldier Light" && object.Team == 1;
					});
					AHuman* localDeployment = nullptr;
					int localPistols = 0;
					if (uiTest.RequestDeploymentTest) for (auto* actor : *g_MovableMan.GetTeamRoster(1)) {
						if (actor->GetModuleName() != "Coalition.rte" || actor->GetPresetName() != "Soldier Light") continue;
						localDeployment = dynamic_cast<AHuman*>(actor);
						if (!localDeployment) continue;
						auto countPistol = [&](const MovableObject* object) { return object && object->GetModuleName() == "Coalition.rte" && object->GetPresetName() == "Auto Pistol"; };
						for (const auto* item : *localDeployment->GetInventory()) if (countPistol(item)) ++localPistols;
						if (countPistol(localDeployment->GetEquippedItem())) ++localPistols;
						if (localDeployment->GetBGArm() && countPistol(localDeployment->GetBGArm()->GetHeldDevice())) ++localPistols;
						break;
					}
					if (uiTest.ConstructionTestStage == 0 && humans == 2 && slot >= 0 && worldStateConstructionMenu->IsActive() && worldStateConstructionMenu->HasAppliedCatalog()) {
						if (worldStateConstructionMenu->PlaceBrain(*worldStateClient, Vector(100 + 100 * slot, 30))) {
							uiTest.ConstructionTestStage = 1; uiTest.ConstructionActionUpdate = uiTest.SimulationUpdates;
						}
					} else if (uiTest.ConstructionTestStage == 1 && worldStateConstructionMenu->HasOwnPreview() && worldStateConstructionMenu->LastAccepted() &&
					           uiTest.SimulationUpdates >= uiTest.ConstructionActionUpdate + 30) {
						const std::string image = UTF8::PathToString(uiTest.OutputDirectory.filename()) + "/debug-run-construction-preview.png";
						const int saved = g_FrameMan.SaveScreenToPNGBlocking(image);
						uiTest.Log << "construction_preview_capture=" << (saved == 0 ? "passed" : "failed") << '\n' << std::flush;
						const auto* gun = dynamic_cast<const SceneObject*>(g_PresetMan.GetEntityPreset("HDFirearm", "AK-47", "Ronin.rte"));
						if (saved == 0 && worldStateConstructionMenu->OpenPicker() && worldStateConstructionMenu->GetPicker()->SelectGroupByName("Weapons") && worldStateConstructionMenu->SelectObject(gun)) {
							const auto& catalog = worldStateClient->GetConstructionCatalog();
							const auto* picker = worldStateConstructionMenu->GetPicker();
							RTEAssert(picker->GetTypeFilter() == catalog.TypeFilter && picker->GetForeignCostMultiplier() == catalog.ForeignMultiplier,
							          "Construction picker did not apply server catalog");
							uiTest.Log << "construction_catalog_applied=passed\n" << std::flush;
							uiTest.ConstructionTestStage = 10; uiTest.ConstructionActionUpdate = uiTest.SimulationUpdates;
						}
					} else if (uiTest.ConstructionTestStage == 10 && uiTest.SimulationUpdates >= uiTest.ConstructionActionUpdate + 30) {
						const std::string image = UTF8::PathToString(uiTest.OutputDirectory.filename()) + "/debug-run-construction-catalog.png";
						const int saved = g_FrameMan.SaveScreenToPNGBlocking(image);
						uiTest.Log << "construction_catalog_capture=" << (saved == 0 ? "passed" : "failed") << '\n' << std::flush;
						worldStateConstructionMenu->GetPicker()->SetEnabled(false);
						uiTest.ConstructionInitialFunds = snapshot.TeamFunds[snapshot.Players[slot].Team];
						uiTest.ConstructionExpectedAffordable = uiTest.ConstructionInitialFunds >= 160;
						if (saved == 0 && worldStateConstructionMenu->PlaceObject(*worldStateClient, Vector(400 + 100 * slot, 30))) uiTest.ConstructionTestStage = 11;
					} else if (uiTest.ConstructionTestStage == 11 && !worldStateConstructionMenu->IsPending() &&
					           worldStateConstructionMenu->LastAccepted() == uiTest.ConstructionExpectedAffordable &&
					           (uiTest.ConstructionExpectedAffordable ? worldStateConstructionMenu->LastChargedCost() > 0 : worldStateConstructionMenu->LastRejection() == "insufficient_funds") &&
					           snapshot.TeamFunds[snapshot.Players[slot].Team] == uiTest.ConstructionInitialFunds - worldStateConstructionMenu->LastChargedCost()) {
						uiTest.Log << "construction_object_ui=passed purchased=" << uiTest.ConstructionExpectedAffordable << '\n' << std::flush;
						uiTest.ConstructionObjectCost = worldStateConstructionMenu->LastChargedCost();
						uiTest.ConstructionTestStage = 12; uiTest.ConstructionActionUpdate = uiTest.SimulationUpdates;
					} else if (uiTest.ConstructionTestStage == 12 && uiTest.SimulationUpdates >= uiTest.ConstructionActionUpdate + 30) {
						const std::string image = UTF8::PathToString(uiTest.OutputDirectory.filename()) + "/debug-run-construction-object.png";
						const int saved = g_FrameMan.SaveScreenToPNGBlocking(image);
						uiTest.Log << "construction_object_preview_capture=" << (saved == 0 ? "passed" : "failed") << '\n' << std::flush;
						if (saved == 0) {
							if (uiTest.ConstructionExpectedAffordable) { if (worldStateConstructionMenu->RemoveLastObject(*worldStateClient)) uiTest.ConstructionTestStage = 13; }
							else { uiTest.ConstructionTestStage = 14; uiTest.ConstructionActionUpdate = uiTest.SimulationUpdates; }
						}
					} else if (uiTest.ConstructionTestStage == 13 && !worldStateConstructionMenu->IsPending() && worldStateConstructionMenu->LastAccepted() &&
					           worldStateConstructionMenu->LastPlacedObjectId() == 0 &&
					           std::none_of(snapshot.Objects.begin(), snapshot.Objects.end(), [&](const auto& object) {
						           return object.ClassName == "HDFirearm" && object.ModuleName == "Ronin.rte" && object.PresetName == "AK-47" && object.Team == snapshot.Players[slot].Team;
					           }) && snapshot.TeamFunds[snapshot.Players[slot].Team] == uiTest.ConstructionInitialFunds - uiTest.ConstructionObjectCost) {
						uiTest.Log << "construction_object_removal=passed\n" << std::flush;
						uiTest.ConstructionTestStage = 14; uiTest.ConstructionActionUpdate = uiTest.SimulationUpdates;
					} else if (uiTest.ConstructionTestStage == 14 && uiTest.SimulationUpdates >= uiTest.ConstructionActionUpdate + 30) {
						const auto* block = dynamic_cast<const SceneObject*>(g_PresetMan.GetEntityPreset("TerrainObject", "Concrete Block", "Base.rte"));
						if (worldStateConstructionMenu->OpenPicker() && worldStateConstructionMenu->GetPicker()->SelectGroupByName("Bunker Bits") && worldStateConstructionMenu->SelectObject(block)) {
							worldStateConstructionMenu->GetPicker()->SetEnabled(false);
							uiTest.ConstructionTerrainFunds = snapshot.TeamFunds[snapshot.Players[slot].Team];
							if (worldStateConstructionMenu->PlaceObject(*worldStateClient, Vector(800 + 100 * slot, 120))) uiTest.ConstructionTestStage = 15;
						}
					} else if (uiTest.ConstructionTestStage == 15 && !worldStateConstructionMenu->IsPending() &&
					           worldStateConstructionMenu->LastAccepted() == uiTest.ConstructionExpectedAffordable &&
					           (uiTest.ConstructionExpectedAffordable ? worldStateConstructionMenu->LastChargedCost() == 20 : worldStateConstructionMenu->LastRejection() == "insufficient_funds") &&
					           snapshot.TeamFunds[snapshot.Players[slot].Team] == uiTest.ConstructionTerrainFunds - worldStateConstructionMenu->LastChargedCost() &&
					           g_SceneMan.GetTerrain()->GetMaterialPixel(804 + 100 * slot, 124) == (uiTest.ConstructionExpectedAffordable ? 177 : 0) &&
					           g_SceneMan.GetTerrain()->GetMaterialPixel(904, 124) == 177 && g_SceneMan.GetTerrain()->GetFGColorPixel(904, 124) == 89 &&
					           g_SceneMan.GetTerrain()->GetBGColorPixel(904, 124) == 181) {
						uiTest.Log << "construction_terrain_ui=passed purchased=" << uiTest.ConstructionExpectedAffordable << '\n' << std::flush;
						worldStateConstructionMenu->SelectBrain();
						uiTest.ConstructionTestStage = 16; uiTest.ConstructionActionUpdate = uiTest.SimulationUpdates;
					} else if (uiTest.ConstructionTestStage == 16 && uiTest.SimulationUpdates >= uiTest.ConstructionActionUpdate + 30) {
						const std::string image = UTF8::PathToString(uiTest.OutputDirectory.filename()) + "/debug-run-construction-terrain.png";
						const int saved = g_FrameMan.SaveScreenToPNGBlocking(image);
						uiTest.Log << "construction_terrain_capture=" << (saved == 0 ? "passed" : "failed") << '\n' << std::flush;
						if (saved == 0) {
							if (!uiTest.RequestDeploymentTest) { if (worldStateConstructionMenu->SetReady(*worldStateClient, true)) uiTest.ConstructionTestStage = 2; }
							else {
								const auto* preset = dynamic_cast<const SceneObject*>(g_PresetMan.GetEntityPreset("Deployment", "Default", "Base.rte"));
								if (worldStateConstructionMenu->OpenPicker() && worldStateConstructionMenu->GetPicker()->SelectGroupByName("Deployments") && worldStateConstructionMenu->SelectObject(preset)) {
									worldStateConstructionMenu->GetPicker()->SetEnabled(false);
									uiTest.ConstructionDeploymentFunds = snapshot.TeamFunds[snapshot.Players[slot].Team];
									if (worldStateConstructionMenu->PlaceObject(*worldStateClient, Vector(812 + 100 * slot, 70))) uiTest.ConstructionTestStage = 17;
								}
							}
						}
					} else if (uiTest.ConstructionTestStage == 17 && !worldStateConstructionMenu->IsPending() &&
					           worldStateConstructionMenu->LastAccepted() == uiTest.ConstructionExpectedAffordable &&
					           (uiTest.ConstructionExpectedAffordable ? worldStateConstructionMenu->LastChargedCost() == 188 : worldStateConstructionMenu->LastRejection() == "insufficient_funds") &&
					           snapshot.TeamFunds[snapshot.Players[slot].Team] == uiTest.ConstructionDeploymentFunds - worldStateConstructionMenu->LastChargedCost() &&
					           deployment != snapshot.Objects.end() && deployment->HasEquippedDevice && deployment->EquippedDeviceModuleName == "Coalition.rte" && deployment->EquippedDevicePresetName == "Assault Rifle" &&
					           std::abs(deployment->VelocityY) < 2 && localDeployment && localDeployment->GetEquippedItem() &&
					           localDeployment->GetEquippedItem()->GetModuleName() == "Coalition.rte" && localDeployment->GetEquippedItem()->GetPresetName() == "Assault Rifle" && !localDeployment->GetController()->IsPlayerControlled()) {
						uiTest.ConstructionDeploymentActorId = deployment->NetworkId; uiTest.ConstructionDeploymentPistols = localPistols;
						uiTest.Log << "construction_deployment_ui=passed purchased=" << uiTest.ConstructionExpectedAffordable << '\n' << std::flush;
						worldStateConstructionMenu->SelectBrain(); uiTest.ConstructionTestStage = 18; uiTest.ConstructionActionUpdate = uiTest.SimulationUpdates;
					} else if (uiTest.ConstructionTestStage == 18 && uiTest.SimulationUpdates >= uiTest.ConstructionActionUpdate + 30) {
						const std::string image = UTF8::PathToString(uiTest.OutputDirectory.filename()) + "/debug-run-construction-deployment.png";
						const int saved = g_FrameMan.SaveScreenToPNGBlocking(image);
						uiTest.Log << "construction_deployment_capture=" << (saved == 0 ? "passed" : "failed") << '\n' << std::flush;
						if (saved == 0 && !uiTest.ConstructionExpectedAffordable) { uiTest.ConstructionTestStage = 20; uiTest.ConstructionActionUpdate = uiTest.SimulationUpdates; }
						else if (saved == 0 && deployment != snapshot.Objects.end()) {
							const auto* pistol = dynamic_cast<const SceneObject*>(g_PresetMan.GetEntityPreset("HDFirearm", "Auto Pistol", "Coalition.rte"));
							if (worldStateConstructionMenu->OpenPicker() && worldStateConstructionMenu->GetPicker()->SelectGroupByName("Weapons") && worldStateConstructionMenu->SelectObject(pistol)) {
								worldStateConstructionMenu->GetPicker()->SetEnabled(false);
								if (worldStateConstructionMenu->PlaceObject(*worldStateClient, Vector(deployment->PositionX, deployment->PositionY))) uiTest.ConstructionTestStage = 19;
							}
						}
					} else if (uiTest.ConstructionTestStage == 19 && !worldStateConstructionMenu->IsPending() && worldStateConstructionMenu->LastAccepted() &&
					           worldStateConstructionMenu->LastChargedCost() == 18 && localDeployment && localPistols == uiTest.ConstructionDeploymentPistols + 1 &&
					           snapshot.TeamFunds[snapshot.Players[slot].Team] == uiTest.ConstructionDeploymentFunds - 188 - 18) {
						uiTest.Log << "construction_inventory_gift_ui=passed\n" << std::flush;
						worldStateConstructionMenu->SelectBrain(); uiTest.ConstructionTestStage = 20; uiTest.ConstructionActionUpdate = uiTest.SimulationUpdates;
					} else if (uiTest.ConstructionTestStage == 20 && uiTest.SimulationUpdates >= uiTest.ConstructionActionUpdate + 30) {
						if (worldStateConstructionMenu->SetReady(*worldStateClient, true)) uiTest.ConstructionTestStage = 2;
					} else if (uiTest.ConstructionTestStage == 2 && snapshot.ActivityState == 4 && slot >= 0 && snapshot.Players[slot].ControlledActorId != 0) {
						uiTest.ConstructionTestPassed = true; uiTest.ConstructionTestStage = 3;
						uiTest.Log << "world_state_construction_ui_started=passed\n" << std::flush;
					}
				}
				if (uiTest.RequestBuyMenuTest && uiTest.SimulationUpdates>=60) {
					if (worldStateBuyMenu->IsSpectator()) { uiTest.BuyMenuTestPassed=true; uiTest.BuyMenuTestStage=4; }
					else if (uiTest.BuyMenuTestStage==0 && worldStateBuyMenu->GetMenu() && worldStateBuyMenu->IsPolicyReady()) {
						const auto* loadout=dynamic_cast<const Loadout*>(g_PresetMan.GetEntityPreset("Loadout", "Default", "Coalition.rte"));
						if (loadout) {
							BuyMenuGUI* menu=worldStateBuyMenu->GetMenu();
							menu->GetLoadoutPresets().push_back(*loadout);
							menu->SelectLoadout(static_cast<int>(menu->GetLoadoutPresets().size())-1);
							worldStateBuyMenu->Open(); uiTest.BuyMenuTestStage=1;
						}
					} else if (uiTest.BuyMenuTestStage==1 && uiTest.BuyMenuCatalogCaptured && uiTest.SimulationUpdates>=180 && (!uiTest.RequirePurchasePolicyTest || uiTest.PurchasePolicyTestObserved)) {
						uiTest.BuyMenuDisplayedCost=worldStateBuyMenu->GetMenu()->GetTotalOrderCost();
						uiTest.Log << "buy_menu_displayed_cost=" << uiTest.BuyMenuDisplayedCost << '\n' << std::flush;
						worldStateBuyMenu->ConfirmOrder(); uiTest.BuyMenuTestStage=2;
					} else if (uiTest.BuyMenuTestStage==2 && uiTest.BuyMenuLandingCaptured && uiTest.SimulationUpdates>=240) {
						const auto& snapshot=worldStateClient->GetLatestSnapshot();
						const int team=snapshot.Players[worldStateClient->GetAssignedPlayerSlot()].Team;
						const Scene::Area* area=&worldStateBuyMenu->GetLandingArea();
						if (area && area->GetFirstBox()) {
							const Vector center=area->GetFirstBox()->GetCenter();
							uiTest.BuyMenuInitialFunds=snapshot.TeamFunds[team];
							if (worldStateBuyMenu->ConfirmLanding(*worldStateClient, Vector(center.m_X, std::clamp(center.m_Y, 0.0F, static_cast<float>(g_SceneMan.GetSceneHeight()-1)))))
								uiTest.BuyMenuTestStage=3;
						}
					} else if (uiTest.BuyMenuTestStage==3 && worldStateBuyMenu->LastOrderAccepted()) {
						const auto& snapshot=worldStateClient->GetLatestSnapshot();
						const int team=snapshot.Players[worldStateClient->GetAssignedPlayerSlot()].Team;
						const bool funds=std::abs(snapshot.TeamFunds[team]-(uiTest.BuyMenuInitialFunds-worldStateBuyMenu->LastCharge()))<0.01F;
						const auto& craft=worldStateBuyMenu->LastRequest().Craft;
						const bool delivered=std::any_of(snapshot.Objects.begin(), snapshot.Objects.end(), [&](const auto& object) {
							return object.ClassName==craft.ClassName && object.ModuleName==craft.ModuleName && object.PresetName==craft.PresetName;
						});
						const bool priceMatches=std::abs(worldStateBuyMenu->LastCharge()-uiTest.BuyMenuDisplayedCost)<0.01F;
						if (funds && delivered && priceMatches) { uiTest.BuyMenuTestPassed=true; uiTest.BuyMenuTestStage=4; }
					}
					if (uiTest.RequirePurchasePolicyTest && worldStateBuyMenu->GetMenu()) {
						BuyMenuGUI* menu=worldStateBuyMenu->GetMenu();
						const bool pricingReady=!uiTest.RequirePurchasePricingTest || (menu->GetPricingModule()==g_PresetMan.GetModuleID("Ronin.rte") &&
						    menu->GetPricingForeignMultiplier()==2 && menu->GetPricingNativeMultiplier()==0.5F);
						if (pricingReady && menu->GetOwnedItemsAmount("Coalition.rte/Soldier Light")==2 && menu->GetOwnedItemsAmount("Base.rte/Rocket MK2")== (uiTest.RequirePurchasePricingTest ? 0 : 1) &&
						    menu->IsAllowedItem("Base.rte/Rocket MK2") && menu->IsProhibitedItem("Base.rte/SMG") && menu->IsAlwaysAllowedItem("Base.rte/Brain Case") &&
						    !menu->EnforceMaxMassConstraint() && !menu->EnforceMaxPassengersConstraint() && worldStateBuyMenu->GetLandingArea().IsInsideX(250) &&
						    !worldStateBuyMenu->GetLandingArea().IsInsideX(900)) uiTest.PurchasePolicyTestObserved=true;
					}
				}
			}
			g_MusicMan.Update();

			if (!worldStateReplicaClient) {
				g_ActivityMan.LateUpdateGlobalScripts();
			}
			if (g_PerformanceMan.GetOverlayLevel() == 2) g_PerformanceMan.UpdateSortedScriptTimings(g_LuaMan.GetScriptTimings());
			AdvanceDebugRunSimulation();

			// This is to support hot reloading entities in SceneEditorGUI. It's a bit hacky to put it in Main like this, but PresetMan has no update in which to clear the value, and I didn't want to set up a listener for the job.
			// It's in this spot to allow it to be set by UInputMan update and ConsoleMan update, and read from ActivityMan update.
			g_PresetMan.ClearReloadEntityPresetCalledThisUpdate();

			g_PerformanceMan.StopPerformanceMeasurement(PerformanceMan::SimTotal);
			g_UInputMan.EndFrame();

			if (!g_ActivityMan.IsInActivity()) {
				DebugRunState& debugRunState = GetDebugRunState();
				const bool waitingForHostTransition = debugRunState.RequestHostWorldStateTransition &&
				                                     debugRunState.HostWorldStateTransitionQueued && !debugRunState.HostWorldStateTransitionApplied;
				const bool waitingForClientTransition = debugRunState.RequireWorldStateTransition &&
				                                       debugRunState.WorldStateTransitionObserved && !debugRunState.WorldStateTransitionApplied;
				if (System::IsDebugRun() && (waitingForHostTransition || waitingForClientTransition)) {
					// Activity/Scene transitions briefly clear the active world while their
					// queued replacement is loaded on the next outer loop iteration.
				} else if (System::IsDebugRun()) {
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
int RunCortexCommand(int argc, char** argv) {
	const bool serverMode = std::any_of(argv + 1, argv + argc, [](const char* arg) { return std::string_view(arg) == "--server"; });
	const bool debugRun = !serverMode && ParseDebugRunArguments(argc, argv);
	System::SetHeadlessServer(serverMode);
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
	if (serverMode) {
		if (std::any_of(argv + 1, argv + argc, [](const char* arg) { return std::string_view(arg) == "--help"; })) {
			std::cout << "Cortex Command Reload server\n--config FILE --listen ADDRESS --port PORT --players CONNECTION_LIMIT\n--name NAME --activity-class CLASS --activity PRESET --scene PRESET\n--mods true|false --log-directory DIRECTORY\nConsole: status, help, quit. Ctrl+C or SIGTERM stops the server.\n";
			allegro_exit();
			return EXIT_SUCCESS;
		}
		ServerConfiguration config;
		std::string configError;
		if (!config.ParseArguments(argc, argv, configError)) { std::cerr << "[SERVER][ERROR] " << configError << '\n'; allegro_exit(); return EXIT_FAILURE; }
		const int result = RunPersistentHeadlessServer(config);
		allegro_exit();
		return result;
	}

	if (!StartDebugRun()) {
		allegro_exit();
		return EXIT_FAILURE;
	}
	if (debugRun && GetDebugRunState().PreflightOnly) {
		if (GetDebugRunState().HeadlessSimulationPreflight) {
			std::string configError;
			if (!GetDebugRunState().HeadlessConfig.ParseArguments(argc, argv, configError)) {
				GetDebugRunState().Log << "result=failed reason=server_configuration error=" << configError << '\n' << std::flush;
				allegro_exit();
				return EXIT_FAILURE;
			}
		}
		if (GetDebugRunState().CPUGraphicsPreflight) {
			const bool coreInitialized = SDL_Init(0);
			if (GetDebugRunState().HeadlessContentPreflight) {
				ConstructManagers();
			} else {
			TimerMan::Construct();
			PresetMan::Construct();
			SettingsMan::Construct();
			WindowMan::Construct();
			GLResourceMan::Construct();
			ConsoleMan::Construct();
			FrameMan::Construct();
			}
			g_TimerMan.Initialize();
			g_GLResourceMan.Initialize(false);
			const bool frameInitialized = g_FrameMan.Initialize() == 0;
			bool screenTextApplied = false;
			if (frameInitialized) {
				const std::string message = "HUD: Привет, Ёжик!";
				g_FrameMan.ClearScreenText(3);
				g_FrameMan.SetScreenText("Local minimum-duration lock", 3, 0, 60000);
				g_FrameMan.ApplyNetworkScreenText(message, 250, true, 3);
				screenTextApplied = g_FrameMan.GetScreenText(3) == message &&
				    g_FrameMan.GetScreenTextBlinkInterval(3) == 250 && g_FrameMan.IsScreenTextCentered(3);
				g_FrameMan.ApplyNetworkScreenText("", 0, false, 3);
				screenTextApplied = screenTextApplied && g_FrameMan.GetScreenText(3).empty() && !g_FrameMan.IsScreenTextCentered(3);
			}
			GetDebugRunState().Log << "world_state_screen_text_application=" << (screenTextApplied ? "passed" : "failed") << '\n' << std::flush;
			bool cpuResourcesPassed = false;
			{
				RenderTarget target(FloatRect(0, 0, 23, 41), FloatRect(0, 0, 23, 41));
				target.Begin();
				target.End();
				SceneLayer layer;
				BITMAP* bitmap = create_bitmap_ex(8, 65, 33);
				layer.Create(bitmap, true, Vector(), false, false, Vector(1, 1));
				putpixel(layer.GetBitmap(), 7, 9, 42);
				const auto texture = g_GLResourceMan.GetStaticTextureFromBitmap(layer.GetBitmap());
				BigTexture tiles(layer.GetBitmap());
				tiles.Update(Box(Vector(), 65, 33));
				tiles.Draw(Rectangle{0, 0, 65, 33}, Rectangle{0, 0, 65, 33});
				cpuResourcesPassed = texture.id == 0 && texture.width == 65 && texture.height == 33 &&
				    target.GetFramebuffer() == 0 && target.GetColorTexture().id == 0 && target.GetDepthTexture().id == 0 &&
				    target.GetColorTexture().width == 23 && target.GetColorTexture().height == 41 &&
				    tiles.m_Textures.empty() && getpixel(layer.GetBitmap(), 7, 9) == 42 &&
				    g_GLResourceMan.UpdateDynamicBitmap(layer.GetBitmap(), true) == 0 &&
				    g_GLResourceMan.GetDynamicUploadBuffer(layer.GetBitmap()) == 0 && g_FrameMan.GetBackBuffer8();
			}
			const bool noVideo = SDL_WasInit(SDL_INIT_VIDEO) == 0 && !g_WindowMan.GetWindow();
			bool contentPassed = true;
			if (GetDebugRunState().HeadlessContentPreflight) {
				const std::string loadingLog = UTF8::PathToString(GetDebugRunState().OutputDirectory / "LogLoading.txt");
				contentPassed = InitializeHeadlessContent(GetDebugRunState().HeadlessConfig, loadingLog, GetDebugRunState().LoadAllOptionalModules);
				contentPassed = contentPassed && screenTextApplied && g_PresetMan.GetOfficialModuleCount() > 0 &&
				    g_PresetMan.GetEntityPreset("Scene", "Tutorial Bunker", "Missions.rte") != nullptr &&
				    SDL_WasInit(SDL_INIT_VIDEO) == 0 && !g_WindowMan.GetWindow();
				GetDebugRunState().Log << "headless_content_preflight=" << (contentPassed ? "passed" : "failed")
				    << " modules=" << g_PresetMan.GetTotalModuleCount() << " official_modules=" << g_PresetMan.GetOfficialModuleCount()
				    << " video_initialized=" << (SDL_WasInit(SDL_INIT_VIDEO) != 0) << '\n' << std::flush;
				if (contentPassed) contentPassed = VerifyHeadlessSoundLengths(GetDebugRunState().Log);
				if (contentPassed) {
					// Exercise actual indexed glyph pixels, not just TTF glyph coverage.
					BITMAP* glyphBitmap = create_bitmap_ex(8, 80, 24);
					int foregroundPixels = 0, wrongColorPixels = 0;
					bool rasterPassed = glyphBitmap != nullptr;
					if (glyphBitmap) {
						clear_to_color(glyphBitmap, 0);
						AllegroBitmap target(glyphBitmap);
						GUIFont* font = g_FrameMan.GetLargeFont();
						rasterPassed = font && font->SupportsCodepoint(0x0416);
						if (rasterPassed) font->Draw(&target, 2, 2, "Ж");
						for (int row = 0; row < glyphBitmap->h; ++row) for (int column = 0; column < glyphBitmap->w; ++column) {
							const int pixel = getpixel(glyphBitmap, column, row);
							if (pixel != 0) { ++foregroundPixels; if (pixel != 118) ++wrongColorPixels; }
						}
						destroy_bitmap(glyphBitmap);
					}
					// The checked-in FontLarge atlas has foreground palette index 118.
					rasterPassed = rasterPassed && foregroundPixels > 0 && wrongColorPixels == 0;
					GetDebugRunState().Log << "utf8_raster_color_smoke=" << (rasterPassed ? "passed" : "failed")
					    << " pixels=" << foregroundPixels << " wrong_color=" << wrongColorPixels << '\n' << std::flush;
					contentPassed = contentPassed && rasterPassed;
					bool iconsPassed = true;
					GUIFont* iconFont = g_FrameMan.GetLargeFont();
					BITMAP* rawPixels = create_bitmap_ex(8, 80, 24);
					BITMAP* utf8Pixels = create_bitmap_ex(8, 80, 24);
					BITMAP* atlas = ContentFile("Base.rte/GUIs/Skins/FontLarge.png").GetAsBitmap();
					iconsPassed = iconFont && rawPixels && utf8Pixels && atlas;
					if (iconsPassed) {
						AllegroBitmap rawTarget(rawPixels), utf8Target(utf8Pixels);
						for (unsigned int icon : {200U, 207U, 217U, 219U, 229U}) {
							const std::string raw(1, static_cast<char>(icon));
							std::string encoded;
							UTF8::AppendCodepoint(encoded, icon);
							clear_to_color(rawPixels, 0); clear_to_color(utf8Pixels, 0);
							iconFont->Draw(&rawTarget, 2, 2, raw);
							iconFont->Draw(&utf8Target, 2, 2, encoded);
							iconsPassed = iconsPassed && iconFont->CalculateWidth(raw) == iconFont->CalculateWidth(raw[0]) && iconFont->SupportsCodepoint(icon);
							const int height = iconFont->GetFontHeight();
							const int sourceY = ((icon - 32) / 16) * height;
							const int separator = getpixel(atlas, 0, 0), background = getpixel(atlas, atlas->w - 1, 0);
							int sourceX = 1;
							for (unsigned int column = 0; column < (icon - 32) % 16; ++column) {
								while (sourceX < atlas->w && getpixel(atlas, sourceX, sourceY) != separator) ++sourceX;
								++sourceX;
							}
							int endX = sourceX;
							while (endX < atlas->w && getpixel(atlas, endX, sourceY) != separator) ++endX;
							const int width = endX - sourceX - 1;
							iconsPassed = iconsPassed && iconFont->CalculateWidth(raw) == width + iconFont->GetKerning();
							int visiblePixels = 0, unicodePixels = 0, differences = 0;
							for (int row = 0; row < rawPixels->h; ++row) for (int column = 0; column < rawPixels->w; ++column) {
								const int pixel = getpixel(rawPixels, column, row);
								if (pixel) ++visiblePixels;
								const int unicodePixel = getpixel(utf8Pixels, column, row);
								if (unicodePixel) ++unicodePixels;
								if (pixel != unicodePixel) ++differences;
								int expected = 0;
								if (column >= 2 && column < 2 + width && row >= 2 && row < 2 + height) {
									const int atlasPixel = getpixel(atlas, sourceX + column - 2, sourceY + row - 2);
									if (atlasPixel != background) expected = atlasPixel;
								}
								iconsPassed = iconsPassed && pixel == expected;
							}
							// Atlas slot 207 is the blank spacer in one-handed reload strings.
							iconsPassed = iconsPassed && (icon == 207U ? visiblePixels == 0 : visiblePixels > 0);
							iconsPassed = iconsPassed && unicodePixels > 0 && differences > 0;
							GetDebugRunState().Log << "legacy_hud_icon=" << icon << " width=" << iconFont->CalculateWidth(raw)
							    << " byte_width=" << iconFont->CalculateWidth(raw[0]) << " encoded_width=" << iconFont->CalculateWidth(encoded)
							    << " pixels=" << visiblePixels << " unicode_pixels=" << unicodePixels << " differences=" << differences << '\n';
						}
						std::string missingGlyph;
						UTF8::AppendCodepoint(missingGlyph, 0x10FFFF);
						clear_to_color(rawPixels, 0); clear_to_color(utf8Pixels, 0);
						iconFont->Draw(&rawTarget, 2, 2, "?");
						iconFont->Draw(&utf8Target, 2, 2, missingGlyph);
						bool fallbackPassed = !iconFont->SupportsCodepoint(0x10FFFF) && iconFont->CalculateWidth(missingGlyph) == iconFont->CalculateWidth("?");
						for (int row = 0; row < rawPixels->h; ++row) for (int column = 0; column < rawPixels->w; ++column)
							fallbackPassed = fallbackPassed && getpixel(rawPixels, column, row) == getpixel(utf8Pixels, column, row);
						GetDebugRunState().Log << "utf8_missing_glyph_fallback=" << (fallbackPassed ? "passed" : "failed") << '\n' << std::flush;
						iconsPassed = iconsPassed && fallbackPassed;
					}
					if (rawPixels) destroy_bitmap(rawPixels);
					if (utf8Pixels) destroy_bitmap(utf8Pixels);
					GetDebugRunState().Log << "legacy_hud_icon_raster=" << (iconsPassed ? "passed" : "failed") << '\n' << std::flush;
					contentPassed = contentPassed && iconsPassed;
					BITMAP* wrappedPixels = create_bitmap_ex(8, 160, 120);
					BITMAP* explicitPixels = create_bitmap_ex(8, 160, 120);
					bool wrapPassed = iconFont && wrappedPixels && explicitPixels;
					if (wrapPassed) {
						AllegroBitmap wrappedTarget(wrappedPixels), explicitTarget(explicitPixels);
						const std::string text = "Ж ЖЖ Ж\nÈ Ï";
						const int width = std::max(iconFont->CalculateWidth("ЖЖ"), iconFont->CalculateWidth("È Ï"));
						const int height = iconFont->GetFontHeight();
						clear_to_color(wrappedPixels, 0); clear_to_color(explicitPixels, 0);
						iconFont->DrawAligned(&wrappedTarget, 2, 2, text, GUIFont::Left, GUIFont::Top, width);
						const std::vector<std::string> expected{"Ж", "ЖЖ", "Ж", "È Ï"};
						for (std::size_t line = 0; line < expected.size(); ++line) iconFont->Draw(&explicitTarget, 2, 2 + static_cast<int>(line) * height, expected[line]);
						wrapPassed = iconFont->CalculateHeight(text, width) == 4 * height &&
						    iconFont->CalculateHeight("\n\n", width) == 3 * height && iconFont->CalculateHeight("", width) == 0 &&
						    iconFont->CalculateWidth("\t") == 4 * iconFont->CalculateWidth(' ');
						for (int row = 0; row < wrappedPixels->h; ++row) for (int column = 0; column < wrappedPixels->w; ++column)
							wrapPassed = wrapPassed && getpixel(wrappedPixels, column, row) == getpixel(explicitPixels, column, row);
					}
					if (wrappedPixels) destroy_bitmap(wrappedPixels);
					if (explicitPixels) destroy_bitmap(explicitPixels);
					GetDebugRunState().Log << "utf8_wrapped_raster_layout=" << (wrapPassed ? "passed" : "failed") << '\n' << std::flush;
					contentPassed = contentPassed && wrapPassed;
					std::size_t unicodeCoverage = 0;
					for (std::uint32_t cp = 128; cp < 0x3000 && unicodeCoverage < 600; ++cp) {
						if (iconFont->SupportsCodepoint(cp)) ++unicodeCoverage;
					}
					const bool unicodeCachePassed = unicodeCoverage >= 600 && iconFont->SupportsCodepoint(0x416) && iconFont->SupportsCodepoint(0x401);
					GetDebugRunState().Log << "utf8_glyph_cache_coverage=" << (unicodeCachePassed ? "passed" : "failed") << " supported=" << unicodeCoverage << '\n' << std::flush;
					contentPassed = contentPassed && unicodeCachePassed;
					GUITextPanel textInputProbe;
					textInputProbe.Create(0, 0, 300, 24);
					textInputProbe.SetFont(iconFont);
					textInputProbe.SetMaxTextLength(3);
					textInputProbe.OnTextInput("Ж🙂AB");
					bool textInputPassed = textInputProbe.GetText() == "Ж🙂A";
					textInputProbe.SetCursorPos(6);
					textInputProbe.OnKeyPress(GUIInput::Key_Backspace, 0);
					textInputPassed = textInputPassed && textInputProbe.GetText() == "ЖA";
					textInputProbe.SetCursorPos(0);
					textInputProbe.OnKeyPress(GUIInput::Key_Delete, 0);
					textInputPassed = textInputPassed && textInputProbe.GetText() == "A";
					textInputProbe.SetText("Ж🙂A");
					textInputProbe.SetSelection(2, 6);
					textInputProbe.OnTextInput("БВ");
					textInputPassed = textInputPassed && textInputProbe.GetText() == "ЖБA";
					textInputProbe.SetLocked(true);
					textInputProbe.OnTextInput("C");
					textInputPassed = textInputPassed && textInputProbe.GetText() == "ЖБA";
					GUITextPanel numericProbe;
					numericProbe.SetNumericOnly(true);
					numericProbe.SetMaxNumericValue(8000);
					numericProbe.OnTextInput("Ж12\n34");
					textInputPassed = textInputPassed && numericProbe.GetText() == "1234";
					numericProbe.SetText("999999999999999999999");
					numericProbe.SetCursorPos(static_cast<int>(numericProbe.GetText().size()));
					numericProbe.OnTextInput("9");
					textInputPassed = textInputPassed && numericProbe.GetText() == "8000";
					GetDebugRunState().Log << "utf8_text_input_policy=" << (textInputPassed ? "passed" : "failed") << '\n' << std::flush;
					contentPassed = contentPassed && textInputPassed;
					GUITextPanel selectionProbe;
					selectionProbe.SetText("Ж🙂A");
					selectionProbe.SetSelection(1, 5);
					bool selectionPassed = selectionProbe.GetSelectionText() == "Ж";
					selectionProbe.SetSelection(5, 1);
					selectionPassed = selectionPassed && selectionProbe.GetSelectionText() == "Ж";
					selectionProbe.SetSelection(-100, 100);
					selectionPassed = selectionPassed && selectionProbe.GetSelectionText() == "Ж🙂A";
					selectionProbe.SetSelection(3, 6);
					selectionPassed = selectionPassed && selectionProbe.GetSelectionText() == "🙂";
					selectionProbe.RemoveSelectionText();
					selectionPassed = selectionPassed && selectionProbe.GetText() == "ЖA" && UTF8::IsValid(selectionProbe.GetText());
					selectionProbe.SetText("Ж🙂A");
					selectionProbe.SetCursorPos(5);
					selectionProbe.OnTextInput("Б");
					selectionPassed = selectionPassed && selectionProbe.GetText() == "ЖБ🙂A";
					selectionProbe.SetText(std::string("\x80\x81") + "A");
					selectionProbe.SetCursorPos(1);
					selectionProbe.OnTextInput("B");
					selectionPassed = selectionPassed && selectionProbe.GetText() == std::string("\x80") + "B" + "\x81" + "A";
					GetDebugRunState().Log << "utf8_selection_boundaries=" << (selectionPassed ? "passed" : "failed") << '\n' << std::flush;
					contentPassed = contentPassed && selectionPassed;
				}
				if (contentPassed && GetDebugRunState().HeadlessSimulationPreflight) {
					// A remote slot can exist before its presentation menus are initialized.
					// Reject such deliveries and hostile indices without reading player/team arrays.
					bool deliveryGuardsPassed = true;
					{
						GameActivity unstartedActivity;
						unstartedActivity.SetTeamOfPlayer(Players::PlayerOne, Activity::Teams::TeamOne);
						deliveryGuardsPassed = !unstartedActivity.CreateDelivery(Players::PlayerOne);
						for (int invalidPlayer : {-1, static_cast<int>(Players::MaxPlayerCount), std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
							unstartedActivity.ClearOverridePurchase(invalidPlayer);
							deliveryGuardsPassed = deliveryGuardsPassed && !unstartedActivity.CreateDelivery(invalidPlayer) &&
							    !unstartedActivity.GetBuyGUI(static_cast<unsigned int>(invalidPlayer)) &&
							    unstartedActivity.SetOverridePurchaseList(static_cast<const Loadout*>(nullptr), invalidPlayer) == 0 &&
							    unstartedActivity.SetOverridePurchaseList("Default", invalidPlayer) == 0;
						}
						for (int invalidTeam : {-1, static_cast<int>(Activity::Teams::MaxTeamCount), std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
							deliveryGuardsPassed = deliveryGuardsPassed && unstartedActivity.GetDeliveryCount(invalidTeam) == 0;
						}
					}
					GetDebugRunState().Log << "delivery_prerequisite_guards=" << (deliveryGuardsPassed ? "passed" : "failed") << '\n' << std::flush;
					contentPassed = contentPassed && deliveryGuardsPassed;
					const ServerConfiguration& serverConfig = GetDebugRunState().HeadlessConfig;
					worldStateServerActivityClass = serverConfig.ActivityClass;
					worldStateServerActivityPreset = serverConfig.ActivityPreset;
					worldStateServerScenePreset = serverConfig.ScenePreset;
					worldStateServerTeamTech = serverConfig.TeamTech;
					std::string activityError;
					const bool started = ConfigureWorldStateHostActivity(activityError) && g_ActivityMan.RestartActivity();
					if (!activityError.empty()) GetDebugRunState().Log << "headless_activity_error=" << activityError << '\n' << std::flush;
					GetDebugRunState().Log << "headless_activity_started=" << started << '\n' << std::flush;
					MOPixel* probeParticle = nullptr;
					if (started) {
						g_ActivityMan.ResumeActivity();
						if (GetDebugRunState().DeliveryValidationTest) {
							auto* activity = dynamic_cast<GameActivity*>(g_ActivityMan.GetActivity());
							const bool passed = activity && RunDeliveryValidationTest(*activity, GetDebugRunState().Log);
							GetDebugRunState().Log << "delivery_validation=" << (passed ? "passed" : "failed") << '\n' << std::flush;
							contentPassed = contentPassed && passed;
						}
						if (const Entity* preset = g_PresetMan.GetEntityPreset("MOPixel", "Gold Particle", "Base.rte")) {
							probeParticle = dynamic_cast<MOPixel*>(preset->Clone());
						}
					}
					const Vector initialPosition(400, 30);
					if (probeParticle) {
						probeParticle->SetPos(initialPosition);
						probeParticle->SetVel(Vector(15, -5));
						probeParticle->SetLifetime(10000);
					    g_MovableMan.AddParticle(probeParticle);
					}
					std::unique_ptr<WorldStateServerSession> headlessServer;
					bool serverStarted = true;
					bool sawClient = false;
					int disconnectUpdate = -1;
					if (started && GetDebugRunState().HeadlessServerPreflight) {
						headlessServer = std::make_unique<WorldStateServerSession>();
						serverStarted = headlessServer->Start(serverConfig.ListenAddress, serverConfig.Port, serverConfig.Players,
						    UTF8::PathToString(GetDebugRunState().OutputDirectory / "WorldStateServer.log"), true, false);
						GetDebugRunState().Log << "headless_server_config name=" << serverConfig.Name << " listen=" << serverConfig.ListenAddress
						    << " players=" << serverConfig.Players << " activity=" << serverConfig.ActivityClass << '/' << serverConfig.ActivityPreset
						    << " scene=" << serverConfig.ScenePreset << '\n' << std::flush;
						GetDebugRunState().Log << "headless_server_ready=" << serverStarted << " port=" << headlessServer->GetBoundPort() << '\n' << std::flush;
					}
					int updates = 0;
					bool particleMoved = false;
					const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(headlessServer ? 300 : 60);
					while (started && serverStarted && updates < GetDebugRunState().UpdateLimit && std::chrono::steady_clock::now() < deadline) {
						g_TimerMan.Update();
						if (headlessServer) {
							headlessServer->PollNetwork(static_cast<std::uint32_t>(updates));
							if (headlessServer->GetConnectedClientCount() > 0) sawClient = true;
							else if (sawClient && disconnectUpdate < 0) disconnectUpdate = updates;
						}
						if (!g_TimerMan.TimeForSimUpdate()) {
							std::this_thread::sleep_for(std::chrono::milliseconds(1));
							continue;
						}
						g_TimerMan.UpdateSim();
						StepHeadlessSimulation(headlessServer.get(), static_cast<std::uint32_t>(updates + 1));
						if (probeParticle && g_MovableMan.ValidMO(probeParticle)) {
							particleMoved = particleMoved || (probeParticle->GetPos() - initialPosition).GetMagnitude() > 1.0F;
						}
						++updates;
					}
					g_MovableMan.CompleteQueuedMOIDDrawings();
					g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
					g_ThreadMan.GetBackgroundThreadPool().wait_for_tasks();
					const bool replicaSpawnPassed = started && WorldStateClientReplica::TestBackgroundEquipment();
					GetDebugRunState().Log << "headless_replica_spawn=" << (replicaSpawnPassed ? "passed" : "failed") << '\n' << std::flush;
					contentPassed = started && replicaSpawnPassed && particleMoved && updates == GetDebugRunState().UpdateLimit &&
					    g_TimerMan.GetSimUpdateCount() >= updates && SDL_WasInit(SDL_INIT_VIDEO) == 0 && !g_WindowMan.GetWindow();
					if (!headlessServer && contentPassed) {
						WorldStateServerSession loopback;
						const bool connected = loopback.Start("127.0.0.1", 0, 4, UTF8::PathToString(GetDebugRunState().OutputDirectory / "SelfTestNetwork.log"), true) &&
						    VerifyWorldStateHostSessionLoopback(loopback, GetDebugRunState().Log, true);
						loopback.Stop(); contentPassed = contentPassed && connected;
					}
					if (headlessServer) {
						const bool networkPassed = serverStarted && sawClient && disconnectUpdate >= 0 && updates - disconnectUpdate >= 60 &&
						    headlessServer->GetSnapshotBroadcastCount() > 0;
						contentPassed = contentPassed && networkPassed;
						GetDebugRunState().Log << "headless_server_network=" << (networkPassed ? "passed" : "failed")
						    << " saw_client=" << sawClient << " snapshots=" << headlessServer->GetSnapshotBroadcastCount()
						    << " input_commands=" << headlessServer->GetInputCommandCount() << " updates_after_disconnect="
						    << (disconnectUpdate >= 0 ? updates - disconnectUpdate : 0) << '\n' << std::flush;
						headlessServer->Stop();
					}
					GetDebugRunState().Log << "headless_simulation_preflight=" << (contentPassed ? "passed" : "failed")
					    << " updates=" << updates << " particle_moved=" << particleMoved << " actors=" << g_MovableMan.GetActorCount()
					    << " video_initialized=" << (SDL_WasInit(SDL_INIT_VIDEO) != 0) << '\n' << std::flush;
				}
				g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
				g_ThreadMan.GetBackgroundThreadPool().wait_for_tasks();
				g_ConsoleMan.SaveAllText(UTF8::PathToString(GetDebugRunState().OutputDirectory / "LogConsole.txt"));
				g_MovableMan.Destroy();
				g_SceneMan.Destroy();
				g_ActivityMan.Destroy();
				g_PresetMan.Destroy();
				g_LuaMan.Destroy();
				g_LoadingScreen.Destroy();
			}
			g_FrameMan.Destroy();
			if (GetDebugRunState().HeadlessContentPreflight) ContentFile::FreeAllLoaded();
			g_GLResourceMan.Destroy();
			g_WindowMan.Destroy();
			g_WindowMan.Destroy();
			SDL_Quit();
			const bool passed = coreInitialized && frameInitialized && cpuResourcesPassed && noVideo && contentPassed;
			GetDebugRunState().Log << "cpu_graphics_preflight=" << (passed ? "passed" : "failed")
			    << " video_initialized=" << !noVideo << " gpu_enabled=" << g_GLResourceMan.IsGPUEnabled() << '\n' << std::flush;
			if (!passed) return EXIT_FAILURE;
		}
		const std::string& probe = GetDebugRunState().ErrorProbe;
		if (probe == "assert") RTEError::AssertFunc("Diagnostic failure probe: Привет", std::source_location::current());
		else if (probe == "abort") RTEError::AbortFunc("Diagnostic failure probe: Привет", std::source_location::current());
		else if (probe == "exception") RTEError::UnhandledExceptionFunc("Diagnostic failure probe: Привет", "diagnostic-probe-callstack");
		else if (!probe.empty()) return EXIT_FAILURE;
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
	if (!debugRun && std::filesystem::exists(UTF8::PathFromString("Base.rte/gamecontrollerdb.txt"))) {
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
	const bool includeOptionalModules = !debugRun || GetDebugRunState().LoadAllOptionalModules;
	if (debugRun) {
		GetDebugRunState().Log << "optional_module_scan=" << (includeOptionalModules ? "enabled" : "disabled") << '\n' << std::flush;
	}
	g_PresetMan.LoadAllDataModules(includeOptionalModules);
	if (debugRun) {
		GetDebugRunState().ModuleLoadEndTime = std::chrono::steady_clock::now();
		GetDebugRunState().SimulationStartTime = GetDebugRunState().ModuleLoadEndTime;
		GetDebugRunState().Log << "stage=modules_loaded\n" << std::flush;

		// Exercise Lua's real file-loading path with a temporary Cyrillic module
		// directory and filename. Only create a uniquely named directory when it
		// does not already exist; cleanup removes only the exact files created here.
		DebugRunState& state = GetDebugRunState();
		const std::string probeDirectoryName = state.OutputDirectoryName + "_LuaProbe.rte";
		const std::filesystem::path probeDirectory = UTF8::PathFromString(System::GetWorkingDirectory()) /
		                                            UTF8::PathFromString(System::GetModDirectory()) /
		                                            UTF8::PathFromString(probeDirectoryName);
		const std::filesystem::path probeScriptPath = probeDirectory / std::filesystem::u8path("проверка.lua");
		const std::filesystem::path probeTargetPath = probeDirectory / std::filesystem::u8path("цель.lua");
		const std::filesystem::path probePackagePath = probeDirectory / std::filesystem::u8path("UTF8_Пакет.lua");
		const std::string probeModulePath = UTF8::PathToString(UTF8::PathFromString(System::GetModDirectory()) /
	                                                       UTF8::PathFromString(probeDirectoryName) /
	                                                       std::filesystem::u8path("проверка.lua"));
		const std::string probeTargetModulePath = UTF8::PathToString(UTF8::PathFromString(System::GetModDirectory()) /
	                                                              UTF8::PathFromString(probeDirectoryName) /
	                                                              std::filesystem::u8path("цель.lua"));
		const std::string probePackageTemplate = UTF8::PathToString(UTF8::PathFromString(System::GetModDirectory()) /
	                                                             UTF8::PathFromString(probeDirectoryName) / std::filesystem::u8path("?.lua"));
		std::error_code probeError;
		const bool probeDirectoryAlreadyExists = std::filesystem::exists(probeDirectory, probeError);
		bool probeDirectoryCreated = false;
		if (!probeDirectoryAlreadyExists && !probeError) {
			probeDirectoryCreated = std::filesystem::create_directory(probeDirectory, probeError);
		}
		auto writeLuaProbeFile = [](const std::filesystem::path& path, std::string_view source) {
			std::ofstream sourceFile(UTF8::PathFromString(UTF8::PathToString(path)), std::ios::binary | std::ios::trunc);
			if (!sourceFile) {
				return false;
			}
			sourceFile.write(source.data(), static_cast<std::streamsize>(source.size()));
			sourceFile.close();
			return static_cast<bool>(sourceFile);
		};
		auto runLuaSourceProbe = [&](std::string_view source, std::string_view expectedError) {
			if (!probeDirectoryCreated) {
				return false;
			}
			if (!writeLuaProbeFile(probeScriptPath, source)) {
				return false;
			}
			LuaStateWrapper& luaState = g_LuaMan.GetMasterScriptState();
			const int runResult = luaState.RunScriptFile(probeModulePath, false, false);
			const std::string luaError = luaState.GetLastError();
			luaState.ClearErrors();
			return runResult < 0 && luaError.find(expectedError) != std::string::npos;
		};
		const std::string utf8LuaSource = "error(\"UTF8_LUA_PATH_PROBE: Привет\")";
		const bool utf8LuaPathPassed = runLuaSourceProbe(utf8LuaSource, "UTF8_LUA_PATH_PROBE: Привет");
		const std::string bomLuaSource = std::string("\xEF\xBB\xBF") + "error(\"UTF8_LUA_BOM_PROBE: Привет\")";
		const bool luaBOMPassed = runLuaSourceProbe(bomLuaSource, "UTF8_LUA_BOM_PROBE: Привет");
		const std::string legacyLuaSource = std::string("error(\"CP1251_LUA_PROBE: ") + "\xCF\xF0\xE8\xE2\xE5\xF2" + "\")";
		const bool legacyLuaSourcePassed = runLuaSourceProbe(legacyLuaSource, "CP1251_LUA_PROBE: Привет");
		const std::string loadTargetSource = "return { text = \"Lua-файл загружен\" }";
		const std::string packageTargetSource = "return { text = \"Lua-пакет загружен\" }";
		const bool probeTargetsWritten = writeLuaProbeFile(probeTargetPath, loadTargetSource) && writeLuaProbeFile(probePackagePath, packageTargetSource);
		bool luaFilesystemPathPassed = false;
		if (probeTargetsWritten) {
			const std::string scratchName = "проверка-файловой-системы.txt";
			const std::string renamedScratchName = "переименованный-файл.txt";
			const std::string subdirectoryName = "проверка-подкаталога";
			const std::string scratchRelativePath = probeDirectoryName + "/" + scratchName;
			const std::string renamedScratchRelativePath = probeDirectoryName + "/" + renamedScratchName;
			const std::string subdirectoryRelativePath = probeDirectoryName + "/" + subdirectoryName;
			const std::string scratchContents = "utf8-path-probe-Привет\n";
			const bool scratchWritten = writeLuaProbeFile(probeDirectory / std::filesystem::u8path(scratchName), scratchContents);
			const bool subdirectoryCreated = g_LuaMan.DirectoryCreate(subdirectoryRelativePath, false);
			bool listedUnicodeFile = false;
			if (const std::vector<std::string>* files = g_LuaMan.FileList(System::GetModDirectory() + probeDirectoryName)) {
				listedUnicodeFile = std::find(files->begin(), files->end(), scratchName) != files->end();
				delete files;
			}
			bool listedUnicodeDirectory = false;
			if (const std::vector<std::string>* directories = g_LuaMan.DirectoryList(System::GetModDirectory() + probeDirectoryName)) {
				listedUnicodeDirectory = std::find(directories->begin(), directories->end(), subdirectoryName) != directories->end();
				delete directories;
			}
			const bool unicodeExistsChecksPassed = g_LuaMan.FileExists(scratchRelativePath) && g_LuaMan.DirectoryExists(subdirectoryRelativePath);
			bool unicodeFileReadPassed = false;
			const int fileIndex = g_LuaMan.FileOpen(scratchRelativePath, "r");
			if (fileIndex >= 0) {
				unicodeFileReadPassed = g_LuaMan.FileReadLine(fileIndex) == scratchContents;
				g_LuaMan.FileClose(fileIndex);
			}
			const bool renamed = scratchWritten && g_LuaMan.FileRename(scratchRelativePath, renamedScratchRelativePath);
			const bool renameChecksPassed = renamed && !g_LuaMan.FileExists(scratchRelativePath) && g_LuaMan.FileExists(renamedScratchRelativePath);
			const bool removed = g_LuaMan.FileRemove(renamed ? renamedScratchRelativePath : scratchRelativePath);
			const bool subdirectoryRemoved = subdirectoryCreated && g_LuaMan.DirectoryRemove(subdirectoryRelativePath, false);
			luaFilesystemPathPassed = scratchWritten && subdirectoryCreated && listedUnicodeFile && listedUnicodeDirectory && unicodeExistsChecksPassed &&
			                          unicodeFileReadPassed && renameChecksPassed && removed && subdirectoryRemoved &&
			                          !g_LuaMan.FileExists(scratchRelativePath) && !g_LuaMan.FileExists(renamedScratchRelativePath) &&
			                          !g_LuaMan.DirectoryExists(subdirectoryRelativePath);
			std::error_code scratchCleanupError;
			std::filesystem::remove(probeDirectory / std::filesystem::u8path(scratchName), scratchCleanupError);
			if (renamed) {
				scratchCleanupError.clear();
				std::filesystem::remove(probeDirectory / std::filesystem::u8path(renamedScratchName), scratchCleanupError);
			}
			if (subdirectoryCreated) {
				scratchCleanupError.clear();
				std::filesystem::remove(probeDirectory / std::filesystem::u8path(subdirectoryName), scratchCleanupError);
			}
			luaFilesystemPathPassed = luaFilesystemPathPassed && !scratchCleanupError;
		}
		state.LuaFilesystemProbePassed = luaFilesystemPathPassed;
		state.Log << "lua_filesystem_utf8_path_probe=" << (luaFilesystemPathPassed ? "passed" : "failed") << '\n' << std::flush;
		const std::string loadfileRunner = "local chunk, err = loadfile(\"" + probeTargetModulePath + "\"); "
		                                   "assert(chunk, err); local value = chunk(); assert(value.text == \"Lua-файл загружен\"); "
		                                   "error(\"UTF8_LUA_LOADFILE_PROBE: Привет\")";
		const std::string dofileRunner = "local value = dofile(\"" + probeTargetModulePath + "\"); "
		                                 "assert(value.text == \"Lua-файл загружен\"); error(\"UTF8_LUA_DOFILE_PROBE: Привет\")";
		const std::string requireRunner = "local key = \"UTF8_Пакет\"; local oldPath = package.path; local oldValue = package.loaded[key]; "
		                                  "package.path = \"" + probePackageTemplate + "\"; package.loaded[key] = nil; "
		                                  "local ok, value = pcall(require, key); package.path = oldPath; package.loaded[key] = oldValue; "
		                                  "if not ok then error(value); end; assert(value.text == \"Lua-пакет загружен\"); "
		                                  "error(\"UTF8_LUA_REQUIRE_PROBE: Привет\")";
		const bool luaLoadfilePassed = probeTargetsWritten && runLuaSourceProbe(loadfileRunner, "UTF8_LUA_LOADFILE_PROBE: Привет");
		const bool luaDofilePassed = probeTargetsWritten && runLuaSourceProbe(dofileRunner, "UTF8_LUA_DOFILE_PROBE: Привет");
		const bool luaRequirePassed = probeTargetsWritten && runLuaSourceProbe(requireRunner, "UTF8_LUA_REQUIRE_PROBE: Привет");
		if (probeDirectoryCreated) {
			probeError.clear();
			std::filesystem::remove(probeScriptPath, probeError);
			bool probeFilesRemoved = !probeError;
			for (const std::filesystem::path& probeFilePath : {probeTargetPath, probePackagePath}) {
				probeError.clear();
				std::filesystem::remove(probeFilePath, probeError);
				probeFilesRemoved = probeFilesRemoved && !probeError;
			}
			probeError.clear();
			std::filesystem::remove(probeDirectory, probeError);
			state.Log << "lua_utf8_source_path_probe=" << (utf8LuaPathPassed && probeFilesRemoved ? "passed" : "failed") << '\n'
			          << "lua_utf8_bom_probe=" << (luaBOMPassed && probeFilesRemoved ? "passed" : "failed") << '\n'
			          << "lua_legacy_windows1251_source_probe=" << (legacyLuaSourcePassed && probeFilesRemoved ? "passed" : "failed") << '\n'
			          << "lua_loadfile_utf8_probe=" << (luaLoadfilePassed ? "passed" : "failed") << '\n'
			          << "lua_dofile_utf8_probe=" << (luaDofilePassed ? "passed" : "failed") << '\n'
			          << "lua_require_utf8_probe=" << (luaRequirePassed ? "passed" : "failed") << '\n'
			          << "lua_probe_cleanup=" << (!probeError ? "passed" : "failed") << '\n' << std::flush;
		} else {
			state.Log << "lua_utf8_source_path_probe=failed reason="
			          << (probeDirectoryAlreadyExists ? "probe_directory_already_exists" : "probe_directory_create_failed") << '\n' << std::flush;
		}
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
		while (!worldStateClient->HasSnapshot() && worldStateClient->GetConnectionError().empty() && std::chrono::steady_clock::now() < connectionDeadline) {
			worldStateClient->Update();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		if (!worldStateClient->HasSnapshot() || !worldStateClient->IsConnected()) {
			std::cerr << "[NETWORK] " << (worldStateClient->GetConnectionError().empty() ? "No valid world snapshot arrived within 15 seconds" : worldStateClient->GetConnectionError()) << '\n';
			worldStateSoundPlayback.Clear();
		worldStateClient.reset();
			DestroyManagers();
			allegro_exit();
			SDL_Quit();
			return EXIT_FAILURE;
		}
		const WorldStateProtocol::Snapshot& snapshot = worldStateClient->GetLatestSnapshot();
		if (debugRun && GetDebugRunState().RequireRemoteEquipmentMutationSmoke) {
			DebugRunState& state = GetDebugRunState();
			state.PreviousMagazineRoundsByNetworkId.reserve(snapshot.Objects.size());
			for (const WorldStateProtocol::ObjectState& object : snapshot.Objects) {
				if (object.HasEquippedDeviceMagazine) {
					state.PreviousMagazineRoundsByNetworkId.emplace(object.NetworkId, object.EquippedDeviceMagazineRounds);
				}
			}
			state.Log << "world_state_remote_equipment_baseline=true snapshot_objects=" << snapshot.Objects.size()
			         << " magazines=" << state.PreviousMagazineRoundsByNetworkId.size() << '\n' << std::flush;
		}
		std::string failureReason;
		if (!QueueWorldStateActivity(snapshot, failureReason)) {
			std::cerr << "[NETWORK] " << failureReason << "; verify installed .rte modules\n";
			worldStateSoundPlayback.Clear();
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
	if (worldStateClientAddress.empty() && (debugRun || worldStateServerRequested)) {
		if (debugRun) {
			GetDebugRunState().Log << "stage=server_activity_config_begin class=" << worldStateServerActivityClass
			                      << " preset=" << worldStateServerActivityPreset << " scene=" << worldStateServerScenePreset << '\n' << std::flush;
		}
		std::string activityConfigurationError;
		if (!ConfigureWorldStateHostActivity(activityConfigurationError)) {
			std::cerr << "[SERVER] " << activityConfigurationError << '\n';
			if (debugRun) {
				GetDebugRunState().Log << "stage=server_activity_config_failed reason=" << activityConfigurationError << '\n' << std::flush;
			}
			// Keep teardown on the normal main() path. Early destruction here left
			// static/runtime state alive and triggered abort() during CRT shutdown.
			worldStateServerConfigurationFailed = true;
			worldStateServerRequested = false;
			g_ActivityMan.SetStartTutorialActivity();
			g_ActivityMan.SetRestartActivity();
			System::SetQuit(true);
		}
		if (debugRun && !worldStateServerConfigurationFailed) {
			GetDebugRunState().Log << "stage=server_activity_config_complete\n" << std::flush;
		}
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
			const std::filesystem::path loadingWarningPath = UTF8::PathFromString(System::GetWorkingDirectory()) / "LogLoadingWarning.txt";
			if (std::filesystem::exists(loadingWarningPath)) {
				std::filesystem::remove(loadingWarningPath);
			}
		}

		if (debugRun) {
			GetDebugRunState().Log << "stage=activity_initialize_begin\n" << std::flush;
		}
		const bool activityInitialized = g_ActivityMan.Initialize();
		if (debugRun) {
			GetDebugRunState().Log << "stage=activity_initialize_complete result=" << activityInitialized << '\n' << std::flush;
		}
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
			const unsigned short serverPort = debugRun && !worldStateServerPortSpecified ? 0 : worldStateServerPort;
			const std::string serverLogPath = debugRun ? UTF8::PathToString(GetDebugRunState().OutputDirectory / "WorldStateServer.log") : "WorldStateServer.log";
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

		if (debugRun && !MainMenuGUI::DebugConnectionAddress.empty()) RunMenuLoop();
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
		worldStateClientLog << "simulation_authority=server_snapshot_replica\n"
		                    << "remote_replica_ticks=" << worldStateClientReplicaSimulationTicks << '\n'
		                    << "client_local_simulation_ticks=" << worldStateClientLocalSimulationTicks << '\n';
		worldStateClientLog.close();
	}
	if (worldStateClient) {
		worldStateClient->Disconnect();
		worldStateSoundPlayback.Clear();
		worldStateClient.reset();
		g_FrameMan.SetClientGUIRenderer({});
		worldStateBuyMenu.reset(); worldStateConstructionMenu.reset();
	}
	DiscordPresence::Shutdown();

	DestroyManagers();

	allegro_exit();
	SDL_Quit();

	if (worldStateServerConfigurationFailed && debugRun) {
		GetDebugRunState().Log << "result=failed reason=invalid_world_state_activity_configuration\n" << std::flush;
	}
	return (worldStateServerConfigurationFailed || (debugRun && !GetDebugRunState().Passed)) ? EXIT_FAILURE : EXIT_SUCCESS;
}
