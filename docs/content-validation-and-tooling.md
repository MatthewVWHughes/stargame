# Content Validation And Tooling

Content validation is the build gate for Stargame's authored game data.

The game depends on stable gameplay IDs, soft asset references, fixture data, generated runtime registries, and editor-created assets all agreeing before the player enters a system. Validation exists to catch broken data while it is still cheap to fix: duplicate IDs, missing references, invalid start profiles, broken fixture slices, assets outside Asset Manager scans, and MCP-created assets that look valid in the editor but cannot be built by the game.

Validation is not a separate design tool. It is part of the game architecture. The same validation code must serve commandlets, editor utilities, automation tests, MCP workflows, and runtime development assertions.

## Core Contract

Create one C++ validation module owned by Stargame code, not by a Blueprint utility or MCP wrapper.

Required types:

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
- `Profile`: `Editor`, `Build`, `Cook`, `M0`, `M1`, `M2`, `M3`, `M4`, `M5`, `M5.5`, `M6`, `M7`, `M8`, `M9`, `M10`, `M10.5`, `M11`, `M12Gameplay`, `M12`, or `MCP`.
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
- `FSystemicGameplayValidator` for M9 factions, jurisdictions, inventory, commodity/item bridges, markets, credit accounts, ledgers, services, comms, and mission lifecycle records.
- `FLogicalEncounterValidator` for M10 logical encounters, route security snapshots, patrol reservations, interdiction hazards, distress, and idempotent event results.
- `FRealizedAISliceValidator` for M11 fixture requirements that connect logical ships, comms hooks, encounters, and actor promotion/demotion budgets.
- `FGeneratedGameplayPassValidator` for generated markets, routes, jurisdictions, patrols, ambushes, factions, services, and mission seeds.
- `FUnrealOwnershipValidator` for C++/Blueprint boundaries: canonical state must be native, mutators must go through approved C++ intent APIs, and Blueprint/StateTree/Behavior Tree assets must not own authoritative simulation writes.

Every entry point must call these validators instead of duplicating rules.

## Entry Points

### Commandlet

Add a commandlet for headless validation:

```text
UnrealEditor-Cmd Stargame.uproject -run=StargameValidateContent -ValidationProfile=Build
```

Expected profiles:

- `M0`: validate in-code fixture data, start profile, non-Sol startup contract, and exactly the M0 target slice.
- `M1`: validate all primary data assets, Asset Manager scan coverage, references, registries, and `frontier_test_01` full authored system data.
- `M2`: validate frame/location value types, deterministic orbit/frame definitions, local bubble scale values, map entries, and save locations without requiring playable supercruise or docking.
- `M3`: validate normal-flight target selection, approach targets, HUD/debug target data, and local-bubble projection inputs.
- `M4`: validate supercruise mode data, speed/dropout/lockout values, gravity-well definitions, and the distant fixture travel leg.
- `M5`: validate docking-port definitions, moving-frame docking fixtures, docking save fields, and docked restore references.
- `M5.5`: validate gate transition readiness, including `frontier_gate_a` destination system, arrival ID, catalog route edge, destination arrival system slice, and arrival transform rule.
- `M6`: validate generated physical system definitions only: stars, bodies, orbits, stations, gates, gravity wells, resource zones, spawn zones, map metadata, and generated source metadata.
- `M7`: validate AI route sampling inputs, moving endpoint prediction data, route segment stubs, and opaque jurisdiction/risk metadata without interpreting M9 legal/security semantics.
- `M8`: validate logical traffic goal state, route progress, group state, promotion/demotion harness data, and reserved non-trade goal serialization without requiring autonomous patrol/pirate/flee/fight decisions.
- `M9`: validate systemic gameplay foundations: event envelope, faction/legal records, inventory/cargo, commodity/item bridges, market transactions, credit ledger/escrow, station services, comms, and mission lifecycle hooks.
- `M10`: validate logical encounter readiness: route security, patrol reservations, encounter events, interdiction hazards, distress, abstract engagement, legal/economy/cargo outcomes, and idempotent result records.
- `M10.5`: validate combat, damage, durability, threat, and abstract attack-intent contracts for logical and realized combatants.
- `M11`: validate realized AI slice readiness: required moving endpoints, route, trader/patrol/pirate groups, distress/interdiction fixtures, low actor cap, comms hooks, and logical-to-actor promotion contracts.
- `M12Gameplay`: validate generated or authored gameplay pass data for markets, routes, jurisdictions, patrols, ambushes, factions, services, and missions.
- `M12`: run `M12Gameplay` validation, then require the named M12 automation/PIE scenario tests for progression, services, illegal rejection, encounter outcome, save/reload, idempotency, and debug trace.
- `Build`: run the strict profile required by CI and packaged builds.
- `Cook`: same as `Build`, plus any cook-only asset reachability checks.
- `MCP`: validate only the assets touched in the current MCP operation plus any owning system assets they reference.

