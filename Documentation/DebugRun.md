# Automated debug run

Run the Windows client from the game directory:

```powershell
& '.\Cortex Command.exe' -debug-run 600
```

The optional number is the target number of simulation updates (60–36,000; default 600). The run creates a hidden SDL window, disables audio, loads the built-in Tutorial Bunker activity, spawns actor and particle batches at the start and halfway point, records module-load time, frame/simulation/render/Lua/AI/object counters and process resident memory, captures the final world state, broadcasts it through local RakNet server/client peers, and saves initial/stress/final screenshots before exiting. The transport smoke connects four clients, then verifies each received and decoded the same live snapshot. Before loading content it verifies UTF-8 helpers and creates/reads/removes a Cyrillic and emoji filename through the native file API. During rendering, it checks that the bundled TrueType font includes Cyrillic glyphs and draws `UTF-8 test: Привет, Ёжик!` through the normal screen-text path; `utf8_glyph_render_smoke` is included in the final pass/fail result and the initial screenshot. Add `-world-state-server` to also start the integrated host session on an ephemeral port; the smoke connects four clients to that session, checks snapshot delivery and disconnect handling, and stores its log in this run's output directory. The harness still needs a working graphics driver because the current engine creates an OpenGL context even when its window is hidden.

Output is written to `ScreenShots/DebugRuns/`. Starting another debug run clears only the files owned by this tool in that directory, then replaces its log copies. The main report is `DebugRun.log`; loading and console logs are copied alongside it.

The process exits with code 0 only when the smoke run completes successfully. A failed check, missing activity, interrupted run, or output setup error returns a nonzero exit code, so CI and scripts can detect failures without parsing screenshots.

Use `-debug-run-preflight-only` to run the UTF-8, file-path and local world-state protocol/transport checks, then exit before SDL, content loading, simulation and rendering. This is useful for automated checks on Windows sessions without an interactive display. It does not count as a gameplay or rendering smoke test and creates no screenshots:

```powershell
& '.\Cortex Command.debug.minimal.exe' -debug-run 60 -debug-run-preflight-only
```

`-debug-run-output <name>` writes a run to `ScreenShots/<name>/`; the name must be one directory component. This allows host and client smoke processes to write separate reports and screenshots. To exercise live host Activity/Scene changes, start these two processes from the game directory in separate terminals:

```powershell
& '.\Cortex Command.debug.minimal.exe' -debug-run 900 -debug-overlay -debug-run-output HostTransition -debug-run-world-state-transition -world-state-server 18000
& '.\Cortex Command.debug.minimal.exe' -debug-run 900 -debug-overlay -debug-run-output ClientTransition -debug-run-require-world-state-transition -world-state-client 127.0.0.1 18000
```

At update 600, the host switches to the built-in `Skirmish Defense` Activity on `Ketanot Hills`. The client run fails unless it receives the new revision and queues that exact Activity/Scene. Check both `ScreenShots/HostTransition/DebugRun.log` and `ScreenShots/ClientTransition/DebugRun.log`; each debug client's transport log is `WorldStateClient.log` in its own output directory. Non-debug clients still write `WorldStateClient.log` in the game directory. Use distinct output names for concurrent runs.

To verify a real host-side terrain edit reaches a separate client, run a longer host session and read the ephemeral port from its `WorldStateServer.log`:

```powershell
& '.\Cortex Command.debug.minimal.exe' -debug-run 3600 -world-state-server -debug-run-world-state-terrain-mutation -debug-run-output TerrainMutationHost
```

After the log says `world-state host listening`, start a second process and replace `<port>` with the logged port:

```powershell
& '.\Cortex Command.debug.minimal.exe' -debug-run 180 -debug-run-require-world-state-terrain-mutation -debug-run-output TerrainMutationClient -world-state-client 127.0.0.1 <port>
```

The host waits until it has sent the client's baseline material patch for a deterministic probe pixel, then removes that pixel through the normal terrain-penetration API and replicates the resulting patch. The client fails unless it receives the material change and reads `Air` back from its loaded terrain. A passing host report also requires that it mutated the pixel and sent its incremental patch. Both processes still create a hidden OpenGL context; this is not a headless-server test.

The resident-memory measurement is the Windows working set or Linux resident pages, in bytes; `0` means that the platform query failed or is not implemented. The network check uses four local client peers and does not connect remote gameplay clients.

On Windows, an unhandled exception also writes `AbortDump.dmp` next to `AbortLog.txt` and `AbortScreen.png`. The dump is overwritten by the next crash and includes thread state plus memory referenced from thread stacks; it can contain game/session data, so review it before sharing it publicly. Windows system error text in the crash dialog is converted to UTF-8 before it is passed to SDL.

## Latest crash follow-up

The 2026-10-04 crash log identified a null dereference in `GibEditor::Update`: the first `EditorDone` action tried to destroy a previous test copy before one existed. The editor now checks the owned test-copy pointer before destroying it. Windows `Debug Minimal|x64` rebuilt successfully, and a fresh hidden `-debug-run 60 -debug-overlay` exited with code 0 and `result=passed` (49 actors, 291 particles). That gameplay smoke does not drive the Gib Editor UI, so it verifies the build and surrounding runtime but is not a direct interactive regression test of the editor action. After the UTF-8 crash-message fallback was added, another rebuilt Windows run also passed with 60 updates (49 actors, 300 particles, 46.21 s elapsed); timings vary across runs and this is not a performance comparison.

This is a repeatable smoke and stress run, not a headless server or a multi-scene benchmark yet. It uses real game content and the existing Lua object creation API.
