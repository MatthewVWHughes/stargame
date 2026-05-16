# Simulation Tiering And Economy

This document defines how NPC ships, distant sectors, and economy simulation should work in Unreal.

Space NPC decision-making, formations, patrols, piracy, and fleeing are covered in more detail by `space-ai-and-dynamic-orbits.md`.

Market, inventory, faction, legal, comms, mission, and canonical event contracts are defined in `systemic-gameplay-foundations.md`. `system-data-contracts.md` owns core system/frame/AI structs; domain-specific M9 structs are canonical in `systemic-gameplay-foundations.md` unless mirrored later. This document owns the tiering model and behavioral requirements.

The Godot tier idea is worth keeping. The Godot implementation details are not.

## Verdict

Keep the tiered simulation model:

- most ships are logical data
- only nearby ships become rendered/interactable Unreal actors
- distant sectors run economy and travel as serialized simulation events, scheduled timestamps, and stock deltas
- the player sees a local slice of a much larger simulation

Do not port the Godot `NpcManager`, `DistantSector`, or hard-coded trade-route setup directly. Preserve the model, replace the implementation with Unreal-native subsystems, structs, data assets, event queues, actor pooling, and optional MassEntity support.

## Godot Source Assessment

The useful Godot ideas:

- three-tier NPC simulation
- spawn/despawn hysteresis so ships do not flicker at the boundary
- data-only distant sectors
- cargo movement as timed arrivals
- economy stock changes triggered by NPC docking/arrival
- resilient economy rules that recover from disruption
- star/catalog tiers used for map readability and progression pacing
- one goal state machine shared across tiers, with different execution detail per tier
- smooth route progress, route-aware ETA, and station/orbit anchored endpoints

The Godot parts to reject:

- hard-coded station and route arrays
- node paths as route/station identity
- direct scene-node ownership of the simulation
- per-frame distant-sector loops as the default for very large populations
- rendering traffic as the simulation source of truth
- Sol-specific station, combat, and route setup
- Godot's hard-coded `VisualKind` lists and station-index route tables

## Unreal Ownership

Use separate ownership layers.

### `UGalaxySimulationSubsystem`

`UGameInstanceSubsystem`.

Owns persistent logical simulation:

- star catalog
- catalog/route tiers
- known/discovered route graph
- global time
- distant sector summaries
- station market state
- faction/economy/legal state
- faction operational state
- credit accounts, ledger, and escrow state
- NPC trade jobs and scheduled arrivals
- generated gameplay pass outputs for markets, routes, jurisdictions, patrols, ambushes, services, and mission seeds
- save/load for logical simulation state

This subsystem does not spawn ship actors.

Current assumption: single-player first. If multiplayer or dedicated server support becomes a target, authoritative simulation state must move behind server-owned gameplay authority. `UGameInstanceSubsystem` remains acceptable for local session/save coordination, but not as a replicated gameplay authority.

### `UStarSystemSubsystem`

`UWorldSubsystem`.

Owns the active loaded system:

- active system definition
- active-system registry
- local traffic realization requests
- promotion/demotion between logical ships and actors
- local navigation targets
- active-system debug snapshots

This subsystem may own the active view of traffic, but not the entire galaxy economy.

### `UTrafficRealizationSubsystem`

`UWorldSubsystem` or internal helper owned by `UStarSystemSubsystem`.

Owns near-player ship realization:

- pooled ship actors
- promotion from logical record to actor
- demotion from actor to logical record
- spawn/despawn hysteresis
- per-frame realization budget
- actor validity and teardown
- optional MassEntity/Instanced Actors bridge later

This prevents `UStarSystemSubsystem` from becoming a giant traffic, economy, rendering, and registry service.

## Catalog Tier Meaning

The old Godot star tiers are useful as progression and map metadata, not as hard runtime loading rules.

Use `CatalogTier` or `RouteTier` terminology for per-star/per-system data. Avoid calling this a loaded-sector tier unless a real sector partition exists.

Preserve the old catalog behavior as data:

- roughly 50 nearby stars around the original home region
- real galactic coordinate mapping: game X = galactic X, game Y = galactic Z, game Z = galactic Y
- `Ly = 2` style display scale for map readability only
- tier-scaled map visuals
- generated route graph using a connected MST backbone plus short-range bonus links
- special route content such as Wolf 359 to Ross 154
- BFS/fewest-jump route display unless deliberately redesigned

