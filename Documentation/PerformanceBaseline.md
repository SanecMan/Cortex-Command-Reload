# Performance baseline

## 2026-10-03: automated Tutorial Bunker run

Measured on AMD Ryzen 5 3500U with Radeon Vega 8, Windows, `Final|x64`, using the automated debug run (`-debug-run 60`). The scene was Tutorial Bunker at 960×540. It spawned 16 actors and 256 particles on update 1, then another batch at update 30.

| Measurement | Result |
| --- | ---: |
| Data module loading, warm-cache run | 24.71 s |
| Total run, including module loading | 29.66 s |
| Simulation-only interval | 4.95 s |
| Simulation updates | 60 |
| Rendered frames | 67 |
| Average frame time during simulation | 73.81 ms |
| Simulation rate | 12.13 updates/s |
| Actors at finish | 49 |
| Particles at finish | 297 |

This is the measured baseline for the current development build and a deliberately small stress load. File-cache state and background activity affect startup time; repeat runs should be compared under the same conditions. The result shows substantial headroom for performance work, but it does not identify which subsystem dominates frame time. The next profiling step should instrument update stages (Lua, physics/object update, AI, terrain, render) before changing them.

## Profiling sample: Debug Minimal

A later run with stage counters enabled used `Debug Minimal|x64` on the same machine and the same 60-update scene. This configuration is unoptimized, so these values identify where to investigate and must not be compared directly with the Final baseline.

| Measurement | Result |
| --- | ---: |
| Data module loading | 41.24 s |
| Simulation-only interval | 7.32 s |
| Measured game-loop work per frame | 60.43 ms |
| Render and upload per frame | 6.34 ms |
| Simulation total, recent average | 61.41 ms/update |
| Actor AI | 14.71 ms/update |
| Actor travel | 8.57 ms/update |
| Actor update | 12.72 ms/update |
| Lua scripts | 10.96 ms/update |
| Particle travel + update | 5.00 ms/update |
| Activity update | 0.37 ms/update |

These counters overlap where one measurement runs inside another (for example, Lua callbacks during actor work), so their durations must not be summed. Per-module timing located the dominant `.rte` packages and a phase-level follow-up timed their parsing paths.

After adding process-memory reporting, a later `Debug Minimal|x64` 60-update run reported a Windows working set of 1,752,379,392 bytes at exit (49 actors, 310 particles, 362 top-level snapshot objects). This is a single debug-build sample, not a release-memory target; use repeated runs with the same modules, scene, build configuration, and stress batches before attributing a change to an optimization.

The same diagnostic build measured these module totals while loading, before phase-level breakdown was added:

| Module | Load time |
| --- | ---: |
| Base.rte | 17.36 s |
| Missions.rte | 11.89 s |
| Ronin.rte | 2.60 s |
| Browncoats.rte | 2.36 s |
| Dummy.rte | 1.91 s |
| Coalition.rte | 1.60 s |
| Uzira.rte | 1.54 s |
| Techion.rte | 0.96 s |
| Imperatus.rte | 0.74 s |
| MuIlaak.rte | <0.01 s |

Base.rte and Missions.rte account for most of the observed load. Neither currently has a `MergedIndex.ini`.

The phase-level follow-up (`Debug Minimal|x64`) measured Base.rte at 14.92 s in index parsing and Missions.rte at 15.44 s. Both reported 0 ms in folder scanning; all included `.ini` content is read through the index hierarchy. That run loaded all modules in 41.24 s. The result points to `Reader`/INI parsing and preset construction as the next areas to profile.

## 2026-10-03: repeated warm-start diagnostics

Additional `Debug Minimal|x64` runs on the same Ryzen 5 3500U machine measured module loading at 37.00 s, 37.26 s, and 52.79 s. The last run overlapped other machine activity, so these values demonstrate significant run-to-run variance and are not evidence of a startup optimization. Its phase counters attributed 17.06 s to `Base.rte` index parsing and 22.84 s to `Missions.rte` index parsing; folder scanning remained 0 ms. Across the measured runs, those two large indices dominate startup time.

A generated flattened `MergedIndex.ini` was tested as a way to avoid repeated file opens, but the existing `Reader` rejected the flattened format at runtime. The generated files were removed and the approach is excluded from the performance results. Any future index-cache or parser change must preserve the engine's include and indentation semantics, then be compared with multiple idle warm-cache runs.

## Optimization: case-sensitive path cache lookup

