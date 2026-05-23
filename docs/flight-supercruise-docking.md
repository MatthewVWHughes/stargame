# Flight, Supercruise, And Docking

This document defines how player ship movement, local reference frames, supercruise, gravity lockout, and docking work.

The goal is not realistic orbital physics. The goal is a readable spaceflight model that works with moving bodies and stations, survives save/load, and does not fight Unreal's actor/physics assumptions.

## Core Rule

The player ship has a logical location and a local Unreal actor representation.

Gameplay systems reason over:

- stable system/body/station/gate IDs
- coordinate frames
- authoritative simulation time
- logical position and velocity
- active flight mode

The Unreal actor transform is the current local presentation of that logical state. It is not the durable source of truth.

## Coordinate Frames And Location Modes

Use the canonical split from `system-data-contracts.md`.

Coordinate frames:

- `system_barycentric`
- `body_relative`
- `station_relative`
- `gate_relative`

Location modes:

- `free_flight`
- `station_docked`
- `gate_arrival`
- `respawn`

Coordinate frame answers "where is this transform measured from?" Location mode answers "how should load/restore treat it?"

## Local Bubble And Origin

The local bubble is the Unreal-space region where the player ship, camera, nearby ships, projectiles, docking volumes, VFX, and local interactables remain numerically stable.

Ownership:

- the headless `UOrbitRouteFrameQueryService` owns logical frame, orbit, route, docking-port, gate-arrival, and gravity-well queries.
- `UStarSystemSubsystem` wraps/caches those query results for the active world.
- local flight code owns player actor movement inside the bubble.
- a local bubble/origin helper owns projection between system-space and Unreal actor space.
- presentation actors update from the current bubble origin after logical frame evaluation.

Required state:

- `BubbleOriginFrame`
- `BubbleOriginAnchorId`, optional
- `BubbleOriginSystemPositionCm`
- `BubbleOriginSimulationTimeSeconds`
- `LocalBubbleRadiusCm`
- `OriginShiftThresholdCm`
- `LastShiftReason`
- `bShiftInProgress`

Rules:

- origin shifts never change canonical logical location
- saves store logical frame data, not actor-space transforms
- actor-space transforms become invalid after a bubble shift until reprojected
- projectiles, particles, HUD markers, docking volumes, and realized NPC actors must be reprojected or explicitly culled on shift
- debug output must show bubble origin, player logical position, player actor position, and last shift reason

Origin-shift ordering and per-object policy:

1. freeze local physics, steering writes, projectile simulation, and presentation attachment updates
2. read authoritative simulation time once
3. evaluate the player's canonical logical location and choose the new bubble origin
4. invalidate all actor-space projections and debug handles tied to the previous origin
5. reproject objects whose policy is `reproject`
6. demote objects whose policy is `demote_to_logical`
7. cull objects whose policy is `cull_transient`
8. fail the shift and restore the old origin only if a required object has policy `fail_on_invalid_projection`
9. rebuild docking volumes, HUD markers, atmosphere/local VFX anchors, and nearby target caches from fresh projections
10. unfreeze local simulation on the next coherent frame

Per-object policies:

| Object Type | Policy | Notes |
| --- | --- | --- |
| Player ship and camera | `reproject` | Failure aborts the shift. |
| Docking final-assist ship and live port volume | `reproject` | Re-evaluate port from authoritative time before projection. |
| Realized NPC with route-recoverable state | `reproject` | If outside bubble after projection, demote. |
| Realized NPC without recoverable logical state | `fail_on_invalid_projection` | Prevent losing canonical state silently. |
| Projectiles and short-lived collision traces | `cull_transient` | Do not persist across large shifts unless explicitly modeled. |
| Persistent missiles/mines/drones | `demote_to_logical` or `reproject` | Policy is defined by archetype. |
| Niagara, particles, decals, audio one-shots | `cull_transient` | Presentation may respawn from logical events. |
| HUD markers and scanner contacts | `reproject` | Rebuild from logical target descriptors. |
| Far body, station, gate, and atmosphere proxies | `reproject` | Rendering doc defines representation-specific behavior. |

