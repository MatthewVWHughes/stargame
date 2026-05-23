# Starlight Recreation Reference

This is a concrete preservation capture for the Godot `starlight` project. Its
goal is pragmatic: if the Godot repo disappeared, this document plus the current
Stargame docs should be enough to recreate the Godot version closely in shape,
content, and behavior, even if the implementation is Unreal-native.

This is not the Unreal design authority. The Unreal docs remain canonical. Treat
these values as Starlight reference facts, prototype balance, source fixtures,
and discard/migration evidence.

## Source Map

The Godot version is spread across these high-value sources:

- Runtime and flight: `game/scripts/ship/ShipController.cs`,
  `game/scripts/game/space/GameplayRoot*.cs`.
- Systems and orbits: `game/scripts/game/space/systems/*`,
  `game/scripts/orbital/*`, `game/data/systems/*.json`.
- Stations, services, missions, inventory, economy:
  `game/scripts/game/stations/*`, `game/scripts/game/missions/*`,
  `game/scripts/game/inventory/*`, `game/scripts/game/runtime/*`.
- Combat and NPCs: `game/scripts/game/combat/*`,
  `game/scripts/npc/*`, `game/scripts/station_interior/*`.
- UI: `game/scripts/game/ui/*`.
- Scene fixtures: `game/scenes/game/space/*`,
  `game/scenes/game/station/*`, `game/scenes/game/ships/*`.
- Tests: `game/tests/*.cs`.
- Legacy design docs: `docs/roadmap/*`, `docs/reference/*`,
  `docs/vertical-slices/*`, `docs/design/*`, `station design docs/*`.

Key Starlight artifacts are archived in this repo under
`docs/archive/starlight/`:

- `data/`: exact Sol, Alpha Centauri, faction style, ship prompt, and station
  layout JSON fixtures.
- `source-scripts/`: copied Godot scripts, including flight, runtime, missions,
  stations, UI, NPC, combat, and station-interior prototypes.
- `tests/`: copied Godot C# tests.
- `scenes-game/`: copied Godot game scene fixtures.
- `roadmap/`, `reference/`, `station-design-docs/`, and `design/`: selected
  Starlight design docs and pipeline specs.

When this document summarizes a catalog or long data fixture, the archived files
are the reconstruction source of record.

## Content Rules

Starlight content used lowercase string gameplay IDs. JSON family schema
versions are explicit and should fail loudly on mismatches. Dynamic ships may
use `Guid`; authored gameplay records should not rely on actor names as IDs.

JSON owns gameplay/state/tuning. Scenes and assets own hierarchy, geometry,
markers, collision, bay anchors, weapon anchors, engine mounts, and cameras. Do
not duplicate a single fact across both layers unless one is deliberately a
runtime cache.

The Godot star-system loader accepts snake_case JSON, comments, trailing commas,
numeric strings, colors as hex or objects, and vectors as `[x,y,z]`, `[uniform]`,
or `{x,y,z}`. Star system definitions contain system metadata, bodies, orbit
records, sphere visuals, scene visuals, rings, asteroid belts, and jump gates.

## Inputs And UI Strings

Space input bindings:

- `W/S`: throttle.
- `A/D`: strafe.
- `Ctrl`: vertical down.
- `Q/E`: roll.
- `Z` or `X`: engine kill.
- `Space`: cursor steering toggle.
- `J`: supercruise toggle.
- `R`: phase scrambler.
- `M`: system map.
- `C`: comms.
- `Tab`: target cycle.
- `L`: jump gate.
- `F2`: debug overlay.
- Left mouse: select contact in space.
- Right mouse: fire. The main menu text says left mouse fires; treat that as a
  known contradiction and prefer the runtime code.

HUD mode labels:

- `FLIGHT`
- `ENGINE KILL`
- `SUPERCRUISE SPOOL N%`
- `SUPERCRUISE`
- `AUTOPILOT`

HUD alert strings include `PHASE DRIVE OFFLINE`, `PHASE INTERDICTION LOCK`, and
`SUPERCRUISE SPOOLING`. HUD labels include `SPEED`, `THROTTLE`, `PRIMARY`, and
`HULL/SHD/WPN`. HUD scale is `clamp(viewportY / 1440, 0.82, 1.35)`.

Comms shows a docking request only when `canDock` is true, otherwise the request
button is disabled and distance is displayed with `F0` formatting. Dialog menus
are modal `CanvasLayer`s with `ProcessMode=Always`, generated buttons with
minimum height `40`, and dimmer alpha `0.45`.

## Flight And Travel

Godot flight states are:

- `Normal`
- `SupercruiseSpool`
- `Supercruise`
- `Autopilot`

Prototype flight tuning:

| Value | Godot value |
| --- | --- |
| Thrust | `120` |
| Strafe | `42` |
| Normal max speed | `60` |
| Supercruise entry | `180` |
| Supercruise spool | `5s` |
| Supercruise recovery | `30s` |
| Near-star max speed | `2000` |
| Deep-space max speed | `20000` |
| Scale distance | `3000000` |
| Supercruise acceleration/deceleration | `800 / 3000` |
| Well radius multiplier | `10` |
| Supercruise turn factor | `0.25` |
| Supercruise SOI multiplier | `3` |
| Interdiction decay/hold | `0.9/s` / `0.2s` |
| Phase scrambler duration/cooldown | `3s` / `25s` |
| Phase scrambler lock decay | `2.8/s` |
| Steering/roll force | `13 / 13` |
| Angular drag | `6.5` |
| Dead zone | `0.05` |
| Normal SOI multiplier | `5` |
| Passive gravity min/max accel | `0.1 / 0.9` |
| Autopilot accel/brake/turn | `40 / 35 / 1.4` |
| Throttle rise/fall | `1.45 / 2.2` |
| Zero-throttle brake | `34` |
| Frame host hysteresis factor | `1.2` |
| Station assist blend in/out | `2.2 / 4.5` |
| Station assist relative/cutoff speed | `10 / 20` |
| Station assist throttle zero point | `0.8` |
| Station assist match accel | `18` |

Supercruise speed limit is two-stage:

