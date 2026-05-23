# Playable Parity Roadmap

This is the migration checklist for getting the Unreal build to a playable baseline comparable to the Godot vertical slice. It is not a 1:1 port plan. The target is the same player loop rebuilt around the Unreal frontier fixture, not Sol.

## Baseline Rule

- Start in `frontier_test_01` through `frontier_test_start`.
- Treat Sol, Earth, Mars, and Luna as legacy reference content only.
- Prefer small usable player-facing surfaces over broad backend-only contracts.
- Keep stable gameplay IDs as the bridge between flight, stations, missions, markets, save/load, and UI.
- Source behavior from the Godot implementation first. If Unreal needs behavior that was implicit in Godot, express it as data plus deterministic query services, not one-off actor callbacks or static coordinates.
- Track every migrated gameplay area with Godot source, Unreal implementation surface, automated coverage, player-facing gap, and next implementation action.

## Godot Source Map

These Godot files/tests are the strongest Starlight behavioral references for
the migrated player loop. They inform parity work, but Stargame's docs own the
Unreal runtime/data contracts and current scope:

- `scripts/game/space/systems/StarSystemDefinition.cs`: system data is pure records. New builder behavior should be driven by fields, not `Action<>` hooks or lambdas.
- `scripts/game/space/systems/StarSystemBuilder.cs`: systems are built from data only; orbiting bodies use `OrbitalBody`, gates use Lagrange anchors, and jump gate scene nodes are split from their moving anchor.
- `scripts/orbital/OrbitalBody.cs` and `tests/OrbitalBodyTests.cs`: orbit position is deterministic Kepler-rail math using elapsed time, phase, eccentricity, and inclination.
- `scripts/orbital/LagrangeAnchor.cs` and `tests/LagrangeAnchorTests.cs`: L1/L2 anchors are recomputed from current planet/star world positions each physics tick.
- `scripts/npc/DistantSector.cs` and `tests/DistantSectorTests.cs`: far NPC traffic is pure data and timers, not spawned actors.
- `scripts/game/stations/StationRegistry.cs` and `tests/StationRegistryTests.cs`: station identity, faction, role, security, market profile, mission tags, interior scene, and quest givers come from registry data.
- `data/systems/alpha_centauri.json`: first non-Sol authored system reference. It currently provides body/gate/orbit data, not authored stations or NPC life, so Unreal's frontier station/trade layer must stay data-driven and Godot-compatible rather than pretending Alpha Centauri already had those records.

Dynamic-world-life rule from Godot: planets, stations, gates, route samples, patrol targets, pirate ambush points, and mission waypoints must resolve through current moving-frame data at the requested simulation time. Durable state may store route IDs, entity IDs, progress fractions, and source events, but realized positions must be refreshed from the current orbit/route frame.

## Current Unreal Baseline

Already present or partially present:

- non-Sol startup fixture, `frontier_test_01`
- target registry for `ember`, `brink_watch`, and `frontier_gate_a`
- player ship pawn with normal flight and supercruise
- docking state, docking ports, docked restore, and undock
- gate transition into `frontier_arrival_test_01`
- save/load for current system, ship location, clock, traffic, systemic state, and pending gate arrival
- systemic records for station services, markets, mission offers, mission instances, progression, traffic, encounters, and threats
- prototype HUD for target, flight, supercruise, docking, docked station context, runtime encounters, and basic service/mission surfaces
- minimal boot menu for new game, continue availability, continue load, and quit
- reusable UMG foundations for boot, comms, dialogs, pause summaries, system map, and station interaction read models
- early station interior room and first-person pawn
- Godot item catalog IDs, player personal inventory, starter ship equipment slots, stack limits, equip validation, and save-carrying equipment state
- Godot-derived ship weapon stats, projectile travel, energy/readiness, lead pip, rendered tracer placeholders, hostile response, and first-pass on-foot combat values

## Godot Playable Surface To Match

The Godot VS1 baseline proved:

- main menu new/continue flow
- space flight and supercruise with lockout/dropout behavior
- target selection and mission waypoint targeting
- station docking and launch flow
- station services: trade, repair/recovery, and launch
- commodity buy/sell loop
- mission offer, accept, progress, and turn-in
- hostile encounter and basic ship combat
- hostile station interior combat
- save/reload preserving credits, cargo, equipment, hull, shields, mission state, station stock, and location

