# C++ And Blueprint Ownership

The default rule is simple: C++ owns deterministic game state and runtime authority. Blueprints own presentation and authored variation.

Blueprints should configure and extend C++ systems. They should not become the source of truth for the sector, active system, orbit state, transition state, save state, docking state, or scale conversion.

## C++ Owns

Use C++ for:

- session state and save/load coordination
- current system ID and start profile resolution
- star catalog and route graph access
- active system build/teardown
- stable gameplay ID registries
- orbit math and simulation time
- coordinate frames and scale conversion
- supercruise state and lockout/dropout rules
- docking state and docking-port resolution
- jump gate activation and arrival placement
- generated system definitions
- validation and automation tests
- performance-sensitive ticking, pooling, and actor lifecycle
- NPC goal state, group state, patrol/pirate/flee policy, and AI decision reasons
- moving-frame AI targets, route sampling, prediction, and risk scoring
- Tier 2/Tier 3 logical AI updates that run without actors

C++ classes should expose tuning through `UPROPERTY` and deliberate Blueprint hooks. Blueprint-callable functions should exist for UI, presentation, debug, or designer interaction, not as the hidden implementation path for core game rules.

Blueprints may call intent APIs such as `RequestTargetSelection`, `RequestDocking`, or `RequestJump`.

Blueprints must not call generic setters for save-critical or session-critical fields such as current system, ship frame, docking state, route state, or simulation time. Expose those fields to Blueprint as read-only view models unless a specific command API validates the change in C++.

## Blueprints Own

Use Blueprints for:

- visual subclasses of C++ actors
- meshes, materials, lights, Niagara, audio, and post process assembly
- authored defaults and tuning values
- UI widgets and presentation
- debug visualization widgets
- camera shakes and atmospheric presentation hooks
- station/gate/planet visual variants
- test-level composition around C++ runtime systems
- comms barks, animation/VFX hooks, and authored presentation for NPC behavior
- designer tuning assets consumed by C++ AI policy

Blueprints may react to C++ state changes. They must not decide which system is loaded, calculate orbit positions, write save identity, own docking state, or own supercruise transition state.

Blueprints may implement Tier 1 behavior flavor through controlled hooks, but the persistent NPC brain stays in C++. A Blueprint actor should not decide that a pirate abandons an ambush, that a patrol owns a jurisdiction, or that a fleeing ship changes destination. It can present those decisions once C++ has made them.

Behavior Trees or StateTree may be used for Tier 1 realized actors only. They must not own persistent AI state.

Rules for BT/StateTree:

- no NavMesh `MoveTo` for normal space movement
- no blackboard object keys as persistent target identity
- blackboard/state data stores stable IDs, frame targets, and resume keys
- tasks call C++ steering/query services
- pooled actors stop/reset brain state, blackboards, perception, latent actions, timers, controller possession, target handles, and UI handles on release

## Data Assets Own Authored Content

Use Unreal assets for authored game content:

- star catalogs
- start profiles
- route graph definitions or route-generation settings
- star system definitions
- body visual profiles
- atmosphere profiles
- station profiles
- gate profiles
- ship archetypes
- commodity/economy/mission location references

Godot JSON is import/interchange input. The runtime target is Unreal-native assets and C++ structs unless runtime modding becomes a deliberate feature.

## Blueprint Compliance Checklist

Blueprint must not write:

- current system ID
- route state
- orbit phase
- simulation time
- save IDs
- docking state
- arrival target
- canonical ship location
- generated system definitions
- NPC goal state or saved goal
- patrol/pirate/flee decision state
- formation membership or leader assignment
- authoritative AI target frames

Blueprint must not:

- scan the world to discover core bodies or systems as normal gameplay flow
- use display names or Actor names as persistent IDs
- hard-code Sol-only paths, station lists, gravity wells, or map entries
- load production assets through constructor hard paths
- compensate for missing architecture with level-specific scripting
- use fixed world-space patrol splines for orbital traffic
- make Tier 2 or Tier 3 AI depend on spawned Actor references

If a Blueprint needs loops over all systems, manual string IDs, transition timing, save decisions, or coordinate conversion, that logic belongs in C++.

If a Blueprint needs to score route risk, police strength, pirate confidence, formation membership, or flee destinations, that logic belongs in C++.
