# Frontier Test Fixture

`frontier_test_01` is the active Unreal architecture validation system.

It is not meant to be pretty. It is meant to expose the hard problems early: stable IDs, nested orbits, moving-frame docking, supercruise distances, gravity lockout, map readability, save/load, gate arrival, traffic, systemic gameplay, encounters, combat/threat state, realized AI hooks, and non-Sol startup.

## Design Rules

- Do not use Sol, Earth, Mars, Luna, or Solar System assumptions.
- Prefer ugly readable placeholders over polished art.
- Every entity must have a stable gameplay ID.
- Every spawned actor must come from fixture data.
- Every important object must appear in debug registry output.
- The fixture is authored game content, not runtime substitute data.

## Startup Slice

Start profile `frontier_test_start`:

- `SystemId`: `frontier_test_01`
- `SpawnZoneId`: `spawn_deep_space`
- `DockedStationId`: unset
- `DockingPortId`: unset
- `ShipArchetypeId`: `wayfarer_mk1`

Targetable startup objects:

- body: `ember`
- station: `brink_watch`
- gate: `frontier_gate_a`

Spawned startup placeholders:

- body marker/proxy for `ember`
- station marker/proxy for `brink_watch`
- gate marker/proxy for `frontier_gate_a`
- player ship at `spawn_deep_space`

Additional authored IDs:

- `frontier_primary`
- `brink`
- `brink_minor`
- `wayfarer_depot`
- `cinder_field`

## Literal Startup Fixture Slice

All transforms are provisional Unreal centimeters in `system_barycentric` frame. They are not astrophysical values.

| ID | Display name | Kind | FrameType | AnchorId | Location cm | Rotation | RadiusCm | TargetType | bShowInHud | bCanTarget |
| --- | --- | --- | --- | --- | --- | --- | ---: | --- | --- | --- |
| `spawn_deep_space` | Deep Space Start | spawn_zone | `system_barycentric` | unset | `(0, 0, 0)` | `(0, 0, 0)` | `5000` | none | false | false |
| `ember` | Ember | body | `system_barycentric` | unset | `(250000, 0, 0)` | `(0, 0, 0)` | `8000` | body | true | true |
| `brink_watch` | Brink Watch | station | `system_barycentric` | unset | `(0, 350000, 0)` | `(0, 0, 0)` | `2000` | station | true | true |
| `frontier_gate_a` | Frontier Gate A | gate | `system_barycentric` | unset | `(-300000, -150000, 0)` | `(0, 0, 0)` | `3000` | gate | true | true |

Additional fields:

- `frontier_gate_a.ActivationRangeCm = 5000`
- `frontier_gate_a.DestinationSystemId = frontier_arrival_test_01`
- `frontier_gate_a.DestinationGateId = arrival_gate_a`
- `frontier_gate_a.DestinationArrivalId = arrival_from_frontier_gate_a`
- `TargetId == source object ID` for all visible startup targets
- exactly three entries should satisfy `bShowInHud && bCanTarget`

The gate destination is part of the foundational fixture. Current authored fixture data keeps the destination system, destination gate, destination arrival marker, and arrival transform populated.

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
- Player ship archetype: `wayfarer_mk1`

## Bodies

### `frontier_primary`

- Type: star
- Purpose: central light/reference source and parent for the physical fixture.

### `ember`

- Type: rocky inner planet
- Purpose: simple visible approach target.
- Features:
  - visible body target
  - gravity well
  - supercruise slowdown radius
  - no docking requirement

### `brink`

- Type: outer planet
- Purpose: parent body for moon and orbiting station.
- Features:
  - parent body for nested content
  - gravity well
  - supercruise lockout/dropout radius
  - intentionally distant from spawn so supercruise is required

### `brink_minor`

- Type: moon
- Parent: `brink`
- Purpose: nested orbit validation.

## Stations

### `brink_watch`

- Parent: `brink`
- Rule: orbiting station
- Purpose: moving-frame docking test.
- Requirements:
  - stable station target
  - orbit fast enough to reveal bad relative-frame math
  - at least one docking port
  - appears in HUD/debug target list
  - appears in system map
  - save/reload while docked restores to this live moving frame

### `wayfarer_depot`

- Rule: deep-space or high-orbit station
- Purpose: simpler station target and service placeholder.
- Requirements:
  - stable navigation target
  - usable local approach target
  - at least one docking port

## Gate

### `frontier_gate_a`

- Purpose: authored transition endpoint.
- Requirements:
  - stable gate ID
  - activation range
  - destination system, destination gate, destination arrival marker, and arrival transform
  - transition requests resolve through catalog-owned data and fail loudly if the destination cannot be resolved
  - appears in HUD target list and system map

### Gate Destination And Arrival

`frontier_gate_a` is a fully validated transition endpoint. The source system remains `frontier_test_01`; the destination system is a deliberately small arrival-only fixture, not Sol.

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
| `bBidirectional` | false |

Destination system minimum slice:

| ID | Display name | Kind | FrameType | AnchorId | Location cm | Rotation | RadiusCm | TargetType | bShowInHud | bCanTarget |
| --- | --- | --- | --- | --- | --- | --- | ---: | --- | --- | --- |
| `arrival_gate_a` | Arrival Gate A | gate | `system_barycentric` | unset | `(0, 0, 0)` | `(0, 0, 0)` | `3000` | gate | true | true |
| `arrival_from_frontier_gate_a` | Frontier Gate Arrival | arrival_marker | `gate_relative` | `arrival_gate_a` | `(0, 18000, 0)` | `(0, 180, 0)` | `4000` | none | false | false |

