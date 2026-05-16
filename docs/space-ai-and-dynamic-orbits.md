# Space AI And Dynamic Orbits

This document defines how space NPC AI must work when planets, stations, routes, patrol zones, and threats are all moving on deterministic orbital rails.

The short version: no important NPC decision may depend on a fixed world-space point unless that point is explicitly local to a moving anchor. Space AI must reason over stable IDs, reference frames, route segments, and predicted future transforms.

Logical encounters depend on the systemic foundation contracts in `systemic-gameplay-foundations.md`: canonical simulation events, faction/jurisdiction data, legal outcomes, cargo/inventory, station market identity, and comms/message records. Space AI may request or consume those outcomes, but it must not invent private event, crime, cargo, or message schemas.

## Verdict

Actor AI in space is viable, but only if it is split into layers:

- strategic intent: what the ship or group is trying to do
- dynamic planning: where that intent resolves at a future simulation time
- tactical steering/combat: how a realized actor behaves near the player

Trying to solve pirate ambushes, patrol routes, convoy formations, or flee destinations as ordinary Unreal actor waypoints will fail. The world is not static enough for that.

## Core Rule

Every AI target must be frame-aware.

Use moving-frame targets instead of raw locations:

- target stable ID
- coordinate frame
- anchor ID
- local offset in that frame
- route segment ID, if applicable

`FMovingFrameTarget` is an immutable target descriptor. It does not contain cached position, cached velocity, prediction time, or actor pointers. Evaluated results belong in `FAIPredictedTransform` or `FRouteSample`.

The actor pointer is never part of saved/logical target state. Tier 1 may use a non-saveable resolved-target adapter for a realized actor, but Tier 2 and Tier 3 ships must be able to make decisions without any actor existing.

## Prediction Contract

Kepler/orbit evaluation is the common source for AI prediction.

Authoritative time is owned by the persistent session/galaxy simulation. `UStarSystemSubsystem` may expose an active-system view of that time and cache frame evaluations, but it does not advance the authoritative gameplay clock.

AI queries need APIs equivalent to:

- evaluate body/station/gate transform at simulation time
- evaluate route endpoint transforms at simulation time
- evaluate route sample at route progress and simulation time
- estimate travel time from current state to a moving target
- estimate intercept feasibility against a moving route or ship
- estimate gravity/supercruise lockout along a candidate path

The planner does not need perfect physics. It does need consistent prediction. A crude but deterministic estimate is better than a visually plausible waypoint that moves out from under the AI.

Prediction returns logical frame-space first. Projection into rebased/local Unreal actor space is a separate Tier 1 query.

Prediction must preserve basis and attitude, not just position. Moving stations, docking ports, route samples, and formation slots require:

- frame basis policy
- resolved rotation
- linear velocity
- angular velocity when available
- simulation time used for the sample

AI docking and station-keeping use the same moving-port transform contract as player docking. A planner estimating arrival at an orbiting station must evaluate the station and port at the predicted arrival time, including station spin/attitude.

Prediction service requirement:

- Phase AI-0 uses the data-only/headless orbit-route-frame query service defined in `system-data-contracts.md`
- the service accepts definitions and logical state, not actors or active registry handles
- offline catch-up, Tier 2 planning, and active actor steering all call the same evaluate/estimate contracts
- active-system caches may accelerate queries but must produce the same answers as the headless service for the same authoritative time
- tests must run the prediction suite without loading a gameplay map

## Runtime Ownership

Space AI must not collapse into `UStarSystemSubsystem`.

Required ownership:

- `UGalaxySimulationSubsystem` or equivalent persistent session owner: authoritative simulation clock, persistent NPC records, Tier 3 jobs/events, RNG streams, processed-event watermarks, faction/economy/legal simulation state
- `UActiveTrafficSubsystem` or `UTrafficRealizationSubsystem`: active-system Tier 2 queues, logical contact tracks, promotion/demotion, pooled actor ownership, realization budgets
- `UStarSystemSubsystem`: active system lifecycle, frame/orbit/registry queries, active-system data view
- Tier 1 ship actors/controllers/components: realized execution only

