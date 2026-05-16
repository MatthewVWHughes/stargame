# Future Systems Support

This document lists the major game systems the architecture must support later, even when they are not part of M0.

The rule is: do not build these systems early, but do not make M0/M1 decisions that block them.

NPC traffic, distant sectors, and economy simulation are covered in more detail by `simulation-tiering-and-economy.md`. Space AI that operates over moving Kepler/orbital frames is covered by `space-ai-and-dynamic-orbits.md`. Legal, faction, market, inventory, comms, and mission foundation contracts are covered by `systemic-gameplay-foundations.md`.

## Core Player Modes

### Space Flight

Required architectural support:

- persistent ship identity
- current ship archetype
- ship equipment/loadout
- local flight state
- supercruise state
- damage/repair state
- cargo hold
- docking state
- current system and logical ship location

M0 only proves basic flight input still works.

### FPS Station Mode

Required architectural support:

- docked station ID
- docking port ID
- station interior/profile ID
- player body/loadout state separate from ship state
- transition from ship cockpit/docked menu to station interior
- transition back to docked ship
- station-local NPCs, vendors, quest givers, and services

Do not bake stations as only menu screens. The architecture should allow menu-only stations first and walkable interiors later.

Station menus and walkable interiors must call the same station service endpoints. FPS counters, vendors, quest givers, and legal offices are presentation/access points for `FStationServiceEndpointDefinition` and `FStationServiceRequest`, not separate transaction systems.

## Player Progression

### Inventory

Required architectural support:

- player inventory
- ship cargo inventory
- station market inventory
- equipment inventory
- item definitions
- item stacks
- ownership/location references

Inventory locations must use stable IDs: player, ship ID, station ID, container ID.

### Personal Equipment

Required architectural support:

- personal gear slots
- weapons/tools/armor
- consumables
- station/FPS-mode equipment access
- item definitions shared with inventory/economy where appropriate

### Ship Equipment

Required architectural support:

- ship hardpoints
- internal modules
- utility slots
- cargo modules
- power/heat/mass/stat modifiers
- damage and repair state
- loadout save data

Ship equipment should be data-driven and attached to ship archetypes/instances, not hard-coded on the pawn.

Saved ship equipment must include loadout resource state, not just installed module IDs. Fuel, ammunition, charges, consumables, module durability, and service-modified quantities are persistent fields on the ship instance or related durability/resource records.

### Ship Ownership And Swapping

Required architectural support:

- owned ship list
- active ship ID
- ship instance state
- purchase/sale/trade-in flow
- storage location
- transfer of cargo/equipment rules

A ship archetype is not enough. The game needs ship instances with persistent condition, loadout, cargo, and location.

`FShipInstanceState`:

- `ShipInstanceId`
- `ShipArchetypeId`
- `OwnerId`
- `DisplayName`, optional
- `LocationStateRef`
- `CargoContainerId`
- `DurabilityStateId`
- `LoadoutStateId`
- `LoadoutResourceStateId`
- `InsuranceStateId`, optional
- `LastServiceTransactionId`, optional
- `State`: active, stored, docked, transferring, destroyed, impounded

`FShipDurabilityState`:

- `DurabilityStateId`
- `ShipInstanceId`
- `HullCurrent`
- `HullMax`
- `ShieldCurrent`
- `ArmorCurrent`
- `ModuleDurabilityBySlot`
- `LastDamageEventId`, optional
- `LastRepairResultId`, optional

`FShipLoadoutResourceState`:

- `LoadoutResourceStateId`
- `ShipInstanceId`
- `FuelByResourceId`
- `AmmoByHardpointOrModule`
- `ChargeCountsByModule`
- `ConsumableCountsByModule`
- `LastRefuelResultId`, optional
- `LastRearmResultId`, optional

### Leveling

Required architectural support:

- player XP/level
- skill/perk unlocks or reputation-gated unlocks
- progression rewards
- save-game versioning

Do not let level progression leak into ship flight constants directly. Progression should modify data-driven stats through defined systems.

## World Simulation

### NPC Flight

Required architectural support:

- NPC ship identity
- persistent character/person identity where needed
- spawn zone
- origin/destination
- current job
- faction/company/pirate/police ownership
- local steering state
- despawn/resume policy
- traffic density controls

NPCs should consume the same navigation targets, stations, gates, and system data as the player.

Named NPCs, vendors, quest givers, faction contacts, police captains, passengers, and story characters need stable character/person IDs independent of any spawned ship actor. Disposable traffic can remain ship/group based.

NPC ship actors should exist only when the logical ship is near enough to the player to be interactive or visible. Distant and same-system-far traffic remains logical simulation.

Space NPC intent must be frame-aware. Patrols, formations, ambushes, flee destinations, and combat targets should resolve through moving anchors and predicted route/body/station positions, not fixed absolute world-space waypoints.

