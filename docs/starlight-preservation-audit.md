# Starlight Preservation Audit

This document answers a deletion-risk question: if the Godot `starlight`
project were deleted, would Stargame lose context, shape, or ideas that are not
yet recorded in the Unreal docs?

Short answer: yes. The current Stargame docs preserve the strategic direction,
Freelancer-inspired player loop, Unreal-native architecture, stable data
contracts, and high-level systemic design. Starlight still contains concrete
authored content, tuning values, UI behavior, tests, visual references, asset
conventions, and station kit rules that are not fully migrated.

Follow-up capture: the concrete reconstruction summary lives in
`starlight-recreation-reference.md`, with exact copied Starlight fixtures under
`docs/archive/starlight/`.

Do not delete Starlight until every item in this audit is either migrated into
Stargame docs, converted into Unreal data/assets, covered by Unreal tests, or
explicitly marked discardable.

## Authority Boundary

Stargame docs remain the authority for the Unreal version. Starlight is evidence
of proven player-loop behavior and old design intent, not a direct-port mandate.

Preserve from Starlight:

- Durable player-loop behavior that matches the understanding doc.
- Content identity, IDs, data schemas, tuning values, and tests that could be
  useful as fixtures or reference data.
- Visual, station-kit, and asset-production rules that are not yet represented
  in Unreal.
- Design decisions that explain why the game feels like a Freelancer-inspired
  space game rather than a generic space framework.

Do not preserve as binding authority:

- Godot node paths, scene ownership, C# service shapes, or Godot-specific
  lifecycle assumptions.
- Sol as the default foundation target. Sol and Alpha Centauri are legacy
  reference data unless intentionally revived.
- Exact numbers as required final balance. Treat them as known working
  prototype values until Unreal-specific tuning replaces them.

## Already Preserved

The current docs already capture the high-level shape:

- A single-player, Freelancer-inspired space game with approachable flight,
  trading, missions, combat, reputation, upgrades, and physical station
  interiors.
- The Unreal rewrite direction: C++ owns runtime contracts, Blueprint is used
  for presentation and authored assembly, and data assets own stable gameplay
  IDs.
- The active foundation target: one fiction-first frontier sector, not a broad
  sector network and not a Sol-first default.
- Core architecture for active systems, generated systems, gates, body/station
  frames, save/load, docking, supercruise, traffic tiering, encounters, markets,
  services, missions, comms, inventory, equipment, law, and factions.
- The rule that Starlight is behavioral evidence and source intent, not an
  override for Stargame's Unreal-native contracts.

The gap is mostly concrete implementation memory: exact catalogs, values,
schemas, source tests, and visual/asset rules.

## Critical Loss Categories

