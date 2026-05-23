# Modular Station Kit

Grid-based modular construction system. Pieces snap on a 4m grid. Walls are swappable slots. Themes swap materials across the whole station via named slots. Same kit builds active stations, derelicts, and pirate bases.

This follows the Bethesda/Arkane/Dead Space pattern: grid-aligned pieces, snap-together geometry, material/texture swaps for visual variety. The problem is solved — this doc defines *our* specific piece set, naming conventions, and assembly rules.

---

## Grid

| Property | Value |
|----------|-------|
| Grid cell | 4m × 4m |
| Ceiling height (interior) | 3.0m |
| Wall thickness | 0.5m |
| Floor / ceiling slab | 0.2m |
| Door opening | 2.0m wide × 2.5m tall |
| Window opening | 1.5m wide × 1.0m tall, sill at 1.2m |

**Coordinate system (Blender):** X east, Y north, Z up. Origin at grid cell center, floor surface.
**In Godot:** +Y up, -Z forward. `world_x = grid_x * 4`, `world_z = grid_z * 4`, `world_y = 0`.

Pieces snap at 4m intervals. Rotation is in 90° increments. The wall-slot system handles all configurations — no special connector pieces needed.

---

## Pieces

Each piece is a GLB with a named hierarchy. The assembler finds children by exact name (case-sensitive).

| Piece | Grid | Interior | Wall slots per side |
|-------|------|----------|---------------------|
| `Corridor_1x1` | 1×1 | 3×4m | 1 (`Wall_North`, `_South`, `_East`, `_West`) |
| `Corridor_1x2` | 1×2 | 3×8m | N/S: 1 each, E/W: 2 each (`Wall_East_0`, `_1`) |
| `Room_2x2` | 2×2 | 7×7m | 2 per side (`Wall_North_0`, `_1`, etc.) |
| `Room_2x3` | 2×3 | 7×11m | N/S: 2 each, E/W: 3 each |
| `Room_3x3` | 3×3 | 11×11m | 3 per side |
| `Stairwell_1x2` | 1×2 (two levels) | vertical | 1 per side, ramp geometry, upper exit on `North` |
| `Airlock_1x1` | 1×1 | 3×4m | 1 per side, inner/outer door slots. One airlock per layout is tagged as the **entry airlock** — where the player spawns after docking (04). The `outer` door faces the ship bay (conceptually; not rendered inside the interior), the `inner` door leads into the station. |

### Standard child structure

```
[PieceName] (Empty — root, at grid cell center)
├── Floor                      (MeshInstance3D)
├── Ceiling                    (MeshInstance3D)
├── Wall_[Direction][_Index]   (MeshInstance3D) — SWAPPABLE
├── Structure_Corner_[NE/NW/SE/SW]  (MeshInstance3D) — PERMANENT corner posts
├── Structure_Pillar_[N/S/E/W]      (MeshInstance3D) — PERMANENT mid-wall pillars (multi-cell only)
├── Marker_Waypoint_##         (Node3D) — NPC patrol/wander
├── Marker_CoverPoint_##       (Node3D) — combat cover
├── Marker_Interaction_##      (Node3D) — NPC trader/terminal position
├── Marker_Spawn_##            (Node3D) — hostile NPC spawn
├── Marker_Prop_Wall_##        (Node3D) — wall-mounted detail (pipes, panels, signs)
├── Marker_Prop_Ceiling_##     (Node3D) — ceiling-mounted detail
├── Marker_Prop_Floor_##       (Node3D) — freestanding prop slot
└── Light_##                   (OmniLight3D)
```

**Corner posts are permanent** — they anchor the grid cell so structure remains when any adjacent wall is removed. Mid-wall pillars subdivide multi-cell edges and separate wall slots.

**Direction naming:** `North` = +Y in Blender. Multi-cell pieces index wall slots along the edge starting from the west/north end.

---

## Wall Slot System

All swappable walls share the same slot dimensions so variants drop in without repositioning:

| Property | Value |
|----------|-------|
| Width | 3.0m (grid cell minus two wall thicknesses) |
| Height | 3.0m (floor to ceiling) |
| Thickness | 0.5m |

