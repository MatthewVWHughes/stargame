# Frontier Test Fixture

`frontier_test_01` is the first Unreal architecture validation system.

It is not meant to be pretty. It is meant to expose the hard problems early: stable IDs, nested orbits, moving-frame docking, supercruise distances, gravity lockout, map readability, save/load, and non-Sol startup.

## Design Rules

- Do not use Sol, Earth, Mars, Luna, or Solar System assumptions.
- Prefer ugly readable placeholders over polished art.
- Every entity must have a stable gameplay ID.
- Every spawned actor must come from fixture data.
- Every important object must appear in debug registry output.

Temporary in-code fixture data is acceptable for M0 only if it is one coherent fixture definition. Actors must not hard-code fixture IDs or transforms locally.

## M0 Required Fixture Data

Start profile `frontier_test_start`:

- `SystemId`: `frontier_test_01`
- `SpawnZoneId`: `spawn_deep_space`
- `DockedStationId`: unset
- `DockingPortId`: unset

M0 targetable objects:

- body: `ember`
- station: `brink_watch`
- gate: `frontier_gate_a`

M0 spawned placeholders:

- body marker/proxy for `ember`
- station marker/proxy for `brink_watch`
- gate marker/proxy for `frontier_gate_a`

Optional debug-only entries:

- `frontier_primary`
- `brink`
- `brink_minor`
- `wayfarer_depot`
- `cinder_field`

Everything else in this fixture is M1+ validation content.

M0 spawns only `ember`, `brink_watch`, `frontier_gate_a`, and the player at `spawn_deep_space`. All other fixture IDs may exist only as unspawned data or comments until their milestone.

## Literal M0 Fixture Slice

All transforms are provisional Unreal centimeters in `system_barycentric` frame. They are not astrophysical values.

| ID | Display name | Kind | FrameType | AnchorId | Location cm | Rotation | RadiusCm | TargetType | bShowInHud | bCanTarget |
| --- | --- | --- | --- | --- | --- | --- | ---: | --- | --- | --- |
| `spawn_deep_space` | Deep Space Start | spawn_zone | `system_barycentric` | unset | `(0, 0, 0)` | `(0, 0, 0)` | `5000` | none | false | false |
| `ember` | Ember | body | `system_barycentric` | unset | `(250000, 0, 0)` | `(0, 0, 0)` | `8000` | body | true | true |
| `brink_watch` | Brink Watch | station | `system_barycentric` | unset for M0 | `(0, 350000, 0)` | `(0, 0, 0)` | `2000` | station | true | true |
| `frontier_gate_a` | Frontier Gate A | gate | `system_barycentric` | unset | `(-300000, -150000, 0)` | `(0, 0, 0)` | `3000` | gate | true | true |

Additional M0 fields:

- `frontier_gate_a.ActivationRangeCm = 5000`
- `frontier_gate_a.DestinationSystemId = unset`
- `frontier_gate_a.DestinationGateId = unset`
- `frontier_gate_a.DestinationArrivalId = unset`
- `TargetId == source object ID` for all M0 targets
- exactly three entries should satisfy `bShowInHud && bCanTarget`

M5.5 replaces the unset gate destination fields with the concrete transition fixture below. M0-M5 validation must still require them to remain unset unless the selected profile is `M5.5` or later.

## IDs

- System ID: `frontier_test_01`
- Start profile ID: `frontier_test_start`
- Primary star: `frontier_primary`
- Inner planet: `ember`
- Outer planet: `brink`
- Moon: `brink_minor`
- Orbiting station: `brink_watch`
- Deep-space station: `wayfarer_depot`
- Gate: `frontier_gate_a`
- Destination test system: `frontier_arrival_test_01`
- Destination gate: `arrival_gate_a`
- Destination arrival marker: `arrival_from_frontier_gate_a`
- Asteroid zone: `cinder_field`
- Deep-space spawn zone: `spawn_deep_space`
- Near-station spawn zone: `spawn_brink_watch`

## Bodies

### `frontier_primary`

- Type: star
- Purpose: central light/reference source
- M0: optional debug-only entry
- M1+: visual/reference proxy

### `ember`

- Type: rocky inner planet
- Purpose: simple visible approach target
- Features:
  - M0: visible body target
  - M2+: gravity well
  - M4+: supercruise slowdown radius
  - no docking requirement

### `brink`

- Type: outer planet
- Purpose: parent body for moon and orbiting station
- Features:
  - M1+: parent body for nested content
  - M2+: gravity well
  - M4+: supercruise lockout/dropout radius
  - M4+: intentionally distant from spawn so supercruise is required

### `brink_minor`

- Type: moon
- Parent: `brink`
- Purpose: M2+ nested orbit validation