## Feature Parity Tracker

Status legend:

- **Playable:** usable from the frontier build and covered by automation.
- **Partial:** backend or prototype surface exists, but the Godot gameplay loop is not fully player-facing yet.
- **Missing:** no meaningful Unreal gameplay surface yet.
- **Improve:** Unreal should intentionally diverge from Godot because the design can be stronger in data, UI, or simulation.

Parity does not mean a 1:1 implementation. Starlight/Godot serves as the main
behavioral reference for the player loop and combat feel, while Stargame docs
own the Unreal contracts. Unreal may add implementation depth or explicit design
layers where they preserve the same fantasy and make the system stronger. Those
layers must be labelled as **Improve** work, backed by tests, and must not hide
missing Godot contracts such as projectile travel, weapon energy, or the hostile
AI state machine.

| Feature area | Godot source of intent | Unreal status | Automated coverage | Player-facing gap | Next action |
| --- | --- | --- | --- | --- | --- |
| Boot/new/continue | `scripts/game/boot/MainMenuController.cs`, `scripts/game/runtime/SaveService.cs`, `tests/SaveServiceTests.cs` | Partial: minimal boot menu, startup profile, save/load, continue availability, new game, continue, and quit paths exist. | `Stargame.M0.StartNewSession.*`, `Stargame.M0.Boot.ContinueReflectsLoadableSave`, `Stargame.M0.SaveReload.PreservesSystemAndSpawnZonePayload`, `Stargame.M9.SaveLoad.SessionCarriesSystemicState` | Save-slot selection, load-failure presentation, and final menu styling/navigation are still thin. | Add widget-level coverage for boot menu button states and failure messages once the UI test harness is stable. |
| System authoring and non-Sol start | `data/systems/alpha_centauri.json`, `scripts/game/space/systems/*`, `tests/StarSystemLoaderTests.cs` | Playable: frontier fixture and authored registry are active; Sol is reference-only. | `Stargame.M1.Catalog.ValidatesFrontierFixture`, `Stargame.M6.Generation.*` | JSON import parity is still thinner than Godot's authored system loader. | Add a source-data conformance test that compares required authored fields against Godot `alpha_centauri.json` intent without copying Sol content. |
| Orbit, moving stations, and Lagrange gates | `scripts/orbital/OrbitalBody.cs`, `scripts/orbital/LagrangeAnchor.cs`, `tests/OrbitalBodyTests.cs`, `tests/LagrangeAnchorTests.cs` | Partial: moving-frame registry, moving route samples, and gate arrival exist. | `Stargame.M2.FrameQuery.*`, `Stargame.M5.5.Gate.*`, `Stargame.M7.Route.SamplesMovingEndpoints` | Need broader player-visible orbit/map/debug validation and moving station approach feedback. | Add a moving-frame debug overlay entry for selected station/gate current and predicted positions. |
| Space flight and supercruise | `scripts/game/space/GameplayRoot.cs`, `GameplayRoot.Guidance.cs`, `SupercruiseWarp.cs` | Playable: normal flight, targeting, supercruise, dropout, docking approach, and gate travel exist. | `Stargame.M3.*`, `Stargame.M4.*`, `Stargame.M5.*` | Feedback is still prototype HUD text; lockout/dropout reasons need clearer UI. | Convert supercruise/docking failure reasons into structured HUD view models instead of ad hoc text. |
| Docking and station services | `scripts/game/space/GameplayRoot.Docking.cs`, `scripts/game/stations/StationInteractableArea.cs`, `StationStubController.cs` | Partial: docking, undock, service endpoints, repair/refuel/rearm execution, docked facade, compact command view model, and station interaction read models exist. Refuel/rearm are Stargame service extensions beyond Starlight's proven repair/recovery surface. | `Stargame.M5.Docking.*`, `Stargame.M9.Service.EndpointAndDecisionInputsResolve`, `Stargame.M12.Progression.ServiceTransaction`, `Stargame.PlayableLoop.DockedStationCommandPanel.ListsGodotStationServices` | Docked UI is foundation-level and still needs a cohesive station panel/presentation pass. | Replace remaining prototype HUD station controls with the reusable station UI flow once the action/read-model contracts are stable. |
| Markets and economy | `scripts/game/runtime/EconomyService.cs`, `tests/EconomyServiceTests.cs` | Partial: deterministic market transactions, ledgers, cargo transfer, station stock records, Godot commodity stock profiles, price pressure, docked market quote view model, and save/reload coverage for visible stock/cargo mutation exist. | `Stargame.M9.Market.BuySellNoUmgDependency`, `Stargame.M9.Inventory.CargoTransferValidatesCapacityOwnershipAndLegality`, `Stargame.M12.Progression.IllegalCargoRejected`, `Stargame.PlayableLoop.DockedStationCommandPanel.ListsGodotStationServices` | Player-facing buy/sell exists as a foundation/prototype flow, not a finished market screen. | Harden the station market panel controls and validation feedback before adding broader commodity depth. |
| Inventory and equipment | `scripts/game/inventory/*`, `scripts/game/runtime/PlayerState.cs`, `tests/ItemCatalogTests.cs`, `tests/PlayerStateTests.cs`, `tests/ItemStackTests.cs` | Partial: Godot item IDs, stack limits, personal inventory, cargo readout, equipment slots, starter ship gear, docked inventory/equipment panel, docked equip/unequip/swap facade, validation, save carry-through, and first ship weapon stat hookup exist. | `Stargame.M9.Inventory.GodotItemCatalogEquipmentAndStacking`, `Stargame.M9.SaveLoad.SessionCarriesSystemicState`, `Stargame.PlayableLoop.DockedStationCommandPanel.ListsGodotStationServices` | Equipment effects are only thinly connected; shield/engine/module effects and full widget list controls are still incomplete. | Extend equipment effects only where they support the selected flight/combat/station slice, starting with shield/engine/resource effects. |
| Missions, contacts, and dialog | `scripts/game/missions/*`, `scripts/game/stations/QuestGiverController.cs`, `scripts/game/ui/DialogMenuController.cs`, `tests/Mission*Tests.cs` | Partial: mission offers, station-authored giver validation, contact interaction views, docked contact panel, accept/progress/turn-in, and waypoint selection exist. | `Stargame.M12.Progression.*`, `Stargame.M12.Progression.DebugProgressionTrace`, `Stargame.PlayableLoop.DockedStationFacade.ServiceTradeMissionUndockSave` | Dialog presentation is still static/foundation-level; the parity contract is station contact/mission-giver driven, with a mission board optional as later Stargame UI depth. | Harden station contact/mission-giver presentation before choosing any richer dialogue runtime or separate mission-board surface. |
| NPC world life and distant sectors | `scripts/npc/DistantSector.cs`, `scripts/npc/NpcManager.cs`, `tests/DistantSectorTests.cs` | Improve: logical traffic, player-relevance promotion/demotion budget, route progress, runtime traffic actors, map-visible projected traffic, distant-sector snapshot categories, save-sanitized realization state, score-derived encounter posture/comms, posture-specific encounter offsets/comms text, local encounter steering metrics, and response hooks exist. | `Stargame.M8.Traffic.*`, `Stargame.M8.WorldLife.RealizesDynamicTrafficActors`, `Stargame.M11.Realization.*` | Godot's distant-sector data-only simulation is simpler; Unreal intentionally deepens it with systemic scoring and actor realization, but those layers are not substitutes for missing combat contracts. | Keep the systemic posture layer as accepted Unreal design depth, then audit it whenever it starts driving combat rules instead of staging, visibility, or presentation. |
| Police patrols and piracy | `scripts/npc/NpcManager.cs`, `scripts/game/combat/HostileEncounterController.cs`, `scripts/game/space/GameplayRoot.Combat.cs` | Improve: patrol reservations, pirate interdiction policy, moving route anchors, legal/evidence/wanted state, runtime encounter outcomes, dynamic patrol/ambush route scoring, score/debug visibility, local steering, enhanced encounter posture/comms, hostile response state after player fire, and rendered combat feedback placeholders exist. | `Stargame.M10.Patrol.*`, `Stargame.M10.Policy.*`, `Stargame.M10.Encounter.*`, `Stargame.M10.WorldLife.*`, `Stargame.M12.Progression.PirateDistressPatrolOutcome` | Godot hostile behavior is a state machine (`Patrol`, `Intercept`, `Interdict`, `Engage`, `Disengage`, `ReturnToAnchor`). Unreal posture/comms depth is accepted, but the actual hostile AI state machine is still incomplete. | Build the Godot hostile AI state contract underneath the accepted Unreal scoring/posture layer. |
| Ship combat | `scripts/game/combat/ShipWeaponRig.cs`, `SpaceProjectile.cs`, `SpaceCombatCommon.cs`, `HostileEncounterController.cs`, `tests/SpaceCombatMathTests.cs` | Foundation parity: damage/threat records, abstract encounter resolution, runtime outcome controls, equipped hardpoint fire, Godot-derived player and hostile pulse-laser stats, cooldown/readiness state, weapon capacitor spend/regen gating, muzzle alignment gating, lead-point/fire-solution readout, rendered lead pip, inherited projectile velocity, projectile travel records, swept projectile hit resolution, hostile response after projectile impact, hostile projectile return fire, rendered tracer placeholder actors, and prototype HUD weapon feedback exist. | `Stargame.M10.5.Combat.AppliesDamageThreatAndIdempotency`, `Stargame.M10.WorldLife.ActivatesRuntimeEncounterBehavior`, `Stargame.M9.Inventory.GodotItemCatalogEquipmentAndStacking` | The first ship-combat foundation now follows the Godot projectile/weapon intent. Remaining combat work belongs under the broader hostile AI state-machine pass, not more point-6 weapon contract work. | Move to hostile AI state transitions only when that becomes the selected combat slice; otherwise shift to the non-combat parity surfaces. |
| Station interiors and combat | `scripts/station_interior/*`, `scripts/game/combat/OnFootWeapon.cs`, `StationHostile.cs`, `tests/SmokeTest.cs` | Foundation parity: early Unreal station room, first-person pawn, mission contacts, interactables, hostile actors, on-foot weapon fire, hostile clear objective, derelict/interior combat mission E2E, and data-backed Starlight station-combat profile exist. This is station interior combat, not ship boarding. | `Stargame.PlayableLoop.StationInterior.BasicRoomTransition`, `Stargame.PlayableLoop.StationInterior.HostileBoardingFoundation`, `Stargame.PlayableLoop.StationInterior.DerelictBoardingMissionE2E`, `Stargame.M9.Inventory.GodotItemCatalogEquipmentAndStacking` | Deferred: doors, richer room building, ambient NPCs, cover, magazines/reload, advanced FPS movement, and deeper enemy cover/peek/fire/retreat/hunt behavior. | Build presentation and Blueprint-facing UI from `FStationInteriorCombatView` before choosing any deeper FPS combat expansion. |
| System and sector maps | `scripts/game/ui/SystemMap.cs`, `scripts/game/ui/SectorMap.cs`, `PlanetOverlay.cs` | Partial: registry entries, session-level overview data, and the first player-facing system map widget exist for moving map entries, dynamic routes, selected target, mission target, player, traffic, patrol reservations, ambush risk, encounter actors, pan/zoom, and click-to-select navigation targets. | `Stargame.M2.MapAndScale.RegistryViewModel`, `Stargame.M8.WorldLife.RealizesDynamicTrafficActors` | Sector map and distant planet overlay are still deferred; the current system map is a functional foundation rather than final presentation. | Harden the system map visuals only as needed, then choose whether the next map pass is sector-map routing or planet/body overlay. |
| Save/reload contract | `scripts/game/runtime/SaveService.cs`, `SaveStore.cs`, `tests/SaveServiceTests.cs`, `GameSessionTests.cs` | Partial: core session/systemic state, ship location, clock, pending gate arrival, traffic, mission progression, market/service results, equipment state, station-interior docked location, and visible station stock/cargo mutations persist. Invalid traffic and invalid docked-station save payloads are rejected. | `Stargame.M0.SaveReload.*`, `Stargame.M2.SaveLoad.*`, `Stargame.M5.Docking.SaveReloadAndRepeatUndock`, `Stargame.M7.Route.SaveReloadAndDebugSummary`, `Stargame.M8.Traffic.LoadRejectsInvalidTrafficState`, `Stargame.M9.SaveLoad.SessionCarriesSystemicState`, `Stargame.M12.Progression.SaveReloadProgression`, `Stargame.PlayableLoop.*` | Every new UI/action surface still needs an explicit save mutation test. | Keep the rule: no feature reaches Playable until save/load verifies the player-visible mutation. |
| UI/HUD polish | `scripts/game/ui/FlightHud.cs`, `StatusHudController.cs`, `CombatHudController.cs`, `CommsMenuController.cs`, `DialogMenuController.cs`, `PauseMenuController.cs` | Partial: prototype C++ HUD still handles flight/combat instrumentation, but reusable UMG foundations now exist for boot, station interactions, system map, comms docking, generic dialog choices, and pause inventory/mission/settings views. | `Stargame.PlayableLoop.DockedStationCommandPanel.ListsGodotStationServices` covers comms, dialog, and pause read-model contracts; broader gameplay tests cover the underlying actions. | Presentation is still foundation-level and some HUD text remains in `PrototypeFlightHud`; no final art/style pass or full menu navigation stack yet. | Keep extracting modal/action UI out of `PrototypeFlightHud` as each gameplay surface becomes stable; do not add new UI concepts until they map to an existing Godot intent. |