### Variants

Each variant is a separate GLB with a matching root object name.

| File | Description |
|------|-------------|
| `Wall_Solid.glb` | Default — fills the whole slot |
| `Wall_Door.glb` | 2.0m × 2.5m opening, centered. Doorframe: two pillars + header |
| `Wall_Window.glb` | 1.5m × 1.0m opening at 1.2m sill height. Four-box frame |
| `Wall_WideDoor.glb` | 5.5m × 2.5m opening, spans **two adjacent wall slots + the pillar between them**. Used for cargo/hangar entries on rooms only |

---

## Wall Slot Resolution

When pieces are placed, adjacent walls between two occupied cells must be removed and replaced with a single door (or window, per JSON override).

### Algorithm

```
For each piece:
  For each Wall_[Direction][_Index] child:
    Compute the grid cell on the other side of this wall
    If that cell is occupied by another piece:
      Hide this wall (neighbor hides its facing wall too)
      The piece with the lower grid coordinate owns the bulkhead placement
      Instance the JSON-specified wall variant (default: Wall_Door) at the boundary
    Else if JSON override specifies a variant:
      Replace this wall with the variant
    Else:
      Keep the solid wall
```

### Wall Index Remapping Under Rotation

**This is the trap.** When a piece is rotated, the wall direction remaps (North → East for 90° CW) but the wall slot *index* also needs to remap because the spatial position of the slot changes meaning.

For a piece with original (pre-rotation) dimensions `W × H`:

**90° CW rotation** (N→E, E→S, S→W, W→N):
| Original | Becomes | Index transform |
|----------|---------|-----------------|
| North (W slots) | East (W slots) | `i` unchanged |
| East (H slots) | South (H slots) | `H-1-i` |
| South (W slots) | West (W slots) | `W-1-i` |
| West (H slots) | North (H slots) | `i` unchanged |

**180°:**
| Original | Becomes | Index transform |
|----------|---------|-----------------|
| North | South | `W-1-i` |
| East | West | `H-1-i` |
| South | North | `W-1-i` |
| West | East | `H-1-i` |

**270° CW** (N→W, E→N, S→E, W→S):
| Original | Becomes | Index transform |
|----------|---------|-----------------|
| North | West | `W-1-i` |
| East | North | `i` unchanged |
| South | East | `i` unchanged |
| West | South | `H-1-i` |

The rule: **the index reverses when the rotation maps a direction to one whose traversal order runs the opposite way along the same physical axis.**

### Alternative: Position-Based Resolution

Instead of remapping indices, compute the wall's world-space position directly from the rotated mesh child's `GlobalPosition`, then determine which grid cell it faces:

```csharp
wallWorldPos = piece.GlobalTransform * wallLocalPos
facingCell = WorldPosToGridCell(wallWorldPos + wallNormal * (gridCell / 2))
```

More robust — avoids the remapping tables entirely. This is how Wave Function Collapse implementations handle rotation.

### Bulkhead Local-Space Positioning

Bulkhead (door/window variant) position at the boundary must be set in the piece's local space. Use Godot's `ToLocal()`:

```csharp
Vector3 boundaryWorld = new Vector3(boundaryWorldX, 0, boundaryWorldZ);
bulkhead.Position = piece.ToLocal(boundaryWorld);
```

Requires the piece to be in the scene tree with its transform set. If called before `AddChild(piece)`, manually inverse-rotate the offset by `-piece.Rotation.Y`.

---

## Surface Detail (Four Layers)

Layers build on each other. Each is independent — playable at any point.

| Layer | Content | Effort | When to build |
|-------|---------|--------|---------------|
| 1 · Base geometry | Flat wall/floor/ceiling boxes | Done | First |
| 2 · Trim sheets | UV-mapped texture atlas per theme | 1 texture per theme | After layout feels right |
| 3 · Detail meshes | Pipes, vents, panels, lights bolted to walls | ~15 small GLBs | Priority — huge visual win |
| 4 · Decals | Rust, signs, graffiti, wear | Texture library + placement | Final polish |

### Detail Mesh Library