## Stations

### `brink_watch`

- Parent: `brink`
- Rule: orbiting station
- Purpose: moving-frame docking test
- Requirements:
  - M0: stable station target
  - M2+: orbit fast enough to reveal bad relative-frame math
  - M5: at least one docking port
  - M0: appears in HUD/debug target list
  - M2+: appears in system map
  - M5: save/reload while docked must restore to this live moving frame

### `wayfarer_depot`

- Rule: deep-space or high-orbit station
- Purpose: simpler station target and service placeholder
- Requirements:
  - M1+: stable navigation target
  - M3+: can be used for local approach before moving-frame docking is ready
  - M5: at least one docking port

## Gate

### `frontier_gate_a`

- Purpose: transition endpoint placeholder
- Requirements:
  - stable gate ID
  - activation range
  - arrival offset
  - destination fields must be unset for M0; destination references become required only once transition validation exists
  - appears in HUD target list and system map

### M5.5 Gate Destination Expansion

`frontier_gate_a` must become a fully validated transition endpoint in M5.5. The source system remains `frontier_test_01`; the destination system is a deliberately small arrival-only fixture, not Sol.

Source gate fields:

| Field | Value |
| --- | --- |
| `GateId` | `frontier_gate_a` |
| `DestinationSystemId` | `frontier_arrival_test_01` |
| `DestinationArrivalId` | `arrival_from_frontier_gate_a` |
| `DestinationGateId` | `arrival_gate_a` |
| `ActivationRangeCm` | `5000` |
| `ArrivalTransformRule` | `gate_relative_offset` |
| `ArrivalFrame` | `gate_relative` |
| `ArrivalLocalOffsetCm` | `(0, 18000, 0)` |
| `ArrivalRotation` | `(0, 180, 0)` |

Catalog transition edge:

| Field | Value |
| --- | --- |
| `RouteEdgeId` | `edge_frontier_gate_a_to_arrival_gate_a` |
| `SourceSystemId` | `frontier_test_01` |
| `SourceGateId` | `frontier_gate_a` |
| `DestinationSystemId` | `frontier_arrival_test_01` |
| `DestinationGateId` | `arrival_gate_a` |
| `DestinationArrivalId` | `arrival_from_frontier_gate_a` |
| `bDiscoveredByDefault` | true |
| `bBidirectional` | false for M5.5 |

Destination system minimum slice:

| ID | Display name | Kind | FrameType | AnchorId | Location cm | Rotation | RadiusCm | TargetType | bShowInHud | bCanTarget |
| --- | --- | --- | --- | --- | --- | --- | ---: | --- | --- | --- |
| `arrival_gate_a` | Arrival Gate A | gate | `system_barycentric` | unset | `(0, 0, 0)` | `(0, 0, 0)` | `3000` | gate | true | true |
| `arrival_from_frontier_gate_a` | Frontier Gate Arrival | arrival_marker | `gate_relative` | `arrival_gate_a` | `(0, 18000, 0)` | `(0, 180, 0)` | `4000` | none | false | false |

M5.5 validation expectations:

- catalog lookup resolves `frontier_arrival_test_01`.
- catalog route edge resolves source system, source gate, destination system, destination gate, and destination arrival marker.
- destination system build registers `arrival_gate_a` and `arrival_from_frontier_gate_a`.
- arrival transform resolves from `arrival_gate_a` plus `ArrivalLocalOffsetCm`; hard-coded actor transforms fail validation.
- no transition fixture field may reference `sol`, `Earth`, `Mars`, or `Luna`.

## Zones

### `cinder_field`

- Type: asteroid/resource zone
- Purpose: future mining/resource/combat hook
- M1+ only needs stable ID, map entry, and debug visibility.

### `spawn_deep_space`

- Purpose: default new-game start for M0
- Requirements:
  - not near a planet lockout zone
  - has visible body/station/gate targets

### `spawn_brink_watch`

- Purpose: debug/local approach start
- Requirements:
  - close enough to test station approach
  - outside immediate docking activation range unless explicitly testing docking

## Required Test Cases

- boot directly into `frontier_test_01`
- spawn at `spawn_deep_space`
- M0: select `ember`, `brink_watch`, and `frontier_gate_a`
- M1: rebuild active system and preserve registry correctness
- M3: approach `brink_watch` in normal flight
- later: supercruise from deep-space spawn to `brink_watch`
- later: dock at `brink_watch`
- later: save while docked, reload, undock

## Scale Notes

These values are provisional and intentionally not astrophysical. They exist so M2-M5 can be tested against concrete numbers instead of vague scale bands. They belong in fixture data/profile assets, not scattered through actors.

### M1-M5 Provisional Scale Values