```text
starCeiling = lerp(NearStarMax, DeepSpaceMax, sqrt(starDist / ScaleDistance))
wellLimit = NormalMaxSpeed + (starCeiling - NormalMaxSpeed)
    * sqrt(surfaceDist / (well.Radius * WellRadiusMultiplier))
```

Entering supercruise near a gravity well is blocked when:

```text
dist < well.Radius * SupercruiseSoiMultiplier
```

If the ship enters that lockout while spooling or already in supercruise, it is
forced back out. Spool completion starts supercruise at
`max(entry * 3, normalMax)`, so default initial cruise speed is `540 u/s`.

Origin shifting triggers at `20000` Godot units. The shift subtracts the ship
position from the star/system root, zeros the ship position, and applies the
same offset to hostiles, projectiles, and camera samples.

Cursor steering uses mouse offset from the viewport center divided by the
shorter half-axis, clamped to the unit circle, with per-axis deadzone. Steering
adds angular acceleration to yaw and pitch; `Q/E` roll adds roll acceleration.
Angular velocity is damped by angular drag every tick, then applied locally.

Autopilot rotates toward the live target with quaternion slerp capped by
`AutopilotMaxTurnRate`, computes braking speed from
`sqrt(2 * AutopilotBrakeAccel * brakeDistance)`, moves velocity toward the
target velocity with `AutopilotAccel`, and still translates while the visual turn
catches up.

Station assist selects the nearest assist body inside its radius, samples that
body's velocity relative to the current frame host, computes relative speed
against ship local velocity, blends influence in/out, and matches station-local
velocity only in normal flight. Large station deltas over `10000` squared units
are ignored as teleports.

## Docking, Launch, And Gates

Docking constants:

| Value | Godot value |
| --- | --- |
| Comms range | `900` |
| Approach speed | `60` |
| Approach arrive radius | `8` |
| Final-leg handoff distance | `12` |
| Docking rail duration | `2.5s` |
| Undock coast duration | `3s` |

Reusable station marker offsets in `BasicStation.tscn`:

- `DockApproach`: `(0, 0, 120)`
- `DockFinal`: `(0, 0, 70)`
- `LaunchPoint`: `(0, 0, 150)`

The final docking rail interpolates in the `DockFinal` local frame with
smoothstep `t*t*(3-2*t)`, so station orbital motion is carried through the
marker transform. Docking is blocked or aborted if hostiles are near the ship or
station, except for hostile boarding stations.

Gate runtime fields are `GateId`, `DestinationSystemId`, `ArrivalGateId`,
`DisplayName`, and `ActivationRange`; default activation range is `450`. Gate
nodes register into group `jump_gates`.

Gate arrival waits three physics frames, finds `PendingArrivalGateId`, and
places the ship at:

```text
gatePosition - gateForward * 360 + Up * 8
```

It then resets velocity, angular velocity, and flight mode.

Lagrange anchors are simple radial offsets:

```text
L2 = planet + normalized(planet - star) * distance
L1 = planet - normalized(planet - star) * distance
```

Coincident star/planet positions return false.

## Systems And Maps

Only two system IDs were known by the Starlight catalog:

- `sol`
- `alpha_centauri`

Unknown IDs fall back to `sol` in Godot. That fallback is legacy behavior, not a
Stargame rule.

The full authored system catalogs are archived as:

- `docs/archive/starlight/data/sol.json`
- `docs/archive/starlight/data/alpha_centauri.json`

Those JSON files preserve the complete body trees, orbit values, station orbit
nodes, gameplay radii, visual node names, texture paths, rings, asteroid belt
entries, and nested moon layouts. Use the archived fixtures rather than this
summary when recreating the actual Sol or Alpha Centauri layout.

Sol gate:

- Gate ID: `sol_gate_earth_l2`
- Display: `Alpha Centauri Transit Gate`
- Destination system: `alpha_centauri`
- Arrival gate: `alpha_gate_primary`
- Anchor: `Star/Earth`
- Distance: `5200`
- Node: `EarthL2Gate`

Alpha Centauri gate:

- Gate ID: `alpha_gate_primary`
- Display: `Sol Transit Gate`
- Destination system: `sol`
- Arrival gate: `sol_gate_earth_l2`
- Anchor: `Star/AlphaCentauriA/Theseus`
- Parent: `Star/AlphaCentauriA`
- Distance: `1400`
- Node: `TheseusL2Gate`

Orbit formula:

```text
period = max(period, 0.1)
theta = elapsed * Tau / period + phaseOffset
radius = a * (1 - e^2) / (1 + e * cos(theta))
```

Inclination maps orbital `z` into world `Y/Z`.

System map behavior:

- Zoom bounds: `0.1` to `20`.
- Wheel multiplier: `1.3`.
- Right drag pans.
- Left-click target radius: `12 px` for stations, `18 px` for planets.
- Projection: `sqrt(dist / MaxOrbit) * MaxScreenR * Zoom`.
- `MaxScreenR = min(viewport) * 0.425`.
- Radar bubble logical range: `100000`, visually forced to `80 * zoom` pixels.

Sector map behavior:

- `Ly = 2`
- `ShortRange = 5 ly`
- `MaxRange = 8 ly`
- Links are generated by Prim MST for connectivity, with extra short-range
  links, plus a natural shortcut between `Wolf 359` and `Ross 154`.

Planet overlay behavior:

- Distant body dots bypass far-plane/frustum issues.
- Dots hide when the real mesh is visible within camera far plane and larger
  than `2 px`.
- Star dot uses radius `2000`, min `10 px`, emission `4`.

## Save, Session, And Runtime State

Production save path: `user://vs1_save.json`. Tests swap in an
`InMemorySaveStore`.

Save version: `5`.

Saved fields:

- `docked_station_id`
- credits
- cargo capacity
- hull, `max_hull`, shields, and `max_shields`
- cargo map
- station stock map
- player equipped items
- player inventory
- ship equipped items
- mission progress

Load rejects:

- null data
- wrong version
- unknown station
- negative credits
- negative cargo capacity
- negative hull or shields
- negative `max_shields`
- max hull `<= 0`
- corrupt JSON

