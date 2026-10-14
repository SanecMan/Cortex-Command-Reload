## Construction catalog codec foundation

WorldStateConstructionCatalog.h reserves optional message kind 14 independently
of existing v29 snapshots, requests and resident state. It carries nonzero scene
revision, player slot 0..3, UTF-8 native-module/module-space names, nonempty type
filter, finite nonnegative foreign cost multiplier, strict show-foreign boolean
and up to 128 visible assembly-group names. Empty native module means all-native
pricing; empty module space means all loaded modules. Strings are at most 1024
bytes with no embedded NUL. The packet limit is 256 KiB. Decode commits both
catalog and sequence only after full validation.

The server publishes this optional message over RELIABLE_ORDERED during native
Activity editing whenever the assigned player's editor catalog changes. Client
sessions retain the newest sequence and clear it on disconnect. The construction
menu applies only metadata matching its current scene revision and player slot.
Module names resolve against local content; a missing required module disables
picker opening. Applying a changed catalog clears the selected preview and
recreates an open picker. Visibility and assembly-group overrides are local to
this picker and do not modify Settings.ini. Native pricing and type/module filters
come from the authoritative editor. Existing servers without this optional packet
retain the default picker behavior. The existing MessageType enum is unchanged;
kind 14 is routed explicitly by the extension.

Windows Meson integration validation: CatalogIntegrationBrain passed two-client
brain placement, purchases/replay, ownership removal, terrain replication,
deployment loadout and inventory gifting in 26.08 seconds, with 130 further server
ticks and video_initialized=0. Both peers logged construction_catalog_received.
CatalogIntegrationVisual passed in 53.25 seconds; both clients logged
construction_catalog_applied, exercised the native picker, deployment rendering
and gifts, menu cancellation/connection and attachment/audio checks. The server
continued for 63 updates after disconnect. The mandatory-receipt headless rerun
CatalogIntegrationRequired also passed in 25.54 seconds with 130 further ticks.
CatalogIntegrationFull passed the full headless UTF-8/config/screen-text/objective
regression in 44.90 seconds with 131 further ticks. Native Final compiled and
linked; the delivered root client/server pair passed CatalogIntegrationDelivered
in 56.41 seconds with 62 server updates after disconnect. Nondefault filters,
live policy changes, missing-module UI and reconnect catalog replacement still
require focused runtime coverage.

The standalone test passed MSVC and MinGW GCC, including Russian names, round
trip, all truncated prefixes, malformed envelope/version/type/reserved/length,
zero sequence/revision, invalid slot and boolean, decoder NaN/invalid UTF-8,
encoder negative/NaN/Infinity/NUL/oversized strings and groups, maximum valid
group/string payload and oversized packets. Failure leaves outputs unchanged.
Linux CI includes the same test. Existing shipped-server content/UTF-8/objective/
spawn regression passed in 19.84 seconds with two device-free peers, 129 further
ticks and video disabled (CatalogFoundationRegression). This proves existing
runtime behavior still passes, not that catalog synchronization is implemented.

# World-state protocol

## Resident-brain construction state

Message kind 13 carries construction state separately from the v29 world
snapshot. The existing snapshot layout is unchanged. A nonzero envelope sequence
and scene revision identify the message, followed by Activity state and four
player records: active/ready booleans, team, and an optional resident Actor state
using the existing object/attachment codec. Brain IDs identify residents within
this stream; they are not the simulation's snapshot network IDs. The packet limit
is 512 KiB. Inactive slots must contain no brain/readiness/team, and non-editing
messages must contain no residents or readiness. Invalid flags, duplicate brain
IDs, wrong teams, malformed object state, UTF-8, bounds and truncation are rejected.

The simulation thread captures native readiness and resident pose/attachments.
Publication follows the snapshot cadence (20 Hz) while peers are connected.
Outside editing it sends an empty resident baseline. Client sessions retain the
latest sequence and clear it on disconnect. A consumer must match scene revision
before using it. The state does not add residents to MovableMan or execute local
physics/Lua. The client construction menu renders residents and sends brain placement/readiness requests; terrain/inventory removal and general placed-object selection remain incomplete.

`WorldStateConstructionStateTest.cpp` passed MSVC and MinGW GCC, covering Russian
identities, atomic decode failure, all truncated prefixes, flags, inactive-slot
constraints, team/ID consistency, invalid coordinates/text, packet bounds and
clearing on transition. `ResidentBrainState` passed two device-free clients in
11.89 seconds: both received their own resident position before readiness, then
required resident/readiness clearing in running state, distinct controlled actors
1/2, revision 1 and 131 further server ticks after disconnect. Full content,
UTF-8/objective/spawn preflight also passed (`ResidentStateRegression`, 19.40
seconds, 127 further ticks). These are test durations, not performance measurements
or visual validation of a construction interface. Linux CI runs the codec and
the strengthened brain-start fixture.

Native Final compiled and linked. The delivered root server passed
`ResidentStateDelivered` in 12.04 seconds with actors 1/2, resident receipt/clear
checks, revision 1 and 128 further ticks. `ResidentStateDeliveredVisual` passed
existing menu/gameplay rendering regressions in 52.06 seconds with 63 further
updates, strict attachments and audio shutdown. The Meson graphical pair also
passed (`ResidentStateVisual`, 73.84 seconds, 66 further updates).
Linux run `38573595112` succeeded for commit `6c62ee15241`.

## Client construction controls

During Editing, clients render native resident-brain sprites from the separate
construction stream without adding them to local physics or running their scripts.
Mouse/arrows move the placement cursor; Click/Enter requests placement of the selected object (brain by default);
F7 opens the native object catalog, B returns to server-selected brain placement, F8/F9 rotate/flip supported selected objects, and F6 toggles ready/cancel. The server validates placement and all-player readiness
before starting the native Activity. Pending requests retry the same ID and time
out after eight seconds. Spectators can see residents but cannot issue commands.
Scene changes and disconnect clear the previews and pending state.

`Tools/RunDedicatedServerSmoke.ps1 -CheckConstruction -ActivityClass GAScripted
-ActivityName 'Skirmish Defense' -SceneName 'Ketanot Hills' -CheckMenuConnection
-RequireAttachmentMatch -EnableAudio` checks two graphical clients, brain
placement/confirmation, resident rendering capture, readiness, distinct controlled
actors and continued server updates after disconnect. The automated fixture calls
the same menu command methods; it does not synthesize physical mouse/key input.
Captures are `debug-run-construction-preview.png` inside each diagnostic output
folder and are removed by that folder's normal repeated-run cleanup.

`ConstructionMenuFinalVisual` passed in 51 seconds with two clients and 62 further
server updates. Both screenshots were inspected: native brain sprites, cursor and
placement/readiness instructions were visible. `ConstructionMenuHeadlessFinal`
passed the content/UTF-8/objective/spawn regression in 20.03 seconds with 129
further ticks and video initialization disabled. These durations are validation
wall times, not performance comparisons. Terrain/inventory removal,
physical-input testing and broader Activity compatibility are still outstanding.

Native Final compiled and linked. The delivered root client/server pair passed
`ConstructionMenuDelivered` in 55.06 seconds with both preview/start markers,
strict attachments and audio shutdown, and 63 further server updates after
disconnect. Linux run `38578726608` succeeded for commit `3bc24daf821`.

### Client object picker and diagnostic sampling

The construction client now owns a native ObjectPickerGUI and a render-only
selected SceneObject clone. The preview is not registered in MovableMan and does
not run object scripts. Scene transition/disconnect destroys its script state
under the same suppression scope as resident previews. Selection sends only the
preset identity and pose; the authoritative server resolves eligibility/cost.
The local catalog uses the normal default native/foreign pricing display;
nondefault server editor pricing/module restrictions are not yet replicated to
this catalog, so server confirmation remains authoritative.

The graphical `-CheckConstruction` fixture opens the native Weapons catalog,
selects AK-47 through the same selection method used by picker input, captures
catalog and object-preview images, submits placement and checks charged funds
(or the insufficient-funds response), then sends readiness after the server
request cooldown. The pair must contain at least one successful paid placement.
It does not synthesize physical F7, mouse or keyboard events. Captures use the
`debug-run-` prefix and are covered by repeated-run cleanup.

Two earlier graphical attempts exposed fixture assumptions and are excluded
from successful validation: immediate readiness after purchase hit the 250 ms
request limit; sampling the first nonempty world captured a firearm without an
Actor, making the camera fixture invalid. Diagnostic sampling now waits for an
Actor and selects an Actor ID explicitly, retaining the full mixed-object
snapshot for spawn/removal/interpolation checks.

`ConstructionPickerConfirmedVisual` passed with two graphical clients in 50.28
seconds, a paid placement and an insufficient-funds response, catalog/object
preview captures, exact funds, native start, camera/view lifetime checks, strict
attachments/menu/audio and 62 further server updates. Catalog and object preview
images were visually inspected. A preceding repaired-camera run had one failed
compressed-version reconnect fixture while its second client passed; this
intermittent loopback failure is recorded, not claimed fixed by the passing rerun.

Native Final rebuilt with the Actor-aware diagnostic fix. `ConstructionPickerDelivered` 
passed against the delivered root binaries in 55.70 seconds with both graphical
clients, a paid placement, all construction/camera/replica/menu/audio assertions
and 63 further server updates. Its catalog screenshot was inspected. The
headless content/UTF-8/objective/spawn regression passed in 22.50 seconds with
128 further ticks and no video initialization (`ConstructionPickerFullFinal`).
Linux CI run `38584475211` succeeded for `6784e34fb00`; previous server
placement run `38581552495` succeeded.