`System::PathExistsCaseSensitive` built a cache of every path hash in the working tree, then searched it with `std::find` for every `.ini` include. That made each of thousands of lookups linear in the number of files. Replacing the vector with `std::unordered_set` preserves exact-hash matching while making lookups average constant time. No simulation or mod-facing behavior changes.

Two comparable warm-cache 60-update `Debug Minimal|x64` Tutorial Bunker runs were measured before and three runs after on the same machine. All runs passed the gameplay smoke test and included the same actor/particle stress batches. Runs with unrelated background activity were excluded.

| Measurement | Before median | After median | Change |
| --- | ---: | ---: | ---: |
| Data module loading | 37.13 s (2 comparable runs) | 24.23 s (3 runs) | −34.7% |
| Total run | 42.85 s (2 runs) | 29.12 s (3 runs) | −32.1% |
| Base.rte + Missions.rte index phases | 30.36 s (earlier instrumented run) | 18.61 s | −38.7% |

The phase comparison uses an earlier instrumented baseline rather than the exact same run set, so the module-load median is the stronger before/after result. Individual simulation timings varied with the number and behavior of spawned particles and are not claimed as an effect of this path lookup change.

## Linux offscreen smoke run

On 2026-10-03, commit `b32ac660e` built successfully on Ubuntu 24.04 x86_64 with the repository's Meson/Ninja build. `ldd` found no unresolved shared dependencies. Running `SDL_VIDEODRIVER=offscreen ./build/CortexCommand -debug-run 60` also passed without an X11/Wayland display and produced all three debug screenshots. The run reported 12.27 s module loading, 15.40 s total elapsed time, and 19.2 simulation updates/s. This uses the offscreen renderer and debug build, so its frame-rate and render-time counters are diagnostic only and are not comparable to normal GPU rendering or the Windows startup benchmark.

After state-transport and memory instrumentation were enabled, Linux x86_64 rebuilt and passed the same offscreen smoke run, including a RakNet loopback snapshot exchange. That run reported 1,110,351,872 resident bytes. The Windows sample above reports 1,752,379,392 working-set bytes from a different build and runtime; these values should not be compared directly.

After the debug harness began skipping audio initialization, commit `0527ca907` rebuilt successfully on both Windows x64 (`Debug Minimal`) and Ubuntu 24.04 x86_64. The Windows offscreen/hidden-window smoke run passed with 60 updates, 378 snapshot objects, 25.36 s module loading, 30.25 s elapsed time, and 1,753,886,624 bytes working set. The Linux offscreen smoke run passed with 60 updates, 358 snapshot objects, 8.68 s module loading, 11.79 s elapsed time, 19.33 updates/s, and 589,729,792 resident bytes. The Linux run no longer reproduced the FMOD worker-thread shutdown crash. A repeated Linux run replaced the prior owned screenshots/logs and again recorded `result=passed`. These are debug harness samples from different builds, asset caches, and machines; they are not a controlled platform or performance comparison.

Commit `e077b098f` extended the harness to send the live Tutorial Bunker stress snapshot through local RakNet peers. The Windows run passed with 335 snapshot objects, 24.10 s module loading, 29.20 s elapsed time, and 899,846,144 bytes working set. Ubuntu 24.04 rebuilt the UTF-8 encoder and live-snapshot test, then passed with 390 snapshot objects, 9.13 s module loading, 11.88 s elapsed time, 21.81 updates/s, and 609,169,408 resident bytes. Both logs report `live_world_state_transport=passed`; Linux exited with status 0. Object counts vary because Lua-driven particles evolve nondeterministically. These runs validate local packet delivery and decoding, not remote gameplay synchronization.

Commit `a6bf00ea0` expanded the local RakNet smoke test to four independent client peers. Each run waits for all four connections, broadcasts the live scene snapshot, and checks that every client decodes it. Windows passed with 341 objects and process exit 0; Ubuntu 24.04 rebuilt and passed with 367 objects and process exit 0. This exercises the configured multi-client transport limit locally but is not an Internet/LAN connection or an in-game client synchronization test.

The integrated world-state host loop also passed its four-client connect, snapshot-receive, disconnect, and host-survival smoke on Windows `Debug Minimal|x64` and Ubuntu 24.04 x86_64. The Windows run captured 382 objects, 23.12 s module loading, and 29.56 s elapsed time; the Linux offscreen run captured 345 objects and 9.12 s module loading. These runs include additional local RakNet connection activity and should be treated as feature-smoke timings, not as a controlled optimization comparison.

