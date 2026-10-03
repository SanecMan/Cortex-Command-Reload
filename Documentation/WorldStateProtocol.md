# World-state protocol

The experimental world-state path serializes data explicitly instead of sending C++ structs or framebuffer pixels. Protocol version 3 carries a server tick, scene revision, UTF-8 Activity class/preset/module and Scene module/preset identity, then top-level actors, items and particles. Each object has a stable host-assigned network ID, class/module/preset identity, transform, velocity, health, team and category. Dynamic `MOPixel` objects without a preset carry material, color, mass, lifetime and sharpness so clients can recreate common transient particles.

The decoded packet starts with a 16-byte header: `CCR1` magic, version, message type, reserved byte, sequence and payload length, all little-endian. The decoder rejects unknown versions/types, malformed UTF-8, non-finite numeric data, duplicate or zero IDs, invalid team/category fields, truncated/oversized packets and counts beyond fixed bounds. This is a versioned experimental protocol; v3 peers are required when sending v3 snapshots.

For network snapshots, the RakNet adapter wraps an LZ4 block in an 8-byte `CCRZ` header containing the uncompressed byte count. It sends the wrapper only when it is smaller than the original packet. The receiver also accepts an uncompressed v3 packet. Decompression validates the advertised size against the 4 MiB protocol limit before allocating. Replaceable snapshots are sent with RakNet `UNRELIABLE_SEQUENCED`; a newer complete snapshot supersedes a delayed or lost one. Connection/session setup and future reliable commands can use reliable delivery separately.

`WorldStateSnapshotBuilder` captures top-level movable objects and tracks host runtime IDs to session network IDs. The host broadcasts full snapshots at 20 Hz while at least one client is connected and skips capture/encoding while idle. It increments scene revision when Activity or Scene identity changes. IDs are local to one host session.

`WorldStateClientReplica` applies received transforms, velocity, rotation, actor health and team on the simulation thread. It matches objects by identity where possible, clones locally available `.rte` presets, creates transient `MOPixel` instances from the additional v3 fields, and removes client-owned clones when they disappear from a later full snapshot. Missing mod presets are reported and skipped. Existing locally owned actors/items are not destroyed when absent from a snapshot because Activities may retain direct pointers. The client still runs its own simulation between snapshots.

The experimental host and graphical client can be started with:

```text
Cortex Command.debug.minimal.exe -world-state-server 8000 -world-state-bind 0.0.0.0 -world-state-players 8
Cortex Command.debug.minimal.exe -world-state-client 192.0.2.10 8000
```

The server defaults to port `8000`, bind address `0.0.0.0`, and 8 clients; the CLI accepts a player limit from 1 to 64. It currently starts the Tutorial Activity, initializes the normal SDL/OpenGL client managers, and logs connection state to `WorldStateServer.log`. The graphical client waits up to 15 seconds for a valid snapshot, resolves the Activity and Scene using module identities supplied by the host, then applies snapshots. Its connection and application counts go to `WorldStateClient.log`.

Verified smoke runs:

- Windows x64 `Debug Minimal` build and hidden `-debug-run` pass protocol, compression round-trip, transient-pixel spawn/removal, four local transport peers and host survival.
- Ubuntu 24.04 x86_64 Meson/Ninja build and SDL offscreen debug run pass the same checks.
- A separate Linux host process and client process connected over loopback; the client loaded `GATutorial/Tutorial Mission` and `Missions.rte/Tutorial Bunker`, applied 156 snapshots, and reported `missing_presets=0`.
- Stress snapshots measured 28,694 → 9,380 bytes on Windows and 27,686 → 9,006 bytes on Linux with LZ4 wrapping.

This remains a state-replication prototype rather than complete gameplay multiplayer. The client continues simulating locally and input commands are not yet accepted by the host. Inventories/equipment, attached objects, terrain changes, complete Lua/Activity state, weapon-specific state, reconnect/session negotiation, interpolation and reconciliation are not fully replicated. The host still needs SDL/rendering initialization; `SDL_VIDEODRIVER=offscreen` enables tests without X11/Wayland but does not make it GPU-free or dedicated. The legacy framebuffer `NetworkServer` and `NetworkClient` sources remain excluded from both build systems and are a separate remote-rendering design.
