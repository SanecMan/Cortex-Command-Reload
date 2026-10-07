# Automated debug run

`Tools/RunDedicatedServerSmoke.ps1 -CheckBuyMenu` runs the client equipment menu
fixture with an active player and spectator. The player opens the real catalog,
selects an official loadout through GUI APIs, confirms a landing point and waits
for authoritative delivery/funds state. Additional `debug-run-buy-catalog.png`
and `debug-run-buy-landing.png` images are captured. Confirmation uses the same
GUI checks as the button, but does not inject physical mouse/keyboard events.

For the v11 purchase RPC, run `Tools/RunDedicatedServerSmoke.ps1 -CheckPurchases`.
It launches a real headless server and two clients. One player exercises request
validation, purchase, replay and stale ID rejection; the spectator must be
rejected. Client `-debug-run-world-state-purchase` enables this explicit fixture.
Reports include `world_state_purchase_codec`, `purchase_reply` and
`world_state_purchase`. This fixture is specific to Tutorial's static landing
zone and uses console funding, not a visible buy menu. Repeated runs replace
their previous diagnostic artifacts.

The explicit delivery/economy fixture runs headless:

```powershell
& '.\Cortex Command.exe' -debug-run 600 -debug-run-delivery-validation -debug-run-output ValidatedDelivery
```

It loads a disposable Tutorial session, verifies rejection without changing
funds/stock/landing zone/queue, places a validated order with duplicate cargo and
checks that a pending Lua override survives. It then runs the requested number
of simulation steps. This fixture changes only its own session, does not save
it, and does not create screenshots. See [DedicatedServer.md](DedicatedServer.md)
for its validation scope and current networking limitations.

Run the Windows client from the game directory:

```powershell
& '.\Cortex Command.exe' -debug-run 600
```

The optional number is the target number of simulation updates (60–36,000; default 600). The run creates a hidden SDL window, disables audio, loads the built-in Tutorial Bunker activity, spawns actor and particle batches at the start and halfway point, records module-load time, frame/simulation/render/Lua/AI/object counters and process resident memory, captures the final world state, broadcasts it through local RakNet server/client peers, and saves initial/stress/final screenshots before exiting. The transport smoke connects four clients, then verifies each received and decoded the same live snapshot. A run with `-world-state-client` does not spawn local stress objects: it checks that server-owned replicas appear in MovableMan's active lists and that local simulation stays disabled. Before loading content it verifies UTF-8 helpers and creates/reads/removes a Cyrillic and emoji filename through the native file API. During rendering, it checks that the bundled TrueType font includes Cyrillic glyphs and draws `UTF-8 test: Привет, Ёжик!` through the normal screen-text path; `utf8_glyph_render_smoke` is included in the final pass/fail result and the initial screenshot. Add `-world-state-server` to also start the integrated host session on an ephemeral port; the smoke connects four clients to that session, checks snapshot delivery and disconnect handling, and stores its log in this run's output directory. The harness still needs a working graphics driver because the current engine creates an OpenGL context even when its window is hidden.

Output is written to `ScreenShots/DebugRuns/`. Starting another debug run clears only the files owned by this tool in that directory, then replaces its log copies. The main report is `DebugRun.log`; loading and console logs are copied alongside it.

The process exits with code 0 only when the smoke run completes successfully. Performance counters are verified during the run and latched, so a later Activity/Scene transition to an initially empty map does not erase evidence collected during the stress phase. Host runs must pass the local stress-spawn check; client runs must observe replicated objects before the transition. A failed check, missing activity, interrupted run, or output setup error returns a nonzero exit code, so CI and scripts can detect failures without parsing screenshots.

Gameplay diagnostics load the ten official data modules by default and skip the optional-module directory scan, ZIP extraction and userdata modules. Add `-module <Name.rte>` to load one selected mod after official content, or use `-debug-run-all-mods` to enable the normal full scan of all enabled Mods and userdata modules. The latter is intended for compatibility runs and can take several minutes with a large Mods folder. Normal non-debug launches still load modules using the existing full behavior.

