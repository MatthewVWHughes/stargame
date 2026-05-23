# Planet Rendering Direction

## Decision

Use KSP as the main reference for planet presentation, not for flight physics.

The problem to solve is visual scale and atmospheric transition, not orbital simulation. Many games already solve this with multiple planet representations rather than one object that works at every distance. The Unreal implementation should follow that established pattern.

This document is not the canonical runtime architecture. It is the rendering/scale direction for planets and atmospheres. Current runtime scope belongs in `runtime-architecture.md`, current data contracts belong in `system-data-contracts.md`, and historical sequencing remains in `build-roadmap.md` and `implementation-plan-m0.md`.

## Implementation Rule

Do not bring Godot architecture into Unreal unchanged.

The Godot version is useful as a reference for:

- nearby star catalog data
- system definition data
- simplified flight/orbit gameplay assumptions
- jump gate flow
- map/navigation behavior

It is not a template for Unreal implementation details.

Avoid:

- C#-style service patterns that fight Unreal object lifetime
- Godot node-tree assumptions mapped directly onto Actors
- hard-coded scene paths or string paths as the main runtime contract
- hand-rolled systems where Unreal already has a supported native concept
- building everything as generic UObject logic while ignoring Actors, Components, Subsystems, Data Assets, soft references, and world ownership
- relying on raw Unreal physics as the source of truth for solar-system scale simulation

Prefer:

- `UGameInstanceSubsystem` for persistent session state
- `UWorldSubsystem` for active world/system runtime
- Actors for loaded gameplay entities
- Actor Components for reusable behavior such as orbiting, gravity sources, gates, and map registration
- `UPrimaryDataAsset`, `UDataAsset`, Data Tables, or Unreal structs managed through native asset workflows
- JSON only as import/interchange data unless runtime modding becomes a deliberate requirement
- `UAssetManager`, `FPrimaryAssetId`, `TSoftObjectPtr`, and `TSoftClassPtr` for authored visuals and Blueprint classes
- registries keyed by stable gameplay IDs, not fragile scene paths
- deterministic game-space simulation with Unreal used as the current local representation

Do not use constructor hard paths, world scans, or global singleton builder objects as the production contract. Prototype shortcuts are acceptable only while proving a slice. Production systems should resolve actors through the active system registry, explicit component references, gameplay IDs, or soft asset references.

## C++ And Blueprint Ownership

The default rule is: C++ owns deterministic systems, data contracts, lifetime, performance-sensitive runtime code, and save-critical behavior. Blueprints own authored presentation, composition, tuning, and designer-facing variations.

Blueprints should configure and extend the core game systems. They should not be the source of truth for sector population, orbit math, system transitions, save state, or scale conversion.

### Write In C++

Use C++ for:

- subsystem classes such as session state, active system runtime, sector catalog access, and save/load coordination
- core data structs for stars, systems, bodies, orbits, gates, stations, atmosphere profiles, and route graphs
- Asset Manager integration and primary asset IDs
- deterministic orbit calculation and parent-relative body transforms
- sector route generation, route lookup, and fewest-jump pathfinding
- system build orchestration and actor registration
- stable gameplay ID registries for bodies, gates, stations, gravity wells, and navigation targets
- jump gate activation rules, transition state, and arrival placement ordering
- local flight state machines, cruise/supercruise rules, lockout/dropout rules, and gravity-well ramping
- floating-origin/world-origin coordination and scale conversion
- save/load serialization for current system, ship state, discovered systems, route state, and station/body references
- performance-sensitive ticking, pooling, spawning, and actor lifecycle management
- automated tests for catalog loading, route graphs, orbit math, system builds, and transitions

C++ classes should expose clear `UPROPERTY` tuning fields, Blueprint events only where presentation needs hooks, and `BlueprintCallable` functions only for intentional UI/designer interaction.

### Build In Blueprint

Use Blueprints for:

- visual subclasses of C++ actors, such as planet bodies, gates, stations, asteroid fields, and atmosphere layers
- component assembly and authored defaults for meshes, materials, Niagara systems, lights, audio, and post process
- material instance selection and visual tuning
- UI widgets for sector map, system map, HUD, targeting, and route display
- camera shakes, entry VFX, atmospheric audio hooks, and local presentation effects
- designer-authored variations of gates, stations, planet profiles, and local effects
- debug visualization widgets and editor-facing preview helpers
- level/test-scene composition around C++ runtime systems

