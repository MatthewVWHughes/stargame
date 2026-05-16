# Build Roadmap

This is the build plan for the Unreal-native game.

The priority is a testable architecture, not architecture theatre. Every milestone must produce something that can be run in PIE, inspected through debug output, and used by the next milestone.

The broader systems this architecture must leave room for are tracked in `future-systems-support.md`. M0 does not implement those systems, but it must not make decisions that block them.

Field-level data contracts live in `system-data-contracts.md`. Local bubble, supercruise, and docking behavior live in `flight-supercruise-docking.md`. Legal, faction, inventory, market, comms, and mission foundations live in `systemic-gameplay-foundations.md`.

For M0 implementation, read the docs in this order: this roadmap, `implementation-plan-m0.md`, `runtime-architecture.md`, `cpp-blueprint-ownership.md`, `frontier-test-fixture.md`, `system-data-contracts.md`, `content-validation-and-tooling.md`, then `save-load-and-versioning.md`.

## Hard Constraints

- The first Unreal architecture validation system is not Sol.
- Sol remains legacy authored content to preserve later, not the initial runtime default.
- No runtime startup path may silently fallback to `sol`.
- Do not port Godot/C# systems directly.
- Do not make Blueprints the source of truth for orbit, route, save, transition, docking, or scale logic.
- Do not rely on raw Unreal physics for solar-system scale simulation.
- Do not start procedural generation before an authored fixture proves the required data fields.

## Target Fixture

Use one ugly but functional non-Sol authored fixture first: `frontier_test_01`.

Full fixture contents by M1-M5:

- 1 primary star
- 2 planets
- 1 nested moon
- 1 station orbiting a planet fast enough to expose moving-frame bugs
- 1 station in a simpler deep-space or high-orbit case
- 1 jump gate with arrival transform data
- 1 asteroid/resource zone
- 1 body with gravity-well slowdown and lockout radius
- 1 deep-space spawn zone
- 1 near-station spawn zone
- 1 intentionally distant leg that requires supercruise

This fixture replaces early procedural generation. Generation comes after the fixture proves what the game actually needs.

M0 uses only the fixture slice defined in `frontier-test-fixture.md`: one body target, one station target, one gate target, and one deep-space spawn zone.

## Prototype Debt To Retire

Current prototype shortcuts that must not become production architecture:

- session default/fallback to `sol`
- startup code naming `Earth`, `Mars`, `Luna`, or `Sol`
- world scans such as `TActorIterator` for atmosphere/body discovery
- constructor hard-path production assets
- HUD hard-coded target labels
- Blueprint-owned canonical route, orbit, docking, save, or transition state
- level-specific scripting for station, gate, gravity, or map identity

## M0: Non-Sol Playable Probe

Goal: get a crude but playable non-Sol boot path immediately.

This is intentionally small. It exists to pressure the architecture with a real player loop before the codebase fills with abstractions.

Required:

- default `StartProfileId` is `frontier_test_start`; resolving it yields `SystemId = frontier_test_01`
- start profile contains `SystemId`, `SpawnZoneId`, optional `DockedStationId`, and optional `DockingPortId`
- player ship spawns from the start profile
- one visible body, one station marker, and one gate marker exist
- normal flight works
- target list can select body/station/gate IDs
- save/reload preserves current system and spawn zone

Acceptance tests:

- `StartNewSession()` resolves to a non-Sol system
- editor play path does not default to `sol`
- player can fly after spawning
- target list is populated from registered gameplay IDs
- save/reload returns to `frontier_test_01`
- test fails if startup requires `Earth`, `Mars`, `Luna`, or `Sol`

## M1: System Data Schemas And Registries

Goal: define the core system data contracts and prove that an active system can be built, queried, torn down, and rebuilt without hard-coded actor names.

M1 is a schema, asset lookup, lifecycle, and registry milestone. It must not require post-M1 behavior such as playable docking, gravity-well slowdown/dropout, supercruise map travel, or jump-gate transition. Those fields may exist in data and may be registered for later consumers, but their gameplay behavior is accepted in later milestones.

