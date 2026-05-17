# System Data Contracts

These contracts define the initial Unreal-native data shape for the active system architecture.

These names are intended to map directly to C++ `USTRUCT`s and `UPrimaryDataAsset` classes. The active foundation should describe the game data that exists now; historical profile names remain only where commandlets, tests, or compatibility notes need them.

## Current Foundation Scope

The current foundation requires:

- `FStartProfileDefinition`
- authored and generated `FStarSystemDefinition`
- `FBodyDefinition`
- `FStationDefinition`
- `FGateDefinition`
- `FSpawnZoneDefinition`
- `FNavigationTargetDefinition`
- `FStarCatalogEntry` and `FRouteGraphEdge`
- `FSimulationClockSnapshot`, `FFrameResolvedTransform`, and route/frame query contracts
- docking-port, gravity-well, resource-zone, map-entry, traffic-route, logical-traffic, encounter, combat/threat, systemic-gameplay, and progression records used by the active fixture
- save/session fields for free flight, docking, gate arrival, systemic state, and progression state

The narrow startup validation profile can still validate the smallest playable slice, but it is not the full definition of the foundation.

The in-code fixture provider and authored assets must expose coherent `FStarSystemDefinition` data. Actors must not hard-code IDs or transforms locally, and services must fail with typed results or validation reports when required data is missing or invalid.

Native fixture data is an explicit test/authoring source. The runtime catalog does not silently substitute native fixtures when Asset Manager content is absent, and session services do not generate systemic gameplay state to cover missing authored or saved data.

## Naming Glossary

- `Definition`: immutable gameplay data used to build runtime state, usually a C++ `USTRUCT`.
- `Profile`: reusable tuning or presentation data referenced by one or more definitions.
- `Asset`: Unreal package-backed authored content, usually `UPrimaryDataAsset`.
- `Instance`: save/runtime state for a specific owned or spawned thing.
- `Runtime actor`: the world representation built from definitions and instances.

Example: a station definition says `brink_watch` exists, a station profile describes its services/visual class, a station instance stores mutable market/security state, and an actor is the current loaded representation.

## Asset Types

Required authored assets:

- `UStarCatalogAsset : UPrimaryDataAsset`
- `UStartProfileAsset : UPrimaryDataAsset`
- `UStarSystemDefinitionAsset : UPrimaryDataAsset`
- `UBodyVisualProfileAsset : UPrimaryDataAsset`
- `UAtmosphereProfileAsset : UPrimaryDataAsset`
- `UStationProfileAsset : UPrimaryDataAsset`
- `UGateProfileAsset : UPrimaryDataAsset`
- `UShipArchetypeDataAsset`
- `UShipMovementProfileAsset : UPrimaryDataAsset`
- `UShipDurabilityProfileAsset : UPrimaryDataAsset`
- `UShipLoadoutProfileAsset : UPrimaryDataAsset`
- `UShipResourceProfileAsset : UPrimaryDataAsset`

Future authored assets:

- `UTrafficAIPolicyAsset`
- `UPatrolZoneAsset`
- `UPirateAmbushProfileAsset`

Traffic, patrol, and pirate assets become required when the active game content moves those policies out of native fixture data and into authored assets.

Use `FPrimaryAssetId`, `TSoftObjectPtr`, and `TSoftClassPtr` for referenced content.

## Asset Manager Contract

Each primary asset class must define a stable primary asset type and be included in project Asset Manager scan settings.

Runtime system builds resolve authored content through `FPrimaryAssetId` or soft object/class references. Hard object references are allowed only for always-loaded engine/default assets or test-only placeholders explicitly marked as such.

System build may synchronously load fixture data while the foundation is still small. Missing assets, duplicate IDs, and unresolved references fail validation before actors are spawned.

## ID Rules

- IDs are stable `FName`s or validated string IDs.
- IDs use lowercase snake case in authored data.
- Use `FName` for compact authored gameplay IDs after validation.
- Use `FPrimaryAssetId` for Unreal asset identity.
- Do not use asset IDs as gameplay IDs, and do not use gameplay IDs as asset paths.
- Actor names and display names are not persistent IDs.
- Unknown required references fail validation.
- Duplicate IDs fail validation.

## Types And Units

Initial C++ contracts should use:

- gameplay IDs: `FName`
- asset references: `FPrimaryAssetId`, `TSoftObjectPtr`, or `TSoftClassPtr`
- transforms: `FTransform3d` where available, otherwise clearly documented `FTransform`
- positions/distances in local Unreal space: centimeters, suffix `Cm`
- sector positions: light years, suffix `Ly`
- simulation distances: documented fixture simulation units from `frontier-test-fixture.md`

`FrameType` values:

- `system_barycentric`
- `body_relative`
- `station_relative`
- `gate_relative`

`Transform` is local to `AnchorId` when `AnchorId` is set. Otherwise it is system-barycentric for the active system.

Do not confuse coordinate frames with save-location states. Coordinate frames describe transform reference space. Save-location states describe what kind of restoration is required, such as free flight, station docked, gate arrival, or respawn.

## Frame Resolution Contract

`FSimulationClockSnapshot`:

- `AuthoritativeSimulationTimeSeconds`
- `TimeScale`
- `ClockOwner`: session, galaxy_simulation
- `RngStreamId`
- `RngCounter`
- `ProcessedEventWatermark`
- `ProcessedEventWatermarks`, optional per-stream/per-system map once scheduled simulation events are active

This is a core runtime value type. It is shared by save/load, orbit evaluation, map/debug rendering, route sampling, local bubble projection, and AI/economy catch-up. It must not be introduced as an AI-only struct.

