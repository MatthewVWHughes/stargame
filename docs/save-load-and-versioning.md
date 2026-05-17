# Save, Load, And Versioning

Save/load is the durable boundary between a playable session and the game simulation. It lets the player leave and return without losing ship state, location, market changes, discovered routes, pending AI events, faction/legal consequences, or the authoritative simulation time that makes moving systems predictable.

The save file is not a snapshot of the Unreal world. It is a structured record of stable gameplay IDs, logical frames, simulation clocks, event watermarks, and mutable gameplay state. Actors, components, object paths, transient registries, and cached transforms are rebuilt from data during load.

## Goals

The save system must support:

- starting a new game from a start profile
- continuing a saved session
- restoring the player ship in a valid system, frame, docking state, or respawn state
- preserving authoritative simulation time for orbits, routes, markets, AI, and events
- catching up elapsed simulation work exactly once
- rejecting incompatible development saves before they corrupt runtime state
- upgrading compatible saves through explicit version steps
- testing save/load behavior without relying on PIE-only actor names or scene layout

## Ownership

`UStargameSessionSubsystem` is the public save/load coordinator for the first implementation.

It owns:

- current session identity
- active save slot name
- new game, save, load, and continue commands
- `FSaveGameData` envelope creation and validation
- top-level load ordering
- user-facing save/load result codes and error text

`UStargameSessionSubsystem` may delegate serialization details to a helper such as `UStargameSaveSubsystem` later, but only one system may own the top-level envelope contract. Other systems contribute section snapshots through explicit C++ APIs. They do not write their own independent `USaveGame` objects.

Required section owners:

| Section | Owner | Notes |
| --- | --- | --- |
| `SessionState` | `UStargameSessionSubsystem` | start profile, current system, pending transition, slot metadata |
| `ShipLocation` | player ship state owner plus session coordinator | logical frame, anchor IDs, docking state, velocity frame |
| `PlayerState` | player profile/inventory owner | pilot identity, account IDs, discovered UI preferences that are gameplay state |
| `DiscoveryState` | catalog/session owner | known systems, known routes, scanned targets |
| `EconomyState` | `UGalaxySimulationSubsystem` or economy subsystem | markets, stock, prices, production timers |
| `CreditLedgerState` / `EscrowState` | economy or finance subsystem | account balances, ledger entries, escrow holds, idempotency keys |
| `GameplayTransactionState` / `CommitJournalState` | transaction coordinator | prepare/commit/recovery records, ordered side effects, idempotency keys |
| `NpcSimulationState` | `UGalaxySimulationSubsystem` or traffic simulation subsystem | logical ships, groups, goals, pending events |
| `ScheduledAIEvents` | event queue owner | durable events sorted by time and ID |
| `ProcessedEventWatermarks` | event queue owner | per stream/per system processing fences |
| `PerSystemRngStreams` | simulation owner | deterministic stream IDs and counters |
| `FactionState` / `FactionOperationalState` | faction subsystem | reputation, relationships, assets, influence, budgets, delta records |
| `LegalState` / `EnforcementState` | legal subsystem | crimes, permits, evidence, enforcement actions, fines, confiscations |
| `MissionState` | mission subsystem | offers, accepted missions, typed objective state, rewards, legal exceptions, follow-up validations |
| `StationServiceState` | station service subsystem | pending service requests/results, repair/refuel/rearm results, service endpoint runtime state |
| `ConversationState` / `MessageLogState` | comms subsystem | message log, conversation instances, delivery arbitration policies/results |
| `ShipInstanceState` / `ShipDurabilityState` / `ShipLoadoutResourceState` | ship ownership/equipment subsystem | persistent ship instances, hull/module condition, fuel/ammo/resource quantities |

Blueprints may request save/load commands and display save status. Blueprints must not mutate save-critical fields such as current system, simulation time, ship frame, docking state, event watermark, market state, or route progress.