## Implementation Status Queue

These entries record the recent dependency order and current status. Use the
feature tracker and parity slices above for what is still missing. Do not start
with visuals before the data/action path is deterministic and tested.

1. **Inventory/equipment swap semantics - done**
   - Source: `PlayerState.cs`, `EquipmentSlotId.cs`, `ItemCatalog.cs`, `ItemDef.cs`.
   - Implemented: unequip, swap, reject incompatible items, return previous equipment to a container, preserve stack limits.
   - Test: `Stargame.M9.Inventory.GodotItemCatalogEquipmentAndStacking`.

2. **Docked station command panel - done**
   - Source: `StationStubController.cs`, `EconomyService.cs`, `MissionDialogService.cs`, `CommsMenuController.cs`.
   - Implemented: compact docked command view model for services, market, mission contact, inventory/equipment, enter interior, and undock.
   - Test: `Stargame.PlayableLoop.DockedStationCommandPanel.ListsGodotStationServices`.

3. **Market and inventory integration - partial**
   - Source: `EconomyService.cs`, `ItemCatalog.cs`, `SaveService.cs`.
   - Implemented: Godot commodity stock profiles, price pressure labels, quote view model, cargo/personal holding counts, buy destination selection, visible stock mutation after buy/sell, and save/load assertions for player-visible stock/cargo mutations.
   - Remaining: station UI buy/sell controls.
   - Test: `Stargame.PlayableLoop.DockedStationCommandPanel.ListsGodotStationServices`, `Stargame.M9.Market.BuySellNoUmgDependency`, and `Stargame.M9.SaveLoad.SessionCarriesSystemicState`.