If any object cannot satisfy its declared policy, the shift result records `LastShiftReason`, failed object ID/type, and recovery action. The shift must not leave a mixed-origin frame.

Initial provisional values:

| Field | Value |
| --- | ---: |
| `LocalBubbleRadiusCm` | `100000000` |
| `OriginShiftThresholdCm` | `40000000` |
| `StationApproachBubbleRadiusCm` | `500000` |
| `DockingBubbleRadiusCm` | `100000` |

These values are tuning. They must live in data/profile settings, not scattered across actors.

## Frame-Resolved Transform

`FFrameResolvedTransform` is a shared runtime contract, not an AI-only contract.

Required fields:

- coordinate frame
- anchor ID
- authoritative simulation time
- position in centimeters
- rotation
- linear velocity in centimeters per second
- angular velocity in radians per second, when available
- actor-space projection validity
- actor-space transform, non-saveable

Any system that needs moving-frame docking, station keeping, AI route prediction, target distance, or save/load restoration uses this shape.

## Station Keeping And Frame Inheritance

Normal local flight can inherit motion from a dominant nearby frame.

Required concepts:

- dominant frame anchor ID
- enter radius
- exit radius
- inherited frame velocity
- relative ship velocity
- absolute/system-space ship velocity
- frame inheritance enabled/disabled

Rules:

- station and body proximity can choose a dominant frame
- enter/exit thresholds use hysteresis
- docking final approach always operates in docking-port local frame
- supercruise does not blindly inherit station/body frame velocity
- save/load records enough velocity-frame state to restore without a speed jump

Initial behavior:

- inside a station approach bubble, inherit station frame velocity for normal flight
- outside the exit radius, return to system-space velocity
- during final docking rail, ship motion is evaluated from live docking-port transform plus local interpolation

## Ship Flight Modes

Use C++ state for flight mode.

`EShipFlightMode`:

- `Normal`
- `Cruise`, optional later
- `Supercruise`
- `DockingAssist`
- `Docked`
- `GateArrival`
- `Disabled`

Blueprint may present mode changes, VFX, audio, and HUD animations. It does not own mode transitions.

## Normal Flight

Normal flight is used for combat, station approach, docking setup, mining, and local maneuvering.

Required inputs:

- throttle/forward
- lateral/vertical strafe
- pitch/yaw/roll
- boost, optional later
- request supercruise
- request docking

Required state:

- local actor velocity
- logical velocity
- current coordinate frame
- dominant frame anchor
- inherited frame velocity
- selected target ID
- flight assist tuning profile

Initial provisional tuning:

| Field | Value |
| --- | ---: |
| `NormalFlightMaxSpeedCmPerSec` | `24000` |
| `NormalAccelerationCmPerSec2` | `8000` |
| `NormalAngularSpeedDegPerSec` | `90` |
| `DockingMaxApproachSpeedCmPerSec` | `2500` |

## Supercruise

Supercruise is a C++ state machine, not a raw speed multiplier.

`ESupercruiseState`:

- `Inactive`
- `Spooling`
- `Cruising`
- `ForcedDropout`
- `ManualDropout`
- `DropoutCooldown`

Transition inputs:

- `RequestEnterSupercruise`
- `CancelSpool`
- `RequestManualDropout`
- `EnterGravityLockout`
- `TargetApproachReady`
- `CooldownElapsed`

Required failure results:

- `AlreadyInSupercruise`
- `InsideLockout`
- `DriveDisabled`
- `NoValidLogicalLocation`
- `TargetTooClose`
- `SpoolInterrupted`

Supercruise evaluates:

- logical system-space position
- selected target frame
- gravity wells
- speed curve
- turn response curve
- lockout/dropout radii
- target approach telemetry

### Moving-Target Guidance And Braking

Supercruise guidance targets a moving frame, not a cached point.

Required inputs:

- ship logical system-space position and velocity
- selected `FMovingFrameTarget` or gate/station/body navigation target
- target resolved transform and velocity at current time
- predicted target transform and velocity at estimated arrival time
- gravity-well overlap query result
- supercruise speed, acceleration, braking, and turn-response tuning

Deterministic guidance law:

1. Resolve the target at current authoritative time.
2. Estimate time-to-go using current range, current closing speed, speed ceiling, and braking acceleration.
3. Resolve the target again at `Now + TimeToGo`.
4. Aim at the predicted target position plus authored dropout/arrival offset in target frame.
5. Compute relative position and velocity against the predicted target frame.
6. Set desired forward direction to the relative position vector unless turn-rate limits require a bounded rotation step.
7. Compute safe braking speed from `sqrt(2 * BrakingAccelerationCmPerSec2 * Max(0, DistanceToDropoutBandCm))`.
8. Desired speed is the minimum of speed curve result, gravity-well speed ceiling, safe braking speed, and target-relative approach speed cap.
9. Acceleration toward desired speed is clamped by supercruise acceleration/deceleration tuning.
10. Dropout readiness requires distance inside the target band, relative speed below threshold, alignment inside threshold, and no unsafe gravity-well dropout result.

The estimate loop uses a fixed maximum iteration count and deterministic tolerances. It must not depend on frame rate, actor tick order, or Blueprint latent timing. If prediction fails, guidance reports a typed failure and continues current safe cruise behavior instead of snapping to a stale target.

Braking is relative to the target frame. A station, moon, or gate moving toward the ship can reduce required braking distance; one moving away can increase it. HUD distance/speed feedback should expose target-relative range and closing speed.

Initial provisional tuning:

| Field | Value |
| --- | ---: |
| `SupercruiseMinSpeedCmPerSec` | `1000000` |
| `SupercruiseMaxSpeedCmPerSec` | `20000000` |
| `SupercruiseSpoolSeconds` | `3` |
| `DropoutCooldownSeconds` | `5` |
| `SupercruiseTargetDropoutMinRadiusCm` | `250000` |
| `SupercruiseTargetDropoutMaxRadiusCm` | `750000` |

## Gravity Lockout

Gravity wells own slowdown, lockout, and dropout radii through `FGravityWellDefinition`.

Bodies do not own gameplay lockout fields directly.

Starlight/Godot used one gravity proximity concept that could block entry and
force dropout. Stargame deliberately separates slowdown, lockout, and forced
dropout so tuning can preserve the same readable player contract while avoiding
unwanted abrupt exits at the outer edge of a gravity well. If parity is in doubt
for a test fixture, set lockout and dropout radii equal before adding extra
tuning.

Required well data:

- stable well ID
- moving anchor ID and coordinate frame
- local offset in anchor frame
- slowdown, lockout, dropout, and minimum safe dropout radii
- affected flight modes
- slowdown/speed-ceiling profile
- entry-block and forced-dropout flags
- route-estimate participation flag

Required query:

- input: logical ship location, velocity, flight mode, simulation time
- output: nearest well ID, slowdown factor, speed ceiling, lockout state, forced dropout reason

Supercruise rules:

- slowdown radius reduces speed ceiling smoothly
- being inside lockout refuses supercruise entry
- entering lockout during supercruise does not immediately force dropout unless
  the fixture intentionally uses equal lockout/dropout radii for Godot parity;
  it blocks re-entry if the player drops out or cancels
- entering the smaller dropout radius during supercruise forces dropout
- dropout clamps velocity to normal-flight range

## Docking

Docking is a C++ state machine attached to station and docking-port identity.

`EDockingState`:

- `None`
- `Requested`
- `Reserved`
- `Approach`
- `FinalAssist`
- `Docked`
- `Undocking`
- `Aborted`

Immutable data:

- `FDockingPortDefinition`
- station-local approach transform
- station-local docked transform
- station-local undock transform
- activation range
- allowed ship classes
- required alignment
- maximum approach speed

Mutable runtime/save data:

- `FDockingPortRuntimeState`
- occupying ship ID
- reserved ship ID
- reservation expiry time
- docking state
- clearance ID
- last failure reason

`FDockingPortDefinition` must not contain occupancy. Fixture occupancy seeds belong in fixture/runtime-state data and become `FDockingPortRuntimeState` during system build.

Per-ship operation/save data:

- `FDockingOperationState`
- ship instance ID
- station and docking port IDs
- operation state
- clearance ID
- captured port-relative transform and velocity, when final assist has begun
- operation start and last state change times
- service transaction IDs
- failure/abort reason

`FDockingOperationState` is not port occupancy. A port can be occupied by a docked ship while that ship's operation state stores service/undock resume details. A ship can also have a reserved or approach operation while the port runtime state records the corresponding reservation.

## Moving-Frame Docking

Final docking assist operates in docking-port local frame.

Required flow:

1. resolve station and port by stable IDs
2. evaluate live port transform from authoritative simulation time
3. verify range, velocity, alignment, clearance, and reservation
4. capture ship transform relative to the live port
5. disable normal flight input during final assist
6. interpolate local port-relative transform
7. each tick apply `LivePortTransform * LocalInterpolatedTransform`
8. write docked logical state on completion

Port attitude and spin:

- the station orbit evaluation returns position, rotation, linear velocity, and angular velocity
- the docking port inherits that resolved station basis
- approach/docked/undock transforms are applied in station-local axes
- inherited angular velocity is included when checking relative velocity and alignment
- AI prediction and save/load use the same live port evaluation API as player docking

Undock:

- evaluates live undock transform
- clears reservation/occupancy state correctly
- gives player immediate control
- does not autopilot back toward the approach marker

Abort:

- clears reservation if appropriate
- restores normal flight in a valid frame
- preserves safe local velocity

## Save/Load Requirements

Save free flight:

- coordinate frame
- location mode
- anchor ID
- position
- rotation
- linear velocity
- velocity frame
- dominant frame anchor
- frame inheritance flag
- flight mode
- bubble origin metadata for actor-space projection only; saved free-flight location remains `system_barycentric`

Save docked:

- system ID
- station ID
- docking port ID
- ship instance ID
- docking state
- per-ship docking operation state
- referenced port occupancy/reservation state
- authoritative simulation time

Load order follows `save-load-and-versioning.md`.

Port occupancy remains station/system runtime state. The player ship's save location references the port and operation ID, then load validates it against `FDockingPortRuntimeState`. If catch-up changes an NPC reservation before load realizes the player, the player docked occupancy has priority only when the saved port state already names that player ship as occupying the port; otherwise the conflict is a validation failure or explicit content/save upgrade case.

Saved bubble origin is a restore hint, not the authoritative location. Load validates any saved projection hint against authoritative simulation time and the ship's logical frame. If the hint is stale, outside the configured bubble radius, or references a missing anchor, the system recomputes a fresh bubble origin around the restored logical ship position before actor projection.

## Debug Requirements

Required debug surface:

- flight mode
- supercruise state
- docking state
- coordinate frame
- location mode
- dominant frame anchor
- inherited frame velocity
- logical position
- actor-space position
- bubble origin and shift threshold
- selected target distance
- nearest gravity well
- lockout state
- speed ceiling
- dropout target band
- docking port ID
- docking reservation/occupancy
- docking operation ID
- docking local transform error

## Acceptance Tests

- frame/location terminology round-trips through save/load
- same authoritative time produces same station and docking-port transforms
- local bubble shift does not change logical ship location
- normal flight inherits station frame velocity inside station bubble
- supercruise entry fails inside lockout
- supercruise forced dropout clamps velocity
- supercruise moving-target guidance gives the same braking decision from the same time/target inputs
- target dropout lands inside configured target band
- gate arrival restores gate-relative position and velocity after the destination gate frame resolves
- final docking assist follows an orbiting station without drift
- undock gives immediate local control
- save while docked reloads to the live moving port
- save during free flight restores velocity frame without speed jump
