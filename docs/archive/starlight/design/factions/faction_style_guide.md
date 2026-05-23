# Faction Style Guide

First-pass visual and faction direction for placeholder ship generation.

The goal is consistency, not final art. Meshy outputs should read as the correct faction and ship role from silhouette first. Detail can be replaced later without changing gameplay scale, hardpoints, collision, or archetype stats.

## Shared Placeholder Rules

- Use low-poly or mid-poly hard-surface forms with broad planar panels.
- Prefer readable silhouette over dense surface detail.
- Use sharp bevels, hard edges, flat armor plates, and modular hull sections.
- Avoid organic curves, melted panel lines, noisy greebles, and ornate fantasy shapes.
- Keep weapon mounts, engines, cargo modules, radiators, hangar bays, and docking collars visually obvious.
- Generate clean, neutral-pose ships with no scene background, no stand, no exhaust trails, and no text markings baked into the mesh.

Negative prompt for most ships:

```text
organic alien curves, melted topology, excessive tiny greebles, asymmetrical damage, fantasy, steampunk, exposed character cockpit, smooth toy-like plastic, glowing magical effects, text labels, logos, background scene, landing gear deployed, weapons firing
```

## United Commonwealth of Earth Navy

ID: `uce_navy`

Full name: United Commonwealth of Earth Navy

Role: Primary Sol military power. Oldest, richest, most standardized navy. Controls Earth, Luna, Mars-aligned defense lanes, and most inner-system security.

Shape language:

- Long horizontal hulls, slab armor, stepped superstructure.
- Wedge or spearhead bows, armored side cheeks, recessed hangars.
- Symmetrical, disciplined, naval-industrial silhouettes.
- Heavy ships should look like fleet command assets, not agile aircraft.
- Small craft should echo the same wedge/slab language in compact form.

Materials and color:

- Naval white, graphite gray, dark blue-black panels.
- Subtle blue identification lights.
- Matte armor, dark recessed mechanical areas, restrained emissive strips.

Prompt prefix:

```text
United Commonwealth of Earth Navy hard-surface spacecraft, disciplined naval military design, angular slab armor, broad flat panels, wedge bow, symmetrical hull, graphite gray and naval white plating, dark blue accent lights, clean low-poly game-ready placeholder, sharp bevels, planar geometry
```

## TRAPPIST Federation

ID: `trappist_federation`

Full name: TRAPPIST Federation

Home territory: TRAPPIST-1 system.

Design note: TRAPPIST-1 is a gameplay/narrative pick because it is deeper than the immediate Solar neighborhood and supports a multi-world breakaway polity. The faction should have enough distance from Sol to make border control, expeditionary logistics, disputed transit corridors, and independent naval doctrine plausible.

Role: Breakaway human faction descended from outer colonists, dissident military officers, and system-scale industrial settlers. More self-reliant, defensive, and modular than Earth.

Shape language:

- Compact armored cores with external mission modules.
- Vertical radiator fins, braced trusses, exposed replaceable fuel/cargo pods.
- Chunkier, practical silhouettes built for repair away from Earth supply chains.
- Warships should feel like fortified tools: less elegant than Earth Navy, but rugged and purposeful.
- Small craft should have forward armor blocks, visible engine clusters, and modular side pods.

Materials and color:

- Burnt titanium, pale gray ceramic armor, muted red-orange trim.
- Amber or warm white lights.
- More visible mechanical structure than Earth Navy.

Prompt prefix:

```text
TRAPPIST Federation hard-surface spacecraft, rugged breakaway colony navy design, compact armored core, modular external mission pods, vertical radiator fins, reinforced truss structure, pale ceramic gray and burnt titanium armor, muted red orange accents, clean low-poly game-ready placeholder, sharp bevels, planar geometry
```

## Civilian Ships

ID: `civilian`

Role: Traders, haulers, miners, passenger craft, gas haulers, tugs, repair tenders, survey craft.

Shape language:

- Function-first ships where the cargo or job is visible.
- Cargo vessels should be spine-and-container designs, tankers should have obvious cylindrical tanks, miners should have drill arms or ore bins, passenger ships should have smoother hab modules.
- Civilian craft should look maintained but not militarized.

Materials and color:

- Industrial white, faded yellow, safety orange, bare metal, dark utility panels.
- Green, amber, and white navigation lights.
- Minimal armor, more exposed equipment.

Prompt prefix:

```text
civilian industrial spacecraft, practical non-military hard-surface design, visible functional modules, broad flat panels, exposed cargo equipment, worn industrial white and safety orange paint, clean low-poly game-ready placeholder, sharp bevels, planar geometry
```

## Pirate Factions

ID: `pirates_generic`