4. **Mission contact panel - done**
   - Source: `QuestGiverController.cs`, `MissionDialogService.cs`, `MissionService.cs`.
   - Implemented: docked contact panel with offer, active objective, ready-to-turn-in, and completed/unavailable states from `FMissionContactInteractionView`.
   - Test: `Stargame.PlayableLoop.DockedStationFacade.ServiceTradeMissionUndockSave`.

5. **Dynamic patrol and ambush scoring - done for the policy slice**
   - Source: `NpcManager.cs`, `DistantSector.cs`, `HostileEncounterController.cs`.
   - Implemented: route candidate scores from current moving route samples, route security, pirate risk, patrol coverage, criminal records, recent offenses, active mission context, route value, and deterministic time pressure.
   - Implemented: `SelectPirateInterdictionHazard` now uses the scored moving route sample and annotates the hazard with score/debug data; `ReservePatrolForBestScoredRoute` reserves and annotates the highest-scored patrol route.
   - Test: `Stargame.M10.Patrol.ReservesAssetsFromRouteSecurity`, `Stargame.M10.Policy.SelectsPirateInterdictionAndFleeDestination`.

6. **Encounter score visibility - done**
   - Source: `SystemMap.cs`, `CombatHudController.cs`, `HostileEncounterController.cs`.
   - Implemented: patrol reservation and ambush markers expose dynamic score/debug labels, and runtime encounter view models carry selected hazard/reservation id, score, and score reason for HUD/comms.
   - Test: `Stargame.M8.WorldLife.RealizesDynamicTrafficActors`, `Stargame.M10.WorldLife.ActivatesRuntimeEncounterBehavior`.

