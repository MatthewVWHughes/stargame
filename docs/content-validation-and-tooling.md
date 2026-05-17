# Content Validation And Tooling

Content validation is the build gate for Stargame's authored and generated game data.

The game depends on stable gameplay IDs, soft asset references, fixture data, generated runtime registries, and editor-created assets all agreeing before the player enters a system. Validation exists to catch broken data while it is still cheap to fix: duplicate IDs, missing references, invalid start profiles, broken fixture slices, assets outside Asset Manager scans, and MCP-created assets that look valid in the editor but cannot be built by the game.

Validation is part of the game architecture. The same validation code serves commandlets, editor utilities, automation tests, MCP workflows, and runtime development assertions.

## Core Types

`EStargameValidationSeverity`:

- `Info`: useful context for editor reports; never blocks a build.
- `Warning`: suspicious or incomplete content; visible in editor and CI logs, but allowed only where policy says warnings are non-blocking.
- `Error`: invalid content; blocks system build, automated build, cook, and required fixture tests.
- `Fatal`: validator cannot continue safely; blocks immediately and should normally stop the current validation pass.

`EStargameValidationResultType`:

- `Valid`: no warnings, errors, or fatal issues.
- `ValidWithWarnings`: no blocking issues, but at least one warning.
- `Invalid`: one or more errors.
- `ValidationFailed`: validator could not complete because of a fatal issue, missing scan data, malformed asset, or internal validator failure.

`FStargameValidationIssue`:

- `Severity`
- `Code`: stable machine-readable code such as `SGV_DUPLICATE_ID`.
- `Message`: human-readable summary.
- `ObjectPath`: Unreal object path or package path when available.
- `SystemId`, optional.
- `SourceId`: gameplay ID that owns the issue, optional.
- `ReferenceId`: unresolved or conflicting referenced ID, optional.
- `FieldPath`: field name or nested path such as `Stations[brink_watch].NavigationTarget.TargetId`.
- `SuggestedAction`, optional.

`FStargameValidationReport`:

- `ResultType`
- `Profile`: validation profile label such as `Editor`, `Build`, `Cook`, `Startup`, `AuthoredData`, `FlightTravel`, `TrafficAI`, `SystemicGameplay`, `LogicalEncounters`, `CombatThreat`, `RealizedAI`, `PlayableProgression`, or `MCP`.
- `Issues`
- `CheckedAssetCount`
- `CheckedSystemCount`
- `CheckedFixtureCount`
- `BlockingIssueCount`
- `GeneratedAtUtc`

The report must be serializable to log text and JSON. JSON is required for CI, MCP, and editor tooling.

## Validation Ownership

Use a shared validator service with small domain-specific passes:

- `UStargameContentValidationSubsystem` or static validator library for editor/runtime callable orchestration.
- `FStartProfileValidator` for start profile IDs, required target system, spawn zone, and optional docked station/port references.
- `FStarSystemDefinitionValidator` for system-level fields, entity IDs, frame data, navigation targets, gates, spawn zones, and per-system references.
- `FStarCatalogValidator` for catalog entries, Asset Manager coverage, primary asset IDs, discoverability metadata, and generated/authored source rules.
- `FStargameFixtureValidator` for required test fixtures such as `frontier_test_01`.
- `FStargameMcpAssetValidator` for assets created or modified through MCP before those assets are considered usable.
- `FSystemicGameplayValidator` for factions, jurisdictions, inventory, commodity/item bridges, markets, credit accounts, ledgers, services, comms, and mission lifecycle records.
- `FLogicalEncounterValidator` for logical encounters, route security snapshots, patrol reservations, interdiction hazards, distress, and idempotent event results.
- `FRealizedAISliceValidator` for fixture requirements that connect logical ships, comms hooks, encounters, and actor promotion/demotion budgets.
- `FGeneratedGameplayPassValidator` for generated markets, routes, jurisdictions, patrols, ambushes, factions, services, and mission seeds.
- `FUnrealOwnershipValidator` for C++/Blueprint boundaries: canonical state must be native, mutators must go through approved C++ intent APIs, and Blueprint/StateTree/Behavior Tree assets must not own authoritative simulation writes.

Every entry point must call these validators instead of duplicating rules.

## Commandlet

Headless validation runs through the project commandlet:

```text
UnrealEditor-Cmd Stargame.uproject -run=StargameValidateContent -ValidationProfile=Build
```

