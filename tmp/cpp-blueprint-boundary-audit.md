# C++ / Blueprint Boundary Audit

Working file, not formal docs. Purpose: identify code that has crept into C++ but should move toward Blueprint, data assets, or authored content so the project matches the intended Unreal workflow:

- C++ owns stable rules, simulation contracts, validation, save/load, spawning services, and runtime facades.
- Data assets own authored game data: systems, ships, commodities, station services, missions, prices, profiles, encounter definitions.
- Blueprint/content owns concrete presentation and authored play surfaces: HUD/menu appearance, ship visuals, station interiors, NPC visuals, VFX/audio, interactable meshes, cameras, and test scenes.

## Executive Summary

The foundation is mostly pointed the right way: session state, star-system definitions, flight/docking rules, station services, systemic gameplay, traffic, encounters, save/load, and validation are legitimate C++ systems.

The drift is in the player-facing layer. Several temporary C++ classes now hardcode presentation, layout, labels, placeholder meshes, colors, lighting, actor placement, and test-fixture content. That is why iteration feels like C++ work when it should feel like Unreal content work.

This does not mean deleting the systems. It means splitting each class into:

- a small C++ base/facade that exposes state and events; and
- a Blueprint or data asset implementation that owns visuals, layout, authored placement, and tuning.

## Highest Priority C++ Drift

### 1. Boot menu is fully hardcoded C++ UI

Files:

- `Source/Stargame/Private/UI/StargameBootMenuWidget.cpp`
- `Source/Stargame/Private/UI/StargameBootMenuWidget.h`
- `Source/Stargame/Private/Core/StargameGameModeBase.cpp`

What crept into C++:

- Full widget construction in code: canvas, border, size box, vertical box, buttons, text blocks.
- Visual style values: colors, sizes, padding, title text, button labels.
- Boot flow presentation and business action are tightly coupled.

What should remain in C++:

- `StartNewSession`, `LoadDevelopmentSlot`, continue availability, and error reporting.
- A clean boot controller/facade function callable from Blueprint.

What should move out:

- Widget hierarchy, styling, button layout, text presentation, and final menu Blueprint.

Target shape:

- C++ `UStargameBootMenuWidget` becomes a thin base class exposing `NewGame`, `Continue`, `Quit`, `RefreshContinueState`, and Blueprint events like `OnStatusChanged`.
- Create a `WBP_BootMenu` Blueprint for layout and visuals.
- GameMode should reference a configurable `TSubclassOf<UUserWidget>` boot menu class, not hardwire `UStargameBootMenuWidget::StaticClass()`.

Priority: P0, because this directly affects green Play and day-to-day iteration.

### 2. Flight HUD is hardcoded Canvas drawing

Files:

- `Source/Stargame/Private/UI/PrototypeFlightHud.cpp`
- `Source/Stargame/Public/UI/PrototypeFlightHud.h`

What crept into C++:

- Entire HUD rendering is C++ Canvas drawing.
- Colors, labels, positions, typography scale, target panels, mission panel, docking panel, encounter panel, system bars.
- Player-facing strings such as `FRONTIER TEST FLIGHT`, `NAV FREE FLIGHT`, `PULSE LASER`, `TARGETS`, `MISSION`, etc.

What should remain in C++:

- View-model construction: speed, throttle, selected target, mission waypoint, docking state, encounter state, system map entries.
- Projection helpers if useful.

What should move out:

- Layout and presentation to UMG Blueprint widgets.
- Weapon names/status labels to ship loadout/profile data.
- Panel visibility and styling to Blueprint.

Target shape:

- Replace Canvas HUD with `WBP_FlightHUD`.
- C++ provides `FStargameFlightHudViewModel` and refresh methods.
- Keep debug Canvas HUD only behind a console variable or editor-only path.

Priority: P0/P1, because this will otherwise make every HUD change a C++ compile/restart.

### 3. Runtime space presentation actor is C++ placeholder content

Files:

- `Source/Stargame/Private/Space/SystemSpacePresentationActor.cpp`
- `Source/Stargame/Public/Space/SystemSpacePresentationActor.h`
- `Source/Stargame/Private/Space/StarSystemSubsystem.cpp`

What crept into C++:

- Starfield generation count/radius/scale.
- Directional/fill light setup and color/intensity.
- Engine basic sphere material/mesh references.
- The system subsystem directly spawns this concrete presentation actor.

What should remain in C++:

- The star system subsystem can request or own a presentation layer.
- It should expose build-complete data and anchor transforms.

What should move out:

- Actual space backdrop, lighting, starfield, skybox, VFX, and visual policy to Blueprint/content.

Target shape:

- Add `TSubclassOf<ASystemSpacePresentationActor>` or a data-asset-driven `PresentationActorClass` to a system/profile/start configuration.
- C++ base has `ConfigureForSystem(SystemDefinition/ViewModel)`.
- Blueprint subclasses implement stars, lighting, sky, and VFX.