## Frame Update Order

The per-frame order must be explicit to avoid stale orbit and route data:

1. advance or read the authoritative simulation clock once
2. evaluate/cache active body, station, gate, and computed-anchor frames for that time
3. sample active route segments and route security windows needed this frame
4. update logical contact tracks
5. run bounded Tier 2 AI planning and logical event processing
6. process promotion/demotion queues and pool lifecycle
7. run Tier 1 steering, combat, comms, and docking behavior
8. update presentation, HUD, debug, VFX, and audio

Do not let arbitrary component ticks advance orbit, route, AI, or economy time independently. Tick groups and subsystem dependencies must be deliberate once implemented.

## AI Layers

### Strategic Job Layer

Persistent logical simulation.

Chooses or updates:

- trade route
- patrol assignment
- escort contract
- convoy membership
- pirate ambush lane
- hunting target type
- flee destination
- distress response
- resupply/repair/docking target

This layer stores durable `FShipGoalState` and group state. It belongs in C++ logical simulation, not Blueprint actor graphs.

### Tactical Planning Layer

Active-system logical simulation.

Runs without ship actors. It evaluates:

- moving route samples
- patrol coverage
- pirate risk
- police response distance
- convoy separation
- target vulnerability
- threat strength
- nearby distress events
- promotion priority near the player

This is where most Tier 2 NPC behavior lives. It should update on bounded cadence, not every ship every frame.

### Actor Behavior Layer

Tier 1 realized actors only.

Runs:

- steering
- docking approach
- formation keeping close to the player
- weapon use
- collision avoidance
- comms timing
- local flee/combat maneuvers
- animation/VFX/audio hooks

Actor behavior consumes the current logical goal and writes back a realization snapshot on demotion.

## Formations

Formations are group goals with moving slots, not fixed offsets in world space.

Required data:

- group ID
- leader ship ID
- formation type
- slot index
- role: leader, trader, escort, police wing, pirate wing
- slot frame: leader ship, leader route sample, or group anchor
- slot basis: leader attitude, route tangent, or explicit axes
- local offset in the declared slot frame
- max separation
- rejoin policy
- break policy
- saved individual goal

Rules:

- convoy offsets resolve from the leader's predicted frame
- route-frame slots resolve from the leader's route sample tangent and route progress offset
- ship-frame slots resolve from the leader's predicted ship attitude
- group-anchor slots resolve from an explicit moving-frame target
- escorts can realize independently from the trader they escort
- demoted escorts keep their formation slot logically
- a destroyed or fleeing leader triggers leader reassignment or group breakup
- formation slots must survive save/load by stable IDs

For distant or Tier 2 formations, do not simulate tight steering. Store group state, route progress, and slot offsets. Promote only the relevant ships near the player.

## Patrols And Policing

Police patrols must be anchored to jurisdictions, stations, routes, and bodies.

Required data:

- patrol zone ID
- jurisdiction ID
- owning faction ID
- anchor frame
- covered route segment IDs
- waypoint definitions in anchor-relative frames
- scan policy
- interdiction policy
- distress response policy
- escalation policy
- threat avoidance policy

Patrol density should derive from:

- station proximity
- faction influence
- route value
- security rating
- recent crime
- recent distress calls
- available police assets

Patrols should not follow static splines in absolute Unreal space. A patrol near an orbital station or trade route must move with that station or route.

Police behavior includes:

- scan traffic
- challenge wanted ships
- respond to distress
- escort high-value routes
- chase pirates if strength is sufficient
- call reinforcements when outmatched
- disengage from impossible fights

## Pirates

Pirates should hunt dynamic traffic, not spawn at decorative encounter points.

Pirate ambush planning should evaluate:

- route value
- cargo likelihood
- victim vulnerability
- convoy escort strength
- police patrol coverage
- navy/gunboat proximity
- escape route quality
- local criminal influence
- faction hostility
- interdiction feasibility

Ambush zones are anchored to route segments and predicted route samples. They are not fixed world-space boxes unless the box is explicitly relative to a moving anchor.

Pirates should:

