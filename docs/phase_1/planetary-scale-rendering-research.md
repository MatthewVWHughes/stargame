# Planetary Scale And Rendering Research

Research snapshot from May 23, 2026. This note gathers online references for
planetary scale, rendering, atmospheres, origin shifting, game precedents, and
reusable technology. It supports `planetary-scale-rendering-plan.md`; it is not
an implementation plan by itself.

Scope guard: references to terrain, spherical terrain plugins, and true
ground-to-space planetary systems are research inputs only. They do not change
the current game scope: no arbitrary planet landing, no walkable planetary
surface, and no whole-planet terrain gameplay unless the canonical project docs
are deliberately updated.

## Executive Summary

The common industry pattern is clear:

- keep authoritative positions in high-precision logical coordinates
- keep gameplay, physics, particles, and most rendering in a small local bubble
- use origin shifting or camera-relative transforms when needed
- separate distant planet representation from close terrain/atmosphere
- use explicit LOD/representation transitions rather than one planet mesh
- treat travel compression as game design, not just raw speed

Unreal Engine 5.7 gives us useful foundations, especially Large World
Coordinates, Sky Atmosphere, Volumetric Clouds, Niagara LWC support, World
Partition, Geometry Script, and Runtime Virtual Texturing. None of those alone
solves fictional space-game planets. The likely Stargame path is a hybrid:

- UE5 LWC for C++ transforms and large coordinate safety
- Stargame-owned logical system coordinates and local bubble policy
- built-in Sky Atmosphere and Volumetric Clouds for first atmosphere tests
- far orbital planet shells plus near orbital/detail representation
- later evaluation of commercial spherical terrain plugins only if we need
  true terrain-style representation sooner than we can build it, and only after
  simple spheres plus atmosphere shells fail a player-facing acceptance test

## Unreal Technologies

### Large World Coordinates

UE5 Large World Coordinates (LWC) changes core coordinate types to double
precision and greatly expands practical world size. Epic still documents
shipping caution and calls out special cases for shader, Blueprint, Niagara, and
physics paths.

Useful lessons:

- use `double` in C++ APIs where scale matters
- audit any Blueprint-callable function that still exposes `float`
- do not assume GPU/material paths are automatically safe at solar-system scale
- keep actor-space values small for physics, VFX, and local gameplay

References:

- Unreal LWC documentation: https://dev.epicgames.com/documentation/unreal-engine/large-world-coordinates-in-unreal-engine-5
- LWC rendering overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/large-world-coordinates-rendering-in-unreal-engine-5
- LWC in Niagara: https://dev.epicgames.com/documentation/en-us/unreal-engine/large-world-coordinates-in-niagara-for-unreal-engine

### World Partition And HLOD

World Partition is valuable for streaming large authored areas, but it is not a
planetary terrain solution by itself. HLOD can replace distant cells with
proxies, which may be useful around authored stations or local surface outposts.
It does not replace a sphere/planet LOD algorithm.

References:

- World Partition: https://dev.epicgames.com/documentation/unreal-engine/world-partition-in-unreal-engine
- World Composition / legacy origin rebasing context: https://dev.epicgames.com/documentation/unreal-engine/world-composition-in-unreal-engine

### Georeferencing And Cesium

Cesium for Unreal is the strongest existing Unreal reference for globe-scale
origin shifting and streamed planet data. It uses georeferencing, globe anchors,
3D Tiles, screen-space-error refinement, and an origin shift component.

Cesium is most relevant as an architecture reference. It is Earth/WGS84 and
3D-Tiles oriented, so it should not own Stargame fictional planet generation
unless we deliberately choose a geospatial/ECEF model.

Lessons:

- every rebased object needs an anchoring discipline
- origin shifts must be centralized
- tile/terrain LOD should be screen-space-error based, not whole-planet loaded
- objects without the correct anchor/component drift when origin changes

References:

- Cesium for Unreal: https://cesium.com/learn/unreal/
- Placing objects and origin shifting: https://cesium.com/learn/unreal/unreal-placing-objects/
- Cesium sublevels: https://cesium.com/learn/unreal/unreal-sublevels/
- Cesium Unreal GitHub, Apache-2.0: https://github.com/CesiumGS/cesium-unreal

### Sky Atmosphere

Unreal Sky Atmosphere is the first built-in technology to test for ground-to-
space atmospheric rendering. It supports planet radius, atmosphere height,
scattering controls, LUTs, aerial perspective, and space-view transmittance.

Risks:

- stock setup is best suited to one main active atmosphere
- non-Earth scales need careful tuning
- multiple visible planet atmospheres may require custom shell/material
  representations

References:

- Sky Atmosphere component: https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-atmosphere-component-in-unreal-engine
- Sky Atmosphere reference: https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-atmosphere-reference
- Sebastian Hillaire UE atmosphere reference implementation, MIT: https://github.com/sebh/UnrealEngineSkyAtmosphere

### Volumetric Clouds

Unreal Volumetric Clouds are material-driven raymarched volumes intended to work
with Sky Atmosphere, Directional Light, and Sky Light. They are useful for
atmospheric planets and maybe gas-giant cloud tests, but should be treated as an
expensive layer with strict budgets.

Risks:

- raymarch and shadow sample costs can be high
- not a full gas giant solution by itself
- material graph limitations make some dynamic branching patterns awkward
- likely best as one local/active layer, not many arbitrary clouds around many
  planets

References:

- Volumetric Cloud component: https://dev.epicgames.com/documentation/unreal-engine/volumetric-cloud-component-in-unreal-engine
- Volumetric Cloud material: https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-cloud-material-in-unreal-engine

### Geometry Script, Dynamic Mesh, Runtime Mesh

For experimental planet meshes, Unreal Geometry Script and Dynamic Mesh are
useful for editor/prototype generation. Runtime-generated planet terrain may
need a stronger runtime mesh substrate or a plugin.

References:

- Geometry Script users guide: https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-script-users-guide
- Geometry Script reference: https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-script-reference-in-unreal-engine
- Gradientspace Geometry Script FAQ: https://www.gradientspace.com/tutorials/2022/12/19/geometry-script-faq
- RealtimeMeshComponent, MIT: https://github.com/TriAxis-Games/RealtimeMeshComponent

### Runtime Virtual Texturing

Runtime Virtual Texturing can help cache landscape or local terrain shading over
large areas. It is probably more useful for local terrain patches/stations than
for a full spherical planet solution.

References:

- Runtime Virtual Texturing: https://dev.epicgames.com/documentation/unreal-engine/runtime-virtual-texturing-in-unreal-engine
- Runtime Virtual Texturing quick start: https://dev.epicgames.com/documentation/unreal-engine/runtimevirtual-texturing-quick-start-in-unreal-engine

## Shipped Game Lessons

### Kerbal Space Program / KSP2

KSP is the closest conceptual reference for perceived scale and representation
handoff. KSP/KSP2 use separate distant/scaled-space representation and close
terrain representation. KSP2 describes procedural quad sphere terrain, compute
shader generation, floating-origin-relative planet mesh data, and dithered
cross-fades between distant and close representations.

Actionable lessons:

- separate far planet shell from close terrain
- generate nearby planet vertices relative to the active origin
- keep procedural material inputs stable at planet coordinates, not mesh LOD
- separate visual terrain LOD from collision LOD
- profile terrain and atmosphere in milliseconds from day one

References:

- KSP2 planet tech: https://www.kerbalspaceprogram.com/news/dev-diaries-developer-insights-12-planet-tech
- KSP2 atmosphere: https://www.kerbalspaceprogram.com/news/developer-insights-22-skys-the-limit
- KSP2 early graphics/performance: https://www.kerbalspaceprogram.com/news/developer-insights-18-graphics-of-ksp2-early-access
- KSP2 orbit tessellation: https://www.kerbalspaceprogram.com/news/dev-diaries-orbit-tessellation
- Kerbol system scale reference: https://kerbalspaceprogram.fandom.com/wiki/Kerbol_System

### Star Citizen

Star Citizen's public planet-tech discussion points to 64-bit positioning,
camera-relative rendering, inverted depth buffer, local terrain chunks, on-demand
LOD, procedural surface texturing, fixed memory budgets, and physically based
atmospheric scattering.

Actionable lessons:

- camera-relative rendering is still valuable even with high-precision world
  positions
- terrain chunks need fixed budgets and local GPU-friendly coordinates
- atmospheres should work from space to close approach, with Stargame's gameplay
  scope stopping before landable terrain
- authored and procedural content can be layered

References:

- RSI monthly report with planet-tech notes: https://robertsspaceindustries.com/en/comm-link/transmission/15155-Monthly-Studio-Report-December-2015
- Pupil-to-planet coverage: https://www.gamewatcher.com/news/2015-17-12-star-citizen-releases-new-from-pupil-to-planet-video-to-show-off-planetary-procedural-generation

### Space Engineers

Space Engineers uses voxel planets and explicitly solved distant planet
visibility as a separate rendering problem. The team also kept speed constrained
because high speed destabilized physics and collisions.

Actionable lessons:

- distant planets may need a separate render path from normal world objects
- do not fix travel feel only by raising physical velocity
- collision stability and speed caps are design constraints
- planet definitions can be data-driven with height, biome, material, ore,
  atmosphere, weather, and LOD settings