On Windows, `Tools/RunWorldStateSmoke.ps1` runs the two-process checks with one command:

```powershell
.\Tools\RunWorldStateSmoke.ps1 -Scenario Transition
.\Tools\RunWorldStateSmoke.ps1 -Scenario Equipment -OutputPrefix EquipmentSmoke
```

The script uses `Cortex Command.exe` by default; `-ExecutablePath '.\Cortex Command.debug.minimal.exe'` selects the debug executable. It chooses an available local UDP port unless `-Port` is supplied, waits for the host's self-test and initial screenshot before launching the client, then requires both process exit codes to be zero and both fresh reports to contain `result=passed`. `-TimeoutSeconds` defaults to 600. Reports go to `ScreenShots/<OutputPrefix>-Host/` and `ScreenShots/<OutputPrefix>-Client/`; rerunning the same prefix uses the engine's normal cleanup of its diagnostic files. On failure or timeout the script stops only the processes it started and preserves their reports. Both binaries must support the same protocol version; the wrapper uses one executable for both peers.

Before launching, the wrapper removes its two existing `DebugRun.log` files and rejects reparse-point output directories. It then checks report contents instead of modification times, which Windows can retain while a recreated log is open. On 2026-10-07 the Windows PowerShell 5.1 wrapper passed the Transition scenario with the rebuilt Final v8 executable. A corrected rerun using the same `V8FinalPair` prefix also passed; both output directories contained seven files after completion, replacing the previous diagnostic outputs without creating more run directories.

Use `-debug-run-preflight-only` to run the UTF-8 text/path checks, a Cyrillic ZIP create/open/read/delete round trip, and the local world-state protocol/transport checks, then exit before SDL, content loading, simulation and rendering. This is useful for automated checks on Windows sessions without an interactive display. It does not count as a gameplay or rendering smoke test and creates no screenshots:

```powershell
& '.\Cortex Command.debug.minimal.exe' -debug-run 60 -debug-run-preflight-only
```

`-debug-run-output <name>` writes a run to `ScreenShots/<name>/`; the name must be one directory component. This allows host and client smoke processes to write separate reports and screenshots. To exercise live host Activity/Scene changes, start these two processes from the game directory in separate terminals:

```powershell
& '.\Cortex Command.debug.minimal.exe' -debug-run 900 -debug-overlay -debug-run-output HostTransition -debug-run-world-state-transition -world-state-server 18000
& '.\Cortex Command.debug.minimal.exe' -debug-run 900 -debug-overlay -debug-run-output ClientTransition -debug-run-require-world-state-transition -world-state-client 127.0.0.1 18000
```

At the configured update (600 by default), after an external client has connected, the host switches to the built-in `Skirmish Defense` Activity on `Ketanot Hills`. Assigned network player slots are configured as human players in the server's Activity before it starts; the host transition check fails if an assigned slot is not active as a human. The host and client runs wait for that Activity and Scene to actually become active locally, then continue for 180 simulation updates so the host can broadcast the loaded world and the client can apply it. They fail after 300 seconds if the transition does not complete. The client also fails unless it receives and queues the matching revision. During the brief interval where an Activity restart clears the old world, the automated run keeps the game loop alive. For a reliable two-process run, start the host first and wait for `world_state_host_session_smoke=passed` and the initial screenshot entry, then start the client; this keeps the client's connection out of the host's four-peer slot-assignment self-test. Check both `ScreenShots/HostTransition/DebugRun.log` and `ScreenShots/ClientTransition/DebugRun.log`; each debug client's transport log is `WorldStateClient.log` in its own output directory. Non-debug clients still write `WorldStateClient.log` in the game directory. Use distinct output names for concurrent runs.

When `-world-state-server` is given an explicit port, debug mode now binds that port too; without one it still requests an ephemeral port to avoid collisions.