7. **Minimal ship weapon loop** - Done for the first playable slice.
   - Source: `ShipWeaponRig.cs`, `SpaceProjectile.cs`, `SpaceCombatCommon.cs`.
   - Implemented: `ship_weapon_hardpoint_1` resolves the equipped ship weapon, creates a deterministic player counter-threat, applies idempotent damage to the hostile combatant, binds `9 FIRE`, and updates the runtime encounter HUD.
   - Test: `Stargame.M10.WorldLife.ActivatesRuntimeEncounterBehavior`.

8. **Player-facing system map foundation** - Done for the first playable slice.
   - Source: `SystemMap.cs`, `SectorMap.cs`, `PlanetOverlay.cs`.
   - Implemented: current positions for map entries, dynamic route endpoints/midpoints, selected target, mission target, player, traffic, patrol reservations, ambush risk, and encounter actors.
   - Implemented: Unreal UMG/Slate system map surface opened with `M`, right-drag pan, mouse wheel zoom, left-click target selection through the existing navigation target resolver, selected/mission/player markers, route lines, orbit rings, and status text.
   - Deferred: Godot's separate `SectorMap` network view and `PlanetOverlay` distant-body screen markers.
   - Test: `Stargame.M8.WorldLife.RealizesDynamicTrafficActors`.