Detail meshes are separate GLBs instanced at `Marker_Prop_Wall` / `Marker_Prop_Ceiling` positions by the assembler, based on the theme's prop table.

| Piece | Dimensions | Attaches To | Slot | Description |
|-------|-----------|-------------|------|-------------|
| `Detail_Pipe_H` | 3.0m × Ø0.08m | Wall, near ceiling | `mat_structure` | Horizontal pipe run |
| `Detail_Pipe_V` | 3.0m × Ø0.08m | Wall corner | `mat_structure` | Vertical pipe |
| `Detail_Pipe_Elbow` | 0.15m cube | Pipe turns | `mat_structure` | 90° joint |
| `Detail_Pipe_T` | 0.15m cube | Pipe branches | `mat_structure` | T-junction |
| `Detail_Vent_Wall` | 0.6 × 0.4 × 0.05m | Wall mid-height | `mat_structure` | Rectangular grille |
| `Detail_Vent_Ceiling` | 0.6 × 0.6 × 0.05m | Ceiling | `mat_structure` | Square vent |
| `Detail_Panel_Electric` | 0.4 × 0.6 × 0.05m | Wall | `mat_trim` | Junction box |
| `Detail_Panel_Control` | 0.5 × 0.4 × 0.08m | Wall near doors | `mat_trim` | Control panel |
| `Detail_Panel_Fusebox` | 0.3 × 0.4 × 0.1m | Wall | `mat_trim` | Small fuse box |
| `Detail_Light_Strip` | 1.5 × 0.15 × 0.05m | Ceiling | `mat_trim` | Fluorescent-style fixture |
| `Detail_Light_Wall` | 0.2 × 0.3 × 0.1m | Wall near doors | `mat_trim` | Wall lamp |
| `Detail_Cable_Bundle` | 2.0m × Ø0.05m | Wall corners | `mat_structure` | Conduit run |
| `Detail_Sign_Direction` | 0.6 × 0.2 × 0.02m | Wall above doors | `mat_trim` | Room label |
| `Detail_Bracket_Shelf` | 0.4 × 0.3 × 0.02m | Wall | `mat_structure` | Shelf bracket |
| `Detail_Fire_Extinguisher` | 0.15 × 0.4 × 0.1m | Wall near doors | `mat_structure` | Extinguisher |

Detail density and type come from the theme (see [themes.md](themes.md)).

---

## Marker Placement Guidelines

| Marker | Count per piece | Placement |
|--------|-----------------|-----------|
| `Marker_Waypoint_##` | 3-5 corridor, 6-8 room | Natural walking positions, floor level |
| `Marker_CoverPoint_##` | 2-3 corridor, 4-6 room | Behind where furniture/pillars would be |
| `Marker_Interaction_##` | 1-2 room | Facing into room with space for player approach |
| `Marker_Spawn_##` | Same as cover/waypoints | Used only in hostile layouts |
| `Marker_Prop_Wall_##` | 4-8 corridor, 8-12 room | Along wall faces, varied heights; marker faces outward |
| `Marker_Prop_Ceiling_##` | 2-4 corridor, 4-6 room | Ceiling surface |
| `Marker_Prop_Floor_##` | 0-2 corridor, 4-8 room | Off walking paths |

Cover points may carry metadata (`peek_direction`, `stance: crouch/stand`).

---

## Material Slot Convention

All meshes in all pieces use consistent material slot names. Themes override by slot name.

| Slot | Applies to |
|------|-----------|
| `mat_wall` | Wall interior faces |
| `mat_wall_exterior` | Wall exterior (visible if adjacent piece is missing) |
| `mat_floor` | Floor surface |
| `mat_ceiling` | Ceiling surface |
| `mat_trim` | Door frames, window frames, pillar faces |
| `mat_structure` | Corner posts, mid-wall pillars, pipes |

Theme material details live in [themes.md](themes.md).

---

## Station Layout JSON

