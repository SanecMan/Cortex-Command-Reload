# World-state protocol

## Player orders (v11)

Message types 7 and 8 carry a purchase request and result. Both use reliable
ordered transport. The header sequence is a nonzero, monotonically increasing
request ID, independent of input/snapshot sequences. A request body is scene
revision (u32), finite landing X/Y (f32 each), cargo AI mode (u8), craft preset
identity (three bounded UTF-8 strings), cargo count (u16, at most 128) and cargo
preset identities. Purchase packets are limited to 64 KiB. They carry no player
slot, price or client-generated object state.

A result body contains scene revision (u32), accepted flag (u8), charged amount
(finite nonnegative f32) and a bounded UTF-8 reason string. Accepted results have
an empty reason; rejected results have zero charge and a nonempty reason. Request
and result decoders reject truncation, trailing data, malformed headers/UTF-8 and
non-finite values without modifying the caller's decoded request.

The server resolves identities against its installed modules, derives the player
from the connection's active assignment, checks current scene identity before
processing, and invokes the shared validated delivery path. Scene identity refresh
does not enumerate movable objects. New requests are limited to one per 250 ms
per connection. The server caches the last result: repeating that ID returns the
same result even with a changed payload; smaller IDs return `stale_request`.
Rejected fresh IDs also advance the high-water mark. The cache is cleared on
disconnect; IDs are not a persistent account transaction system. Retry only the
latest ID, and reconnect before exhausting the u32 ID space.

Client session callers send `PurchaseRequest` and drain `PurchaseResult` values;
responses are queued with a 64-result bound. The client equipment menu uses these calls. UI callers must associate results with their pending request and
scene revision. The RPC does not replicate complete craft inventories or provide
cross-reconnect purchase receipts.

The experimental world-state path serializes data explicitly instead of sending C++ structs or framebuffer pixels. Protocol version 11 carries a server tick, scene revision, signed Activity state, four finite team-funds values, UTF-8 Activity class/preset/module and Scene module/preset identity, then top-level actors, items and particles and four player-view records. Each object has a stable host-assigned network ID, class/module/preset identity, transform, velocity, health, sprite frame, horizontal flip, team and category. `AHuman` actor records also carry foreground equipped-device identity and, for an `HDFirearm`, inserted magazine identity and round count. Clients switch or clone those devices from locally installed `.rte` presets and apply the authoritative magazine count. Dynamic `MOPixel` objects without a preset carry material, color, mass, lifetime and sharpness so clients can recreate common transient particles. Version 11 retains the bounded terrain-patch frame with scene revision, pixel rectangle, material/foreground/background layer and indexed pixel bytes.

The decoded packet starts with a 16-byte header: `CCR1` magic, version, message type, reserved byte, sequence and payload length, all little-endian. The decoder rejects unknown versions/types, malformed UTF-8, non-finite numeric data, duplicate or zero IDs, invalid team/category/equipment fields, invalid terrain layer/dimensions, truncated/oversized packets and counts beyond fixed bounds. Version 11 peers are required; earlier peers cannot exchange v11 frames. Each fixed player-view record contains the controlled actor's network ID (zero for no actor), a finite camera position, team and active-human flag. Nonzero actor references must resolve to an actor in the same snapshot, and inactive players cannot reference a controlled actor. Assigned clients follow their server-selected view, using the replica actor's interpolated position to smooth the camera between snapshots. The local Activity player view now binds to the selected replica actor for existing HUD identification; visual and remote-control gameplay validation remain outstanding.

On 2026-10-07, Windows x64 Debug Minimal rebuilt for v8. The preflight passed player-view round-trip and rejected a non-actor control reference and a non-finite camera coordinate. A 60-update gameplay smoke passed with 292 replicated objects and 22 human equipment records. Its camera check resolved the actor, rejected the removed actor, restored it, and applied the expected 20-pixel interpolation correction (`x=300`, `y=240` from an authoritative camera probe of `320,240`). The four-client assignment/input/disconnect test also passed. Artifacts are under `ScreenShots/V8ViewPreflight/` and `ScreenShots/V8ViewGameplay/`. A separate-process visual check of camera following and the player HUD remains outstanding.