Validation expectations:

- catalog lookup resolves `frontier_arrival_test_01`.
- catalog route edge resolves source system, source gate, destination system, destination gate, and destination arrival marker.
- destination system build registers `arrival_gate_a` and `arrival_from_frontier_gate_a`.
- arrival transform resolves from `arrival_gate_a` plus `ArrivalLocalOffsetCm`; hard-coded actor transforms fail validation.
- invalid destination system or arrival ID fails without changing the active system.
- pending arrival save/load keeps the ship in `gate_arrival` until the arrival safety window completes.
- no transition fixture field may reference `sol`, `Earth`, `Mars`, or `Luna`.

## Zones

### `cinder_field`

- Type: asteroid/resource zone
- Purpose: mining/resource/combat hook.
- Requirements:
  - stable ID
  - map entry
  - debug visibility

### `spawn_deep_space`

- Purpose: default new-game start.
- Requirements:
  - not near a planet lockout zone
  - has visible body/station/gate targets

### `spawn_brink_watch`

- Purpose: debug/local approach start.
- Requirements:
  - close enough to test station approach
  - outside immediate docking activation range unless explicitly testing docking

## Required Test Cases

- boot directly into `frontier_test_01`
- spawn at `spawn_deep_space`
- select `ember`, `brink_watch`, and `frontier_gate_a`
- rebuild active system and preserve registry correctness
- approach `brink_watch` in normal flight
- supercruise from deep-space spawn to `brink_watch`
- dock at `brink_watch`
- save while docked, reload, undock
- transition through `frontier_gate_a` into `frontier_arrival_test_01`
- save/reload pending gate arrival
- run logical traffic, systemic, encounter, combat/threat, realized AI, and progression tests against the fixture without special-case runtime substitutions

## Scale Values

These values are provisional and intentionally not astrophysical. They exist so the foundation can be tested against concrete numbers instead of vague scale bands. They belong in fixture data/profile assets, not scattered through actors.

Hard authoring rule for the HD 219134 fixture:

- `1 AU = 1,000,000,000 Unreal centimeters`.
- Orbit radii are authored from known AU values through one conversion helper.
- Stellar and planetary radii use the same AU-derived scale. Any later readability inflation must be explicit visual-profile tuning, not mixed raw numbers.

| Field | Value | Notes |
| --- | ---: | --- |
| `NormalFlightMaxSpeedCmPerSec` | `24000` | Initial normal-flight cap. |
| `LocalBubbleRadiusCm` | `100000000` | Active Unreal-space projection radius around the player. |
| `OriginShiftThresholdCm` | `40000000` | Rebase the local bubble when the player exceeds this distance from bubble origin. |
| `StationApproachBubbleRadiusCm` | `500000` | Near-station local approach/debug radius. |
| `DockingActivationRangeCm` | `15000` | Begin docking-request eligibility. |
| `DockingCorridorLengthCm` | `25000` | Approach rail/corridor length for fixture ports. |
| `DockingMaxApproachSpeedCmPerSec` | `2500` | Reject above this before assist takes over. |
| `DockingRequiredAlignmentDegrees` | `10` | Initial strictness for manual approach. |
| `SupercruiseMinSpeedCmPerSec` | `1000000` | 10 km/s near the stellar gravity lockout, before the distance curve opens up. |
| `SupercruiseMaxSpeedCmPerSec` | `20000000` | Fast enough to cross the fixture long leg quickly. |
| `SupercruiseTargetDropoutMinRadiusCm` | `250000` | Inner edge of desired target exit band. |
| `SupercruiseTargetDropoutMaxRadiusCm` | `750000` | Outer edge of desired target exit band. |
| `GravitySlowdownRadiusCm` | authored per well | Speed ceiling starts falling inside this radius. |
| `GravityLockoutRadiusCm` | authored per well | Supercruise engage is refused inside this radius; active supercruise may continue under slowdown. |
| `GravityDropoutRadiusCm` | authored per well | Entering this during active supercruise forces dropout. |
| `MapDistanceScaleCmPerUnit` | `1000000000` | Display scale only; one map unit is one authored AU. |
| `MapMinIconScale` | `0.75` | Display scale only. |
| `MapMaxIconScale` | `2.0` | Display scale only. |

## Orbit Values

| ID | Parent | SemiMajorAxisCm | PeriodSeconds | PhaseOffsetRadians | Purpose |
| --- | --- | ---: | ---: | ---: | --- |
| `ember` | `frontier_primary` | `12000000` | `900` | `0.2` | Inner body, simple gravity-well approach. |
| `brink` | `frontier_primary` | `40000000` | `1800` | `2.2` | Long supercruise leg from spawn. |
| `brink_minor` | `brink` | `3500000` | `240` | `0.7` | Nested moon frame validation. |
| `brink_watch` | `brink` | `3000000` | `90` | `1.4` | Fast moving station to expose docking/frame bugs while keeping station dropout outside Brink's gravity slowdown radius. |
| `wayfarer_depot` | `frontier_primary` | `26000000` | `1500` | `3.0` | Simpler high-orbit station case. |

`brink_watch` is deliberately fast. If docking works only when this station is static, the architecture has failed the moving-frame requirement.
