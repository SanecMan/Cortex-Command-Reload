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