- lurk near valuable route segments
- avoid strong navy or police patrols
- target isolated traders and miners
- avoid convoys with strong escorts unless in a group
- demand cargo before attacking when appropriate
- flee when local threat strength exceeds confidence
- relocate when policing increases

Interdiction can be represented as a logical event or hazard on a route segment until the player is close enough for it to become a realized encounter.

## Fleeing

Flee decisions need moving destinations.

A fleeing ship should choose a destination from:

- nearest friendly station by predicted arrival time
- nearest safe patrol zone
- gate/wormhole escape route
- friendly convoy/group
- deep-space temporary hide point anchored to route/body context

Flee scoring should include:

- current threat vector
- faction access
- docking permission
- route security
- supercruise state
- drive cooldown or lockout
- expected police response
- hull/module condition
- cargo value

The flee destination must keep updating as planets, stations, patrols, and the threat move.

## Dynamic Routes

Routes between moving endpoints are definitions, not baked world splines.

Store:

- source anchor ID
- destination anchor ID
- route segment ID
- route policy
- allowed ship classes
- legal/security metadata
- risk profile
- sample policy

Sample the route at a given simulation time to get current or predicted positions. Use the sampled route for trade progress, patrol coverage, pirate ambushes, convoy slots, scanner contacts, and realization priority.

Strategic route estimates can update slowly. Active-system and near-player route samples need faster refresh. Both must use the same data contract.

Route progress semantics must be explicit. A route may use time fraction, distance fraction, or a policy-defined curve, but the definition must state which one. Estimated velocity must include endpoint motion and travel-along-route velocity.

Demotion may call `FindClosestProgress` only when the logical motion state is route-recoverable. Ships that are pursuing, fleeing, off-route, or recovering to a route need frame-relative logical state, not blind projection onto route progress.

The first deterministic fixture route geometry is `fixture_dynamic_arc_v1` from `system-data-contracts.md`. AI tests should use it to prove moving endpoint sampling, derivative velocity, and inverse progress before adding richer authored route shapes.

`FShipTrafficInstance` has one durable location truth: its logical frame/location record. Realized actor transforms, resolved frame samples, route closest-point results, and debug positions are derived views. Promotion/demotion bugs that would leave both route progress and independent absolute position as competing saved state must fail validation or choose an explicit `off_route_free_flight` logical state.

## Logical Events And Encounters

Systemic AI requires durable logical events, not just score checks.

Required event types:

- interdiction hazard
- pirate ambush
- police scan/challenge
- distress call
- patrol response
- logical engagement
- cargo surrender/jettison
- ship destruction
- route disruption

Rules:

- every event has a durable event ID
- every event has participants by stable ship/group IDs
- every event has a state, start time, expiry time, and processed watermark
- participants are reserved while the event is active
- actor promotion attaches to existing events instead of creating duplicate encounters
- an event resolves exactly once across save/load, tier promotion, and player arrival
- outcomes emit durable legal, economy, faction, news, and debug records

A ship cannot be both safely arrived and an unresolved engagement participant. If an event interrupts a route job, the route job records an interruption and waits for the encounter outcome.

## Sensors And Contact Tracks

Unreal AI Perception only sees actors, so it cannot be the source of truth for space AI.

Logical AI uses contact tracks:

- observer ship/group/faction ID
- target ship/group/route/event ID
- last known `FMovingFrameTarget`
- signature
- confidence
- identification level
- first seen time
- last seen time
- decay policy
- witness/evidence state

Tier 1 perception may feed or present contacts, but Tier 2 and Tier 3 decisions use logical contact data. Pirates should not know every police ship by omniscient route metadata; they should act from contacts, informants, route history, or security snapshots.

## Decision Inputs

Space AI needs these inputs available as C++ query data:

- ship role and faction
- combat strength estimate
- cargo value estimate
- legal/criminal status
- jurisdiction and law profile
- faction influence in the zone
- station/gate access rights
- route risk and route value
- nearby distress calls
- police/navy strength
- pirate/criminal pressure
- sensor confidence
- current goal interrupt priority
- recent event memory

Blueprints may present comms, barks, VFX, and authored behavior flavor. They should not own the canonical AI decision state.