Query-time evaluation must be explicit. A frame, route, orbit, docking, gate-arrival, or gravity query receives both the authoritative clock snapshot and the requested evaluation time. The requested time may be equal to now or a future prediction time. Querying a future time is pure calculation and must not advance the clock owner, mutate saved state, or consume persistent RNG counters.

`FFrameResolvedTransform`:

- `CoordinateFrame`
- `AnchorId`, optional
- `SimulationTimeSeconds`
- `PositionCm`
- `Rotation`
- `LinearVelocityCmPerSec`
- `AngularVelocityRadPerSec`, optional
- `bActorSpaceValid`
- `ActorSpaceTransform`, optional, non-saveable

This is a core runtime value type, not only an AI helper. It is the handoff between logical frame-space simulation and rebased Unreal actor space. It may be cached for active-system runtime work, but saves must persist the logical frame/location fields instead of `ActorSpaceTransform`.

## Headless Frame Query Service

Orbit, route, gravity-well, and computed-anchor queries must be available as a data-only service before active-system actors are built.

`UOrbitRouteFrameQueryService` or equivalent C++ service:

- accepts immutable `FStarSystemDefinition`, mutable saved system/session state, and `FSimulationClockSnapshot`
- evaluates body, station, gate, docking-port, computed-anchor, gravity-well, and route-sample frames at a requested simulation time
- returns `FFrameResolvedTransform`, `FRouteSample`, and gravity query results without spawning actors, loading Blueprints, or reading world transforms
- is deterministic from `(SystemId, definition content, mutable state, simulation time, route policy IDs, RNG stream/counter where applicable)`
- supports offline catch-up, save validation, AI planning, route recovery, gate arrival placement, and active-system build seeding
- may use pure data caches inside one load/catch-up operation, but those caches are discarded or invalidated when authoritative time, definitions, or mutable route/system state changes

The active `UStarSystemSubsystem` may wrap or cache this service after system build. It must not be the only implementation path, because load catch-up needs route and frame answers before actors, components, or registries exist.

## Start Profile

`FStartProfileDefinition`:

- `StartProfileId`
- `DisplayName`
- `SystemId`
- `SpawnZoneId`
- `DockedStationId`, optional
- `DockingPortId`, optional
- `ShipArchetypeId`
- `InitialInventoryProfileId`, optional

The active foundation uses `frontier_test_start`.

## Ship Archetype

`FShipArchetypeDefinition`:

- `ShipArchetypeId`
- `DisplayName`
- `ShipClassTags`
- `PawnClass`, soft class reference
- `ActorClass`, optional soft class reference for NPC/traffic variants
- `MovementProfileId`
- `DefaultCargoCapacityMassKg`
- `DefaultCargoCapacityVolumeM3`
- `DefaultDurabilityProfileId`
- `DefaultLoadoutProfileId`
- `DefaultResourceProfileId`
- `DockingSizeClass`
- `AllowedDockingPortTags`

The active foundation uses this to identify the player's starting craft and prove the ship can be spawned from data. Detailed equipment, ship purchase, outfitting, insurance, and progression live in ship-instance and service contracts.

`FShipMovementProfileDefinition`:

- `MovementProfileId`
- `SupportedFlightModeTags`
- `NormalFlightTuningId`
- `SupercruiseTuningId`
- `MaxLocalSpeedCmPerSec`, positive
- `MaxAngularSpeedDegPerSec`, positive

`FShipDurabilityProfileDefinition`:

- `DurabilityProfileId`
- `MaxHull`, positive
- `MaxShield`, optional but non-negative when present
- `DisabledHullFraction`
- `DestroyedHullFraction`

`FShipLoadoutProfileDefinition`:

- `LoadoutProfileId`
- `DefaultInstalledSlotIds`
- `HardpointTags`
- `RequiredResourceProfileId`

`FShipResourceProfileDefinition`:

- `ResourceProfileId`
- `ResourceCapacities`
- `DefaultFillPolicy`

These profiles are deliberately thin foundation records. They exist so ship archetype references are real Unreal-native data, not unresolved strings. Equipment, balance, weapon, fuel, ammunition, and service systems may extend them without changing the spawn contract.

## Star Catalog

`FStarCatalogEntry`:

- `SystemId`
- `DisplayName`
- `StellarClass`
- `CatalogSource`
- `GalacticPositionLy`
- `DisplayPositionFrame`
- `bAuthored`
- `bDiscoverable`
- `bInitiallyKnown`
- `RouteTier`
- `SystemDefinitionAsset`, optional
- `GenerationSeed`, optional

`FRouteGraphEdge`:

- `SourceSystemId`
- `DestinationSystemId`
- `RouteType`
- `DistanceLy`
- `RouteGenerationRule`
- `bNaturalWormhole`
- `bArtificialGate`
- `bOfficialRoute`
- `bCriminalRoute`
- `bInitiallyKnown`
- `TraversalRequirementId`, optional

The sector model should preserve the useful Godot behavior: nearby real-star list, route tier, galactic coordinates, generated MST backbone, short-range bonus links, special Wolf 359 to Ross 154 natural wormhole, and BFS/fewest-jump route display.

In-system traffic routes are separate from interstellar route graph edges. Traffic route segments must store endpoint anchors and route policies, not baked world-space splines.

`FTrafficRouteSegmentDefinition`:

- `RouteSegmentId`
- `SourceAnchorId`
- `DestinationAnchorId`
- `SourceLocalOffsetCm`
- `DestinationLocalOffsetCm`
- `RoutePolicyId`
- `RouteGeometryPolicyId`
- `TravelModelId`
- `RouteProgressSemantic`: time_fraction, distance_fraction, policy_curve
- `AvoidanceAnchorIds`
- `ExclusionZoneIds`
- `RouteFrameBasisPolicy`
- `ControlData`, optional
- `JurisdictionId`, optional
- `SecurityRating`
- `RouteValue`
- `AllowedShipClassTags`
- `RiskProfileId`, optional
- `bSupportsPatrolCoverage`
- `bSupportsPirateAmbush`