Priority: P0/P1. The current actor is useful as a temporary floor, but it should become a Blueprint hook quickly.

### 4. Space markers use C++ placeholder mesh/material/light

Files:

- `Source/Stargame/Private/Space/M0SystemMarkerActor.cpp`
- `Source/Stargame/Public/Space/M0SystemMarkerActor.h`
- `Source/Stargame/Private/Space/StarSystemSubsystem.cpp`

What crept into C++:

- Sphere mesh selection.
- Marker color by entity type.
- Light intensity, attenuation, scale policy.
- Labels and visual shape for bodies/stations/gates/resources.

What should remain in C++:

- The active system entity registry.
- Entity IDs, entity types, transforms, target metadata.

What should move out:

- Marker mesh, material, light, scale multiplier, and per-type visual style.

Target shape:

- Rename away from `M0` when cleanup happens.
- Introduce `AStargameSystemEntityActor` C++ base with Blueprint events/properties.
- Resolve actor class by entity type/profile, ideally from body/station/gate profile data.

Priority: P1.

### 5. Ship pawn owns too much placeholder presentation

Files:

- `Source/Stargame/Private/Flight/SpaceFlightPawn.cpp`
- `Source/Stargame/Public/Flight/SpaceFlightPawn.h`

What crept into C++:

- Cube mesh ship visual.
- Ship visual scale and offsets.
- Ship visibility light.
- Atmospheric dust mesh/material/count/color and local placement.
- Camera presentation values are editable C++ properties, but still embedded in the C++ pawn class.

What should remain in C++:

- Flight input, movement integration, docking state, collision behavior, camera anchor/base components, telemetry access.
- Core component relationships that Blueprints can extend.

What should move out:

- Concrete mesh choice, ship art, ship light, particle/VFX, camera feel presets, atmospheric dust visuals.

Target shape:

- C++ `ASpaceFlightPawn` becomes the base flight pawn.
- Blueprint `BP_WayfarerFlightPawn` owns mesh, camera tuning, VFX, audio, lights.
- Ship archetype data should point to the pawn Blueprint class, not just C++ base.
- Movement and camera tuning can be loaded from `ShipMovementProfile` or pawn Blueprint defaults.

Priority: P1. It is a foundation risk because all ship presentation currently looks like C++ work.

### 6. Station interior room and interactables are authored in C++

Files:

- `Source/Stargame/Private/Station/StationInteriorRoomActor.cpp`
- `Source/Stargame/Public/Station/StationInteriorRoomActor.h`
- `Source/Stargame/Private/Station/StationInteriorInteractableActor.cpp`
- `Source/Stargame/Public/Station/StationInteriorInteractableActor.h`
- `Source/Stargame/Private/Station/StationMissionContactActor.cpp`
- `Source/Stargame/Private/Station/StationInteriorHostileActor.cpp`

What crept into C++:

- Room geometry: floor/walls are built from cubes.
- Interactable list and positions: repair, refuel, market, launch.
- Interactable labels: `Repair Service`, `Refuel Service`, `Commodity Desk`, `Airlock / Launch`.
- Hostile spawn positions.
- Interactable mesh, text render labels, prompt text, focus scaling.

What should remain in C++:

- Station interior session transition.
- Interaction queries and command facades.
- Hostile combat state and mission objective completion hooks.

What should move out:

- Room layout, spawn points, interactable actor classes, labels, prompt presentation, NPC meshes, hostile spawn points, station-specific interior setup.

Target shape:

- C++ base actors remain.
- Add `UStationInteriorProfileAsset` or extend station profile with interior class/layout references.
- `AStationInteriorRoomActor` should support authored child actors/markers and not always spawn C++ defaults.
- Create simple Blueprint room now, even if it is just plane/walls/markers, so future rooms are content work.

Priority: P0/P1. This is directly aligned with the user goal: station interiors should become authored Unreal spaces, not panels or C++-built boxes.

### 7. Traffic, encounter, and combat presentation actors are C++ placeholders

Files:

- `Source/Stargame/Private/Space/LogicalTrafficActor.cpp`
- `Source/Stargame/Public/Space/LogicalTrafficActor.h`
- `Source/Stargame/Private/Space/CombatShotPresentationActor.cpp`
- `Source/Stargame/Public/Space/CombatShotPresentationActor.h`
- `Source/Stargame/Private/Space/StarSystemSubsystem.cpp`

What crept into C++:

- Traffic actor visual uses engine cone mesh and fixed scale.
- Combat shot presentation uses cube mesh and scale math.
- Pirate/patrol behavior variants, local offsets, comms variant IDs, and comms lines live in C++.

What should remain in C++:

- Realization decisions, actor lifecycle, route transform evaluation, encounter state facades.
- The resolved view model for “there is a pirate/patrol/shot here”.

What should move out:

- Traffic ship mesh/class per archetype.
- Combat shot VFX/audio class per weapon/presentation profile.
- Encounter comms lines and behavior variant presentation data.

Target shape:

- Ship archetypes resolve actor Blueprint classes for traffic and encounter ships.
- Weapon/loadout profiles resolve shot presentation Blueprint/VFX.
- Encounter presentation profiles define comms text and local staging offsets, or at least a data table/data asset does.

Priority: P1/P2. The systems are useful, but the presentation should not harden in C++.

### 8. Frontier fixture content is still partly C++

Files:

- `Source/Stargame/Private/Data/FrontierTestFixtureProvider.cpp`
- `Source/Stargame/Private/Data/StarCatalogSubsystem.cpp`
- `Source/Stargame/Private/Data/StargameM1FixtureAuthoring.cpp`

What crept into C++:

- Specific system, station, gate, spawn, mission, market, account, faction, commodity, and route definitions.
- Content names like `Brink Watch`, `Ember`, `Frontier Gate A`, `Wayfarer`, specific NPC IDs, service inventories, mission offers.

What should remain in C++:

- Fixture fallback for automation tests and empty projects is acceptable.
- Validation logic and schema contracts belong in C++.

What should move out:

- Actual playable frontier content should be authored in data assets.
- C++ fixture should be explicitly test/dev fallback, not the main content source.

Target shape:

- Keep `FrontierTestFixtureProvider` only as a fallback/test provider.
- Make runtime prefer content assets from `/Game/Data/...`.
- Add validation tests that assert authored assets can fully replace fixture content.

Priority: P1.

### 9. PlayerController still has debug interaction shortcuts in C++

Files:

- `Source/Stargame/Private/Core/StargamePlayerController.cpp`
- `Config/DefaultInput.ini`

What crept into C++:

- Numeric station/encounter actions and service amount defaults.
- Debug command composition and hardcoded station service amounts.

What should remain in C++:

- Input can call session facades.
- Debug shortcuts can remain for dev automation if gated.

What should move out:

- Player-facing station interaction path must be walk-up/interactable-driven.
- Service amount/pricing/command UI should be data/UI driven.

Target shape:

- Keep debug hotkeys behind `stargame.DebugStationHotkeys` and ideally editor-only docs.
- Move normal station command issuance into interactable Blueprint/UI flow.

Priority: P2, because it is already gated, but it should not be mistaken for real design.

## What Should Stay In C++

These areas are generally appropriate for C++ and should not be moved just for the sake of moving them:

- `UStargameSessionSubsystem`: session lifecycle, save/load, runtime facade methods, state transitions.
- `UStarSystemSubsystem`: active system registry, spawning services, transform refresh, route/system lifecycle.
- `UOrbitRouteFrameQueryService`: coordinate frames, route evaluation, orbit/frame math.
- `USystemicGameplayQueryService`: economy/legal/mission/systemic state mutation rules.
- `ULogicalTrafficQueryService`: traffic simulation, relevance, route samples.
- `UShipFlightModeComponent`: core flight mode math and supercruise rules.
- `StargameDataTypes.h`: schema contracts, view models, save payloads, validation structs.
- Automation tests, as long as they assert behavior and do not become the only place content exists.

## Recommended Cleanup Order

1. Boot menu class reference cleanup:
   - Add configurable boot menu widget class to GameMode.
   - Create Blueprint widget as the actual menu.
   - Keep C++ as facade/base only.

2. Playable map/presentation hook:
   - Stop depending on `/Engine/Maps/Entry` as the visible experience.
   - Add a simple project map or configurable runtime presentation Blueprint.
   - Keep generated fallback only as dev safety.

3. Station interior authoring hook:
   - Add station interior Blueprint class/profile support.
   - Move default room/interactable layout out of C++ arrays.

4. Flight pawn Blueprint split:
   - Keep movement in C++.
   - Move mesh/lights/VFX/camera presentation to `BP_WayfarerFlightPawn`.

5. HUD UMG split:
   - Build C++ view-model structs.
   - Move Canvas drawing to UMG Blueprint.

6. Space marker/traffic/combat presentation split:
   - Create Blueprint actor class hooks for entity markers, traffic ships, and shot presentation.
   - Resolve classes from profiles/archetypes.

7. Fixture/data hardening:
   - Runtime prefers data assets.
   - Fixture provider remains fallback/test-only.

## Acceptance Criteria For This Cleanup

- Pressing green Play shows a project-owned boot/menu experience without needing C++ widget layout.
- Starting a game enters an authored or Blueprint-configured visible sector presentation.
- Replacing a ship mesh, station room, NPC look, marker style, HUD layout, or boot menu styling does not require a C++ compile.
- Adding a commodity, service, mission, station interior layout, ship archetype, or traffic visual class is data/Blueprint work.
- C++ changes are needed only for new systemic rules, new contracts, or new engine-level capabilities.

## Notes

- The current C++ presentation code is not all “wrong”; some was useful scaffolding to make foundations visible.
- The problem is letting scaffolding become the normal authoring path.
- The cleanup should preserve the working C++ systems while moving visible/tunable choices into Unreal-native content.
