# Runtime Architecture

This document defines the Unreal-native runtime architecture for Stargame.

The aim is not to recreate Godot in Unreal. The aim is to preserve the useful game/data shape while letting Unreal own runtime lifetime, assets, actors, components, editor workflows, and validation.

The architecture must eventually support the systems listed in `future-systems-support.md`: space flight, FPS station mode, inventory, NPC traffic/trading, economy, mining, trading, combat, ship ownership/equipment, factions, quests, sectors, and communications. Early milestones should expose stable IDs and reference paths for those systems without implementing them prematurely.

NPC traffic and economy must follow `simulation-tiering-and-economy.md`: logical simulation first, actors only for the near-player interactive slice. Space AI must follow `space-ai-and-dynamic-orbits.md`: decisions resolve against moving anchors, route samples, and predicted reference frames rather than static Unreal locations.

## Architecture Rule

Preserve from Godot:

- nearby star catalog intent
- system definition content
- simplified orbit/flight assumptions
- jump gate flow
- map/navigation behavior
- useful authored content such as Sol and Alpha Centauri

Do not preserve from Godot:

- C# service-style architecture
- node paths as gameplay identity
- scene paths as the primary runtime contract
- hard-coded Sol-specific setup
- fallback-to-Sol behavior
- direct node-tree assumptions mapped onto Unreal Actor hierarchies

## Runtime Layers

The game has three runtime layers.

### Sector Layer

The sector layer is lightweight interstellar data.

`UStarCatalogSubsystem` or its asset-backed C++ equivalent is the single write owner for this layer. It owns:

- star catalog
- discovered/known state
- route graph
- jump/wormhole connectivity
- sector map data
- start-system selection

It does not spawn full star systems.

It also does not spawn NPC ships. Distant traffic and economy are logical data owned by persistent simulation systems.

Session, active-system, UI, HUD, save, AI, map, and debug systems are read-only consumers of catalog, route, discovery, and start-selection state. They may request mutations only through explicit catalog subsystem commands that validate the write, update the catalog-owned state, and emit change events. No other subsystem may independently rewrite discovered routes, select a different start system, or treat a cached catalog copy as authoritative.

### Active System Layer

The active system layer owns the currently loaded star system.

It owns:

- active system definition
- system build/teardown
- orbit/frame evaluation cache from authoritative simulation time
- generated body/station/gate actors
- registries
- map/debug view models
- active-world wrappers and caches for headless frame/query results

Only one full active system should be built for normal gameplay. "Built" means represented as needed, not every body materialized at true physical scale with full-detail actors.

### Local Flight Layer

The local flight layer owns the player-scale representation.

It owns:

- player ship control
- camera/HUD interaction
- local combat/projectiles
- local station approach
- local atmosphere and VFX
- player-relative/world-origin stability

Long-range positions remain game-space data. Unreal Actor transforms represent the local/current view of that data.

NPC ships only enter this layer after promotion from logical traffic data. Ships outside the realization radius remain Tier 2 or Tier 3 logical records, not hidden actors.

## Core Unreal Types

Required C++ systems:

- `UStargameSessionSubsystem`
- `UStarCatalogSubsystem` or asset-backed catalog access
- `UStarSystemSubsystem`

Core ownership:

- `UStarCatalogSubsystem` owns star catalog lookup, route graph state, discovery mutations, and start-profile/start-system selection.
- `UStargameSessionSubsystem` owns the current session state, save/load coordination, pending arrival, and player ship persistence. It stores selected IDs from the catalog owner, but it does not mutate catalog, route, or discovery state directly.
- `UStarSystemSubsystem` owns only the active system lifecycle, active registries, active-world query caches, and active-world view models. Headless frame/orbit/route conversion belongs to `UOrbitRouteFrameQueryService` or the equivalent C++ service. The active subsystem reads catalog/route/discovery data for build and debug purposes, but it does not become a second source of truth.
- UI, HUD, map, save serializers, and AI planners consume view models or subsystem queries and never write canonical catalog, route, discovery, start selection, orbit, docking, transition, or save state.

Required core actors/components:

- `AStarSystemRootActor`
- `UStargameIdentityComponent`
- `UNavigationTargetComponent`
- `UOrbitComponent`
- `UGravityWellComponent`
- `UDockingPortComponent`
- `UTransitEndpointComponent`
- `UReferenceFrameComponent`

Base actors such as `AOrbitalBodyActor`, `AStationActor`, and `ATransitGateActor` are acceptable. Shared identity, registration, navigation, orbit, gravity, docking, and transition behavior should be components where practical.

## Registry Contract

The active system registry is the production lookup path.

Required registries:

- bodies by body ID
- stations by station ID
- docking ports by station/port ID
- gates by gate ID
- gravity wells by ID/body
- navigation targets by ID
- map entries by ID
- spawn zones by ID

Rules:

- `UStarSystemSubsystem` owns registry mutation
- components may request registration, but the subsystem accepts it only for the active build generation
- duplicate IDs are hard errors in development
- registry queries are valid only after `OnSystemBuildComplete`
- actors unregister on explicit pool release and on `EndPlay`
- rebuilds invalidate stale handles
- UI/HUD/debug reads from registries or view models, not world scans

Every registry entry stores:

- stable gameplay ID
- entry type
- active build generation
- weak actor/component pointer
- source definition pointer or ID
- readiness state

Registry handles are not persistent save data. Saves store IDs and frames only.

World scans and Actor names are not allowed in milestone acceptance paths. They may be used only for explicit debug diagnostics.

## Reference Frames

Required frame types:

- sector/catalog position
- active-system simulation position
- body-relative position
- station/docking-port-relative position
- local Unreal transform

Save data must store logical frame and anchor IDs, not raw Actor paths.

## Sol Policy

Sol is legacy authored content to preserve later. It is not the first architecture validation system.

The first Unreal validation system is `frontier_test_01`.

No startup path may silently fallback to `sol`. Unknown systems should fail loudly in development.

Failure means the session/start-profile API returns a failure or error enum, logs through a Stargame startup category at Error level, does not build a system, and shows a visible PIE/debug error.

## Prototype Debt

Retire these shortcuts:

- `sol` runtime default
- startup references to `Earth`, `Mars`, `Luna`, or `Sol`
- `TActorIterator` body/atmosphere discovery
- constructor hard paths for production assets
- hard-coded HUD target labels
- hard-coded station/gravity/map/NPC lists
- Blueprint-owned canonical gameplay state