Route sampling evaluates endpoint anchors at authoritative simulation time. The route contract must support evaluation, travel-time estimation, derivative/velocity calculation, and inverse mapping back to progress when the ship state is route-recoverable.

Endpoint offsets are measured in each endpoint anchor's declared frame basis. `RouteFrameBasisPolicy` defines how the route sample frame is built from the two moving endpoints:

- `source_to_destination`: forward axis points from evaluated source to evaluated destination.
- `source_basis`: sample rotation inherits the source anchor basis.
- `destination_basis`: sample rotation inherits the destination anchor basis.
- `policy_curve`: `RouteGeometryPolicyId` supplies the basis and tangent rule.

`FRouteSample.Tangent` and `EstimatedVelocityCmPerSec` must include endpoint motion and travel-along-route motion. A ship demoted from actor steering may restore onto a route only when its `LogicalMotionState` is route-recoverable and the inverse progress query can prove the recovered frame, basis, and velocity are within policy tolerance.

## System Definition

`FStarSystemDefinition`:

- `SystemId`
- `DisplayName`
- `SourceType`: authored, generated, imported
- `Seed`
- `Bodies`
- `Stations`
- `Gates`
- `SpawnZones`
- `GravityWells`
- `ResourceZones`
- `TrafficRoutes`
- `PatrolZones`
- `AmbushZones`
- `MapEntries`

Generation must output this definition first. Runtime actors are built from the definition.

The narrow startup profile requires at least:

- `SystemId`
- `DisplayName`
- `Bodies`
- `Stations`
- `Gates`
- `SpawnZones`

## Body Definition

`FBodyDefinition`:

- `BodyId`
- `DisplayName`
- `BodyType`: star, planet, moon, asteroid, gas_giant, station_anchor
- `ParentBodyId`, optional
- `FrameType`
- `AnchorId`, optional
- `Transform`
- `Orbit`
- `PhysicalReferenceRadiusCm`
- `VisualRadiusCm`
- `CollisionRadiusCm`
- `VisualProfile`
- `AtmosphereProfile`, optional
- `NavigationTarget`, optional
- `MapColor`
- `MapIconScale`

Gravity and supercruise fields belong in `FGravityWellDefinition`, not directly on the body.

## Orbit Definition

`FOrbitDefinition`:

- `ParentId`
- `OrbitFrameBasis`: parent_equatorial, parent_ecliptic, explicit_axes
- `BasisForward`, optional when `OrbitFrameBasis = explicit_axes`
- `BasisRight`, optional when `OrbitFrameBasis = explicit_axes`
- `BasisUp`, optional when `OrbitFrameBasis = explicit_axes`
- `SemiMajorAxisCm`
- `PeriodSeconds`
- `PhaseOffsetRadians`
- `EpochSeconds`
- `Eccentricity`
- `InclinationDegrees`
- `LongitudeOfAscendingNodeDegrees`
- `ArgumentOfPeriapsisDegrees`
- `Frame`: system or body-relative
- `AttitudeMode`: inherit_orbit_frame, face_velocity, face_parent, fixed_relative, spin
- `FixedRelativeRotation`, optional
- `SpinAxisLocal`, optional
- `SpinPeriodSeconds`, optional
- `SpinPhaseOffsetRadians`, optional
- `bEvaluateVelocity`
- `bEvaluateAngularVelocity`

Initial behavior preserves the Godot-style deterministic rail orbit before any more complex model is considered.

Authoritative simulation time belongs to the persistent session/galaxy simulation owner. `UStarSystemSubsystem` exposes the active system's view of that time for world-local orbit evaluation. Orbit components evaluate transforms from authoritative time plus orbit definition/epoch. They do not advance their own authoritative clocks.

There is no per-orbit gameplay clock flag. Any visual-only elapsed time must be named `PresentationTimeSeconds` and must not feed gameplay prediction, route sampling, saves, economy, or AI.

Orbit output must provide a full `FFrameResolvedTransform` when `bEvaluateVelocity` or `bEvaluateAngularVelocity` is true. Stations, gates, docking ports, route endpoints, formation slots, and AI prediction all consume the same resolved position, attitude, linear velocity, and angular velocity. A moving docking port inherits station orbit attitude and spin first, then applies the immutable station-local port transform.

Orbit-attitude math invariants:

- orbit basis axes are normalized, mutually orthogonal, and right-handed after validation
- explicit axes fail validation if the derived basis is degenerate or left-handed beyond tolerance
- eccentricity must be in the supported rail range; the initial rail implementation supports `0 <= Eccentricity < 1`
- position and velocity are evaluated from the same anomaly/time sample, not from separate rounded samples
- `AttitudeMode = face_velocity` requires non-zero evaluated velocity; zero-velocity definitions must explicitly use the validated orbit-frame forward axis and emit a warning/result flag
- `AttitudeMode = face_parent` points the declared forward axis toward the parent at the same simulation time
- spin applies around `SpinAxisLocal` after the base attitude mode, using `EpochSeconds` and `SpinPhaseOffsetRadians`
- quaternion output is normalized and finite
- angular velocity is expressed in the same coordinate frame as the resolved rotation

Required orbit-attitude tests:

- same definition and authoritative time produce bit-stable position, rotation, velocity, and angular velocity within deterministic floating-point tolerance
- circular equatorial orbit at phase zero resolves on the expected basis axis
- inclined orbit preserves radius and plane normal across representative phases
- explicit-axis validation rejects collinear or zero-length axes
- spin phase wraps without discontinuity across period boundaries
- moving station port transform equals station resolved transform composed with station-local port transform