| Area | Starlight source | What would be lost | Preservation target |
| --- | --- | --- | --- |
| Flight feel and supercruise tuning | `game/scripts/ship/ShipController.cs` | Cursor-steering implementation, normal thrust/strafe/speed values, supercruise spool/recovery, deep/near speed ceilings, SOI slowdown formulas, interdiction and phase-scrambler behavior, and station assist thresholds. | Unreal flight profiles and tests; docs appendix for reference values. |
| Docking, launch, and hostile lockout | `game/scripts/game/space/GameplayRoot.Docking.cs`, `GameplayRoot.cs` | Comms range, approach/final docking rail numbers, station-local handoff, launch placement, undock no-autopilot bug context, and hostile docking-block rules. | `flight-supercruise-docking.md`, docking data, and automation. |
| Gate travel and arrival | `GameplayRoot.cs`, `JumpGateController.cs`, `LagrangeAnchor.cs`, `LagrangeAnchorTests.cs` | Three-frame transition readiness, arrival gate resolution, offset placement, velocity reset, Lagrange anchor tests, and legacy gate behavior. | Data-driven Unreal gate tests and legacy reference notes. |
| System/orbit data | `game/data/systems/sol.json`, `alpha_centauri.json`, `StarSystemDefinition.cs`, `OrbitalBody.cs`, `OrbitalBodyTests.cs` | Concrete Sol and Alpha Centauri body IDs, orbit values, station anchors, gate IDs, texture paths, and schema examples. | Archived import fixtures or Unreal data assets with validation tests. |
| System and sector maps | `SystemMap.cs`, `SectorMap.cs`, `PlanetOverlay.cs`, `FlightHud.cs` | Square-root system-map projection, zoom/click radii, label behavior, route display, real-star sector list, BFS route display, overlay dots for distant bodies, and HUD targeting details. | UI reference docs and later widget/map tests. |
| Save schema and validation | `SaveService.cs`, `SaveStore.cs`, `GameSession.cs`, `SaveServiceTests.cs`, `GameSessionTests.cs` | Version `5`, docked autosave path, exact saved fields, old-save rejection, corrupt JSON handling, unknown-station rejection, equipment filtering, and inventory caps. | Unreal save tests; legacy schema notes. |
| Player state, inventory, and equipment | `PlayerState.cs`, `ItemCatalog.cs`, `EquipmentSlotId.cs`, `ItemDef.cs`, `PlayerStateTests.cs`, `ItemDefTests.cs` | Starting credits/cargo/hull/shield/weapon-energy values, 20-slot inventory, cargo rules, item IDs, slot IDs, default ship equipment, and slot-fit tests. | Unreal data tables/assets and inventory/equipment tests. |
| Station and faction catalog | `StationRegistry.cs`, `StationRegistryTests.cs` | EarthHub, ArmstrongStation, MarsColony, CeresPort, DerelictOutpost, owners, roles, regions, security values, market profiles, mission tags, and quest giver positions. | Legacy content archive; optional future Sol data migration. |
| Station services and UI flow | `StationStubController.cs`, `StationStub.tscn`, `StationInteractableArea.cs`, `AirlockDoor.cs` | Physical walk-up service flow, trader/repair/launch interactions, input locking, dynamic market rows, feedback text, repair formula, and launch confirmation. | Unreal station UI tests and service transaction profiles. |
| Mission catalog and dialog | `MissionCatalog.cs`, `MissionService.cs`, `MissionDefinition.cs`, `QuestGiverController.cs`, `DialogMenuController.cs`, mission tests | Mission IDs, contact names, rewards, dependencies, objective chains, cargo quantities, Ceres encounter tuning, and written dialog. | Unreal mission data, lifecycle tests, and dialog/reference docs. |
| Economy and commodities | `EconomyService.cs`, `EconomyServiceTests.cs` | Commodity IDs, base prices, market profiles, caps/default stock, price-pressure formula, scarcity labels, and buy/sell rollback edge cases. | Unreal commodity/market data and transaction tests. |
| NPC traffic | `NpcManager.cs`, `DistantSector.cs`, `DistantSectorTests.cs` | Tier 3 distant-sector timers, 72-ship local traffic manifest, spawn/despawn bands, role speeds, convoy/formation offsets, route smoothing, and visual archetype speed factors. | Traffic profile data and tiering tests. |
| Space combat and hostile AI | `HostileEncounterController.cs`, `GameplayRoot.Combat.cs`, `SpaceCombatCommon.cs`, `SpaceProjectile.cs`, `ShipWeaponRig.cs`, `SpaceCombatMathTests.cs` | Hostile AI states, ranges, weapon/shield/energy values, interdiction lock math, recovery rewards, swept projectile collision, lead solver tests, inherited velocity, aim cones, and alternating muzzles. | Combat data profiles and automation. |
| Hostile station and on-foot combat | `HostileStationController.cs`, `StationHostile.cs`, `OnFootWeapon.cs`, `HostileStation.tscn` | Clear-room loop, four spawn markers, launch blocking until clear, player-down recovery, sidearm/hostile damage, ranges, cooldowns, and simple FPS mission flow. | Unreal station-combat profiles and tests. |
| Advanced station interior AI | `station_interior/HostileNPC.cs`, `CombatArena.cs`, `CoverManager.cs`, `CoverPoint.cs`, `PerceptionSystem.cs`, `SquadCoordinator.cs`, `FPSWeapon.cs`, station interior roadmap docs | Cover scoring, reservation, peek-side logic, hearing, attack tokens, patrol/hunt/attack/retreat states, magazines, sprint/crouch/slide/lean, and procedural station rules. | Future station-interior design docs first; tests/data once implemented. |
| Visual style and asset prompts | `game/data/factions/faction_style_catalog.json`, `placeholder_ship_prompts.json`, `docs/design/ships`, `docs/design/factions` | Faction style IDs, colors, materials, relationships, prompt prefixes, ship class sizes, negative prompts, and blockout guidance. | Visual reference docs and eventual Unreal faction/ship style assets. |
| Station kit and layout rules | `game/data/station_layouts/test_station.json`, `docs/roadmap/station_interiors/*`, `station design docs/*` | Multi-level station layout schema, room purposes, props, airlocks, 4m interior grid, 2m external grid, connector sizes, docking bay dimensions, snap helper names, material slots, and door animation names. | Station-layout fixture data, art docs, and mesh/data validation. |
| Older roadmap design detail | `docs/roadmap/02_*`, `03_*`, `06_*`, `07_*`, `09_*`, `10_*`, `13_*`, `17_*` | Build sequencing, no-ammo primary weapons, independent weapon clocks, Goto cancel semantics, jettison safety, mission slot limits, cargo scanning, countermeasures, brown dwarf mining sectors, salvage, production profiles, and broader balancing tables. | Reference docs and future subsystem tests/data as features land. |