Equipment deserialization drops invalid slots, unknown item IDs, and items that
do not fit the parsed slot. Stack counts clamp to max stack or `1` for
non-stackable items. Inventory deserialization splits oversized stacks, drops
unknown/empty entries, and truncates at the player inventory slot count.

Static session handoff state:

- `PendingEntryMode`: `NewGame`, `Continue`, `LaunchFromStation`
- `CurrentStationId`
- `PendingStatusMessage`
- `CurrentSystemId`
- `PendingArrivalGateId`

`JumpToSystem(destinationSystemId, arrivalGateId, statusMessage)` only mutates
session state; scene reload consumes it.

## Player, Inventory, And Economy

Player defaults:

| Field | Value |
| --- | --- |
| Credits | `1000` |
| Cargo capacity | `40` |
| Hull/max hull | `100 / 100` |
| Shields/max shields | `100 / 100` |
| Weapon energy/max | `100 / 100` |
| On-foot health/max | `100 / 100` |
| Personal inventory slots | `20` |

Default ship equipment:

- `ShipWeaponHardpoint1`: `pulse_laser_mk1`
- `ShipShield`: `shield_basic`
- `ShipEngine`: `engine_basic`
- `ShipThrusters`: `thrusters_basic`

Item IDs:

| ID | Display | Category | Stack |
| --- | --- | --- | --- |
| `food` | Food | Commodity | `999` |
| `water` | Water | Commodity | `999` |
| `iron` | Iron | Commodity | `999` |
| `synthetic_parts` | Synthetic Parts | Commodity | `999` |
| `pistol_basic` | Basic Pistol | Player sidearm | `1` |
| `rifle_basic` | Basic Rifle | Player weapon | `1` |
| `medkit` | Medkit | Consumable | `10` |
| `pulse_laser_mk1` | Pulse Laser Mk1 | Ship weapon | `1` |
| `shield_basic` | Basic Shield | Ship shield | `1` |
| `engine_basic` | Basic Engine | Ship engine | `1` |
| `thrusters_basic` | Basic Thrusters | Ship thrusters | `1` |
| `countermeasures_basic` | Basic Countermeasures | Ship countermeasures | `1` |

Equipment slots:

- `PlayerPrimaryWeapon`
- `PlayerSecondarySidearm`
- `ShipWeaponHardpoint1`
- `ShipWeaponHardpoint2`
- `ShipShield`
- `ShipEngine`
- `ShipThrusters`
- `ShipCountermeasures`

Cargo capacity uses a `+0.001f` tolerance. Removing cargo below `0.001f`
deletes the stack. Buy failure after a credit debit refunds credits. Station
trade buttons transact exactly `1` unit.

Commodity base prices:

- `food`: `10`
- `water`: `6`
- `iron`: `14`
- `synthetic_parts`: `40`

Price formula:

```text
unitPrice = basePrice * (2.0 - fillRatio * 1.5)
```

This gives `2x` base at empty and `0.5x` base at full.

Stock pressure labels:

- `scarce`: `<= 0.18`
- `tight`: `<= 0.42`
- `steady`: otherwise
- `surplus`: `>= 0.62`
- `glut`: `>= 0.82`

Station stock caps/defaults:

| Station | Food | Water | Iron | Synthetic parts |
| --- | --- | --- | --- | --- |
| EarthHub | `220 / 200` | `180 / 140` | `100 / 25` | `80 / 70` |
| ArmstrongStation | `120 / 30` | `140 / 40` | `80 / 55` | `60 / 50` |
| MarsColony | `220 / 55` | `180 / 120` | `120 / 90` | `60 / 20` |
| CeresPort | `100 / 20` | `80 / 20` | `260 / 230` | `40 / 10` |

Commodity presentation:

- `food`: display `Food`, export hint `agri surplus`, import hint `rations`.
- `water`: display `Water`, export hint `processed water`, import hint
  `reserve water`.
- `iron`: display `Iron`, export hint `ore and feedstock`, import hint `ore`.
- `synthetic_parts`: display `Synthetic Parts`, export hint
  `fabrication parts`, import hint `replacement parts`.

Station market summaries:

- EarthHub: High-volume inner-system exchange with broad consumer stock and
  steady fabrication demand. Exports `food`, `synthetic_parts`; imports `iron`.
- ArmstrongStation: Lunar transfer post with tight life-support reserves and
  modest industrial throughput. Exports `synthetic_parts`; imports `water`,
  `food`.
- MarsColony: Growing industrial colony that leans on imported parts while
  moving processed materials. Exports `iron`; imports `synthetic_parts`,
  `food`.
- CeresPort: Frontier ore port with deep raw stock and fragile support
  logistics. Exports `iron`; imports `water`, `food`, `synthetic_parts`.
- DerelictOutpost: No active civilian market.

## Stations, Services, And Missions

Godot station scene kinds:

- `CivilianHub`
- `HostileBoarding`

Station service modes:

- `None`
- `Trade`
- `Repair`
- `Launch`

Repair cost:

```text
ceil(hullMissing * 2 + shieldMissing * 1.2)
```

Repair restores hull, shields, and weapon energy, then saves docked state.
Launch falls back to `EarthHub` if no session station is set. Service windows
lock FPS input.

Physical station stub:

- Room: `18 x 18`.
- Walls: `18 x 5 x 0.3`.
- Trader: `(-3.5, 1, -1)`.
- Repair: `(3.5, 1, -1)`.
- Airlock marker: `(-0.738, 1.248, 16.1)`.
- Launch label: `(0, 1.9, 18)`.
- Interactable collision layer: `4`.
- Interactable collision mask: `0`.
- Default interact range: `2.75`.
- NPC talk range: `3.0`.
- Action IDs: `trade`, `repair`, `launch`, and `talk:<npc_id>`.
- Interactable group: `vs1_station_interactable`.

Notable quest givers:

- `sam_kessler`: EarthHub at `(0, 0, -3.5)`.
- `mira_chen`: ArmstrongStation at `(2.6, 0, -3.2)`.
- `rhea_voss`: MarsColony at `(-2.8, 0, -2.8)`.
- `tomas_vale`: CeresPort at `(-2.8, 0, -2.8)`.