M1 defines complete empty/native containers and optional fields for later systems without claiming their final behavior. A field such as `Gate.DestinationSystemId`, `Station.DockingPorts`, `GravityWells`, `RouteSegments`, or systemic gameplay metadata may be present as an unset value, empty array, opaque ID, or stub record according to the active validation profile. The milestone fails only if the declared M1-required fixture data cannot be discovered, validated, registered, torn down, and rebuilt. Later milestones turn those same fields into required, semantically validated data.

### Unreal Asset Contracts

Required native asset types:

- `UStarCatalogAsset : UPrimaryDataAsset`
- `UStartProfileAsset : UPrimaryDataAsset`
- `UStarSystemDefinitionAsset : UPrimaryDataAsset`
- `UBodyVisualProfileAsset`
- `UAtmosphereProfileAsset`
- `UStationProfileAsset`
- `UGateProfileAsset`
- `UShipArchetypeDataAsset`
- `UShipMovementProfileAsset`
- `UShipDurabilityProfileAsset`
- `UShipLoadoutProfileAsset`
- `UShipResourceProfileAsset`

Use `FPrimaryAssetId`, `TSoftObjectPtr`, and `TSoftClassPtr` for referenced content. Godot JSON is import/interchange input, not the runtime source of truth unless runtime modding becomes a deliberate feature.

`UShipArchetypeDataAsset` is required in M1 only at the minimal spawn/identity level. It must define ship archetype ID, ship class tags, pawn/actor class reference, movement profile ID, default cargo capacity, default durability profile, default loadout/resource profile, and docking size class. Equipment progression, ship buying, and detailed outfitting remain later systems.

The profile assets referenced by `UShipArchetypeDataAsset` are also M1 contracts, but only as minimal resolvable data:

- movement profile: profile ID, normal-flight tuning ID, supercruise tuning ID, and supported flight mode tags
- durability profile: profile ID, max hull, max shield, disabled threshold, destroyed threshold
- loadout profile: profile ID, default installed slot IDs, hardpoint/resource compatibility tags
- resource profile: profile ID, fuel/ammunition/resource capacities and default fill policy

M1 validation requires these IDs to resolve and carry sane positive values where a capacity or max value exists. Equipment progression, module shopping, balancing, per-weapon behavior, and advanced damage modeling remain later milestones.

### Required Data Schemas

Define field-level structs for:

- system definition: system ID, display name, seed/source, body list, station list, gate list, zones, map metadata
- body definition: body ID, parent ID, body type, orbit, physical radius, visual radius, collision radius, visual profile, atmosphere profile
- orbit definition: semi-major axis, period, phase offset, eccentricity, inclination, parent-relative frame
- station definition: station ID, parent/anchor ID, orbit or offset rule, service profile, docking ports, map entry
- docking port definition: port ID, approach transform, docked transform, undock transform, allowed ship classes
- gate definition: gate ID, parent/anchor ID, destination system ID, destination gate ID, destination arrival ID, activation range, arrival transform rule
- spawn zone definition: zone ID, parent/reference frame, transform/radius, allowed start contexts
- navigation target definition: target ID, display name, target type, frame, map/debug visibility
- gravity well definition: well ID, anchor body, slowdown radius, lockout/dropout radius, strength/profile
- minimal ship archetype definition: archetype ID, ship class tags, pawn/actor class, movement profile, cargo capacity, durability profile, loadout/resource profile, docking size class
- minimal ship movement, durability, loadout, and resource profile definitions sufficient to resolve the ship archetype without placeholder strings

### Runtime Ownership

Required C++ systems:

- `UStargameSessionSubsystem`
  - selected start profile
  - current system ID
  - pending arrival target
  - persistent ship state
  - authoritative simulation clock access
  - save/load coordination

- `UStarCatalogSubsystem` or asset-backed equivalent
  - Asset Manager lookup
  - start profile lookup
  - canonical start-profile resolution and start-system selection
  - canonical catalog, route graph, discovered-system, and discovered-route mutation
  - catalog validation
  - duplicate ID detection
  - generated-vs-authored override rules
  - missing-reference failure behavior