## Construction command codec foundation

`WorldStateConstructionProtocol.h` reserves envelope message kinds 11
(`ConstructionRequest`) and 12 (`ConstructionResult`). The v29 snapshot layout
and existing messages are unchanged. Sessions now route readiness requests and
their responses and brain placement; terrain/inventory removal and general placed-object selection remain incomplete.
This does not yet establish playable networked construction. Placement integration
must validate content, ownership, scene bounds and native brain placement before
modifying the authoritative scene.

Both packets use the existing 16-byte envelope and carry a nonzero request ID
in its sequence field. The maximum total packet size is 4096 bytes. Request
bodies start with a nonzero uint32 scene revision and a uint8 action:

- 0, PlaceBrain: X/Y/rotation float32 and uint8 horizontal flip. The server
  selects the player's allowed brain; there is no client-supplied brain preset.
- 1, PlaceObject: the same pose, followed by nonempty UTF-8 class/module/preset
  identities, each with the existing uint16 byte length and 1024-byte limit.
- 2, RemoveObject: nonzero uint64 server-assigned construction object ID.
- 3, SetReady: a strict uint8 boolean, supporting both ready and cancellation.

Requests carry neither a player slot nor a price. The authenticated peer must
determine the slot, and the server must determine costs. The codec rejects
nonfinite poses, invalid actions/booleans/UTF-8, oversized/truncated packets,
trailing bytes, zero IDs/revisions and fields unused by the selected action.
Coordinates are only checked for finiteness here; scene bounds and placement
permissions belong to the runtime validator. A construction object ID must not
be interpreted as a raw pointer or trusted without an ownership check.

Result bodies carry uint32 scene revision, uint8 acceptance, float32 charged
cost, uint64 construction object ID and a UTF-8 reason string. Accepted results
have an empty reason and finite nonnegative cost. Rejections require a reason,
zero cost and zero object ID. A zero object ID is valid for successful operations
that do not create an object. Both decoders leave the caller's output unchanged
on failure.

`Tools/WorldStateConstructionTest.cpp` passed MSVC and MinGW GCC. It covers all
four request forms, positive/negative responses, 64-bit IDs, Russian identities
and reasons, every truncated prefix, unknown actions, reserved header bits,
invalid flags, NaN on the wire, Infinity at encode, trailing bytes with a corrected
body length, unused fields, string/packet bounds and decode-output atomicity.
The Linux workflow compiles/runs the same codec test.

Windows Meson client/server and native Final builds completed. The headless
content/UTF-8/objective regression passed (`ConstructionCodecHeadless`, 44.63
seconds, 123 further ticks). A graphical Bunker Breach check failed an existing
replica membership gate: 210 live objects versus 211 expected, with one missing
`AHuman/Browncoats.rte/Exterminator` (`ConstructionCodecVisual`). This is not
construction gameplay validation or evidence that the missing-object issue is
resolved. The diagnostic identity details are retained for investigation.

### Readiness routing

Transport sends construction messages through RakNet reliable ordered delivery.
The client exposes `SendConstructionRequest` and `DrainConstructionResults`,
with at most 64 queued replies, cleared on disconnect. The server derives the
player from its reported connection assignment, checks the scene revision,
rejects stale IDs and conflicting reuse of an ID, and limits new requests to
one per 250 milliseconds. It retains the latest request and reply per peer;
an identical repeat returns the original response without applying an action
again. Conflicting/stale IDs do not replace that cached original response.
Connection teardown clears the cache and cooldown.

SetReady, PlaceBrain, PlaceObject and owned movable RemoveObject are currently applied during editing. Outside editing the response is
`not_editing`; spectators receive `spectator`. A stale scene is rejected before
replay handling. `GameActivity::SetNetworkConstructionReady` validates the player
before accessing arrays and uses the native `TestBrainResidence`, including its
fallback to an unassigned brain already in the simulation. A missing/invalid
brain denies readiness. A valid intent sets the native DONEEDITING mode;
`UpdateEditing` performs the normal all-player transition. Cancellation clears
that player's readiness and restores its appropriate native placement mode.
No GUI window is needed on the server.

### Authoritative ordinary object placement

`PlaceObject` now resolves an exact class/module/preset on the server and delegates
to `SceneEditorGUI::PlaceNetworkObject`. Eligibility uses the native object
picker's buyable mode, visible groups, module space and foreign-item policy.
Brains are excluded from this command. The server validates finite scene bounds,
team, total price and available funds. Movable placement requires an air cursor;
actors/devices/particles transfer ownership to MovableMan. Held devices use the
native nearby-human/resident-brain inventory behavior. Deployments use native
loadout creation and cost tally; terrain objects use native PlaceOnTerrain and
CleanAir. Successful placement clears that player's readiness. Request replay
uses the existing cached reply and does not clone or charge twice.

Returned movable IDs currently identify the server's native unique object in the
construction response; they are not snapshot IDs. Terrain placement returns zero.
Terrain/inventory removal and undo/refunds still need
integration. Terrain/deployment/inventory-gift branches need dedicated runtime
fixtures; a firearm fixture does not prove these branches work.

Native Final and both Meson targets built. `ConstructionObjectsDelivered` passed
in 14.05 seconds: one client placed an AK-47 for 40 units, the other exercised
insufficient funds. Each checked replay equality, exact funds, a single matching
object (or none after refusal), unknown preset, prohibited brain and out-of-bounds
placement. Both started with distinct controlled actors and 128 further server
ticks after disconnect. The server initialized no video. The firearm moved from
requested y=30 to y=130.333 before observation: native MovableMan updates during
Editing, so the fixture checks identity/count rather than fixed post-spawn pose.
An earlier strict-position fixture failed and is not counted as passed.
The delivered graphical construction regression also passed in 54.61 seconds
with two clients, strict attachment/menu/audio checks and 63 further updates.
Full content/UTF-8/objective/spawn preflight passed in 25.26 seconds with 128
further ticks (`ConstructionObjectsFullRegression`). These are validation times.

### Owned movable removal

The server records the authenticated peer, scene revision and original team for
successful movable placement IDs. RemoveObject requires that record and current
team, a live native unique ID, and a top-level Actor/device/particle. It uses
SetToDelete so the normal MovableMan update owns destruction. It clears readiness
and returns the removed ID with zero charged cost. An identical repeated request
returns the cached success, while a new request for that deleted ID is refused.
Placement costs are not refunded. Brains, unrelated objects, inventory/attached
items and terrain objects cannot be removed through this path.

Ownership records clear on scene revision change, connection teardown and Stop.
Dead records are pruned before placement. At most 4096 live construction records
are retained; further placement is refused. The client remembers its latest
acknowledged movable ID; Delete requests removal of that object. It does not yet
provide arbitrary cursor-based selection or undo of terrain edits, and reconnect
does not recover old placement permissions.

The device-free brain-start fixture now checks real removal, response replay,
an unknown uint64 ID, repeated removal with a fresh request ID, replicated absence
and unchanged post-purchase funds, before starting both players. It does not yet
exercise an actual second peer requesting the first peer's recorded placement ID,
nor inventory/terrain cleanup. The graphical construction fixture uses the same
client removal method and requires removal success for its paid purchase; it
calls methods directly rather than synthesizing a physical Delete key.

`ConstructionRemoval` passed with two clients in 63.24 seconds, real purchase and
removal, distinct controlled actors and 128 further server ticks, with video off.
`ConstructionRemovalVisual` passed in 57.67 seconds with both graphical clients,
strict attachments/menu/audio and 63 further updates. The full content/UTF-8/
objective/spawn regression passed in 38.66 seconds with 129 further ticks
(`ConstructionRemovalFullRegression`). These are validation wall times.
Native Final compiled and linked. `ConstructionRemovalDelivered` passed against
the delivered root binaries in 55.46 seconds with two clients, removal/absence
checks, native start, strict attachments/menu/audio and 64 further updates.
Linux CI run `38586735265` succeeded for commit `fcce5202a34`.

### Cross-peer removal authorization fixture

The portable runner's `--brain-start` probes now exchange acknowledged placement
IDs through their own `ScreenShots/<prefix>/Проверка_владения` folder. Each writes
an ID to a temporary file and atomically publishes it, requests removal of the
other peer's real ID when nonzero, and must receive `object_not_owned`. A barrier
keeps the owner from deleting early. After both attempts, two fresh snapshots
must arrive; the owner verifies a single surviving firearm and unchanged funds
before performing its legitimate deletion. The runner requires at least one
real foreign-ID refusal, in addition to paid placement and removal/replay checks.
These diagnostic files do not change the production network protocol or expose
IDs to ordinary clients. The test currently uses the fixture's different teams;
a same-team peer case still needs separate coverage.

`--construction-share-dir` is a probe-only option requiring `--brain-start` and
an existing folder. UTF-8 paths use the engine's platform path conversion.
Repeated runs clear only the eight known txt/tmp names in this runner's checked
output directory. They never remove unrelated files. Without this optional
argument, standalone brain-start probes retain their previous behavior.

Both Meson targets linked successfully. `ConstructionOwnership` passed first
against the Meson binary in 15.35 seconds, then against the delivered root server
in 15.27 seconds using the same output prefix. Each run refused the other peer's
real object ID, verified preservation followed by owner removal, started both
players and observed 128 further server ticks with video disabled. The repeated
run left exactly four published files and no temporary files. Misuse of the new
option without brain-start was rejected with exit code 2 before connection.
The native Final client is unchanged from the preceding verified removal build.

