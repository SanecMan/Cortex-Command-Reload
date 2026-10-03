# Automated debug run

Run the Windows client from the game directory:

```powershell
& '.\Cortex Command.exe' -debug-run 600
```

The optional number is the target number of simulation updates (60–36,000; default 600). The run creates a hidden SDL window, disables audio, loads the built-in Tutorial Bunker activity, spawns actor and particle batches at the start and halfway point, records module-load time, frame/simulation/render/Lua/AI/object counters and process resident memory, captures the final world state, broadcasts it through local RakNet server/client peers, and saves initial/stress/final screenshots before exiting. The transport smoke connects four clients, then verifies each received and decoded the same live snapshot. Before loading content it verifies UTF-8 helpers, including four-byte code points, and exchanges a representative snapshot. During rendering, it checks that the bundled TrueType font includes Cyrillic glyphs and draws `UTF-8 test: Привет, Ёжик!` through the normal screen-text path; `utf8_glyph_render_smoke` is included in the final pass/fail result and the initial screenshot. Add `-world-state-server` to also start the integrated host session on an ephemeral port; the smoke connects four clients to that session, checks snapshot delivery and disconnect handling, and stores its log in this run's output directory. The harness still needs a working graphics driver because the current engine creates an OpenGL context even when its window is hidden.

Output is written to `ScreenShots/DebugRuns/`. Starting another debug run clears only the files owned by this tool in that directory, then replaces its log copies. The main report is `DebugRun.log`; loading and console logs are copied alongside it.

The process exits with code 0 only when the smoke run completes successfully. A failed check, missing activity, interrupted run, or output setup error returns a nonzero exit code, so CI and scripts can detect failures without parsing screenshots.

The resident-memory measurement is the Windows working set or Linux resident pages, in bytes; `0` means that the platform query failed or is not implemented. The network check uses four local client peers and does not connect remote gameplay clients.

On Windows, an unhandled exception also writes `AbortDump.dmp` next to `AbortLog.txt` and `AbortScreen.png`. The dump is overwritten by the next crash and includes thread state plus memory referenced from thread stacks; it can contain game/session data, so review it before sharing it publicly. Windows system error text in the crash dialog is converted to UTF-8 before it is passed to SDL.

This is a repeatable smoke and stress run, not a headless server or a multi-scene benchmark yet. It uses real game content and the existing Lua object creation API.