Required commandlet options:

- `-ValidationProfile=<Profile>`
- `-SystemId=<id>` to limit a pass to one system.
- `-Asset=<object path>` repeatable for targeted validation.
- `-JsonReport=<path>` for CI/MCP report capture.
- `-FailOnWarnings` for release/cook hardening.

Current profile labels:

| Profile | Purpose |
| --- | --- |
| `Editor` | Warning-friendly authoring pass for selected assets or the current system. |
| `Startup` | Non-Sol start profile, source system, spawn zone, required HUD targets, and basic save/reload fields. |
| `AuthoredData` | Primary asset discovery, catalog entries, references, ship archetypes, registries, build/teardown/rebuild. |
| `FlightTravel` | Frames, scale, map entries, normal-flight approach, supercruise data, docking ports, gate transition, and pending arrival state. |
| `GeneratedPhysicalSystem` | Generated physical systems: stars, bodies, orbits, stations, gates, gravity wells, resource zones, spawn zones, map metadata, and source metadata. |
| `TrafficAI` | Route sampling, moving endpoint prediction, logical traffic, group state, promotion/demotion, and reserved goal serialization. |
| `SystemicGameplay` | Factions, jurisdictions, legal records, inventory/cargo, commodity bridges, markets, credit ledger, escrow, services, comms, and mission hooks. |
| `LogicalEncounters` | Route security, patrol reservations, interdiction, distress, abstract engagement, legal/economy/cargo outcomes, and idempotent result records. |
| `CombatThreat` | Combatant IDs, damage events, durability state, threat records, and abstract attack intent. |
| `RealizedAI` | Logical-to-actor promotion contracts, low actor cap, comms hooks, damage/threat writeback, and demotion snapshots. |
| `PlayableProgression` | Runnable progression scenarios using validated gameplay data: services, trade, illegal rejection, encounter outcome, save/reload, idempotency, and debug trace. |
| `Build` | Strict active-foundation gate required by CI and packaged builds. |
| `Cook` | `Build` plus cook-only asset reachability checks. |
| `MCP` | Targeted checks for assets touched in the current MCP operation plus owning system/catalog/start-profile references. |

The commandlet exits non-zero if `ResultType` is `Invalid` or `ValidationFailed`, or if policy promotes warnings to blocking for the selected profile.

## Editor Utility

Editor utilities may be widgets, toolbar commands, or content browser actions, but they must call the same validator service as the commandlet.

Required actions:

- Validate selected assets.
- Validate current system asset.
- Validate all Stargame primary assets.
- Validate `frontier_test_01`.
- Open the JSON/text report and focus the first blocking issue.

Editor validation may show warnings without blocking saves, but it must clearly mark errors as build-blocking.

## Automation Tests

Automation tests assert the core rules directly so validation remains stable as tooling changes.

Required tests:

- duplicate entity IDs produce `Error` and include every owning field path.
- duplicate navigation target IDs produce `Error`.
- unresolved start profile `SystemId` produces `Error`.
- unresolved `SpawnZoneId` produces `Error`.
- unresolved station, gate, body, map entry, and profile references produce `Error` when those fields are required by the selected validation profile.
- `frontier_test_01` passes the active fixture profile.
- no startup validation path substitutes Sol or Sol body names.

## Runtime Development Guard

Runtime system build must refuse invalid data in development builds. `UStarSystemSubsystem::BuildSystem` should accept only a validated definition or run the strict system-level validator before spawning actors.

For development builds:

- validation errors stop the system build before actor spawn.
- the last validation report is exposed through debug output.
- a failed build returns a typed failure result, not a silent substitution.

For shipping builds:

- authored content should already be validated by commandlet/CI.
- runtime still rejects impossible or corrupt data and shows a controlled failure path.

## Asset Manager Scan Validation

Primary data assets must be discoverable by Unreal's Asset Manager. Validation confirms the project scan settings cover every required primary asset type and package path.

Required primary asset types:

- `UStarCatalogAsset`
- `UStartProfileAsset`
- `UStarSystemDefinitionAsset`
- `UBodyVisualProfileAsset`
- `UAtmosphereProfileAsset`
- `UStationProfileAsset`
- `UGateProfileAsset`
- `UShipArchetypeDataAsset`

