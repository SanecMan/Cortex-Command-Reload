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
