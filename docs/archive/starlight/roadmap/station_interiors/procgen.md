# Procedural Generation & Advanced Tooling

Going from hand-crafted layouts to generated ones, plus runtime integration with the space sim. This is the capstone of Pillar B — don't build it until hand-authored stations are common enough to extract templates from, and do not treat it as a blocker for the Pillar A vertical slice.

Three major systems:

1. **Station templates** — reusable layout fragments (medbay, cargo bay, crew quarters) that can be stamped in the editor.
2. **Procedural generation** — whole-station layouts from parameters (size, theme, purpose, seed).
3. **Runtime station loading** — docking transitions the player from space to a dynamically-loaded interior and back.

Plus editor quality-of-life: copy/paste, multi-select, undo/redo, quick test.

---

## Station Templates (Prefab Fragments)

Reusable layout fragments stampable in the editor. Same JSON format as full layouts, but with relative positions and an anchor.

### Example templates

| Template | Contents |
|----------|----------|
| `medbay` | Room 2×2 + corridor + Room 2×2, medical props, doctor NPC |
| `cargo_bay` | Room 3×3 with crate stacks, wide door entry |
| `command_deck` | Room 2×3 with console props, captain's chair, viewscreen |
| `crew_quarters` | Corridor + 3× Room 2×2, bunks + lockers |
| `bar_lounge` | Room 3×3 with tables, bar counter, bartender NPC |
| `engineering` | Room 2×3 with machinery props, pipes, engineer NPC |

### Template JSON

```json
{
  "template": "medbay",
  "size": [4, 3],
  "anchor": [0, 0],
  "grid": [
    { "piece": "room_2x2", "pos": [0, 0], "rotation": 0,
      "purpose": "medbay", "props": [...] },
    ...
  ]
}
```

### Editor integration

- Template palette in the editor dock
- Click to preview, click again to stamp at cursor grid position
- Offset all positions by cursor grid position
- Merge into existing layout (reject if cells overlap)
- A stamped template is a single undo step, not N individual piece placements (composite command)

---

## Procedural Station Generation

### Inputs

| Parameter | Options |
|-----------|---------|
| `size` | small (8-12 pieces), medium (15-25), large (30-50) |
| `theme` | military, industrial, frontier, criminal, derelict |
| `purpose` | trade_hub, military_outpost, mining_facility, pirate_den, derelict_wreck |
| `levels` | 1-3 |
| `seed` | int for deterministic generation |

### Algorithm — Concept Graph + Growth Hybrid

This is Dead Cells' approach and maps directly to our system: a hand-authored concept graph defines structure per purpose; a growth algorithm realizes it on the 4m grid.