Blueprints may react to C++ state changes, but they should not calculate canonical orbit positions, decide which system is loaded, own transition state, or store persistent gameplay identity.

### Use Data Assets For Authored Content

Use Unreal assets for authored game content:

- star catalogs
- route graph definitions or route-generation settings
- star system definitions
- body visual profiles
- atmosphere profiles
- station definitions
- gate definitions
- ship archetypes
- commodity/economy/mission location references

JSON from Godot should be treated as an import source or interchange format. The runtime target should be Unreal-native assets and structs unless runtime modding becomes a deliberate feature.

### Blueprint Boundaries

Blueprints must not:

- scan the world to discover core bodies or systems as normal gameplay flow
- use display names or actor names as persistent IDs
- hard-code Sol-only paths, station lists, gravity wells, or map entries
- own save-critical state without C++ serialization support
- implement hidden route, orbit, scale, or transition logic that C++ cannot test
- load assets through constructor hard paths
- compensate for missing architecture with level-specific scripting

If a Blueprint starts needing loops over all systems, manual string IDs, transition timing, save decisions, or coordinate conversion, that logic belongs back in C++.

## Starlight Fidelity Baseline

The first implementation target is behavior parity for the existing space
structure where it supports the primary frontier-sector loop, not a full
nearby-star campaign or Sol rebuild.

Preserve these behaviors as future-compatible data or deterministic Unreal
systems:

- nearby-star catalog around the home region, including the prepared roughly
  50-star list, as future map/reference material
- original sector coordinate mapping: galactic X to game X, galactic Z to game Y, galactic Y to game Z
- sector map display scale equivalent to the Godot `Ly = 2` convention, treated as display scale only
- Sol and Alpha Centauri as legacy authored/reference content to preserve later,
  not as current fixture or presentation targets
- generated route graph based on a connected minimum-spanning backbone plus short-range extra links
- special route content such as the Wolf 359 to Ross 154 natural wormhole
- sector route selection by fewest jumps for player-facing display, not weighted physical distance unless redesigned deliberately
- nested body definitions with parent-relative orbits
- simple deterministic orbital rails using semi-major axis, period, phase offset, eccentricity, and inclination
- jump gates anchored relative to bodies, including anti-star L2-style placement
- post-load arrival placement after orbiting bodies and gate anchors have resolved their live transforms

Eliminate these Godot shortcuts during implementation:

- hard-coded Sol-only station bindings
- hard-coded Sol-only gravity wells
- hard-coded combat spawn points
- hard-coded NPC trade route/station lists
- hard-coded system-map body entries
- substitution to Sol for unknown system IDs
- node paths as gameplay identity
- scene paths as the main data contract

The current Godot content uses JSON for systems, simple sphere visuals, scene visuals for special bodies, rings, asteroid belts, and jump gates. Unreal should preserve useful content as reference/import material, then upgrade rendering through native Unreal assets and layered representation when that work is explicitly selected.

## Keep Simple Flight Physics

Keep the simpler Godot-derived space flight model unless a concrete gameplay limitation appears.

Reasons:

- The game is not currently a hardcore orbital mechanics sim.
- KSP-style patched conics and sphere-of-influence gameplay would add major complexity without fixing planet visuals.
- Unreal physics is not suitable as the source of truth for solar-system scale simulation.
- Long-range system state should remain game-space data, with actors used as local render/gameplay representations.

Unreal changes the rendering and tooling options, not the need for a separate simulation model. Large World Coordinates help, but they do not remove the need for careful scale management.

## System Population Architecture

The game needs three runtime layers.

### Sector Space

Sector space is the lightweight interstellar layer. It contains the nearby-star catalog that the Godot version had prepared, including the roughly 50 known stars near the home region.

Responsibilities:

- store star IDs, display names, stellar classes, and positions
- support the old nearby-star catalog presentation later without making Sol the runtime default
- store route links, jump lanes, wormholes, or generated connections
- provide pathfinding between systems
- track discovered/known stars and route visibility
- feed the sector map without spawning full star systems

The original Godot sector map used a compact star list and generated links. Unreal should extract that into data. The map can present an authored start region without making Sol, Earth, or any specific real-world system the runtime default.

### Active Star System

Only one full star system should be loaded for normal gameplay.

"Loaded" means simulated, registered, and represented as needed. It does not mean every body is materialized at true physical transform scale as a full-detail Actor.