## Highest-Risk Starlight-Only Details

These are the details most likely to be missed if Starlight disappears before
migration.

1. Concrete authored catalogs: Sol/Alpha body IDs, station IDs, gate IDs,
   faction IDs, commodity IDs, item IDs, ship archetype names, mission IDs,
   station piece IDs, and asset paths.
2. Balance and tuning values: flight/supercruise, docking, gate arrival,
   station assist, economy price pressure, repair costs, traffic speeds, hostile
   combat, projectile behavior, and station combat.
3. Executable behavior tests: save rejection/sanitization, economy rollback,
   cargo capacity, station registry integrity, mission progression, distant
   sector timers, Lagrange anchors, orbital math, and projectile intercept/hit
   math.
4. UI flow details: comms, maps, targeting, trader rows, service interaction,
   modal input locking, launch confirmation, jettison confirmation, mission
   offer/turn-in states, and docked interface readouts.
5. Visual and art rules: faction shape language, ship prompts, blockout sizes,
   station grid dimensions, docking module clearances, material slots, snap
   helpers, and theme/audio/VFX notes.
6. Future-system design seeds: mining, salvage, brown dwarfs, cargo scanning,
   countermeasures, station procedural generation, and deeper FPS station AI.

## Migration Checklist

Before deleting or archiving Starlight, complete these items.

- Create a legacy content fixture or archive for `sol.json` and
  `alpha_centauri.json`, including body/station/gate IDs and authored orbit
  values.
- Move faction style, ship prompt, commodity, item, equipment slot, station
  layout, and mission ID catalogs into Stargame docs or Unreal data.
- Preserve Starlight tuning profiles for flight, supercruise, docking, traffic,
  economy, space combat, and station combat as reference data.
- Port or replace the behavioral tests that encode edge cases: save validation,
  market transactions, cargo/equipment, mission progression, orbital/Lagrange
  math, distant-sector simulation, projectile lead, and projectile collision.
- Add a station art/reference appendix for the 2m/4m grids, connector sizes,
  docking voids, snap helper names, material slots, room purposes, and door
  animation names.
- Preserve the UI intent from the Starlight HUD/map/comms/inventory docs and
  scripts before rebuilding those surfaces in Unreal.
- Decide which Sol-first or Godot-specific content is intentionally discarded,
  and record those discard decisions explicitly.

## Practical Verdict

Deleting Starlight today would not destroy the core game direction. The
Freelancer-inspired theme, high-level loop, Unreal architecture, and current
foundation contracts are already in Stargame docs.

It would lose a significant amount of concrete design memory. The most important
loss would be authored data and executable examples: exact IDs, values, mission
content, map behavior, save edge cases, combat math, station service flow,
traffic behavior, and visual/asset rules. Those should be treated as migration
source material until captured elsewhere.

The safe policy is:

1. Keep Starlight available as read-only reference while the Unreal version
   reaches and surpasses parity.
2. Migrate concrete catalogs and tests first, because they are harder to
   reconstruct from prose.
3. Migrate design-roadmap details as reference notes only when they support the
   current understanding doc.
4. After migration, Starlight can be archived, but deletion should require a
   final pass against this checklist.
