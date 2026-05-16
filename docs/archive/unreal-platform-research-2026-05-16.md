# Unreal Research Notes

Research date: 2026-05-16

These notes verify current Unreal Engine assumptions before creating the project. They are not an implementation plan by themselves; they record which Unreal systems are suitable for the project and where caution is needed.

## Checked Sources

- Unreal Engine 5.7 Input documentation: https://dev.epicgames.com/documentation/en-us/unreal-engine/input-in-unreal-engine?application_version=5.7
- Unreal Engine 5.7 Blueprints documentation: https://dev.epicgames.com/documentation/en-us/unreal-engine/blueprints-visual-scripting-in-unreal-engine?application_version=5.7
- Unreal Engine 5.7 CommonUI input guide: https://dev.epicgames.com/documentation/en-us/unreal-engine/commonui-input-technical-guide-for-unreal-engine?application_version=5.7
- Unreal Engine 5.7 UWorldSubsystem API: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UWorldSubsystem
- Unreal Engine 5.7 release notes: https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-7-release-notes?application_version=5.7
- Unreal data-driven gameplay documentation: https://dev.epicgames.com/documentation/en-us/unreal-engine/data-driven-gameplay-elements-in-unreal-engine
- Unreal Large World Coordinates / Niagara documentation: https://dev.epicgames.com/documentation/en-us/unreal-engine/large-world-coordinates-in-niagara-for-unreal-engine

## Confirmed Unreal Capabilities

### C++ and Blueprints

Unreal still supports the intended mixed workflow: C++ for baseline gameplay systems and Blueprint for iteration, content assembly, tuning, UI wiring, and designer-facing behavior.

Project implication:

- Core contracts should be C++ `USTRUCT`, `UCLASS`, and `UDataAsset` types.
- Ship flight, world simulation, save/load, and station transition logic should start in C++.
- Blueprint subclasses should own content-facing defaults and rapid iteration.

### Gameplay Framework

Unreal's gameplay framework remains the correct base for players, controllers, pawns, cameras, and game rules.

Project implication:

- Use a `PlayerController` as the durable owner of player input and high-level mode switching.
- Use separate player-controlled pawns for space flight and on-foot station interiors.
- Do not force one pawn/controller design to cover both pillars.

### Enhanced Input

Enhanced Input remains the modern input path. It supports input actions, mapping contexts, contextual priority, and player-local mapping.

Project implication:

- Create separate input mapping contexts for:
  - space flight
  - docking/menu interaction
  - first-person station movement
  - UI-only mode
- Keep mode changes explicit during transitions so stale input does not leak between ship and FPS controls.

### Subsystems

`UWorldSubsystem` and related subsystem types are current. They are suitable for auto-instanced systems tied to engine, game instance, local player, or world lifetime.

Project implication:

- Use `UGameInstanceSubsystem` for persistent cross-map/session state.
- Use `UWorldSubsystem` for active star-system simulation, orbital rail updates, and world-local registries.
- Avoid a single oversized "GameplayRoot" equivalent.

### Data-Driven Gameplay

Unreal still supports data tables mapped to `FTableRowBase` structs, plus data assets and asset references.

Project implication:

- Use `UPrimaryDataAsset` or regular `UDataAsset` for authored game object definitions such as ship archetypes, equipment, factions, stations, and mission templates.
- Use Data Tables or JSON for large tabular/system data where diffability and external editing matter.
- Keep schema/version fields for external JSON, following the Starlight lesson.

### UMG and CommonUI

UMG is the default UI path. CommonUI is available and documented, but has additional setup complexity around input routing.

Project implication:

- Start with UMG for early HUD, debug UI, and docked service menus.
- Consider CommonUI once the UI scope needs controller/platform-ready navigation.
- Do not block the first vertical slice on CommonUI.

### Large Worlds

UE5 has Large World Coordinates, and FVector is double precision in the core engine. However, large-world support is not a complete space-game solution by itself. VFX, rendering, physics, particle systems, materials, and gameplay systems can still have practical limits or special handling.

Project implication:

- Keep a floating-origin/reference-frame design.
- Store long-range simulation state in game-space coordinates independent of actor transforms.
- Keep the player/camera close to local origin when possible.
- Treat Large World Coordinates as helpful infrastructure, not a replacement for careful space-scale architecture.

## Initial Unreal Project Shape

Recommended first project form:

- C++ Unreal project.
- Enhanced Input enabled.
- Runtime modules kept small and domain-oriented.
- A minimal initial vertical slice:
  - boot map
  - `frontier_test_01` non-Sol test fixture
  - ship pawn with simple flight mode
  - placeholder body/station/gate actors built from fixture data
  - docking target actor
  - station menu placeholder
  - save/session subsystem skeleton

Recommended early folders:

```text
Source/Stargame/
  Core/
  Data/
  Flight/
  Space/
  Stations/
  UI/
  Runtime/

Content/
  Blueprints/
  Data/
  Input/
  Maps/
  UI/
```

## Cautions

- Do not build a Godot-style scene-tree coordinator in Unreal.
- Do not assume Large World Coordinates removes the need for origin management.
- Do not start with full station interiors before the space docking/economy loop is stable.
- Do not overcommit to Gameplay Ability System for early ship/FPS mechanics unless a concrete need emerges.
- Do not use CommonUI until the basic UI flow exists in UMG.