- `UOrbitRouteFrameQueryService`
  - data-only orbit, route, frame, gate-arrival, docking-port, computed-anchor, and gravity-well queries
  - query-time evaluation without spawning actors or reading world transforms
  - deterministic answers for build, save/load, AI planning, route recovery, and tests

- `UStarSystemSubsystem`
  - active system lifecycle
  - system build phases
  - active-system view of authoritative simulation time
  - wrapper/cache over the headless frame query service
  - active registries
  - `OnSystemBuildComplete`

- `UGalaxySimulationSubsystem`, post-M1 or stubbed for M1
  - authoritative simulation clock owner
  - persistent NPC records
  - scheduled simulation event queue
  - RNG streams and processed-event watermarks
  - Tier 3 economy/traffic/legal state

`UStarSystemSubsystem` may cache orbit/frame evaluations for the active world, but it must not advance the authoritative gameplay clock.

By M2, there must be exactly one C++ owner of `FSimulationClockSnapshot`. If `UGalaxySimulationSubsystem` is not implemented yet, `UStargameSessionSubsystem` temporarily owns the authoritative clock and exposes the same snapshot API. No orbit, route, save, economy, AI, or presentation system may keep an independent gameplay clock.

Catalog, route, discovery, and start selection have one write owner: `UStarCatalogSubsystem` or its asset-backed C++ equivalent. Session, active-system, UI, HUD, map, save, AI, and debug code are read-only consumers of catalog/route/discovery/start-selection state unless they call an explicit catalog subsystem command that performs validation, writes the mutation, and emits change events.

### Validation Scope

M0 validation is a roadmap/index gate, not a full content-tooling gate. For M0, the required validator scope is limited to `frontier_test_start -> frontier_test_01`, no fallback to `sol`, the fixture slice named above, stable target IDs, and M0 save/reload. The broader validation, import, CI, and editor tooling contract remains owned by `content-validation-and-tooling.md` and is not required to be complete before M0 passes.

### Component Bias

Prefer reusable components over deep actor inheritance:

- `UStargameIdentityComponent`
- `UNavigationTargetComponent`
- `UOrbitComponent`
- `UGravityWellComponent`
- `UDockingPortComponent`
- `UTransitEndpointComponent`
- `UReferenceFrameComponent`

Base actors such as `AOrbitalBodyActor`, `AStationActor`, and `ATransitGateActor` are acceptable, but identity, registration, navigation, orbit, gravity, docking, and transition behavior should be componentized where practical.

### Registry Rules

The active system registry must define:

- who registers each object
- when registration happens
- when unregister happens
- duplicate ID behavior
- rebuild invalidation
- weak actor handle policy
- readiness state
- UI/HUD/debug events

Actors register during an explicit post-build phase or controlled `BeginPlay` path. Actors unregister in `EndPlay`. Duplicate IDs are fatal or hard errors in development. Registry queries are not valid until `OnSystemBuildComplete`.

Pooled actors must also explicitly unregister on release to pool. Do not rely on `EndPlay` for pooled traffic actors, because pooled actors may be hidden and reused without ending play. Runtime handles should include a pool generation ID so stale weak handles cannot silently refer to a reused actor.

Required registries:

- bodies by body ID
- stations by station ID
- docking ports by station/port ID
- gates by gate ID
- gravity wells by ID/body
- navigation targets by ID
- map entries by ID
- spawn zones by ID

Acceptance tests:

- build `frontier_test_01` from data
- validate all IDs are unique
- validate all references resolve
- register fixture star, body, station, gate, gravity-well, spawn-zone, map-entry, and navigation-target definitions from data
- spawn only the minimal visible active-system shell needed to inspect registered body, station, gate, and spawn-zone data
- build, tear down, and rebuild without stale registry entries
- debug list/map comes only from registries/view models
- no system logic depends on actor names or Sol-specific paths
- acceptance does not require playable docking, gravity-well slowdown/dropout, jump-gate transition, or full map travel behavior

## M2: Reference Frames, Scale, And Map

Goal: make position ownership explicit before supercruise and docking depend on it.

### Required Value Types

Define explicit C++ value types:

- `FStargameCoordinateFrame`
- `FSectorPosition`
- `FSystemSpaceLocation`
- `FBodyRelativeLocation`
- `FLocalSpaceLocation`
- `FShipSaveLocation`
- `FFrameResolvedTransform`
- `FLocalBubbleState`
- `FSimulationClockSnapshot`