Station faction catalog:

| ID | Display | Short | Description |
| --- | --- | --- | --- |
| `uce_commonwealth` | United Commonwealth of Earth | UCE | Inner-system sovereign authority centered on Earth. |
| `uce_navy` | UCE Navy | UCE Navy | Fleet arm responsible for deep-system security, interdiction, and strategic response. |
| `lunar_transit_cooperative` | Lunar Transit Cooperative | LTC | Civilian transfer and support operator across Earth-Luna traffic. |
| `mars_heavy_industry` | Mars Heavy Industry | MHI | Major shipbuilding and heavy fabrication bloc operating across the Martian sphere. |
| `belt_extraction_cooperative` | Belt Extraction Cooperative | BEC | Mining and ore-routing consortium spanning Ceres and the inner belt. |
| `independent_raiders` | Independent Raiders | Raiders | Decentralized hostile groups preying on weakly defended traffic and abandoned facilities. |

Station catalog:

| Station | Parent/region | Owner | Role | Security | Market | Tags |
| --- | --- | --- | --- | --- | --- | --- |
| EarthHub | Earth, Earth orbital core | `uce_commonwealth` | Administrative and logistics hub | High security civilian port | `earth_exchange` | `enforcement`, `courier`, `investigation` |
| ArmstrongStation | Luna, Earth-Luna transfer corridor | `lunar_transit_cooperative` | Transfer, refuel, and support station | Moderate civilian traffic control | `lunar_transfer` | `relief`, `rush cargo`, `transit support` |
| MarsColony | Mars, Martian industrial sphere | `mars_heavy_industry` | Shipyard and industrial colony port | Corporate-secured industrial zone | `martian_industry` | `procurement`, `prototype recovery`, `industrial escort` |
| CeresPort | Ceres, Main asteroid belt | `belt_extraction_cooperative` | Ore port and frontier brokerage station | Low-to-moderate frontier security | `belt_frontier` | `convoy defense`, `salvage`, `recovery`, `belt security` |
| DerelictOutpost | Earth, Earth L-zone fringe | `independent_raiders` | Captured research and cargo platform | Hostile contested space | `none` | `boarding`, `clearance`, `manifest recovery` |

Mission statuses:

- `NotOffered`
- `Offered`
- `InProgress`
- `ReadyToTurnIn`
- `Completed`

Mission lifecycle:

- `Accept` only works from `Offered`.
- `AdvanceObjective` requires the expected objective ID.
- Advancing past the last objective sets `ReadyToTurnIn`.
- Cargo delivery removes cargo and reports missing cargo with display names.

Mission catalog:

| Mission | Title | Giver | Issuer/region | Objectives | Requirements | Reward |
| --- | --- | --- | --- | --- | --- | --- |
| `clear_derelict_outpost` | Clear the Derelict Outpost | `sam_kessler` at EarthHub | `uce_commonwealth`, Earth orbital core | `fly_to_derelict`, `clear_hostiles`, `return_to_giver` | none | `500` |
| `martian_feedstock_contract` | Martian Feedstock Contract | `rhea_voss` at MarsColony | `mars_heavy_industry`, Martian industrial sphere | `deliver_feedstock`, `return_to_giver`; deliver `16 iron` to EarthHub | tags `procurement`, `industrial escort` | `700` |
| `relief_run_to_ceres` | Relief Run to Ceres | `mira_chen` at ArmstrongStation | `lunar_transit_cooperative`, Earth-Luna transfer corridor | `deliver_relief_cargo`, `return_to_giver`; deliver `12 water` to CeresPort | none | `650` |
| `secure_ceres_approach` | Secure Ceres Approach | `tomas_vale` at CeresPort | `belt_extraction_cooperative`, Main asteroid belt | `clear_ceres_raiders`, `return_to_giver`; encounter `ceres_approach_sweep`, wing `4`, activation `2200`, spawn radius `520-880` | completed `relief_run_to_ceres`; tags `convoy defense`, `belt security` | `750` |
| `recover_outpost_manifest` | Recover the Outpost Manifest | `tomas_vale` at CeresPort | `belt_extraction_cooperative`, Main asteroid belt | `board_outpost`, `secure_manifest`, `return_to_giver` | completed `secure_ceres_approach`; tags `salvage`, `recovery`, `belt security` | `900` |

Exact mission summaries, objective text, and offer/accepted/in-progress/turn-in
dialog are preserved in the archived script
`docs/archive/starlight/source-scripts/game/missions/MissionCatalog.cs`.

Dialog implementation is not real Ink yet. `MissionDialogService` mimics an Ink
shape but `LoadStory()` returns false, `HasChoices` is false, `Continue()`
returns the current/empty line, and variables are null/no-op. Station dialog
falls back to `MissionDefinition` text.

Pause/inventory UI:

- `I` opens/closes the pause menu on the Inventory tab.
- `Esc` also toggles the pause menu on the Inventory tab.
- Opening stores prior mouse mode, shows the cursor, pauses the tree, and
  refreshes all tabs.
- Closing hides the root, unpauses the tree, and restores prior mouse mode.
- Tabs are `Inventory`, `Missions`, and `Settings`.
- Inventory sections are player equipped, player inventory grid, ship equipped,
  and ship cargo.
- Player equipped labels: `Primary Weapon`, `Sidearm`.
- Ship equipped labels: `Weapon 1`, `Weapon 2`, `Shield`, `Engine`,
  `Thrusters`, `Countermeasures`.
- Player inventory renders `20` item cards.
- Cargo header is `Cargo Hold  used / cap`; empty cargo shows `(empty)`.
- Mission cards show title, status, current objective text, and
  `Reward: N credits` or `Reward: none listed`.
- Main menu button changes to `res://scenes/game/MainMenu.tscn` with fade text
  `Returning to menu...`.

## NPC Traffic

Local traffic manifest:

- RNG seed: `4107`.
- Logical ships: `72`.
- Spawn/despawn: `85000 / 120000`.
- Max spawned visuals: `48`.
- Speeds: local `48`, patrol `55`, military `60`, supercruise `900`.
- Endpoint hold: `4s` plus jitter.