| Field | Value | Milestone | Notes |
| --- | ---: | --- | --- |
| `NormalFlightMaxSpeedCmPerSec` | `24000` | M3 | Initial normal-flight cap. |
| `LocalBubbleRadiusCm` | `5000000` | M2 | Active Unreal-space projection radius around the player. |
| `OriginShiftThresholdCm` | `2000000` | M2 | Rebase the local bubble when the player exceeds this distance from bubble origin. |
| `StationApproachBubbleRadiusCm` | `500000` | M3 | Near-station local approach/debug radius. |
| `DockingActivationRangeCm` | `15000` | M5 | Begin docking-request eligibility. |
| `DockingCorridorLengthCm` | `25000` | M5 | Approach rail/corridor length for fixture ports. |
| `DockingMaxApproachSpeedCmPerSec` | `2500` | M5 | Reject above this before assist takes over. |
| `DockingRequiredAlignmentDegrees` | `10` | M5 | Initial strictness for manual approach. |
| `SupercruiseMinSpeedCmPerSec` | `100000` | M4 | Above normal flight, below useful cruise. |
| `SupercruiseMaxSpeedCmPerSec` | `20000000` | M4 | Fast enough to cross the fixture long leg quickly. |
| `SupercruiseTargetDropoutMinRadiusCm` | `250000` | M4 | Inner edge of desired target exit band. |
| `SupercruiseTargetDropoutMaxRadiusCm` | `750000` | M4 | Outer edge of desired target exit band. |
| `GravitySlowdownRadiusCm` | `2000000` | M4 | Speed ceiling starts falling inside this radius. |
| `GravityLockoutRadiusCm` | `500000` | M4 | Supercruise engage is refused inside this radius; active supercruise may continue under slowdown. |
| `GravityDropoutRadiusCm` | `350000` | M4 | Entering this during active supercruise forces dropout. |
| `MapDistanceScaleCmPerUnit` | `1000000` | M2 | Display scale only. |
| `MapMinIconScale` | `0.75` | M2 | Display scale only. |
| `MapMaxIconScale` | `2.0` | M2 | Display scale only. |

### M1-M5 Provisional Orbit Values

| ID | Parent | SemiMajorAxisCm | PeriodSeconds | PhaseOffsetRadians | Purpose |
| --- | --- | ---: | ---: | ---: | --- |
| `ember` | `frontier_primary` | `12000000` | `900` | `0.2` | Inner body, simple gravity-well approach. |
| `brink` | `frontier_primary` | `40000000` | `1800` | `2.2` | Long supercruise leg from spawn. |
| `brink_minor` | `brink` | `3500000` | `240` | `0.7` | Nested moon frame validation. |
| `brink_watch` | `brink` | `3000000` | `90` | `1.4` | Fast moving station to expose docking/frame bugs while keeping station dropout outside Brink's gravity slowdown radius. |
| `wayfarer_depot` | `frontier_primary` | `26000000` | `1500` | `3.0` | Simpler high-orbit station case. |

`brink_watch` is deliberately fast. If docking works only when this station is static, the architecture has failed the moving-frame requirement.

The `brink_watch` orbit radius is also deliberate. With `SupercruiseTargetDropoutMaxRadiusCm = 750000` and `GravitySlowdownRadiusCm = 2000000`, the nearest possible station dropout point toward `brink` remains `2250000` cm from the planet. That keeps the station target dropout band outside Brink's slowdown, lockout, and forced-dropout radii so M4 failures identify flight logic bugs rather than fixture overlap.

### M4 Provisional Gravity Wells

These are runtime/system definitions, not body fields. `LocalOffsetCm` is zero for all fixture wells.

| WellId | AnchorId | GravityBodyId | CoordinateFrame | SlowdownRadiusCm | LockoutRadiusCm | DropoutRadiusCm | MinimumSafeDropoutRadiusCm | Strength | Priority | Flags |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `ember_well` | `ember` | `ember` | `body_relative` | `2000000` | `500000` | `350000` | `750000` | `0.55` | `10` | blocks entry, forces dropout, affects route estimates |
| `brink_well` | `brink` | `brink` | `body_relative` | `2000000` | `500000` | `350000` | `750000` | `0.75` | `20` | blocks entry, forces dropout, affects route estimates |
| `brink_minor_well` | `brink_minor` | `brink_minor` | `body_relative` | `900000` | `250000` | `175000` | `400000` | `0.35` | `15` | blocks entry, forces dropout, affects route estimates |

The route fixture refers to `brink_well` and `brink_minor_well` as exclusion/avoidance constraints. If implementation keeps separate exclusion-zone IDs, those zones must be derived from these well IDs rather than introducing different radii.

### M5 Provisional Docking Port