## Station Definition

`FStationDefinition`:

- `StationId`
- `DisplayName`
- `FrameType`
- `AnchorId`, optional
- `Transform`
- `ParentId`
- `AnchorRule`: orbit, fixed_offset, body_relative, deep_space
- `Orbit`, optional
- `RelativeTransform`, optional
- `StationProfile`
- `DockingPorts`
- `NavigationTarget`
- `MapEntry`
- `StationRole`
- `RegionId`
- `OwnerFactionId`
- `MarketProfileId`
- `MarketId`, optional; defaults to `StationId` unless a station deliberately hosts multiple markets
- `MissionTags`
- `InteriorProfileId`
- `ServiceProfileId`, optional
- `FactionId`, optional

Keep physical station anchoring, service/faction/market identity, interior/service destination, and navigation target as distinct data. Do not recreate Godot's hard-coded station registry.

## Docking Port Definition

`FDockingPortDefinition`:

- `PortId`
- `DisplayName`
- `ApproachTransform`
- `DockedTransform`
- `UndockTransform`
- `PortFrameBasis`: station_local
- `ActivationRangeCm`
- `MaxApproachSpeedCmPerSec`
- `RequiredAlignmentDegrees`
- `AllowedShipClasses`

Docking-port transforms are station-local and use the station's resolved attitude basis. Moving-frame docking evaluates `StationResolvedTransform * PortLocalTransform` at authoritative simulation time; it must not cache a world transform across ticks.

`FDockingPortRuntimeState`:

- `StationId`
- `PortId`
- `OccupyingShipId`, optional
- `ReservedShipId`, optional
- `ReservationExpiryTimeSeconds`, optional
- `ClearanceId`, optional
- `State`: free, reserved, occupied, blocked, disabled
- `LastFailureReason`, optional
- `RuntimeStateRevision`
- `LastChangedSimulationTimeSeconds`

Docking-port definitions are immutable. Occupancy, reservation, clearance, and blocked state are runtime/save state. Test fixtures may seed initial runtime occupancy, but that seed must not live in `FDockingPortDefinition`.

`FDockingOperationState`:

- `DockingOperationId`
- `ShipInstanceId`
- `StationId`
- `PortId`
- `State`: requested, reserved, approach, final_assist, docked, undocking, aborted, failed
- `ClearanceId`, optional
- `ReservationExpiryTimeSeconds`, optional
- `OperationStartTimeSeconds`
- `LastStateChangeTimeSeconds`
- `CapturedPortRelativeTransform`, optional
- `CapturedRelativeVelocityCmPerSec`, optional
- `ServiceTransactionIds`, optional
- `FailureReason`, optional

Per-ship docking operation state is separate from `FDockingPortRuntimeState`. The port runtime state answers whether a port is free, reserved, occupied, blocked, or disabled. The operation state answers what one ship is doing and how to resume or abort it across save/load.

## Gate Definition

`FGateDefinition`:

- `GateId`
- `DisplayName`
- `FrameType`
- `AnchorId`, optional
- `Transform`
- `ParentId`
- `AnchorRule`: l2_anti_star, body_relative, fixed_offset, deep_space
- `AnchorDistanceCm`
- `RelativeTransform`, optional
- `DestinationSystemId`
- `DestinationGateId`
- `DestinationArrivalId`
- `ActivationRangeCm`
- `ArrivalOffset`
- `GateProfile`
- `NavigationTarget`
- `MapEntry`

Computed gate anchors such as Lagrange-style placements need explicit prediction data:

- `PrimaryAnchorId`, optional
- `SecondaryAnchorId`, optional
- `AxisRule`, optional
- `DistanceCm`, optional
- `EvaluationRuleId`, optional

The gate's derived transform and velocity must evaluate from authoritative simulation time just like stations and bodies.

Gate arrival kinematics contract:

- arrival starts from destination `SystemId`, `DestinationGateId`, `DestinationArrivalId`, optional `ArrivalOffset`, and saved/transition ship kinematics
- the destination gate frame is evaluated by the headless frame query service before active actor placement
- `DestinationGateId` identifies the destination transit endpoint; `DestinationArrivalId` identifies the destination arrival marker or computed arrival target attached to that endpoint
- `ArrivalOffset` is gate-local unless its definition explicitly declares another frame
- arrival position is `GateResolvedTransform * ArrivalOffset`
- arrival rotation defaults to the gate arrival basis; an authored arrival rotation offset may be composed after the gate basis
- arrival linear velocity is gate linear velocity plus authored/local arrival relative velocity transformed through the gate basis
- inherited angular velocity from the gate frame is considered when restoring relative velocity and immediate collision safety
- the restored mode is `gate_arrival` until the arrival safety window completes, then transitions to normal flight or supercruise according to transition policy
- if the gate frame, destination system, or arrival offset is invalid, transition/load fails instead of falling back to a spawn zone

The contract must produce the same arrival transform and velocity whether evaluated during a live transition, load from `gate_arrival`, or an automated transition test.

## Spawn Zone Definition

`FSpawnZoneDefinition`:

- `SpawnZoneId`
- `DisplayName`
- `FrameType`
- `AnchorId`, optional
- `Transform`
- `RadiusCm`
- `AllowedContexts`: new_game, arrival, debug, respawn

## Navigation Target Definition

`FNavigationTargetDefinition`:

- `TargetId`
- `DisplayName`
- `TargetType`: body, station, gate, zone, resource, debug
- `FrameType`
- `AnchorId`
- `bShowInHud`
- `bShowInSystemMap`
- `bCanTarget`

Body targets may be derived from `BodyId`, `DisplayName`, and transform for narrow startup fixtures when `NavigationTarget` is omitted.