### Terrain construction replication

PlaceNetworkObject now checks PlaceOnTerrain's return before charging funds.
The brain-start fixture observes both construction coordinates from the initial
terrain baseline, requiring air before placement. It then places Base.rte
Concrete Block, verifies a charge of 20 (or insufficient funds), replays the
request without a second charge and requires both clients to receive material,
foreground and background values matching the source bitmap's pixel (4,4):
177/89/181. It also requires the refused peer's own location to remain unchanged.
The native terrain change journal publishes the placement through existing
reliable terrain patches; no protocol layout changed.

The graphical construction fixture selects the block through the client menu,
submits placement, checks all three applied local terrain layers and funds,
clears the selection preview and captures `debug-run-construction-terrain.png`
before readiness. This confirms the placed terrain is visible without the
render-only object clone. Physical mouse/key events are not synthesized.
Terrain undo/removal, child-object assemblies and deployment behavior still need
separate coverage.

`ConstructionTerrain` passed the device-free pair in 24.14 seconds with both
terrain-layer markers, replay/cost/ownership/removal checks, distinct controlled
actors and 128 further ticks. Full content/UTF-8/objective/spawn regression passed
in 23.71 seconds with 128 ticks (`ConstructionTerrainFullRegression`). The first
graphical run passed construction/layer/capture/start checks but failed one
compressed-version reconnect smoke, so it is excluded from overall success.
`ConstructionTerrainDiagnosticVisual` then passed completely in 51.45 seconds
with two clients, strict attachments/menu/audio and 63 further updates. The
placed-block screenshot was inspected after clearing the selected preview.

The intermittent reconnect fixture is not declared fixed: additional logs now
record restart success, bound port, connect/receive state and error, and each
version trial's encoding/compression, rejection and snapshot counts. This retains
all existing assertions and supplies evidence on the next recurrence.
A delivered-root attempt later failed initial reconnection (before version
trials); its construction/terrain checks passed but its overall result is
excluded. Receive diagnostics now log connected/snapshot/count/port/error for
every wait. Two trace runs passed with both clients (53.64 and 51.28 seconds),
using the same output prefix. The receive wait predicate was then made consistent
with its return invariant: it waits for both connection and snapshot, retaining
the five-second deadline and all rejection/retry assertions. This prevents a
possible early exit if a snapshot precedes the connection event; the earlier
intermittent failure's cause remains unproven. `ConstructionTerrainReadyVisual` 
passed in 54.51 seconds with 63 further updates after that change.
Native Final compiled and linked. `ConstructionTerrainReadyDelivered` passed
against the delivered root client/server in 55.75 seconds with both clients and
63 further server updates. Both wait states and both foreign-version rejection
checks passed; the delivered terrain screenshot was inspected without a selected
object preview. Linux terrain run `38592076787` succeeded for `ac7487c274c`.
Linux cross-peer authorization run `38588875168` succeeded for `1ec61b9636d`.

### Deployment loadouts and inventory gifting

The optional device-free `--deployment-check` fixture requires `--brain-start`
and `--team-tech Coalition.rte`. It places Base.rte Default Deployment over the
previously constructed block, verifies one Coalition Soldier Light with flipped
pose and equipped Assault Rifle, and requires the native Default loadout charge
of 188 (120 actor + 50 rifle + 18 pistol). Exact request replay must preserve the
response and team funds. The actor must settle before the next operation.

It places an additional Coalition Auto Pistol at that actor's position, checks
a single additional owned inventory/hand item, no top-level dropped pistol,
charge 18, and exact replay. Removing the inventory item through the construction
removal command must return object_attached. This validates the existing safe
refusal; it does not implement inventory deletion. The other peer exercises
insufficient funds. Both subsequently require normal native start and distinct
controlled brains, retaining the deployed actor in the world.

`ConstructionDeployment` and `ConstructionDeploymentLogged` passed two separate
clients in 25.83/25.78 seconds with 128/129 further ticks and video disabled.
The root server passed `ConstructionDeploymentDelivered` in 25.71 seconds with
128 further ticks. `DeploymentCliOverride` passed in 25.60 seconds with 130 ticks:
the CLI Coalition override replaced an intentionally invalid Base.rte config
value on all four teams. The earlier config-only Coalition run also passed.
The Linux service-mode launch retains the same CLI override. A dedicated Linux
CI step runs this fixture. Workflow YAML parsing passed locally.

These checks cover one native Default actor loadout and gifting to an ordinary
AHuman. They do not prove deployments containing only devices, custom loadouts,
resident-brain gifting, graphical rendering of this deployment transaction or
safe deletion from an inventory. Those remain separate validation/implementation
work. Durations are validation wall times, not performance comparisons.

### Graphical deployment and native inventory application

`Tools/RunDedicatedServerSmoke.ps1 -CheckDeployment -ActivityClass GAScripted
-ActivityName 'Skirmish Defense' -SceneName 'Ketanot Hills' -CheckMenuConnection
-RequireAttachmentMatch -EnableAudio` extends construction with the Coalition
Default Deployment. It selects through the native catalog/selection methods,
checks the 188 charge or insufficient funds, requires the native client AHuman
with the correctly identified Assault Rifle, clears the preview and captures
`debug-run-construction-deployment.png` on both clients. The paid client gives
an Auto Pistol and requires the actual native inventory/hand count to increase
by exactly one with an 18 charge. Both retain zero local simulation ticks.
Physical mouse/key events are not synthesized.

`DeploymentVisualCorrect` passed in 51.76 seconds with both clients, native actor/
weapon and inventory assertions, strict attachments/menu/audio and 62 further
server updates. Both screenshots were inspected: the replicated soldier and
rifle are visible standing on the constructed block, with no selected deployment
preview. The initial fixture incorrectly required NPC Controller::IsDisabled;
it failed and is excluded. The corrected fixture checks no local player control
and the real zero-local-simulation counter, retaining actor/equipment assertions.
The replicated NPC may retain AI controller mode while client simulation is
skipped. Linux deployment/config/gifting run `38599673678` succeeded for
`9f6fbb78aeb`.

Native Final compiled and linked. `DeploymentVisualDelivered` passed against
the delivered root binaries in 55.50 seconds with both clients, native inventory
gift application, rendering captures, strict attachments/menu/audio and 62
further server updates. Full headless content/UTF-8/objective/spawn regression
also passed in 50.24 seconds with 127 ticks (`DeploymentVisualFullRegression`).
Linux run `38602303397` is in progress for commit `cccaf4305c8`.

### Brain placement and native start

`SceneEditorGUI::InstallNetworkBrain` clones the server's selected brain, existing
resident brain, or the default Base.rte Brain Case. Brain deployments use their
native deployed-actor conversion. Only an Actor in the Brains group can become
a resident. The server assigns team/player ownership, validates finite pose and
scene bounds before integer coordinate conversion, and applies rotation/flip.
It joins the existing background path workers before replacing the callback
target, refreshes pathfinding and applies the native orbit-path cost limit when
the Activity requires it. No new worker threads or simulation copies are added.

Ownership transfers to ResidentBrains only after validation; rejected placement
retains the previous resident. Accepted placement cancels that player's ready
intent. Brain installation has the same free-brain cost behavior as the native
editor. Ordinary simulation objects and terrain already use their existing
snapshot/terrain streams; resident-brain preview replication is still needed.
The native INSTALLINGBRAIN path also now handles an empty held-object pointer
when a resident exists, rather than dereferencing it.

The portable `--brain-start` fixture waits for two active humans, places a brain
for each through separate clients, repeats the placement request, rejects an
out-of-bounds replacement, then sends readiness. It requires the native running
state, distinct controlled actor IDs and unchanged scene revision without an
admin restart. `BrainConstructionStart` passed on Windows in 11.68 seconds:
actors 1/2, state 4, revision 1, zero restarts, 128 further ticks and two actors
after disconnect, with no initialized video driver. Skirmish Defense's default
requires a clear orbit path. This verifies a positive native path; blocked-orbit
placement is not yet covered by a separate fixture.

`BrainConstructionRegression` passed full content/UTF-8/objective/spawn preflight
in 19.36 seconds with 128 further ticks. The existing Meson Bunker Breach GUI
regression passed (`BrainConstructionVisualRegression`, 67.88 seconds,
62 further updates, menu late admission, strict attachments and audio shutdown).
These are test durations, not performance measurements or proof of a client
construction interface. Linux CI runs the placement/start scenario separately.

Native Final compiled and linked. The root server passed `BrainConstructionDelivered`
in 12.25 seconds with actors 1/2, running state, revision 1, zero restarts and
129 further ticks. The delivered graphical gameplay regression passed
`BrainConstructionDeliveredVisual` in 51.75 seconds with 62 further updates.
Linux run `38567206238` passed the preceding readiness-routing stage; brain
installation/start requires the new CI scenario.

`--construction-check` runs seven checks per separate probe: cancellation,
exact replay, conflicting body under the same ID, unchanged original reply after
that conflict, stale scene, readiness without a brain/outside editing, and stale
request ID. Tests wait for active assignment rather than racing the initial
spectator placeholder. The editing-only fixture permits an empty world while
still requiring advancing ticks, input commands, two distinct active slots and
all seven responses. General gameplay probes still require objects.