NPC station indices:

- `0`: Earth Hub.
- `1`: Derelict Outpost.
- `2`: Armstrong Station.
- `3`: Mars Colony.
- `4`: Ceres Port.

Traffic roles:

- `Civilian`
- `Convoy`
- `Military`
- `Patrol`

Visual archetypes include `Fighter`, `Interceptor`, `Bomber`, `Shuttle`,
`UtilityTug`, `LuxuryYacht`, `SurveyVessel`, `RepairTender`,
`LightFreighter`, `MediumHauler`, `PassengerLiner`, `ContainerFreighter`,
`HeavyTrader`, `MiningBarge`, `FuelTanker`, `ColonyTransport`, `Corvette`,
`Destroyer`, `Frigate`, `Cruiser`, and `Battleship`.

Authored convoy and formation seeds:

- Container/medium/light freighter convoy at progress `0.10`.
- Fuel tanker/utility tug convoy at `0.48`.
- Heavy trader/mining barge/corvette convoy at `0.28`.
- Passenger liner/shuttle convoy at `0.68`.
- Military formations on authored routes `6`, `7`, and `4`.

Authored traffic routes:

| Start -> end | Role | Lockout | Start offset | End offset | Speed |
| --- | --- | --- | --- | --- | --- |
| `0 -> 2` | Convoy | true | `(0, 180, -1500)` | `(0, 140, 1100)` | local |
| `0 -> 1` | Civilian | true | `(1450, 160, 280)` | `(-1050, 130, 840)` | local |
| `2 -> 0` | Civilian | true | `(860, 120, -720)` | `(-1160, 150, 680)` | local |
| `0 -> 0` | Patrol | true | `(-3800, 420, -2300)` | `(4200, 360, 2500)` | patrol |
| `0 -> 0` | Patrol | true | `(3600, -260, -3400)` | `(-3400, -180, 3600)` | patrol |
| `2 -> 2` | Patrol | true | `(-1900, 240, -1300)` | `(2100, 220, 1380)` | patrol |
| `0 -> 1` | Military | true | `(-5200, 760, -3200)` | `(3600, 500, 2400)` | military |
| `0 -> 2` | Military | true | `(4600, 520, 3000)` | `(-3200, 420, -2200)` | military |
| `0 -> 3` | Convoy | false | `(-1800, 240, 720)` | `(1300, 180, -950)` | supercruise |
| `2 -> 4` | Convoy | false | `(1300, 180, -980)` | `(-1300, 120, 980)` | supercruise |

Movement formulas:

```text
planetaryProgress = t * t * (3 - 2 * t)
```

Cruise routes reserve the first and last `18%` for approach and departure; the
middle `64%` maps linearly from `0.16` to `0.84`.

Visual speed factors:

- Battleship `0.22`
- Cruiser `0.32`
- Destroyer `0.48`
- Frigate `0.60`
- Corvette `0.78`
- Heavy trader/fuel tanker/mining barge/colony transport `0.42`
- Container freighter/passenger liner `0.54`
- Medium hauler/repair tender `0.68`
- Light freighter/utility tug/survey vessel `0.82`
- Fighter/interceptor `1.18`
- Bomber `0.92`

Distant sectors are pure timer data. Defaults: travel `60-600s`, dock
`10-60s`, pick a new route on departure, flip direction. Tests assert
zero-delta no arrivals, all ships accounted for after tick, and a `1000s` tick
produces arrivals.

## Space Combat

Player weapon constants:

| Value | Godot value |
| --- | --- |
| Damage | `14` |
| Cooldown | `0.18s` |
| Projectile speed | `760` |
| Max distance | `1200` |
| Energy cost | `9` |
| Weapon energy regen | `30/s` |
| Shield regen | `7/s` |
| Shield regen delay | `2.75s` |
| Minimum muzzle alignment | `0.955` |

Hostile ship states:

- `Patrol`
- `Intercept`
- `Interdict`
- `Engage`
- `Disengage`
- `ReturnToAnchor`

Hostile ship tuning:

| Value | Godot value |
| --- | --- |
| Detection range | `1500` |
| Fire range | `560` |
| Preferred range | `260-440` |
| Breakoff range | `170` |
| Reengage range | `520` |
| Leash range | `1700` |
| Speeds patrol/engage/disengage/return | `60 / 118 / 140 / 100` |
| Hull/shields | `90 / 55` |
| Shield regen | `6/s` after `2.5s` |
| Weapon cooldown | `0.35s` |
| Weapon damage | `12` |
| Projectile speed/max distance | `700 / 900` |
| Weapon energy cost/regen/max | `14 / 28/s / 100` |

Interdiction tuning:

- Acquire range: `3000`.
- Collapse range: `850`.
- Offline duration: `30s`.
- Lock build: `1.9/s`.
- Angular tolerance: `2.4`.
- Energy drain: `15/s`.
- Chase speed: `180`.

Transition rules:

- `Patrol` enters `Intercept` if the player is in supercruise/spool within
  engagement range.
- `Intercept` enters `Interdict` at `0.96 * acquireRange` and angle quality
  `>= 0.38`.
- `Interdict` drops if range `> 1.12 * acquireRange` or bad angle `< 0.3` for
  `0.8s`.
- `Engage` breaks off after bad range/angle for `0.55s`.

Steering behavior:

- Patrol circles anchor at radius `180`.
- Engage strafes every `1.4-2.8s`.
- Lateral weight: `0.38`.
- Back off at close range with `-0.7` forward.
- Anchor bias `0.65` near `0.78 * leashRange`.

Interdiction lock quality:

```text
lockQuality = alignmentFactor * rangeFactor * crossingFactor
```

Minimum lock contribution clamps to `0.22`. Phase scrambler reduces lock delta
to `15%`.

Phase scrambler also immediately reduces current lock by `0.4`, lasts `3s`,
has a `25s` cooldown, and uses stronger lock decay of `2.8/s`.
Interdiction signal hold is `0.2s`; normal lock decay is `0.9/s`.

Combat spawn defaults:

- Wing size: `4`.
- Activation range: `3200`.
- Respawn: `18s`.
- Spawn radius: `260-640`.
- Per-frame spawn loop safety cap: `8`.

Authored free-roam combat spawn anchors:

- Earth Watch: parent `Star/Earth`, local `(3400, 0, -2500)`.
- Mars Drift: parent `Star/Mars`, local `(-3100, 0, 1800)`.
- Ceres Fringe: parent `Star/Ceres`, local `(2400, 0, -1800)`.

Projectile contract:

- Swept segment-vs-sphere collision.
- Same-faction targets are skipped.
- Projectile collision radius `1.5` is added to combatant radius.
- Earliest hit `t` wins.
- Travel is `(direction * Speed + inheritedVelocity) * dt`.
- Bolt visual: radius `0.18`, length `3.2`.
- Beam visual: radius `0.12`, length `8.5`.
- Emissive multiplier: `1.8`.

Lead solver uses quadratic relative motion and returns false when there is no
positive finite intercept. Tests cover stationary targets, crossing targets,
fleeing target faster than projectile, head-on approach, moving shooter, and
degenerate no-crash behavior.

## On-Foot And Station Combat

VS1 station hostile values:

- Health `45`.
- Damage `8`.
- Detection range `35m`.
- Fire range `28m`.
- Cooldown `1.3s`.
- Inaccuracy `0.04 rad`.
- LOS ray starts at `+1.2m`.
- Fire ray length is `FireRange * 1.2`.

Player sidearm:

- Damage `20`.
- Range `60m`.
- Cooldown `0.35s`.
- Model path `res://assets/weapons/ScannDMR/ScannDMR.glb`.
- Local position `(0.28, -0.22, -0.45)`.
- Muzzle `(0, 0, -0.6)`.

Hostile station scene fixture:

- Room `30 x 30m`.
- Walls `5m` high.
- Four cover blocks, each `2.4 x 1.3 x 0.8`.
- Player at `(0, 1.1, 21.6)`.
- Hostile spawn markers:
  - `(-5, 0, -6)`
  - `(5, 0, -6)`
  - `(0, 0, -12)`
  - `(-10, 0, -10)`

Hostile station loop:

- Spawn player, weapon, and hostiles.
- Advance boarding entry.
- Block launch until all hostiles are dead.
- Disable weapon on clear.
- Advance objective `clear_hostiles`.
- On player down, charge `min(150, credits)`, reset health/hull, and dock at
  `EarthHub`.

Advanced FPS AI prototype values:

- Walk/run `2.5 / 4.5`.
- Sight `25m`, cone `80deg`.
- HP `50`.
- Damage `8`.
- Fire interval `1.5`.
- Accuracy `0.1`.
- Peek/hide `1.5 / 3.0`.
- Flee threshold `0.3`.
- Preferred distance `10`.

Advanced FPS states:

- `Idle`
- `Patrol`
- `Hunting`
- `Engage`
- `Attack`
- `Retreat`
- `Dead`

Patrol waits `PatrolIdleTime + random(0-2)`. Stuck timeout is `6s`. Hunting
scans for `5s`. Attack hides `0.5s` before the first peek.

Cover scoring:

- Reject if no environment block or no peek LOS.
- Self distance weight `0.3` over `25m`.
- Threat sweet spot `8-15m` adds `0.15`.
- Threat distance `<5m` subtracts `0.2`.
- Ally spacing `<3m` subtracts `0.4`; `>6m` adds `0.1`.
- Flank angle adds up to `0.15`.
- Height bonus `0.1`.

Cover point semantics:

- Valid cone `60deg`.
- Low cover `<1.2m`; high cover `>1.6m`.
- Peek offset `0.7m`.
- Auto peek checks lateral `0.8m` and forward `2m`.
- Low-cover overpeek uses `CoverHeight + 0.3`.

Squad coordinator:

- Runs at `1Hz`.
- Max attack tokens `3`.
- Alert range `15m`.
- Spot range `20m`.
- Confidence is `member count * average HP fraction`.
- Low confidence is `<0.3`.

Prototype FPS weapon:

- Damage `15`.
- Range `50`.
- Fire rate `4/s`.
- Magazine size `12`.
- Starting magazines `5`.
- Reload `1.5s`.
- Hold-`R` radial threshold `0.3s`.
- Gunshot noise radius `30`.

## Asset, Visual, And Station Kit Rules

Ship marker conventions:

- `Hp_Gun_01..`
- `Hp_Missile_01..`
- Optional `Hp_Turret_01`
- `EngineMount_L/R`
- `ShieldCenter`
- `AudioCenter`
- `Cockpit`

`ShipArchetype.ModelPath` points to a `.tscn`, not raw `.glb`.

Ship visual scale targets:

- Light fighter: about `12m`, `3-5k` tris.
- Heavy fighter: about `20m`, `5-8k` tris.
- Freighter: `35-60m`, `10-15k` tris.
- Gunboat: `50-70m`, `10-15k` tris.
- One `mat_hull`.
- 2K BaseColor/Normal/ORM atlas.
- Convex `CollisionHull`.

Faction style IDs:

- `uce_navy`
- `trappist_federation`
- `civilian`
- `pirates_generic`

Faction visual direction:

- UCE: long horizontal hulls, slab armor, stepped superstructure, wedge or
  spearhead bows, recessed hangars, symmetrical naval-industrial silhouettes,
  naval white, graphite gray, dark blue-black, subtle blue lights.
- TRAPPIST: compact armored cores, external mission modules, vertical radiator
  fins, reinforced trusses, replaceable pods, pale ceramic gray, burnt titanium,
  muted red-orange, amber lights.
- Civilian: function-first silhouettes, visible cargo modules,
  spine-and-container freighters, tankers, industrial white, faded yellow,
  safety orange, bare metal.
- Pirates: modified civilian hulls, welded armor, asymmetric repairs, external
  weapons, oversized engines, dark primer, bare metal, rust, red hazard lights.