For `UShipArchetypeDataAsset`, validation checks the spawn/identity contract: archetype ID, ship class tags, pawn/actor class reference, movement profile ID, cargo capacity fields, durability profile, loadout/resource profile, and docking size class. Equipment progression, ship purchase, outfitting, and detailed service availability are validated by the relevant service/progression profiles.

For each primary asset:

- primary asset type is expected and stable.
- primary asset ID is unique.
- package path is inside an approved content root.
- the asset can be found by Asset Manager scan before direct load.
- soft object/class references resolve when required by the selected validation profile.
- hard references are rejected unless the field explicitly allows always-loaded default assets or test-only placeholders.

Asset Manager scan failure is an `Error` in `Build`, `Cook`, and `AuthoredData`; it may be a `Warning` in `Editor` only while an asset is still being authored and is not referenced by an active system.

## Active Build Scope

`Build` validates the current active foundation:

- start profile `frontier_test_start`.
- source system `frontier_test_01`.
- destination arrival system `frontier_arrival_test_01`.
- primary asset discovery for catalog, start profile, systems, visual/profile assets, and ship archetype/profile assets.
- unique gameplay IDs across the namespaces listed below.
- source system registries, navigation targets, map entries, frames, spawn zones, gravity wells, docking ports, gates, route segments, traffic, systemic records, encounters, combat/threat records, realized AI hooks, and progression data.
- `frontier_gate_a` destination system, destination gate, destination arrival, catalog route edge, and gate-relative arrival transform.
- generated physical-system definitions and generated source metadata.
- generated/authored gameplay pass records for markets, routes, jurisdictions, patrols, ambushes, factions, services, and missions where referenced by the active foundation.

`Cook` maps to `Build` plus cook-only asset reachability checks. `Editor` may run any profile in warning-friendly mode, but active fixture errors remain blocking. `MCP` runs targeted checks for touched assets plus owning system/catalog/start-profile references.

## Duplicate ID Rules

Duplicate gameplay IDs are always content errors when they share a namespace.

Required namespaces:

- start profile IDs
- system IDs
- body IDs within a system
- station IDs within a system
- gate IDs within a system
- spawn zone IDs within a system
- navigation target IDs within a system
- docking port IDs within a station
- map entry IDs within a system
- gravity well IDs within a system
- resource zone IDs within a system
- route segment IDs within a system
- logical ship IDs within a system
- ship group IDs within a system
- faction IDs
- faction operational state IDs by faction/system/zone
- faction relationship IDs
- reputation record IDs by owner/faction
- jurisdiction IDs
- law profile IDs
- enforcement profile IDs
- offense event IDs
- evidence record IDs
- criminal record IDs
- enforcement action IDs
- credit account IDs
- ledger entry IDs
- escrow hold IDs
- commodity IDs
- item IDs
- commodity/item bridge IDs
- container IDs
- item stack IDs within a container
- cargo transfer request/result IDs
- market IDs
- market transaction IDs
- trade job IDs
- station service endpoint IDs
- station service request/result IDs
- comms endpoint IDs
- message definition IDs
- message instance/log entry IDs
- conversation IDs
- message delivery arbitration IDs
- mission definition IDs
- mission offer IDs
- mission instance IDs
- mission objective state IDs
- mission reward definition IDs
- patrol asset IDs
- patrol reservation IDs
- route security snapshot IDs
- ambush site IDs
- interdiction hazard IDs
- distress event IDs
- logical engagement IDs
- combatant IDs
- damage event IDs
- threat record IDs

Duplicate ID issue format:

- code: `SGV_DUPLICATE_ID`
- severity: `Error`
- source ID: duplicated gameplay ID
- field path: namespace and field where the duplicate was found
- message: name the namespace and every owner that claims the ID

The startup fixture must reject duplicate IDs across `ember`, `brink_watch`, `frontier_gate_a`, `spawn_deep_space`, and their navigation targets. `TargetId == source object ID` is required for visible startup targets.

## Unresolved References

Required references must resolve before actors spawn.

Examples:

- start profile `SystemId` resolves to a known system definition.
- start profile `SpawnZoneId` resolves inside that system.
- station/gate/body `AnchorId` resolves when set.
- navigation target `AnchorId` resolves when set.
- gate destination fields resolve when transition validation is active.
- visual/profile primary asset references resolve when profile assets are required.
- map entries reference valid target IDs.
- docking ports belong to a valid station.
- systemic gameplay records resolve their referenced factions, jurisdictions, accounts, markets, services, comms endpoints, missions, legal records, and generated records.