To verify that lifecycle packets are polled while the host simulation is paused, start a long-running diagnostic host and then a client from the game directory:

```powershell
& '.\Cortex Command.debug.minimal.exe' -debug-run 36000 -debug-run-output NetworkPauseHost -debug-run-pause-after-client -debug-run-require-client-disconnect-after-pause -world-state-server 18000 -module VoidWanderers.rte
& '.\Cortex Command.debug.minimal.exe' -debug-run 180 -debug-run-output NetworkPauseClient -world-state-client 127.0.0.1 18000 -module VoidWanderers.rte
```

The host waits until it has sent snapshots to a real client, pauses simulation 30 updates after the first connection, and keeps rendering/polling the transport. The short client run exits normally after receiving and applying replicated state. The host then records `network_disconnect_while_paused=passed`, saves a final screenshot, and exits; its log also shows that the server remained active until disconnect handling completed. Each process loads the official content plus the selected `VoidWanderers.rte` module, avoiding the rest of the installed Mods collection for this protocol test.

The client transition check treats the requested update count as a minimum. If the host simulation is slower, the client keeps polling and rendering until it observes the target Activity/Scene revision, with a 300-second timeout after gameplay starts. The final screenshot is captured after that transition is observed.

To verify a real host-side terrain edit reaches a separate client, run a longer host session and read the ephemeral port from its `WorldStateServer.log`:

```powershell
& '.\Cortex Command.debug.minimal.exe' -debug-run 3600 -world-state-server -debug-run-world-state-terrain-mutation -debug-run-output TerrainMutationHost
```

After the log says `world-state host listening`, start a second process and replace `<port>` with the logged port:

```powershell
& '.\Cortex Command.debug.minimal.exe' -debug-run 180 -debug-run-require-world-state-terrain-mutation -debug-run-output TerrainMutationClient -world-state-client 127.0.0.1 <port>
```

The host waits until it has sent the client's baseline material patch for a deterministic probe pixel, then removes that pixel through the normal terrain-penetration API and replicates the resulting patch. The client fails unless it receives the material change and reads `Air` back from its loaded terrain. A passing host report also requires that it mutated the pixel and sent its incremental patch. Both processes still create a hidden OpenGL context; this is not a headless-server test.

To verify live equipment state replication between separate processes, start a long-running host and then a client:

```powershell
& '.\Cortex Command.debug.minimal.exe' -debug-run 3600 -debug-run-output EquipmentMutationHost -debug-run-world-state-equipment-mutation -world-state-server 18121
& '.\Cortex Command.debug.minimal.exe' -debug-run 600 -debug-run-output EquipmentMutationClient -debug-run-require-world-state-equipment-mutation -world-state-client 127.0.0.1 18121
```