The commandlet exits non-zero if `ResultType` is `Invalid` or `ValidationFailed`, or if policy promotes warnings to blocking for the selected profile.

Required commandlet options:

- `-ValidationProfile=<Profile>`
- `-SystemId=<id>` to limit a pass to one system.
- `-Asset=<object path>` repeatable for targeted validation.
- `-JsonReport=<path>` for CI/MCP report capture.
- `-FailOnWarnings` for release/cook hardening.

If the C++ implementation uses enum-safe names, map `M5_5` to the external profile string `M5.5` and `M10_5` to `M10.5`. Logs, JSON reports, docs, and commandlet examples should use the dotted milestone names.

### Editor Utility

Add an editor utility entry point for designers and content authors. It can be an Editor Utility Widget, toolbar command, or content browser action, but it must call the same validator service.

Required actions:

- Validate selected assets.
- Validate current system asset.
- Validate all Stargame primary assets.
- Validate `frontier_test_01`.
- Open the JSON/text report and focus the first blocking issue.

Editor validation may show warnings without blocking saves, but it must clearly mark errors as build-blocking.

### Automation Tests

Automation tests should assert the core rules directly so validation remains stable as tooling changes.

Required tests:

- duplicate entity IDs produce `Error` and include both object paths or owning field paths.
- duplicate navigation target IDs produce `Error`.
- unresolved start profile `SystemId` produces `Error`.
- unresolved `SpawnZoneId` produces `Error`.
- unresolved station, gate, body, map entry, and profile references produce `Error` once those fields are required by milestone scope.
- `frontier_test_01` passes the active M0/M1 fixture profile.
- no M0 startup validation path falls back to Sol or Sol body names.

### Runtime Development Guard

Runtime system build must refuse invalid data in development builds. `UStarSystemSubsystem::BuildSystem` should accept only a validated definition or run the strict system-level validator before spawning actors.

For development builds:

- validation errors stop the system build before actor spawn.
- the last validation report is exposed through debug output.
- a failed build returns a typed failure result, not a silent fallback.

For shipping builds:

- authored content should already be validated by commandlet/CI.
- runtime should still reject impossible or corrupt data and show a controlled failure path.

## Asset Manager Scan Validation

M1 data assets must be discoverable by Unreal's Asset Manager. Validation must confirm the project scan settings cover every required primary asset type and package path.

Required checks:

- `UStarCatalogAsset`
- `UStartProfileAsset`
- `UStarSystemDefinitionAsset`
- `UBodyVisualProfileAsset`
- `UAtmosphereProfileAsset`
- `UStationProfileAsset`
- `UGateProfileAsset`
- `UShipArchetypeDataAsset`

For `UShipArchetypeDataAsset`, M1 validation checks only the minimal spawn/identity contract: archetype ID, ship class tags, pawn/actor class reference, movement profile ID, cargo capacity fields, durability profile, loadout/resource profile, and docking size class. Equipment progression, ship purchase, outfitting, and detailed service availability are not M1 validation concerns.

For each primary asset:

- primary asset type is expected and stable.
- primary asset ID is unique.
- package path is inside an approved content root.
- the asset can be found by Asset Manager scan before direct load.
- soft object/class references resolve when required by the selected validation profile.
- hard references are rejected unless the field explicitly allows always-loaded default assets or test-only placeholders.

