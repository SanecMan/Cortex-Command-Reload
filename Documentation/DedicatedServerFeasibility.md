# Dedicated server feasibility

## Assessment

A dedicated server is technically feasible without replacing the simulation engine, but it is not a small switch around the existing network server. The current network implementation is a remote-rendering protocol: the host runs the game and sends encoded framebuffer updates, scene/terrain data, sound events, and effects. The clients send input to that host. A headless server that sends authoritative world state needs a new replication protocol and a startup path that does not initialize the client renderer.

## Current architecture

- In the legacy, currently unbuilt `NetworkServer` source, the server receives `MsgInput` and gates simulation on frame delivery/consumption. Its start path binds an IPv4 socket and hardcodes four connections (`Source/Managers/NetworkServer.cpp`, `Source/Managers/NetworkServer.h`).
- That legacy server maintains per-client 8-bit game and GUI buffers, compression state, frame queues, terrain changes, and send threads. It sends frame setup and pixel-line/box messages (`NetworkServer::SendFrame`, `SendFrameSetupMsg`, `SendSceneData`).
- The legacy `NetworkClient` reconstructs those images into `FrameMan` network buffers. It also applies scene setup and terrain updates (`Source/Managers/NetworkClient.cpp`). `Source/System/NetworkMessages.h` confirms that the legacy wire message set is frame/pixel-oriented; there are no replicated movable-object or actor-state messages.
- The old protocol made the host process authoritative, but tied hosting to a regular graphical game instance. The current executables do not include that networking code, so there is no active multiplayer mode to reuse as a working dedicated server or world-state replication.
- Multiplayer sources are commented out of `Source/Managers/meson.build` and `Source/Activities/meson.build`, and are also absent from the `ClCompile` items in `RTEA.vcxproj`. The framebuffer-based networking code is therefore a legacy implementation that is not built into either current client target; it cannot be treated as a working compatibility server without first restoring and validating it.
- `Main.cpp` unconditionally calls `SDL_Init(SDL_INIT_VIDEO | ...)` before manager initialization. `WindowMan`, OpenGL resources, `FrameMan`, audio and input are global managers. A server mode therefore needs explicit client/server manager initialization, not just hiding the window.
- `c_MaxClients` is 4 and affects shared arrays in activities, audio and networking. Raising the player count requires auditing all these fixed-size interfaces and content assumptions.

## Recommended design

Keep one simulation and introduce a server-authoritative replication layer:

1. **Restore a buildable network baseline.** Re-enable networking sources in one build system, fix compile errors against current manager APIs, and add a loopback host/client test. This is a prerequisite to protocol work.
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

The preferred outcome is one shared simulation library used by the client and server, with separate client presentation and server hosting executables. A first deliverable can reuse the existing simulation and `.rte` loading while disabling presentation; true authoritative world-state multiplayer follows after the object protocol exists. Linux support is plausible because the repository has a Meson build and cross-platform engine code, but it is not currently established for the network target: networking is excluded from Meson and must be repaired before a Linux server build can be claimed.

## Verified build status

The current development branch builds the Linux x86_64 client with Meson/Ninja, and its `-debug-run 60` smoke test passes using SDL's offscreen video driver without X11/Wayland. This confirms the Linux client build and hidden diagnostic path only. The process still initializes the renderer/OpenGL context; it is not a dedicated server, does not prove GPU-free simulation, and does not validate network replication.

## Completion checks

- Server target builds on Windows x64 and Linux x64.
- Server starts without a window or graphics context and loads the selected scene/activity.
- Two independent clients connect to the server; simulation continues after either client disconnects.
- Actor, inventory, terrain and Activity changes converge from server snapshots.
- Existing single-player and host/client content continues to load, including representative legacy `.rte` modules and Lua Activities.