## Unreal Implementation Direction

Use C++ for:

- goal state
- group/formation state
- route sampling
- moving-frame target queries
- intercept estimation
- risk scoring
- patrol/pirate/flee policy evaluation
- tier transition snapshots
- debug view models

Use Blueprint for:

- comms presentation
- authored bark selection
- ship-specific animation/VFX hooks
- tuning exposed from C++ data assets
- mission-specific scripted beats that call stable C++ services

Do not use NavMesh for normal space navigation. Space movement needs custom steering and route queries.

Behavior Trees or StateTree are acceptable for Tier 1 actor behavior later, but they must consume and report to the logical goal model. They must not become the persistent simulation.

If Behavior Trees or StateTree are used:

- Tier 1 only
- no NavMesh `MoveTo` for normal space movement
- no blackboard object keys as persistent target identity
- blackboard/state data stores stable IDs, frame targets, and resume keys
- tasks call C++ steering/query services
- pooled actors stop/reset brain state, blackboards, perception, latent actions, timers, controller possession, and target handles on release

MassEntity may be useful later for high-volume active-system updates. It is not required for the first implementation, and it should not replace the hard logical contracts above.

## Implementation Plan

This is not an M0 feature. It becomes valid only after system definitions, reference frames, supercruise, docking, and seeded system generation are already testable. The plan below is the required build order once space AI starts.

### Phase AI-0: Query Foundation

Goal: make moving-frame AI queries possible without any NPC brain.

Build:

- `FMovingFrameTarget`
- `FAIPredictedTransform`
- `FRouteSample`
- `FSimulationClockSnapshot`
- data-only C++ query service for body/station/gate/docking-port/computed-anchor transforms at simulation time
- C++ route sampler for endpoint-anchored route segments
- `fixture_dynamic_arc_v1` route geometry implementation for deterministic fixture coverage
- travel-time estimate from current ship state to moving target
- risk/security query stubs by route segment and jurisdiction
- debug command to print target prediction at `Now`, `Now + 60s`, and estimated arrival time

Acceptance:

- an orbiting station target predicts different future transforms at different times
- route samples between moving endpoints change consistently as endpoints orbit
- save/reload at the same authoritative simulation time produces the same predicted transforms
- no query requires an actor pointer
- prediction output separates target descriptor from evaluated transform/velocity
- `fixture_dynamic_arc_v1` route sampling returns deterministic position, basis, derivative velocity, and closest progress
- traffic instance validation rejects competing saved absolute position and route progress truths

### Phase AI-1: Logical Ship Goals

Goal: create persistent NPC intent without rendering ships.

Build:

- `FShipGoalState` integration with logical traffic records
- `FShipGroupState`
- interrupt priority and saved-goal restore
- logical goals for `Trade`, `Patrol`, `Escort`, `Pirate`, `Flee`, and `Fight`
- bounded Tier 2 update loop for active-system logical ships
- debug view for goal, target frame, route progress, and decision reason

Acceptance:

- a logical trader can progress along a moving route without spawning
- a logical patrol can follow an anchor-relative patrol assignment without static world points
- a logical pirate can select an ambush route segment based on route value and police risk
- a logical ship can enter `Flee`, preserve its saved goal, and restore it if valid
- Tier 2 update cost is capped and visible in debug output

### Phase AI-1.5: Realization Harness

Goal: expose Unreal actor lifecycle problems before formations, pirates, or police make the bug surface larger.

Build:

- one logical trader on one moving route
- pooled `AShipTrafficActor` binding to that logical record
- target steering against `FMovingFrameTarget`
- demotion snapshot back to logical state
- explicit register/unregister calls for pool acquire/release and `EndPlay`
- pool generation IDs for debug handles

Acceptance:

- the trader promotes, follows the same moving target, demotes, and resumes logically
- promotion does not create a second logical ship or encounter
- no saved/logical state stores actor pointers
- active handles include ship instance ID, actor ID, and pool generation ID
- pool release clears AI controller/brain state, blackboard/state tree data, perception, delegates, timers, target handles, and UI handles