## Save Envelope

The persistent file is a `USaveGame` subclass containing one `FSaveGameData` envelope.

Credits are ledger/account state, not player-state state. `PlayerState` may store the player's primary account ID and derived display caches, but the durable balance source of truth is `CreditLedgerState` / account state. Save validation must fail if a persisted player credit cache diverges from the account/ledger result it claims to display.

Required envelope fields:

- `SaveFormatVersion`
- `GameContentVersion`
- `BuildCompatibilityId`
- `CreatedUtc`
- `LastSavedUtc`
- `SlotName`
- `SessionId`
- `StartProfileId`
- `SessionState`
- `SimulationClock`
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

The narrow startup save is intentionally smaller than the full envelope:

- `SaveFormatVersion`
- `GameContentVersion`
- `BuildCompatibilityId`
- `SystemId`
- `SpawnZoneId`
- `SelectedTargetId`, optional

Startup load restores by rebuilding the saved system and placing the player at the saved spawn zone. The active save path also supports `FShipSaveLocation` for free flight, docked state, gate arrival, and respawn.

## Version Policy

Use three separate compatibility values.

`SaveFormatVersion` is the serialized schema version. It changes when fields are added, removed, renamed, retyped, or when section meaning changes.

`GameContentVersion` is the authored data compatibility version. It changes when stable IDs, fixture data, market definitions, station IDs, docking port IDs, routes, item definitions, factions, or mission definitions change in a way that affects existing saves.

`BuildCompatibilityId` is a development invalidation token. It may be a string, GUID, or monotonic integer generated from project settings. It exists so unstable development builds can reject old save slots without pretending an upgrade path exists.

Rules:

- Production saves may only load through explicit version upgrade functions.
- Development saves may be rejected on `BuildCompatibilityId` mismatch.
- No load path may silently substitute `sol`, a default spawn, or a best-effort actor scan.
- Unknown required stable IDs fail the load before actors are spawned.
- Optional sections can be absent only when the version upgrade step supplies a deterministic default.
- Version upgrades run on data only, before any system build or actor spawn.
- Each upgrade step must be deterministic and individually testable.

Recommended C++ shape:

```cpp
enum class ESaveLoadResult : uint8
{
    Success,
    SlotMissing,
    ReadFailed,
    UnsupportedVersion,
    DevelopmentSaveInvalid,
    ContentVersionMismatch,
    ValidationFailed,
    SystemBuildFailed,
    RestoreFailed
};

struct FSaveVersionHeader
{
    int32 SaveFormatVersion = 0;
    int32 GameContentVersion = 0;
    FString BuildCompatibilityId;
};
```

## Development Save Invalidation

Development save invalidation must be loud and early.

On load:

1. read only the save header
2. compare `SaveFormatVersion`, `GameContentVersion`, and `BuildCompatibilityId`
3. return `DevelopmentSaveInvalid` or `UnsupportedVersion` before deserializing section payloads that may no longer match
4. log the slot, expected version, found version, and reason
5. leave the current runtime session unchanged

The UI/debug surface should show the invalid slot and reason. It should offer a new game path, not an implicit substitution.

Do not delete invalid development saves automatically. Keep deletion as an explicit user/debug command so failed compatibility checks remain inspectable.

## Authoritative Simulation Clock

There is one authoritative gameplay clock for saved simulation state. It belongs to the persistent session or galaxy simulation owner, not orbit components, station actors, AI controllers, market widgets, or local visual effects.

Required clock snapshot:

- `AuthoritativeSimulationTimeSeconds`
- `TimeScale`
- `ClockOwner`
- `RngStreamId`
- `RngCounter`
- `ProcessedEventWatermark`

Load restores this clock before any orbit, route, market, AI, docking, or player-placement evaluation. The active system reads this time to evaluate frames and cached transforms. It does not advance the authoritative clock during load.

