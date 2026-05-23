# Sol Navy Battleship V2 Brief

## Goal

Design a believable human 2200s battleship for a game. It must read as utilitarian, mass-produced, and naval-industrial without becoming a Star Destroyer wedge, a tube, or a floating rectangle.

This pass prioritizes silhouette and functional architecture. Fine surface detail, final UVs, and final material work are explicitly out of scope.

## Core Premise

The ship is a **fleet citadel battleship**: a protected command/power/crew core with replaceable external mission structures. It is designed to survive damage by keeping the vital core narrow and armored while letting radiators, hardpoint pallets, magazines, and service pods be swapped or sacrificed.

## Functional Architecture

### 1. Armored Citadel Spine

The central spine contains command, crew pressure volume, combat information center, primary power routing, shield systems, and redundant life support.

Visual rules:
- Long, narrow, faceted central hull.
- Not cylindrical: use asymmetric broken planes and hard shoulders.
- Forward profile is blunt and stepped, not a spear.
- Dorsal command/sensor block is small and armored, not a bridge window.

### 2. Twin Mission Sponsons

Two large side structures carry magazine space, future weapon hardpoint sockets, small craft/service bays, and sacrificial equipment.

Visual rules:
- Sponsons do not run the full length. They should start behind the bow and end before the engine raft.
- Leave visible negative space between the citadel and sponsons.
- Connect with truss braces and armored transfer collars.
- Shapes should taper and notch, not be plain boxes.

### 3. Thermal Architecture

The ship has high power demand, so radiators must be visible design features.

Visual rules:
- Use paired aft radiator wings/cassettes mounted edge-on to the ship's likely attack aspect.
- Radiators should look replaceable and mechanically supported.
- Avoid decorative fins. Every fin should read as heat rejection hardware.

### 4. Reactor and Drive Raft

The aft third contains the main reactor shielding, power conversion, propellant systems, and clustered engines.

Visual rules:
- Aft block can be massive, but not a simple rectangular brick.
- Engine nozzles should cluster around a reinforced frame.
- Add dark service voids and brace geometry around the drive raft.

### 5. Hardpoint Strategy

No weapon models in this pass. Only future attachment geometry.

Visual rules:
- Heavy hardpoint sockets live on dorsal shoulders of the citadel and outer faces of sponsons.
- Missile/torpedo cells live in peripheral sponsons, separated from the citadel.
- Point-defense hardpoints are small sockets on bow, aft, dorsal, ventral, and sponson corners.
- All hardpoints must have named marker empties in Blender later.

## Target Silhouette

Top view should read as:

- Narrow armored central spine.
- Two broken side sponsons with gaps.
- Aft drive raft wider than the citadel but not wider than the total radiator span.
- Radiator wings/cassettes aft and outboard.
- Bow has stepped shoulders and sensor chin.

Side view should read as:

- Tall enough to feel like a capital ship, but not a tower.
- Dorsal command/sensor island near forward-midship.
- Lower keel/service spine visible below the citadel.
- Aft drive raft has strong vertical mass.

## Proportion Targets

For a 1000 m class battleship:

- Overall length: 1000 m.
- Citadel length: 860 m.
- Total beam without radiator wings: 360-440 m.
- Total beam with radiator wings: 560-680 m.
- Total height: 150-220 m.
- Sponson length: 520-650 m.
- Engine raft length: 180-230 m.

## Explicit Avoid List

- Single unbroken tube.
- Single unbroken rectangle.
- Giant triangular wedge.
- Random greebles before silhouette.
- Exposed oversized glass bridge.
- Weapon barrels/turrets in this pass.
- Radiator fins placed as decoration.

## Blender Build Plan

1. Build an orthographic guide plane from the selected top/side silhouettes.
2. Construct the model from named modules:
   - `CitadelSpine`
   - `BowArmor`
   - `PortMissionSponson`
   - `StarboardMissionSponson`
   - `AftDriveRaft`
   - `RadiatorWing_*`
   - `TransferTruss_*`
   - `HardpointSocket_*`
3. Review top, side, front, rear, and three-quarter screenshots.
4. Only after the silhouette works, export one visual mesh plus separate `CollisionHull`.