Windows Meson client/server compiled and linked. `ConstructionPhaseVerified`
passed two probes against Bunker Breach in 12.80 seconds, with 129 further ticks
and 24 actors after disconnect. `ConstructionNativeEditing` passed the same
seven checks against Skirmish Defense in 12.72 seconds, with native readiness
cancellation accepted, readiness without a brain denied, zero actors and 128
further ticks. The full content/UTF-8/objective/spawn preflight passed
(`ConstructionRoutingRegression`, 39.57 seconds, 136 further ticks).
These routing checks are test elapsed times, not performance measurements. They do not themselves prove
successful brain placement, a ready-all match start, spectator rejection in an
independent probe, or a player-facing construction interface. Linux CI contains
the phase and editing fixtures separately.

Native Final compiled and linked. `ConstructionRoutingVisual` passed the Meson
graphical pair in 64.02 seconds with 65 further updates; the delivered native
pair passed `ConstructionRoutingDelivered` in 53.09 seconds with 62 further
updates. Both covered late admission through the menu, strict sampled attachments
and audio shutdown. The root server also passed `ConstructionRootEditing` in
15.98 seconds, with all seven command checks per player, zero actors and 123
further ticks. These graphical runs are existing gameplay regressions, not a
construction UI test. Linux run `38561184555` passed the preceding codec/spawn
stage; the new routing fixtures require their own run.

## v29 native objective points

Full snapshots carry ordered `GameActivity` objective points, before the player
records: unsigned 16-bit count, followed by UTF-8 description (16-bit byte length),
X/Y float32, signed 16-bit team (-1..3), and unsigned 8-bit arrow direction
(0 down, 1 left, 2 right, 3 up). The bounds are 256 points and 4096 UTF-8 bytes
per description, within the existing 4 MiB packet limit. Nonfinite coordinates,
invalid UTF-8, invalid team/direction, overflows and truncation are rejected.
An empty list clears previous goals on the replica. Servers normalize legacy
Windows-1251 descriptions at capture; local `.rte`/Lua APIs are unchanged.

Capture reads the existing Activity objective list; replica application clears
and recreates it through `AddObjectivePoint`. Native HUD rendering retains its
team filtering, arrows and screen/scene wrapping. This carries the native points
used by Lua Activities, not arbitrary Lua drawing calls. Matched v29 binaries
are required; older versions are explicitly refused by the existing envelope
check. All previous snapshot/device/audio semantics are retained.

`Tools/WorldStateObjectivesTest.cpp` passed MSVC and MinGW GCC, covering roundtrip,
Cyrillic/emoji, clearing, malformed text/count/team/arrow, oversized text/list,
nonfinite coordinates and truncation. Screen-text and device codecs also passed
GCC with v29. Full native `Final|x64` and Meson client/server builds passed.
`ObjectivePersistentHeadless` passed full content/simulation, two separate clients,
objective/UTF-8 screen-text capture and no video initialization in 47.19 seconds,
with 129 observed ticks (concurrent native linking, not a benchmark).

`ObjectiveCreateClearVisual` passed two Meson graphical clients in 52.46 seconds
with 72 post-disconnect updates. Both applied the same UTF-8 descriptions,
positions, team and direction, then confirmed removal of the diagnostic goal.
The active client's intermediate screenshot visibly showed the Cyrillic goal;
the final screenshot showed it removed. `ObjectiveCreateClearDelivered` repeated
this with the root native Final pair in 34.26 seconds and 63 updates, including
strict attachment-state checks and clean audio shutdown. Linux v29 is pending.

The explicit server commands `debug-objectives` and `debug-objectives-clear`
enable/disable a persistent disposable fixture. Its goals are set immediately
before snapshot capture, so the Tutorial's per-tick objective clearing cannot
make the test accidentally miss the text. This behavior is enabled only by the
diagnostic command. Normal goals continue to come from the Activity's own updates.

## v28 recent-shot presentation under validation

Device records carry a wrapping unsigned 32-bit shot sequence and unsigned
16-bit simulation age in milliseconds (65535 means no recent history). Native
HDFirearm increments the counter by actual rounds fired, before OnFire, and
resets a dedicated shot timer. The counter is runtime state, not a new .rte
property or Lua API. Non-firearms and absent devices require empty history.
Matched protocol 28 client/server binaries are required.

The client preserves the received FireFrame getter separately from its render
pulse. A changed sequence with age below 100 ms schedules a bounded muzzle-flash
pulse using a monotonic presentation clock. Duplicate snapshots and interpolation
cannot restart it. Old history does not flash on late join, and counter wrap is
handled. No local firing, rounds, audio, scripts or physical impulse are created.
This makes a recent shot visible even when its native one-frame FireFrame was
cleared before the snapshot. Existing muzzle attachment frames/poses are used.

Portable tests pass the no-shot baseline, elapsed expiry, duplicate packets,
skipped sequences, stale history and counter wrap. Native attachment tests now
check a recent shot with FireFrame false, followed by expired history, while
retaining the weapon/magazine and unchanged ammunition. The inventory network
fixture carries sequence 17/age 20 then sequence 18/age 40 with FireFrame false.
Both Meson executables compiled and linked. MSVC codec/timeline and MinGW GCC
timeline tests passed. `ShotFlashV28Headless` passed in 22.12 seconds with two
clients, 127 ticks, native headless content/simulation checks and UTF-8 input/text.
The corrected `ShotFlashV28Visual` passed in 28.63 seconds with two menu-connected
clients and 62 server updates after disconnect. The server performed a native
shot and cleared FireFrame before capture; the client logged sequence 1/age 16
with native_fire_frame=0, then expired without spending another round. Both PNGs
were inspected: a visible muzzle flash disappeared. The fixed-camera muzzle
region (418..431,290..306) had 15 yellow pixels before expiry and zero after.

Use `Tools/RunDedicatedServerSmoke.ps1 -CheckMenuConnection -CheckRecentShot`.
The console fixture creates a pinned actor above terrain, waits for its client
view, fires normally and limits only this disposable gun to one round per minute
to exclude additional controller shots during expiry. The expiry screenshot
waits two further presentation updates for the scene layer to refresh. Identical
PNGs fail the script. Early attempts exposed a stale-frame capture and extra
authoritative shots; those failed observations are not the visual evidence above.
The same-prefix rerun after the identical-image/expired-age guards passed in
29.48 seconds with 63 further server updates; both clients passed native
attachment checks. `ShotFlashV28Inventory` passed sequence/age transitions in
45.34 seconds with two clients and 64 updates after disconnect. Received stale
ages are preserved verbatim, even with FireFrame set, and do not create a fresh
render pulse. Linux CI `38509673969` for `285eddbcd3e` passed GCC timeline/codec
checks, server compilation, device-free self-tests and two-client build-tree/
packaged checks in 11.58/8.61 seconds. UTF-8 input/config/text, no video device
and SIGTERM shutdown passed. This is not a Linux visual or cross-OS LAN check.

Native Final compiled and linked, but its first delivered visual run exposed
sound receive-queue overflow and an unattended debug client returning to the
menu until the 360-second harness timeout. A minidump showed RunMenuLoop rather
than a deadlocked simulation. Consecutive same-scene sound packets are now
combined without dropping/reordering commands, retaining the 2048-event limit
and 32 distinct pending scene-batch limit. Unexpected disconnect ends a debug
run immediately unless an explicit disconnect/reconnect menu test is active.
The sound queue regression checks 65 small packets and exact command order;
the existing event-budget/scene-order/wrap/lifecycle tests also pass.

After these guards, MSBuild Final|x64 compiled and linked and the matched root
pair passed `ShotFlashV28Delivered` in 32.92 seconds, with the rendered recent
shot and expiry, unchanged ammunition and 62 updates after disconnect.
`ShotFlashV28SoundQueue` passed two Meson clients with real audio/mix controls
in 33.32 seconds and 63 further server updates. `ShotFlashV28Disconnect` passed
the deliberate-server-shutdown test in 18.46 seconds: both root debug clients
logged the disconnect and exited 1; the server exited 0. The queue/debug-guard
follow-up still requires its own Linux CI run.

## Audio sample cache shutdown regression

Enabling audio in a later v28 client check exposed EXCEPTION_ACCESS_VIOLATION
inside FMOD::getGlobals while releasing the playback System. It also reproduced
in a single-player diagnostic, so it was not attributed to world replication.
Small standalone FMOD tests passed 30 cycles with DSP groups, per-voice DSPs and
a second metadata System. Loading all 2248 Data audio files asynchronously in
the standalone test reproduced the crash on System-only teardown. Releasing
each Sound first then releasing System passed three cycles with the same files.

ContentFile::ReleaseLoadedSounds now releases and clears the cached FMOD samples
after channels stop and before the playback System closes. Its implementation
is separate from bitmap cleanup and is compiled by MSBuild and Meson. Shutdown
waits for queued MOID/priority tasks and reopens the debug report for
audio_shutdown_begin/audio_shutdown_complete markers. Audio-enabled smoke tests
now require the completion marker. Experimental callback/polling and metadata
changes that did not remove the crash were reverted.

MSBuild Final|x64 and both Meson binaries compiled/linked. The previously failing
single-player diagnostic exited 0. Three consecutive delivered two-client
recent-shot/audio runs passed in 33.26, 34.15 and 34.40 seconds, with 62/63/63
server updates after disconnect and six clean client audio shutdowns. The
standalone Tools/FmodAsyncSoundShutdownTest.cpp passed three cycles of 2248
samples. The root headless regression passed in 18.92 seconds with two clients,
127 ticks, input/config/text UTF-8, no video device and clean exits. These checks
cover the reproduced shutdown failure, not every possible FMOD or mod race.
Linux CI `38518778663` for `bec44d60d12` subsequently passed the standalone
asynchronous-sound shutdown regression, server compilation and two-client
build-tree/package checks in 11.39/8.41 seconds. Both retained UTF-8 input/text,
initialized no video device and shut down cleanly on SIGTERM. This Linux
headless evidence is separate from the Windows graphical/audio repeats.