Role: Raiders, smugglers, scavengers, hijacked civilian hulls, improvised warships.

Shape language:

- Built from civilian hulls with welded armor, asymmetric repairs, external weapons, extra engines.
- Still mostly human-made and functional, not alien or fantasy.
- Use asymmetry carefully: enough to imply modification, not enough to break readability.
- Raiders should be fast and predatory; larger pirate ships should look like converted haulers or salvage barges.

Materials and color:

- Dark primer, bare metal, rusted panels, mismatched replacement armor.
- Red hazard lights, patched paint, stripped markings.

Prompt prefix:

```text
pirate raider spacecraft, modified civilian hard-surface hull, welded armor plates, exposed weapons, mismatched repair panels, extra engine pods, dark bare metal and patched paint, red hazard lights, clean low-poly game-ready placeholder, sharp bevels, planar geometry
```

## Role Prompt Suffixes

Use one faction prefix plus one role suffix.

## Recommended Triangle Counts

Use triangle output for Meshy placeholders. These are target counts, not hard limits. If Meshy gives a range slider, stay close to the target and avoid pushing above the high end unless the silhouette is unreadable.

| Role | Target tris | Practical range |
| --- | ---: | ---: |
| Fighter | 4,000 | 1,500-6,000 |
| Bomber | 7,000 | 3,000-10,000 |
| Support craft | 6,000 | 3,000-10,000 |
| Small trader | 10,000 | 5,000-15,000 |
| Corvette | 12,000 | 6,000-18,000 |
| Destroyer | 28,000 | 15,000-45,000 |
| Mining ship | 30,000 | 15,000-45,000 |
| Passenger liner | 32,000 | 18,000-50,000 |
| Large trader | 45,000 | 25,000-70,000 |
| Gas hauler | 50,000 | 30,000-80,000 |
| Pirate raider | 8,000 | 4,000-14,000 |
| Pirate frigate | 30,000 | 15,000-45,000 |
| Cruiser | 70,000 | 40,000-100,000 |
| Battleship | 120,000 | 80,000-160,000 |

### Battleship

```text
massive battleship capital ship, long armored command hull, layered broadside armor, multiple heavy turret mounts, recessed hangar bays, large engine block, strong horizontal silhouette, flat armor planes, 1000 meter class
```

### Cruiser

```text
heavy cruiser warship, balanced capital escort hull, forward command spine, side armor cheeks, visible missile cells and medium turret mounts, large but faster than a battleship, 600 meter class
```

### Destroyer

```text
destroyer warship, aggressive narrow hull, long sensor mast, missile batteries, point defense mounts, fast escort silhouette, sharp prow, 300 meter class
```

### Corvette

```text
corvette patrol warship, compact armored hull, oversized engines, patrol sensors, small turret mounts, agile escort silhouette, 100 meter class
```

### Fighter

```text
single-seat space fighter, compact wedge body, twin engine nacelles, small weapon hardpoints, armored cockpit canopy, sharp angular wings or stabilizer fins, 18 meter class
```

### Bomber

```text
space bomber attack craft, compact heavy strike hull, visible missile racks or torpedo bay, reinforced nose armor, twin or quad engines, less agile than fighter, 32 meter class
```

### Support Craft

```text
military support craft, compact utility hull, docking collar, sensor pods, repair arms or cargo clamps, lightly armored, visible maneuver thrusters, 35 meter class
```

### Small Trader

```text
small trader freighter, compact cargo hull, visible container bay, single main engine block, practical cockpit, lightly armored, 50 meter class
```

### Large Trader

```text
large commercial freighter, long cargo spine, stacked container modules, tug-like command module, large engine cluster, minimal weapons, 150 to 220 meter class
```

### Gas Hauler

```text
gas hauler tanker spacecraft, multiple large cylindrical pressure tanks, reinforced central spine, hazard bands, industrial piping, tug command module, 220 meter class
```

### Mining Ship

```text
mining spacecraft, rugged industrial hull, ore bins, drill arms or cutting booms, exposed machinery, heavy landing clamps, work lights, 180 meter class
```

### Passenger Liner

```text
passenger liner spacecraft, elongated habitable modules, observation windows as dark inset strips, smooth but still planar hull panels, safety markings, large but non-military silhouette, 120 meter class
```

### Pirate Raider

```text
fast pirate raider, converted small freighter hull, welded front armor, exposed guns, oversized engines, asymmetric side pods, patched metal, 25 to 70 meter class
```

### Pirate Frigate

```text
pirate frigate, converted industrial or escort hull, improvised broadside guns, welded armor belts, mismatched engine modules, salvage equipment, threatening but human-built silhouette, 180 meter class
```
