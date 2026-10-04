# Performance controls and diagnostics

Open **Settings → Perf.** in the main menu or pause menu. Changes take effect immediately and are saved with the other game settings. **F8** cycles the in-game overlay through Off, Basic, and Detailed. F3 retains its existing console-log shortcut.

Basic shows actual wall-clock FPS and frame time (including VSync/FPS-cap waiting), measured simulation and render time, active actors and top-level movable objects, and process resident RAM. Detailed adds existing engine stage timers for input, Lua VM plus scripts, actor AI, and actor/particle travel (the latter includes collision work); world-state snapshot capture time; a once-per-second count of live scene object classes; spawn/delete events per second; tracked Lua callback invocations; resolution, VSync, FPS cap, and connected network peers. Object counts are top-level `MovableMan` objects, so attached parts are excluded. Projectile-like and collision-enabled are classifications, not independent object lists. The collision timer is part of travel time, not a separate exact collision-only measurement. The callback count is limited to calls instrumented by the Lua manager, currently the master script state. The expensive class scan runs only while Detailed is visible, at most once a second.

The optimization controls affect these systems:

- **FPS limit**: menu and gameplay frame pacing. Unlimited disables this extra wait. VSync remains independently configurable; on Windows the existing renderer requests a swap interval only in fullscreen because windowed presentation is managed by DWM. The automated `-debug-run` bypasses the gameplay cap to keep benchmarks comparable.
- **Particle cap / Gib cap / Max MOs**: once a second, mark the oldest eligible visual particles for normal deletion until the selected soft cap is met. Eligibility is limited to non-colliding, script-free, finite-lived `MOSParticle` objects and tagged visual gibs. Actors, devices, mission-critical objects, and untagged `MOPixel` objects are excluded. The total MO cap is therefore soft if protected objects alone exceed it.
- **Debris life / Particle life**: shorten only finite-lived, non-colliding, script-free visual objects. Debris life applies to gib-tagged objects; particle life also applies when an eligible object enters `MovableMan`.
- **Background**: High draws every scene background layer; Medium draws alternating layers; Low draws the first; Off skips them.
- **Screen FX**: Reduced skips glow scanning and halves screen shake; Off also skips post effects and removes screen shake.
- **Gore density**: reduces the spawn count of safe visual gibs. Gameplay-relevant colliding or scripted gibs retain their original count.
- **Preset**: Quality keeps the original limits and effects. Balanced, Performance, and Potato progressively lower the visual budgets. Changing an individual visual budget marks the preset Custom. Resolution is never changed by a preset.

These limits intentionally prefer preserving game and mod behavior to enforcing a strict count. The numerical cap levels are implementation budgets, not promises about final scene totals. A visual effect can already be in flight when a limit changes; it will be considered on the next check.

Current eligible-particle budgets are High 10,000, Medium 6,000, Low 3,000, and Very Low 1,000. Gib budgets are High 2,000, Medium 1,000, and Low 400. Debris lifetimes are Long 30 s, Normal 12 s, and Short 5 s. Unlimited leaves that particular limit off.

For an automated stress capture, run `Cortex Command.debug.minimal.exe -debug-run 60 -debug-overlay`. The run writes `ScreenShots/DebugRuns/DebugRun.log` and three screenshots, then exits. The next debug run replaces the old debug artifacts. Use `CCCP_SETTINGSPATH` to point to a separate settings file when benchmarking different presets without changing personal settings.

For checks that do not need graphics, use `Cortex Command.debug.minimal.exe -debug-run 60 -debug-run-preflight-only`. This runs UTF-8, protocol, dirty-terrain-grid and four-client transport checks, then exits before SDL/window initialization. A full gameplay stress capture still creates a hidden OpenGL window; SDL's Windows offscreen video driver can initialize here, but it cannot create the engine's OpenGL window without a usable OpenGL display/context.