The same Windows diagnostic run ended with 49 actors and 330 particles. Its `PerformanceMan` averages reported 55.07 ms for simulation, 11.14 ms for AI, 8.03 ms for actor travel, 10.49 ms for actor update, 9.78 ms for Lua scripts, 2.93 ms for particle travel, and 1.78 ms for particle update. This is one debug-build stress sample rather than a benchmark median, but it points to actor work, scripts, and their interaction as the next runtime areas to profile more deeply. The snapshot-host smoke itself is not included in this `PerformanceMan` sample because the test clients run before the gameplay stress loop.

After adding the visible Cyrillic glyph check, a Windows `Debug Minimal|x64` smoke run passed with 49 actors and 327 particles. It measured 22.46 s module loading, 30.21 s total elapsed time, 37.12 ms average simulation update, 9.22 ms AI, 8.26 ms actor update, 7.90 ms Lua scripts, 903,475,200 bytes working set, and 379 snapshot objects. This run validates the diagnostic but is a single sample and does not demonstrate a performance gain; compare repeated runs with the same build and workload before attributing timing changes.


The world-state protocol v2 Windows and Ubuntu 24.04 smoke runs both passed after adding Activity/Scene module identity to snapshots. The Linux offscreen Debug Minimal run completed in 12.04 s, including 8.22 s module loading; it ended with 49 actors, 295 particles, 12.79 ms average simulation update, 6.23 ms AI, 2.22 ms Lua scripts, and 605,982,720 resident bytes. This is a single Linux debug sample and is not directly comparable to the Windows timings or earlier Linux runs using different revisions and workloads.

## ASCII fast path in legacy text conversion

The Windows Debug Minimal build ran the same hidden `-debug-run 1` startup three times before and after adding an ASCII fast path to `UTF8::PreserveLegacyWindows1251`. Before: 23.5491 s, 25.4176 s, 24.2147 s (median 24.2147 s). After: 22.8374 s, 23.8866 s, 23.3706 s (median 23.3706 s), a 3.5% median reduction. All runs exited successfully; the after runs also passed UTF-8/Windows-1251, world-state protocol, and transient MOPixel replica smoke checks. The ranges overlap, so this is a small observed gain rather than a strong causal result. Per-module output on the last after run showed Base.rte index parsing at 8.44 s and Missions.rte at 9.65 s; these remain the main startup targets.

## World-state snapshot compression and live Linux client

LZ4 framing was measured on stress-scene snapshots and only retained when smaller than the original packet. A Windows snapshot compressed from 28,694 to 9,380 bytes (67.3% less); a Linux snapshot compressed from 27,686 to 9,006 bytes (67.5% less). On the configured 20 snapshots/second cadence, this corresponds to about 188 KB/s instead of 574 KB/s for the Windows sample, before RakNet/UDP overhead. The Linux x64 build passed the offscreen debug smoke and a separate server/client process test; the client connected, loaded the server Activity and Scene, applied 156 snapshots, and reported zero missing presets. This validates transport compression and replica application, not yet authoritative input handling or full simulation-state synchronization.

## Reader stream-buffer benchmark: 2026-10-04

The old parser used `std::istream::peek/get/ignore` for each byte while reading property names, values, whitespace and comments. The optimized parser reads through the same stream buffer directly and retains the existing Reader grammar, UTF-8/Windows-1251 handling and recursive include semantics.

Measured on this Windows x64 machine with `Debug Minimal`, the same installed modules, Tutorial Bunker, and `-debug-run 60`. Runs were made in an old/new/old source sequence with warm filesystem caches. Both versions passed the automated run, UTF-8 glyph probe and world-state transport smoke.

| Measurement | Existing Reader | Buffered Reader |
| --- | ---: | ---: |
| Data module load, conservative paired run | 57.47 s | 32.74 s |
| Base.rte index/include parsing | 25.80 s | 11.34 s |
| Missions.rte index/include parsing | 17.75 s | 10.74 s |

The observed total load reduction was 43%. A second buffered run measured 29.60 s, so the single paired comparison is reported conservatively; cache and system activity still affect totals. This optimization changes only how bytes are fetched from each existing stream, not the on-disk data format or module order.

The rebuilt Windows `Final|x64` executable also passed `-debug-run 60` and measured 7.18 s module loading in a warm-cache run. This single release sample is not the paired comparison above.

## 2026-10-04: cold and warm startup variance