## Map Entry Definition

`FMapEntryDefinition`:

- `EntryId`
- `DisplayName`
- `TargetId`
- `IconType`
- `MapColor`
- `MapIconScale`

## Resource Zone Definition

`FResourceZoneDefinition`:

- `ZoneId`
- `DisplayName`
- `FrameType`
- `AnchorId`, optional
- `Transform`
- `RadiusCm`
- `NavigationTarget`, optional
- `MapEntry`, optional

## Moving Target And Route AI Contracts

These space AI contracts are listed here because they depend directly on system frames, route anchors, and stable IDs.

`FMovingFrameTarget`:

- `TargetId`
- `TargetType`: body, station, gate, route_sample, ship, formation_slot, patrol_zone, ambush_zone, flee_destination
- `CoordinateFrame`
- `AnchorId`
- `LocalOffsetCm`
- `RouteSegmentId`, optional
- `RouteProgress01`, optional
- `ShipInstanceId`, optional
- `GroupId`, optional

`FMovingFrameTarget` is a descriptor only. It must not contain prediction time, cached transform, cached velocity, or actor references.

`FAIPredictedTransform`:

- `Target`
- `SimulationTimeSeconds`
- `ResolvedTransform`
- `bValid`
- `InvalidReason`, optional

`FRouteSample`:

- `RouteSegmentId`
- `SimulationTimeSeconds`
- `RouteProgress01`
- `RouteDistanceCm`, optional
- `RouteProgressSemantic`: time_fraction, distance_fraction, policy_curve
- `ResolvedTransform`
- `Tangent`
- `EstimatedVelocityCmPerSec`
- `SourceAnchorTransform`
- `DestinationAnchorTransform`
- `RouteGeometryPolicyId`
- `TravelModelId`
- `SecurityRating`
- `RiskScore`

Route sampling must support `EvaluateRoute`, `EstimateTravelTime`, and `FindClosestProgress`. `FindClosestProgress` is valid only for route-recoverable logical motion states.

`fixture_dynamic_arc_v1` route geometry algorithm contract:

- deterministic fixture policy for early moving-endpoint traffic routes
- inputs are source/destination resolved transforms, source/destination local offsets, route progress, simulation time, `ControlData`, and route segment ID
- source point `P0` and destination point `P1` are evaluated at the requested simulation time from their live anchor frames
- chord direction is normalized `P1 - P0`; zero-length chords fail route validation
- arc normal is selected from `ControlData.ArcNormalHint` projected away from the chord, or from the source basis up axis, or from deterministic tie-break axes chosen by route segment ID
- arc height defaults to `0.15 * ChordLength` unless `ControlData.ArcHeightCm` is supplied; height is clamped to route policy min/max
- curve position uses a quadratic Bezier with control point `Pc = Midpoint(P0, P1) + ArcNormal * ArcHeightCm`
- progress is clamped to `[0, 1]`; route policies may reject out-of-range progress before sampling
- tangent is the normalized derivative of the Bezier; endpoint degeneracy must explicitly use chord direction and emit a warning/result flag
- basis forward is tangent; basis up is arc normal re-orthogonalized against tangent; basis right completes a right-handed frame
- estimated velocity equals endpoint-motion interpolation plus derivative-with-respect-to-progress times `dProgress/dTime` from the travel model
- `FindClosestProgress` projects onto the same Bezier by bounded deterministic subdivision plus fixed-iteration Newton refinement; it returns progress, distance error, basis error, and velocity error
- all tie breaks use route segment ID and endpoint IDs, never actor order or registry iteration order

This policy is for deterministic fixtures and tests, not the only future route shape. Other route geometry policies must provide the same evaluate, derivative, travel-time, and inverse-progress semantics.

`FInterceptEstimate`:

- `bFeasible`
- `EarliestArrivalTimeSeconds`
- `InterceptTarget`
- `InterceptTransform`
- `RequiredFlightMode`
- `FailureReason`, optional
- `Confidence`
- `EvaluatedSpeedProfileId`
- `bCrossesGravityLockout`
- `bRequiresFrameTransition`

Intercept estimates must evaluate the target at arrival time and account for drive cooldown, turn limits, lockout/dropout crossings, and reference-frame transitions.

`FShipGoalState`:

- `GoalType`
- `SavedGoal`, optional
- `RouteId`, optional
- `RouteSegmentId`, optional
- `RouteProgress01`, optional
- `LogicalMotionState`: on_route, approaching_anchor, docking, off_route_free_flight, pursuing, fleeing, recovering_to_route
- `DestinationStationId`, optional
- `TargetShipId`, optional
- `ThreatShipId`, optional
- `TargetFrame`, optional
- `FleeDestination`, optional
- `FleeCommitUntilTimeSeconds`, optional
- `CargoSummary`
- `InterruptReason`, optional
- `LastGoalUpdateTimeSeconds`

Flee destination changes require a materially better score, expiry of the commitment window, or invalidation of the current destination.

`FShipTrafficInstance`:

- `ShipInstanceId`
- `ShipArchetypeId`
- `OwningFactionId`
- `JurisdictionId`, optional
- `CurrentTier`
- `LogicalLocation`
- `LogicalRotation`
- `LogicalVelocityCmPerSec`
- `VelocityFrame`
- `DominantFrameAnchorId`, optional
- `bInheritsFrameVelocity`
- `FrameVelocityCmPerSec`
- `FlightMode`
- `GoalState`
- `HealthSummary`
- `CargoManifestSummary`
- `GroupId`, optional
- `CurrentEventId`, optional
- `LastUpdateTimeSeconds`
- `RngStreamId`
- `EventWatermark`