Asset Manager scan failure is an `Error` in `Build`, `Cook`, and `M1`; it may be a `Warning` in `Editor` only while an asset is still being authored and is not referenced by an active system.

## Profile Scope Matrix

Validation profiles are cumulative only where a later milestone consumes an earlier contract. They must not pull later-system semantics backward into earlier profiles.

| Profile | Required scope | Explicitly out of scope |
| --- | --- | --- |
| `M0` | start-profile resolution, non-Sol boot, M0 fixture slice, target IDs, M0 save/reload fields | authored Asset Manager coverage, docking, supercruise, gate transition |
| `M1` | native schemas, empty/optional later-system containers, primary asset discovery, unique IDs, references required by M1, active registries, build/teardown/rebuild | playable docking, gravity behavior, supercruise behavior, gate transition, AI route semantics, systemic gameplay semantics |
| `M2` | coordinate frames, scale values, deterministic orbit/frame evaluation, map entries, logical save location | normal-flight acceptance, supercruise mode, docking |
| `M3` | normal-flight target/approach data and HUD/debug view-model references | supercruise, docking, gate transition |
| `M4` | supercruise speed/dropout/lockout/gravity-well data and distant-leg fixture checks | docking, gate transition |
| `M5` | docking port definitions, moving-frame docking data, docking save/restore references | gate transition |
| `M5.5` | source gate registration, destination system/arrival/catalog route edge, arrival transform rule, pending-arrival save state | generated systems, AI route/security, systemic gameplay |
| `M6` | generated physical system definitions and generated source metadata | traffic, patrol, pirate, economy, encounters |
| `M7` | moving endpoint prediction, route samples, route segment IDs, opaque jurisdiction/risk/security stub fields | legal authority, faction ownership, patrol risk, pirate risk, enforcement outcomes |
| `M8` | logical trader route progress, group state, promotion/demotion harness, reserved goal serialization | autonomous patrol/pirate/flee/fight decisions and encounter resolution |
| `M9` | systemic event, faction, jurisdiction, legal, inventory, market, services, comms, mission hooks | logical encounter resolution and actor realization |
| `M10` | zero-actor logical encounters, route security, patrol reservation, interdiction, distress, abstract engagement, idempotent outcomes | requiring spawned actors to complete an encounter |
| `M10.5` | combatant IDs, damage events, durability, threat records, abstract weapon/attack intent | actor steering and full realized AI slice |
| `M11` | realized actor promotion/demotion attached to logical records, low actor cap, comms hooks, damage/threat writeback | repeatable player progression loop |
| `M12Gameplay` | generated/authored gameplay pass data for markets, routes, jurisdictions, patrols, ambushes, factions, services, missions | scenario execution |
| `M12` | runnable progression scenarios using validated M12 gameplay data | station interiors and mature economy balancing |

`Build` maps to the strictest profile required by the current branch target plus all earlier consumed profiles. Before M5.5 lands, `Build` must not fail because `frontier_gate_a` destination fields are unset. Once M5.5 is active, `Build` includes `M5.5` and those fields become blocking.

`Cook` maps to `Build` plus cook-only asset reachability checks. `Editor` may run any profile in warning-friendly mode, but selected active fixture profiles still treat errors as blocking. `MCP` runs targeted checks for touched assets plus the owning profile of any referenced system or fixture.

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

Report format for duplicate IDs:

- code: `SGV_DUPLICATE_ID`
- severity: `Error`
- source ID: duplicated gameplay ID
- field path: namespace and field where the duplicate was found
- message: name the namespace and every owner that claims the ID

For M0, the validator must reject duplicate IDs across `ember`, `brink_watch`, `frontier_gate_a`, `spawn_deep_space`, and their navigation targets. `TargetId == source object ID` is required for M0 targets.

## Unresolved References

Required references must resolve before actors spawn.

Examples:

- start profile `SystemId` resolves to a known system definition.
- start profile `SpawnZoneId` resolves inside that system.
- station/gate/body `AnchorId` resolves when set.
- navigation target `AnchorId` resolves when set.
- gate destination fields resolve once transition validation is enabled.
- visual/profile primary asset references resolve once profile assets are required.
- map entries reference valid target IDs.
- docking ports belong to a valid station.