After a client connects, the host waits 120 simulation updates and changes every loaded human firearm magazine to `-1` (the engine's infinite-ammo value). This mutation exists only in the automated debug session. The client records magazine round counts from the initial server snapshot before loading its Activity, then requires a later snapshot to report a changed count for the same network object ID. A passing client log includes `world_state_remote_equipment_mutation_observed=passed`; the host must report at least one magazine changed. Use separate output directory names if you change the commands for repeated runs. This proves a persistent authoritative equipment-field mutation is serialized and applied over a live connection; it does not simulate a player firing or switching weapons.

The resident-memory measurement is the Windows working set or Linux resident pages, in bytes; `0` means that the platform query failed or is not implemented. The network check uses four local client peers and does not connect remote gameplay clients.

### 2026-10-07 particle-settling crash follow-up

The October 6 `AbortLog.txt` pointed to a null particle-material dereference in the settling pass. The current source guards both the particle material and terrain material before reading settling properties or drawing into terrain (`MovableMan::Update`); this is included in commit `d744362bc00`. Rebuilt Windows x64 `Debug Minimal` and `Final` configurations with MSBuild, then ran the Final executable through `-debug-run 60 -module VoidWanderers.rte`. The run spawned the 16-actor/256-particle stress batches, completed 60 updates, captured three screenshots, passed the UTF-8 glyph-render and world-state transport checks, and exited with `result=passed` (process exit status 0). Artifacts are in `ScreenShots/CrashGuardFinalVerifyOct7/`. This verifies the rebuilt executable on the automated stress scenario; it does not replay the exact saved game/session from the old crash dump. The build emitted one existing LuaJIT `C4244` conversion warning.

On Windows, an unhandled exception also writes `AbortDump.dmp` next to `AbortLog.txt` and `AbortScreen.png`. The dump is overwritten by the next crash and includes thread state plus memory referenced from thread stacks; it can contain game/session data, so review it before sharing it publicly. Windows system error text in the crash dialog is converted to UTF-8 before it is passed to SDL.

## Latest crash follow-up

The 2026-10-04 crash log identified a null dereference in `GibEditor::Update`: the first `EditorDone` action tried to destroy a previous test copy before one existed. The editor now checks the owned test-copy pointer before destroying it. Windows `Debug Minimal|x64` rebuilt successfully, and a fresh hidden `-debug-run 60 -debug-overlay` exited with code 0 and `result=passed` (49 actors, 291 particles). That gameplay smoke does not drive the Gib Editor UI, so it verifies the build and surrounding runtime but is not a direct interactive regression test of the editor action. After the UTF-8 crash-message fallback was added, another rebuilt Windows run also passed with 60 updates (49 actors, 300 particles, 46.21 s elapsed); timings vary across runs and this is not a performance comparison.

This is a repeatable smoke and stress run, not a headless server or a multi-scene benchmark yet. It uses real game content and the existing Lua object creation API.
## Remote movement/fire smoke

`Tools/RunWorldStateSmoke.ps1 -Scenario Control` starts a paired world-state run.
Use `-ExecutablePath '.\Cortex Command.debug.minimal.exe'` for Debug Minimal.
The host flag is `-debug-run-world-state-control`; the client flag is
`-debug-run-require-world-state-control`. These imply the existing Activity/Scene
transition checks. The fixture loads Skirmish Defense/Ketanot Hills, creates a
Pistol-equipped human for the remote slot and a surviving actor for the other
human team, and switches the host Activity to running. This fixture is enabled
only by these diagnostic flags.

After the client receives the controlled actor ID and initial magazine count,
it sends right movement and pulsed fire through the normal InputCommand path.
Both peers require horizontal displacement greater than eight pixels and a
decrease in that actor's magazine count. The client also requires a successful
player-view binding. The host keeps simulating for 180 updates after its own
success so the client can observe later snapshots. A missing result fails the
run; the existing 300-second scenario timeout and wrapper timeout still apply.

Debug Minimal x64 passed this scenario on October 7 in 178.93 seconds with exit
code 0 from both processes. Host displacement was 8.22287 pixels and client
displacement 8.45746 pixels; both observed 11 rounds becoming 9. Reports and
screenshots are in `ScreenShots/RemoteControlPair-Host/` and
`ScreenShots/RemoteControlPair-Client/`. Reusing this prefix replaces the prior
artifacts. The initial attempt exposed a fixture lifetime error: the host exited
on its own success before the client could observe the displacement threshold;
the post-success simulation interval fixes that observed case.

This checks the actual client-input/server-simulation/snapshot path, rather than
only codec or input-manager state. It does not validate all weapons, reloading,
actor switching, inventory, player death/reconnect or mod activities. The final
client screenshot contains terrain and HUD markers, but complete actor visual
presentation and authoritative visibility/fog remain unverified.
## Non-interactive diagnostic failures

In `-debug-run`, RTE assertions, explicit aborts and the unhandled-exception
handler write `DebugFailure.log` in the selected run directory, append
`fatal_error=true` and `result=failed` to `DebugRun.log`, report to stderr and
exit with code 1. They do not open an error dialog or offer restart/ignore.
Normal interactive gameplay retains its existing dialogs.

Fatal state may hold engine/Lua locks or contain damaged objects. This path does
not call the renderer, save the Activity, or run shutdown destructors. Initial,
stress and final screenshots already captured by the run remain available; it
does not attempt a fresh screenshot at the failure point. Report writing is
best effort if storage is unavailable. Platform crash-dump generation remains
separate from this handler behavior.

Run the deterministic handler checks with Windows PowerShell 5.1 or later:

```powershell
.\Tools\RunDebugFailureSmoke.ps1
```

The tool starts three preflight-only probes (`assert`, `abort`, `exception`),
expects target exit code 1, validates the UTF-8 failure message and failed result,
and verifies that the exception handler preserves its supplied callstack. No
game window is initialized by these probes. A timeout fails the tool and stops
only its own process. Optional `-ExecutablePath`, `-OutputPrefix` and
`-TimeoutSeconds` parameters select the binary, report prefix and timeout.
The engine flag `-debug-run-error-probe <assert|abort|exception>` is used together
with `-debug-run-preflight-only`; these are direct handler tests, not deliberate
memory corruption or a test of OS exception dispatch.

Windows x64 Final rebuilt and all three probes passed with expected code 1 and
the Cyrillic message preserved under `ScreenShots/DebugFailureSmoke-*/`.
`ScreenShots/DebugFailureCleanup/` verified that an assertion creates the failure
report and a subsequent successful preflight in the same directory removes it
and exits with code 0. The full 60-update Final gameplay stress run under
`ScreenShots/NonInteractiveHealthy/` exited with code 0 and `result=passed`,
including UTF-8 glyph and replica-player-view tests. The wrapper retains the
original process handle through `System.Diagnostics.Process`, avoiding missing
`ExitCode` from rapidly exiting redirected `Start-Process -PassThru` processes
on Windows PowerShell 5.1.

## CPU graphics startup probe

Run `Cortex Command.exe -debug-run 60 -debug-run-cpu-graphics-preflight -debug-run-output CPUGraphicsFoundation` to exercise CPU frame-buffer and bitmap resource initialization without SDL video or OpenGL. The probe also calls WindowMan shutdown twice without initializing a window, to verify safe cleanup for simulation-only startup. To verify that no working video driver is required, set `SDL_VIDEODRIVER=invalid-driver-for-headless-test` for this process and restore the previous environment value afterwards.

This probe exits before content-module loading and Activity simulation; it creates no screenshots. A passing report contains `cpu_graphics_preflight=passed video_initialized=0 gpu_enabled=0` and `result=passed`. It does not demonstrate a playable dedicated server or a software renderer. The regular debug run remains the rendering and gameplay startup check.

## Headless content loading probe

`Cortex Command.exe -debug-run 60 -debug-run-headless-content -debug-run-output HeadlessOfficialContent` extends the CPU graphics probe with common manager construction, Lua initialization and normal PresetMan loading of all official modules. LoadingScreen writes a log without creating GUI controls, loading splash textures or drawing. Shader presets retain their data definitions but do not compile GL programs while GPU resources are disabled. Audio, input, menu and post-process rendering are not initialized.

The Windows Final check passed with an invalid process-local `SDL_VIDEODRIVER`, loading 10 official modules and resolving the Tutorial Bunker scene preset. Its report contains `headless_content_preflight=passed modules=10 official_modules=10 video_initialized=0`; exit code is 0. Logs are written to the selected diagnostic output directory, and the next diagnostic run cleans previous reports using the existing output-directory safeguards. Add `-debug-run-all-mods` to include optional modules; this may expose errors in installed mods and is not implied by the official-content result.

The probe stops before loading scene terrain into a running Activity, before physics updates and before an external client connection. It is a content-startup check, not a dedicated server. CPU collision images still load through the normal content path to preserve content definitions and physics compatibility.

The installed-content Windows check also passed with the same invalid video driver: 43 modules total, 10 official, exit code 0. A first attempt was stopped by the test wrapper at its 180-second limit during module loading; the retry with a 600-second limit completed. Existing mod-version warnings remain. No mod Activity or runtime script behavior is validated by this loading-only check. The ordinary GPU client also passed 60 simulation updates and its final terrain/actor/HUD/UTF-8 screenshot was inspected.

## Headless simulation probe

Run `Cortex Command.exe -debug-run 60 -debug-run-headless-simulation -debug-run-output HeadlessSimulation` to load official content, start the real Tutorial Mission on Tutorial Bunker and execute 60 simulation updates without SDL video or a window. This includes Lua manager updates, Activity updates, scene/terrain updates, movable-object physics and late global scripts. A cloned Gold Particle starts with nonzero velocity; the test requires observed movement and advancing simulation ticks. It keeps CPU MOID collision drawing but calls MovableMan::Update(false) to skip the color-layer presentation draw. The existing client Update() behavior is unchanged.

The Windows Final probe passed with `SDL_VIDEODRIVER=invalid-driver-for-headless-test`: `headless_activity_started=1`, `headless_simulation_preflight=passed updates=60 particle_moved=1 actors=17 video_initialized=0`, exit code 0. No ERROR or WARNING entries were found in its saved console log. This verifies a short real simulation run; it does not verify every Activity or mod's runtime callbacks. Activity-owned CPU UI state still exists for compatibility. No graphical screenshots are created by this probe.

This path remains a diagnostic entry point inside the client executable. A standalone server target, remote-client acceptance during this simulation, long-running sessions and Linux execution are not yet verified.

The same headless simulation check also passed at 600 updates with the invalid video driver, 17 actors, observed particle movement, exit code 0 and zero console errors/warnings. The existing output directory was reused. Ordinary client regression passed 60 updates and its final screenshot was visually inspected.

## Headless world-state host/client smoke

Run `Tools\RunHeadlessWorldStateSmoke.ps1` from PowerShell. By default it starts the Final executable as a headless diagnostic host with 3600 simulation updates and two separate graphical clients with 600 replica updates each. The host uses an invalid process-local SDL video driver and listens on an automatically selected loopback UDP port. Clients use their normal graphical startup. The wrapper waits for fresh host readiness, checks native exit codes and reports, validates received snapshots without missing presets and checks zero client-local simulation ticks. It owns and cleans up only the processes it starts. Diagnostic files reuse fixed output directories; stale readiness reports are removed with reparse-point safeguards.

The underlying host command is `Cortex Command.exe -debug-run 3600 -debug-run-headless-server -debug-run-output HeadlessWorldStateSmoke-Host`. This is a bounded diagnostic session using Tutorial Mission/Tutorial Bunker and an automatic localhost port, not the production server CLI. The report prints `headless_server_ready=1 port=...`. The host test requires a connected client, broadcast snapshots, client disconnection and at least 60 further simulation updates. A shorter host duration can fail if clients have not completed loading or disconnected. Wrapper options include `-Clients 1`, `-HostUpdates 9000`, `-TimeoutSeconds 600` and `-ExecutablePath`.

Windows Final two-client evidence: both clients exited with code 0 and passed their existing replica tests. They received 251 and 248 snapshots with zero local simulation ticks. The GPU-free host ran 3600 updates, broadcast 278 snapshots, accepted 600 input commands for Player 1 and ran 1835 further updates after all clients disconnected. Its console had no errors or warnings, and the first client's final scene/actor/HUD screenshot was inspected. The entire wrapper completed in 71.76 seconds.

WorldStateServerSession::Start now accepts an optional reserveLocalPlayer argument, defaulting to true for the playable graphical host. The headless path passes false and makes Player 1 available to the first remote client. Tutorial supports only one active human player; the second connection receives world snapshots as an inactive/spectator slot. This test demonstrates simultaneous transport and authoritative replication, not two active Tutorial players. Remote movement/fire is covered separately by the Control scenario in RunWorldStateSmoke.ps1. Funds, complete Activity-specific HUD/Lua state, production administration and Linux operation remain incomplete or unverified.

Regression after integration exposed a diagnostic-only Lua mutex race when stress fixtures cloned actors while asynchronous GC was still running. The fixtures and final replica smoke now wait for existing worker tasks before native cloning. Windows Final was rebuilt: the graphical Control host/client test passed again in 55.62 seconds, and the two-client headless wrapper passed again in 70.83 seconds, including simultaneous connections and continued updates after disconnection. These joins occur only in diagnostic fixture/completion code, not in every production frame.

## Server configuration and CLI overrides

The headless simulation/server diagnostic entry points accept a server configuration independent of client Settings.ini. `Server.example.ini` documents the defaults. Files use UTF-8, optionally with a BOM; Cyrillic paths and values are supported. Keys are case-sensitive: ListenAddress, Port, Players, Name, ActivityClass, Activity, Scene and IncludeMods. Blank lines, full-line `#`/`;` comments and an optional `[Server]` heading are accepted. Values may be quoted. Unknown keys and malformed values fail startup with file/line context. A failed parse does not partially apply configuration.

Use `--config <path>`, `--listen <address>`, `--port <0..65535>`, `--players <1..64>`, `--name <text>`, `--activity-class <class>`, `--activity <preset>`, `--scene <preset>` and `--mods <true|false>`. CLI overrides are applied after configuration files regardless of option order. Port 0 requests an automatic bound port. Players limits transport connections, not the Activity's supported active humans; current engine Activities retain their player limits. Name is currently an administrative log label, not a server-browser advertisement. IncludeMods controls optional module loading; the existing `-debug-run-all-mods` diagnostic switch also enables it.

Example: `Cortex Command.exe -debug-run 3600 -debug-run-headless-server --config Server.example.ini --listen 127.0.0.1 --port 8000 --players 4 -debug-run-output ConfiguredHost`. The diagnostic still expects a client to connect and disconnect before its bounded run ends. It is not a persistent production server command or a standalone dedicated binary. A simulation-only check can use `-debug-run-headless-simulation --activity-class GAScripted --activity "Skirmish Defense" --scene "Ketanot Hills"`.

Windows Final configuration evidence: a BOM UTF-8 `сервер.ini` containing the name `Тестовый сервер` was read; explicit CLI values overrode config address/port/player limit to loopback, an automatic port and 4 connections. The two-client wrapper passed in 70.30 seconds. Seven invalid CLI cases (negative/oversized port, zero/oversized player limit, invalid boolean, unknown option and missing scene value) exited with code 1 and a server_configuration error. The selected Skirmish Defense/Ketanot Hills Activity also loaded and executed 60 physics updates without video; no actors were present, so this is a startup/scene-selection check rather than an active multiplayer battle.

`RunHeadlessWorldStateSmoke.ps1 -ServerConfigPath <path> -PlayerLimit 4` exercises config loading and explicit overrides. The wrapper always overrides listen address and port to loopback/automatic for isolated tests. The command-line parser is shared code intended for the future persistent server entry point.

## Persistent dedicated server

The separate Windows console binary and the client's --server mode use the same headless content/step functions as the diagnostics. The persistent mode does not run debug fixtures or require clients to exit successfully; it runs until console/signal shutdown. See DedicatedServer.md for build, CLI/configuration, runtime limits and the real two-client console-server verification. After the extraction, RunHeadlessWorldStateSmoke also passed again in 70.66 seconds.

## Team-funds replication checks

Protocol v10 adds four authoritative team balances. Preflight reports
world_state_team_funds_codec=passed after finite-value round trips and invalid
float rejection. Connected diagnostic clients check every applied snapshot
against Activity::GetTeamFunds and report world_state_team_funds_presentation.
RunDedicatedServerSmoke.ps1 -CheckTeamFunds waits for both clients' initial
snapshots, sends two console balance changes and requires both exact values
in both clients' network logs, in addition to the existing lifecycle checks.