Visual-only presentation time may exist, but it must not feed gameplay prediction, route sampling, saves, economy, AI, or docking restoration.

## Time Advancement Policy

Load does not automatically advance authoritative time just because wall-clock time passed while the game was closed. Offline advancement is an explicit policy chosen by the session or game mode.

Required fields:

- `SavedUtc`
- `LastAuthoritativeSimulationTimeSeconds`
- `OfflineCatchUpPolicy`: none, capped_elapsed, full_elapsed, scripted
- `MaxOfflineCatchUpSeconds`, optional
- `RequestedCatchUpTargetTimeSeconds`
- `AppliedCatchUpThroughTimeSeconds`

Default development policy is `none`: continue from the saved authoritative time and run no wall-clock catch-up. When offline catch-up is enabled, the target time is computed once from policy, clamped if required, stored in the temporary load envelope, and processed idempotently before the active system is built. UI time, real UTC, and platform clock drift must not directly tick markets, AI, docking, or routes.

Catch-up frame and route queries:

- catch-up uses the data-only/headless orbit-route-frame query service from `system-data-contracts.md`
- the query service is created from validated definitions, temporary upgraded save data, and the restored clock snapshot before actor build
- route travel, NPC arrival, docking reservation expiry, gate arrival validation, gravity-well route estimates, and moving-target event placement must not require active-system actors
- query caches created during catch-up are scoped to the load operation and keyed by system ID, simulation time, and definition/content version
- after catch-up commits, active-system build may seed its frame cache from the final data-only query results but must revalidate against the same authoritative time

## Event Watermarks

Every durable simulation event stream needs a stable ordering key and a processed watermark.

Event records must include:

- stable `EventId`
- `SystemId` or stream ID
- event type
- scheduled time
- payload reference or inline payload
- RNG stream ID and counter
- processing state

Watermarks must include:

- `SystemId` or stream ID
- last processed event ID
- last processed simulation time

The event owner persists pending events and processed watermarks in the save envelope. The load path must rebuild event queues from saved records, apply watermarks, and reject duplicate event IDs before catch-up begins.

## Idempotent Catch-Up

Catch-up is the simulation work needed when a saved game loads at time `T` and the game needs markets, AI, production, travel, and legal consequences to be coherent at a later authoritative time.

Catch-up must be idempotent: retrying load, crashing mid-load, or realizing an event near the player must not apply the same trade delivery, market delta, distress call, interdiction, crime record, or AI outcome twice.

Required rules:

- Process events in `(ScheduledTimeSeconds, EventId)` order.
- Check the relevant processed watermark before applying each event.
- Event application writes deterministic result records before advancing the watermark.
- Transaction-scoped event application writes or resumes gameplay transaction prepare/commit/recovery records before applying ordered side effects.
- Market, faction, legal, and NPC side effects must reference the source `EventId`.
- If an event has already produced a result record, reload uses that record instead of recomputing a new outcome.
- RNG streams persist both stream ID and counter.
- Catch-up may run in bounded chunks, but each committed chunk must leave watermarks and result records consistent.
- Events that become locally realized keep the same `EventId`; actor realization does not create a second logical event.
- Any prepared or partially committed transaction must recover before new due events are processed.

Safe catch-up sequence:

1. load and validate data-only envelope
2. restore authoritative clock snapshot
3. rebuild event queues, result indexes, transaction indexes, and commit journal indexes
4. recover prepared or partially committed transactions
5. process due events up to the requested catch-up time
6. commit updated watermarks, transaction records, result records, markets, faction/legal state, and logical ship states
7. validate final ship locations, route states, and gate-arrival frames with the headless query service
8. build the active system view
9. realize nearby actors from already-updated logical state

Docked ships and port occupancy during catch-up:

- `FDockingPortRuntimeState` is part of station/system mutable state, not a ship location field.
- `FDockingOperationState` is per ship and references station/port IDs.
- A ship saved as `station_docked` pins that port as occupied by its ship instance through catch-up unless an explicit due event undocks, transfers, destroys, or relocates that same ship.
- NPC reservations may expire during catch-up; occupied ports do not expire by reservation timeout.
- If two catch-up events would occupy the same port, deterministic event ordering decides the first result and the second event records a failed/redirected docking outcome.
- Player docked occupancy is validated after catch-up against the saved ship instance ID. A mismatch fails load unless a version/content upgrade has explicitly moved the player.

## Load Ordering

Load must be explicit because orbits, docking ports, markets, AI events, and player placement depend on one another.

Required order:

1. **Read Header:** load slot metadata and version fields only.
2. **Validate Versions:** reject unsupported or invalid development saves.
3. **Deserialize Envelope:** read data into temporary structs, not live subsystems.
4. **Upgrade Data:** run version upgrade steps on the temporary envelope.
5. **Validate IDs:** verify current system, anchors, station IDs, docking ports, routes, markets, ship IDs, event IDs, and required asset references.
6. **Restore Clock:** set authoritative simulation time, RNG streams, and event watermarks.
7. **Create Headless Frame Query:** construct the data-only orbit/route/frame query service from validated definitions and temporary state.
8. **Recover Transactions:** rebuild gameplay transaction indexes, complete or compensate prepared/partial commits, and restore ordered side effect records.
9. **Catch Up Logical Simulation:** process due markets, AI events, production, travel, legal, and faction changes idempotently using headless frame/route queries.
10. **Validate Final Logical Frames:** resolve saved player location, traffic locations, route progress, docking references, and gate-arrival frames in data before actors exist.
11. **Build Active System:** load the saved `SystemId`, build bodies, stations, gates, spawn zones, registries, and navigation targets.
12. **Resolve Orbit And Frames:** evaluate bodies, stations, gates, computed anchors, route endpoints, and local frame caches at authoritative time.
13. **Restore Markets:** attach active-system market views to already-loaded persistent market state.
14. **Restore Docking State:** resolve station and docking port IDs, validate occupancy/reservation, and compute live docked transform if the player is docked.
15. **Restore Player Ship:** spawn or possess the ship, apply logical location, velocity frame, flight mode, cargo/loadout/resource state, durability state, and selected target.
16. **Restore Conversations:** restore active conversations and message arbitration result state before presenting queued UI.
17. **Restore Local AI View:** realize only nearby eligible logical ships, contacts, pending events, and encounters.
18. **Publish Ready State:** fire load-complete events, enable player input, refresh HUD/debug views, and allow normal ticking.

No subsystem should tick normal gameplay during steps 1 through 17. Load runs in a guarded state where systems can build data and actors but cannot independently advance simulation time.

## Save Ordering

Save should collect a coherent data snapshot, not whatever each actor happens to contain mid-tick.

Required order:

1. pause or fence save-critical simulation writes
2. read authoritative simulation clock once
3. flush in-progress logical event commits
4. flush or recover in-progress gameplay transactions through the commit journal
5. demote or snapshot realized AI ships into logical state
6. snapshot player ship logical location, docking state, ship instance state, durability, and loadout resources
7. snapshot markets, inventory, equipment, missions, faction deltas, legal, discovery, services, conversations, arbitration results, and message state
8. snapshot pending events, result records, transaction records, commit journal records, watermarks, and RNG streams
9. validate the assembled envelope
10. write to a temporary slot/file
11. atomically promote the temporary write to the requested slot
12. release the simulation write fence

The save file must not contain actor pointers, component pointers, object names, transient registry handles, cached actor-space transforms, widget state, or pooled actor lifecycle state.

## System Build And Frames

System build reconstructs runtime state from definitions and saved instances.

Rules:

- `SystemId` selects the definition or fixture.
- Stable IDs select bodies, stations, gates, spawn zones, docking ports, markets, and navigation targets.
- Active registries are rebuilt; registry handles are never loaded from save.
- Orbit and computed-anchor transforms are evaluated from authoritative simulation time.
- Actor-space transforms are derived late and are never the canonical saved location.
- Missing required anchors fail load before player possession.

For `FShipSaveLocation`, restore by mode:

| Location Mode | Restore Behavior |
| --- | --- |
| `free_flight` | resolve coordinate frame and anchor, validate saved local-bubble metadata if present, re-center/recompute the bubble if stale, then project position, rotation, velocity frame, and flight mode into actor space |
| `station_docked` | resolve station and docking port, wait for live station frame, attach/position ship at docked transform |
| `gate_arrival` | resolve arrival gate frame, apply arrival offset after gate anchor is valid |
| `respawn` | resolve respawn profile or spawn zone and create a clean ship state according to insurance rules |

Gate-arrival restore also restores kinematics. The destination gate resolved velocity plus authored gate-local arrival relative velocity becomes the ship's starting logical velocity. The load path remains in `gate_arrival` mode until the arrival safety window completes; it must not downgrade to `free_flight` before gate frame validation succeeds.

## Docking Restore

Docking is a saved logical state, not attachment to an actor.

Saved docked state must include:

- `SystemId`
- `StationId`
- `DockingPortId`
- player ship instance ID
- `DockingOperationId`
- per-ship docking operation state
- referenced port occupancy/reservation state version or timestamp
- every persisted player credit display/cache value matches the canonical account/ledger state, or is discarded and rebuilt as non-authoritative presentation state
- relevant cargo/service/repair transaction state
- authoritative simulation time

On load:

1. build the system
2. evaluate station frame at authoritative time
3. resolve docking port definition and occupancy state
4. validate the saved ship is allowed to occupy the port
5. place or attach the ship using the live docked transform
6. restore station services, market view, and local UI state from persistent data

If the port no longer exists because the content version changed, the load must fail or run an explicit content upgrade. It must not choose the nearest port silently.

## AI Events And Markets

AI events and markets are persistent logical simulation, even when their actors are not loaded.

Markets must save:

- market ID
- station/system owner IDs
- stock levels
- price state or deterministic price inputs
- production/consumption timers
- pending transaction/event references
- last update simulation time

AI simulation must save:

- logical ship and group IDs
- current tier
- goal state and saved goal
- route progress and motion state
- pending encounter events
- contacts and distress records that affect gameplay
- event watermarks and RNG counters

Ship instance state must save:

- ship instance ID
- ship archetype ID
- owner ID
- active, stored, docked, transfer, or destroyed state
- hull, shield, armor, and module durability
- equipped modules, hardpoints, utility slots, cargo modules, and modifiers
- fuel, ammo, charges, and other loadout resource quantities
- pending repair/refuel/rearm service request and result references
- last service transaction ID and source event ID

Mission, service, and conversation state must save:

- active mission instances and typed objective state for route, gate, cargo, and security objectives
- mission reward ledger/escrow and turn-in transaction result IDs
- generated follow-up opportunity and validation result records
- station service request/result records, including repair/refuel/rearm result records
- active conversations, message logs, arbitration policies, and arbitration result records

On load, markets and AI catch up before the active system realizes actors. The active station market UI reads the persistent market state after catch-up. A nearby NPC actor is spawned from the already-updated logical ship state, not from stale saved actor data.

## Implementation Checklist

The save/load implementation must provide:

1. `USaveGame` envelope and version header validation.
2. Development invalidation through `BuildCompatibilityId`.
3. Startup save/load for `SystemId`, `SpawnZoneId`, and optional `SelectedTargetId`.
4. Guarded load state so normal ticks cannot advance simulation during restore.
5. `FSimulationClockSnapshot` persistence.
6. Event queue and processed watermark persistence.
7. Idempotent catch-up for markets and scheduled AI events.
8. `FShipSaveLocation` for free flight.
9. Docked restore ordering.
10. Gate arrival and respawn restore paths.
11. Production save upgrade steps when versions change.
12. Gameplay transaction prepare/commit/recovery journal persistence.
13. Ship instance durability/loadout resource persistence and service result restore.
14. Systemic save/load tests for mission, legal cargo, services, arbitration, and faction/reputation deltas.

