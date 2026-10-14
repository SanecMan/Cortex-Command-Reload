# Dedicated server

## Connect from the main menu

For a disposable authoritative reload/HUD test on Windows, run
`Tools/RunDedicatedServerSmoke.ps1 -CheckMenuConnection -CheckReloadHud`.
The server creates a player-controlled actor, uses the normal Reload path and
then changes server timer progress. The client saves `debug-run-reload-hud-35.png`
and `debug-run-reload-hud-75.png` inside the selected debug output prefix. This
fixture modifies only its temporary runtime session, not saved game content.

Choose **Multiplayer** in the main menu, enter the server's IPv4 address or
hostname and UDP port (default `8000`), then click **Connect**. The menu remains
responsive while it waits for the first valid world snapshot (up to 15 seconds).
**Back to Main Menu** or Escape cancels the pending connection. Invalid ports,
connection timeout and unavailable Activity/Scene presets appear on this screen;
you can correct the settings and retry without restarting the game.

This connects to the authoritative world-state server, not the legacy streamed
framebuffer server. Client and server must use the same protocol version and
have matching `.rte` content installed. A successful connection replaces the
current Activity; cancelling keeps the previous game. Starting local content
from a paused network session stops snapshot replication.

For an automated Windows check through the actual menu handlers, run
`Tools/RunDedicatedServerSmoke.ps1 -CheckMenuConnection` with the client/server
binary paths and `-GameDirectory` if using an out-of-tree build. This checks
invalid port rejection, cancellation, retry, two separate clients, native
snapshot replication and continued server ticking after disconnect. Debug
screenshots are saved as `main-menu.png` and `multiplayer-menu.png` in each
client's normal debug output directory. The explicit client option is
`-debug-run 60 -debug-run-menu-connect 127.0.0.1 PORT`; normal launches never
perform these automated menu actions.