Responsibilities:

- load the current system definition
- spawn the star, planets, moons, stations, asteroid belts, gates, and gameplay volumes
- evaluate and project deterministic orbit state from authoritative time
- register bodies, gates, stations, gravity wells, and navigation targets
- provide data to the local system map and HUD
- cleanly unload/rebuild when transitioning to another system

This replaces the Godot `StarSystemScene` and `StarSystemBuilder` idea with Unreal-native runtime ownership.

Recommended Unreal pieces:

- `UStarSystemSubsystem` as the active world/system manager
- `AStarSystemRootActor` as the root of the generated system
- `UStarSystemBuilder` as a short-lived `UObject`, subsystem method, or plain C++ helper invoked by the subsystem
- `AOrbitalBodyActor` plus `UOrbitComponent` for deterministic body motion
- registry components or subsystem maps keyed by stable IDs such as `frontier_test_01/ember`

The builder must not become a persistent global service with hidden lifetime. Spawned actors belong to the world and the active system root, not to the builder.

### Local Flight Space

Local flight space is the player-relative representation used for the ship, camera, combat, NPCs, particles, local atmospheric effects, and nearby interactables.

Responsibilities:

- keep the player ship stable and responsive
- use UE5 Large World Coordinates where useful
- support Unreal world-origin rebasing or player-relative shifting where needed
- keep combat/projectiles/NPCs in reliable Unreal-scale coordinates
- render only the currently relevant local representations
- avoid using full solar-system coordinates directly as Actor transforms

Long-range positions should remain game-space data. Unreal Actors should represent what is currently loaded and visible around the player.

Physics-sensitive actors, projectiles, particles, and replicated gameplay should stay inside a small local coordinate bubble. Do not invent a parallel transform hierarchy if an Unreal world-origin strategy solves the problem cleanly.

## Coordinate Frames

The game needs explicit coordinate frames:

| Frame | Purpose |
| --- | --- |
| Canonical sector frame | Stable positions for the nearby-star catalog |
| Home-region display frame | Player-facing map/navigation view centered on the current authored start region |
| Active system frame | Current loaded star system in Unreal world space |
| Player-relative frame | Local flight, camera, combat, particles, and atmosphere effects |

Do not mix these accidentally. The sector map, active system, and local flight code should convert between frames deliberately.

## Scale Contract

KSP-like scale means coherent perceived scale and stable transitions, not a requirement for true physical distances everywhere.

| Scale | Purpose | Owner |
| --- | --- | --- |
| Catalog scale | Nearby-star positions in light years | Sector catalog |
| Display scale | Sector/system map readability | Map UI/view model |
| Simulation scale | Deterministic orbits, gate anchors, travel distances | Active system subsystem |
| Render proxy scale | Far planets, moons, stars, atmosphere silhouettes | Planet/body representation components |
| Local Unreal scale | Ship, camera, combat, particles, local atmosphere | Active world and local flight systems |

Every body must separate:

- physical reference radius
- visual radius
- collision radius
- gravity well radius
- supercruise lockout radius
- atmosphere entry radius
- map icon scale

Those values may intentionally differ. The conversion between simulation position and render position should be owned by the active system representation layer, not scattered across ship, camera, map, and planet code.

## Data Needed

### Star Catalog

Each nearby star/system needs:

- system ID
- display name
- stellar class
- position in the canonical sector frame
- discovery/known state
- route tier or gameplay category
- optional seed for generated content

### Route Graph

The interstellar graph needs:

- source system ID
- destination system ID
- distance
- route type
- visibility/discovery state
- traversal requirements

This can initially reproduce the Godot behavior: a generated connected graph plus extra short-range links.

### Star System Definition

Each loaded system needs:

- system ID and display name
- one or more stellar bodies
- explicit light/gravity/render source selection for multi-star systems
- bodies
- orbits
- visual profiles
- stations
- jump gates
- asteroid belts
- gravity wells
- spawn zones
- map metadata

Separate already-authored system data from runtime content that was hard-coded in Godot. Stations, gravity wells, map entries, combat spawn points, NPC station lists, trade routes, and station assist targets should become authored system data instead of code branches for Sol.

### Body Definition

Each body needs:

- stable body ID
- display name
- parent body ID
- body type
- physical reference radius
- visual radius
- collision radius
- gameplay/gravity radius
- supercruise lockout radius
- atmosphere entry radius
- orbit parameters
- visual profile
- atmosphere profile, if any
- gravity profile, if any
- map color/icon/tags