`FShipSaveLocation` must include:

- `SystemId`
- `CoordinateFrame`: `system_barycentric`, `body_relative`, `station_relative`, `gate_relative`, or `local_free_flight`
- `LocationMode`: `free_flight`, `station_docked`, `gate_arrival`, or `respawn`
- `AnchorId`
- position in centimeters
- rotation
- linear velocity in centimeters per second
- velocity frame and frame-inheritance data
- authoritative simulation time seconds

Frame, orbit, route, gate-arrival, docking-port, computed-anchor, and gravity-well query ownership belongs to the headless `UOrbitRouteFrameQueryService` or equivalent C++ service from M2 onward. `UStarSystemSubsystem` may expose convenience wrappers and active-world caches, but it is not the authoritative implementation path.

Frame queries must take an explicit requested simulation time in addition to the authoritative `FSimulationClockSnapshot`. Future-time queries for supercruise, AI, route planning, and docking recovery are pure reads: they must not advance the authoritative clock, consume persistent RNG counters, or mutate subsystem state.

Coordinate frame and location mode are separate concepts. The coordinate frame defines spatial reference. The location mode defines restore behavior.

M2 promotes `FSimulationClockSnapshot` from a future AI helper to a core runtime contract. The same snapshot must drive orbit/frame evaluation, save/load, map rendering, local bubble projection, and debug output.

### Scale Contract

Document and implement provisional scale bands for:

- map scale
- orbital simulation scale
- render proxy scale
- local flight scale
- station local bubble radius
- docking corridor size
- supercruise speed range
- gravity-well slowdown radius
- atmosphere transition radius
- map icon scale
- local bubble radius and origin-shift threshold

The numbers can change, but they must be centralized and visible in debug output. Magic scale numbers scattered across actors fail this milestone.

Numeric provisional values for the fixture live in `frontier-test-fixture.md`. M2 cannot pass with category names only.

The local bubble and origin-shifting contract is defined in `flight-supercruise-docking.md`.

### Map And Debug

Required:

- system map view model from registry data
- orbit path/debug renderer
- navigation target list
- frame/scale debug overlay
- command or debug UI to teleport to each nav target and validate local distance/frame

Acceptance tests:

- one authoritative simulation clock owner exists and exposes `FSimulationClockSnapshot`
- nested moon and orbiting station maintain stable transforms
- same simulation time produces same body transforms after reload
- map displays body/station/gate hierarchy from data
- local transform remains stable near a station and near a body
- save/reload preserves ship logical frame, not raw actor path

## M3: Normal Flight, Targeting, And Local Approach

Goal: prove the player can fly in the fixture system before supercruise or docking complicate the problem.

Required:

- `USpaceFlightMovementComponent` or `UShipFlightModeComponent`
- `EShipFlightMode` state owned in C++
- targeting/selection subsystem or component using navigation target IDs
- HUD view model for selected target, distance, and frame
- local approach to station without docking
- local approach to gate without transition

Normal flight must use the local bubble projection rules in `flight-supercruise-docking.md`, even before supercruise is playable.

Acceptance tests:

- spawn and fly normally in `frontier_test_01`
- select body, station, and gate targets by stable ID
- approach the orbiting station in normal flight
- target remains stable after active system rebuild
- HUD/debug reads from C++ state/view models, not Blueprint-owned canonical state

## M4: Supercruise Travel

Goal: make in-system travel work as a real gameplay mode, not a raw speed multiplier.

Supercruise states:

- `Normal`
- `Spooling`
- `Supercruise`
- `DropoutCooldown`

Required:

- state machine in C++
- curve assets for acceleration/deceleration and turn response
- maximum speed by context
- gravity-well speed ceiling
- lockout/dropout checks
- target approach telemetry
- forced dropout when entering the configured dropout radius
- velocity clamp after dropout
- HUD/debug telemetry for mode, speed limit, nearest gravity well, lockout state, and target distance

Reference-frame decision:

- supercruise must operate on logical system position plus local transform
- local Unreal actor motion is only the current representation
- the local bubble may shift around the player as needed

The implementable speed, dropout, lockout, and target-approach contract is defined in `flight-supercruise-docking.md`.

Acceptance tests:

- engaging inside lockout is refused
- entering lockout during supercruise blocks re-entry if the player drops or cancels
- entering the smaller dropout radius during supercruise forces dropout
- speed ceiling decreases near gravity wells
- player exits within a defined configured radius band from the selected target
- after dropout, velocity is clamped to normal-flight range
- target approach works on the distant travel leg in `frontier_test_01`

## M5: Docking And Save Loop

Goal: prove station interaction against a moving/orbiting station.

Required:

- `FDockingPortDefinition`
- `UDockingPortComponent`
- docking port IDs
- approach transform
- docked transform
- undock transform
- occupancy/reservation state
- relative velocity/alignment checks
- docked state in session/save data
- minimal docked UI state

Immutable docking-port definitions and mutable reservation/occupancy state are split in `flight-supercruise-docking.md` and `system-data-contracts.md`.

Moving-frame docking requirement:

- final docking rail/assist operates in docking-port local frame
- normal ship movement is disabled during the final docking rail/assist
- final approach captures ship transform relative to the docking port and applies `LivePortTransform * LocalInterpolatedTransform` each tick
- station orbital motion is inherited during final approach
- undock gives immediate player control from a stable launch transform, with no autopilot back toward the approach marker
- reload while docked waits for system, orbit, station, and docking port transforms to resolve

Acceptance tests:

- reject docking at wrong speed/alignment
- dock at the orbiting station
- save while docked
- reload after simulation time has advanced
- restore to the correct live station/port
- undock into stable local flight
- repeat dock/undock five times without drift, stuck automation, or bad save state

## M5.5: Gate Transition And Arrival

Goal: make jump-gate transition an explicit runtime boundary instead of an implicit side effect of targeting or map code.

Required:

- transition request/result contract in C++
- source gate ID, destination system ID, destination gate ID, destination arrival ID, and arrival transform rule
- concrete fixture expansion for `frontier_gate_a`:
  - `DestinationSystemId = frontier_arrival_test_01`
  - `DestinationArrivalId = arrival_from_frontier_gate_a`
  - `DestinationGateId = arrival_gate_a`
  - `ArrivalFrame = gate_relative`
  - `ArrivalLocalOffsetCm = (0, 18000, 0)`
  - `ArrivalRotation = (0, 180, 0)`
  - catalog route edge `frontier_test_01/frontier_gate_a -> frontier_arrival_test_01/arrival_gate_a/arrival_from_frontier_gate_a`
- authored destination system `frontier_arrival_test_01` with minimal arrival catalog entry, one registered arrival gate, one arrival spawn/arrival marker, and no Sol fallback content
- validation that the requested source gate is registered in the active system
- validation that the destination system and arrival target resolve through the catalog owner
- session-owned pending arrival target and transition state
- active system teardown/build handoff without fallback to `sol`
- debug output for source, destination, arrival frame, failure reason, and elapsed transition phase

Acceptance tests:

- activating the fixture gate resolves a destination system/arrival ID through catalog data
- `frontier_gate_a` M5.5 validation fails if any destination system, destination arrival, catalog route edge, or arrival transform field is missing
- `frontier_arrival_test_01` builds the minimal arrival slice and registers `arrival_gate_a` and `arrival_from_frontier_gate_a`
- invalid destination system or arrival ID fails loudly and does not change systems
- transition teardown clears active registries before the next system build completes
- arrival spawns the player in the configured arrival frame, not at a hard-coded actor transform
- save/reload during pending arrival resumes or fails deterministically without duplicating the ship
- no gate transition state is owned by Blueprint or level script

## M6: Seeded System Generation

Goal: generate systems only after the authored fixture has proven the fields and gameplay constraints.

M6 generates physical system definitions only: stars, bodies, orbits, stations, gates, gravity wells, resource zones, spawn zones, and map metadata. It does not try to generate mature traffic, patrol, pirate, security, economy, or encounter data. Those contracts are defined by M7-M10 first, then a later generator pass can emit AI-ready route/security content.

