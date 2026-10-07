# Dedicated server feasibility

## Assessment

A dedicated server is technically feasible without replacing the simulation engine, but it is not a small switch around the existing network server. The current network implementation is a remote-rendering protocol: the host runs the game and sends encoded framebuffer updates, scene/terrain data, sound events, and effects. The clients send input to that host. A headless server that sends authoritative world state needs a new replication protocol and a startup path that does not initialize the client renderer.

## Current architecture

- In the legacy, currently unbuilt `NetworkServer` source, the server receives `MsgInput` and gates simulation on frame delivery/consumption. Its start path binds an IPv4 socket and hardcodes four connections (`Source/Managers/NetworkServer.cpp`, `Source/Managers/NetworkServer.h`).
- That legacy server maintains per-client 8-bit game and GUI buffers, compression state, frame queues, terrain changes, and send threads. It sends frame setup and pixel-line/box messages (`NetworkServer::SendFrame`, `SendFrameSetupMsg`, `SendSceneData`).
- The legacy `NetworkClient` reconstructs those images into `FrameMan` network buffers. It also applies scene setup and terrain updates (`Source/Managers/NetworkClient.cpp`). `Source/System/NetworkMessages.h` confirms that the legacy wire message set is frame/pixel-oriented; there are no replicated movable-object or actor-state messages.
- The old protocol made the host process authoritative, but tied hosting to a regular graphical game instance. The current executables do not include that obsolete framebuffer implementation. They now include the separate experimental WorldStateServerSession/WorldStateClientSession path, with server-owned objects, input assignments, terrain patches, interpolation and Activity phase replication; this still hosts from the graphical executable.
- Multiplayer sources are commented out of `Source/Managers/meson.build` and `Source/Activities/meson.build`, and are also absent from the `ClCompile` items in `RTEA.vcxproj`. The framebuffer-based networking code is therefore a legacy implementation that is not built into either current client target; it cannot be treated as a working compatibility server without first restoring and validating it.
- `Main.cpp` unconditionally calls `SDL_Init(SDL_INIT_VIDEO | ...)` before manager initialization. `WindowMan`, OpenGL resources, `FrameMan`, audio and input are global managers. A server mode therefore needs explicit client/server manager initialization, not just hiding the window.
- `c_MaxClients` is 4 and affects shared arrays in activities, audio and networking. Raising the player count requires auditing all these fixed-size interfaces and content assumptions.

## Recommended design

Keep one simulation and introduce a server-authoritative replication layer:

1. **Build a state-protocol baseline.** Keep the obsolete framebuffer managers isolated; compile a transport adapter for the versioned world-state packets and prove loopback delivery before integrating it with gameplay.
2. **Define the replicated state.** Begin with stable network IDs, actor ownership, position/velocity, health/status, inventory/equipment changes, and lifecycle events (spawn/delete). Add terrain deltas and Activity state as versioned messages. Continue using existing `.rte` preset loading and Lua Activity scripts on the authoritative process.
3. **Send input upstream and snapshots downstream.** Clients send input commands with sequence/tick numbers. The server applies them in the simulation loop, then publishes snapshots. Clients render interpolated snapshots and predict only local input if needed. Keep the old framebuffer protocol behind a compatibility mode during transition.
4. **Separate client startup from simulation startup.** Add a server entry point that skips SDL video, `WindowMan`, OpenGL, UI and client input. Audit simulation dependencies on `FrameMan`, `AudioMan`, `CameraMan`, `PrimitiveMan` and GUI globals; replace only simulation-critical calls with no-op/service interfaces where required. Keep terrain and collision data available to the server.
5. **Add server CLI/config and operational behavior.** Configure bind address, port, player limit, activity, scene, module list and logs. Handle connect/disconnect without terminating the session. Avoid NAT punch-through as a requirement for direct-connect dedicated servers.
6. **Port and verify Linux.** Build the simulation/network target first; do not include OpenGL/window libraries in the dedicated target. Verify with a headless process, two clients, disconnect/reconnect, terrain changes, and long-running sessions on the available Linux host.

## Main engineering risks

- Movable objects and activities use polymorphic presets and Lua callbacks; raw object pointers cannot be replicated safely. Network IDs and explicit serialization are required.
- Simulation is not demonstrated deterministic across different machines, so the server must be authoritative and clients must accept snapshots rather than independently trusting their simulation.
- Lua scripts may call presentation/audio APIs. These calls need to be harmless on a server while preserving existing mod APIs.
- Terrain is both simulation data and a render asset. The server can omit visual rendering, but it cannot omit collision/material terrain data.
- Existing four-player arrays and Activity scripts may encode the current player limit. Increasing capacity must be a deliberate compatibility change.
- The current networking implementation has per-client send threads and shared global state. Thread safety must be audited before reusing that threading model for world snapshots.

## Feasibility conclusion

The preferred outcome is one shared simulation library used by the client and server, with separate client presentation and server hosting executables. A first deliverable can reuse the existing simulation and `.rte` loading while disabling presentation. The host session now accepts versioned client input and applies it to the four existing player slots before Activity updates. Full gameplay reconciliation and the presentation-free simulation startup path remain incomplete. Slot assignments, remote movement/fire and basic player-view binding have Windows test coverage; they do not establish a completed dedicated server. Linux has a working Meson build for the current graphical client; that does not establish a Linux headless server target.

## Verified build status