9. **Distant-sector snapshot query** - Done for the first playable slice.
   - Source: `DistantSector.cs`, `NpcManager.cs`, `DistantSectorTests.cs`.
   - Implemented: Blueprint read model that separates data-only logical ships, map-projected route traffic, locally realized traffic, and encounter actors while carrying tier, role, route progress, moving route sample, and realization reason.
   - Test: `Stargame.M8.WorldLife.RealizesDynamicTrafficActors`.

9b. **Boot, pause, comms, and dialog UI foundation** - Done for the first playable slice.
   - Source: `CommsMenuController.cs`, `DialogMenuController.cs`, `PauseMenuController.cs`, `StatusHudController.cs`, `CombatHudController.cs`.
   - Implemented: `UStargameBootMenuWidget` for New Game, Continue, disabled Continue without a save, and Quit; `UStargameCommsWidget` for selected-station docking prompts and docking requests; `UStargameDialogWidget` for reusable title/body/choice dialogs and mission-contact accept/turn-in commands; and `UStargamePauseMenuWidget` for inventory, cargo, loadout, mission, and settings summary tabs.
   - Implemented: controller/input bindings for `R` comms and `I`/`Esc` pause, while leaving station-interior walk-up interaction panels intact.
   - Test: `Stargame.M0.Boot.ContinueReflectsLoadableSave`, `Stargame.PlayableLoop.DockedStationCommandPanel.ListsGodotStationServices`.

10. **Distance and budget driven traffic promotion** - Done for the first playable slice.
   - Source: `NpcManager.cs`, `DistantSector.cs`, `DistantSectorTests.cs`.
   - Implemented: route traffic only spawns actors when promoted to `Tier1Realized`; a player-relevance pass promotes nearby/high-priority traffic within the M11 actor budget and demotes local traffic when it falls outside the distance/budget window.
   - Test: `Stargame.M11.Realization.AppliesPlayerRelevancePromotionBudget`.