Report format for unresolved references:

- code: `SGV_UNRESOLVED_REFERENCE`
- severity: `Error` when the field is required by the selected profile.
- source ID: owner ID.
- reference ID: missing ID or primary asset ID.
- field path: exact referencing field.
- suggested action: create the referenced object, clear an optional field, or move the content to a later profile.

Optional references may be unset. Optional references that are set but invalid are still errors unless a profile explicitly allows work-in-progress warnings.

For M1, empty arrays and unset optional fields for later systems are valid only when the profile marks those systems inactive. For example, an empty `DockingPorts` array is valid for a station until M5, unset gate destination fields are valid until M5.5, route risk IDs are opaque until M9, and combat/threat fields are absent until M10.5. Once a later profile activates a field, unset or malformed data becomes an unresolved-reference or missing-required-field error.

## Systemic Reference Validation

M9 and later profiles must validate cross-system gameplay references before logical encounters or station services can execute.

Required M9 checks:

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

Required M10 checks:

- every route security snapshot references valid route segments and time windows.
- every patrol reservation references real patrol assets, jurisdiction, faction operational state, and expiry/cooldown policy.
- every logical encounter wraps or references a canonical simulation event.
- every interdiction hazard references a route sample, owner group, expiry, and save/load event ID.
- every distress event references source, threat, location, responding patrol assets when assigned, and generated messages.
- every abstract engagement output references cargo transfer, market transaction, offense/evidence, criminal record, faction reputation, and message IDs where those effects occurred.
- processed watermarks prevent the same encounter/event outcome from applying twice.

M10 checks must execute without spawned actors. Promotion metadata may be validated only to prove that a later realized actor can attach to an existing logical encounter/event record; M10 must fail if completing the encounter requires an actor pointer, actor transform, or promoted actor lifecycle.

Required M10.5 checks:

- every damage event has a stable event ID, source combatant ID, target combatant ID, damage type, amount, authority timestamp, and idempotency key.
- every combatant ID resolves to a player, logical NPC, or realized actor mapping allowed by the selected test.
- durability state can represent shields, hull, disabled, destroyed, surrendered, and escaped outcomes without actor-owned canonical state.
- threat records are keyed by attacker/defender IDs and include last-known frame, severity, confidence, and expiry.
- abstract attack intent records can be consumed by both logical and realized combat without inventing a second health or threat model.
- repeated damage event IDs do not apply twice after save/load.

Required M11 checks:

- the fixture contains one fast-orbiting station endpoint and one nested-moon endpoint.
- the fixture contains one valuable moving route with route security/risk state and gravity lockout interaction.
- required trader, patrol, and pirate logical ships/groups have valid goals, route segments, factions, cargo summaries, and promotion eligibility.
- the fixture resolves the named M10/M11 IDs from `frontier-test-fixture.md`: three trader ships, trader group, patrol group, pirate group, distress event, interdiction hazard, low actor budget profile, and message definitions.
- distress, scan, pirate demand, flee/surrender, and interdiction comms hooks resolve to message/conversation definitions.
- actor cap and promotion priority are configured low enough to prove budget behavior.
- realized actors can map back to logical ship IDs, group IDs, goal state, target/threat IDs, and demotion snapshots.
- pending interdiction save/reload uses the same event and hazard IDs after reload.

## Generated Gameplay Pass Validation

The generated gameplay pass is valid only when it emits the same contracts authored content would emit.

`M12Gameplay` must validate:

- markets: each station with trade service has market profile, stock, price policy, account, access policy, and commodity/item bridge coverage.
- routes: each gameplay route has stable route/segment IDs, moving endpoint anchors, tier/value/security metadata, and eligible cargo/service links.
- jurisdictions: each system/station/restricted zone/patrol zone has deterministic jurisdiction priority or explicit uncontrolled state.
- patrols: each patrol zone has authority faction, patrol assets, response profile, cooldown/budget fields, and moving anchors.
- ambushes: each ambush zone references a valuable route segment, risk window, owner group/faction, escape target, and optional interdiction seed.
- factions: each active faction has definition, relationship defaults, influence/operational state, legal authority where needed, and service/mission posture.
- services: each service endpoint resolves station, provider faction, account, inventory/market where needed, legal/access policies, comms endpoint, and supported presentation modes.
- missions: each generated mission pool resolves source service, issuer faction, objective target pools, reward policy, legal exception policy, and turn-in endpoint.
- graph completeness: at least one generated/authored loop connects route, market/service endpoint, cargo or mission objective, account/ledger records, legal context, faction/reputation outcome, message hooks, and save/load result records.

