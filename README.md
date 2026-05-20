# stargame

## Documentation Index

These docs describe the Unreal-native game we are building.

Start with `docs/README.md`. The active game contracts live in the domain docs: runtime architecture, system data contracts, frontier fixture, save/load, flight/docking, validation/tooling, AI, and systemic gameplay. `docs/build-roadmap.md` and `docs/implementation-plan-m0.md` are historical planning references; do not treat stale staging text there as more authoritative than the current domain contracts.

The current foundation is the authored frontier slice through the systemic progression foundation: non-Sol startup, minimal boot new/continue flow, system build, save/load, flight, supercruise, docking, gate arrival, generated-system support, logical traffic, systemic records, logical encounters, combat/threat records, realized AI hooks, and service-level progression. It is an engineering-playable foundation, not yet a complete player-facing vertical slice.

| Doc | Status | Owns |
| --- | --- | --- |
| `docs/build-roadmap.md` | historical planning | original staged build order, gates, and acceptance-test history |
| `docs/implementation-plan-m0.md` | historical planning | original startup, fixture, save, test, and cleanup scope |
| `docs/frontier-test-fixture.md` | canonical | first non-Sol fixture IDs, provisional scale values, validation content |
| `docs/runtime-architecture.md` | canonical | subsystem ownership, catalog/route/discovery/start-selection ownership, active system lifecycle, registry rules |
| `docs/system-data-contracts.md` | canonical | field-level C++ data contracts and stable IDs |
| `docs/save-load-and-versioning.md` | canonical | save envelope, versioning, compatibility policy |
| `docs/content-validation-and-tooling.md` | canonical | validation, import, editor tooling, debug requirements |
| `docs/cpp-blueprint-ownership.md` | canonical | what must be C++ and what Blueprints may own |
| `docs/flight-supercruise-docking.md` | canonical | local bubble, frame projection, supercruise, docking behavior |
| `docs/systemic-gameplay-foundations.md` | canonical | faction, legal, inventory, market, comms, mission foundation contracts |
| `docs/playable-parity-roadmap.md` | active migration plan | Godot-to-Unreal playable parity slices for the non-Sol frontier build |
| `docs/simulation-tiering-and-economy.md` | supporting architecture | NPC tiers, realization, distant economy model |
| `docs/space-ai-and-dynamic-orbits.md` | supporting architecture | frame-aware NPC planning, patrol, pirate, formation behavior |
| `docs/legal-crime-system.md` | supporting architecture | player/NPC offense, evidence, wanted, jurisdiction behavior |
| `docs/future-systems-support.md` | supporting architecture | long-term game systems the early architecture must not block |
| `docs/planet-rendering-direction.md` | supporting architecture | planet/atmosphere rendering direction, not runtime ownership |
| `docs/unreal-mcp.md` | tooling reference | MCP setup and operation |
| `docs/archive/legacy-gas-giant-scale.md` | archive | removed prototype gas-giant scale notes, superseded by fixture and flight docs |
| `docs/archive/unreal-platform-research-2026-05-16.md` | archive | platform research snapshot |
| `docs/archive/legacy-godot-source-review.md` | archive | old Godot source observations, not an implementation plan |