The preserved orbit model is a deterministic parent-relative rail: semi-major axis, period, phase offset, eccentricity, and inclination in degrees. The Unreal implementation can be native C++, but the first implementation target should match that behavior before introducing any different orbital model.

### Transit Definition

Each gate or transition point needs:

- gate ID
- destination system ID
- destination arrival gate ID
- anchor body ID
- optional parent/root ID
- anchor distance
- L1/L2-style placement rule, with anti-star L2 as the default inherited behavior
- activation range
- display name
- visual class/reference

## Transition Flow

System-to-system travel should work through data and session state:

1. Player activates a gate or route transition.
2. The session subsystem records destination system ID, arrival gate ID, and ship state.
3. The current active system is unloaded or the gameplay map is reloaded.
4. The destination system definition is loaded.
5. The active system root is rebuilt from data.
6. The arrival gate is resolved by stable ID.
7. The ship is placed near the arrival gate after the system has initialized.
8. HUD, map, gravity, stations, NPC traffic, and nav targets reconnect from registries.

No system transition should depend on hard-coded Sol actor paths.

Arrival placement has an ordering requirement: orbit components and gate anchors must run first so the arrival gate has a valid live transform. The Godot version waited multiple physics frames before placing the ship near the gate; Unreal needs an equivalent explicit readiness signal or delayed placement step.

## In-System Travel Transitions

Keeping simple flight physics still requires deliberate travel modes.

Preserve the design class from Godot:

- normal flight for docking, combat, and close maneuvering
- cruise or supercruise for in-system travel
- gravity-well ramping near bodies
- forced dropout or lockout near stations, planets, and hazards
- stable handoff from cruise-scale navigation to local body/station approach
- clear navigation targets and distance/speed feedback

These should be Unreal-native movement modes or ship state components, not a direct port of Godot controller code.

## Planet Representation State Machine

Planets and atmospheres need explicit representation states.

Planet and atmosphere representation is visual/travel presentation only. It
must not imply planet landing, surface gameplay, walkable planet terrain, or
low-altitude terrain flight. The allowed target is readable orbital approach,
upper-atmosphere skim, gas-giant/cloud-layer presence, and clear lockout/damage
rules where needed.

| State | Responsibility |
| --- | --- |
| Map/scaled proxy | Sector/system readability, icon/silhouette, route context |
| Far orbital | Distant body, broad limb, rings, major cloud/terrain color |
| Near orbital | Higher-detail body shell, local star lighting, horizon continuity |
| Atmosphere entry | upper-atmosphere haze, color shift, cloud deck approach, speed/depth cues |
| Local atmosphere | player-local fog/clouds/particles/turbulence around the ship, without surface approach |
| Deep layer | gas-giant/hostile-atmosphere readability, storm/depth pressure, damage or lockout |

Each state needs enter distance, exit distance, hysteresis, blend duration, owning components, and debug visualization. No transition should depend on a single threshold with no fade plan.

The invariant is horizon continuity: the player should not see the planet limb, cloud deck, or atmosphere shell jump when the representation changes.

## Atmosphere Profile Contract

Atmospheres need authored profiles instead of arbitrary fog zones.

Each atmosphere profile should define:

- entry altitude/radius
- haze falloff
- cloud-top altitude
- dense layer altitude
- deep/crush layer altitude
- color grading and exposure behavior
- wind/turbulence bands
- local VFX hooks
- audio hooks
- damage, pressure, or lockout rules
- sun direction and planet-up behavior

Local fog and cloud effects must be planet-frame driven. They should understand planet center, local up, sun direction, wind bands, and cloud deck altitude. They also need to survive world-origin shifts without popping or drifting.

Origin-shift rendering policy:

- body, ring, atmosphere, gate, station, and far-proxy actors store stable IDs and render-state identifiers, not canonical positions
- on origin shift, representation components receive the new bubble origin after logical frames are evaluated and before visibility/LOD decisions
- components reproject from `FFrameResolvedTransform` or planet-frame parameters; they do not offset their previous actor transform incrementally
- far proxies outside the post-shift local bubble may demote to map/scaled representation instead of preserving a stale near-space actor
- local atmosphere VFX tied to the active planet reprojects from planet center, planet up, and wind phase at authoritative/presentation time
- transient near-camera particles, decals, and audio one-shots may cull and respawn from presentation state; persistent storms, rings, and hazards must reproject or demote from logical state
- if horizon continuity cannot be preserved for the active planet during a shift, the rendering layer reports a failed representation transition so flight can defer the shift or use a fade policy