11. **Patrol and pirate enhanced encounter posture** - Done for the first playable slice.
   - Source: `NpcManager.cs`, `DistantSector.cs`, `HostileEncounterController.cs`.
   - Intent: accepted Unreal design depth over the Godot ambient traffic and hostile encounter fantasy, not a direct Godot AI-state port.
   - Implemented: realized pirate and patrol encounter actors carry posture and comms variant IDs derived from score/debug context, role, and dynamic pressure; runtime encounter views and prototype HUD expose the selected posture.
   - Test: `Stargame.M8.WorldLife.RealizesDynamicTrafficActors`, `Stargame.M10.WorldLife.ActivatesRuntimeEncounterBehavior`.

12. **Posture-driven patrol and pirate staging** - Done for the first playable slice.
   - Source: `NpcManager.cs`, `DistantSector.cs`, `HostileEncounterController.cs`.
   - Intent: score-derived posture may drive staging, local steering targets, and comms flavor; it must not replace Godot-backed combat states.
   - Implemented: pirate high-pressure/probe/shadow and patrol emergency/screen/presence postures now select distinct local encounter offsets, local behavior state IDs, and player-facing comms lines.
   - Test: `Stargame.M8.WorldLife.RealizesDynamicTrafficActors`, `Stargame.M10.WorldLife.ActivatesRuntimeEncounterBehavior`.

13. **Continuous hostile and patrol steering** - Done for the first playable slice.
   - Source: `NpcManager.cs`, `DistantSector.cs`, `HostileEncounterController.cs`.
   - Implemented: realized pirate and patrol actors preserve their live actor location across score refreshes, steer toward behavior-specific desired positions over simulation time, and expose active steering, distance-to-goal, and closing trend through runtime encounter state and the prototype HUD.
   - Test: `Stargame.M8.WorldLife.RealizesDynamicTrafficActors`, `Stargame.M10.WorldLife.ActivatesRuntimeEncounterBehavior`.

14. **Hostile response and weapon feedback** - Done for the first playable slice.
   - Source: `ShipWeaponRig.cs`, `SpaceProjectile.cs`, `SpaceCombatCommon.cs`, `HostileEncounterController.cs`.
   - Implemented: equipped ship weapon fire now carries cooldown/readiness state, rejects repeat fire while cooling down, allows the next shot after simulation time advances, records a hostile response state, and exposes weapon readiness plus hostile response in the runtime encounter HUD.
   - Test: `Stargame.M10.WorldLife.ActivatesRuntimeEncounterBehavior`.

15. **Combat response movement and projectile travel** - Done for the first playable slice.
   - Source: `ShipWeaponRig.cs`, `SpaceProjectile.cs`, `SpaceCombatCommon.cs`, `HostileEncounterController.cs`.
   - Implemented: player and hostile weapon fire create in-flight projectile records with Godot-derived speed, max distance, inherited shooter velocity, and collision radius; damage is applied by swept projectile hit after travel time, not at click time; hostile response movement happens after player projectile impact and can queue hostile return fire.
   - Test: `Stargame.M10.WorldLife.ActivatesRuntimeEncounterBehavior`.

16. **Rendered combat feedback and authored weapon stats** - Done for the first playable slice.
   - Source: `ShipWeaponRig.cs`, `SpaceProjectile.cs`, `SpaceCombatCommon.cs`, `HostileEncounterController.cs`.
   - Implemented: player and hostile pulse laser fire now resolve through authored ship weapon stat records using Godot damage, cooldown, weapon energy cost, weapon energy regen, projectile max-distance, projectile speed, and projectile collision-radius values; player fire also uses minimum muzzle alignment. The HUD renders the predictive lead pip from the existing lead-point data and exposes rendered/faded shot state, fire-solution text, and authored stat IDs.
   - Test: `Stargame.M9.Inventory.GodotItemCatalogEquipmentAndStacking`, `Stargame.M10.WorldLife.ActivatesRuntimeEncounterBehavior`.

17. **Combat design-depth audit** - done.
   - Source: `GameplayRoot.Combat.cs`, `ShipWeaponRig.cs`, `SpaceProjectile.cs`, `HostileEncounterController.cs`, `NpcManager.cs`.
   - Result: dynamic scoring, encounter posture, and posture-specific comms are accepted Unreal design depth when used for staging/presentation. The remaining Godot combat contract is the broader hostile AI state-machine behavior. No lock-on or new target-acquisition concept exists in Godot.

## Tracking Rules

