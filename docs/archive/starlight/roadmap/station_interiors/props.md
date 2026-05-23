# Props & Items

Freestanding and attached objects inside station rooms: barrels, crates, consoles, furniture, terminals, debris. Placed via JSON for authored layouts and generated procedurally for scatter. All props share a catalog, a placement system, an interaction protocol, and a GLB loading pipeline.

**Scope:** props are *objects that live inside a piece*. The piece provides the shell + markers (see [modular_kit.md](modular_kit.md)); props are everything the shell wraps.

---

## PropCatalog

Central registry of all prop types. Each entry defines collision, placement constraints, and visual bounds.

```csharp
public class PropInfo {
    public string Id;              // "crate_small", "barrel", "console_terminal"
    public string GlbPath;         // "res://game/assets/station_kit/props/Prop_CrateSmall.glb"
    public Vector3 Size;           // bounding box for overlap checks
    public CollisionTier Collision; // Blocking | Interactable | Clutter
    public SnapType Snap;          // Floor | Wall | Ceiling
    public PropCategory Category;  // Cargo | Furniture | Tech | Decoration | Clutter
    public string[] Purposes;      // ["cargo_bay", "engineering"] — which room purposes want this
    public float ScatterWeight;    // probability weight in scatter passes
    public Vector3 WallMountHeight; // for wall props: Y offset from floor
}
```

### Collision tiers

| Tier | Body type | Behavior |
|------|-----------|----------|
| `Blocking` | `StaticBody3D` | Physical obstacle — player and NPCs collide with it |
| `Interactable` | `Area3D` | No physical collision, player can look + press F |
| `Clutter` | None | Purely visual, no collision at all |

All collision uses **primitive shapes** (box/cylinder). Never trimesh on props — it's expensive and unnecessary for small objects.

### Snap types

| Snap | Placed at | Orientation |
|------|-----------|-------------|
| `Floor` | `Marker_Prop_Floor_##` | Upright, rotation from placement data |
| `Wall` | `Marker_Prop_Wall_##` | Flush against wall; Y rotation from wall direction |
| `Ceiling` | `Marker_Prop_Ceiling_##` | Flipped, hanging from ceiling surface |

### Categories

`Cargo` (crates, barrels, containers), `Furniture` (tables, chairs, bunks, shelves), `Tech` (terminals, panels, servers, screens), `Decoration` (signs, posters, plants), `Clutter` (mugs, tools, loose debris).

---

## PropPlacement (per-instance data)

```csharp
public class PropPlacement {
    public string Type;          // matches PropCatalog.Id
    public string Marker;        // optional: "Marker_Prop_Wall_02" — snap to this marker
    public float[] Offset;       // optional: [x, y, z] — offset-based placement (piece-local)
    public float Rotation;       // Y degrees
    public bool Interactable;    // overrides catalog default
    public string InteractLabel; // "Access Terminal", "Open Crate"
}
```

Either `Marker` **or** `Offset` is provided — not both. Offset is piece-local; the piece rotation is applied automatically.

---

## Hand Placement (JSON)

In a station layout, a piece can carry an authored prop list:

```json
{
  "piece": "room_2x2",
  "pos": [1, 3],
  "purpose": "bar",
  "no_scatter": false,
  "props": [
    { "type": "bar_counter",    "offset": [0, 0, -2], "rotation": 0 },
    { "type": "bar_stool",      "offset": [-1, 0, -1], "rotation": 0 },
    { "type": "console_terminal", "marker": "Marker_Prop_Wall_03", "interactable": true, "interact_label": "Order a Drink" }
  ]
}
```

Hand-placed props always spawn. Scatter fills in around them.

---

## Multi-Pass Scatter

Automatic prop population per room. Runs after hand-placed props, before play. Deterministic — seeded by station name + piece position.

### Pass 0 — Exclusion zones

Before placing anything, build AABB rectangles around:
- **Doorways** (hidden `Wall_*` children): center ± `door_width/2`, extending 1.5m into the room
- **NPC spawn markers** (`Marker_Spawn_##`, `Marker_Interaction_##`): small radius
- **Hand-placed props**: their bounding box + 0.3m margin

Scatter passes reject positions inside any exclusion zone.

### Pass 1 — Furniture groups

Composite templates placed as a unit. Each template defines a center + member offsets.

| Template | Members | Used in |
|----------|---------|---------|
| `bar_table` | table_round + 2-3 chairs | Bars, lounges |
| `workstation` | console_terminal (wall) + chair | Offices, engineering |
| `cargo_stack` | crate_large + crate_small + barrel | Cargo bays, storage |
| `eng_storage` | shelf_rack + 2 barrels | Engineering |
| `med_bed` | examination_bed + monitor_stand + supply_cart | Med bay |