Windows x64 Final subsequently rebuilt successfully and passed a separate-process v8 Activity/Scene transition through `Tools/RunWorldStateSmoke.ps1`. Both host and client returned exit code 0 and `result=passed`, including their replica camera checks. Reusing the same output prefix also passed after removing reliance on Windows log modification timestamps. Reports are under `ScreenShots/V8FinalPair-Host/` and `ScreenShots/V8FinalPair-Client/`. This confirms the Final protocol and automated lifecycle path; the test's target Skirmish scene enters editing with no controlled actor, so it does not prove camera following of a remote player-controlled actor.

For network snapshots, the RakNet adapter wraps an LZ4 block in an 8-byte `CCRZ` header containing the uncompressed byte count. It sends the wrapper only when it is smaller than the original packet. The receiver also accepts an uncompressed v9 packet. Decompression validates the advertised size against the 4 MiB protocol limit before allocating. Replaceable snapshots are sent with RakNet `UNRELIABLE_SEQUENCED`; a newer complete snapshot supersedes a delayed or lost one.

Clients send 32-byte `InputCommand` payloads through RakNet reliable ordered delivery. Each command includes a client tick, held input-element mask, relative mouse movement, wheel and mouse buttons, signed 16-bit analog move/aim axes, and activity vote flags. On connection, the host sends a reliable `ClientAssignment` control frame identifying the assigned player slot or spectator status. The host validates each input packet and sequence, then applies controls to `UInputMan` on the simulation thread before the Activity update. Mouse impulses are cleared after one simulation step. In graphical host mode `PlayerOne` stays local, leaving three remote gameplay slots; other connected peers can observe snapshots. Assigned remote players can vote to restart or end the current Activity; votes latch on key-down, require unanimity among assigned remote players, exclude the multiplayer lobby, and are rate-limited to one action per three seconds. Spectators do not vote. The host logs a completed vote and queues the transition through the normal Activity restart path.

`WorldStateSnapshotBuilder` captures top-level movable objects and tracks host runtime IDs to session network IDs. The host broadcasts full snapshots at 20 Hz while at least one client is connected and skips capture/encoding while idle. It increments scene revision when Activity or Scene identity changes. IDs are local to one host session.

Remote input slots are reserved per connection, but a reservation is advertised as spectator status until the current Activity activates that slot as a human player. This respects `MaxPlayerSupport`: the one-player Tutorial can stream its world to peers without assigning them unusable gameplay controls. On simulation updates the host configures eligible players before processing their input, then sends a reliable assignment update when the slot becomes playable or returns to spectator status. An Activity transition can therefore activate a previously observing peer without reconnecting. Graphical clients send gameplay input only while assigned a playable slot; only currently playable remote assignments participate in Activity voting.

On 2026-10-07, Windows x64 Debug Minimal rebuilt and passed the four-peer assignment/input/disconnect self-test. A separate host/client transition run then passed with the host reporting two human players in Skirmish Defense and sending `client assignment updated ... to player slot 1` to the observing peer. Both processes logged `result=passed`, and the server log contained no rejected gameplay-input warnings. Artifacts are under `ScreenShots/AssignmentHost/` and `ScreenShots/AssignmentClient/`. This checks Activity roster activation and assignment updates; it does not establish control of a specific actor or a live Activity vote.

`WorldStateClientReplica` applies received transforms, velocity, rotation, actor health and team on the simulation thread. It matches objects by identity where possible, clones locally available `.rte` presets, creates transient `MOPixel` instances from the additional fields, and removes client-owned clones when they disappear from a later full snapshot. Missing mod presets are reported and skipped. In authoritative client mode, local startup objects absent from the server snapshot are removed after clearing controlled-actor and brain references. Isolated replica smoke checks can still preserve locally owned objects. Once the first valid server snapshot arrives, the client stops advancing Lua gameplay callbacks, Activity, Scene pathfinding, MovableMan physics/AI and Activity global scripts. It continues input sampling, networking, terrain patch application, audio and rendering. Newly cloned objects are transferred from MovableMan's pending-add queues into the active render lists, then MOIDs are rebuilt after the previous asynchronous MOID pass completes.

Tracked replica positions now interpolate over the server-tick interval between full snapshots (normally three 60 Hz simulation ticks for the 20 Hz snapshot stream). The client advances interpolation on its fixed simulation updates, follows the shortest wrapped path on horizontally wrapping scenes, and caps a delayed interval at 12 ticks; newly created objects still start directly at their authoritative position. This smooths rendering between received snapshots without enabling client-side gameplay simulation. Other replicated fields continue to update when each snapshot arrives.