Generation must emit a validated system definition first. It must not spawn actors directly as its primary output.

Generated output:

- star type
- body hierarchy
- orbit classes
- body types
- station anchors
- gate anchors
- resource/hazard zones
- spawn zones
- map metadata
- required gameplay test cases

Acceptance tests:

- same seed produces same system definition
- generated IDs are unique
- all references resolve
- generated system passes the M1 registry validation
- generated system includes at least one long supercruise leg, one moving station, one gate, and one gravity-well lockout case

## M7: Space AI Query Foundation

Goal: prepare NPC AI for moving orbital worlds before any serious actor behavior is built.

Space AI direction is defined in `space-ai-and-dynamic-orbits.md`.

Required:

- moving-frame target contract
- route sampling between moving endpoints
- at least one authored fixture route from `frontier-test-fixture.md` that crosses or skirts a gravity lockout case and is usable by the route sampler
- predicted body/station/gate transforms at arbitrary simulation time
- travel-time estimate to moving targets
- risk/security query stubs by route segment and jurisdiction
- debug command or panel for predicted target transforms and route samples

M7 may carry `JurisdictionId`, `SecurityRating`, and `RiskProfileId` only as opaque fixture metadata. Before M9, the route sampler validates that these fields are syntactically present when required by the fixture and stable across save/load, but it must not interpret legal authority, faction ownership, patrol risk, pirate risk, or enforcement consequences. If implementation needs a concrete object before M9, define a minimal `FRouteRiskStub` with only `RouteSegmentId`, `JurisdictionId`, `SecurityRating`, `RiskProfileId`, and `bOpaqueUntilM9`; all semantic validation remains disabled until the M9 profile.

Acceptance tests:

- the fixture route defined in `frontier-test-fixture.md` can be loaded by stable route ID
- orbiting station prediction changes over time and is deterministic at the same simulation time
- route samples update as endpoint anchors move
- route sampling survives save/reload without actor pointers
- all AI target predictions use stable IDs and frames
- debug output shows `Now`, `Now + 60s`, estimated-arrival, and `ETA + 60s` target transforms

## M8: Logical Space Traffic AI

Goal: implement minimal NPC movement intent while ships are still logical data.

M8 proves logical route following and promotion/demotion plumbing. It may define goal enums and serialized state for later patrol, pirate, flee, and fight behavior, but full decision logic for those behaviors waits for the systemic inputs in M9 and the encounter response layer in M10.

Required:

- shared `FShipGoalState`
- logical trade route goal
- reserved goal kinds for patrol, escort, pirate, flee, and fight, without full autonomous decisions
- interrupt priority and saved-goal restoration for trade plus one scripted test interrupt
- `FShipGroupState` and formation slots
- bounded Tier 2 active-system update loop
- debug view for goal, target frame, route progress, group, and decision reason
- one logical-trader realization harness before patrol/pirate/flee complexity

Acceptance tests:

- logical trader progresses along a moving route without spawning
- scripted non-combat interrupt can pause and restore the trader's saved goal
- patrol, pirate, flee, and fight goal records serialize and reject autonomous decision execution until M9/M10 inputs are available
- Tier 2 update count and time budget are visible and capped
- one logical trader can promote to a pooled actor, follow its moving target, demote, and resume without actor references in saved/logical state

## M9: Systemic Gameplay Foundations

Goal: define the non-actor gameplay contracts that logical traffic and encounters need before pirate, police, trade, legal, and comms outcomes are allowed to exist.

This milestone does not build full trading UI, quest content, walkable stations, or a mature economy. It creates the durable data contracts and minimal simulation services that prevent later AI slices from baking fake outcomes into encounter code.

Systemic foundation direction is defined in `systemic-gameplay-foundations.md`.

Required:

- canonical simulation event and event-result envelope
- faction definitions, faction relationships, jurisdiction definitions, and reputation/status records
- legal/offense/evidence/wanted-state records that work for player and NPC offenses
- item, inventory, cargo manifest, and cargo transfer contracts
- commodity, market state, and market transaction request/result contracts
- station market identity rule, defaulting to `MarketId = StationId`
- comms endpoint, message definition, and message log contracts
- minimal mission hook records for later story, side, and faction quests
- debug panels or commands for event log, legal state, faction state, market state, inventory/cargo state, and message log