On the same Ryzen 5 3500U system, the Final executable first measured 105.03 s module loading (Base.rte 60.79 s). A subsequent Debug Minimal run measured 184.98 s (Base.rte 92.31 s, Missions.rte 35.94 s) and passed the automated scenario. Immediately afterward, the Final executable loaded the same modules in 7.52 s (Base.rte 3.61 s, Missions.rte 1.89 s) and passed. These builds and runs are not a controlled cold/warm pair, so the difference is not a speedup claim. It does show that startup measurements are dominated by highly variable cold-read/system conditions; warm Final startup remains close to the earlier 7.18 s sample.

At the time of these runs, the repository was on `H:`, reported by Windows as a removable exFAT volume. `Base.rte` contains about 120 MB across 4,385 files, including 787 sound files, and `Missions.rte` adds about 5.3 MB across 317 files. Cold startup on that volume is therefore a different target from the warm parser benchmark. No filesystem cache flush, volume change, or user data move was performed. Future cold-start comparisons should record the volume and cache condition, and warm-start comparisons should use the same Final binary and module set.

A paired Ubuntu 24.04 x86_64 offscreen run on the configured Linux machine measured 3.16 s with the existing Reader and 2.96 s with the buffered Reader (about 6%). Both passed the same 60-update UTF-8 and world-state smoke. Its absolute times differ from Windows because this machine, filesystem cache and build setup differ; only the within-machine pair is relevant.

## Fresh Linux build and startup sample: 2026-10-04

A fresh source staging on Ubuntu 24.04 x86_64 configured with Meson 1.12.1 and built with GCC 13.3.0/Ninja. The offscreen `-debug-run 60 -debug-overlay` passed with 60 updates, 8.85 s module loading, 11.82 s total elapsed time, 20.15 simulation updates/s and 633,425,920 resident bytes. `ldd` reported no unresolved shared dependencies. This is a single Linux smoke sample, not a before/after optimization comparison; its startup time is not comparable to Windows runs or earlier Linux builds with different caches and source staging.

Another Windows `Debug Minimal|x64` automated run on 2026-10-04 completed all 60 updates and the four-client assignment/input smoke. It loaded modules in 51.32 s: `Base.rte` took 16.40 s and `Missions.rte` 23.39 s. This is consistent with the previously observed high debug-run variance and does not indicate a regression or speedup relative to the optimized warm Release measurements. The full run reported 62.49 ms average simulation update and 13.00 ms actor AI; it generated 49 actors and 317 particles in the stress scenario.

## Windows gameplay debug run: terrain patch apply smoke

On 2026-10-04, `Debug Minimal|x64` ran `-debug-run 60 -debug-overlay -debug-run-output WindowsNativeGameplaySmoke` on the Ryzen 5 3500U / Vega 8 laptop. The hidden OpenGL window path initialized, loaded Tutorial Bunker, spawned the scripted stress batches, captured all three screenshots, and exited with `result=passed`. The run measured 94.22 s module loading, 109.65 s total elapsed time, 124.03 ms average frame time, 36.17 ms average simulation update, 7.82 ms AI, 6.74 ms Lua scripts, and 930,750,464 bytes process resident memory. This single run followed an offscreen SDL attempt and the volume is removable exFAT `H:`; cold/warm cache conditions are not controlled, so startup time is not a before/after optimization claim.

The new gameplay smoke applied and restored one pixel in the material, foreground and background terrain layers on the loaded scene and verified each readback. The same run passed the four-client snapshot/terrain-patch network loopback, replica creation/removal, network input, UTF-8 glyph rendering, and performance-counter checks. Screenshots are in `ScreenShots/WindowsNativeGameplaySmoke/`.

## Separate Windows world-state host and client processes

On 2026-10-04, a hidden Windows x64 host ran `-debug-run 3600 -world-state-server` while a separate client process connected to `127.0.0.1:62725`. The client reported a successful connection, loaded Tutorial Mission / Tutorial Bunker, received a 109-object snapshot, applied 63 snapshots, and also applied received terrain patches. The host logged the client's connection and later disconnect, then finished all 3,600 updates. Both runs exited with `result=passed`. The host averaged 32.74 ms/frame (28.81 ms simulation, 6.38 ms AI, 4.58 ms Lua) and used 937,594,880 resident bytes; this is a feature-smoke measurement from a debug stress run, not a controlled performance benchmark. This verifies Windows process-to-process transport and client application, while full authoritative simulation and host-originated terrain-change visual comparison remain unverified.