When an established client receives a new `SceneRevision`, it resolves and queues the host's Activity and Scene by module identity, then restarts into that pair before applying the new snapshot. If the client lacks the requested `.rte` preset, it logs the mismatch and disconnects instead of continuing to replicate on a different map. The initial connection uses the same resolver.

The revision-following path has passed a live Windows two-process Activity/Scene transition test. Activity voting is handled by the host session, but has not yet had a live gameplay vote test.

On 2026-10-07, the protocol was advanced to v7 for `AHuman` foreground equipment. The snapshot builder records the equipped device's class/module/preset, plus an inserted firearm magazine's class/module/preset and signed round count (negative values retain the engine's infinite-ammo meaning). Replicas resolve those identities from local module presets, switch or clone the foreground device, replace a mismatched magazine preset, apply its count, and clear the magazine or foreground device when the authoritative snapshot says none is present. Windows x64 `Debug Minimal` and `Final` rebuilt successfully. Preflights passed actor/equipment UTF-8 round-trip, preservation of a negative round count, rejection of equipment fields on non-actors, and four-client transport. The automated Tutorial Bunker `Debug Minimal` and `Final` stress runs completed all 60 updates, encoded and compressed 292-object snapshots, applied all 22 captured AHuman equipment states, passed replica and transport checks, rendered the Cyrillic glyph smoke and exited with `result=passed`. The Final smoke loaded official modules only; its cache-warm module load was 6.797 s. Output is in `ScreenShots/EquipmentV7FinalSmoke/`, `ScreenShots/EquipmentV7FinalVerify/` and `ScreenShots/FinalOfficialStartupSmoke/`.

Also on 2026-10-07, separate Windows x64 `Debug Minimal` host and client processes passed the Activity/Scene transition scenario on loopback with protocol v7. The client received an initial snapshot of 162 objects, reported `missing_presets=0`, and applied 37 human equipment states. It queued revision 2 for `Base.rte/Skirmish Defense` on `Base.rte/Ketanot Hills`; both automated processes ended with `result=passed`, and the host recorded the external client's connection, input commands, and disconnect. Logs and screenshots are under `ScreenShots/EquipmentV7TwoProcessHost/` and `ScreenShots/EquipmentV7TwoProcessClient/`. This confirms initial foreground equipment and magazine state are applied across real processes.

A separate automated live host/client run then changed three loaded host magazines to infinite ammo after the client had received its initial snapshot. The client observed the same network object's rounds change from `100` to `-1` at server tick `2057`, with `missing_presets=0`; both 3,600-update host and 600-update client runs exited with `result=passed`. Logs and screenshots are under `ScreenShots/WorldStateAmmoHostFinalCheck/` and `ScreenShots/WorldStateAmmoClientFinalCheck/`. This verifies that a persistent host-side magazine-count change reaches the replica across separate processes. Player-driven firing/ammo depletion, weapon switching, and equipment reconciliation across Activity transitions remain unverified.

The experimental host and graphical client can be started with:

```text
Cortex Command.debug.minimal.exe -world-state-server 8000 -world-state-bind 0.0.0.0 -world-state-players 8
Cortex Command.debug.minimal.exe -world-state-client 192.0.2.10 8000
```

The server defaults to port `8000`, bind address `0.0.0.0`, and 8 network peers; the CLI accepts a connection limit from 1 to 64. Because this experimental session still runs inside a playable host client, local `PlayerOne` is reserved and up to three remote peers are assigned `PlayerTwo` through `PlayerFour`; additional peers receive a spectator assignment. It currently starts the Tutorial Activity, initializes the normal SDL/OpenGL client managers, and logs connection state to `WorldStateServer.log`. The graphical client waits up to 15 seconds for a valid snapshot, resolves the Activity and Scene using module identities supplied by the host, applies snapshots and exposes its assigned player slot through the session API. Its connection and application counts go to `WorldStateClient.log`.

Verified smoke runs:

- Windows x64 `Debug Minimal` build and `-debug-run 60` passed the client-assignment codec and live four-client host assignment test on 2026-10-04. It assigned the three remote player slots and spectator status, applied a remote input to the client's assigned slot, and stayed alive after all clients disconnected. This run also passed compression, transient-pixel spawn/removal, performance counters and UTF-8 glyph checks.
- Windows x64 `Debug Minimal` rebuilt after adding host-side Activity vote handling. `-debug-run 60 -debug-overlay -world-state-server` exited with code 0 and passed the four-client assignment/input/disconnect harness, snapshot replication, and UTF-8 glyph checks. This smoke does not inject Activity vote inputs or verify the resulting live transition.
- Ubuntu 24.04 x86_64 Meson/Ninja build and SDL offscreen `-debug-run 60 -world-state-server` passed the same checks plus four-client host input and disconnect tests. A remote input command reached player slot 0, and the host remained active after all four clients disconnected.
- Separate Linux host and client processes connected over loopback. The client loaded `GATutorial/Tutorial Mission` and `Missions.rte/Tutorial Bunker`; the host logged an applied client input command. The older snapshot-only run applied 156 snapshots with `missing_presets=0`.
- On 2026-10-04, separate Windows x64 host and client processes connected over loopback using the hidden OpenGL gameplay path. The client loaded `GATutorial/Tutorial Mission` and `Missions.rte/Tutorial Bunker`, received a 109-object initial snapshot, applied 63 snapshots and also applied received terrain patches, then disconnected. The host accepted the connection, observed the disconnect, and continued its 3,600-update run to completion. Both debug runs exited with `result=passed`. This confirms separate-process transport, initial scene loading, ongoing snapshot/terrain-patch application, and host survival after client disconnect; it does not prove authoritative simulation or live terrain mutation originating from the host.
- A dedicated terrain-mutation smoke was then added and passed on Windows x64 `Debug Minimal`: the host sent the baseline material value `177` for probe `(784,245)`, removed it using `SceneMan::TryPenetrate`, and sent material `0` in terrain patch sequence `1749`. The separate client log confirms sequence `1749` changed its readback from `177` to `0`; its debug report and the host's 3,600-update report both ended with `result=passed`. This verifies a host-originated terrain edit reaches client game state over separate processes. It does not yet demonstrate visual comparison, reconciliation of the client's independently running simulation, or headless authoritative hosting.
- A two-client Windows x64 `Debug Minimal` run exposed and fixed stale terrain batches across peer joins: a second connection now discards queued patch bytes and schedules a complete baseline from the current terrain. In the repeat run, host sent current material `0` for probe `(784,245)` in sequence `1869`; both distinct clients read back `0` and completed 180 updates with `result=passed`. The host assigned them different player slots, observed both disconnects, survived through all 3,600 updates and also ended with `result=passed`. Debug client transport logs now go to their separate `ScreenShots/<name>/WorldStateClient.log` files so parallel runs no longer overwrite one shared log. This feature smoke used three OpenGL processes simultaneously and is not a controlled performance sample.
- On 2026-10-04, a fresh Linux source staging of the current development revision built with Meson 1.12.1/Ninja and GCC 13.3.0. The resulting binary passed `SDL_VIDEODRIVER=offscreen ./build/CortexCommand -debug-run 60 -debug-overlay`: 60 updates, all protocol/replica/UTF-8 checks passed, and `ldd` reported no unresolved dependencies. A separate offscreen host and debug client then connected over loopback; the client loaded the server Activity/Scene, received 24 initial objects and applied 87 snapshots. The host logged the client disconnect and remained running until explicitly stopped. This verifies transport lifecycle and snapshot application, not a host Activity/Scene change while connected.
- Stress snapshots measured 28,694 → 9,380 bytes on Windows and 27,686 → 9,006 bytes on Linux with LZ4 wrapping.
- On 2026-10-04, the Windows x64 `Debug Minimal` host logged snapshot-builder cost for a separate connected client on a Ryzen 5 3500U / Vega 8 system with 22 GiB RAM: 0.489770 ms and 0.467910 ms average across two consecutive 100-snapshot windows, with 20 top-level objects in each snapshot. The separate client completed a 60-update run with `result=passed`. This measures snapshot capture only (not encoding, terrain transfer or full scene simulation) and is a low-object baseline, not a stress benchmark.
- A Windows x64 `Debug Minimal` stress host on the same system ran 3,600 updates while a separate client connected after the second 16-actor/256-particle debug stress batch. The host reached 47 actors; its 100-snapshot window averaged 1.529070 ms of snapshot capture with 119 replicated objects (the final partial window measured 1.857000 ms for one snapshot with 128 objects). The client applied 66 snapshots, completed 60 updates with `result=passed`, and disconnected; the host continued to 3,600 updates and also passed. The host averaged 36.642 ms per simulation update and 6.202 ms in actor AI during the run. This is a debug stress feature smoke, not a controlled optimization comparison; the later actor/AI measurements should not be interpreted as a pure multiplayer cost.
- A Windows x64 client stress run exposed an access violation in `MovableObject::UpdateMOID`: the asynchronous MOID drawing task could still traverse object collections while the client applied a snapshot and inserted or deleted replica objects immediately after `MovableMan::Update()`. Snapshot application now occurs just after `CompleteQueuedMOIDDrawings()` at the next frame boundary. The rebuilt client passed `-debug-run 180`, received 197 snapshots, and exited with `result=passed`; one sampled update measured 51 matched objects with 0.58 px mean and 4.41 px maximum pre-correction position error. Its separate host continued running after the client disconnected. This addresses the observed object-lifetime race; it does not prove all concurrent accesses in the legacy engine are race-free.
- On 2026-10-05, a separate Windows x64 host (`-debug-run 3600 -world-state-server`) and graphical client (`-debug-run 180 -world-state-client`) passed after disabling client-side gameplay simulation. The client received 35 snapshots with `missing_presets=0`, logged 180 replica ticks and 0 local simulation ticks, and completed its debug run with `result=passed`; its overlay showed 65 actors and 96 snapshot objects at the final sample. The host logged the connection, processed 120 input commands, saw the client disconnect and continued through all 3,600 updates to `result=passed`. This verifies snapshot-driven lifecycle and client-owned replica object insertion/removal for the Tutorial scene, not full inventory/equipment or Lua simulation replication.
- On 2026-10-07, separate Windows x64 host and client processes changed the host Activity to `Base.rte/GAScripted/Skirmish Defense` on `Base.rte/Ketanot Hills`; the client applied the revision at update 133 and both host/client transition checks passed. A second automated host/client run then paused host simulation after client snapshots had been sent. The client completed 180 updates and disconnected normally; the host detected that disconnect while still paused, wrote `network_disconnect_while_paused=passed`, captured its final screenshot and exited with `result=passed`. This validates inter-frame transport polling and Activity/Scene revision handling in the integrated graphical host. The server still starts SDL/OpenGL, and snapshots still do not represent complete gameplay state.

This remains an experimental state-replication prototype rather than complete gameplay multiplayer. Clients no longer simulate gameplay after receiving the first snapshot. The current weapon replication slice covers only an `AHuman` foreground device and the identity/round count of its inserted magazine; other inventory, background-arm equipment, reload/activation state, thrown devices, attached objects, complete Lua/Activity state, prediction/reconciliation and reconnect/session negotiation remain unreplicated. Controls beyond the current input mapping may therefore behave incompletely. The host still needs SDL/rendering initialization; `SDL_VIDEODRIVER=offscreen` enables tests without X11/Wayland but does not make it GPU-free or dedicated. The legacy framebuffer `NetworkServer` and `NetworkClient` sources remain excluded from both build systems and are a separate remote-rendering design.

Protocol v5 added `MOSprite` frame and horizontal-flip state to each object record. Windows x64 `Debug Minimal` and `Final` builds passed hidden `-debug-run 60 -debug-overlay` on 2026-10-04, including the v5 snapshot round trip, compression, four-peer RakNet transport, 387/357-object replica smoke, live transport, performance counters and UTF-8 glyph checks. The Final run loaded `Base.rte` in 60.789 s and all modules in 105.033 s; this was a cold-run outlier and needs a paired warm-cache Release measurement before comparison with previous startup samples.

Terrain changes now enter a 64-pixel dirty grid from penetration, orphan removal, dislodging, silhouette erasure and terrain-object placement. The host snapshots material, foreground and background indexed pixels into bounded 64×64 patches, sends them reliably, and sends a full terrain baseline when a peer joins. Clients queue patches by scene revision and apply them on the simulation thread only after the matching local Scene is loaded. On 2026-10-04 the Windows x64 `Debug Minimal` solution build succeeded and `-debug-run 60 -debug-run-preflight-only` passed dirty-grid clipping/wrap/coalescing and a four-client RakNet snapshot-plus-terrain-patch client-session loopback. A full hidden-window debug run passed the in-game apply/restore smoke for all three terrain layers on the loaded Tutorial Bunker scene, alongside the protocol, replica, input, UTF-8 glyph and performance-counter checks. The separate Windows process run additionally passed an end-to-end host-originated terrain mutation: the client read back material `0` after receiving the host's penetration update for a known probe pixel. This does not establish visual comparison or reconcile the independently running client simulation. The Windows offscreen SDL driver still cannot create the engine's OpenGL window (`Invalid window`); the ordinary hidden OpenGL window path did work on this machine. The abort reporter is safe without a GL context and records the window error and CPU details without a secondary access violation. The Linux host was unavailable during this turn.

An Ubuntu 24.04 x86_64 rebuild passed. Separate offscreen server and client processes connected over loopback: the client loaded `GATutorial/Tutorial Mission` and `Missions.rte/Tutorial Bunker`, applied 111 snapshots and created its replica; the server applied client input commands (120 were logged), observed the client disconnect, and remained running until manually stopped. This verifies the experimental transport lifecycle, not a full authoritative gameplay session. The client process also exposed an FMOD worker-thread shutdown crash on Ctrl+C in this Linux configuration; forced termination was required for that test process, so graceful shutdown needs separate investigation.
### Replica control-pointer lifetime (October 7)

When a mapped object disappears from the authoritative snapshot, has already
been removed locally, or is forgotten during a scene transition, the replica
now releases matching Activity controlled-actor links before discarding the
mapping or destroying the object. The presentation-specific
`ClearReplicaControlledActor` compares pointer identities and validates actor
liveness without dereferencing an externally removed object. It changes
controller fields directly, avoiding gameplay Lua callbacks.

Debug Minimal x64 compiled successfully. The paired Transition smoke
(`ScreenShots/ReplicaLifetimePair-Host/` and `ReplicaLifetimePair-Client/`)
completed in 211.95 seconds with both processes exiting with code 0 and
`result=passed`. This covers normal replica application and Activity/Scene
transition. The fixture does not bind a controlled actor on the client, so the
new controlled-pointer clearing branch was not covered by that run.

Assigned clients now bind their local Activity player view to the authoritative
controlled actor ID and team through `BindPlayerView`. A missing actor clears
the old selection; spectator/inactive assignments clear the view as well.
`SetReplicaControlledActor` enables the existing player HUD identification but
keeps the actor controller disabled. Local simulation remains skipped. This
presentation operation does not call `SwitchToActor`, alter squads, run
`OnControllerInputModeChange`, or play switch sounds.

The new targeted Debug Minimal gameplay test under
`ScreenShots/PlayerViewLifecycle/` passed with exit code 0:
`world_state_player_view_smoke=passed bound=1 removal_cleared=1 forget_cleared=1`.
It binds a live replica actor, verifies player identification and disabled local
control, removes the snapshot actor, restores/binds it, and forgets the mapping.
The existing camera interpolation, UTF-8 glyph, terrain and transport checks
also passed. This verifies pointer/controller behavior, not a visual HUD review
or remote movement/fire gameplay.

The separate-process Debug Minimal Transition pair also passed in 178.6 seconds
(`ScreenShots/PlayerViewPair-Host/`, `PlayerViewPair-Client/`), with exit code 0
and `result=passed` from both peers. Both ran the new player-view lifetime check;
the host applied and the client observed the scene transition. As before, the
transition fixture does not exercise remote controlled-actor movement/fire.

The subsequent `Control` fixture does exercise movement/fire across separate
processes: Debug Minimal peers passed in 178.93 seconds, with the same actor's
11-round magazine decreasing to 9 on host and client and horizontal movement
exceeding eight pixels on both. The client generated InputCommands only after
recording the actor's initial authoritative state. This is a narrow gameplay
test for the Pistol fixture, not a claim of complete equipment/pose/visibility
replication. See `Documentation/DebugRun.md` for commands and limitations.

### Replica rendering and complete world membership

Snapshot clients skip `MovableMan::Update`, which also normally fills the MO
color layer. The client now clears and draws that layer on drawn updates without
running physics, AI, Activity updates or Lua simulation. Attachables and wounds
are repositioned from existing offsets after snapshot application and each
interpolation step; this maintains coherent preset geometry, but does not yet
replicate each limb's authoritative animated pose.

The live client applies snapshots with `authoritativeWorld=true`. Locally spawned
objects not matched to a snapshot ID are removed, and matched scene objects
become replica-owned so later server removal deletes them as well. Controlled
actor and brain references are cleared before destruction. Standalone internal
smoke replicas retain their previous non-exclusive behavior by default. This
does not change normal offline gameplay or any Lua API.

The Debug Minimal Control pair passed in 163.82 seconds under
`ScreenShots/ReplicaRenderPair-Host/` and `ReplicaRenderPair-Client/`, both with
exit code 0. The client observed movement of 8.94177 pixels and 11 rounds becoming
9. Its new membership check passed across received snapshots. The initial
snapshot removed 20 unmatched locally spawned objects and left exactly 68
server objects; later logged counts included 275/275, 24/24 and 32/32 live versus
expected objects, with no missing presets. The final client screenshot was
inspected: actor bodies/limbs now render on the terrain alongside HUD markers,
where the earlier Control screenshot showed markers with missing actor sprites.
Full pose, authoritative fog/visibility, Activity state and all HUD values still
need their own synchronization and tests.

The first Final Control run exposed a separate race in the diagnostic fixture:
native `AHuman::Clone` ran immediately after `MovableMan::Update` had started
async Lua GC. A process dump showed the host main thread inside the assertion
dialog from `LuaMan::GetAndLockFreeScriptState`, not a network deadlock. The
fixture now waits for priority-pool GC/MOID tasks before native cloning. This
wait is diagnostic-only and occurs when creating the control fixture, rather
than adding a synchronization barrier to ordinary gameplay frames. The failed
run's stack report is `ScreenShots/ReplicaRenderFinalPair-Host/HangStacks.txt`.

After that fixture fix, Windows x64 Final rebuilt successfully and the Control
pair passed in 70.51 seconds using the same output prefix. Both peers exited with
code 0 and `result=passed`. Host movement was 8.30072 pixels, client movement
8.85358 pixels, and both observed 11 rounds becoming 10. Player-view lifetime and
client membership checks passed. Logged client counts included 7/7, 50/50 and
18/18 live versus expected objects with no missing presets. The Final client
screenshot was inspected: bodies and preset limbs render on terrain, and the
selected actor shows its 10-round ammo counter and health. These checks do not
extend to the remaining pose/fog/Activity-state gaps described above.
### Version 9 Activity presentation

Version 9 adds one signed byte immediately after the snapshot scene revision.
It carries the existing engine ActivityState values: NoActivity (-1), NotStarted
(0), Starting (1), Editing (2), PreGame (3), Running (4), HasError (5), Over (6).
The encoder and decoder reject values outside this range, and the snapshot
builder checks the enum bounds at compile time. Both peers must use v9; this
changes only the experimental world-state protocol, not `.rte` data, Lua APIs
or the legacy framebuffer multiplayer path.

The authoritative replica applies the state with the existing non-virtual
`SetActivityState` setter. It does not call Activity lifecycle functions, Lua
callbacks or local simulation. Pause is separate from ActivityState and is not
carried by this field; paused-host updates still need their own synchronization.
Team funds, winning team and full Activity-specific HUD data also remain separate
work.

Windows x64 Final compiled successfully. `ScreenShots/ActivityStateV9Preflight/`
passed a codec round-trip of every enum value, encoder rejection of an invalid
value and decoder rejection of an invalid state byte in an otherwise valid
packet. The separate-process Control pair under `ActivityStateV9Pair-Host/` and
`ActivityStateV9Pair-Client/` passed in 54.14 seconds, both with exit code 0.
The client checked local Activity state against every applied snapshot and
reported `world_state_activity_presentation=passed`; logged snapshots of Tutorial
and Skirmish both carried Running (4). Movement/fire, object membership and
player-view checks also passed. The final client frame was inspected and showed
rendered actors and the selected actor's ammo/health counters. Editing and Over
have codec coverage, but their live UI transitions were not exercised by this
fixture.

## Authoritative team funds (v10)

Four little-endian IEEE-754 float balances follow the signed Activity-state byte
in snapshot payloads. All values must be finite; negatives and fractions remain
valid to preserve existing economy semantics. The snapshot builder reads all
teams from Activity, and authoritative client application uses SetTeamFunds
without running economy callbacks or local simulation. Wire-version mismatch
rejects v9 peers; rebuild client and server together.

Windows Final passed the finite-value/invalid-value codec probe, a two-client
dedicated-server test with live console fund changes (40.28 seconds), exact
Activity balance checks after every applied snapshot, and the existing Control
movement/fire/Activity-transition regression (56.19 seconds). This replicates
balances; it does not implement remote purchases, deliveries or all Activity UI.