This is bounded latest-shot reconstruction rather than a queue of every shot:
bursts can coalesce, removed weapons have no surviving history, and packet
latency is not subtracted from the reported simulation age. Arbitrary weapon Lua
visual state and full Activity-specific presentation remain unverified.

## v27 firearm flash and device recoil presentation under validation

Protocol 27 extends device presentation with the authoritative firearm FireFrame
flag and the device Recoiled flag plus finite two-dimensional recoil offset.
Offsets are sent only when recoil is present; absent recoil requires zero offsets.
FireFrame is valid only on HDFirearm. Absent devices cannot carry any visual flags.
The decoder clears its destination before reading, including omitted fields.
Matching client/server binaries are required.

Capture reads native getters. Replica application changes only visual flags and
sprite recoil; it does not fire a round, add physical recoil impulse, consume a
magazine, invoke Lua or play duplicate audio. Muzzle-flash attachment frame and
pose continue through the existing attachment tree, while HDFirearm::Draw uses
the replicated FireFrame to decide whether that attachment should be visible.
Local weapons and their .rte/Lua API retain their existing simulation behavior.

The portable codec test passes round trips, invalid recoil fields, illegal class
and absent flags, and every truncated prefix of a valid packet. Native tests now
exercise fire/recoil on and off on an equipped gun while retaining the weapon
and magazine pointers. MSVC and MinGW GCC codec tests passed. Both Meson
executables compiled and linked. `DeviceVisualV27Headless` passed in 18.31
seconds with two separate clients, 128 observed ticks, input and UTF-8 text,
without video initialization. `DeviceVisualV27Inventory` passed in 30.52 seconds
with two menu-connected clients, inventory changes and the native fire/recoil
on/off attachment tests. Both clients exited successfully; the server continued
62 updates after disconnect. Native MSBuild Final|x64 compiled and linked;
the root matched client/server pair passed `DeviceVisualV27Delivered` in
32.45 seconds, including both native attachment tests and inventory changes,
with 62 server updates after disconnect. Linux CI `38506544539` for
`94f6fc8b68f` passed server compilation, codecs and headless two-client probes.
Build-tree and packaged runs took 13.24/9.62 seconds, verified input/UTF-8 text,
initialized no video device, and exited cleanly on SIGTERM.

This is snapshot presentation, not a reliable shot-event stream. A shot entirely
between snapshot captures can still be missed visually. Event history/counters
and shot age are needed to close that timing gap; arbitrary weapon Lua state,
screen shake and full Activity-specific presentation are also unproven.

## v26 device activation and reload presentation under validation

Development sources add DeviceVisualState to world/inventory objects and native
attachment nodes. A compact byte carries presence, activation and reloading;
present devices carry finite reload progress in [0,1]. Non-reloading state uses
progress 1, absent state contains no active/reload flags, and reloading requires
HDFirearm identity. Peers must use matching protocol 26.

Capture reads actual HeldDevice/HDFirearm getters. Reconciliation sets activation
and cached reload progress through presentation-only methods, without Activate,
Reload, magazine mutation, sounds or Lua callbacks. Local firearms retain their
original timer-derived progress; Clear resets the replica-only override. Both
hands, mounted devices and stored/loose firearms use their existing attachment
or object records. Existing Lua APIs and .rte properties are unchanged.

MSVC codec checks passed root-device and attachment round trips, progress bounds,
non-finite values, incompatible reload class, absent-state flags and truncation.
Native attachment tests check activation, progress 0.35 -> 0.75 -> completed and
retained weapon/magazine identity. The inventory fixture injects active/reloading
stored firearms and verifies a later progress/activation change. Both Meson
executables compiled and linked. `DeviceVisualV26Headless` passed in 23.25
seconds with two external clients, 125 updates, native attachment/device checks,
UTF-8 configuration and screen messages, without video initialization.
`DeviceVisualV26Inventory` passed in 30.45 seconds with two menu-connected
clients observing progress 0.35/activation true followed by progress 0.75/
activation false on stored firearms. Native getter checks and attachment
comparison included the new state; no inventory/attachment mismatches were
reported. Both exited cleanly and the server continued after disconnect.
These are elapsed test durations, not performance measurements. Native Final,
Linux and a visible player HUD reload-bar check are separate gates.

Native MSBuild `Final|x64` subsequently compiled and linked. The updated root
pair passed `DeviceVisualV26Delivered` in 31.63 seconds with two menu-connected
clients, exact inventory device-state transitions, zero attachment mismatches,
clean exits and 63 additional server updates after disconnect. Linux CI run
`38501991229` for `87cfef33db3` passed the GCC device/attachment codec test,
server-only build and two-client headless checks. Build-tree and extracted
package runs passed in 13.23/9.61 seconds with UTF-8 configuration/input/text,
no video initialization and clean SIGTERM shutdown.

The separate `-CheckReloadHud` fixture spawns a disposable, pinned player-zero
AHuman, equips an Auto Pistol and invokes the normal authoritative Reload path
with a long reload timer. The client captures its rendered HUD near progress
0.35, then captures it again after the server changes timer progress to 0.75.
It does not start Reload on the replica. Screenshot failures terminate the
diagnostic instead of reporting success. `ReloadHudV26Visual` passed initially
in 29.44 seconds; its same-prefix rerun after the screenshot-error guard passed
in 29.03 seconds with 63 further server updates. Both screenshots were visually
inspected: Reloading and the player actor were visible. Reading the first pair
of PNGs found 11 filled bar pixels near (184,301) at progress 0.350056 and 22
near (400,304) at progress 0.750019, matching the HUD's inclusive 28-pixel formula.
The bar changed palette index 77 to 13 due to existing one-handed reload blink.
Camera positions differ between frames; this is a HUD-content check, not an
identical-frame render comparison.

Native `Final|x64` rebuilt successfully with the screenshot-error guard.
`ReloadHudV26Delivered` passed on the root client/server pair in 32.99 seconds,
with progress 0.350296 followed by 0.750019, both captured HUD images, clean
client exits and 63 updates after disconnect. Both delivered screenshots were
inspected and showed the changing reload bar. Linux CI run `38503760116`
validates the diagnostic commit `67fb1533d69` separately; it does not perform
this Windows visual check.
This does not yet replicate fire/recoil timing,
arbitrary weapon Lua state or all Activity-specific HUD elements.

## v25 fade gain and audio clocks under validation

Development sources use session protocol 25 and inner sound schema 6. Play
includes a finite FadeGain in [0,1] along with the remaining fade duration.
Baseline gain remains separate from base channel volume, so later Volume
commands preserve the current envelope rather than restarting it at full gain.

The portable SoundFadeEnvelope follows FMOD parent-clock semantics: individual
pause does not stop a fade; bus pause freezes it; bus pitch changes its clock
rate. The silent FMOD subgroup probe measured 10,240 ticks for ten ordinary
updates and 5,120 with group pitch 0.5. Headless playback and capture now use
monotonic real time rather than scaled simulation ticks or scene-reset timers.
The native owner registry additionally collects expired individually paused
fades because a paused FMOD channel may not process its scheduled stop itself.
New channel registration resets its deadline; cleanup remains on the audio
update thread under the existing owner lock.

Replicas apply the same envelope to base volume each presentation update, rather
than scheduling native fade points whose parent clock would continue while the
replicated bus is represented by individually paused channels. Expired voices
are stopped and removed. Packet transit delay and frame/DSP quantization are
not compensated by this model; sample-accurate waveform alignment is unproven.

MSVC envelope and codec/capture tests passed gain round trips, non-finite/out-of-
range rejection, pause differences, bus pitch, backwards-time rejection and
partial baseline gain. `-CheckFadingLateJoin` checks a partially faded voice,
freezes its bus before connecting client two, changes base volume while paused,
then resumes and requires natural expiration without Stop.

Both Meson executables compiled and linked. `SoundFadeV25Headless` passed in
20.52 seconds with two external clients, 122 updates, UTF-8 input/configuration
and screen messages, without video initialization. Its injected-clock replica
check verifies partial gain, explicit pause, bus freeze, base-volume changes and
expiration. `SoundFadeV25LateJoinFMOD` passed in 118.82 seconds: client two restored
factor/volume 0.715, retained it while paused, changed to volume 0.3575 after
base volume was halved, resumed and naturally expired without Stop. Client one
also froze, applied the volume change and expired. Each received one fixture
Play; both passed the individually paused native fade owner cleanup check and
exited cleanly, with 62 further server updates. The separate ordinary audio
regression passed in 29.79 seconds with two FMOD clients and 62 further updates.
These timings are test elapsed times, not performance benchmarks. Native Final
delivery and Linux verification remain separate stages.