References:

- Space Engineers planet dev blog: https://blog.marekrosa.org/2015/04/guest-post-by-ondrej-petrzilka-space_17/
- Planet generator definition docs: https://spaceengineers.wiki.gg/wiki/Modding/Reference/SBC/PlanetGenerator_Definition

### Elite Dangerous

Elite's Stellar Forge is a strong reference for deterministic, science-inspired
system and planet generation. It mixes catalog seed data, procedural generation,
and art direction rather than pure randomness.

Actionable lessons:

- use deterministic generation with stable seed/input data
- treat procedural generation as authored rules plus artist-tuned detail
- inject hand-authored sites when gameplay or story needs them

References:

- PC Gamer Stellar Forge article: https://www.pcgamer.com/the-mind-bending-science-behind-the-planets-of-elite-dangerous/
- Space.com David Braben interview: https://www.space.com/31366-elite-dangerous-stellar-forge-interview.html

### No Man's Sky

No Man's Sky demonstrates deterministic procedural planets regenerated from
seeded rules. Its public technical notes describe cube/sphere planet generation,
voxel/marching-cubes terrain evolution, GPU planetary object rendering, texture
streaming, LOD generation, and procedural mesh optimization.

Actionable lessons:

- regenerate from stable seeds instead of storing every detail
- combine procedural rules with authored asset libraries
- avoid pure random generation without art direction
- streaming, LOD, and procedural object rendering are part of the same pipeline

References:

- GDC Vault, Continuous World Generation: https://www.gdcvault.com/play/1024265/Continuous-World-Generation-in-No%20%2814.08.2018%29
- Worlds Part I technical notes: https://www.nomanssky.com/worlds-part-i-update/
- GamesBeat procedural art coverage: https://gamesbeat.com/unraveling-the-mysteries-of-no-mans-skys-18-quintillion-planets/

### Outer Wilds

Outer Wilds is a useful counterexample: very small handcrafted planets, dense
simulation, and a player-centric origin approach. Its scale avoids most
planetary LOD problems by making the whole solar system intentionally tiny.

Actionable lessons:

- small systems can work if systemic density is high
- fully simulated moving worlds create unusual streaming/collision constraints
- player-centric origin tricks are viable but should be deliberate

References:

- Outer Wilds making-of: https://www.gamesradar.com/the-making-of-outer-wilds/
- Unity interview context: https://mcvuk.com/business-news/shadowgun-void-bastards-gtfo-and-outer-wilds-the-many-flavours-of-unity/

## Reusable Technology Options

### Strong Candidates To Evaluate

| Option | Fit | License / Cost | Risk |
| --- | --- | --- | --- |
| Cesium for Unreal | Globe anchoring, origin shifting, 3D Tiles, geospatial references | Apache-2.0 | Earth/geospatial first; may not fit fictional planets |
| WorldScape | Spherical/flat clipmap terrain for Unreal | Commercial | collision cost and vendor/update risk |
| Spherescape | UE 5.7 spherical terrain using landscapes on a sphere | Commercial early access | new vendor, activation, production maturity unknown |
| PlanetGen on Fab | Runtime procedural spherical planet chunks | Commercial | new/no rating; ProceduralMeshComponent validation needed |
| EXOSKY Planets | GPU/material full-scale rocky/barren planet visuals | Commercial/beta | likely visual-first; collision/gameplay pipeline uncertain |
| Voxel Plugin | voxel terrain/destruction/caves | commercial plus legacy MIT pieces | spherical planets not default path |
| RealtimeMeshComponent | runtime mesh substrate for custom terrain | MIT | docs/version compatibility need validation |

Adoption gates for any external planet, terrain, atmosphere, or cloud plugin:

- compatible with the project Unreal version and C++ build pipeline
- source availability or acceptable vendor support path
- license/cost acceptable for the project
- works with Stargame stable IDs, logical coordinates, local bubble, and origin
  shifting
- supports deterministic loading or deterministic enough generated outputs for
  save/load and route/debug reproducibility
- has clear LOD, collision, and transition behavior
- can be disabled or replaced without rewriting core data contracts
- passes a player-facing test better than high-segment spheres plus simple
  atmosphere/cloud shells

References:

- Cesium for Unreal: https://github.com/CesiumGS/cesium-unreal
- WorldScape terrain clipmaps: https://iolacorp-1.gitbook.io/worldscape-plugin/getting-started/terrain-clipmaps-technology
- Spherescape: https://spherescape.dev/
- PlanetGen on Fab: https://www.fab.com/listings/96c36654-f244-4c75-a527-cfaaab2d5ac4
- EXOSKY Planets: https://docs.exoskyplanets.com/
- Voxel Plugin licensing: https://docs.voxelplugin.com/resources/licensing
- Voxel Plugin GitHub org: https://github.com/VoxelPlugin
- RealtimeMeshComponent: https://github.com/TriAxis-Games/RealtimeMeshComponent