1. **Build a concept graph** from purpose/size. Defines room counts per type, corridor counts, branching factor, required special rooms (airlock, command deck, stairwells).
2. **Place airlock at origin** as the root node.
3. **Grow corridors outward** from the airlock with weighted random walk + branching (40% continue, 30% branch left, 30% branch right — Spelunky's guaranteed-path pattern).
4. **Attach rooms at corridor endpoints and T-junctions**, sized by purpose weights (trade hubs favor large cargo rooms; military favors command decks and armories).
5. **Add stairwells at branch points** to connect vertical levels. Stairwells occupy cells on both levels — reserve upper-level cells before placing anything on `level+1`.
6. **Validate connectivity** via BFS from airlock. All pieces must be reachable. Reject and retry if not (cap retries at 10 per seed; on final failure, increment seed and log warning — never infinite loop).
7. **Assign special rooms** (boss/loot room for derelicts at maximum graph distance from airlock — Binding of Isaac pattern).
8. **Assign NPCs** based on room type and theme.
9. **Place props** based on room type (cargo rooms get crates, bars get tables, etc. — see [props.md](props.md)).
10. **Apply wall overrides** (windows on exterior walls, bulkheads at connections, wide bulkheads for large room-to-room).

Output: a standard station layout JSON indistinguishable from hand-crafted layouts.

### Why not WFC or BSP

- **WFC** excels at filling large tile grids with locally consistent patterns. Our pieces are large (4m cells), only ~7 piece types, and we need global guarantees (connectivity, room counts, pacing). Would require a constraint layer on top — more complex for no benefit at our scale. (WFC could later fill *furniture layouts within rooms* as a secondary pass — that's where it shines.)
- **BSP** produces rectangular subdivisions — great for dungeons, awkward for stations. Stations are corridors with rooms hanging off them, not subdivisions of a rectangle. BSP also struggles with multi-floor.

### Lessons kept from other games

- **Spelunky** — always guarantee a critical path (airlock → all rooms) before adding variety.
- **Binding of Isaac** — place special/important rooms at dead ends and max graph distance; prevent loops for cleaner layouts (max 2 neighbors per room).
- **Dead Cells** — concept graph per biome + hand-crafted room templates matching connection requirements.
- **Hades** — even a small pool of hand-crafted rooms + random selection creates excellent variety. The template system (above) can serve as the room pool for procgen.
- **RimWorld** — room role assignment driven by what furniture is placed, not by a label. Our purpose field remains the primary signal; prop contents validate it.

### Edge cases

- **Dead-end detection** — corridors leading nowhere get a room or are trimmed.
- **Minimum room count** — generator ensures minimums per purpose (trade hub: 1 cargo bay, 1 shop room; military: 1 armory, 1 barracks).
- **Rotation awareness** — track piece rotations, swap w/h for occupancy checks.
- **Airlock orientation** — must face outward (toward empty space), not into a wall.

---

## Runtime Station Loading

Connects the modular interior system to the space game.

### Docking flow (space → interior)

1. Player approaches station in space (flight system)
2. Within docking range, "Dock" prompt appears
3. Player initiates docking → fade to black (ColorRect tween, ~0.5s)
4. Begin async load: `ResourceLoader.LoadThreadedRequest("res://scenes/station_interior.tscn")`
5. Monitor progress with `LoadThreadedGetStatus()` each frame; extend tunnel/fade if load > 1s
6. On completion: `LoadThreadedGet()` → instance scene → `StationInteriorBuilder.BuildStation(layout_json)`
7. Remove/hide space scene (free or cache depending on memory)
8. Spawn player at airlock entry marker → fade in
9. Player explores, trades, talks to NPCs

### Undocking flow (interior → space)

1. Player interacts with ship or "Launch" in hangar
2. Fade to black
3. Station interior freed
4. Space scene restored
5. Main camera switches to chase cam
6. Position ship outside station → fade in

### Scene transition manager

Autoload singleton `SceneTransitionManager`:
- Owns the `ColorRect` overlay for fades
- Manages the async load lifecycle
- Handles space scene cache (keep in memory for instant undock, or free to save RAM)

### State persistence

`PlayerState` (owned by `SimulationManager`) survives scene changes: inventory, credits, reputation, active missions. Serialized in full on every dock (14).

Station interior state is owned by `StationStateManager` (16). On dock:
- `StationStateManager.GetInterior(stationId)` returns the cached `InteriorState` if the station was visited recently, or null
- Null → regenerate fresh shop rosters, bar rosters, ambient NPC schedules from the station layout
- Non-null → re-hydrate the interior from cache

On undock, the interior's current state is persisted via `StationStateManager.SaveInterior(stationId, state)`. The manager LRU-caches the 10 most recent non-derelict stations; the save file carries the same 10 entries.

- **Friendly / trade / military stations:** NPC/shop state persists (within the 10-station cache)
- **Derelict stations:** bypass the cache entirely — loot and enemies regenerate on each visit per (06)/09

### Edge cases

- **Loading failure** — if station JSON is missing or corrupt, show error and return to space (don't crash).
- **Player death in station** — respawn/reload flow: either reload the station or return to space at last save (14).
- **Save/load mid-station** — save system handles saves inside a station. On load, rebuild the station and restore player position.

---

## Station Data in the Space Game

Each station entity in the space sim carries:

```json
{
  "name": "Earth Hub",
  "faction": "sol_navy",
  "theme": "military",
  "purpose": "trade_hub",
  "station_layout": "res://game/data/station_layouts/earth_hub.json"
}
```

Theme determines faction allegiance and available services. Purpose determines what NPCs/services spawn inside:

| Purpose | Interior content |
|---------|-----------------|
| trade_hub | Traders (large inventories), equipment dealers, ship dealer |
| military | Mission givers, military equipment, armory |
| mining | Raw material traders, refinery equipment |
| pirate | Black market, smuggling missions |
| derelict | No NPCs, loot containers, hostile enemies |

---

## Editor Quality-of-Life

### Copy / paste / multi-select

- Ctrl+click to add to selection; shift+click range
- Ctrl+C copies selection (pieces + props + wall overrides)
- Ctrl+V pastes at cursor grid position
- Multi-select operations: move, delete, rotate (as a group)

### Undo / redo (Command pattern)

Two stacks: `_undoStack`, `_redoStack`. Each action creates a command object.

```csharp
public interface IEditorCommand {
    void Execute();
    void Undo();
}
```

Commands:
- `PlacePieceCommand` — piece type, position, level, rotation
- `DeletePieceCommand` — full piece data for restoration
- `MovePieceCommand` — old + new positions
- `RotatePieceCommand` — old + new rotation
- `SetWallCommand` — slot key, old + new wall type
- `StampTemplateCommand` — composite of PlacePieceCommands
- `PasteCommand` — composite

Ctrl+Z pops undo → `Undo()` → pushes to redo. Ctrl+Y reverses. Executing a new command clears the redo stack.

**Best practices:**
- Store deltas (what changed) not full snapshots — memory efficient
- Group related operations into transactions (template stamp = one undo step)
- Cap history at ~100 commands to prevent memory bloat

### Other QoL

- **Floor plan overlay** — toggle 2D top-down view for quick layout planning
- **Validation overlay** — highlight errors directly on pieces in 3D (overlap, disconnected, missing required rooms for purpose)
- **Quick test** — button to launch the station directly from the editor
- **Auto-save** — periodic auto-save of the current layout (separate from the user's save file)

---

## Performance Targets

| Station size | Pieces | Est. meshes | Est. draw calls | Target FPS |
|-------------|--------|-------------|-----------------|------------|
| Small | 8-12 | ~100-150 | ~50-75 | 60 |
| Medium | 15-25 | ~200-350 | ~100-175 | 60 |
| Large | 30-50 | ~400-700 | ~200-350 | 60 |
| Maximum | ~80 | ~1000 | ~500 | 30+ |

### Key optimizations

- **Occlusion culling** — ideal for indoor scenes with many small rooms. Use piece geometry as `OccluderInstance3D` sources — walls naturally occlude behind them. No `DirectionalLight3D` = best case.
- **Material reuse** — all pieces share a single material atlas (1-2 materials total). Fewer unique materials = automatic draw call batching.
- **Automatic instancing** — Godot 4 batches identical `MeshInstance3D` nodes with the same mesh + material. Corridors and walls repeat heavily; this kicks in for free.
- **Collision** — use simplified collision meshes exported from Blender (convex or hand-placed boxes), not trimesh. Pre-bake in GLB rather than generate at runtime.
- **Light budget** — max 8-12 shadow-casting lights visible at once. Disable shadows on distant lights. See [themes.md](themes.md) for light rules.
- **LOD / visibility ranges** — for large stations, set `visibility_range_end` on prop meshes; piece geometry stays visible.

Indoor scenes with 30-50 pieces are well within Godot 4's capabilities — the official occlusion culling demo runs 1,024 rooms in a 64×64 grid.