Place 0-2 groups per room, scaled by area. Group picks a random center in the room, checks bounds + exclusion + overlap, retries up to 5 times, skips if no valid position.

### Pass 2 — Wall props

Iterate visible (non-hidden) `Wall_*` children:
- Roll density check (wall prop density = floor density × 0.6)
- Pick a weighted random wall-snap prop from the theme's pool for the room purpose
- Position at the wall's center, mount at `WallMountHeight` (1.4m for tech, 1.0m for extinguishers, near-ceiling for vents)
- Orient to face inward — Y rotation from wall direction

### Pass 3 — Floor fill

Scatter individual floor-snap props in remaining space:
- Random position within piece bounds, on the grid or sub-grid (0.25m)
- Reject if inside an exclusion zone or overlaps another prop
- Pick a weighted random prop from the theme pool for the room purpose
- Random Y rotation (0, 45, 90, 135, ...°)

Continue until target fill rate is reached (40-90% by theme density) or max attempts exceeded.

### Pass 4 — Ceiling (optional)

Ceiling-snap props at `Marker_Prop_Ceiling_##` positions. Light fixtures, vents, cable trays. Sparse — usually 1-3 per room.

### no_scatter flag

A piece with `"no_scatter": true` skips all four passes. Only hand-placed props appear. Use for hero rooms where every object is deliberate.

---

## Interaction Protocol

```csharp
public interface IInteractable {
    string InteractLabel { get; }
    bool CanInteract(FPSController player);
    void OnInteract(FPSController player);
}
```

Any node implementing `IInteractable` in the hit path is interactable.

### Runtime wrapper

`InteractableArea3D` — an `Area3D` that implements `IInteractable` for runtime-generated prop instances. Holds a reference to its `PropInfo` and the label. Dispatches `OnInteract` by prop ID to the appropriate handler (terminal, crate loot, door control, etc.).

### Player integration

FPSController:
1. Raycast forward ~2m from camera each frame
2. Walk up the hit's parent chain looking for `IInteractable`
3. If found and `CanInteract()` returns true: fade in the prompt label (`[F] {InteractLabel}`) with 0.1s tween
4. On `Input.IsActionJustPressed("interact")`: call `OnInteract()` once (never `IsKeyPressed` — fires every frame)
5. Looking away: fade prompt out

### Dispatch by ID

`OnInteract` on the wrapper switches on prop ID:
- `console_terminal` → UI panel, "Accessing terminal..."
- `crate_small` / `crate_large` → loot roll (derelict stations)
- `door_control` → toggle connected door state
- `bar_counter` → open trade UI for the station's bar service
- unknown → log a warning, do nothing

---

## GLB Asset Pipeline

### Loading

`StationInteriorBuilder.LoadAssets()` builds a dictionary keyed by prop ID:

```csharp
Dictionary<string, PackedScene> _propScenes;

// During load:
foreach (var propInfo in PropCatalog.All) {
    if (ResourceLoader.Exists(propInfo.GlbPath))
        _propScenes[propInfo.Id] = ResourceLoader.Load<PackedScene>(propInfo.GlbPath);
    // else: placeholder primitive fallback at runtime
}
```

### Placeholder fallback

When a GLB isn't present, the builder generates a colored primitive sized to the catalog's `Size`:
- `Cargo` → brown box
- `Furniture` → beige box
- `Tech` → dark grey box with emissive strip
- `Decoration` → white cylinder
- `Clutter` → small grey box

This means: new prop IDs work immediately with placeholders, upgrade silently when the GLB appears.

### Authoring

Single Blender source (`game/assets/station_kit/source/props.blend`) with one collection per prop. Two shared materials: `mat_structure` (metal/industrial), `mat_trim` (painted/accent). Theme overrides swap these.

Triangle budget guidance:
- Clutter: 50-200 tris
- Small props: 200-500
- Medium props: 500-1500
- Large props: 1000-2000

A full station with 200-300 props sits around 150-300K triangles — trivial.

Export via Hextant Batch Exporter (one click, all GLBs).

---

## Performance

| Metric | Budget |
|--------|--------|
| Props per station | 200-300 |
| Collision shapes | All primitives |
| Unique materials | 2 (shared across all props) |
| Draw calls from props | <100 (batched by shared material) |
| Interactable Area3Ds | <30 per station |

The shared-material rule is what keeps draw calls low. Don't let per-prop materials proliferate — use per-theme swaps on the two shared slots instead.

---

## Set Pieces (future)

Composite prop groups placeable as a unit from JSON:

```json
{ "type": "set:bar_table", "offset": [2, 0, -1], "rotation": 45 }
```

The `set:` prefix triggers the furniture group template logic. All members spawn relative to the group center. Same templates used by Pass 1 scatter, now available for hand placement.