### NPC Trading

Required architectural support:

- trade job definition
- cargo manifest
- source station
- destination station
- route/risk profile
- profit/priority model
- arrival/completion events

NPC trade should affect or at least be consistent with station economy state.

### Global Economy

Required architectural support:

- commodity definitions
- station markets
- production/consumption profiles
- stock levels
- price model
- faction/company ownership
- route accessibility
- save/update cadence

The economy should be logical data first. It should not require all systems to be loaded as actors.

Distant sector economy should use scheduled events, station market state, and coarse update cadence. It should not depend on rendered NPC ships.

### Mining

Required architectural support:

- resource zone definitions
- asteroid/resource node definitions
- extraction equipment
- depletion/regeneration rules
- cargo output
- legal/faction status hooks
- NPC mining operations

Resource zones should use stable zone IDs and map/navigation entries.

### Trading

Required architectural support:

- market transaction contracts
- service access APIs
- station market IDs
- cargo validation
- prices
- legality
- faction modifiers
- mission cargo flags
- durable account/ledger/escrow records
- commodity-to-item bridge records

Trading must integrate player cargo, station market inventory, economy state, and faction/legal rules.

UI is not the support primitive. The architecture needs validated buy/sell/service transactions first; UMG screens can arrive later.

Market logic uses `CommodityId`; inventory/cargo logic uses `ItemId`. Any tradable carried good must cross through one canonical commodity/item bridge so markets, cargo holds, mission deliveries, and confiscation agree on identity.

## Stations And Factions

### Stations

Required architectural support:

- station ID
- station physical anchor
- docking ports
- owner faction
- region
- services
- market profile
- mission/quest tags
- comms endpoint
- optional interior profile
- credit/service account IDs

Separate station physical placement from station services and faction/economy identity.

Service endpoints should be addressable by stable service ID and expose the same transaction contract to:

- docked menu screens
- cockpit comms
- FPS vendor counters
- FPS dialogue with station NPCs
- remote/debug tooling

Service request/result records must fan out to market transactions, cargo transfers, credit ledger entries, legal enforcement actions, mission lifecycle updates, and message/conversation records as needed.

### Factions

Required architectural support:

- faction definitions
- faction relationships
- faction status/reputation with player
- legal status
- station/system ownership
- patrol behavior
- mission access
- market modifiers
- story/quest branching
- mutable faction operational state

Faction status must be stable save data, not UI-only state.

Faction operational state is separate from faction definition and reputation. It tracks local influence, alert level, patrol availability, service posture, trade posture, mission posture, security pressure, criminal pressure, and recent operational events so route security, police response, service access, and mission generation read one source of truth.

### Police Patrols

Required architectural support:

- patrol zones
- faction/legal authority
- scan/interdiction rules
- wanted/criminal state
- response escalation
- station/system ownership hooks

Police should be driven by faction/legal data and zone definitions.

Police patrol zones may be anchored to stations, bodies, gates, or route segments. They must move with those anchors when the underlying content is orbital.

Police scans, challenges, fines, confiscation, surrender, and interdiction need lifecycle records. A scan that begins in cockpit space, continues through a comms response, and resolves after docking or reload should still be the same enforcement action.

### Pirates

Required architectural support:

- pirate factions/groups
- ambush zones
- interdiction rules
- smuggling/risk routes
- hostility rules
- bounty hooks

Pirates should use the same ship/NPC/navigation architecture as lawful traffic, with different job policies.

Pirate ambush zones should be attached to route segments and risk profiles, not static encounter points, so piracy naturally follows valuable moving trade lanes.

## Combat

### Flight Combat

Required architectural support:

- weapon hardpoints
- projectile/hitscan definitions
- targeting
- shields/hull/armor
- damage types
- factions/hostility
- AI combat behavior
- combat zones/spawn rules
- loot/salvage hooks

Combat must work in local flight space and survive origin shifts.

### Ship Damage And Repair

Required architectural support:

- hull state
- shield state
- module damage
- repair services
- field repair items
- insurance/rebuy policy, if used

This is a likely missing system if not planned early.

Repair, refuel, and rearm are station service transactions. They must write service result records, ledger entries, and updated `FShipDurabilityState` or `FShipLoadoutResourceState` references. Service code must not directly mutate ship durability, fuel, or ammunition without a result record and transaction journal entry.

## Quests And Story

### Main Story

Required architectural support:

- quest state
- objective tracking
- system/station/body references
- dialogue/comms triggers
- faction status gates
- save/load state

Story content should reference stable IDs, not actors.

### Side Quests

Required architectural support:

- mission templates
- generated mission instances
- rewards
- failure states
- timers, if used
- station/faction/source context
- typed objectives for routes, gates, cargo manifests, security scans, and threat clearance

### Faction Quests