Native `Final|x64` subsequently compiled and linked. The root pair passed
`SoundFadeV25Delivered` in 120.94 seconds with both FMOD clients exiting cleanly
and 63 updates after disconnect. Client two restored gain/volume 0.719376 and
changed to volume 0.359688 when base volume was halved during bus pause. Both
clients naturally expired without Stop and passed paused native fade cleanup.
Linux CI `38499590492` for `e7ff7957299` passed GCC codec/envelope tests and the
server-only build. Build-tree and extracted-package checks passed in 11.74/8.76
seconds with two external clients, input and UTF-8 text/configuration, no video
initialization and clean SIGTERM shutdown. Linux checks exercise the logical
envelope, while Windows checks query actual FMOD channel volume.

## Native fade lifecycle fixes under validation

The silent `Tools/FmodFadeProbe.cpp` checks the installed FMOD build using
NOSOUND_NRT. With base channel volume 0.5, a fade point of 0.5 produced
audibility 0.25; changing that point to 1 restored audibility 0.5. Therefore
fade points are multipliers, and the former engine code applied channel volume
twice. Native FadeOut now starts its envelope at 1, keeps the channel's base
volume and schedules channel termination at the end of the envelope. Zero or
negative durations stop immediately. Sample-count arithmetic uses 64-bit values
and checks deadline addition, rather than multiplying two signed ints.

The portable `SoundFadeMathTest` passed maximum-int duration and overflow checks.
The FMOD probe also showed that group pause freezes parent DSP time, while
individual channel pause does not. Reaching a zero fade point alone left an
infinite channel playing; a scheduled end stopped it after resume. A paused
channel can remain allocated until resume, even with a scheduled end.

The engine audio diagnostic now checks the actual normalized fade points, base
volume 0.5, scheduled end, natural expiration of a 100 ms infinite voice and
immediate zero-duration termination. Both Meson targets compiled and linked;
`SoundFadeLifecycleFMOD` passed in 29.68 seconds with two audio-enabled clients,
group pause/resume and owner/fade checks, clean exits and 62 further server
updates. The rerun reused the same output prefix. Native Final delivery and
Linux checks for this update are separate stages.

Native MSBuild `Final|x64` also compiled and linked. The root client and matching
server passed `SoundFadeLifecycleDelivered` in 32.65 seconds with two menu-connected
FMOD-enabled clients, normalized fade points, natural termination and zero-duration
checks, group pause/resume and clean exits. The server continued 62 updates after
disconnect. Linux CI run `38496468668` for `b76a2d44db3` passed the portable
fade arithmetic test, server-only build and two-client startup/input/text checks.
Build-tree and extracted-package checks passed in 11.37/8.40 seconds with no
video initialization, UTF-8 configuration and clean SIGTERM shutdown. This
run precedes the v25 clock/envelope changes.

At this earlier checkpoint, late-join fade gain was not reconstructed and the
headless timeline froze fades for both explicit and bus pause. v25 addresses
these model differences as described above. Recorded waveform alignment remains
outside the checks established by either checkpoint.

## v24 sound cycle completion under validation

Development sources add FinishBusLoops to session protocol 24 and inner sound
schema 5. It uses container 0, player -1 and one validated bus index (0..2).
`AudioMan::FinishIngameLoopingSounds` captures the SFX command while retaining
the existing local FMOD/headless behavior. Replica playback sets loop count to
zero without stopping the current sample cycle. The ordinary Lua API is unchanged.

The active-voice journal updates each affected timeline with FinishLoops and
retains the voice as a finite, non-repeating baseline at its current cursor.
A paused finishing voice remains available to late joiners until its remaining
cycle is consumed after resume; expired voices disappear from the baseline.
Other buses remain unaffected.

The standalone MSVC codec/capture/queue test passed the new action round trip,
bus/identity rejection, paused finishing baseline, UI bus isolation and current
cycle tail lifetime. `-CheckFinishLoops` checks connected clients through native
FMOD loop-count queries and waits for voice expiration without Stop.
`-CheckFinishingLateJoin` first connects one client, pauses and finishes the
loop, connects the second client, then resumes both. It additionally requires
the second client to restore completion state from the baseline.

Both Meson executables compiled and linked. `SoundFinishV24Headless` passed in
21.31 seconds with two external clients, 125 updates, replica finish/bus-isolation
checks, UTF-8 input/text/configuration and no video initialization.
`SoundFinishV24ConnectedFMOD` passed in 59.01 seconds with both clients checking
native loop count and natural expiration, no Stop, and 63 further server updates.
`SoundFinishV24LateJoinFMOD` passed in 117.97 seconds: client one received the
command while paused, client two restored the finishing state from its baseline
at cursor 45 ms, both resumed and naturally expired, and the server continued
62 updates after disconnect. Both received a single fixture Play.

An initial staggered trial ended client one before resume. Its timeout allowance
is now 6,000 updates, with an explicit check that it remains alive when client
two is ready. Repeating the same output prefix exposed stale readiness reports
before client startup cleared them. The runner now removes only DebugRun.log
and WorldStateClient.log in its two validated client output directories before
starting processes; it rejects paths outside ScreenShots and reparse directories.
The successful late-join rerun reused the same prefix, exercising that repair.
These are elapsed test durations, not performance benchmarks; waveform quality
and sample-accurate synchronization are separate checks. Native Final and Linux
verification for v24 remain separate stages.

Native MSBuild `Final|x64` subsequently compiled and linked successfully. The
root pair passed `SoundFinishV24Delivered` in 33.24 seconds: both audio-enabled
clients connected through the menu, queried native loop count zero, naturally
expired without Stop and exited cleanly. The server continued 63 updates after
disconnect. Linux CI run `38494425830` for `5d2cfaf03f8` passed the v24 server
build, codec/capture checks and extracted-package verification. Build-tree and
package checks passed in 13.24/9.72 seconds with two external clients, input and
UTF-8 screen messages, no video initialization and clean SIGTERM shutdown.
This run precedes the native fade lifecycle changes.

## v23 active sound baselines under validation

Development sources add a sample cursor and optional remaining fade duration
to Play (inner sound schema 4). Both peers must use protocol 23. The capture
journal retains up to 1,024 infinite voices with resource, position, volume,
pan, pitch and explicit pause controls. Simulation time advances their sample
cursor; bus pause and bus pitch also affect this timeline. Stop, source release
and completed fades remove voices from the baseline. Capacity exhaustion stops
the network session explicitly rather than forgetting persistent sounds.

After draining ordinary sound events, the server sends mix and active voices to
each newly connected peer through reliable ordered targeted packets. Scene
changes request a new baseline for every peer because client scene cleanup
removes voices. Existing voice IDs suppress duplicate Play events. Playback
starts paused, seeks to the supplied cursor and then applies explicit/group
pause. Packet transit time is not compensated, and audio waveform continuity
and global FinishIngameLoopingSounds replication still require further work.

The standalone MSVC codec/capture/queue regression passed cursor/fade round trips,
pause and pitch cursor advancement, position changes, remaining fade duration,
fade completion and source-release cleanup. Both Meson executables compiled and
linked. `SoundLoopsV23Headless` passed in 22.91 seconds with 122 updates, two
external clients, input and UTF-8 screen messages, without video initialization.
`SoundLoopsV23LateJoinFMOD` passed in 31.40 seconds: the server started and paused
a loop before either client connected; both restored it with cursor 12 ms and
actual FMOD pause state, verified owner lifetime, received Stop and exited
cleanly. The server continued 62 updates after disconnect. These are elapsed
test durations, not performance benchmark results. Recorded audio comparison
remains a separate check.
Use `RunDedicatedServerSmoke.ps1 -CheckLateJoinSound -EnableAudio` for the
new paused-loop-before-connection fixture; run it separately from `-CheckSoundMix`.

The v23 ordinary audio regression (`SoundLoopsV23MixRegression`) also passed
in 36.28 seconds with two FMOD-enabled clients and 63 updates after disconnect.
Native MSBuild `Final|x64` compiled and linked successfully; the root client and
matching server passed `SoundLoopsV23Delivered` in 32.29 seconds. Both clients
restored the pre-existing paused loop at cursor 29 ms, passed owner-lifetime
checks, received Stop and exited cleanly; the server continued 62 updates.
Linux CI run `38491505316` for `7eee7ebae2b` passed the server-only build and GCC
sound codec/capture tests, including the active-loop baseline regression.
Build-tree and extracted-package runs passed in 13.29/9.67 seconds with two
external clients, input and UTF-8 screen messages, no video initialization,
UTF-8 configuration and clean SIGTERM shutdown. This verifies headless logical
sound state; the FMOD late-join playback test ran on Windows separately.

`-CheckStaggeredSound` checks a different ordering: client one connects first,
then receives Play and bus pause; client two connects afterwards and receives
the active-loop baseline. It requires exactly one fixture Play event per client,
no late-join marker on the existing client, a nonzero cursor with native pause
state on the new client and Stop on both. `SoundStaggeredV23FMOD` passed in
42.57 seconds, with 62 further server updates after disconnect. Both clients
received voice ID 2 exactly once: cursor 0 for client one, cursor 29 ms for client
two. This demonstrates targeted baseline delivery without an extra Play sent
to the existing peer; it does not compare recorded waveforms or FMOD channel
positions over the network.

Native `Final|x64` was rebuilt with the per-fixture Play receipt markers and the
matching root client/server pair passed `SoundStaggeredV23Delivered` in 49.04
seconds. Again, each client received exactly one voice-2 Play (cursor 0 versus
29 ms), both received Stop and exited cleanly, and the server continued 62
updates after disconnect. The second client's stress screenshot was inspected:
the terrain and HUD rendered, with the expected spectator presentation for this
Activity. This image check does not demonstrate control of two independent actors.

## Playback cursor groundwork