Protocol v19 additionally replicates FrameMan screen messages for the assigned
player, including UTF-8 text, blinking and centered placement. See
[the message wire format](WorldStateProtocol.md#screen-messages-v19).
For an automated headless check of screen-message capture, use
`Tools/run_server_smoke.py --probe-clients --screen-text` with the usual executable
and game-directory arguments, or run
`CortexCommandServer --probe-client ADDRESS PORT --screen-text` directly.
The runner creates a fixed Cyrillic message through the server console's
`debug-screen-text` fixture before connecting clients. `debug-screen-text-clear`
clears it. Tutorial instructions themselves are drawn through a separate native
HUD path; this fixture does not establish replication of those instructions.
The fixture resets the Activity message timer as well as setting FrameMan text;
otherwise ordinary Activity updates clear it after five seconds. The graphical
runner refreshes it while clients load.
For graphical clients, use `Tools/RunDedicatedServerSmoke.ps1 -CheckScreenText`
with the matching client/server binary paths; `-CheckMenuConnection` can be added.
Both diagnostic probes must receive a message from an active player; a spectator
probe can inspect all player records but does not display their messages locally.

Windows Meson verification on 2026-10-14: the headless screen-text run passed in
19.28 seconds with two separate clients and 127 additional ticks after disconnect.
Two consecutive graphical menu/message runs passed in 28.95 and 30.15 seconds,
with 62 and 63 additional server ticks and clean exits from both clients. The
assigned client's final screenshot visibly showed the centered Cyrillic message.
The delivered native Windows Final client/server pair passed again:
`ScreenTextV19DeliveredHeadless` took 19.39 seconds with 127 post-disconnect ticks,
and `ScreenTextV19DeliveredMenu` took 32.46 seconds with two graphical clients,
63 post-disconnect ticks and clean exits.
Linux v19 run
[38462923605](https://github.com/SanecMan/Cortex-Command-Reload/actions/runs/38462923605)
passed compilation, the standalone screen-message codec test, device-free
message capture/application and two separate processes receiving HUD text. The
build-tree check took 13.24 seconds (121 post-disconnect ticks), and the extracted
package check 9.61 seconds (120 ticks). Both passed UTF-8 config/CLI and SIGTERM.
This does not establish graphical Linux glyph rendering or arbitrary Lua HUD
synchronization.

Those runs exposed a metadata-only FMOD system being released during CRT teardown.
Audio shutdown now resets it explicitly even when the regular playback device is
disabled. The metadata cache and logical voices are cleared together; later
initialization can lazily reopen metadata access. Repeated graphical shutdown
passed after this change. This does not implement network sound event playback.

Windows validation on 2026-10-08: `MultiplayerMenuPair` passed with two clients
using the real GUI handlers and the native world-state transport in 40.79 seconds.
Both debug runs passed port validation, cancellation, retry, scene loading and
snapshot replica checks. The headless server continued 65 updates after both
clients disconnected. The two menu screenshots were visually inspected at
960x540. This is an automated handler/visual check, not a manual mouse test.
The native MSBuild `Final|x64` client was then rebuilt in the checkout and
`MultiplayerMenuFinalPair` passed against the matching server binary there:
two clients, 64 updates after disconnect, all processes exited successfully.
Its total verification time was 167.71 seconds including startup, not a benchmark.

## Connection failure handling

The world-state client now reports explicit transport failures: server capacity,
bans, password rejection, incompatible RakNet protocol, connection refusal,
server shutdown and lost connection. The direct-connect menu displays that
reason and re-enables its fields immediately instead of waiting for the generic
15-second snapshot deadline. CLI clients also stop waiting after a terminal
transport failure, and do not load a snapshot received just before disconnect.
A recognizable incompatible *world-state wire version* now immediately stops
the client peer and reports both server/client versions with an instruction to
update them together. This checks the bounded stable packet envelope before
parsing a foreign body, including LZ4-wrapped snapshots. Unrecognizable or
malformed envelopes remain ignored; the RakNet protocol error is a separate
transport response. Existing replicas stay frozen until explicit retry.

On an established session's failure, the peer stops and the last authoritative
snapshot stays frozen. This deliberately prevents the current game loop from
resuming local physics/Lua for a remote world. Explicit Disconnect/Connect
clears old snapshots, sequence counters, player assignment, terrain patches and
purchase data before retry. An established connection failure now stops simulation ticks and opens the
Multiplayer connection screen with its reason and the remembered address/port.
Connect and Back remain usable; Resume is hidden for the disconnected world.
The transition pauses the clock without invoking local Activity pause callbacks.
Starting local content clears the disconnected-session restriction; successful
server connection clears replica caches and that restriction. Automatic retry
and restoration of the previous server-side player identity are not implemented.

Every normal diagnostic simulation run now includes
`world_state_client_failure_smoke`: a real one-slot loopback server sends a
snapshot to the first client, rejects the second as full, shuts down, and then
accepts a fresh connection. It checks frozen-state handling and clean reconnect.
It also sends real uncompressed older-version and compressed newer-version
snapshots through RakNet, checks explicit refusal without replacing the last
snapshot, and reconnects successfully to the matching version.
Windows Meson verification on 2026-10-14: `ProtocolVersionRefusal` completed
60 simulation updates with exit code 0. `version_refused`,
`compressed_version_refused` and `version_retry` all reported true alongside
the existing capacity/disconnect checks. This exercises actual loopback packets
with altered envelope versions, rather than building historical server binaries.
The root MSBuild `Final|x64` client and matching server then passed
`ProtocolVersionNativeFinalPair`: two clients connected through the menu in
31.49 seconds, all failure/retry gates passed, and the server continued 63
updates after disconnect. No protocol version or content format was changed.
On Windows, `ClientFailureLoopback` passed on 2026-10-13 with all four checks true
and exit code 0. Both client/server targets built and linked in the Windows
Meson build directory; no `.rte`, Lua API or world-state wire format changed.
`ClientFailureMenuPair` then passed two separate clients through the main-menu
connection flow in 28.68 seconds; both passed the failure/reconnect loopback
check, and the headless server continued 63 updates after their disconnect.

### Real server shutdown UI check

`Tools/RunWorldStateDisconnectSmoke.ps1` starts an actual headless server and a
hidden client, waits for 60 replica updates, sends the server's `quit` command,
and checks the client's automatic menu return. The explicit debug option is
`-debug-run-disconnect-menu` alongside `-debug-run` and `-world-state-client`.
It saves `connection-lost-menu.png`, verifies the retained authoritative snapshot
and zero client-local simulation ticks, then runs the normal diagnostic gates
and exits. Normal launches never automatically exit at this menu.

`DisconnectMenuReal` passed on Windows on 2026-10-13 in 15.25 seconds: both
processes exited 0, `disconnected_server_menu=passed`, `result=passed`, and
`client_local_simulation_ticks=0`. The 960x540 screenshot was inspected; reason,
Connect/Back controls and the remembered endpoint were readable. This test
covers orderly server shutdown and menu return, not a restarted server or
silent network cable loss. Test targets are in `_Bin/MesonPortableEntry`; the
root native Final executable is not updated by these Meson checks.
`DisconnectMenuRegression` also passed two clients through the ordinary menu
connection path in 29.04 seconds, with 63 server updates after disconnect.
The shutdown script clears its known server logs and removes the previous client
report before launching; this prevents stale `simulation_update` lines from
ending a repeated test early. Client screenshot/log cleanup is still performed
by the normal debug runner. A repeated `DisconnectMenuReal` run passed after
that harness correction in 15.43 seconds.

### Reconnect to a restarted server

The persistent game loop now loads the Activity selected by Connect and resumes
its own frame loop after the disconnect menu. Returning from that frame loop
would instead tear down the application; the end-to-end restart test exposed
and fixed that transition bug.

Run `Tools/RunWorldStateDisconnectSmoke.ps1 -CheckReconnect` to verify the full
path in one client process. After the initial Tutorial session, the script stops
the real server, waits for the disconnect menu, restarts the same endpoint with
Skirmish Defense / Ketanot Hills, creates server-side craft/passenger/inventory
objects, then requests Connect through the native menu handler. Readiness uses
a debug-output marker, which normal launches ignore. The client checks the new
Activity/Scene, non-empty matching replica membership, player assignment and
zero local simulation ticks, runs 120 additional replica ticks and exits.
The wrapper also rejects inventory/attachment mismatches. This creates a new
server session; it does not restore the old server-side player identity.

Windows Meson `ReconnectChangedWorld` passed on 2026-10-13 in 26.93 seconds:
`menu_reconnect_queued=passed`, `menu_reconnect_world=passed`,
`world_state_menu_reconnect=passed`, `result=passed`, and both server instances
and the client exited 0. The new network log had matching live/expected object
counts, matching inventories, five native attachment states checked with zero
mismatches, 120 replica ticks and zero local simulation ticks. The duration is
verification time, not a controlled performance benchmark.
The native MSBuild `Final|x64` client was subsequently built in the checkout.
`ReconnectNativeFinal` passed the stronger restart fixture against the matching
server binary in 28.92 seconds, with the same reconnect/native inventory gates,
120 replica ticks, zero client-local simulation ticks and exit code 0 for all
processes. Its final screenshot was inspected and showed the Ketanot Hills
terrain rendered. Both root executables were updated by this verification;
this is not a manual player-control or broad mod gameplay test.

## Device-free playback lifetime

When AudioMan has no playback device, `SoundContainer` now maintains logical
voices on the simulation clock. `Play`, `IsBeingPlayed`, `Stop`, `Restart`,
overlap/ignore/restart policies, loop counts, pause, pitch and fade govern those
lifetimes without creating FMOD channels. Metadata supplies selected clip lengths.
SFX/UI/music lifetimes are kept separately, including when a container changes
bus for a later play. Ingame pause/finish-loops affect SFX; global stop and pitch
controls follow the existing bus selection. Copy/reset/destruction do not inherit
or leave active voices. The existing 128-voice limit applies across the buses.

Registry access is serialized by a mutex; this does not make simultaneous
arbitrary mutation of one SoundContainer/SoundSet thread-safe. Actual playback
channel IDs remain empty in device-free mode. The ordinary enabled-audio client
continues to use FMOD channels. Unlike that real-time audio path, device-free
lifetimes follow simulation time and therefore stop advancing when simulation
is stopped. Sound event transport, audible-volume modeling and client playback
of server sounds are still incomplete.

The headless diagnostic includes deterministic timeline tests for expiration,
finite/infinite loops, overlapping voices, local/group pauses, pitch changes and
fade. It also exercises actual SoundContainer play policies, the 128-voice cap,
stop/restart/fade/reset/copy, position/volume setters and AudioMan global controls,
checking that no physical channels are created.

Windows Meson checks on 2026-10-14: `HeadlessPlayback` completed 60 simulation
updates with `playback=1`, `soundless=1`, invalid SDL video/audio drivers and
`gpu_enabled=0`. `HeadlessPlaybackPair` passed two clients through the menu and
inventory/wound mutation in 31.03 seconds, with zero replica mismatches and 64
server updates after disconnect. These checks cover lifetime/API behavior and
ordinary network regression, not listening to remotely replicated sound.
The root MSBuild `Final|x64` client was rebuilt and paired with the matching v18
server. `HeadlessPlaybackNativeFinal` passed the device-free lifetime/API probes
and 60 simulation updates; `HeadlessPlaybackNativeFinalPair` passed two clients
and the inventory/wound fixture in 33.65 seconds, with 64 updates after
disconnect. These binaries preserve the existing wire version and content APIs.

## Sound metadata without playback

Headless content keeps sound file paths without FMOD playback handles.
`SoundContainer::GetLength` now reads lengths lazily through a separate FMOD
system configured with `NOSOUND_NRT`, `OPENONLY` and `ACCURATETIME`. It neither
selects an audio device nor plays channels, and normal content loading does not
create this metadata system. Accurate-time scanning may read compressed files
on the first query; successful results use a bounded, mutex-protected cache.
The temporary sound is released immediately. The regular AudioMan stays disabled.
See FMOD's [mode definitions](https://www.fmod.com/docs/2.04/api/core-api-common.html).

Property updates skip absent playback handles. Runtime `SoundSet::AddSound`
retains metadata when playback is disabled, matching the INI reader. Empty
containers return zero for both length selection modes, and unreadable metadata
returns zero without dereferencing a null handle. Existing alternative-extension
rules (`.wav`, `.ogg`, `.flac`) are used without changing the stored content path.
The live playback path continues to use its existing sound handles.

The automatic headless-content diagnostic now probes a generated 100 ms WAV
under a Cyrillic path, both selection modes, parallel cached queries, extension
fallback, an absent file, an empty container and the official Confirm FLAC.
Its generated WAV is removed after a completed probe; normal repeated debug
directory cleanup removes it if a run is interrupted. This supplies simulation
length queries (including firearm reload timing), not replicated audio playback.

Windows verification on 2026-10-14: repeated `HeadlessSoundLength` runs passed
with an invalid SDL video/audio driver, 10 official modules, 60 simulation updates
and `gpu_enabled=0`. WAV/next/alternative lengths were exactly 100 ms, Confirm
FLAC was 204 ms, missing/empty cases were zero, parallel queries passed, and the
generated file was removed. The first probe exposed a discarded runtime sound
record and an empty-next-selection crash; those were corrected and rerun.
`SoundLengthMenuPair` passed two real clients through the ordinary connection
and inventory/wound fixture in 59.61 seconds, with 67 server updates after their
disconnect. Linux metadata/runtime and actual audio event replication remain
unverified; these durations are not controlled performance measurements.
The rebuilt root MSBuild `Final|x64` game passed `SoundLengthNativeHeadless`
with the same exact duration/cleanup checks and GPU disabled.
`SoundLengthNativeFinalPair` then passed both menu-connected clients and the
inventory/wound fixture in 32.29 seconds, with 63 server updates after disconnect.
Both root executables were updated; protocol v18 and `.rte`/Lua APIs are unchanged.

## Wound replication (protocol v18)

Protocol v18 adds a numbered `Wound` role to each rotating object's attachment
tree, including wounds on limbs and other nested attachments. Wounds are kept
in the native `MOSRotating` wound list, not the ordinary attachment list. Client
reconciliation preserves matching instances, applies sampled pose/emission
visuals, removes obsolete wounds and restores list order. Replacing a limb also
retires its owned wounds. Both endpoints must be rebuilt together.

Replica-only removal updates owned mass and frees the wound without selecting
random gameplay wounds or inflicting damage. Adoption uses `AddWound` with the
gib-limit check disabled. No local emitter Update, injury simulation or particle
spawning runs on the client. `.rte` wound definitions and Lua APIs are unchanged.
Wound slots must be contiguous for each parent; only emitter classes with valid
emitter state are accepted. Existing attachment count/depth limits still apply.

Diagnostic native tests exercise wounds on the body and arm, instance retention,
reordering, removal and final mass restoration. The two-client inventory fixture
now starts with three wounds, then removes a wounded arm and replaces body wounds
with one moved wound; initial/mutated stages require those wire records and
native attachment matching. Native tests also compare the wounded source and
replica's CPU-rendered pixels and require the image to change after wound removal.
Replica removal detaches the wound under callback suppression before deletion,
unregistering mod-provided pie slices/listeners. Replicated sound, postprocess glow and arbitrary
Lua HUD state remain separate work.

Windows Meson validation on 2026-10-14: `WoundV18Native` passed 60 updates with
exit code 0, codec, native ownership/mass and pixel checks. `WoundV18Pair` passed
two clients through the main-menu connection and inventory mutation in 30.70
seconds. Both required the three-wound initial stage and one-wound changed stage,
matched their native attachment trees with zero mismatches, and reported zero
local simulation ticks. The server continued 64 updates after disconnect. This
does not establish compatibility with every mod's wound scripts or replicated
audio/glow effects; timings are verification durations, not benchmarks.
`WoundV18Headless` separately loaded 10 official modules and completed 60
simulation updates with intentionally invalid SDL video/audio drivers,
`video_initialized=0` and `gpu_enabled=0`. This validates Windows device-free
startup/simulation, not Linux or the graphical wound comparison (which runs in
the ordinary diagnostic client).
The root MSBuild `Final|x64` client was rebuilt and paired with the v18 server.
`WoundV18NativeFinalPair` passed both menu-connected clients and the wound-bearing
inventory fixture in 31.91 seconds, with zero attachment mismatches and 63
server updates after disconnect. Both root binaries now use protocol v18.

## Emitter visuals (protocol v17 foundation)

Protocol v17 adds emission-enabled and flash-visible state for root and nested
`AEmitter` / `AEJetpack` instances, attachment scale, and the native `EmitterFlash`
slot. Both endpoints must be updated together. `.rte` serialization and Lua APIs
are unchanged. Client application sets visual state without running emitter
Update, emission timers, sound playback or client-side particle spawning; particles
continue to come from the authoritative server's object snapshots.

The draw predicate retains the ordinary emitter rules, while replicas use the
server's captured flash visibility. Attachment scale preserves finite signed
values used by content. Invalid emitter classes/flag combinations and non-finite
scale are rejected before application.

Windows Meson validation on 2026-10-13: `EmitterV17FinalPair` passed with two
clients in 29.58 seconds and 64 server updates after disconnect. Both clients
passed codec truncation/invalid-field checks and native attachment checks.
The native emitter test compares CPU-rendered source and replica pixels with
emission enabled, then checks that disabling emission changes the image and
hides the flash. This covers sustained emitter flash state; a burst occurring
entirely between snapshots can still be missed. Postprocess glow
registration, replicated audio and arbitrary Lua HUD state remain incomplete.

The root MSBuild `Final|x64` game was rebuilt and paired with the v17 server.
`EmitterV17NativeFinalPair` passed the menu connection and inventory fixture
with two clients in 32.38 seconds; the server continued 64 updates after
disconnect. These durations include startup and are not performance benchmarks.

## Attachment and pose replication (v16 foundation)

The v16 protocol carries an attachment tree on each rotating world/inventory
object. Records include parent indices, native pointer-slot roles, class/module/
preset identity, parent and joint offsets, rotation relative to the root, sprite
frame, horizontal flip, draw order, arm hand offsets and magazine round counts.
Actor records also carry aim angle and status. Client and server must have the
same wire version; this does not change `.rte` syntax or the Lua API.

The native roles cover human limbs/head/jetpack, crab limbs/turret/jetpack,
rocket legs/thrusters, dropship thrusters/hatches, feet, held devices, firearm
magazines/flashes and mounted turret devices. Other attachables use numbered
generic slots. Reconciliation uses the existing ownership setters, immediately
retires absent/replaced objects under callback suppression and restores the
authoritative draw order. Existing instances with matching slot and identity
are retained. Interpolation reapplies server pose without local physics/Lua.

Trees are limited to 256 nodes and 16 levels per rotating object, with bounded
UTF-8 identity strings and finite numeric fields. Truncated data, invalid parent
indices, repeated slots and invalid role values are rejected. Exceeding a capture
limit rejects the snapshot rather than silently removing server attachments.
The subsequent v17/v18 extensions above add sampled emitter flash activation
and wound ownership. Arbitrary Lua-local state is not represented by this tree.

Windows x64 Meson validation on 2026-10-08:

- Both targets compiled and linked. The standalone codec check passed valid
  round trips, all packet truncations and malformed tree/number/string cases.
- `AttachmentV16Pose_Ёжик` passed 60 simulation updates, native arm pose, weapon/
  magazine instance retention, infinite ammunition, limb removal/restoration,
  callback isolation and the existing terrain/object tests.
- `AttachmentV16PosePair` passed with two real clients in 53.72 seconds. The
  server fixture changes native arm offsets/angles and aim, removes a foreground
  arm, changes the other passenger's pose, and mutates nested inventory/weapons.
  Each client verifies its actual native attachment trees and numeric poses
  against received records, with zero attachment mismatches. The headless server
  continued 65 updates after disconnect. Diagnostic pose comparison runs only
  under `-debug-run`, not during ordinary gameplay.
- `AttachmentV16Control` passed movement, firing and scene transition in 65.25
  seconds. These durations describe verification, not controlled benchmarks.

The tests used the separate Meson build directory. Linux and broad mod visual/
Lua HUD compatibility have not been verified by these runs.

## Portable headless startup check and Linux CI

### Peer identity regression

Run 38464747236 passed Linux software Cyrillic rasterization (54 foreground
pixels, zero wrong-color pixels) and the build-tree server checks. Its extracted
package check failed because one of two probe clients never established a
session. The available logs did not record GUIDs, so the exact cause is unproven.
Inspection found POSIX RakNet generated identities only from `gettimeofday`, and
the client ignored `ID_ALREADY_CONNECTED`, which can hide an identity conflict
until timeout. Generation now uses a process seed with OS entropy and a unique
process-local atomic sequence; the client reports duplicate-identity rejection.
This is a peer identifier, not an authentication token.

`Tools/RakNetGuidTest.cpp` passed 16,000 identities from eight concurrent threads
on Windows. `Tools/check_peer_guid_processes.py` passed 4,000 identities from four
simultaneous processes. The rebuilt Windows RakNet library and Meson frontends
passed headless startup/two-client checks in 19.83 seconds (134 further server
ticks) and graphical menu/message checks in 28.53 seconds (62 ticks).
Linux package verification must be repeated after this change.
Repeat Linux run
[38469296527](https://github.com/SanecMan/Cortex-Command-Reload/actions/runs/38469296527)
passed compilation, threaded/cross-process peer identity tests, sound payload
codec tests, Cyrillic software rasterization and both startup checks. The
extracted package test passed in 9.62 seconds with two clients, HUD text, applied
input, 121 further server ticks and clean SIGTERM shutdown. This successful
repeat does not prove the exact cause of the earlier one-client failure.
The delivered native Windows Final pair subsequently passed
`PeerIdentityDeliveredMenu` in 32.44 seconds: two simultaneous clients and 62
simulation ticks after disconnect, with clean process exits. MSBuild reported
the RakNet source project before linking the game.

The Windows solution now builds its RakNet source project as a dependency.
Meson with prebuilt libraries still requires rebuilding that library when its
source changes; the tracked release library is updated with this identity fix.

`Tools/run_server_smoke.py` uses only Python's standard library and runs on
Windows or Linux. From the content checkout, run:

```sh
python3 Tools/run_server_smoke.py --exe build-server/CortexCommandServer --game-dir . --self-test --probe-clients
```

It removes DISPLAY/WAYLAND_DISPLAY, forces invalid SDL video/audio drivers,
loads a config with a Cyrillic file/log-directory path, checks UTF-8 server-name
CLI override and the ephemeral-port override, rejects an invalid player limit,
loads the Tutorial Activity, observes at least 120 additional simulation ticks
with live actors and no clients, then requests a clean console shutdown. Known
outputs within its own diagnostic directory are replaced on repeat runs; other
workspace files are not removed. Results include `result.json` and stdout/stderr
logs. On POSIX it additionally starts with stdin redirected from /dev/null and
checks a clean SIGTERM shutdown, as needed for service/container execution.
That branch is not exercised by a Windows run. With `--self-test`, it first runs
the dedicated executable's device-free simulation diagnostics, including four
native loopback clients receiving a populated world, player assignment, input
manager application, and server survival after disconnect. This does not test
four graphical clients, cross-platform LAN gameplay or text glyph rendering.
`CortexCommandServer --self-test` also runs those diagnostics directly; use
`-debug-run-output NAME` to select a diagnostic directory. Repeat runs replace
that directory's diagnostic outputs.
`--probe-clients` additionally starts two separate native client processes using
`CortexCommandServer --probe-client ADDRESS PORT`. This diagnostic mode loads no
game content or devices: it decodes snapshots, requires a populated world that
advances at least 60 server ticks and sends neutral input. The runner requires
both connections to be visible simultaneously in server status, both clients to
exit successfully, server-side application of neutral input to a player slot,
and further simulation updates after their disconnection.
It validates transport across processes, not graphical replica application or
the gameplay effects of input. It can also be used against a remote test server;
the probe disconnects after success or after a 30-second deadline.
Windows `SeparateClientProbeWindows` passed on 2026-10-14 in 18.72 seconds:
two simultaneous processes, populated advancing snapshots, server-side neutral
input application and 127 more ticks after disconnect. Tutorial assigned one
playable slot and one spectator. Invalid/missing probe ports returned exit 2.
An unreachable loopback server returned exit 1 with a connection error. The
delivered root server and native Final client then passed
`SeparateClientFinalMenu` in 32.41 seconds with two menu-connected clients and
62 simulation updates after disconnection.

`.github/workflows/linux-server.yml` configures only the dedicated executable on
Ubuntu 24.04 / GCC 13 with the checked-in SDL3/LuaJIT/Allegro sources. It is called
by the normal CI workflow and supports manual/weekly runs and isolated
`codex/linux-server-*` verification branches. It runs the portable
smoke without a display, saves failure diagnostics, and packages the executable,
FMOD runtime, example config and server documentation. The package needs existing
Data/ content and the Linux system runtime dependencies; it is not a standalone
content distribution. Client rendering dependencies remain link dependencies of
the common engine even in a server-only build.
The workflow also extracts the package outside the build tree, checks its shared
library dependencies there and reruns headless startup/config/shutdown checks
against that extracted executable with the checkout supplying game content.

The Python syntax and workflow YAML syntax/structure were checked locally.
`PortableServerWindows` passed repeatedly against the existing Windows server on
2026-10-13; the final run took 7.88 seconds, observed 127 ticks and 17 actors,
and verified `video_initialized=0`, UTF-8 config/CLI and invalid-config rejection.
There is no installed WSL distribution or container runtime on this Windows
machine, and the separate Linux host remains disabled at the user's request.
The first real Linux CI run (38448845526, 2026-10-14) passed configuration but
failed compilation because classic minizip names its ZIP64 entry API differently
from Windows minizip-ng. The small diagnostic ZIP entry now uses their common
non-ZIP64 API. Repeat CI run
[38452209295](https://github.com/SanecMan/Cortex-Command-Reload/actions/runs/38452209295)
passed Ubuntu 24.04 / GCC 13 compilation and linking, dependency checks, and
device-free runtime in 10.13 seconds: 120 further simulation ticks, 17 actors,
UTF-8 config/CLI, invalid configuration rejection, four native in-process clients
receiving populated snapshots and logical sound lifetimes. It also verified
continued operation with closed stdin and clean SIGTERM shutdown.
The following run,
[38453967381](https://github.com/SanecMan/Cortex-Command-Reload/actions/runs/38453967381),
also passed the separate-process probes and extracted-package check. The build
tree test took 13.24 seconds and the extracted-package test 9.62 seconds. Each
connected two clients concurrently, verified server-side neutral input
application and 120 further simulation ticks after disconnect, and passed
UTF-8 config/CLI, closed-stdin survival and clean SIGTERM shutdown. Both probe
processes received 59 populated snapshots advancing 174 server ticks; Tutorial
assigned one playable slot and one spectator. `ldd` resolved FMOD from the
extracted package and reported no missing libraries or direct OpenGL/X11
dependencies. The tests loaded the ten official modules, not the local Mods
collection. These results do not prove cross-OS/LAN gameplay, two active Tutorial
players, actual systemd/Docker deployment, or graphical Linux client rendering.
Windows `DedicatedSelfTestWindows` passed the expanded runner:
126 simulation ticks, 17 actors, no video initialization, four native loopback
clients receiving a live world, and device-free sound playback lifetime checks.
The rebuilt native Windows `Final|x64` client and matching server passed again
on 2026-10-14: `DedicatedSelfTestFinal` took 15.98 seconds, and
`LinuxSelfTestFinalMenu` connected two separate clients through the actual menu
handlers in 32.4 seconds. The server continued 62 updates after disconnection.
These are Windows results, not evidence of Linux execution or cross-OS gameplay.

## Tracy build-option consistency

For a Meson build with `-Dtracy_enable=false`, run:

```sh
python Tools/check_tracy_build.py --compile-commands build-server/compile_commands.json --expect disabled
```

This verifies compiler macro flags, including separate `-D` arguments and later
`-U` overrides, and requires a TracyClient.cpp record. It does not preprocess
source files or measure runtime speed. Linux CI runs it before compilation.
The GCC Allegro build previously forced `TRACY_ENABLE` and exported it through
its dependency even with the parent option disabled. That forced define was
removed; the existing Tracy dependency now supplies the macro when enabled.
No changes to `.rte`, Lua or the network protocol are involved.

On Windows on 2026-10-13, Meson regeneration succeeded and needed no object
rebuild for this GCC-only flag change. The real disabled compilation database
passed (637 records), four flag-parser regression cases passed, and GCC C API
object probes showed Tracy event references only in the enabled variant.
`TracyDisabledServer` passed the portable headless smoke in 7.88 seconds with
126 ticks and 17 actors. These are build-configuration/runtime smoke checks,
not a full GCC/Linux compile or an FPS optimization benchmark.

## Portable build targets

Meson provides `build_client` (default true) and `build_server` (default false).
The common `CortexCommandEngine` static library is compiled once; client and
server entry targets link all its objects so reflection-only entity factories
are retained. It currently contains the legacy rendering/UI code as well as
simulation. Headless operation is selected at startup, not by a second engine
implementation; graphics/audio libraries are still link dependencies.

With the Linux dependencies listed in README installed, the server target can
be configured with:

```sh
meson setup build-server -Dbuild_client=false -Dbuild_server=true -Dinstall_runner=false --buildtype=release
meson compile -C build-server -j 2
./build-server/CortexCommandServer --config Server.example.ini
```

Run from the checkout (or another directory containing Data/); game content is
resolved relative to that working directory. Config paths can also be absolute.

`ClientEntry.cpp` and `ServerEntry.cpp` call the same `RunCortexCommand` startup
and loop. Windows frontends convert UTF-16 arguments to UTF-8; the POSIX server
entry forwards native UTF-8 arguments. The server entry adds `--server` so it
cannot accidentally open the client menu. The argv array has its required null
sentinel. A server-only configuration does not install desktop launchers/icons.

Both Meson targets built on Windows x64. `MesonPortablePair` passed with two
real clients, nested inventory and background-hand changes, zero client
simulation updates and 67 server updates after disconnect. The server reported
`video_initialized=0` and ran with an invalid SDL video driver. The separate
`MesonPortable_Ёжик` run passed 60 diagnostic updates using a Unicode output path.
These are runtime verification results, not performance benchmarks.

The initial Meson link warned about conflicting CRT defaults (LNK4098). The
legacy prebuilt zlib-ng archive used `/MT`, whereas the Meson engine uses `/MD`.
Meson now builds zlib-ng from its checked-in sources with the selected runtime,
the existing compatibility API and runtime SIMD dispatch. Both executables
relinked without LNK4098. `meson test -C build 'zlib round-trip'` verifies 40
combinations of data sizes, compression levels and repetitive/random data,
including checksums and rejection of truncated streams. This change applies
to Meson; the Visual Studio dependency projects are unchanged.
`ZlibSource_Ёжик` passed the 60-update engine diagnostic with the rebuilt library.
`SourceZlibPair` passed nested inventory/background-hand replication with two
clients in 40.31 seconds, then observed 71 server updates after disconnect.
The earlier binary remains the user's MSBuild Final game; these Meson tests run
from their separate build directory.

A compile of the POSIX entry branch with a stub engine proved argument forwarding
and forced server mode; it is not a Linux engine build or runtime test. Linux
headless, signal handling, systemd and Docker operation remain unverified while
the test host is offline.

For an out-of-tree Windows build, run the smoke test from the checkout:

```powershell
Tools/RunDedicatedServerSmoke.ps1 -ServerExecutablePath '.\build-server\CortexCommandServer.exe' -ClientExecutablePath '.\build-client\Cortex Command.exe' -GameDirectory (Get-Location).Path -CheckInventory -OutputPrefix PortableInventory
```

`GameDirectory` must contain the game content and runtime DLLs. Both binaries
must use the same protocol version. Without this option the script uses the
server executable's directory, preserving the normal in-checkout workflow.

### Replica callback isolation verification (2026-10-08)

Matching Final client/server binaries passed `ReplicaCallbacksPair` with two
clients in 66.71 seconds, including nested inventory/background-hand mutation,
zero replay callbacks, successful normal Lua controls and no leftover diagnostic
attachments. The server continued 72 updates after both clients disconnected.
`ReplicaCallbacksControl` passed movement, firing and scene transition in 76.68
seconds. These two runs overlapped; their durations are not benchmark results.
The separate `ReplicaCallbacksPrice` catalog/server pricing regression passed in
53.73 seconds with two clients and 66 subsequent server updates. All processes
returned exit code 0. Native arm and magazine setters remain available to normal
gameplay; the replica reconciler bypasses their deferred deletion and script
side effects during snapshot application.

### v15 Windows verification (2026-10-08)

Both Final x64 binaries were rebuilt from the same core. `BackgroundV15Pair`
passed the expanded inventory/background-hand fixture with two real clients in
40.04 seconds. Both native initial/mutated states and repeated inventory reuse
passed with no missing presets; the headless server continued 64 updates after
the last disconnect and exited normally. `BackgroundV15PriceRegression` passed
catalog/server purchase pricing and two-client lifecycle in 86.46 seconds,
including 70 updates after disconnect. `BackgroundV15Control` passed movement,
firing and scene transition in 95.56 seconds. The last two runs overlapped and
these timings are verification durations, not performance benchmarks.
`BackgroundV15ValidationLocal` passed 60 simulation updates and the explicit
native background-hand/codec checks with exit code 0. Results are in the matching
`ScreenShots/` directories. This does not validate complete mod/Lua UI replication
or Linux operation.

## Inventory and background-hand diagnostic (v15)

Build both executables, then run
`Tools/RunDedicatedServerSmoke.ps1 -CheckInventory -OutputPrefix InventoryV15`.
This fixture creates a stationary, disabled dropship containing two passenger
actors with stored pistols. Both real clients must observe the initial native
inventory tree, with a Combat Shield in one passenger's background hand and a
pistol in the other's. The wrapper replaces the shield with a four-round pistol,
clears the other background hand, changes stored ammunition, adds a second
stored pistol and swaps passenger order. Clients must match native ownership/slot order, health
and magazine state, and observe a subsequent snapshot with no inventory clones.
The wrapper also checks simultaneous connections and continued server updates
after disconnection. The local console commands `debug-inventory-create` and
`debug-inventory-mutate` are for disposable test sessions only.

v14 snapshots carry actor inventory contents, including nested actors and stored
firearm magazines. Runtime reconciliation retains existing item instances by
network ID. World-to-inventory or inventory-to-world transfers currently recreate
the representation in its new domain. v15 adds background-arm equipment and
magazines. Complete attachments, actor pose and Lua-local state still require
additional replication.
The codec has passed preflight checks for nested records, infinite ammunition,
depth bounds, truncation and invalid ownership. Real-process fixture verification
is required separately; the existence of this command is not its test result.

The first real fixture run failed its inventory assertions: the dropship had
returned and refunded its contents before clients finished loading. Both clients
correctly showed empty inventory rather than reporting a false success. The
fixture now waits for both clients' first snapshots before creation, pins its
transport and delays its hatch opening. Diagnostic clients can wait up to 60
seconds for the required inventory observations before failing. These changes
are confined to the explicit test fixture and do not change normal craft AI.

The pinned-fixture run then exposed a real identity-comparison lifetime bug.
GetModuleName returns an owning string, while the replica identity helper kept
a string_view to its destroyed temporary. Native module names in mismatch logs
were corrupted, causing inventory items to be rejected and cloned repeatedly.
The helper now stores the module string for the duration of key construction.
This also corrects the preexisting world-object candidate identity comparison.
Client diagnostics retain bounded mismatch details and live/cache/clone counts
so a codec pass cannot conceal native ownership or state failures.

After the identity fix, `InventoryV14IdentityFixed` passed with two separate
Windows clients in 39.74 seconds. Both observed the original tree and the server
mutation (passenger order, stored ammunition 7 -> 3 and an added pistol with 5
rounds). Native inventory equality passed, and later snapshots applied all five
records with `inventory_spawned=0`, `inventory_live=5`, `inventory_cached=5`.
The server continued 64 updates after both disconnected and quit with code 0.
This run used the existing v14 fixture server and the newly fixed client; a final
server relink and regressions are checked separately. The earlier standard
60-update local diagnostic passed, including replica spawn/removal/interpolation
and 22 foreground equipment states (`InventoryV14Local`). Elapsed time is test
duration, not a performance comparison.

Final matching v14 binaries passed `InventoryV14Final` again in 57.83 seconds,
with both native inventory checks passing and 65 updates after disconnection.
The parallel `InventoryV14PriceRegression` displayed and charged 798, passed
both client checks and continued 67 updates, in 57.33 seconds. Both servers quit
with code 0. The v14 Control regression also passed movement, firing and scene
transition in 71.03 seconds. These concurrent test durations are not benchmarks.
The successful fixture's final screenshot was inspected and showed its transport
without placing stored cargo in the world's object collections.

## Live catalog policy (v13)

The client now receives server-owned stock, allow/always-allow/prohibit lists,
only-owned and transport-limit flags, and landing-area boxes. Changes made by
Activity/Lua to the existing BuyMenu or LZ state propagate during the session.
The menu waits for the first matching policy, refreshes its total after a stock
change and clamps landing selection against the server area. Server validation
remains authoritative at purchase time. Empty stock in only-owned mode hides
unavailable catalog entries while retaining always-allowed exceptions.

Both executables must use the current protocol (v15). The rule frame includes the server's pricing
module name and foreign/native multipliers. The client uses these for displayed
prices and affordability checks, separately from its faction theme. Custom base
preset prices and complete craft inventories are not transmitted yet.

Run `Tools/RunDedicatedServerSmoke.ps1 -CheckPurchasePolicy` for the live rule
fixture. After the client's catalog screenshot, the wrapper sends the explicit
local console command `debug-purchase-policy`. This modifies player 0's rules
in a disposable Tutorial session: restricts allowed items, adds an always-allowed
brain and a prohibited SMG, gives one Rocket MK2 and two soldiers as stock,
disables transport-limit enforcement and moves the LZ to X=100..400. The client
must observe the changes before confirmation and buy the official loadout for
324 instead of 864. The command is diagnostic and permanently changes that
running session; use it only in a disposable test server. Normal Lua API callers
need no new method or wire handling to publish their rule changes.

## Client equipment orders

On a world-state client, press F6 to open the existing equipment catalog. Select
craft/cargo and confirm the order, then move the landing cursor and click to
submit. F6 cancels the catalog or landing selection. The client waits for server
confirmation and displays acceptance/charged cost or a rejection reason. Gameplay
input is suppressed while this interface is open; the server keeps simulating.
Spectators cannot open the order menu. Scene/assignment changes discard its local
presentation state. Delayed requests retry the same ID once per second.

This presentation has its own local controller and uses the ordinary BuyMenuGUI
controls/rendering. It does not run GameActivity gameplay update, clone a craft,
consume stock or subtract money on the client. The server's validated RPC owns
those actions. A FrameMan client-GUI callback draws it into the normal player
framebuffer, and teardown clears that callback before releasing UI resources.

Base preset prices and assets still come from local installed content. Server
stock, allow/prohibit rules, transport constraints and dynamic landing areas now
update the catalog through policies. v13 also carries metagame pricing modifiers;
custom base-price differences are still outside that policy frame; complete remote
BuyMenu parity is not yet established.

The stock-price regression also checks that the client's displayed order total
equals the server's accepted charge. The earlier v12 policy fixture checked
replicated stock and the server deduction, but did not compare the displayed
total: the legacy menu incorrectly reset its duplicate-item counter to one.
The counter now increases for each copy, so copies beyond available stock are
paid normally. This correction also applies to local play and retains the
existing delivery economics and Lua purchase API.

Windows Final stock-total verification passed with matching rebuilt client and
server binaries. `StockCostPolicy` displayed 324 and accepted a 324-unit charge;
`StockCostNormal` displayed 864 and accepted an 864-unit charge. Both wrappers
ran two separate clients, checked the replicated balance and delivered craft,
and exited with code 0. The servers continued 66 and 64 updates respectively
after disconnection. Elapsed times were 62.10 and 64.15 seconds while the tests
ran concurrently, not performance benchmarks. The normal 60-update diagnostic
also passed (`BuyStockLocalRegression`). These runs cover duplicate soldiers
exceeding stock and a stock-owned delivery craft. At that v12 checkpoint,
metagame multipliers were not yet implemented or verified.

The v13 pricing fixture is `Tools/RunDedicatedServerSmoke.ps1
-CheckPurchasePricing`. It implies the live-policy check, sends the local
`debug-purchase-pricing` command after the catalog opens, and loads a disposable
MetaPlayer through the engine's normal data reader. The fixture uses Ronin.rte
as native tech, foreign multiplier 2 and native multiplier 0.5. Two Coalition
soldiers are stock-owned, while the rocket is paid. The expected order total is
798 (150 for the Base rocket and 648 for the remaining foreign Coalition cargo).
The wrapper requires both the pre-submit menu total and the accepted server
charge to match, together with normal delivery/replica/lifecycle checks.
The command refuses to add a fixture if metagame players already exist. Use it
only in a disposable server session; it changes that session's economic context.

Windows Final v13 pricing verification passed with the rebuilt console server
and two separate clients (`MetaPricingV13`). The real MetaPlayer context was
loaded successfully; the active client observed module Ronin.rte, multipliers
2/0.5 and the updated stock before confirmation. Its pre-submit total was 798,
and the server accepted exactly one order for 798. The client also checked the
authoritative balance at acceptance and observed the delivery craft. The
spectator had no buy menu. The server continued 64 updates after both clients
disconnected and quit with code 0. The ordinary v13 stock-policy regression
(`StockPricingV13`) displayed and charged 324 and continued 65 updates.

Elapsed times were 61.81 and 63.39 seconds while both tests ran concurrently;
they are not performance benchmarks. The final pricing screenshot was inspected
and showed the `Delivery ordered: 798.00 oz` confirmation. Its later balance
includes normal craft-return refunds, so the final screenshot is not used as
the deduction assertion. These tests use GUI confirmation APIs; physical mouse
input and a complete metagame campaign remain outside their coverage. Codec
checks rejected negative/non-finite multipliers and retained the prior decoded
policy on failure. The standard 60-update actor/particle diagnostic also passed
on this client (`PlayableFinalCheck`).

The same v13 binaries passed the Control regression (movement, firing and scene
transition) in 72.61 seconds (`PricingV13Control`) and the separate purchase RPC
regression in 62.27 seconds (`PricingV13Rpc`), including changed-payload duplicate
ID replay without a second charge. The RPC server continued 83 updates after
disconnection and exited with code 0. These concurrently executed checks are
functional regressions, not timing comparisons.

`Tools/RunDedicatedServerSmoke.ps1 -CheckBuyMenu` enables the automated menu
fixture. It selects an official Coalition loadout through the GUI's loadout/cart
path, captures catalog and landing screenshots, submits from the landing chooser,
and verifies an accepted order, replicated charge and craft. It uses GUI APIs for
confirmation rather than physical mouse events. The spectator must stay without
a menu. Server funding is set to 10000 for this disposable Tutorial session.

Windows Final menu verification passed in 45.65 seconds with two separate
clients (`RemoteBuyMenuDedicated`). The active client captured the catalog and
landing chooser, submitted one 864-unit order and observed the charge and craft
in server snapshots; the spectator had no menu. Both screenshots were inspected.
The landing cursor uses the engine's palette color, and normal gameplay input
stays blocked until the confirmation's fire button is released. This last input
guard was compiled and exercised without physical held-button injection; actual
mouse/keyboard interaction remains a manual validation gap. The server continued
65 updates after disconnection and shut down with code 0.

After the final server relink, the menu test passed again in 62.46 seconds and
the separate RPC replay test passed in 60.11 seconds, with 65/66 continued updates
after disconnection and clean shutdown. Both ran concurrently. The ordinary
60-update client stress test and v11 Control/transition regression also passed.
These are functionality checks, not a performance comparison. Renderer callback
initialization and cleanup were exercised by those client starts/exits.

## Current implementation

Windows x64 has a separate `CortexCommandServer.exe` console executable with a
persistent authoritative world-state simulation. It loads content and an Activity
without SDL video, a game window, OpenGL initialization, audio initialization or
client rendering. Disconnecting all clients does not stop simulation. It accepts
the existing world-state clients; this is separate from the obsolete framebuffer
multiplayer sources.

The console entry converts Windows UTF-16 arguments to UTF-8 and invokes the same
engine startup code as the client with `--server`. Common headless content loading
and simulation steps serve both diagnostic runs and the persistent server.
Windows links the existing engine objects and libraries with a console entry
object, preserving one simulation implementation and avoiding a second object
cache. Renderer/audio code is still linked and runtime DLLs are still required;
their presence is not evidence of GPU or audio-device initialization. Activity
CPU UI state and CPU bitmap loading remain for compatibility and collision data.

## Build on Windows

Install Visual Studio 2022's Desktop development with C++ tools and satisfy the
existing README build prerequisites. From the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File Tools/BuildDedicatedServer.ps1
```

The script builds `RTEA.sln` as Final x64, compiles `Source/ServerEntry.cpp`, and
links `CortexCommandServer.exe` and its PDB from the engine's exact MSBuild link
inputs. It uses a response file to avoid command-line length limits. It requires
MSBuild's generated link tlog; it fails if the expected record is absent rather
than silently omitting engine objects. `-Configuration 'Debug Minimal'` builds a
debug-minimal server. `-SkipCoreBuild` is only for a freshly built matching engine,
not a substitute for rebuilding changed core source.

Keep the executable beside `Data/`, `Mods/` when enabled, and the same runtime
DLLs used by the client. The server does not launch or require a running client
executable. A standalone MSVC project/Core library split is not implemented yet.

## Start and configure

```powershell
.\CortexCommandServer.exe --config Server.example.ini
.\CortexCommandServer.exe --listen 0.0.0.0 --port 8000 --players 8 --name "My server"
.\CortexCommandServer.exe --help
```

The client executable also supports the same persistent startup via `--server`.
Use the separate console binary for terminal administration.

Configuration uses UTF-8 INI, optionally with a BOM. The supplied example documents
ListenAddress, Port, Players, Name, ActivityClass, Activity, Scene, IncludeMods and
LogDirectory. CLI overrides apply after config values, regardless of option order:

```text
--config FILE
--listen ADDRESS
--port 0..65535
--players 1..64
--name TEXT
--activity-class CLASS
--activity PRESET
--scene PRESET
--mods true|false
--log-directory DIRECTORY
```

Port 0 chooses an available port and reports it in the ready message. Players
limits connections; Activities still impose their own active-player limits.
Tutorial supports one active human and additional connected clients receive an
inactive/spectator assignment. Name is an administrative label, not a server
browser advertisement. Optional mods are disabled by default. Client Settings.ini
is not loaded or overwritten by this startup.

## Console and shutdown

- `status`: simulation tick, connected clients and actor count.
- `help`: available console commands.
- `quit`, `exit`, `stop`: clean shutdown.
- Ctrl+C/Ctrl+Break request shutdown on Windows. SIGINT/SIGTERM support is present
  in the portable console code but has not been tested on Linux.

Console input is polled without blocking the simulation, including redirected
stdin. Closed stdin does not terminate an unattended server. Shutdown stops
networking, waits for worker tasks and destroys the loaded simulation resources.
The Windows console-close event requests shutdown, but Windows can impose its
own deadline; clean shutdown through `quit` is the tested path.

Default logs are in `ServerLogs/`: Server.log, WorldStateServer.log, LogLoading.txt
and LogConsole.txt. Server.log appends timestamped server events and also sends
them to stdout. The networking log records assignments, disconnects and snapshot
costs. Fatal assertions/aborts/exceptions use a noninteractive failure path and
write ServerFailure.log rather than attempting a game save or graphics dump.
Invalid CLI/config returns a nonzero code and prints an error to stderr.

## Connect a client

```powershell
& '.\Cortex Command.exe' -world-state-client 127.0.0.1 8000
```

Replace address/port with the server's endpoint. Both sides must use compatible
content and the current world-state protocol (v10). No server browser or general
reconnect workflow is provided. LAN/WAN behavior was not validated by the local
tests below.

## Verification

```powershell
powershell -ExecutionPolicy Bypass -File Tools/RunDedicatedServerSmoke.ps1
```

The test starts the actual console server with an invalid SDL video driver, then
connects two separate graphical clients. It checks no native server window, two
simultaneous connections, received snapshots without missing presets, and zero
local simulation ticks on both clients. After client disconnection it sends
status commands and verifies at least 60 further updates, then sends quit and
requires exit code 0. It cleans only its owned processes and diagnostic logs.

Windows Final passed: 130.04 seconds, 2 clients, 66 updates after disconnect,
`video_initialized=0`, clean console shutdown at tick 2079. Native help and
invalid-port/missing-config rejection passed. A separate UTF-8/BOM config with
a Cyrillic filename and server name passed with help/status/quit commands and
an unavailable video driver. The stderr log contained a preexisting libpng sRGB
profile warning; no fatal error occurred.

## Recent-shot diagnostic

`Tools/RunDedicatedServerSmoke.ps1 -CheckMenuConnection -CheckRecentShot` tests
two clients and a real authoritative shot that occurs between snapshots. It
requires client captures of both the recent muzzle flash and its expiry, stable
ammunition during the expiry interval, and continued server updates after
disconnect. The fixture uses `debug-shot-create` and `debug-shot-fire` console
commands in a disposable session. Screenshot directories are cleaned on repeat
runs using the same output prefix. Protocol 28 binaries must be paired.

`-CheckUnexpectedDisconnect` runs separately from gameplay/sound fixtures. After
both clients receive a world snapshot, the harness stops the server and requires
both debug clients to log `unexpected_network_disconnect` and exit 1 with
`result=failed`; the overall negative test reports passed. It catches diagnostic
runs that would otherwise remain in the connection/menu screen indefinitely.
Normal interactive clients retain the disconnected menu and reconnect flow.

Audio-enabled diagnostics also require `audio_shutdown_complete` after gameplay
finishes. Cached asynchronous samples are released before FMOD closes. A
standalone SDK regression can be compiled from
`Tools/FmodAsyncSoundShutdownTest.cpp` with the bundled FMOD include path/library;
run it from the game directory with `Data` as its argument. It opens no window
or output device and tests three cycles of all supported audio files in Data.

## Remaining limits

This is a working Windows/Linux headless host foundation. A complete gameplay release
still needs remote purchase/deployment flows, full Activity-specific state and
HUD synchronization, reconnect, broader mod/runtime regression coverage,
long-running soak and LAN tests. The two-client Tutorial test proves simultaneous
connections and replication, not two active Tutorial players. Rendering/audio
dependencies are still linked, although startup avoids those devices. Linux
build/runtime and POSIX signal handling passed the CI checks described above;
actual systemd/Docker deployment and cross-OS LAN gameplay remain unverified.
The supplied Linux test machine is currently offline at the user's request.

A malformed temporary .rte module also exercised the real Reader abort path:
exit code 1, ServerFailure.log with the module/line error, no game dialog.
The temporary module was removed. The shared diagnostic headless two-client
regression passed again in 70.66 seconds after the common-step extraction.

## Team funds (protocol v10)

Snapshots now carry all four team balances as finite IEEE-754 float values after
the Activity-state byte. Clients apply the authoritative balances directly;
fractional and negative values are preserved for existing economy semantics.
NaN and infinities are rejected by both encoder and decoder. Protocol v9 clients
are rejected and must be rebuilt together with the server.

The server console accepts funds TEAM VALUE (TEAM 0..3). This sets an
administrative balance; it does not emulate a purchase or delivery. Use
RunDedicatedServerSmoke.ps1 -CheckTeamFunds to change teams 0 and 1 while two
clients are connected and verify exact replicated balances and presentation.

Windows Final v10 funds verification passed with two separate clients in
40.28 seconds. Console changes to team 0 (12345.25) and team 1 (-17.5)
appeared in both clients' snapshots and matched their Activity balances for
every applied snapshot. The first client's final screenshot showed Funds: 12345,
the existing integer HUD display. The server continued 64 further updates after
disconnection and quit with code 0. Preflight round-trip tests cover all four
balances, fractional/negative values, and rejection of NaN and both infinities
in each team's wire field.

## Delivery prerequisites

The shared GameActivity delivery path rejects invalid player/team indices,
missing BuyMenu state, a missing selected craft, and a player whose established
brain has been lost. Craft clones remain owned by the function until they enter
the delivery queue, so stock-policy rejection does not leak the cloned craft.
The headless simulation diagnostic checks out-of-range indices (including signed
integer extremes) and a valid player whose menu has not been initialized.

Windows Final verification: `-debug-run 60 -debug-run-headless-simulation
-debug-run-output DeliveryGuardsHeadless` exited with code 0, reported
`delivery_prerequisite_guards=passed`, loaded all 10 official modules, and ran
60 simulation updates with 17 actors and a moving particle. Both
`video_initialized=0` and `gpu_enabled=0` were checked using an invalid SDL video
driver. This test checks rejection prerequisites, not successful remote orders.
The rebuilt client also passed the 60-update gameplay stress run in
`ScreenShots/DeliveryGuardsGameplay/`, with actor/particle batches, three
screenshots, UTF-8 glyph rendering and live snapshot transport checks. The final
screenshot was inspected. Timing from this run is not a performance comparison:
the server linker was running concurrently.
After linking finished, `RunDedicatedServerSmoke.ps1 -OutputPrefix
DeliveryGuardsDedicated -CheckTeamFunds` passed with two separate clients in
41.69 seconds. Both clients applied the exact authoritative balances, the
server continued for 66 updates after their disconnection, and console shutdown
returned code 0. The server reported no SDL video initialization.

These checks prepare the existing delivery implementation for server use. They
do not expose a network purchase command or add an affordability rule to trusted
Lua override purchases. A remote purchase must use the validated entry point
below, with request replay protection and a client buy UI at the network boundary.

## Validated player deliveries

`GameActivity::CreateValidatedDelivery` is the simulation-thread entry point for
player orders. The caller supplies server-resolved craft/cargo presets, landing
coordinates and cargo AI mode. It checks the active human slot, running activity,
brain/menu availability, preset identity, buyability and allow/prohibit policy,
stock counts for duplicate items, finite nonnegative prices/masses, transport
mass/passenger limits, funds and the team's landing-zone area. Orders are limited
to 128 cargo entries and 16 pending deliveries per team. Network callers must
derive the player slot from their connection rather than a client-supplied slot.

Validation rejects the entire order before consuming stock or charging money.
Accepted orders use the existing `CreateDelivery` path, with its current cargo
nesting, craft AI, delivery delay, stock consumption and native/foreign economy
modifiers. A pending Lua override order is preserved. Existing Lua delivery calls
continue to use their established rules, including deliberate overspending.

The v11 RPC and client equipment menu now call this entry point. A server must call it serially with simulation updates;
the method does not add locking around the global managers. A fatal allocation
failure during cloning is not a recoverable purchase rollback.

Use `-debug-run 60 -debug-run-delivery-validation -debug-run-output
ValidatedDelivery` to run the explicit headless delivery fixture. It uses a
disposable Tutorial session, tests rejected orders without changing funds, stock,
landing zone or queue, then buys a craft with duplicate cargo (one owned and one
paid item) and verifies that a pending trusted Lua order still creates a delivery.
It does not use the player's active game session or save it.

Windows Final verification used 600 updates with an invalid SDL video driver
and completed with exit code 0 (`ScreenShots/ValidatedDelivery/DebugRun.log`).
All ten rejection scenarios passed with unchanged economy/queue/LZ. The accepted
mixed stock/paid order charged exactly 530, consumed one owned cargo item and
queued the delivery; the preserved Lua override then queued a second delivery.
The subsequent simulation finished with 19 actors, a moving probe particle,
`video_initialized=0`, `gpu_enabled=0` and `result=passed`.
The ordinary 60-update client stress test also passed in
`ScreenShots/ValidatedDeliveryGameplay/`, including screenshots (final frame
inspected), glyph rendering and live snapshot transport. Both Final executables
were rebuilt. The subsequent two-client dedicated-server regression with funds
checks passed in 46.10 seconds (`ValidatedDeliveryDedicated`), continued 65
updates after disconnection and shut down cleanly with code 0. That network
regression checks session behavior, not purchase messages.

## Network purchases (v11)

### v12 live policy verification

`Tools/RunDedicatedServerSmoke.ps1 -OutputPrefix PurchasePolicyDedicated
-CheckPurchasePolicy` passed with two separate Windows Final clients. The fixture
changed the server's stock, allow/deny lists, mass/passenger flags and landing
area after the remote catalog opened. The active client checked the replicated
values and submitted its order through the equipment menu API. The server
accepted exactly one order, charging 324 rather than the initial 864 because
the rocket and two soldiers were in stock. The spectator remained unable to
purchase. Both clients reported `world_state_purchase_policy=passed`; the
server continued 64 updates after disconnection and exited with code 0.

Elapsed time was 42.74 seconds, not a performance benchmark. The landing-screen
capture was inspected. This fixture exercises API confirmation rather than
physical mouse input. `OnlyOwned=true` has codec coverage but was not enabled
in this live scenario. Custom pricing/meta multipliers and complete inventory
replication remain outside this policy message.

The same v12 binaries also passed the Control regression (movement, firing and
scene transition) in 62.48 seconds and the separate purchase RPC regression in
63.14 seconds, including duplicate-ID idempotency and 65 updates after both
clients disconnected. These checks ran concurrently; their elapsed times do
not measure engine performance. A normal 60-update actor/particle diagnostic
also exited with code 0 (`ScreenShots/CrashScreenshotCheck/DebugRun.log`).

The world-state sessions now carry client purchase requests and server results.
The server owns preset lookup, player assignment, scene revision and charging.
It returns the last result on a retry of the same ID, rejects older IDs, checks
spectator status and limits new requests to one per 250 ms. Native craft/cargo
cloning waits for the priority task pool's asynchronous Lua GC to finish.
The client equipment menu now uses this API.
See [WorldStateProtocol.md](WorldStateProtocol.md) for the wire layout and retry
semantics. Rebuild both client and server: v10 peers cannot exchange v11 messages.

`Tools/RunDedicatedServerSmoke.ps1 -CheckPurchases` runs an active player and a
spectator through real separate processes. The active fixture tests stale scene,
missing preset, acceptance, a changed-payload duplicate ID and an old ID. It
requires exactly one new dropship in snapshots and a single 500-unit deduction.
The spectator must receive a rejection. Use a different run from `-CheckTeamFunds`
because that fixture changes balances administratively.

Windows Final v11 purchase verification passed in 41.14 seconds with two separate
clients (`ScreenShots/PurchaseRpcDedicated-*`). The Tutorial fixture is funded to
10000 through the server console before connection. The active player received
`stale_scene` and `invalid_preset`, then an accepted 500-unit order. A retry with
a different payload and the same ID returned the identical accepted result,
without another purchase. The older ID returned `stale_request`. The client
observed exactly one dropship and 9500 funds in authoritative snapshots. The
other client received `spectator`. The server continued 65 updates after both
disconnected and quit with code 0.

Initial fixture runs correctly rejected a landing point outside Tutorial's team
area and a purchase with Tutorial's zero starting balance. The fixture now picks
the map's static LZ and receives explicit console funding; server validation was
retained. These tests do not cover dynamic scripted landing-area replication or
the remote buy UI. Codec and shared delivery regression checks also passed.

After the final server relink, the purchase regression passed again in 60.48
seconds and the separate funds regression passed in 61.30 seconds; both continued
65 updates after disconnection and shut down with code 0. They ran concurrently,
so these durations are test elapsed times, not performance measurements. The
v11 Control scenario (movement, firing and scene transition) passed in 79.23
seconds. The purchase client's final screenshot was inspected and displayed
Funds: 9500. At that RPC checkpoint, complete inventory replication and the
visible remote BuyMenu were still outside the completed step. The client menu
is now described above; complete inventory replication remains outstanding.
## Network sound validation

The development client now plays server-selected SoundContainer resources and
applies their container controls. Use matching client/server protocol versions and the
same installed modules. Sound resources are resolved inside loaded module roots;
missing resources are skipped rather than causing an assertion. Active loops that
began before a client joined are reconstructed starting with v23. v22 transfers
bus pause/pitch and container pan/panning controls, including a group-state baseline
on connection. See `WorldStateProtocol.md` for measured checks and limits.

v24 additionally replicates SFX cycle completion: `FinishIngameLoopingSounds`
removes repeating playback while preserving the current cycle tail. A joining
client restores a paused finishing voice as a finite sample at its saved cursor.
Automated Windows fixtures use:

```powershell
.\Tools\RunDedicatedServerSmoke.ps1 -CheckFinishLoops -EnableAudio
.\Tools\RunDedicatedServerSmoke.ps1 -CheckMenuConnection -CheckFinishingLateJoin -EnableAudio
```

The finishing late-join fixture holds client one for up to 6,000 updates and
verifies it remains connected when client two is ready. It checks actual FMOD
loop counts and expiration without Stop, not recorded waveform quality.
The runner removes stale client readiness reports before launching new processes
with a reused output prefix. Client startup clears that prefix's remaining debug
outputs; other runs' output directories are retained.

v25 adds current fade gain to late-join playback. Fade time follows bus pitch and
bus pause, independently of an individual channel pause. The replica envelope
retains base-volume changes during fading and stops expired voices. Matching v25
client/server binaries are required. The Windows fixture queries actual FMOD
volume after restoration and after halving base volume while paused:

```powershell
.\Tools\RunDedicatedServerSmoke.ps1 -CheckMenuConnection -CheckFadingLateJoin -EnableAudio
```

This verifies state/lifecycle behavior, not sample-accurate audio alignment or
recorded output quality. See the protocol documentation for Windows and Linux
validation results.

The automated two-client runner can exercise server-selected looping sound and
its Stop event:

```powershell
.\Tools\RunDedicatedServerSmoke.ps1 -CheckMenuConnection -CheckSound
.\Tools\RunDedicatedServerSmoke.ps1 -CheckMenuConnection -CheckSound -EnableAudio
```

`-EnableAudio` passes the opt-in `-debug-run-audio` flag to the diagnostic clients
and requires both to report FMOD enabled. Ordinary diagnostics keep audio disabled.
The server console provides `debug-sound` and `debug-sound-stop` for this disposable
fixture. The runner waits for both worlds to load, then verifies Play/Stop on each
client. It does not validate audio quality by listening or recording.