Required architectural support:

- faction mission pools
- reputation gates
- consequences
- faction relationship changes
- legal/criminal hooks

## Communications

### Station Communications

Required architectural support:

- station comms endpoint
- range/availability
- docking request
- landing/docking clearance
- trade/service access
- mission board access
- faction/security responses

Comms should be a system/service interface on station identity, not direct Blueprint UI calls.

### NPC/Ship Communications

Required architectural support:

- hail/response flow
- hostile warnings
- police scans
- pirate demands
- mission dialogue
- distress calls

This is a gap not explicitly listed, but it should be supported by the same comms/message architecture.

### Dialogue And Message System

This means a shared game system for authored and procedural conversations/messages, not just voiced dialogue.

It should support:

- station hails
- docking clearance
- police scan warnings
- pirate threats/demands
- mission offers
- mission objective updates
- faction quest conversations
- main story conversations
- distress calls
- news/rumors
- service/vendor greetings
- reply choices, when needed
- consequences from player choices

The message system should separate:

- message source: station, NPC ship, faction, mission, story, system event
- delivery channel: comms panel, station UI, cockpit HUD, FPS dialogue UI, notification log
- content payload: text, speaker, portrait/voice hooks, response options
- gameplay effects: quest state, faction change, legal state, docking permission, market/service unlock

This should not be a pile of direct UI calls. Gameplay systems should emit messages/events with stable source IDs. UI decides how to present them.

Conversation and delivery arbitration are required because legal, combat, docking, service, mission, and story systems can all speak at the same time.

Required arbitration support:

- active conversation state with owner, participants, node/progress, related mission/service/legal IDs, and expiry
- candidate message queue by target and delivery channel
- priority inputs for distress, police, docking, combat, mission, story, and service messages
- suppression/queue/replacement decisions with debug reasons
- response effects that apply once through message response result records
- persistence across save/load for active conversations and pending delivery decisions

The same conversation can be presented through cockpit comms, docked menus, or FPS dialogue if the source service or mission supports those presentation modes.

## Sectors And Travel

Required architectural support:

- sector catalog
- system catalog
- route graph
- discovered/known routes
- gates/wormholes
- travel requirements
- system ownership/security metadata
- map UI

Sectors must be logical data. Do not require every sector/system to be loaded in Unreal.

### Exploration, Scanning, And Discovery

Required architectural support:

- sensor scan definitions
- unknown contact state
- discoverable body/resource/route records
- survey data ownership
- codex/logbook entries
- exploration rewards
- faction/mission/legal hooks for restricted scans

Discovery state must be logical data. A body, resource zone, or route can exist in the system definition while being unknown to the player.

### Fuel, Consumables, And Travel Logistics

Required architectural support:

- fuel or energy resource definitions, if travel has cost
- jump/supercruise range constraints
- refuel services
- emergency recovery/stranding rules
- route affordability checks
- consumable degradation and replenishment

Even if M0 ignores fuel, route and service architecture should not assume travel is always free.

### Death, Failure, And Respawn

Required architectural support:

- player defeat state
- active ship destruction/loss rules
- cargo/equipment loss rules
- mission failure hooks
- legal/faction consequences
- respawn station/spawn-zone resolution
- insurance/rebuy state
- save recovery behavior

This is a cross-system contract, not just a combat feature.

## Post-M12 Playable Loop Support

After M12, the expected playable loop is not just realized ships. The architecture must support a route-to-service-to-encounter-to-resolution loop using systemic records:

1. generated or authored markets expose cargo opportunities through station service endpoints
2. generated or authored routes connect stations with route value, travel windows, and security/risk state
3. jurisdictions and faction operational state decide scans, patrol response, service access, fines, and interdictions
4. patrol and ambush zones attach to moving route/station/body anchors
5. market buy/sell, service fees, fines, bounties, and rewards use ledger/escrow records
6. cargo moves through containers using the commodity/item bridge
7. pirate, police, distress, and mission interactions use conversation delivery arbitration
8. mission offers and turn-ins use the same station service endpoint contracts in menu and FPS contexts
9. gameplay actions that touch multiple systems use one prepare/commit/recovery transaction journal
10. repair, refuel, and rearm services update saved ship durability/loadout resources through result records
11. generated follow-up opportunities validate route, cargo, security, faction, legal, service, and reward references before becoming offers

This loop can start tiny, but each step needs stable IDs and idempotent records so content can scale without replacing the foundation.

## Additional Gaps To Plan For

These were not explicit in the list, but the architecture should leave room for them:

- **Save versioning and schema upgrades:** required once inventory, economy, quests, and ship instances exist.
- **Legal/crime system:** needed for piracy, policing, smuggling, scans, faction status, and station access. See `legal-crime-system.md`.
- **Insurance/rebuy/respawn:** needed if ship destruction exists.
- **Exploration/scanning/discovery:** needed for unknown contacts, resource finding, survey data, rewards, and restricted-space gameplay.
- **Fuel/consumables/travel logistics:** needed if supercruise, jumps, repairs, or long-distance travel have costs.
- **Death/failure/respawn:** needed before combat consequences, cargo loss, mission failure, and save recovery become tangled.
- **Reputation separate from faction relationship:** player standing, criminal status, permits, and access should not be one number.
- **Permits/access control:** stations, systems, equipment, missions, and faction space may need gates.
- **Time simulation/update cadence:** global economy and NPC jobs need updates without all actors loaded.
- **Content validation:** all quests, stations, markets, factions, routes, and item references need validation.
- **Station service endpoint contracts:** menus and FPS interiors need shared service transactions, not parallel implementations.
- **Generated gameplay pass:** generated systems need systemic markets, routes, jurisdictions, patrols, ambushes, factions, services, and mission seeds before they support the post-M12 loop.
- **Gameplay transaction commit journal:** cross-system actions need prepare/commit/recovery phases, ordered side effects, and idempotent result records.
- **Ship instance resources:** repair/refuel/rearm require saved durability, loadout resource, and service result records.
- **Generated follow-up validation:** event-created missions, tips, bounties, and patrol requests need validation before they enter offer pools.

## Foundational Debug And Telemetry

Debuggability is foundational, not optional polish.

Every major system needs an inspectable debug surface from its first useful implementation. This can start as a console command, log category, Canvas HUD panel, editor utility widget, or MCP-readable data dump. It can become a proper debug menu later, but it must exist.

Required debug surfaces by system:

- session/start profile: current profile, current system, save/load status, pending arrival
- active system: build state, build generation, loaded definition, registered actors
- registries: IDs, entry types, weak actor validity, duplicate/missing references
- navigation targets: target IDs, display names, frames, distances, visibility
- coordinate frames/scale: current frame, conversion outputs, origin shifts, local bubble status
- ship flight: mode, velocity, input state, selected target, local/reference-frame location
- supercruise: state, speed cap, gravity well, lockout/dropout state, target approach telemetry
- docking: station ID, port ID, approach state, relative velocity, alignment, reservation/occupancy
- stations: services, faction, market, docking ports, comms endpoint
- economy: station stock, prices, production/consumption, recent transactions
- gameplay transactions: transaction phase, prepare result, ordered side effects, commit result, recovery result, idempotency key
- NPC traffic: spawned groups, jobs, routes, intent, despawn/resume state
- mining/resources: resource zones, node state, depletion, ownership/legal status
- combat: hostile lists, damage events, weapon state, faction hostility
- factions/legal: reputation, criminal state, patrol authority, permits/access
- faction operations: influence, alert level, service posture, patrol budget, reserved assets
- quests/story: active quests, objective state, source IDs, blockers
- inventory/equipment: containers, item stacks, equipped modules/gear, ownership
- ship services: repair/refuel/rearm result records, durability state, fuel/ammo/resource state, ledger entries
- comms/messages: active messages, source IDs, channels, responses, gameplay effects
- conversations: arbitration queue, suppressed messages, active node, related mission/service/legal records
- travel logistics: fuel/energy state, range checks, refuel services, stranding/recovery status
- exploration/scanning: contacts, scan progress, discovered IDs, survey ownership, rewards
- death/respawn: defeat reason, respawn target, insurance/rebuy outcome, cargo/equipment loss
- content validation: missing IDs, duplicate IDs, unresolved references, invalid asset refs

Rules:

- each system gets a log category
- debug views read C++ state/view models, not private Blueprint state
- debug output uses stable IDs
- debug views must be safe in PIE
- critical validation failures should be visible without digging through full logs
- MCP should be able to inspect important debug state once the system exists

## Architecture Implication

The early architecture must preserve these stable reference types:

- system ID
- sector ID
- body ID
- station ID
- docking port ID
- spawn zone ID
- ship archetype ID
- ship instance ID
- faction ID
- jurisdiction ID
- law profile ID
- enforcement profile ID
- offense event ID
- criminal record ID
- permit/access requirement ID
- commodity ID
- item ID
- item instance ID
- container ID
- equipment ID
- market ID
- transaction ID
- trade job ID
- service/vendor ID
- service endpoint/request/result ID
- mission/quest ID
- mission instance ID
- objective ID
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
- zone ID
- route ID
- comms endpoint ID
- gameplay transaction ID
- transaction journal entry ID
- ship durability state ID
- ship loadout resource state ID
- service result ID
- message arbitration result ID
- generated follow-up opportunity ID

M0 does not implement these systems. M0 only proves the first thin path: start profile, non-Sol system ID, spawn zone, navigation targets, and save/reload. Later systems must build on the same stable-ID discipline.