`SoundContainer::SeekNetworkPlayback` can move already-playing voices to a
sample cursor. The metadata-only timeline wraps a cursor inside each sample;
this is not an absolute elapsed-time or finite-loop-count seek. No cursor has
been added to the wire format yet, and late-join loop reconstruction is still
incomplete.

On Windows, `SoundPlaybackSeekTest` passed wrapping, pause/resume, pitch,
FinishLoops, finite lifetime, invalid cursor and empty timeline checks.
`SoundPlaybackSeekHeadless` passed in 18.73 seconds with two external clients,
127 simulation updates, UTF-8 configuration and screen messages, without video
initialization. Both Meson executables compiled and linked.
`SoundPlaybackSeekFMOD` passed in 29.88 seconds with two menu-connected,
audio-enabled clients and 62 server updates after disconnect. The native audio
fixture seeks both finite and infinite paused voices to half their sample
duration and queries FMOD's actual cursor before destroying the owner. It also
checks the existing group pause/resume and ownership lifecycle regressions.
These checks do not establish audible continuity for joining clients.

Native MSBuild `Final|x64` also compiled and linked. The root client and matching
server passed `SoundPlaybackSeekDelivered` in 33.71 seconds: two audio-enabled
clients connected through the menu, channel seeking/lifetime and group
pause/resume checks passed, both exited cleanly and the server continued 62
updates after disconnect.
Linux CI `38489551838` for `0845a239b02` passed the cursor regression, server-only
build and two-client checks. Build-tree and extracted-package checks passed in
12.34/9.11 seconds with no video initialization, UTF-8 configuration/input/text
checks and clean SIGTERM shutdown. This run precedes the v23 active-loop changes.

## v22 sound mix controls

Development sources add GlobalPitch (a three-bit bus mask), BusPause, Pan and
Panning to inner sound schema 3 and session protocol 22. Global commands use a
zero container ID and player -1; invalid masks/identities are rejected. Capture
deduplicates unchanged group state and provides a six-event baseline for new
connections and scene revisions. Baselines do not reconstruct already-playing
loops; that remains separate work.

Client playback tracks per-bus pitch and pause plus each voice's original rate
and explicit pause. Group resume must preserve explicit container pause. It
applies mix changes to replica voices rather than calling AudioMan global setters
on the client. Headless AudioMan now applies the same time-scale pitch adjustment
as the graphical host. Existing Lua SetPitch bounds remain unchanged.

The MSVC protocol/queue/capture test passed all new action round trips, invalid
mask/identity rejection, deduplication and baseline state checks. Both Meson
executables compiled and linked. `SoundMixV22Headless` passed in 18.16 seconds,
including independent UI pitch, SFX pitch, group pause and explicit-pause retention.
`SoundMixV22VerifiedFMOD` passed with two audio-enabled clients in 29.29 seconds
and 62 server ticks after disconnect. Pause/resume markers require queries of the
actual FMOD channels, rather than just packet receipt. `-CheckSoundMix` requires
resume after pause. This does not prove audible mix quality or all new command
combinations. Native `Final|x64` compiled and linked successfully. The root pair
was updated to v22 and `SoundMixV22Delivered` passed in 33.22 seconds with two
audio-enabled menu-connected clients, FMOD channel pause/resume queries,
container lifetime checks, clean client shutdown and 62 further server ticks
after disconnect.

Linux v22 CI run `38486687753` for `547c0a81979` also passed. The build-tree
check completed in 13.29 seconds and the extracted package in 9.62 seconds.
Both connected two separate clients, applied client input, received screen text,
initialized no SDL video, accepted UTF-8 configuration, rejected invalid
configuration and shut down through SIGTERM. This checks headless simulation
and logical audio state; it does not measure audible Linux output.

Linux lifecycle-update run `38483629070` (commit `f99ffdecb43`) passed headless
startup and two external clients in 12.34 seconds, with the extracted package
passing in 9.01 seconds. Both runs initialized no SDL video and handled SIGTERM.

## v21 sound capture and playback

The current development sources capture actual SoundContainer playback decisions
through `WorldStateSoundCapture`, including the selected resource and random pitch
factor. Both the metadata-only headless path and the graphical FMOD path capture
Play. Container setters capture position, volume, pitch, pause and fade; Stop and
container reset capture lifecycle termination. Capture uses local pointer lookup
keys but emits monotonically assigned 64-bit container/voice IDs. It is disabled
outside a running world-state host. The journal is mutex-protected and bounded to
8,192 pending events; overflow or failed batch send explicitly stops the network
session. Batches contain at most 100 events to fit worst-case resource names.

The server update drains the journal into reliable scene-scoped packets. The
client gameplay loop drains packets into `WorldStateSoundPlayback`, resolving
resources only inside loaded module roots (including a canonical-path containment
check). Missing/invalid files do not abort the game. It maintains at most 2,048
local voices, applies targeted player routing and container controls, and clears
voices on scene transition, disconnect or reconnect. Global StopAll and graphical
restart/stop calls also emit lifecycle termination. The network-only pitch setter
retains server-selected variation outside the normal setter range; Lua's existing
SetPitch bounds remain unchanged.

v21 adds two explicit float offset coordinates to Play (inner schema version 2).
Position controls remain base-container coordinates, allowing the client to retain
each selected sound's offset. Mixed v20/v21 peers must be rejected. The standalone
MSVC protocol, queue and capture tests have passed, including source address reuse,
voice identity uniqueness, selected Unicode resources and bounded capture overflow.

Windows MSVC protocol/queue/capture tests passed, including 1,600 concurrent Play
events from eight threads with unique voice identities. Both Meson executables
compiled and linked. `SoundCaptureV21Headless` passed in 18.66 seconds: two external
clients, 143 captured sound events, local replica playback/stop/scene cleanup and
no SDL video initialization. `SoundCaptureV21Menu` passed with two clients in
28.29 seconds, confirming network-selected playback and stop without an audio
device. `SoundCaptureV21FMOD` passed with two graphical clients in 30.23 seconds;
both reported `audio_enabled=1`, successful Play and successful Stop. Each menu
run observed 62 further server ticks after client disconnect. These verify FMOD
channel startup/lifecycle, not recorded waveform quality or human listening.

Remaining audio coverage includes late-join loop reconstruction, global bus pause
and pitch changes, dynamic pan changes and actual mix/output comparisons. Linux validation of v21 passed as recorded below. The first native Final delivery test failed during shutdown. The corrected root
pair and subsequent repeat results are recorded below.

The lifecycle update has now been rebuilt in the root Final client and server.
`SoundOwnerLifetimeDelivered` and `SoundOwnerLifetimeDeliveredRepeat` both passed
with two audio-enabled clients, explicit container lifetime checks, network
Play/Stop and clean client exit (31.94/31.64 seconds, 63 further server ticks).
Meson `SoundShutdownTraceFMOD` and `SoundShutdownTraceRepeat` also passed twice
in sequence (29.48/29.44 seconds); the headless regression passed in 18.09 seconds.
Earlier lifecycle/explicit-close trials still reproduced the shutdown failure, so
these successful repeats should not be treated as proof of every possible FMOD
shutdown race. The lifetime checks prove finite tails survive owner destruction
and infinite voices do not remain orphaned. Linux verification of this latest
lifecycle update is separate from the green v21 transport run below.

Linux CI run `38477672589` for `83ac67d5d50` passed the v21 build, codec/capture
tests, headless playback/replica diagnostics and both external clients. Build-tree
startup passed in 13.24 seconds, extracted-package startup in 9.62 seconds, with
no SDL video initialization and successful SIGTERM shutdown.

The native Final delivery test subsequently failed during FMOD shutdown despite
successful Play/Stop markers. The dump places the main thread in
`AudioMan::Destroy -> FMOD::System::release`, with an FMOD worker access violation.
The original callback directly accessed container pointers, while container reset
could discard ownership without detaching channels. A lifecycle fix is under
validation: callbacks queue channel IDs, the audio update processes completions,
container destruction detaches finite sounds and stops orphaned infinite loops,
and FMOD shuts down before owners are destroyed. Detached finite sounds retain
volume and panning metadata. The capture journal also preserves finite sound tails
when their source disappears. Native delivery must be repeated successfully before
claiming this shutdown failure is resolved; the dump does not prove every internal
FMOD failure mechanism.

## Sound payload preparation (v20 historical checks)

`WorldStateSoundProtocol.h` defines a separate bounded inner payload schema for
Play, Stop, Position, Volume, Pitch, Pause and Fade. It uses explicit integer
widths and little-endian encoding, logical container/voice identities and UTF-8
module-relative resource names. It rejects invalid UTF-8, traversal/absolute
path syntax, non-finite numbers, invalid booleans, excessive counts and trailing
bytes. Priority 256 is retained in a 16-bit field rather than truncated to a byte.

The standalone test covers every action, Unicode resource names, a fixed golden
layout, truncated/corrupt packets and the 256-event batch bound. This codec is
now connected to reliable ordered session transport in v20. Sound envelopes
carry a scene revision and nonzero sequence. The client ignores repeated/older
sequences, holds future-scene batches, discards stale-scene batches when drained,
and bounds pending data to 32 batches/2,048 events. Disconnect clears the queue.
Queue overflow ends the connection with an explicit error rather than silently
discarding lifecycle commands (for example Stop on a looping voice). Transport
failures and foreign protocol versions clear pending sounds while retaining the
last world snapshot. The portable queue regression covers late duplicate packets,
stale/future scenes, reconnect reset, both capacity limits and sequence/scene wrap.
Capture of ordinary game sounds and client playback are still separate work.
This change requires matched v20 client/server binaries and does not yet provide
audible multiplayer sound. `WorldStateServerSession::SendSoundEvents` is the
explicit server emission API; the regular simulation does not call it yet.