- A feature is **Playable** only when a player can reach it without console commands and an automation test verifies the core mutation or interaction.
- A feature is **Partial** when backend state or debug/prototype controls exist but the Godot loop is not yet fully usable.
- A feature is **Missing** when Unreal has no real equivalent beyond placeholder content.
- Every new gameplay mutation must name either the Godot source file that established the intent or the accepted Unreal design-depth layer it belongs to, and must add or extend one Unreal automation test.
- Dynamic world-life features must store durable IDs and simulation state, not static coordinates. Realized positions must be recomputed from current moving-frame data.
- Prefer Unreal-native UI and actor architecture, but keep Godot's gameplay contracts visible in tests: item IDs, slot IDs, station IDs, mission IDs, route IDs, faction IDs, and save payload meaning.

## Parity Slices

### Slice 1: Playable Frontier Loop - Partial

Goal: boot, fly, target, dock, use a station, undock, and gate-transition without console commands.

Required:

- [done] bind selected-target interaction paths for stations, gates, and docked station context
- [done] keep docked station service state reachable after docking
- [done] verify `frontier_gate_a` only transitions when the ship is inside activation range
- [done] expose service, market, mission, inventory/equipment, interior, and undock commands through player-facing prototype HUD/input
- [partial] replace prototype/exec-style surfaces with real UI; the boot menu exists, while station surfaces are still prototype/foundation-level

### Slice 2: Station Services Surface - Partial

Goal: turn docked backend records into a usable docked station panel.

Required:

- [done] list station service endpoint records at `brink_watch`
- [done] execute repair/refuel through deterministic service requests
- [done] buy/sell at least one commodity through the transaction path
- [done] persist service and market mutations across save/load
- [done] show player credits, ship resources, market stock, and cargo in prototype state
- [partial] build the compact docked station panel as a reusable view model

### Slice 3: Mission Contact And Waypoint - Partial

Goal: accept one frontier mission, target its destination, and complete it.

Required:

- [done] show mission offers from docked station/contact context
- [done] accept one mission through the mission service path
- [done] set/select a mission waypoint target in space
- [done] progress objective state from location and encounter conditions
- [done] complete/turn in at the source station through the backend facade
- [partial] persist mission state and debug progression records
- [partial] build the mission contact UI with dialog text and reward presentation; a separate mission board is optional later UI depth

### Slice 4: Combat Encounter - Partial

Goal: create a short hostile space encounter comparable to the Godot hostile patrol.

Required:

- [done] spawn or realize pirate and patrol actors from systemic encounter data
- [done] resolve runtime encounter outcomes into simulation/progression records
- [done] track abstract damage/threat records for combat outcomes
- [done] give the HUD hostile encounter state and outcome controls
- [done] add Godot-style projectile travel and impact, including inherited velocity and hostile projectile fire
- [done] connect equipped ship weapons to damage, HUD feedback, and first hostile response
- [done] migrate Godot's moving projectile travel/collision semantics
- [done] migrate Godot's weapon energy, lead point, rendered lead pip, and muzzle-alignment fire solution

### Slice 5: Station Interior Combat Placeholder - Foundation Done

Goal: replace Starlight's hostile station-interior combat loop with an
Unreal-native station interior placeholder. This is not ship boarding; boarding
other ships remains out of scope.

Required:

- [done] add an early interior room and first-person pawn
- [done] add a hostile station-interior combat room state on top of the station interior foundation
- [done] add simple enemy actors with health and attack behavior
- [done] add Godot-derived, data-backed player sidearm and hostile combat values
- [done] expose `FStationInteriorCombatView` for Blueprint/UI/testing
- [done] complete an interior combat objective by clearing the room
- [done] return to space/station services cleanly
- [deferred] richer rooms, cover behavior, magazines/reload, and advanced FPS movement

## Current Cleanup Rule

Godot is the source for the player loop and minimum gameplay contracts, not a ceiling on Unreal implementation depth. New Unreal design concepts are allowed when they capture the intended fantasy better, but they must be explicitly labelled as design-depth work and kept separate from missing parity contracts. For combat, the point-6 weapon/projectile foundation is now covered; the next hard combat gap is the hostile AI state machine.