Acceptance tests:

- same simulation event cannot apply twice after save/load
- an NPC offense can create evidence and legal state without actor pointers
- player and NPC cargo transfers validate inventory location, ownership, capacity, and legality
- market buy/sell request returns a result without a UMG dependency
- station service lookup resolves market, faction, legal, and comms endpoint IDs
- a route-security or encounter decision can reference faction, jurisdiction, legal, market, and cargo data through stable IDs
- patrol, pirate, flee, and fight decision queries can read the required systemic inputs, but do not yet resolve full encounters
- debug output shows why each transaction, offense, or message was accepted or rejected

## M10: Logical Encounters And Response

Goal: prove systemic pirate/police/distress behavior with zero actors.

Required:

- logical contact tracks
- route security snapshots by time window
- patrol asset reservation
- full logical patrol, pirate, flee, and fight decision policies using M9 systemic inputs
- logical encounter events
- interdiction hazard state
- distress event records
- abstract engagement records
- cargo/legal/economy outcome events
- idempotent event processing across save/load

Acceptance tests:

- logical patrol chooses coverage from route/security/jurisdiction inputs
- logical pirate chooses ambush/interdiction candidates from route value, cargo, faction, legal, and patrol-risk inputs
- logical flee chooses a safe moving destination from threat, jurisdiction, and route samples and can restore the saved goal afterward
- pirate attack, distress call, patrol response, and cargo/economy consequence can complete without spawned actors
- the same encounter cannot resolve twice after save/load or player arrival
- actor promotion is validated as metadata only: no actor may be required to resolve the M10 encounter, but any later promotion must attach to existing logical event records instead of duplicating encounters
- patrol response consumes/reserves patrol assets; density does not spawn infinite police
- NPC-NPC offenses use the same jurisdiction/evidence pipeline as player offenses

## M10.5: Combat, Damage, And Threat Contract

Goal: define the smallest combat contract needed before realized AI actors can fight without inventing incompatible health, damage, and threat state.

Required:

- stable combatant ID that can refer to player, logical NPC, or realized actor
- damage event request/result envelope
- hit source, target, damage type, amount, shield/hull routing, and authority timestamp
- minimal ship durability state for shields, hull, disabled, destroyed, surrendered, and escaped
- threat record keyed by attacker/defender IDs, last-known frame, severity, confidence, and expiry
- abstract weapon/attack intent record for logical and realized combat
- debug output for damage application, rejected damage, threat creation, and threat expiry

Acceptance tests:

- damage can apply to a logical NPC without an actor pointer
- damage can apply to a realized actor and write back to logical durability state before demotion
- repeated damage event IDs are idempotent across save/load
- flee and patrol response can read threat records without depending on static world-space positions
- M11 attack steering consumes this contract instead of owning independent health or threat state

## M11: Realized Space AI Slice

Goal: connect logical AI to visible actors for a small playable encounter using the M10 encounter records and M10.5 combat/threat contract.

Required first slice:

- one fast-orbiting station endpoint
- one nested-moon endpoint
- one valuable moving trade route
- one route crossing or skirting a gravity lockout
- three logical traders
- one two-ship patrol
- one two-ship pirate group
- one distress call
- one pending interdiction saved and reloaded mid-event
- low actor cap to prove promotion priority
- Tier 1 steering for approach, formation, flee, and simple attack
- comms hooks for scan, pirate demand, distress, and flee/surrender messages
- damage/threat integration through the M10.5 contract only

Acceptance tests:

- only nearby/budget-eligible ships become actors
- promoted actors keep their logical goal, group, target, and threat
- demoted actors write back enough state to continue logically
- pirates avoid stronger patrols unless configured to be reckless
- police respond to distress without requiring every ship to be realized
- no patrol, ambush, formation, or flee target uses static absolute world space
- save/reload with a pending interdiction does not duplicate or erase the event
- promote/demote during flee preserves target, threat, velocity frame, and recovery policy
- realized damage, surrender, disabled, destroyed, and escaped outcomes write back to logical state through the shared combat contract

