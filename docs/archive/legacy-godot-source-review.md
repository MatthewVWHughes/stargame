# Starlight Source Review

Review date: 2026-05-16

Source repo reviewed: `/home/matthew/Repos/starlight`

This document captures useful design and prototype findings from the Godot/C# version. It is not a 1:1 port checklist. The Unreal project should use Unreal's gameplay framework, actor/component model, data assets, Blueprints, and subsystems.

## Project Identity

Starlight is an open-world single-player space sim inspired by Freelancer.

The design has two major pillars:

- Pillar A: space flight, docking, trading, missions, combat, save/load, star-system simulation.
- Pillar B: first-person station interiors with modular station sections, FPS movement, NPCs, props, doors, and cover combat.

The pillars share data such as station identity, factions, reputation, mission state, and player state. They should not share low-level movement, rendering, or AI systems.

## Important Proven Ideas

### Space Simulation

- Floating origin: keep the player/camera near local origin and shift world presentation around them.
- Kepler rail orbits: celestial body positions are computed from game time rather than simulated by physics.
- Gravity wells / spheres of influence: bodies and stations define proximity zones used for HUD, docking, supercruise lockout, station keeping, and reference frames.
- Three-tier flight model:
  - normal flight for combat/docking
  - cruise for local approach
  - supercruise / phase compression for interplanetary travel
- Supercruise speed limits depend on star distance and nearby gravity wells.
- Station keeping: near a body or station, the ship inherits that body's reference-frame motion.
- Space navigation targets and autopilot matter. The docs call out deliberate `Goto` cancellation rather than accidental cancellation from regular input.

### Playable Slice

The Godot project evolved beyond a pure prototype. VS1 current state reports:

- main menu boot flow
- save/continue flow
- Sol sector flight
- supercruise with gravity lockout
- interdiction and phase-scrambler response
- ship combat with visible projectiles
- hostile patrol spawning and early combat AI
- station docking and launch flow
- reusable station interaction space
- hostile boarding interaction space
- commodity trading
- repair/recovery
- inventory/equipment display
- mission offer/accept/progress/turn-in
- mission waypoint targeting

For Unreal, this validates the desired vertical slice, but the implementation should be rewritten.

### Station Interiors

Station interiors are a separate gameplay context:

- modular kit on a 4m grid
- JSON-authored layouts in the Godot version
- runtime station builder
- FPS controller with sprint, crouch, slide, lean, jump, stamina, HP, and interaction ray
- cover points, squad coordination, hostile NPC states
- ambient NPC direction planned but incomplete
- props, doors, airlocks, themes, and procgen planned in phases

For Unreal, this suggests:

- first-person station mode should use `ACharacter`/Character Movement, not the ship pawn.
- modular pieces should be authored as Unreal meshes/Blueprint actors.
- early layouts can be hand-authored in editor or imported from data, but procgen should wait.
- station services should be data/UI first, physical rooms later.

### Data and Contracts

The Starlight docs repeatedly emphasize contracts before code:

- ship entity
- gravity well
- station identity
- commodity stack
- events
- save data
- mission definitions
- faction state

This should carry forward into Unreal. The exact class shapes should change, but the discipline should remain.

Recommended Unreal equivalents:

- `FShipState`, `FPlayerStateData`, `FGravityWell`, `FCommodityStack`, `FStationId`, `FMissionState`
- `UShipArchetypeDataAsset`
- `UEquipmentDataAsset`
- `UFactionDataAsset`
- `UStationDataAsset`
- `UStargameSessionSubsystem`
- `UStarSystemSubsystem`

## Key Godot Files Reviewed

- `CLAUDE.md`
- `docs/roadmap/ARCHITECTURE.md`
- `docs/roadmap/00_vision.md`
- `docs/roadmap/space_foundation.md`
- `docs/roadmap/01_orbital_mechanics_and_system_data.md`
- `docs/roadmap/02_ship_entity_and_flight_model.md`
- `docs/roadmap/station_interiors/README.md`
- `docs/vertical-slices/VS1/current_state.md`
- `docs/vertical-slices/VS4/current_state.md`
- `game/scripts/ship/ShipController.cs`
- `game/scripts/game/space/GameplayRoot.cs`
- `game/scripts/game/space/systems/StarSystemDefinition.cs`
- `game/scripts/station_interior/FPSController.cs`
- `game/scripts/station_interior/StationInteriorBuilder.cs`
- `game/scripts/station_interior/HostileNPC.cs`
- `game/scripts/game/runtime/GameSession.cs`
- `game/data/systems/sol.json`

## Unreal Translation Decisions

### Keep

- space/interior pillar split
- data-driven definitions
- floating-origin/reference-frame thinking
- orbital rail math
- gravity wells/SOI
- docking as a clear transition boundary
- first vertical slice discipline
- C++ contracts with designer-facing tuning

### Rewrite

- Godot node-tree architecture
- monolithic `GameplayRoot`
- Godot-specific station builder
- Godot-specific FPS controller
- Godot-specific UI and scene transition flow
- C# script layout

### Defer

- full physical station interiors
- station procgen
- full NPC schedules
- advanced cover peeking
- CommonUI adoption
- Gameplay Ability System adoption
- full content import/adaptation

## First Unreal Slice Recommendation

The first Unreal slice should prove the foundation in Unreal terms:

1. Boot into `frontier_test_01`, not Sol.
2. Resolve the start path through `frontier_test_start`.
3. Spawn the ship at `spawn_deep_space`.
4. Spawn placeholder targets for `ember`, `brink_watch`, and `frontier_gate_a`.
5. Populate a minimal navigation target registry from stable IDs.
6. Verify the existing ship pawn accepts input and moves in PIE.
7. Save/load current system and spawn zone.

The fuller fixture should later include one primary, two planets, a nested moon, an orbiting station, a deep-space station, an L2-style gate, an asteroid/resource zone, a gravity-lockout body, a deep-space spawn, and a near-station spawn.

Only after that should we expand toward combat, missions, and first-person station interiors.