```json
{
  "name": "Earth Hub",
  "theme": "military",
  "hostile": false,
  "grid": [
    { "piece": "airlock_1x1",     "pos": [0, 0], "rotation": 0 },
    { "piece": "corridor_1x2",    "pos": [0, 1], "rotation": 0 },
    { "piece": "room_2x2",        "pos": [1, 3], "rotation": 0,
      "purpose": "bar",
      "walls": { "west_0": "door", "north_1": "window" },
      "props": [
        { "type": "bar_counter", "offset": [0, 0, -2], "rotation": 0 }
      ]
    }
  ]
}
```

**Wall override keys** use local direction names (`north_0`, `east_1`) — the resolver applies rotation remapping before lookup.

---

## Assembly Algorithm

```
1. PARSE layout JSON

2. BUILD OCCUPANCY MAP
   For each piece: record which grid cells it occupies (accounting for rotation)
   occupied_cells: Set<(x, z, level)>
   cell_to_piece:  Dict<(x, z, level), PieceInfo>

3. FOR EACH PIECE:
   a. Instance the piece GLB at (pos.x * 4, level * floor_height, pos.z * 4)
   b. Apply rotation
   c. Resolve wall slots (see above)
   d. Apply theme: override material slots by name, set light color/energy
   e. Place detail meshes at Marker_Prop_Wall / _Ceiling positions from theme pool
   f. Place freestanding props at Marker_Prop_Floor (multi-pass — see props.md)
   g. Spawn NPCs at Marker_Interaction / Marker_Waypoint / Marker_Spawn
   h. Generate collision from visible MeshInstance3D children

4. After all pieces placed:
   a. Bake NavigationMesh (see below)
   b. Spawn player at first piece (or explicit player_spawn marker)
```

---

## Navigation Mesh Baking

Single runtime `NavigationRegion3D` per level. All piece nodes are children of (or siblings under) the region. Bake once all geometry + collision is finalized.

**Indoor-tuned parameters:**

| Setting | Value | Reason |
|---------|-------|--------|
| `CellSize` | 0.15 | Fine enough for doorways, not too slow to bake |
| `CellHeight` | 0.1 | |
| `AgentRadius` | 0.3 | Match character capsule |
| `AgentHeight` | 1.7 | |
| `AgentMaxClimb` | 0.35 | Step over small lips |
| `AgentMaxSlope` | 50.0 | Stairwell ramps |
| `ParsedGeometryType` | `StaticColliders` | **Critical:** parse collision shapes, not visual meshes (visual-mesh parsing is a significant runtime performance hazard — Godot issue #90607) |
| `SourceGeometryMode` | `RootNodeChildren` | Parse all children of the region |

For multi-level stations (stairwells): one `NavigationRegion3D` per level. Stairwell geometry is included in both levels' regions. The default `edge_connection_margin` merges the nav meshes where edges align at stairwell openings.

---

## Blender Build Conventions

- Grid snap: 4m, metric units (1 BU = 1m)
- Export: GLB binary, include meshes + materials (names only) + empties/markers, exclude cameras/lights (added by assembler)
- Apply modifiers on export, preserve hierarchy
- Materials in Blender have names matching the slot convention (`mat_wall`, `mat_floor`, etc.) — Godot overrides by slot name
- Do NOT include collision in GLBs — collision is generated at runtime from visible meshes

### Exact object names (case-sensitive, assembler does lookup by name)

See the standard child structure above. The master Blender file lives at `game/assets/station_kit/source/station_kit.blend` with one collection per piece; batch export via the Hextant Batch Exporter addon.

---

## File Structure

```
game/assets/station_kit/
├── pieces/
│   ├── Corridor_1x1.glb / _1x2.glb
│   ├── Room_2x2.glb / _2x3.glb / _3x3.glb
│   ├── Stairwell_1x2.glb
│   └── Airlock_1x1.glb
├── walls/
│   ├── Wall_Solid.glb
│   ├── Wall_Door.glb
│   ├── Wall_Window.glb
│   └── Wall_WideDoor.glb
├── detail/          — the 15 Detail_*.glb files
├── props/           — freestanding furniture/objects (see props.md)
├── themes/          — one JSON per theme
├── materials/       — {theme}/mat_*.tres
└── source/station_kit.blend

game/data/station_layouts/
└── {station_name}.json
```