## M12: Playable Systemic Progression

Goal: turn the first realized AI slice into a repeatable player progression loop without expanding into full station interiors, full economy balancing, or story production.

Required:

- player reputation/status changes from legal, faction, cargo, comms, and encounter events
- simple station service interactions for refuel/repair/rearm and cargo buy/sell through existing M9 contracts
- basic mission-board records that can offer, accept, complete, and fail route/cargo/security jobs
- ship repair/rearm cost and availability checks through market/service IDs
- persistent player cargo, account/ledger state, legal state, and faction reputation across save/load
- generated or authored follow-up route opportunities that depend on prior outcomes
- debug progression ledger that links accepted jobs, events, rewards, penalties, and reputation changes

Acceptance tests:

- `M12_TradeRouteGateRoundTrip`: player completes the named generated trade/courier scenario across a gate transition and receives ledger-backed credits plus reputation from stable result IDs.
- `M12_ServiceTransaction`: station repair, rearm, and refuel consume ledger-backed credits and update ship durability/resource state through service result records.
- `M12_IllegalCargoRejected`: illegal cargo buy, sell, or delivery is rejected through legal/access validation before stock, cargo, or ledger state mutates.
- `M12_PirateDistressPatrolOutcome`: player triggers the generated pirate, distress, or patrol scenario and receives deterministic legal/faction/reputation outcomes.
- `M12_SaveReloadProgression`: save/reload preserves active jobs, account/ledger state, cargo, ship durability/resources, legal state, reputation, message arbitration, and generated follow-up opportunities.
- `M12_IdempotentCompletion`: repeating the same completion or turn-in event after reload does not duplicate rewards, penalties, cargo removals, reputation deltas, permits, or messages.
- `M12_DebugProgressionTrace`: progression debug output explains each reward, penalty, unlock, rejection, or blocked action by stable ID.

`M12Gameplay` validation is content-only. M12 itself is accepted only when the generated/authored gameplay pass validation succeeds and the named automation or PIE tests above pass against that data.

## Post-M12 Gameplay Expansion

These systems are deliberately after the first realized AI slice and the M12 systemic progression loop, but M9 must leave their contracts viable:

- richer station services and trading UI
- equipment, ship buying, and ship swapping
- mining and resource depletion/regeneration
- generated traffic, market, security, and criminal-pressure content
- faction reputation, faction missions, side quests, and main story hooks
- FPS station interiors, station NPCs, vendors, and quest givers
- communications UI, mission board, and persistent message history
- richer economy balancing and distant-sector audit tools

## Test Harness Requirements

The project needs automation and debug hooks early:

- C++ automation test for fixture determinism
- registry test for stable IDs and valid parents
- orbit test for deterministic transforms after reload
- save test for logical ship location
- docking save/reload test
- debug command to teleport to nav targets
- debug overlay for IDs, frames, scale conversions, representation state, gravity wells, docking volumes, and supercruise state
- manual playtest script: start, target station, supercruise, dropout, dock, save, reload, undock

## Blueprint Compliance Checklist

Blueprint may own visuals, meshes, VFX, tuning defaults, widgets, and presentation.

Blueprint must not write:

- current system ID
- route state
- orbit phase
- save IDs
- docking state
- arrival target
- canonical ship location
- generated system definitions

Any Blueprint that needs loops over all systems, manual string IDs, transition timing, save decisions, or coordinate conversion is a sign the logic belongs in C++.

## Critical Risks

### Risk: Architecture Theatre

If a milestone creates APIs without a playable or testable path, it has failed.

### Risk: Generation Too Early

Procedural generation before fixture gameplay will encode guesses. Prove the fixture first.

### Risk: Supercruise As Speed Multiplier

This will fail. Supercruise needs state, reference-frame handling, speed limits, gravity response, lockout, dropout, and telemetry.

### Risk: Moving-Frame Docking

Docking at an orbiting station is the real test. Static docking proves little.

### Risk: Sol Sneaks Back In

Sol is content, not architecture. Starting with Sol will bias the implementation toward hard-coded Solar System exceptions.

This document is the milestone spine. It should stay test-focused.