### Atmosphere And Gas Giant References

No mature drop-in "gas giant gameplay planet" solution stood out. Gas giants
should probably be treated as an art/rendering pipeline:

- banded procedural surface or cloud-top shader
- separate atmosphere/cloud shell
- optional raymarched local storm/deep-layer volumes
- strict performance budgets
- gameplay pressure/heat/radiation volumes separate from visuals

References:

- UE Sky Atmosphere: https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-atmosphere-component-in-unreal-engine
- UE Volumetric Clouds: https://dev.epicgames.com/documentation/unreal-engine/volumetric-cloud-component-in-unreal-engine
- sebh UE atmosphere source reference: https://github.com/sebh/UnrealEngineSkyAtmosphere
- Bruneton sky model reference, MIT: https://github.com/diharaw/bruneton-sky-model
- Older UE precomputed atmosphere plugin: https://www.unrealengine.com/marketplace/en-US/product/precomputed-atmosphere
- Volumetric Nebula and Clouds plugin thread: https://forums.unrealengine.com/t/volumetric-nebula-and-clouds-plugin-now-available-on-fab-marketplace/2589317
- TSL gas giant shader reference: https://boytchev.github.io/tsl-textures/docs/gas-giant.html

### Procedural Terrain And Noise References

Useful building blocks if Stargame owns custom planet terrain:

- cube-sphere or icosphere mesh topology
- quadtree/chunked LOD
- geometry clipmaps
- horizon culling
- stable planet-space procedural sampling
- skirt/crack prevention between LODs
- collision LOD separate from render LOD
- noise library with C++ and shader equivalents

References:

- FastNoiseLite, MIT: https://github.com/Auburn/FastNoiseLite
- CDLOD, MIT: https://github.com/fstrugar/CDLOD
- Cuberact planet chunked LOD, MIT/Godot reference: https://www.cuberact.org/projects/planet-chunked-lod/
- Geometry Clipmaps paper/project: https://hhoppe.com/proj/geomclipmap/
- Real-time rendering of procedural planets at arbitrary altitudes: https://www.markussteinberger.net/papers/FromGroundToSpace.pdf

### Astronomy/System Tooling

These are more useful for offline authoring, validation, or future accurate
catalog work than for runtime gameplay:

- SPICE Toolkit: https://naif.jpl.nasa.gov/naif/toolkit.html
- Orekit, Apache-2.0: https://www.orekit.org/
- poliastro, MIT: https://docs.poliastro.space/
- Astropy, BSD-3-Clause: https://docs.astropy.org/en/stable/license.html
- OpenSpace, MIT: https://github.com/OpenSpace/OpenSpace
- Gaia Sky licenses: https://gaiasky.space/licenses/

## Risks For Stargame

- LWC reduces precision problems but does not remove the need for a local bubble.
- Stock World Partition is planar/open-world tooling, not a spherical planet
  renderer.
- Stock Sky Atmosphere is valuable but may not support all multi-planet needs.
- Volumetric clouds can become the performance bottleneck quickly.
- Commercial planet plugins need proof-of-fit before adoption, especially with
  UE 5.7, source availability, collision, LOD seams, and long-term support.
- Terrain/plugin research should not become a production dependency until the
  playable frontier loop has a concrete visual need that simpler planet shells
  cannot satisfy.
- Procedural terrain visual LOD and collision LOD must be decoupled.
- Gas giants have no obvious complete third-party answer; assume custom shader
  and local VFX work.
- Travel feel should be tuned through distance compression plus supercruise
  curves, not by letting local physics run at extreme velocities.

## Recommended Research Follow-Up

Before implementation, do a short proof-of-fit spike for each serious external
candidate:

1. Prototype a far sphere plus near high-segment sphere transition without true
   terrain.
2. Test built-in Sky Atmosphere on a half-KSP-scale rocky body.
3. Test Volumetric Clouds as an upper-atmosphere/gas-giant layer with frame-time
   capture.
4. Install/evaluate WorldScape or Spherescape in a throwaway UE 5.7 project only
   if licensing permits and the simpler representation fails a specific
   player-facing acceptance test.
5. Evaluate whether RealtimeMeshComponent is compatible with a future custom
   cube-sphere LOD path.
6. Keep Cesium as an origin-shift/georeference reference, not the first fictional
   planet implementation.
