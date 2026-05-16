# stargame

## Documentation Index

These docs describe the Unreal-native game we are building.

`docs/build-roadmap.md` owns milestone sequencing, including M0. `docs/implementation-plan-m0.md` owns the exact M0 implementation checklist and acceptance details. For M0 code work, read `docs/runtime-architecture.md` and `docs/cpp-blueprint-ownership.md` before fixture/data-contract implementation details so subsystem and Blueprint boundaries are clear up front.

Current roadmap sequencing includes M1 System Data Schemas And Registries, M5.5 Gate Transition And Arrival, M10.5 Combat, Damage, And Threat Contract, M11 Realized Space AI Slice, and M12 Playable Systemic Progression. M0 validator scope is limited here to the non-Sol startup fixture path; full validation tooling and profile mapping for M2-M8, M5.5, M10.5, and later gates remains owned by `docs/content-validation-and-tooling.md`.

| Doc | Status | Owns |
| --- | --- | --- |
| `docs/build-roadmap.md` | canonical | milestone order, milestone gates, acceptance tests, M0 validator scope |
| `docs/implementation-plan-m0.md` | canonical | exact M0 startup, fixture, save, test, and cleanup scope |
| `docs/frontier-test-fixture.md` | canonical | first non-Sol fixture IDs, provisional scale values, validation content |
| `docs/runtime-architecture.md` | canonical | subsystem ownership, catalog/route/discovery/start-selection ownership, active system lifecycle, registry rules |
| `docs/system-data-contracts.md` | canonical | field-level C++ data contracts and stable IDs |
| `docs/save-load-and-versioning.md` | canonical | save envelope, versioning, compatibility policy |
| `docs/content-validation-and-tooling.md` | canonical | validation, import, editor tooling, debug requirements |
| `docs/cpp-blueprint-ownership.md` | canonical | what must be C++ and what Blueprints may own |
| `docs/flight-supercruise-docking.md` | canonical | local bubble, frame projection, supercruise, docking behavior |
| `docs/systemic-gameplay-foundations.md` | canonical | faction, legal, inventory, market, comms, mission foundation contracts |
| `docs/simulation-tiering-and-economy.md` | supporting architecture | NPC tiers, realization, distant economy model |
| `docs/space-ai-and-dynamic-orbits.md` | supporting architecture | frame-aware NPC planning, patrol, pirate, formation behavior |
| `docs/legal-crime-system.md` | supporting architecture | player/NPC offense, evidence, wanted, jurisdiction behavior |
| `docs/future-systems-support.md` | supporting architecture | long-term game systems the early architecture must not block |
| `docs/planet-rendering-direction.md` | supporting architecture | planet/atmosphere rendering direction, not runtime ownership |
| `docs/unreal-mcp.md` | tooling reference | MCP setup and operation |
| `docs/archive/legacy-gas-giant-scale.md` | archive | removed prototype gas-giant scale notes, superseded by fixture and flight docs |
| `docs/archive/unreal-platform-research-2026-05-16.md` | archive | platform research snapshot |
| `docs/archive/legacy-godot-source-review.md` | archive | old Godot source observations, not an implementation plan |
