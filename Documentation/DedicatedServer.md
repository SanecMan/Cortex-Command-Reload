# Dedicated server

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
These are runtime verification results, not performance benchmarks. The Meson
link currently warns about conflicting CRT defaults (LNK4098); the successful
smoke runs do not establish safety of every third-party allocation boundary.

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

## Remaining limits

This is a working Windows headless host foundation. A complete gameplay release
still needs remote purchase/deployment flows, full Activity-specific state and
HUD synchronization, reconnect, broader mod/runtime regression coverage,
long-running soak and LAN tests. The two-client Tutorial test proves simultaneous
connections and replication, not two active Tutorial players. Rendering/audio
dependencies are still linked, although startup avoids those devices. Linux
build/runtime, systemd/Docker deployment and signal handling are not verified;
the supplied Linux test machine is currently offline at the user's request.

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
