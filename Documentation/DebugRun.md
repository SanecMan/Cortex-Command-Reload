# Automated debug run

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