Windows x64 (`Debug Minimal`) and Ubuntu 24.04 x86_64 Meson/Ninja builds both pass the automated `-debug-run 60` scenario; Linux uses SDL's offscreen driver without X11 or Wayland. The test clears/replaces its own previous screenshots and logs, checks BOM/legacy CP1251/UTF-8 handling (including supplementary code points), and exits nonzero on failed checks. It captures initial, stress and final frames and records frame/simulation/Lua/AI timings, object counts and resident memory. Debug mode skips audio initialization after repeatable FMOD worker-thread shutdown crashes on Linux; normal game launches retain audio initialization. Current Windows and Linux debug-run logs report `result=passed` and exit status 0.

The versioned world-state codec, RakNet transport, host session, and client snapshot receiver compile on both platforms. Debug tests start the host session in the simulation process, connect four local client sessions, validate received snapshots, verify that empty hosts skip snapshot broadcasts, disconnect all clients, and confirm that the host remains alive. Server object IDs are monotonically assigned for each live object within a session. This proves local multi-peer session wiring. Snapshot replication and client-side application are now implemented for top-level actors, items and particles, with transient MOPixel support; separate Linux host/client processes have also been exercised.

The `-world-state-server` mode still starts simulation inside the graphical client executable and initializes SDL video, WindowMan, OpenGL and render managers. The experimental world-state client skips local MovableMan simulation and applies authoritative object, terrain, equipment and Activity-phase snapshots. Windows smoke tests cover remote movement/fire, assigned player presentation and scene transitions. They do not prove complete Activity-specific Lua/HUD synchronization, reconnect or production gameplay compatibility. Linux evidence predates these protocol changes; current Linux operation remains unverified while the test host is unavailable. A GPU-free dedicated executable and independent server simulation startup are still required.

## Completion checks

- Server target builds on Windows x64 and Linux x64.
- Server starts without a window or graphics context and loads the selected scene/activity.
- Two independent clients connect to the server; simulation continues after either client disconnects.
- Actor, inventory, terrain and Activity changes converge from server snapshots.
- Existing single-player and host/client content continues to load, including representative legacy `.rte` modules and Lua Activities.
## CPU graphics resource foundation (October 7)

`GLResourceMan::Initialize(false)` selects a mode without GPU resource creation.
It preserves CPU bitmap data and size/format metadata but returns zero texture,
program, framebuffer and upload-buffer IDs. `RenderTarget` records its selected
backend at construction and avoids GPU allocation, Begin/End commands and GPU
destruction in this mode. `BigTexture` retains bitmap dimensions and pixels
without querying maximum GL texture size, allocating tile textures or uploading.
This is an initialization-time choice, not a supported runtime renderer switch
or a software renderer.

Windows x64 Final compiled and passed
`-debug-run 60 -debug-run-cpu-graphics-preflight -debug-run-output CPUGraphicsFoundation`
with `SDL_VIDEODRIVER=invalid-driver-for-headless-test`. The probe initializes only
SDL core, constructs default managers needed by FrameMan, selects GPU-disabled
resources, initializes CPU frame buffers and palette assets, and creates a
bitmap SceneLayer, RenderTarget and BigTexture. It checks zero GPU IDs, retained
dimensions and pixel value 42 after upload/draw calls. The report says
`cpu_graphics_preflight=passed video_initialized=0 gpu_enabled=0`, exit code 0.

The full game still initializes SDL video, WindowMan, UI and the renderer.
This foundation does not load an authoritative headless Activity, create a
dedicated binary or prove a Linux server build. The next required boundary is
server manager startup and a simulation loop that does not call client rendering
or loading-screen drawing. CPU collision bitmap loading must remain available.

Normal GPU startup also passed a Windows Final 60-update diagnostic run
(CPUGraphicsClientRegression, exit code 0). Its final screenshot was inspected:
scene terrain, actor sprites, HUD and the UTF-8 diagnostic text rendered.
This is a short startup/render regression check, not a multiplayer gameplay test.

## Headless content initialization (October 7)

The Windows Final executable now passes the headless content probe documented in DebugRun.md: all 10 official modules load through PresetMan with SDL video uninitialized and an invalid video driver. Common manager construction is shared with client startup; the probe initializes Lua and CPU simulation resources, uses LoadingScreen's log-only path and skips shader compilation in CPU mode. It resolves the Tutorial Bunker scene preset and tears down loaded content without a window. This advances the content boundary; an Activity simulation loop, dedicated executable and live headless client/server test remain required. Linux execution of this path is not yet verified.

## Headless Activity simulation (October 7)

Windows Final now starts the real Tutorial Mission/Tutorial Bunker without SDL video and runs 60 updates through Lua, Activity, scene, movable physics and late global scripts. The headless probe observes a moving Gold Particle and 17 actors; it passes with an unavailable video driver and no window. MovableMan exposes an optional color-layer draw flag, preserving MOID drawing for collision queries. Activity-owned CPU UI state is retained. This establishes a short simulation foundation, not a completed dedicated server: standalone startup, live headless host/client integration, session lifecycle and Linux verification remain required.

## Headless authoritative host integration (October 7)

The headless diagnostic simulation now uses the existing WorldStateServerSession transport with remote Player 1 enabled. A Windows Final host with no SDL video accepted two separate graphical clients, delivered authoritative snapshots and input commands, and continued 1835 updates after both disconnected. The two clients ran 600 replica updates with zero local simulation ticks. Tutorial permits one active human, so the second client was an inactive/spectator connection. This validates headless host/replica integration inside the client executable, not a completed standalone server. Production CLI/configuration, independent binary, full gameplay/HUD synchronization and Linux testing remain required. See DebugRun.md and Tools/RunHeadlessWorldStateSmoke.ps1 for the exact bounded smoke workflow.