### Phase AI-2: Groups, Formations, And Convoys

Goal: prove group behavior before combat complexity.

Build:

- `FFormationSlotState`
- convoy group creation from traffic/economy jobs
- escort assignment and slot offsets in leader route/ship frame
- leader reassignment or group breakup rules
- formation promotion/demotion state transfer
- debug overlay for group ID, leader, slot, separation, and rejoin state

Acceptance:

- a convoy exists logically while outside realization range
- when the player approaches, only budget-eligible convoy members realize
- realized escorts hold approximate formation relative to the moving leader
- demoted escorts preserve their logical slot
- destroying or forcing the leader to flee produces deterministic group behavior
- partial promotion is defined: realized escorts protect the logical group/encounter record, not just other realized actors

### Phase AI-3: Logical Contacts And Encounters

Goal: make NPC-NPC interaction causal before actor combat is introduced.

Build:

- logical contact tracks for sensors/witnesses
- logical encounter lifecycle
- interdiction/distress event records
- abstract engagement records
- patrol asset reservation
- cargo/legal/economy event outputs
- save/load idempotency for AI events

Acceptance:

- a pirate attack, distress call, patrol response, and cargo/economy consequence can complete with zero actors
- the same encounter cannot resolve twice after save/load or player arrival
- actor promotion attaches to the existing logical event instead of spawning a duplicate encounter
- legal offenses from NPC-NPC events enter the same jurisdiction/evidence pipeline as player offenses

### Phase AI-4: Patrols And Lawful Response

Goal: make policing dynamic and jurisdiction-driven.

Build:

- `FPatrolZoneDefinition`
- patrol assignments anchored to station/body/gate/route frames
- scan/interdiction policy
- distress response event
- response strength scoring
- disengage/reinforce behavior for outmatched patrols
- legal/crime integration for wanted/hostile status

Acceptance:

- patrols move with orbital stations and covered route segments
- patrol density changes from faction influence, security, and recent crime
- police scan and challenge wanted traffic
- police respond to a distress call without requiring all ships to be actors
- patrol debug shows jurisdiction, assignment, target frame, response score, and reason

### Phase AI-5: Pirate Hunting And Ambush

Goal: make pirates behave like opportunistic traffic predators.

Build:

- `FPirateAmbushProfile`
- pirate group/job generation from criminal pressure and route value
- victim selection from cargo value, isolation, escort strength, police risk, and own strength
- ambush/interdiction event on a dynamic route segment
- demand, attack, flee, and relocate decisions
- escape route scoring

Acceptance:

- pirates prefer valuable weakly-policed routes
- pirates avoid obviously stronger navy/police groups
- pirates target isolated traders before escorted convoys
- pirate ambushes follow moving trade routes instead of static encounter boxes
- a pirate can abandon an ambush and relocate when policing rises

### Phase AI-6: Tier 1 Actor Behavior

Goal: connect the logical AI to realized Unreal ships.

Build:

- `AShipTrafficActor` AI binding to `FShipGoalState`
- steering for target approach, follow, flee, and attack
- local formation keeping
- simple weapon engagement and disengage rules
- comms hooks for police scans, pirate demands, distress calls, and surrender/flee chatter
- demotion snapshot including current target, threat, group, and actor behavior resume key

Acceptance:

- realized actors execute the same goal they had logically
- promotion does not reset goal, target, group, or flee state
- demotion writes enough state to continue the goal logically
- actor AI can be pooled and reset without stale delegates, timers, targets, or UI handles
- debug makes clear whether a bad behavior came from planning, prediction, or actor steering

### Phase AI-7: Stress And Tuning

Goal: prove the model scales before adding content volume.

Build:

- synthetic active-system traffic fixture
- route/patrol/pirate density profiles
- automated debug dump for AI state and budgets
- stress test for many logical ships with capped realized actors
- deterministic replay seed for AI decisions

Acceptance:

- realized actor count never exceeds budget
- Tier 2 AI update time remains inside configured budget
- pirate/patrol decisions are repeatable from seed and simulation time
- no AI event applies twice across save/load
- debug output explains blocked promotions, skipped updates, and decision outcomes