Placeholder ship roles include fighter `18m`, bomber `32m`, support craft
`35m`, small trader `50m`, corvette `100m`, passenger liner `120m`, large trader
`150-220m`, gas hauler `220m`, destroyer `300m`, cruiser `600m`, battleship
`1000m`, pirate raider `25-70m`, and pirate frigate `180m`.

UCE battleship reference:

- `1000m` class.
- Forward `-Z`, up `+Y`.
- Hardpoints: `Hp_Gun_01..08`, `Hp_Missile_01..04`, `Hp_Turret_01..06`.
- Named modules include `CitadelSpine`, `PortMissionSponson`,
  `AftDriveRaft`, and `RadiatorWing_*`.
- V2 brief avoids wedge/tube/rectangle/glass bridge/turrets in the blockout.

Interior station kit:

- Grid cell: `4m`.
- Ceiling: `3m`.
- Walls: `0.5m`.
- Floor/ceiling slab: `0.2m`.
- Door opening: `2.0m x 2.5m`.
- Window: `1.5m x 1.0m`, sill `1.2m`.
- Blender: X east, Y north, Z up.
- Godot: +Y up, -Z forward.

Interior piece IDs:

- Layout IDs are lowercase: `corridor_1x1`, `corridor_1x2`,
  `room_2x2`, `room_2x3`, `room_3x3`, `stairwell_1x2`,
  `airlock_1x1`.
- Capitalized names such as `Corridor_1x1` and `Room_3x3` are asset/node names,
  not layout IDs.

Expected child names include `Wall_*`, `Structure_Corner_*`,
`Marker_Waypoint_##`, `Marker_CoverPoint_##`, `Marker_Interaction_##`,
`Marker_Spawn_##`, prop markers, and `Light_##`.

Wall slots:

- Slot width `3m`.
- Slot height `3m`.
- Slot thickness `0.5m`.
- Variants: `Wall_Solid.glb`, `Wall_Door.glb`, `Wall_Window.glb`,
  `Wall_WideDoor.glb`.
- Rotation-index remap tables are fragile; position-based resolution is the
  robust fallback.

Station material slots:

- `mat_wall`
- `mat_wall_exterior`
- `mat_floor`
- `mat_ceiling`
- `mat_trim`
- `mat_structure`

Station asset layout:

- `game/assets/station_kit/pieces`
- `game/assets/station_kit/walls`
- `game/assets/station_kit/detail`
- `game/assets/station_kit/props`
- `game/assets/station_kit/themes`
- `game/assets/station_kit/materials`
- `game/assets/station_kit/source`
- layout JSON under `game/data/station_layouts`

`test_station.json` fixture:

- Name: `Test Station`.
- Theme: `frontier`.
- Multi-level layout across levels `0-2`.
- Purposes: `bar`, `cargo_bay`, `engineering`, `quarters`, `bridge`.
- Stairwells connect levels.
- `airlock_1x1` at position `[3, 1]`, rotation `90`.
- Exact 23-piece grid with positions, rotations, levels, and initial props is
  preserved at `docs/archive/starlight/data/test_station.json`.

External station blockout:

- External grid: `2m`.
- Connector helpers: `SNAP_FRONT_8x6`, `SNAP_REAR_8x6`,
  `SNAP_LEFT_4x3`, `SNAP_RIGHT_4x3`, `SNAP_TOP_12x8`,
  `SNAP_BOTTOM_12x8`, and related side/front/rear helpers.
- Small craft dock clearance: `8m W x 6m H x 10m D`.
- Prefixes: `STATIC_*`, `ANIM_*`, `COLLISION_*`, `SNAP_*`, `HELPER_*`,
  `LIGHT_*`, and material names `MAT_*`.

`Docking_Module_A`:

- Size: `16m W x 10m H x 12m D`.
- Front bay: `8m x 6m`.
- Rear connector: `8m x 6m`.
- Optional side connector: `4m x 3m`.
- Must have a real hollow fly-in volume.
- Visual requirements: heavy armored front frame, rails, clamp blocks, inner
  pressure frame, emissive approach/status lights.

Four-dock cartridge baseline:

- `2 x 2` dock array.
- Each cartridge opening `8m x 6m`.
- Footprint `11m x 8m`.
- Depth `10-12m`.
- Rear connector `8m x 6m`.
- Station body connects to rear faces rather than swallowing the docks.

Station themes:

- Military
- Industrial
- Frontier
- Criminal
- Derelict

Theme rules include material meanings, lighting colors/energy/ambient/contrast,
flicker states, a budget of `8-12` shadow-casting visible lights, `30-50`
visible decals per room, and deterministic station-name seeded placement.

Prop system:

- Collision tiers: `Blocking`, `InteractableOnly`, `None`.
- `Clutter` is a prop category, not a collision tier.
- Snap types: `Floor`, `Wall`, `Ceiling`.
- Placement is deterministic and seeded by station name plus piece position.
- Scatter is multi-pass.
- Target scale: `200-300` props per station, fewer than `30` interactables.

Door behavior:

- States: `Closed`, `Opening`, `Open`, `Closing`, plus `Blocked`, `Locked`,
  `Malfunction`.
- Collision disabled at `30%` opening.
- Close only after all bodies exit plus `2s` timer.
- Tween duration `0.4s`.
- Door crossing uses `NavigationLink3D`.
- Airlock interlock forbids both doors being open.

Procedural station seeds:

- Templates: `medbay`, `cargo_bay`, `command_deck`, `crew_quarters`,
  `bar_lounge`, `engineering`.
- Inputs: `size`, `theme`, `purpose`, `levels`, `seed`.
- Growth weights: `40/30/30` for corridor continue/branch behavior.
- Connectivity uses BFS.
- Max retries: `10`.
- Special rooms are placed at max graph distance.

## Future Design Seeds

Combat roadmap details:

- Primary weapons have no ammo.
- Heat is per mount.
- `80-100%` heat warning.
- At `100%`, DPS is `90%` until overheat.
- Overheat at `100%` disables until heat drops below `50%`.
- Overheat cooldown is about `3s`.
- `100%` weapon power gives `1.5x` cooling.
- `0%` weapon power disables primaries.

Damage type table:

- Kinetic: shield `25%`, hull `150%`.
- Laser: shield `100%`, hull `100%`.
- Plasma: shield `150%`, hull `75%`.
- Missile: shield `100%`, hull `100%`.

Range falloff:

```text
lerp(1.0, 0.25, distance / max_range)
```

Future missile contract:

- Range `1500-2000u`.
- Tracking cone `60deg`.
- Lock about `2s`, doubled to `4s` against ECM.
- Lock persists while in range after cone exit.
- Flares on `G`.

Mining cycle states:

- `DOCKED`
- `OUTBOUND`
- `EXTRACTING`
- `RETURN`
- `DOCKED`

Mining details:

- Stations own `3-5` miners.
- Extraction time `60-180s`.
- Far timer includes outbound/return distance over average speed, extraction
  capacity/rate, and dock unload `10-30s`.
- Resource zone fields: `type`, `center_body`, `orbit`, `radius`, `richness`,
  resource IDs, `danger_level`.
- Example `Ceres Belt`: `asteroid_field`, orbit `400000`, radius `20000`,
  richness `0.8`, resources `raw_ore`, `rare_metals`.
- Wreck salvage sites are fixed positions in debris fields.
- Salvage loot contains commodities, equipment, and data.
- Wreck respawn `30-60 minutes`.
- Scanner tier controls detection.
- First visit gives discovery XP.
- Tractor beam and scanner support specialization.

Player mining equipment:

- Mining laser Mk1: `1 unit / 5s`, range `200u`.
- Mining laser Mk2: `1 unit / 3s`, range `300u`.
- Mining laser Mk3: `1 unit / 1.5s`, range `400u`.
- Gas scoop: `1 unit / 4s`.
- Beam breaks above `2 u/s`.
- Asteroids have finite yield `5-20`, respawn `10-15 min`.

Brown dwarf sector seeds:

- Dense playable areas `100000-200000m`.
- Volumetric orange/red/brown haze.
- Gas streamers.
- No clear space.
- Resource bonanza.
- Named seeds: `Luhman 16`, `WISE 0855`, `Epsilon Indi Ba/Bb`.

Legacy economy roadmap includes full ship tier/pricing/equipment tables, about
`42` commodity IDs, single-entry metals, production profile IDs, tick periods,
consumption profiles, `5%` partial-input floor, `1%` baseline drift, and price
clamp `0.25x-3x`. It is archived at
`docs/archive/starlight/roadmap/17_content_and_balancing.md`. Preserve that
roadmap as a future economy/balancing source even though the current VS1 runtime
only implements four commodities.

## Test Contracts To Preserve

The Godot tests are the best executable memory. Preserve or replace these in
Unreal:

- Format tests: distance formatting at units, `k`, `M`, and boundaries.
- Item catalog tests: known/unknown item lookup, display fallback, registered
  IDs.
- Item definition tests: stackability and category-to-slot fit matrix.
- Item stack tests: empty, zero, negative, and populated stack behavior.
- Player state tests: defaults, shield-first damage, hull clamp, credit spend,
  cargo add/remove capacity and refusal behavior.
- Economy tests: default stock, known station validation, empty/full price
  endpoints, scarcity vs glut, buy/sell mutation, insufficient credits,
  insufficient stock, cargo-overflow refund.
- Station registry tests: known/unknown station, civilian scene resolution,
  hostile scene resolution, unknown fallback to civilian, faction null/empty.
- Mission catalog tests: all expected mission IDs, objective ordering,
  dependencies, rewards.
- Mission service tests: reset, unknown status, empty capture, null restore.
- Mission flow tests: offer, accept, wrong-state refusal, objective matching,
  ready-to-turn-in, payout, progress round-trip, giver station matching, locked
  dependency refusal.
- Mission dialog tests: stub service not loaded, no choices, load false,
  continue empty, variables null, choose no-op.
- Save tests: no save, save exists, round-trip credits/cargo, corrupt JSON,
  unknown station, negative credits, old version, loadable-save query.
- Star system loader tests: ID/display parsing, orbit parsing, vector formats,
  numeric strings, jump gates, comments, trailing commas.
- Star system catalog tests: known IDs include `sol` and `alpha_centauri`.
- Orbital body tests: quarter/half orbit, eccentric perihelion, inclination,
  zero-period clamp, phase offset.
- Lagrange anchor tests: L2, L1, diagonal radial, coincident false.
- Distant sector tests: construction counts, zero initial arrivals, zero-delta
  no arrivals, all ships accounted for, large delta produces arrivals.
- Space combat math tests: stationary intercept, crossing lead, fleeing target
  false, head-on intercept, moving shooter frame, degenerate finite result.

## Known Divergences And Discard Notes

These are useful if trying to recreate Starlight closely while still preserving
the Unreal direction:

- Sol and Alpha Centauri are legacy reference fixtures. Stargame should not
  silently default to Sol.
- Godot code falls back to Sol for unknown system IDs; Unreal should reject or
  resolve explicitly.
- Starlight flight is reference for feel, not an instruction to port
  `ShipController` structure.
- First Starlight slice used V1 docked menus and kept full procedural station
  interiors out of scope.
- Some Starlight roadmap items were unprototyped: autopilot toggle, gravity
  drift, station SOIs, and full collision/death handling.
- VS3 already deleted orphan prototype files/scenes and documented the remaining
  pure C# test suite plus deferred scene-runner coverage. Treat that as a
  record of intentional pruning.
- Unreal already diverges in good ways: typed data assets, systemic ledgers,
  service transaction journals, frontier mission IDs, and stricter validation.
  Preserve Starlight facts as references, not regressions.

## Practical Confidence

With this reference, `starlight-preservation-audit.md`, and the existing
Stargame domain docs, the remaining deletion risk is no longer the general game
shape. The general shape, content skeleton, and most concrete behaviors are now
captured.

What would still be lost by deleting the Godot repo is implementation texture:
exact scene transforms not listed here, full authored dialog wording, raw source
comments, all asset files, and any behavior hidden in scene metadata. If close
recreation matters, archive the repo or export a frozen artifact bundle. If
design and Unreal migration are the goal, this capture is enough to move forward
with high confidence.