## Test Matrix

| Area | Test |
| --- | --- |
| Startup save/reload | Save `frontier_test_01`, `spawn_deep_space`, and selected target; reload returns to the same system and target without substituting Sol |
| Header validation | Invalid `BuildCompatibilityId` returns `DevelopmentSaveInvalid` and leaves the current session unchanged |
| Unsupported version | Future `SaveFormatVersion` returns `UnsupportedVersion` before system build |
| Version upgrade | Old compatible envelope upgrades in memory and validates before actors spawn |
| Missing system | Unknown saved `SystemId` fails load before player possession |
| Missing anchor | Saved ship anchor ID missing from system definition fails load |
| Clock restore | Same authoritative time produces same body/station/gate transforms after reload |
| Tick guard | No orbit, market, AI, or event tick advances time during load ordering |
| Event watermark | A processed event does not apply again after reload |
| Pending event | A pending interdiction/trade arrival remains pending if its scheduled time is still in the future |
| Catch-up | Due market and AI events apply once in `(ScheduledTimeSeconds, EventId)` order |
| Headless frame query | Catch-up can resolve moving route, station, gate, and gravity-well frames before active-system actors exist |
| RNG persistence | Catch-up outcomes match after save/load when RNG stream ID and counter are restored |
| Market restore | Station market stock and price state reload before station UI reads it |
| Free-flight restore | Player ship restores frame, anchor, transform, velocity frame, and flight mode |
| Docked restore | Reload while docked waits for system build, station frame, and docking port resolution before placing the ship |
| Gate arrival | Reload during gate arrival resolves the destination gate frame before applying arrival offset |
| Gate arrival velocity | Reload during gate arrival restores gate-relative velocity without using a default spawn velocity |
| Respawn | Respawn load uses saved respawn/insurance state and does not reuse invalid destroyed-ship transform |
| Actor independence | Save file contains no actor paths, component pointers, registry handles, or pooled actor state |
| Duplicate IDs | Duplicate event IDs, market IDs, or stable object IDs fail validation |
| Atomic write | Failed save write does not corrupt the previous valid slot |
| Transaction prepare | Saving with a prepared transaction and no side effects reloads without applying ledger, cargo, mission, or service mutations |
| Transaction partial commit | Saving after a ledger side effect but before later ordered side effects resumes or recovers the same transaction without duplicate ledger entries |
| Transaction recovery | A prepared or partial market/service/mission transaction writes one recovery record and leaves journal entries in a deterministic state |
| Active mission turn-in | Reload during an active mission turn-in resolves through the same service request, mission state, ledger/escrow records, and commit journal |
| Repeated reward | Repeating mission turn-in input or reprocessing the same event does not create duplicate rewards, reputation deltas, permits, cargo removals, or messages |
| Illegal cargo rejection | Illegal cargo buy/sell/mission delivery is rejected through legal/access validation before stock, cargo, or ledger state mutates |
| Service repair/refuel/rearm | Repair, refuel, and rearm service requests persist result records, ship durability/resource/loadout state, ledger entries, and rejection/partial reasons |
| Arbitration restore | Reload restores the selected, queued, suppressed, and expired message arbitration result without presenting a different message |
| Reputation deltas | Mission, legal, trade, and route-security outcomes persist reputation, faction relationship, and faction operational delta records and do not reapply them after reload |
| Ledger/account integrity | Save validation rejects non-conserving ledger entries, negative balances, duplicate escrow transitions, and player credit caches that do not match account state |