## First Playable AI Slice

The first slice should be deliberately small:

- one generated or authored non-Sol system
- two stations on moving anchors
- one fast-orbiting station endpoint
- one nested-moon endpoint
- one valuable trade route between them
- one route crossing or skirting a gravity lockout
- three logical traders
- one two-ship patrol
- one two-ship pirate group
- one distress event
- one pending interdiction saved and reloaded mid-event
- hard actor cap low enough to prove promotion rules

Expected player experience:

1. traders move along the route logically
2. patrol covers the route and scans traffic
3. pirates choose an ambush point on the moving route
4. if the player approaches, relevant ships promote into actors
5. pirates either demand cargo, attack, or flee depending on patrol/player strength
6. distress and police response appear through debug/comms
7. save/reload during the pending event does not duplicate or erase it
8. promotion/demotion during flee preserves target, threat, velocity frame, and recovery policy

This slice proves the AI architecture. It is not trying to produce final combat, final economy, or final mission content.

## Required Data

Add these contracts when space AI enters implementation:

- `FMovingFrameTarget`
- `FAIPredictedTransform`
- `FRouteSample`
- `FShipGroupState`
- `FFormationSlotState`
- `FPatrolZoneDefinition`
- `FPatrolAssignment`
- `FLogicalContactTrack`
- `FLogicalEncounterEvent`
- `FLogicalEngagementRecord`
- `FRouteSecuritySnapshot`
- `FPatrolAssetState`
- `FInterdictionHazardState`
- `FDistressEventRecord`
- `FPirateAmbushProfile`
- `FAmbushSiteState`
- `FFleeDecisionRecord`
- `FInterceptEstimate`
- `FAIThreatAssessment`
- `FAIDecisionReason`

## Debug Requirements

Every AI decision needs inspectable reasons.

Required debug views:

- ship ID and current tier
- current goal and saved goal
- group/formation ID
- formation slot and leader
- target frame and anchor ID
- route segment and route progress
- current predicted target transform
- predicted target transform at arrival time
- intercept estimate
- patrol jurisdiction
- scan/interdiction state
- pirate risk score
- police response score
- flee destination and reason
- threat strength comparison
- last interrupt reason
- actor realization state
- active handle ID and pool generation ID
- blocked promotion reason, if any

Debug output should make bad decisions obvious. If a pirate ignores a navy patrol, the debug view should show whether it failed to see the patrol, mis-scored the risk, or had no valid escape route.

## Hard Invariants

- no Tier 2 or Tier 3 AI decision may require an actor pointer
- no patrol waypoint may be absolute world space unless explicitly marked local-only/test-only
- every patrol, ambush, convoy, and flee destination must resolve through a frame-aware target
- formation offsets must be local to leader/route frame, not absolute world space
- pirate and police behavior must reference faction, jurisdiction, and legal state
- route risk and security must be queryable without loading every actor
- demotion must preserve enough AI state to resume the same intent logically
- save/load must not duplicate AI events, distress calls, or interdictions
- a logical encounter resolves exactly once across save/load, tier promotion, and player arrival
- actor promotion must attach to an existing logical event/ship/group, never create an independent duplicate encounter
- patrol response must consume or reserve a patrol asset; density does not create infinite police
- pirate target choice must use plausible knowledge from contact tracks, informants, route history, or sensors
- route security used by pirates must be time-windowed, not only static route metadata
- flee destination changes require a materially better score or invalidation of the current destination
- legal offenses from NPC-NPC events must use the same jurisdiction/evidence pipeline as player offenses

## Implementation Rule

Keep the Godot intent:

- patrols that scan and respond
- pirates that hunt traffic and fear stronger patrols
- convoys with escorts
- fleeing ships that seek safety
- distress calls and comms events
- traffic that follows stations and orbital routes

Replace the implementation:

- no Godot node paths
- no static route arrays as runtime truth
- no actor-only NPC brain
- no absolute world-space patrol splines for moving orbital content
- no Blueprint-owned persistent AI state

The correct Unreal version is data-driven, frame-aware, tiered, and debuggable from day one.