Generated output must also declare source metadata: generated seed, generator version, source system ID, generation profile, and authored override IDs. Validation must reject generated gameplay data that depends on array order, actor names, or editor-only object state.

## Fixture Validation

Fixtures are not informal examples. They are buildable test content.

`frontier_test_01` is the required architecture fixture. The fixture validator must check the active profile:

M0 profile:

- start profile `frontier_test_start` exists.
- `frontier_test_start.SystemId == frontier_test_01`.
- `frontier_test_start.SpawnZoneId == spawn_deep_space`.
- no Sol, Earth, Mars, or Luna startup dependency is required.
- system contains exactly one M0 body target: `ember`.
- system contains exactly one M0 station target: `brink_watch`.
- system contains exactly one M0 gate target: `frontier_gate_a`.
- exactly three navigation targets satisfy `bShowInHud && bCanTarget`.
- `frontier_gate_a` destination references are unset.
- player spawn zone transform is valid and comes from fixture data.

M1 profile:

- all authored primary data assets for `frontier_test_01` are discoverable through Asset Manager.
- all system entities have unique IDs.
- all required references resolve.
- minimal ship archetype data resolves spawn class, movement profile, cargo capacity, durability profile, loadout/resource profile, and docking size class.
- movement, durability, loadout, and resource profile assets exist, are Asset Manager discoverable, and contain the minimal positive/canonical fields required by `system-data-contracts.md`.
- registry build, teardown, and rebuild can run without stale entries.
- debug/map/navigation lists are sourced from registry data, not actor names.

M5.5 profile:

- `frontier_gate_a.DestinationSystemId == frontier_arrival_test_01`.
- `frontier_gate_a.DestinationArrivalId == arrival_from_frontier_gate_a`.
- `frontier_gate_a.DestinationGateId == arrival_gate_a`.
- catalog edge `edge_frontier_gate_a_to_arrival_gate_a` resolves source system, source gate, destination system, destination gate, and destination arrival marker.
- destination system `frontier_arrival_test_01` builds a minimal arrival slice and registers `arrival_gate_a` plus `arrival_from_frontier_gate_a`.
- arrival transform is resolved from the destination gate frame and configured local offset/rotation.
- invalid destination or arrival IDs block transition and leave the active system unchanged.

M7/M8 fixture profiles:

- route segment `m7_brink_watch_wayfarer_trade` resolves moving endpoints and gravity-well exclusion IDs.
- `JurisdictionId`, `SecurityRating`, and `RiskProfileId` are present where the route fixture requires them, but are treated as opaque/stub metadata until M9.
- M8 reserved patrol, pirate, flee, and fight goal records serialize and reject autonomous execution until M9/M10 inputs are available.

Fixture validation should run before acceptance tests that spawn actors. If fixture data is invalid, actor-spawn tests should fail fast with the validation report.

## MCP-Created Asset Validation

MCP may create or modify assets, but MCP is not a source of truth. MCP output must pass the same validator as manually authored content before the asset is considered usable by the game.

Ownership validation must run for MCP and milestone profiles:

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
- placeholder visual/profile assets used outside M0.
- asset path is valid but outside the preferred folder convention.
- display names missing where gameplay IDs are still valid.
- map/debug visibility omitted for content not used by the current profile.

Info only:

- asset count summaries.
- skipped optional references.
- profile-specific checks that are not active.

`Build`, `Cook`, `M0`, `M1`, `M2`, `M3`, `M4`, `M5`, `M5.5`, `M6`, `M7`, `M8`, and `M10.5` should treat all `Error` and `Fatal` issues as blocking. `Build` and `Cook` may use `-FailOnWarnings` once content stabilizes.

