# Sol Navy Battleship Design Direction

## Intent

The battleship is a 2200s human capital ship built for fleet presence, station defense, and long-duration patrols. It should feel advanced, but still like an engineered object that could be assembled in orbital yards from repeated sections. Avoid a heroic fantasy wedge silhouette: this is a military-industrial machine, not a ceremonial blade.

## Design Philosophy

- **Mass-producible structure:** repeated armored bays, standardized weapon terraces, rectangular radiator and sensor pallets, and modular aft engine blocks.
- **Human utilitarian lineage:** submarine, aircraft carrier, naval destroyer, container ship, and orbital drydock cues. The hull should read as pressure compartments and service trunks wrapped in armor, not as one smooth alien shell.
- **Function before elegance:** exposed service corridors, recessed maintenance panels, external truss hints, and clear replaceable armor modules are preferable to seamless curves.
- **Advanced, not magical:** no gravity-defying decorative fins, oversized glowing cores, or impractical knife profiles. Engines, radiators, sensors, and hardpoints should have obvious places to live.
- **Readable game silhouette:** broad from above, stacked from the side, and asymmetrical only in small utility details. At distance it should read as a long armored capital ship with an aft power block.

## Shape Language

- Long central keel/spine along local `-Z` forward.
- Narrower armored bow with stepped sensor and command decks, not a pointed spear.
- Broad midship combat deck with port/starboard recessed hardpoint terraces.
- Raised dorsal command and sensor island offset only slightly from centerline.
- Ventral keel blocks for magazines, shield projectors, and docking clamps.
- Aft engine raft made from multiple rectangular nozzles and heavy service housings.
- Armor panels use chamfered boxes, repeated ribs, and layered slabs. Keep bevels small and mechanical.

## Gameplay Requirements

- Ship length target remains roughly `1000 m`, matching `battleship_blockout_1000m.glb`.
- Forward axis is local `-Z`; up is `+Y`; width is local `X`.
- Include named hardpoint marker empties:
  - `Hp_Gun_01` through `Hp_Gun_08` for heavy dorsal/side mounts.
  - `Hp_Missile_01` through `Hp_Missile_04` for side rail or VLS-style launch positions.
  - `Hp_Turret_01` through `Hp_Turret_06` for point-defense mounts.
  - `EngineMount_L`, `EngineMount_R`, `ShieldCenter`, `AudioCenter`, and `Cockpit`.
- Keep mount positions visually exposed and mechanically plausible, with flat pads that can accept future turret models.
- Include a simple hidden `CollisionHull` convex envelope for later Godot wrapping.

## Current Battleship Pass

This first art pass should move beyond the scale blockout while staying editable:

- Build from separate named modules in Blender so the hull can be adjusted quickly.
- Use neutral navy gray materials with darker armor recesses and restrained blue-white emissions.
- Prioritize silhouette, hardpoint pads, bridge placement, engine massing, and panel rhythm.
- Do not spend time on final UVs, texture atlases, tiny greebles, or destruction variants yet.