Recommended meaning:

| Tier | Meaning | Runtime implication |
| --- | --- | --- |
| `0` | initial/core authored region | known early, high route density, safer economy |
| `1` | inner expansion systems | commonly connected, regular traffic |
| `2` | developed mid-range systems | known or discoverable, moderate traffic |
| `3` | frontier systems | sparse services, more risk, weaker law |
| `4` | deep frontier | low infrastructure, unstable routes, rare services |

Catalog/route tier may influence:

- map icon scale
- initial known/discovered state
- route generation weighting
- traffic density
- market baseline stability
- faction/police coverage
- pirate/criminal activity chance
- fuel/service availability
- mission generation weighting

Catalog/route tier must not decide whether a system exists. It is metadata that feeds generation and simulation.

## NPC Goal State

Preserve the Godot rule: the same logical goal model exists at every tier. Only execution detail changes.

Required goal categories:

- `Trade`
- `Patrol`
- `Mine`
- `Pirate`
- `Escort`
- `Flee`
- `Fight`
- `Dock`
- `Undock`
- `Dead`

`FShipGoalState` is field-level canonical in `system-data-contracts.md`. It must preserve at least:

- `GoalType`
- `SavedGoal`, optional
- `RouteId`
- `RouteSegmentId`
- `RouteProgress`
- `DestinationStationId`
- `TargetShipId`, optional
- `ThreatShipId`, optional
- `CargoSummary`
- `InterruptReason`, optional
- `LastGoalUpdateTime`

Interrupt priority starts as:

1. destroyed -> `Dead`
2. hull below flee threshold -> `Flee`
3. attacked/hostile -> `Fight`
4. interdicted -> forced dropout, then `Fight` or `Flee`
5. distress call nearby -> patrol response, others ignore unless explicitly configured
6. otherwise continue current goal

After `Fight` or `Flee` resolves, restore `SavedGoal` if still valid. If the saved goal no longer resolves, choose a new goal from the ship role policy.

Tier executors:

- Tier 1 executes goals through actor movement, steering, comms, combat, docking, and local perception.
- Tier 2 executes goals through route sampling, abstract conflict checks, and promotion eligibility.
- Tier 3 executes goals through scheduled events, market deltas, and abstract risk outcomes.

Do not reduce NPCs to trade timers only. Traders, patrols, miners, pirates, convoys, escorts, and mission ships all use the same goal-state contract.

All non-trade goals must remain frame-aware. Patrols, ambushes, formation slots, flee destinations, and combat targets resolve through stable IDs, route segments, and moving reference frames rather than fixed world-space points.

## NPC Simulation Tiers

### Tier 1: Realized Local Ships

Near the player.

Has Unreal actor representation:

- mesh/visuals
- collision
- targetability
- comms
- scanner contact
- combat
- local steering
- docking approach if relevant

Tier 1 ships are the only ships that render as actual ship actors.

Required rules:

- actors come from pools for the first implementation
- actor count is capped by hard budget
- promotion is rate-limited per frame
- spawn attempts are rate-limited per frame
- priority ordering decides which eligible ships realize first
- over-budget ships remain logical even if inside realization radius
- demotion records enough state to resume logically
- actors unregister on both release-to-pool and `EndPlay`
- actors are never the persistent source of truth

Required `FTrafficRealizationProfile` fields:

- `RealizeRadiusCm`
- `UnrealizeRadiusCm`
- `EmergencyCullRadiusCm`
- `MaxRealizedActors`
- `MaxPromotionsPerFrame`
- `MaxDemotionsPerFrame`
- `MaxSpawnAttemptsPerFrame`
- `MaxTier2UpdatesPerFrame`
- `MinRealizedLifetimeSeconds`
- `PromotionCooldownAfterDemotionSeconds`
- per-class or per-role pool sizes
- priority weights for player target, hostile, mission, police, pirate, trader, ambient

If the actor cap is full, the system must fail closed: do not spawn. Record a blocked-promotion reason for debug.

Initial suggested bands:

- promote at `RealizeRadiusCm`
- demote at `UnrealizeRadiusCm`
- `UnrealizeRadiusCm > RealizeRadiusCm`
- use a second emergency cull distance for runaway actors

The exact distances are tuning, not architecture. They must live in traffic profile data and debug output.

Pooled actor lifecycle:

1. `AcquireFromPool`
2. `BindLogicalShip`
3. register identity/navigation/comms/combat handles
4. run Tier 1 behavior
5. `UnbindLogicalShip`
6. write `FShipRealizationSnapshot`
7. unregister handles
8. `ResetForReuse`
9. `ReleaseToPool`

`ResetForReuse` must clear collision enablement, tick state, AI/state tree/blackboard state, delegates, timers, replicated state, owner/instigator, selected target handles, UI/HUD handles, and weak registry entries.

### Tier 2: Active-System Logical Ships

Same active star system, not close enough to render.

No ship actor.

Has logical state:

- ship instance ID
- route/job ID
- current goal
- current route segment
- route progress
- faction
- cargo summary
- estimated transform in active-system frame
- promotion eligibility

Tier 2 route sampling must be deterministic and anchored to current station/body frames. It should preserve:

- smoothstep or cruise-aware progress
- route endpoint anchors by stable station/body IDs
- route offsets in the endpoint frame
- route direction and estimated velocity for promotion
- formation offsets for convoys/patrol groups
- route progress reconstruction on demotion

Tier 2 update work must be bounded. Use spatial buckets, time-sliced candidate evaluation, and nearest/importance-first promotion queues instead of checking every logical ship every frame once counts grow.

Tier 2 ships may appear on maps/scanners as abstract contacts if discovered or in sensor range, but they do not render and do not run full steering/combat.

Tier 2 may run logical encounters through durable event records:

- contact tracks
- patrol response reservations
- interdiction hazards
- distress calls
- abstract engagements
- cargo surrender/jettison outcomes
- legal offense records
- economy/faction/news event outputs

Abstract checks must write records. They must not be hidden transient calculations that can apply twice after save/load or disappear when the player arrives.

### Tier 3: Distant Sector Jobs

Other systems/sectors.

No actor.
No per-frame position.
No active-system transform.
No live patrol/flee/intercept logic that requires active-system transforms.

Uses scheduled events:

- departure time
- arrival time
- source station ID
- destination station ID
- cargo manifest summary
- owning faction/company/pirate group
- route ID
- risk outcome, if resolved abstractly
- durable event ID and processed watermark

These are serialized simulation records keyed by authoritative simulation time. Do not model distant economy jobs with thousands of `FTimerManager` handles; Unreal timers are world-lifetime runtime mechanisms, not durable economy state.

Tier 3 is limited to scheduled route/window events and abstract outcomes. When a system activates, pending Tier 3 route/event records are converted into Tier 2 logical ships, encounters, and contact records using authoritative time and route prediction.

When a scheduled arrival fires:

1. prepare one gameplay transaction for cargo delivery, market stock, cargo transfer, ledger, and faction/economy deltas
2. commit ordered side effects through the canonical transaction journal
3. update faction/economy metrics through reputation, relationship, and operational delta records
4. validate any generated follow-up opportunities created by the event
5. choose next job
6. schedule the next departure/arrival

When a risk or piracy outcome fires:

1. create or resolve a durable logical encounter/event record
2. apply cargo loss, surrender, destruction, or delay outcome exactly once through the transaction journal
3. emit legal/evidence records if the event is witnessed or reportable
4. emit economy/faction/influence/news records and delta records
5. validate generated follow-up opportunities before they become mission offers, bounties, tips, or patrol requests
6. update route security/criminal pressure for future decisions

For large populations, prefer an event queue or time-bucketed update over checking every distant ship every frame.

Required distant-event bounds:

- `MaxDistantJobsPerSector`
- `MaxEventsProcessedPerFrame`
- `MaxCatchUpSecondsPerFrame`
- `MaxOfflineCatchUpHorizonSeconds`
- deterministic overflow behavior: aggregate low-priority ambient jobs before dropping important jobs
- persisted RNG seed or deterministic event IDs for repeatable save/load catch-up

## Economy Model

The Godot economy direction is still the right high-level approach:

- station stock levels
- production and consumption profiles
- prices from supply/demand
- price clamps
- baseline stock drift
- NPC trade jobs react to profit signals
- disruptions create temporary shortages, not permanent collapse

Initial economy invariants to preserve from the Godot roadmap:

- price formula starts from `base_price * (2.0 - fill_ratio * 1.5)`
- prices clamp between `0.25x` and `3.0x` base price
- no negative stock
- production with partial inputs has a low floor, initially `5%`
- stock drifts gently toward baseline, initially around `1%` of max capacity per tick
- NPC route rebalancing should correct ordinary shortages within minutes
- player/NPC disruption should last minutes to tens of minutes, not permanently break a market
- the player should be able to read trade opportunities from simple surplus/shortage presentation, not spreadsheets

The Unreal version should be logical simulation first.

Do not require ships to be spawned for the economy to function. A station market changing stock because a trade job arrived is valid even if no ship actor was ever rendered.

Credit movement is part of the economy simulation. Player trades, NPC trades, service fees, mission payouts, fines, bounties, and escrow holds must use durable account and ledger records from `systemic-gameplay-foundations.md`. Economy code may calculate prices and deltas, but it must not directly mutate account balances outside ledger application.

## Distant Economy

Distant sectors should not be loaded worlds.

They should have:

- sector ID
- station market states
- production tick schedule
- route graph subset
- queued trade arrivals
- faction influence summary
- faction operational state summary
- per-zone faction influence values
- security/piracy pressure
- recent event log

Distant economy update cadence:

- production/consumption can tick in coarse intervals
- trade arrivals run from scheduled events
- faction/security changes can update in slower buckets
- catch-up after loading a save should process elapsed simulation time deterministically or in clamped chunks

Save/load catch-up must persist:

- authoritative simulation time
- pending event queue or time buckets
- station market states
- credit ledger high-water marks and pending escrow holds relevant to scheduled jobs
- faction/zone influence states
- faction operational state
- RNG seeds or deterministic event IDs
- processed-event watermark
- recent transaction/event log needed for debug/news

Catch-up must be idempotent. A trade arrival, production tick, or influence update should not apply twice if load/retry happens mid-process.

Transaction-scoped catch-up must rebuild the gameplay transaction journal before processing new due events. Prepared or partially committed distant trade, risk, service, legal, or mission transactions recover from their ordered journal entries before the processed-event watermark advances.

Avoid full per-frame simulation for all distant sectors. Use:

- next-event timestamps
- time buckets
- dirty market lists
- capped catch-up work per frame when needed

## Cross-Sector Trade And Influence

The economy is one persistent simulation across all sectors, not separate unloaded worlds.

Cross-sector trade jobs require:

- source station ID
- destination station ID
- route ID
- route segment list, including gate/wormhole boundaries
- cargo summary
- owning faction/company/criminal group
- departure time
- arrival time
- route risk/security profile
- route eligibility requirements such as permits, faction access, fuel/range, or legal restrictions
- transaction IDs and result IDs for route events that mutate cargo, market stock, credits, reputation, legal records, or faction operations

Shortages should propagate across sectors after travel delay. A mining shortage in one system should affect downstream stations only when scheduled supply, production, or route events carry the effect.

Faction influence must be tracked per zone, not only as a sector summary.

Initial influence rules:

- influence values per zone should be normalized or explicitly clamped
- update cadence starts around 30 seconds
- maximum influence shift starts around 2% per tick
- influence changes from patrol presence, combat outcome, player kills, station proximity, trade volume, piracy, and criminal activity
- contested/lawless status should derive from influence and security state

Faction operational state is the bridge between influence and gameplay availability. It should track alert level, available patrol assets, reserved patrol budget, trade posture, service posture, mission posture, and recent operational events per faction/system/zone where relevant. Route security, police response, service access, and generated mission availability read this state rather than inventing local copies.

Influence, security pressure, criminal pressure, alert changes, service posture, trade posture, and mission posture changes must be recorded as faction operational delta records. Current summaries are saved for fast reads, but event replay, recovery, and debug surfaces rely on the committed delta records.

Criminal activity requires simulation hooks for:

- contraband legality by jurisdiction/faction
- smuggling routes
- pirate raids against trade/mining routes
- criminal market/station access
- hidden criminal base emergence from sustained criminal influence
- news/events from raids, shortages, police actions, and base discovery/destruction

## Active Sector Economy

The active sector/system may run more detail:

- more frequent production/consumption ticks
- Tier 2 route progress for ships in the active system
- promotion into Tier 1 actors near the player
- local piracy/patrol/combat hooks
- player-visible market changes
- debug surfaces for recent transactions
- station service request/result execution
- conversation/message arbitration for visible service, police, mission, and distress interactions

It should still use the same market and job data as distant sectors. The active sector is more detailed, not a separate economy.

## M12 Gameplay Pass

M6 may generate physical system definitions, but M12 needs a gameplay pass that fills systemic content around those physical systems before the playable loop is considered valid. This pass may be generated, authored, or generated-then-overridden, but the resulting data must be the same validated contracts used by hand-authored content.

Required outputs:

- markets: station market profiles, starting stock, production/consumption tags, price policy, access policy, and account IDs
- routes: route IDs, segment IDs, endpoint anchors, route tier, travel windows, route value, route security snapshots, and cargo suitability
- jurisdictions: system, station, zone, route, and restricted-area jurisdiction references with deterministic priority
- patrols: patrol zones, patrol assets, authority faction, response profiles, cooldown/budget state, and moving anchor references
- ambushes: ambush zones tied to valuable route segments, risk windows, escape targets, owning pirate group/faction, and interdiction hazard seeds
- factions: local faction presence, relationships, influence, operational state, legal authority, station ownership, and service posture
- services: station service endpoint definitions for market, repair/refuel/rearm, legal office, mission board, storage, shipyard/outfitting, vendors, and black markets where present
- missions: mission source pools, generated offer seeds, objective target pools, reward policies, legal exception policies, and turn-in service endpoints
- follow-up opportunities: validated records for mission offers, bounties, distress responses, market tips, patrol requests, smuggling offers, and story hooks created after simulation events

Generation rules:

- generated IDs are stable for the same content seed and source system ID
- generated content declares its source: authored, generated, imported, override, or debug
- authored overrides replace or patch generated records through stable IDs, not array positions
- generated market stock must be compatible with commodity/item bridge records
- generated routes must be frame-aware and resolve through station/body/gate anchors
- generated jurisdictions must not leave stations, patrol zones, or restricted routes with ambiguous authority
- generated patrol/ambush content must consume faction operational budgets when used by simulation
- generated services must resolve market, account, legal, faction, inventory, comms, and presentation-mode references
- generated mission offers must create durable mission offer/instance IDs and optional reward escrow records
- generated follow-up opportunities must validate route, cargo, security, faction, legal, service endpoint, and reward/escrow references before becoming offers
- generated route/cargo/security mission objectives must use typed objective targets for systems, stations, gates, routes, route segments, cargo manifests, jurisdictions, and security contexts

Minimum M12 playable loop:

1. a generated or authored route creates value and risk between stations
2. a market exposes buy/sell stock through a station service endpoint
3. the player can buy cargo through a market transaction, cargo transfer, and ledger entry
4. NPC traders move along the same route and affect stock through scheduled events
5. pirates or hazards can threaten the route through logical encounter records
6. patrols respond using jurisdiction, faction operational state, and reserved assets
7. police/pirate/player comms arbitrate visible messages without duplicating effects
8. fines, confiscation, surrender, cargo loss, and rewards resolve through durable records
9. mission offers can send the player through the same service, route, cargo, legal, and reward contracts
10. repair, refuel, and rearm services mutate saved ship durability/loadout resources through service result records

If any generated or authored gameplay pass output is absent, the loop may still run as a narrow test fixture, but it should not be described as the systemic M12 playable loop.

## Ship Rendering Rule

Ships render only when realized.

Unreal actors are presentation and interaction for the local slice. They are not proof that the ship exists.

Rules:

- Tier 1: rendered/interactable actor
- Tier 2: no actor, optional abstract map/sensor contact
- Tier 3: no actor, no transform, event/timer only
- stored/docked/offline ships: no actor unless in the loaded local context

The renderer/actor layer must be able to answer:

- why was this ship realized?
- what logical ship ID does this actor represent?
- what pool slot or Mass entity owns it?
- when will it demote?
- what state will be written back on demotion?

`FShipRealizationSnapshot` should include:

- ship instance ID
- current tier
- route/job/goal state
- transform and velocity in active-system frame
- health/shield/module damage summary
- cargo delta since realization
- current target/threat IDs
- comms/scan/docking state summary
- AI state tree/blackboard resume key, if used
- last authoritative simulation time

## Unreal Implementation Options

Start simple:

- C++ structs for logical traffic and trade jobs
- `UGalaxySimulationSubsystem` for persistent logical simulation
- `UStarSystemSubsystem` for active-system view
- pooled `AShipTrafficActor` for Tier 1
- debug snapshots from all tiers

Only add MassEntity when the simple model proves insufficient.

MassEntity is appropriate later if we need high-volume data-oriented updates for thousands of active-system traffic records. If adopted, use Mass LOD/Representation/ActorSpawner patterns instead of running an unrelated custom pool beside Mass ownership.

Instanced Actors and instanced static meshes are candidates for high-volume ambient representation. They are not the first choice for fully interactive combat ships that need unique collision, damage, comms, targetability, and gameplay state.

Do not make MassEntity a prerequisite for M0 or the first economy slice.

World Partition is not the answer for NPC ships or economy. It is useful for authored spatial content and large maps, not for simulating distant markets or millions of logical ships.

## Required Data

Add these later as contracts:

- `FShipTrafficInstance`
- `FTrafficRouteDefinition`
- `FTrafficJobDefinition`
- `FTradeJobInstance`
- `FShipGoalState`
- `FShipRealizationSnapshot`
- `FStationMarketState`
- `FMarketTransactionRecord`
- `FScheduledSimulationEvent`
- `FZoneInfluenceState`
- `FFactionOperationalState`
- `FEconomyTickPolicy`
- `FSectorSimulationSummary`
- `FSimulationEventRecord`
- `FTrafficRealizationProfile`
- `FGameplayTransactionRecord`
- `FGameplayTransactionJournalEntry`
- `FShipInstanceState`
- `FShipDurabilityState`
- `FShipLoadoutResourceState`
- `FShipRepairResultRecord`
- `FShipRefuelResultRecord`
- `FShipRearmResultRecord`
- `FReputationDeltaRecord`
- `FFactionRelationshipDeltaRecord`
- `FFactionOperationalDeltaRecord`
- `FMessageArbitrationResultRecord`
- `FGeneratedFollowupOpportunityRecord`

## Debug Requirements

Required debug views:

- total logical ships by tier
- realized actor count and budget
- promotion/demotion counts
- blocked promotions and reason
- promotion backlog age
- pool occupancy by ship class/role
- per-frame realization time in ms
- Tier 2 update time in ms
- emergency cull count
- per-ship logical ID, route/job ID, tier, and actor pointer if realized
- traffic profile radii and budgets
- distant sector queued event count
- events processed this frame and capped/skipped count
- next economy event timestamp
- station stock and price history
- recent market transactions
- account balances, ledger entries, pending escrow holds
- catch-up work performed after save/load
- faction influence by zone and last influence causes
- faction operational state, alert level, patrol budget, and reserved assets
- recent criminal/piracy events
- generated gameplay pass summary and validation issues

Hard invariants:

- realized actor count must never exceed `MaxRealizedActors`
- no promotion may occur without a logical ship ID
- no released actor may remain registered as a navigation/comms/combat target
- no distant event may apply twice across save/load
- no stock value may go below zero
- no account balance may change without a ledger entry
- no generated service endpoint may execute without resolving account, legal, faction, comms, and inventory references needed by its action
- active actor count, Tier 2 work, and distant-event processing must be visible to automated tests or debug dumps

Debug output must make it obvious that distant ships and markets still exist even when no actors are rendered.

## Implementation Rule

Preserve the Godot tier model and economy intent.

Replace the implementation with Unreal-native ownership:

- logical simulation in subsystems
- authored definitions in Unreal assets/data contracts
- active-system representation in world subsystems
- actors only for the local interactive slice
- no hard-coded route/station arrays
- no scene paths as identity
- no economy behavior hidden inside rendered actors