`M9`, `M10`, `M11`, `M12Gameplay`, and `M12` also treat all `Error` and `Fatal` issues as blocking. Warnings may remain non-blocking only for inactive authored content that is not referenced by the selected profile, fixture, or generated gameplay pass.

## M0 Workflow

M0 validation proves the non-Sol playable slice before richer data assets exist.

Expected developer loop:

1. Edit C++ fixture/provider data for `frontier_test_start` and `frontier_test_01`.
2. Run `StargameValidateContent -ValidationProfile=M0`.
3. Fix duplicate IDs, unresolved references, or incorrect target counts before opening PIE acceptance tests.
4. Run M0 automation tests.
5. Start PIE only after validation passes.
6. Confirm debug output exposes the selected start profile, current system ID, spawn zone, registered entities, navigation targets, and last validation report.

M0 may validate in-code fixture data directly. It must still produce `FStargameValidationReport` so the path can be reused by M1 asset validation.

## M1 Workflow

M1 validation proves authored data assets and registries.

Expected developer loop:

1. Add or edit primary data assets for catalog, start profiles, systems, visual profiles, stations, gates, and ship archetypes.
2. Confirm Asset Manager scan settings include the new primary asset types and content roots.
3. Run selected-asset validation from the editor while authoring.
4. Run `StargameValidateContent -ValidationProfile=M1 -SystemId=frontier_test_01`.
5. Run all Stargame content validation automation tests.
6. Run build/teardown/rebuild registry tests.
7. Commit only content that passes the M1 profile or is not referenced by active build profiles.

M1 is complete only when `frontier_test_01` can be discovered through Asset Manager, validated without blocking issues, built into runtime registries, torn down, rebuilt, and inspected through debug/MCP output without relying on actor names or editor-only state.

## M9-M11 Workflow

M9 validation proves non-actor systemic gameplay contracts before logical encounters consume them.

Expected M9 loop:

1. add or edit faction, jurisdiction, inventory, commodity, market, account, service, comms, and mission hook records.
2. run `StargameValidateContent -ValidationProfile=M9`.
3. fix duplicate IDs, unresolved references, missing commodity/item bridges, missing ledger paths, or invalid service endpoints.
4. run systemic automation tests for event idempotency, NPC offense records, cargo transfer, market transaction, service lookup, and debug reasons.

M10 validation proves logical encounters can resolve without actors.

Expected M10 loop:

1. add route security, patrol reservation, logical encounter, interdiction, distress, and abstract engagement records.
2. run `StargameValidateContent -ValidationProfile=M10`.
3. fix missing canonical event links, missing patrol assets, invalid legal/economy/cargo outcome references, or duplicate processed watermarks.
4. run logical encounter tests across save/load and player-arrival promotion boundaries.

M11 validation proves the first realized AI slice can attach actors to existing logical state.

Expected M11 loop:

1. prepare the fast-station, nested-moon, valuable-route, trader, patrol, pirate, distress, and interdiction fixture data.
2. run `StargameValidateContent -ValidationProfile=M11`.
3. fix invalid comms hooks, actor budget profile, promotion/demotion snapshots, route anchors, or pending-event state.
4. run realized AI acceptance tests with low actor cap and save/reload during interdiction.

M12 gameplay validation proves generated or authored systemic content can support the first repeatable loop.

Expected M12 loop:

1. generate or author gameplay pass data for markets, routes, jurisdictions, patrols, ambushes, factions, services, and missions.
2. run `StargameValidateContent -ValidationProfile=M12Gameplay -SystemId=<id>`.
3. fix unresolved endpoint, bridge, jurisdiction, patrol, mission, ledger, escrow, and route-security references.
4. run `StargameValidateContent -ValidationProfile=M12 -SystemId=<id>` or the equivalent automation test set.
5. run `M12_TradeRouteGateRoundTrip`, `M12_ServiceTransaction`, `M12_IllegalCargoRejected`, `M12_PirateDistressPatrolOutcome`, `M12_SaveReloadProgression`, `M12_IdempotentCompletion`, and `M12_DebugProgressionTrace` without special-case fixture code.