Unresolved reference issue format:

- code: `SGV_UNRESOLVED_REFERENCE`
- severity: `Error` when the field is required by the selected profile.
- source ID: owner ID.
- reference ID: missing ID or primary asset ID.
- field path: exact referencing field.
- suggested action: create the referenced object, clear an optional field, or move the content out of an active validation profile.

Optional references may be unset. Optional references that are set but invalid are still errors unless a profile explicitly allows work-in-progress warnings.

## Systemic Reference Validation

Systemic gameplay validation checks cross-system gameplay references before logical encounters or station services can execute.

Required checks:

- every `FSimulationEventRecord` result references one canonical event and idempotency key.
- every transaction-scoped gameplay mutation has one `FGameplayTransactionRecord`.
- every prepared/committing transaction has prepare, commit, journal, or recovery records in a deterministic state.
- every transaction journal entry has a unique sequence within its transaction and references a valid ordered side effect.
- no ledger, escrow, cargo, market, service, mission, legal, reputation, faction, message, or generated-follow-up side effect may bypass the transaction journal when the action is transaction-scoped.
- every credit balance change has a `FCreditLedgerEntry`.
- every escrow hold references valid holding and beneficiary accounts.
- every market transaction references valid market, accounts, legal context, and source event or player action.
- every market transaction that moves cargo resolves a valid `FCommodityItemBridge`.
- every carried tradable commodity has exactly one canonical bridge to `ItemId`.
- every cargo transfer resolves source/destination containers, stack IDs, ownership, legality, mission locks, and capacity.
- every station service endpoint resolves station, provider faction, access policy, legal policy, account, comms endpoint where required, and presentation modes.
- every station service result references generated market, cargo, ledger, legal, mission, or message records that actually exist.
- every conversation/message response effect references valid gameplay effects and applies through a response result.
- every mission offer/instance/objective/reward/cargo flag references valid station/service/faction/container/legal/comms records for its active lifecycle state.
- every faction operational state references a valid faction and a valid system or zone when scoped.

## Encounter, Combat, And Realization Validation

Logical encounter validation runs without requiring spawned actors:

- every route security snapshot references valid route segments and time windows.
- every patrol reservation references real patrol assets, jurisdiction, faction operational state, and expiry/cooldown policy.
- every logical encounter wraps or references a canonical simulation event.
- every interdiction hazard references a route sample, owner group, expiry, and save/load event ID.
- every distress event references source, threat, location, responding patrol assets when assigned, and generated messages.
- every abstract engagement output references cargo transfer, market transaction, offense/evidence, criminal record, faction reputation, and message IDs where those effects occurred.
- processed watermarks prevent the same encounter/event outcome from applying twice.
- completing a logical encounter must not require an actor pointer, actor transform, or promoted actor lifecycle.

Combat/threat validation checks:

- every damage event has a stable event ID, source combatant ID, target combatant ID, damage type, amount, authority timestamp, and idempotency key.
- every combatant ID resolves to a player, logical NPC, or realized actor mapping allowed by the selected validation profile.
- durability state can represent shields, hull, disabled, destroyed, surrendered, and escaped outcomes without actor-owned canonical state.
- threat records are keyed by attacker/defender IDs and include last-known frame, severity, confidence, and expiry.
- abstract attack intent records can be consumed by both logical and realized combat without inventing a second health or threat model.
- repeated damage event IDs do not apply twice after save/load.

Realized AI validation checks:

- the fixture contains one fast-orbiting station endpoint and one nested-moon endpoint.
- the fixture contains one valuable moving route with route security/risk state and gravity lockout interaction.
- required trader, patrol, and pirate logical ships/groups have valid goals, route segments, factions, cargo summaries, and promotion eligibility.
- distress, scan, pirate demand, flee/surrender, and interdiction comms hooks resolve to message/conversation definitions.
- actor cap and promotion priority are configured low enough to prove budget behavior.
- realized actors can map back to logical ship IDs, group IDs, goal state, target/threat IDs, and demotion snapshots.
- pending interdiction save/reload uses the same event and hazard IDs after reload.

## Generated Gameplay Pass Validation

The generated gameplay pass is valid only when it emits the same contracts authored content would emit.

Validation checks:

- markets: each station with trade service has market profile, stock, price policy, account, access policy, and commodity/item bridge coverage.
- routes: each gameplay route has stable route/segment IDs, moving endpoint anchors, tier/value/security metadata, and eligible cargo/service links.
- jurisdictions: each system/station/restricted zone/patrol zone has deterministic jurisdiction priority or explicit uncontrolled state.
- patrols: each patrol zone has authority faction, patrol assets, response profile, cooldown/budget fields, and moving anchors.
- ambushes: each ambush zone references a valuable route segment, risk window, owner group/faction, escape target, and optional interdiction seed.
- factions: each active faction has definition, relationship defaults, influence/operational state, legal authority where needed, and service/mission posture.
- services: each service endpoint resolves station, provider faction, account, inventory/market where needed, legal/access policies, comms endpoint, and supported presentation modes.
- missions: each generated mission pool resolves source service, issuer faction, objective target pools, reward policy, legal exception policy, and turn-in endpoint.
- graph completeness: at least one generated/authored loop connects route, market/service endpoint, cargo or mission objective, account/ledger records, legal context, faction/reputation outcome, message hooks, and save/load result records.

Generated output must also declare source metadata: generated seed, generator version, source system ID, generation profile, and authored override IDs. Validation rejects generated gameplay data that depends on array order, actor names, or editor-only object state.

## Fixture Validation

Fixtures are buildable test content, not informal examples.

`frontier_test_01` is the required architecture fixture. The fixture validator checks:

- start profile `frontier_test_start` exists.
- `frontier_test_start.SystemId == frontier_test_01`.
- `frontier_test_start.SpawnZoneId == spawn_deep_space`.
- no Sol, Earth, Mars, or Luna startup dependency is required.
- startup system contains the required body target: `ember`.
- startup system contains the required station target: `brink_watch`.
- startup system contains the required gate target: `frontier_gate_a`.
- exactly three navigation targets satisfy `bShowInHud && bCanTarget`.
- `frontier_gate_a` resolves its authored destination system, gate, arrival marker, and arrival transform under active build validation.
- player spawn zone transform is valid and comes from fixture data.
- all authored primary data assets for `frontier_test_01` are discoverable through Asset Manager.
- all system entities have unique IDs.
- all required references resolve.
- minimal ship archetype data resolves spawn class, movement profile, cargo capacity, durability profile, loadout/resource profile, and docking size class.
- movement, durability, loadout, and resource profile assets exist, are Asset Manager discoverable, and contain the minimal positive/canonical fields required by `system-data-contracts.md`.
- registry build, teardown, and rebuild can run without stale entries.
- debug/map/navigation lists are sourced from registry data, not actor names.

Gate transition and arrival validation checks:

- `frontier_gate_a.DestinationSystemId == frontier_arrival_test_01`.
- `frontier_gate_a.DestinationArrivalId == arrival_from_frontier_gate_a`.
- `frontier_gate_a.DestinationGateId == arrival_gate_a`.
- catalog edge `edge_frontier_gate_a_to_arrival_gate_a` resolves source system, source gate, destination system, destination gate, and destination arrival marker.
- destination system `frontier_arrival_test_01` builds a minimal arrival slice and registers `arrival_gate_a` plus `arrival_from_frontier_gate_a`.
- arrival transform is resolved from the destination gate frame and configured local offset/rotation.
- invalid destination or arrival IDs block transition and leave the active system unchanged.

AI route and logical traffic validation checks:

- route segment `m7_brink_watch_wayfarer_trade` resolves moving endpoints and gravity-well exclusion IDs.
- `JurisdictionId`, `SecurityRating`, and `RiskProfileId` are present where the route fixture requires them.
- reserved patrol, pirate, flee, and fight goal records serialize and reject autonomous execution unless their required systemic and encounter inputs are present.

Fixture validation should run before acceptance tests that spawn actors. If fixture data is invalid, actor-spawn tests fail fast with the validation report.

## MCP-Created Asset Validation

MCP may create or modify assets, but MCP is not a source of truth. MCP output must pass the same validator as manually authored content before the asset is considered usable by the game.

Ownership validation must run for MCP and validation profiles:

- Blueprint assets may call approved intent/query APIs, but must not expose direct authoritative mutators for credits, inventory, cargo, legal state, faction state, ship location, route progress, docking state, combat state, or transaction commit state.
- `BlueprintCallable` functions that mutate canonical state must be explicitly allowlisted and route through a C++ subsystem command with validation, result records, and debug output.
- Blueprint variables must not be the canonical storage for persistent/systemic state. They may cache presentation view models only.
- Behavior Tree and StateTree assets may store object keys, target IDs, and transient intent state, but canonical goal, route, threat, encounter, and ship state remain in C++ logical records.
- Validation should flag suspicious writable Blueprint variables, direct component/property writes to canonical actors, and assets that bypass the intent API.

Required MCP workflow:

1. MCP creates or edits assets only inside approved project content roots.
2. MCP saves through the bridge's safe save path.
3. MCP runs targeted validation for every touched asset.
4. If a touched asset is part of a system, MCP also validates the owning system definition and related catalog/start profile entries.
5. MCP reports blocking issues in JSON/text form.
6. Assets with validation errors are not referenced by start profiles, fixtures, or build profiles until fixed.

MCP-created content must not bypass C++/Blueprint ownership rules. For example, a Blueprint created through MCP may provide presentation or component composition, but it must not become the canonical owner of system IDs, route state, save state, orbit state, docking state, or transition rules.

## Fail-Build Versus Warn Policy

Use the selected validation profile to decide whether warnings block.

Always fail build:

- duplicate gameplay IDs.
- missing required start profile.
- missing required system definition.
- unresolved required references.
- Asset Manager cannot discover a required primary asset.
- invalid fixture required by the active profile.
- runtime system build would spawn actors from invalid data.
- commandlet cannot complete validation.

Warn in editor, fail when promoted by profile:

- optional content fields left unset.
- placeholder visual/profile assets used outside the startup fixture.
- asset path is valid but outside the preferred folder convention.
- display names missing where gameplay IDs are still valid.
- map/debug visibility omitted for content not used by the current profile.

Info only:

- asset count summaries.
- skipped optional references.
- profile-specific checks that are not active.

`Build`, `Cook`, and all strict validation profiles treat all `Error` and `Fatal` issues as blocking. Warnings may remain non-blocking only for inactive authored content that is not referenced by the selected profile, fixture, or generated gameplay pass.

## Developer Workflows

Startup fixture workflow:

1. Edit fixture/provider data or authored assets for `frontier_test_start`, `frontier_test_01`, and `frontier_arrival_test_01`.
2. Run `StargameValidateContent -ValidationProfile=Startup` for the narrow startup fixture or `-ValidationProfile=Build` for the full active foundation.
3. Fix duplicate IDs, unresolved references, incorrect target counts, or missing gate-arrival data before opening PIE acceptance tests.
4. Run startup automation tests.
5. Start PIE only after validation passes.
6. Confirm debug output exposes the selected start profile, current system ID, spawn zone, registered entities, navigation targets, and last validation report.

Authored data workflow:

1. Add or edit primary data assets for catalog, start profiles, systems, visual profiles, stations, gates, and ship archetypes.
2. Confirm Asset Manager scan settings include the new primary asset types and content roots.
3. Run selected-asset validation from the editor while authoring.
4. Run `StargameValidateContent -ValidationProfile=AuthoredData -SystemId=frontier_test_01` for asset/registry scope or `-ValidationProfile=Build` for the full active foundation.
5. Run Stargame content validation automation tests.
6. Run build/teardown/rebuild registry tests.
7. Commit only content that passes the relevant validation scope or is not referenced by active build profiles.

Systemic and AI workflow:

1. Add or edit faction, jurisdiction, inventory, commodity, market, account, service, comms, mission, route security, patrol, logical encounter, interdiction, distress, and combat/threat records.
2. Run the matching strict validation profile or `Build`.
3. Fix missing canonical event links, missing patrol assets, invalid legal/economy/cargo outcome references, duplicate processed watermarks, invalid comms hooks, actor budget profile issues, promotion/demotion snapshot issues, route anchor issues, or pending-event state issues.
4. Run the relevant systemic, logical encounter, combat/threat, and realized AI automation tests.

Playable progression workflow:

1. Generate or author gameplay pass data for markets, routes, jurisdictions, patrols, ambushes, factions, services, and missions.
2. Run `StargameValidateContent -ValidationProfile=PlayableProgression -SystemId=<id>`.
3. Fix unresolved endpoint, bridge, jurisdiction, patrol, mission, ledger, escrow, and route-security references.
4. Run service, trade, illegal-cargo, encounter-outcome, save/reload, idempotency, and debug-trace automation tests without special-case fixture code.