The native four-client transport diagnostic now sends a sound batch twice with
the same sequence and verifies delivery to every client, future-scene retention
and one-shot draining. Windows `SoundTransportV20Headless` passed in 18.17
seconds with four internal sound recipients and two separate world-state clients.
Graphical v20 menu/message checks passed in 31.07 seconds with two clients and
63 additional server ticks after disconnect. These validate packet delivery,
not audio mixing or late-join reconstruction of active looping sounds.

The lifecycle queue update was compiled in both Meson executables. The standalone
MSVC test passed the protocol and queue cases described above. Windows
`SoundQueueLifecycleHeadless` passed with two external clients in 18.42 seconds
and no SDL video initialization. `SoundQueueLifecycleMenu` passed with two
graphical clients in 28.45 seconds and 62 additional server ticks after disconnect.
The native MSBuild `Final|x64` client and the updated server beside it passed
`SoundQueueLifecycleDelivered`: two graphical clients, 32.19 seconds, 62 further
server ticks after disconnect.

Linux CI run `38473453031` verified the preceding v20 transport commit
`25092231d91`: four internal sound recipients, two external clients, no SDL video
initialization, UTF-8 configuration, invalid configuration rejection and SIGTERM
shutdown. The build-tree run passed in 12.04 seconds and the extracted server
package in 8.82 seconds. This is evidence for sound packet delivery, not playback.

## Screen messages (v19)

Each player view now carries its authoritative screen message as UTF-8, a
32-bit nonnegative blink interval and a one-byte centered flag. Text is bounded
to 16 KiB; the decoder rejects malformed UTF-8, oversized lengths, invalid
boolean flags and blink intervals outside the engine's signed integer range.
The server normalizes legacy Windows-1251 text and truncates oversized messages
at a UTF-8 boundary. Existing resource/preset strings retain their 1024-byte cap.

Capture uses the Activity's player-to-screen mapping. An assigned active client
applies that player's message to local screen zero, bypassing its local minimum
display-duration lock. Empty messages clear old text; spectators and inactive
slots clear their local screen message. Blink animation uses the client's local
UI timer. This covers FrameMan messages produced through C++ or Lua
`SetScreenText`; arbitrary Lua drawing primitives/HUD state remain separate.

This changes the wire format: rebuild client and server together. Version 18
peers are rejected by the existing connection-version check. The standalone
`Tools/WorldStateScreenTextTest.cpp` exercises round trips, maximum text size,
Unicode and malformed packets without engine globals or graphics.

## Background hand (v15)

Each AHuman object record now includes a background-hand presence byte after
its foreground equipment block. A present device carries bounded UTF-8
class/module/preset strings, a magazine-presence byte and, when present,
magazine class/module/preset and signed round count (minimum -1).
Absent devices must have no residual data; non-human records cannot carry this
state. Both world actors and actors nested in inventories use the same encoding.
This is a wire-format change: rebuild client and server together.

Clients reconcile both hands directly through the existing Arm ownership API,
without inventory rotation or automatic shield selection. Matching devices are
retained; replacements are cloned from installed presets.
Removal deletes the replica device instead of moving it into inventory. The
explicit native debug check covers repeated ammunition updates without device
replacement, infinite ammunition, an absent magazine, shield replacement and
device removal. The two-client inventory fixture covers shield-to-pistol
replacement and removal of a second passenger's background pistol.
Preset resolution must match class, module and preset exactly: the engine's
official-module fallback cannot silently substitute another module's device.

## Object script isolation during reconciliation

Native attachment changes can call OnAttach/OnDetach and lazily initialize Lua
Create even though the client simulation loop is disabled. Snapshot application
therefore uses a per-thread RAII suppression scope for MovableObject script
initialization, generic callbacks, individual script functions and UpdateScripts.
Script registry/data cleanup still runs when replicas are deleted. The scope
restores its previous state on return, including nested use; ordinary local and
authoritative-server simulation retains normal callbacks. This scope concerns
object reconciliation, not every possible Activity/UI/global Lua call.

Both hand and magazine replacement detach and delete retired replica-owned
attachments immediately. Their removal flags are cleared before detaching so
they cannot enter the normal deferred-delete queue or spawn native gibs. Client
simulation is disabled and cannot be relied on to drain that queue. Inventory
ownership continues to be handled by the snapshot inventory records.

## Inventory ownership (v14)

After the four player-view records, a snapshot contains a u16 inventory-record
count. Each record carries owner network ID (u64), contiguous zero-based slot
(u16) and the same object-state encoding as world objects. Records are ordered
so an inventory actor appears before items owned by that actor. Owner IDs must
refer to actors in the world or earlier inventory records. IDs are unique across
both collections. Self/cyclic/forward ownership, slot gaps and non-actor owners
are rejected transactionally. Limits are 8192 inventory objects per snapshot,
512 per actor, depth 16, and the existing 4 MiB total packet bound. Capture refuses
an incomplete ownership tree instead of silently truncating its contents.

HDFirearm object records additionally carry their own magazine-presence byte,
magazine class/module/preset strings and signed round count (including -1 for
infinite ammunition). This covers stored and loose firearms as well as the
existing foreground equipped-weapon state on AHuman records.

The client keeps inventory objects owned by their native Actor, without adding
them to MovableMan's world collections or running their gameplay simulation.
At reconciliation, items are detached under temporary ownership and existing
instances are reused by network ID before restoring server order and nesting.
Preset identity, health, team and magazine state are applied. Borrowed cached
pointers are validated against the live ownership tree before use. Default
clone inventories and items absent from the authoritative snapshot are removed.
Foreground equipment remains represented by the existing actor fields;
background hands are added by v15 above. Arbitrary attachments and Lua-local
state are not covered by these records.

## Catalog rules (v13)

Message type 9 is a reliable ordered purchase-policy frame. It contains scene
revision, active player slot/team, only-owned/mass/passenger flags, allow/always
allow/prohibit key lists, signed stock counts and landing-area boxes. It is
bounded to 1 MiB, 4096 entries in each collection and 1024 finite-float boxes.
Keys use bounded UTF-8 strings. Duplicate keys, invalid role/flags, non-finite
coordinates, truncation and trailing bytes are rejected without replacing the
previous decoded policy. Negative stock counts retain existing engine semantics.

The host compares rules every 15 simulation updates and sends only changes to
that player's connection. Policies have a separate sequence counter. The client
applies them only for its matching scene, slot and team, rebuilds catalog rows,
refreshes the order total and constrains the landing cursor to the replicated
area. The catalog waits for its first matching policy. Purchase validation still
runs against current server state; policy delivery does not authorize an order.
Base preset prices still require matching installed content; metagame-specific
pricing modifiers are carried by v13. Immediately after the flag byte, the body
contains a bounded nonempty UTF-8 pricing-module name and two finite nonnegative
f32 values: foreign and native multipliers. The module name is resolved against
the client's installed content; numeric module IDs are never shared between
processes. An unavailable pricing module prevents that policy from enabling the
menu. Ordinary non-metagame delivery uses Base.rte and multipliers 1/1. During
a metagame the server captures the assigned player's MetaPlayer economics,
matching GameActivity's delivery calculation. Faction theme/filter choices stay
separate from the authoritative pricing context. Changed contexts refresh shop,
cart, craft and total prices. Peers must use the same protocol version.

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
scene revision. The order RPC carries preset identities; spawned craft and cargo
inventories are synchronized by the v14+ snapshot ownership records. Purchase
receipts are not retained across reconnects.

The experimental world-state path serializes data explicitly instead of sending C++ structs or framebuffer pixels. Protocol version 15 carries a server tick, scene revision, signed Activity state, four finite team-funds values, UTF-8 Activity class/preset/module and Scene module/preset identity, then top-level actors, items and particles and four player-view records. Each object has a stable host-assigned network ID, class/module/preset identity, transform, velocity, health, sprite frame, horizontal flip, team and category. `AHuman` actor records also carry foreground equipped-device identity and, for an `HDFirearm`, inserted magazine identity and round count. Clients switch or clone those devices from locally installed `.rte` presets and apply the authoritative magazine count. Dynamic `MOPixel` objects without a preset carry material, color, mass, lifetime and sharpness so clients can recreate common transient particles. Version 15 retains the bounded terrain-patch frame with scene revision, pixel rectangle, material/foreground/background layer and indexed pixel bytes.

The decoded packet starts with a 16-byte header: `CCR1` magic, version, message type, reserved byte, sequence and payload length, all little-endian. The decoder rejects unknown versions/types, malformed UTF-8, non-finite numeric data, duplicate or zero IDs, invalid team/category/equipment fields, invalid terrain layer/dimensions, truncated/oversized packets and counts beyond fixed bounds. Version 15 peers are required; earlier peers cannot exchange v15 frames. Each fixed player-view record contains the controlled actor's network ID (zero for no actor), a finite camera position, team and active-human flag. Nonzero actor references must resolve to an actor in the same snapshot, and inactive players cannot reference a controlled actor. Assigned clients follow their server-selected view, using the replica actor's interpolated position to smooth the camera between snapshots. The local Activity player view now binds to the selected replica actor for existing HUD identification; visual and remote-control gameplay validation remain outstanding.

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
