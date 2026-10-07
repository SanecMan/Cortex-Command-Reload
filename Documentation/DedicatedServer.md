# Dedicated server

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
content and the current world-state protocol (v9). No server browser or general
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

## Remaining limits

This is a working Windows headless host foundation. A complete gameplay release
still needs remote purchase/deployment flows, full Activity-specific state and
HUD/funds synchronization, reconnect, broader mod/runtime regression coverage,
long-running soak and LAN tests. The two-client Tutorial test proves simultaneous
connections and replication, not two active Tutorial players. Rendering/audio
dependencies are still linked, although startup avoids those devices. Linux
build/runtime, systemd/Docker deployment and signal handling are not verified;
the supplied Linux test machine is currently offline at the user's request.

A malformed temporary .rte module also exercised the real Reader abort path:
exit code 1, ServerFailure.log with the module/line error, no game dialog.
The temporary module was removed. The shared diagnostic headless two-client
regression passed again in 70.66 seconds after the common-step extraction.