`brink_watch` must define at least one immutable docking-port definition:

| Field | Value |
| --- | --- |
| `PortId` | `brink_watch_port_a` |
| `ApproachTransform` | location `(0, -25000, 0)`, rotation `(0, 0, 0)` in station-local frame |
| `DockedTransform` | location `(0, -1200, 0)`, rotation `(0, 0, 0)` in station-local frame |
| `UndockTransform` | location `(0, -6000, 0)`, rotation `(0, 0, 0)` in station-local frame |
| `ActivationRangeCm` | `15000` |
| `MaxApproachSpeedCmPerSec` | `2500` |
| `RequiredAlignmentDegrees` | `10` |
| `AllowedShipClasses` | `small` |

Initial port occupancy is runtime fixture state, not part of `FDockingPortDefinition`.

### M7 Provisional Traffic Route Segment

M7 must include one concrete in-system segment so route sampling, AI prediction, and demotion recovery can be tested against moving endpoints.

`m7_brink_watch_wayfarer_trade`:

| Field | Value |
| --- | --- |
| `RouteSegmentId` | `m7_brink_watch_wayfarer_trade` |
| `SourceAnchorId` | `brink_watch` |
| `DestinationAnchorId` | `wayfarer_depot` |
| `SourceLocalOffsetCm` | `(0, -6000, 0)` |
| `DestinationLocalOffsetCm` | `(0, 8000, 0)` |
| `RoutePolicyId` | `fixture_trade_lane_basic` |
| `RouteGeometryPolicyId` | `fixture_dynamic_arc_v1` |
| `TravelModelId` | `fixture_supercruise_lane_v1` |
| `RouteProgressSemantic` | `time_fraction` |
| `AvoidanceAnchorIds` | `brink`, `brink_minor` |
| `ExclusionZoneIds` | `brink_well`, `brink_minor_well` |
| `RouteFrameBasisPolicy` | `source_to_destination` |
| `ControlData` | arc height `600000` |
| `JurisdictionId` | `frontier_local_authority` |
| `SecurityRating` | `0.45` |
| `RouteValue` | `0.65` |
| `AllowedShipClassTags` | `small`, `medium` |
| `RiskProfileId` | `fixture_mixed_patrol_pirate_risk` |
| `bSupportsPatrolCoverage` | true |
| `bSupportsPirateAmbush` | true |

`JurisdictionId`, `SecurityRating`, and `RiskProfileId` are opaque M7 fixture metadata. M7 and M8 validation may require the fields to be present and stable, but may not interpret authority, legal risk, patrol risk, pirate risk, or enforcement semantics until the M9 systemic gameplay profile owns those records. If code needs a typed value earlier, it should use a minimal stub marked opaque until M9 rather than importing M9 validation rules into M7.

`fixture_dynamic_arc_v1` is deterministic: endpoints are evaluated at the requested simulation time, the route basis is source-to-destination, the midpoint is exactly halfway between resolved endpoints, and the arc normal/height creates the single control point. Sampling count is a debug or test setting, not route data, and must not change the mathematical route, closest-progress result, derivatives, or travel-time estimate.

### M10/M11 Realized AI Fixture Slice

The M10/M11 fixture extends `frontier_test_01` with named logical actors and events so validation and automation do not rely on vague content:

| ID | Kind | Required purpose |
| --- | --- | --- |
| `trader_brink_01` | logical ship | trade route from `brink_watch` toward `wayfarer_depot` |
| `trader_brink_02` | logical ship | same route, offset schedule for actor-cap testing |
| `trader_wayfarer_01` | logical ship | reverse route toward `brink_watch` |
| `group_traders_m7_trade_lane` | traffic group | owns the three trader ships and route goal state |
| `patrol_frontier_local_01` | logical ship/group | patrols the valuable route and responds to distress |
| `pirate_raider_01` | logical ship/group | attempts ambush against route cargo |
| `event_distress_trade_lane_01` | scheduled event | distress call used by police and comms tests |
| `hazard_interdiction_trade_lane_01` | encounter hazard | pending interdiction save/reload target |
| `actor_budget_m11_low` | actor budget profile | caps realized ships low enough to prove promotion/demotion |
| `msg_distress_trade_lane_01` | message definition | distress presentation hook |
| `msg_pirate_demand_01` | message definition | pirate demand hook |
| `msg_police_scan_01` | message definition | police scan hook |

M11 validation must require these IDs, or explicitly named replacements in a fixture override, before realized AI acceptance tests run.

Fixture expectations:

- sampling at the same authoritative time is deterministic
- source and destination endpoint motion contributes to estimated velocity
- `FindClosestProgress` is valid only for ships still flagged as route-recoverable
- the route must not bake absolute Unreal-space spline points