Ordering relative to flight origin shift:

1. flight freezes local simulation and selects the new bubble origin
2. active-system frame queries resolve body/station/gate transforms at the shift time
3. planet/body representation components reproject or demote
4. local atmosphere, fog, cloud, ring, and VFX anchors rebuild from planet-frame data
5. culling and LOD selection run after reprojection, not before
6. debug records old/new representation state and any culled transient presentation objects

Different body types need different profiles:

- airless moons: surface/limb readability, no atmosphere stack
- rocky planets: terrain, horizon, optional thin atmosphere
- terrestrial atmospheres: cloud/weather layers, entry haze, and distant horizon
  presentation only; no landing or ground gameplay
- gas giants: cloud-top, deep bands, volumetric local layers, crush/deep lockout
- ringed bodies: ring plane, shadowing, near-ring readability
- stars: light source, corona/proxy representation, exclusion/damage radius

## Rendering Model

Build planets as layered representations:

| Distance/Mode | Responsibility |
| --- | --- |
| Scaled space | Distant planet body, moons, orbital readability, broad atmosphere rim |
| Near planet | Higher-detail body/cloud/atmosphere representation near the player |
| Atmospheric entry | Haze, color shift, depth cueing, cloud/deck silhouettes |
| Local atmospheric flight | Player-local fog/cloud volumes, storm cues, particles, turbulence, audio later |

No single mesh, material, or `UVolumetricCloudComponent` should be expected to carry all of these jobs.

## KSP Lesson To Preserve

KSP feels coherent because it manages representation changes across scale. It does not render a planet the same way from map view, orbit, high atmosphere, and low flight.

For this project, copy that architecture:

- separate far and near planet visuals
- explicit transition rules between representations
- atmosphere as its own layer
- local effects around the ship for flight-through readability
- planet/moon/system scale handled by game logic, not by raw Unreal physics

## Unreal Direction

Use Unreal for:

- materials and atmosphere shells
- volumetric or local fog where it supports the visual target
- Niagara for wisps, lightning, dust, storm motion, and near-camera speed cues
- post process/color grading for atmospheric entry depth
- Blueprint tuning for visual thresholds

Avoid:

- treating stock Unreal volumetric clouds as a complete gas giant solution
- relying on many arbitrary per-planet volumetric cloud actors without validating engine assumptions and performance
- building atmosphere from many obvious fog blobs
- switching the game to KSP-like physics before the visual scale problem is solved
- coupling planet rendering directly to Unreal physics bodies

Use Unreal volumetric clouds only where they are the right native feature for the current active body or local flight layer. Otherwise prefer material shells, impostors, local fog volumes, Niagara, and post process as representation tools.

## Validation Targets

Every planet/system rendering pass needs concrete checks:

- sector map view: nearby stars, selected route, and fewest-jump path are readable
- system map view: loaded bodies, stations, and gates come from data, not hard-coded Sol paths
- high orbit: planet limb, rings, moons, star lighting, and atmosphere rim are stable
- low orbit: far proxy to near body transition has no visible pop
- entry: haze, color grading, and cloud deck align with the planet horizon
- cloud-top/local flight: local fog and VFX move with the planet frame, not the camera alone
- deep layer: lockout, pressure, damage, or exit rules are clear
- origin shift: atmosphere, VFX, gates, and body proxies do not jump or drift
- origin shift ordering: active planet reprojects before LOD/cull, while transient effects either reproject from logical state or cull by declared policy

Acceptance criteria should include horizon continuity, no one-frame representation pops, stable framerate at the intended test distance, and debug visualization for active representation state and scale conversions.

## Next Practical Step

Do not rebuild planet rendering as part of the current foundation unless the
selected task is explicitly a planet/scale rendering pass. The active fixture
can keep crude body/station/gate placeholders while player-facing loop work
continues.

When planet rendering resumes, rebuild it as a deliberate
scaled-space/near-space system. Prove each representation separately before
blending them, and keep the runtime/data ownership aligned with
`runtime-architecture.md`, `system-data-contracts.md`, and
`cpp-blueprint-ownership.md`.