`LogicalLocation` is the only durable ship position source for traffic. It is a `FMovingFrameTarget` or equivalent frame/location record with enough frame data to resolve a transform through the headless frame query service. `LogicalRotation` and `LogicalVelocityCmPerSec` are expressed in `VelocityFrame` or in the location frame when `VelocityFrame` is unset. `CurrentFrame`, `AnchorId`, `PositionCm`, `Rotation`, and `LinearVelocityCmPerSec` must not also exist as independent saved fields on `FShipTrafficInstance`; they may appear only inside a resolved/query result or a non-saveable realization snapshot. If `bInheritsFrameVelocity` is true, `FrameVelocityCmPerSec` is persisted so promotion, demotion, and offline catch-up do not introduce a speed jump.

Promotion uses `LogicalLocation` plus the current authoritative time to create a `FFrameResolvedTransform`. Demotion writes back exactly one logical location record and velocity frame. If actor steering cannot be converted into a route-recoverable or frame-relative logical location within policy tolerance, the ship becomes `off_route_free_flight` with an explicit anchor/frame instead of silently keeping stale route progress.

`FShipRealizationSnapshot`:

- `ShipInstanceId`
- `CurrentTier`
- `LogicalMotionState`
- `FrameResolvedTransform`
- `VelocityFrame`
- `DominantFrameAnchorId`, optional
- `bInheritsFrameVelocity`
- `FrameVelocityCmPerSec`
- `PoolGenerationId`
- `ActiveHandleId`, optional
- `FlightMode`
- `GoalState`
- `HealthSummary`
- `CargoManifestSummary`
- `TargetShipId`, optional
- `ThreatShipId`, optional
- `GroupId`, optional
- `CurrentEventId`, optional
- `ActorBehaviorResumeKey`, optional
- `LastAuthoritativeSimulationTimeSeconds`

`PoolGenerationId` increments each time a pooled actor is acquired for a logical ship. Debug handles and UI references must include both `ActiveHandleId` and `PoolGenerationId`; stale handles are invalid after release or reuse and are never saved as canonical state.

`FActiveTrafficHandle`, non-saveable:

- `ActiveHandleId`
- `ShipInstanceId`
- `ActorInstanceId`, optional
- `PoolGenerationId`
- `AcquiredTimeSeconds`
- `ReleasedTimeSeconds`, optional
- `bValid`

`FTrafficDebugSnapshot`, non-saveable:

- `ShipInstanceId`
- `CurrentTier`
- `ActiveHandleId`, optional
- `PoolGenerationId`, optional
- `ActorName`, optional debug-only
- `LogicalLocation`
- `FrameResolvedTransform`, optional
- `GoalState`
- `RealizationState`
- `BlockedPromotionReason`, optional
- `LastUpdateTimeSeconds`

Active handles and debug snapshots are diagnostic/runtime surfaces only. Save files persist `FShipTrafficInstance` and related logical event/group records, never pool handles, actor names, or generated actor IDs.

`FShipGroupState`:

- `GroupId`
- `GroupType`: convoy, patrol_wing, pirate_pack, escort_contract, mission_group
- `LeaderShipId`
- `MemberShipIds`
- `RouteSegmentId`, optional
- `GoalState`
- `FormationSlots`
- `OwningFactionId`, optional
- `JurisdictionId`, optional
- `LastUpdateTimeSeconds`

`FFormationSlotState`:

- `GroupId`
- `SlotId`
- `AssignedShipId`, optional
- `Role`
- `LeaderShipId`
- `SlotFrame`: leader_ship, leader_route_sample, group_anchor
- `SlotBasis`: leader_attitude, route_tangent, explicit_axes
- `BasisForward`, optional when `SlotBasis = explicit_axes`
- `BasisRight`, optional when `SlotBasis = explicit_axes`
- `BasisUp`, optional when `SlotBasis = explicit_axes`
- `RouteSegmentId`, optional when `SlotFrame = leader_route_sample`
- `RouteProgressOffset01`, optional when `SlotFrame = leader_route_sample`
- `AnchorId`, optional when `SlotFrame = group_anchor`
- `LocalOffsetCm`
- `MaxSeparationCm`
- `RejoinPolicy`
- `BreakPolicy`

Formation slots are not absolute positions. The slot frame and basis define how `LocalOffsetCm` is evaluated at prediction time. Tier 2 uses the leader's predicted ship or route-sample frame; Tier 1 may steer loosely toward the same evaluated slot but must write back the stable slot identity on demotion.

`FPatrolWaypointTarget`:

- `Target`
- `ArrivalRadiusCm`
- `HoldSeconds`
- `WaypointPolicy`

`FPatrolZoneDefinition`:

- `PatrolZoneId`
- `JurisdictionId`
- `OwningFactionId`
- `AnchorFrame`
- `AnchorId`
- `CoveredRouteSegmentIds`
- `WaypointTargets`
- `ScanPolicyId`
- `InterdictionPolicyId`
- `DistressResponsePolicyId`
- `EscalationPolicyId`
- `ThreatAvoidancePolicyId`
- `ScanRadiusCm`
- `InterdictionRadiusCm`
- `ResponseTimeWindowSeconds`
- `MinPatrolStrength`
- `MaxPatrolStrength`

`FRouteSecuritySnapshot`:

- `RouteSegmentId`
- `WindowStartTimeSeconds`
- `WindowEndTimeSeconds`
- `PatrolCoverage`
- `ResponseEtaSeconds`
- `EnforcementStrength`
- `CriminalPressure`
- `RecentIncidentIds`
- `Confidence`

`FPatrolAssetState`:

- `PatrolAssetId`
- `ShipOrGroupId`
- `JurisdictionId`
- `CurrentAssignmentId`
- `bResponseAvailable`
- `Strength`
- `CooldownUntilTimeSeconds`
- `FuelOrRangeState`
- `PursuitAuthority`

`FPirateAmbushProfile`:

- `AmbushProfileId`
- `OwningFactionId`, optional
- `PreferredRouteTags`
- `MinimumRouteValue`
- `MaximumPoliceRisk`
- `MinimumVictimCargoValue`
- `MinimumVictimIsolationScore`
- `RequiredEscapeRouteScore`
- `DemandPolicyId`
- `InterdictionPolicyId`
- `FleePolicyId`

`FAmbushSiteState`:

- `AmbushSiteId`
- `RouteSegmentId`
- `RouteProgressMin01`
- `RouteProgressMax01`
- `TimeWindowStartSeconds`
- `TimeWindowEndSeconds`
- `AnchorFrame`
- `EscapeTarget`
- `OwningGroupId`
- `EncounterEventId`, optional

`FLogicalContactTrack`:

- `ObserverId`
- `TargetId`
- `TargetType`
- `LastKnownTarget`
- `Signature`
- `Confidence`
- `IdentificationLevel`
- `FirstSeenTimeSeconds`
- `LastSeenTimeSeconds`
- `DecayPolicyId`
- `WitnessEvidenceState`

`FSimulationEventRecord`:

- `EventId`
- `SystemId`
- `EventType`
- `SourceType`: ship, group, station, faction, market, mission, debug
- `SourceId`, optional
- `TargetType`, optional
- `TargetId`, optional
- `LocationTarget`, optional
- `ParticipantShipIds`, optional
- `ParticipantGroupIds`, optional
- `PayloadRef`, optional
- `State`
- `CreatedTimeSeconds`
- `ScheduledTimeSeconds`, optional
- `ExpiryTimeSeconds`
- `RngStreamId`
- `RngCounter`
- `IdempotencyKey`
- `ProcessedEventWatermark`

`FSimulationEventResultRecord`:

- `EventId`
- `ResultId`
- `AppliedTimeSeconds`
- `OutcomeType`
- `AffectedStateRefs`
- `DebugReason`
- `IdempotencyKey`

All encounter, legal, economy, cargo, distress, comms, and mission events use this canonical envelope so save/load idempotency is one system instead of many ad hoc flags.

`FLogicalEncounterEvent` is a domain-specific payload/view over `FSimulationEventRecord` for interdiction, pirate ambush, police scan, distress call, patrol response, engagement, cargo surrender, ship destruction, and route disruption. It must not create a second event identity or second processed watermark.

`FLogicalEngagementRecord`:

- `EngagementId`
- `EncounterEventId`
- `AttackerGroupId`
- `DefenderGroupId`
- `WeaponStrengthSummary`
- `RangeModelId`
- `DamageStateSummary`
- `SurrenderThreshold`
- `FleeThreshold`
- `DistressPolicyId`
- `CargoTransferResultIds`
- `MarketTransactionResultIds`
- `OffenseEventIds`
- `EvidenceRecordIds`
- `CriminalRecordIds`
- `FactionReputationDeltaIds`
- `GeneratedMessageIds`

`FInterdictionHazardState`:

- `HazardId`
- `RouteSampleTarget`
- `OwningGroupId`
- `TriggerConditions`
- `DetectionSignature`
- `bArmed`
- `bConsumed`
- `ExpiryTimeSeconds`
- `SaveLoadEventId`

`FDistressEventRecord`:

- `DistressEventId`
- `SourceShipOrGroupId`
- `ThreatId`
- `LocationTarget`
- `BroadcastRadiusCm`
- `Confidence`
- `RespondingPatrolAssetIds`
- `PlayerMarkerState`
- `ExpiryTimeSeconds`
- `RewardEventIds`
- `LegalEventIds`
- `GeneratedMessageIds`

`FCargoManifestSummary`:

- `CommodityIds`
- `TotalValue`
- `LegalitySummary`
- `OwnerFactionId`
- `MissionFlags`
- `JettisonableFraction`
- `DestructionLossPolicy`

`FAIDecisionReason`:

- `DecisionId`
- `ShipInstanceId`
- `GoalType`
- `SelectedTarget`
- `PrimaryScore`
- `ScoreBreakdown`
- `RejectedAlternatives`
- `SimulationTimeSeconds`

`FProcessedEventWatermark`:

- `SystemId`
- `LastProcessedEventId`
- `LastProcessedTimeSeconds`

`FSectorSimulationSummary`:

- `SectorId`
- `SystemIds`
- `StationMarketIds`
- `PendingEventQueueIds`
- `FactionInfluenceStateIds`
- `SecurityPressure`
- `PiracyPressure`
- `RecentEventIds`
- `LastUpdateTimeSeconds`

`FCrossSectorScheduledJob`:

- `JobId`
- `SourceStationId`
- `DestinationStationId`
- `RouteId`
- `RouteSegmentIds`
- `CargoManifestSummary`
- `OwningFactionId`
- `DepartureTimeSeconds`
- `ArrivalTimeSeconds`
- `RiskProfileId`
- `CurrentEventId`
- `ProcessedEventWatermark`

`FZoneInfluenceState`:

- `ZoneId`
- `FactionInfluenceByFactionId`
- `SecurityPressure`
- `CriminalPressure`
- `LastUpdateTimeSeconds`

These structs must remain actor-independent. Actor references may appear only in non-saveable Tier 1 runtime adapters or debug output.

## Gravity Well Definition

`FGravityWellDefinition`:

- `WellId`
- `AnchorId`
- `CoordinateFrame`: system_barycentric, body_relative, station_relative, gate_relative
- `LocalOffsetCm`
- `Shape`: sphere
- `GravityBodyId`, optional
- `AffectedFlightModes`: supercruise, cruise, normal
- `SlowdownRadiusCm`
- `LockoutRadiusCm`
- `DropoutRadiusCm`
- `MinimumSafeDropoutRadiusCm`
- `SlowdownCurveId`, optional
- `SpeedCeilingProfileId`, optional
- `Strength`
- `Priority`
- `OverlapPolicy`: strongest_slowdown, nearest_lockout, priority_override, union_blocking
- `bBlocksSupercruiseEntry`
- `bForcesSupercruiseDropout`
- `bAffectsRouteEstimates`
- `DebugColor`, optional
- `ProfileId`, optional

Gravity wells are gameplay volumes. Bodies may author or generate them, but body definitions do not own supercruise lockout fields directly.

Radii are centered on `AnchorId + LocalOffsetCm` evaluated at authoritative simulation time. Required validation: `SlowdownRadiusCm >= LockoutRadiusCm >= DropoutRadiusCm`, all radii are non-negative, and any navigation target dropout band near a station or gate must either remain outside conflicting well radii or explicitly document the intended slowdown/lockout behavior.

Gravity-well overlap resolver:

- evaluates every participating well from authoritative simulation time through the headless frame query service
- computes normalized distance bands for slowdown, lockout, dropout, and minimum safe dropout independently
- slowdown factor is the most restrictive active speed-ceiling result unless `priority_override` selects a higher-priority well
- lockout is true when any `bBlocksSupercruiseEntry` well contains the ship inside its lockout radius
- forced dropout is true when any `bForcesSupercruiseDropout` well contains the ship inside its dropout radius
- nearest well in query output is the nearest by normalized active radius, not raw centimeters, so large stars do not hide small station hazards
- ties resolve by higher `Priority`, then lexicographic `WellId`
- minimum safe dropout radius is the maximum active safe radius among wells that caused or constrained dropout
- route estimates use only wells with `bAffectsRouteEstimates`

The resolver returns all contributing well IDs for debug, plus the selected blocking/dropout well ID used for player-facing reason text.

## Ship Save Location

`FShipSaveLocation`:

- `SystemId`
- `CoordinateFrame`: system_barycentric, body_relative, station_relative, gate_relative
- `LocationMode`: free_flight, station_docked, gate_arrival, respawn
- `AnchorId`
- `ShipInstanceId`
- `DockingPortId`, optional
- `DockingOperationId`, optional
- `PositionCm`
- `Rotation`
- `LinearVelocityCmPerSec`
- `VelocityFrame`
- `DominantFrameAnchorId`, optional
- `bInheritsFrameVelocity`
- `FrameVelocityCmPerSec`
- `FlightMode`
- `LocalBubbleOrigin`, optional projection hint for free-flight actor placement
- `LocalBubbleEpochSeconds`, optional projection hint timestamp
- `SimulationTimeSeconds`

Saves must use logical IDs and frame data, not Actor paths.

The narrow startup save payload requires only:

- `SystemId`
- `SpawnZoneId`
- `SelectedTargetId`, optional

The startup save still uses the save envelope header defined in `save-load-and-versioning.md`: `SaveFormatVersion`, `GameContentVersion`, and `BuildCompatibilityId`.

The active save path uses `FShipSaveLocation` for free flight, docked state, gate arrival, and respawn.

## Save Game Envelope

`FSaveGameData`:

- `SaveFormatVersion`
- `GameContentVersion`
- `BuildCompatibilityId`
- `CreatedUtc`
- `LastSavedUtc`
- `SlotName`
- `SessionId`
- `StartProfileId`
- `SimulationClock`
- `SessionState`
- `ShipLocation`
- `PlayerState`
- `EconomyState`
- `CreditLedgerState`
- `EscrowState`
- `GameplayTransactionState`
- `CommitJournalState`
- `InventoryState`
- `EquipmentState`
- `ShipInstanceState`
- `ShipDurabilityState`
- `ShipLoadoutResourceState`
- `MissionState`
- `DiscoveryState`
- `FactionState`
- `FactionOperationalState`
- `ReputationDeltaState`
- `FactionRelationshipDeltaState`
- `FactionOperationalDeltaState`
- `LegalState`
- `EnforcementState`
- `StationServiceState`
- `ShipServiceResultState`
- `ConversationState`
- `MessageArbitrationResultState`
- `NpcSimulationState`
- `ScheduledAIEvents`
- `ProcessedEventWatermarks`
- `PerSystemRngStreams`
- `PermitState`
- `MessageLogState`
- `GeneratedFollowupOpportunityState`
- `RespawnInsuranceState`

Development saves may be rejected on version mismatch while the schema is still moving.

## Future Stable IDs

Later systems must preserve stable IDs for:

- sector ID
- system ID
- body ID
- station ID
- docking port ID
- spawn zone ID
- gate ID
- route ID
- zone/resource zone ID
- faction ID
- jurisdiction ID
- law profile ID
- enforcement profile ID
- offense event ID
- criminal record ID
- permit/access requirement ID
- commodity ID
- commodity/item bridge ID
- item definition ID
- item instance ID
- container ID
- credit account ID
- credit ledger entry ID
- escrow hold ID
- equipment ID
- market ID
- transaction ID
- trade job ID
- service/vendor ID
- station service endpoint ID
- station service request/result ID
- mission/quest ID
- mission instance ID
- objective ID
- enforcement action ID
- message/conversation ID
- message instance/log entry ID
- character/person ID
- NPC ship/group ID
- traffic route ID
- resource node ID
- discovery/contact record ID
- reputation record ID
- faction relationship ID
- evidence/witness record ID
- docking reservation/clearance ID
- damage/kill event ID
- ship archetype ID
- ship instance ID
