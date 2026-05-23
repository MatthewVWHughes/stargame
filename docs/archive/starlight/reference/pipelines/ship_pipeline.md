# Ship Asset Pipeline

Authoring conventions for player and NPC ships. Parallel to the station kit pipeline ([station_interiors/modular_kit.md](../../roadmap/station_interiors/modular_kit.md)) but scoped to spaceflight — one mesh per ship, simpler hardpoint rules, LOD for distance rendering.

---

## Authoring in Blender

| Rule | Value |
|------|-------|
| Units | Metric, 1 Blender unit = 1 meter ([space_foundation.md](../../roadmap/space_foundation.md) Units rule) |
| Forward axis | `-Z` (Godot convention) — ship nose points along -Z in Blender |
| Up axis | `+Y` |
| Export format | GLB binary, embedded textures |
| Apply modifiers | Yes, on export |
| Preserve hierarchy | Yes (markers + collision hull must survive) |

**Length by class:**

| Ship class | Length along -Z | Tris (LOD0) |
|-----------|----------------|-------------|
| Light fighter (Starling, Viper) | ~12 m | 3,000 – 5,000 |
| Heavy fighter (Corsair, Falcon) | ~20 m | 5,000 – 8,000 |
| Freighter (Hauler, Barge, Titan) | 35 – 60 m | 10,000 – 15,000 |
| Gunboat | 50 – 70 m | 10,000 – 15,000 |

These are visual targets, not hard caps. Collision/gameplay stats come from the archetype JSON, not the mesh.

---

## Required Marker Nodes

Empty `Node3D` / Blender empties with **exact names** (case-sensitive). The SpawnRenderer (16) scans for these at instance time.

| Marker | Purpose | Required count |
|--------|---------|---------------|
| `Hp_Gun_01`, `Hp_Gun_02`, … | Gun mount hardpoint — muzzle + projectile spawn | Matches `ShipArchetype.GunMountCount` |
| `Hp_Missile_01`, `Hp_Missile_02` | Missile rail hardpoint | Matches `ShipArchetype.MissileRailCount` |
| `Hp_Turret_01` (optional) | Point-defense turret mount | 0 – 2 |
| `EngineMount_L`, `EngineMount_R` | Engine exhaust + audio attach points | 2 (center-engine ships use only `_L`) |
| `ShieldCenter` | Shield bubble effect origin | 1 |
| `AudioCenter` | Non-directional ship sounds (comms, alerts) | 1 |
| `Cockpit` | First-person camera anchor — future cockpit view | 1 |

Numbers start at `01`. Place markers at the correct world-space origin inside the mesh (muzzle tip for guns, exhaust nozzle for engines, etc.). VFX and audio scenes are parented to these markers at runtime — no per-ship VFX asset, just shared effect scenes at known positions. See [11_visual_effects.md](../../roadmap/11_visual_effects.md).

---

## Mesh + Materials

- **One MeshInstance3D per ship** (the hull) for draw-call batching. Moving sub-parts (rotating turrets, landing gear) can be separate MeshInstance3Ds as children of the hull
- UV-unwrapped to a single **2048×2048 texture atlas**:
  - BaseColor (albedo) — 2K
  - Normal — 2K
  - ORM (Occlusion / Roughness / Metallic packed) — 2K
- Material slot name: `mat_hull`. Single-slot convention means faction livery overrides (below) replace one material
- Use `ORMMaterial3D` (Godot 4.x) over `StandardMaterial3D` for the ~40% texture-sampling saving

**Polygon guidance per class** is in the table above. Overshoot is fine for hero ships; undershoot is fine for capital-ship stand-ins.

### Faction liveries

Same mesh, different texture set, different atlas. `mat_hull` material override at runtime swaps the atlas.

```
game/assets/ships/corsair/
├── corsair.glb
├── corsair.tscn
├── textures/
│   ├── default/             ← baseline livery
│   │   ├── basecolor.png
│   │   ├── normal.png
│   │   └── orm.png
│   ├── sol_navy/
│   │   └── ... (same three maps, navy colors)
│   └── red_claw/
│       └── ... (pirate livery)
```

Spawning an NPC Corsair of faction `red_claw` instantiates `corsair.tscn`, then overrides `mat_hull` with the Red Claw atlas textures. No mesh duplication.

---

## Collision

Author a **convex collision hull** in Blender as a child mesh named `CollisionHull`:

- Hidden from render (disable "Show in viewport" / set visible = false)
- 20 – 80 vertices typical — just the silhouette envelope
- Exported in the same GLB

At scene instance, the `.tscn` wraps the `CollisionHull` mesh in a `CollisionShape3D` using `ConvexPolygonShape3D.CreateFromMesh(collisionHullMesh)`. No trimesh collision — ships move fast, convex is sufficient and orders of magnitude cheaper.

---

## LOD (post-prototype)

Three LODs, driven by Godot's `visibility_range_begin` / `_end`:

| LOD | Tri target | Range |
|-----|-----------|-------|
| LOD0 | 100% | 0 – 500 m |
| LOD1 | ~40% | 500 – 2,000 m |
| LOD2 | ~10% (silhouette) | 2,000 – 10,000 m |
| MultiMesh dot | single quad | beyond 10,000 m — handled by sector-scale renderer (11) |

Author LOD0 first; LOD1 and LOD2 are decimated passes. Skip until the prototype proves ship visuals are ship-shaped — premature LOD is wasted effort.

---

## The `.tscn` Wrapper

Every ship archetype has BOTH the GLB (model + markers) and a `.tscn` (wiring + script):

```
Starling.tscn (CharacterBody3D, script = ShipController.cs)
├── Starling.glb instance     ← model + markers loaded as a child
│   ├── Hp_Gun_01, Hp_Gun_02
│   ├── EngineMount_L, _R
│   ├── ShieldCenter
│   ├── AudioCenter
│   ├── Cockpit
│   └── CollisionHull (hidden)
├── CollisionShape3D          ← uses ConvexPolygonShape3D from CollisionHull
├── ChaseCamera (Camera3D, TopLevel = true)
└── (effects attached at runtime to the markers)
```

`ShipArchetype.ModelPath` in the archetype JSON points at the **`.tscn`**, not the `.glb` — because every archetype needs the ShipController + CollisionShape wiring. Raw GLB is never instanced directly.

---

## File Layout

```
game/assets/ships/
├── starling/
│   ├── starling.glb              ← model + markers + CollisionHull
│   ├── starling.tscn             ← ShipController wrapper
│   └── textures/
│       ├── default/
│       ├── sol_navy/             ← faction liveries
│       └── ...
├── corsair/
├── hauler/
├── viper/
├── barge/
├── falcon/
├── titan/
└── source/
    ├── starling.blend            ← Blender source (committed for edit history; optional)
    └── ...

data/ships/
├── starling.json                 ← ShipArchetype data (hull, cargo, slot counts, ModelPath → starling.tscn)
├── corsair.json
└── ...
```

Archetype JSON fields: [data_contracts.md](../data_contracts.md#shiparchetype).

---

## Checklist for a New Ship

1. Author mesh in Blender (1u = 1m, nose along -Z, under poly budget)
2. UV-unwrap to a single 2K atlas; export BaseColor + Normal + ORM
3. Add all required marker empties at correct positions (guns, missiles, engines, shield, audio, cockpit)
4. Author `CollisionHull` as a hidden child mesh (convex envelope)
5. Export as GLB binary with embedded textures
6. Create `{ship}.tscn` wrapping the GLB, attach `ShipController.cs`, wire `CollisionShape3D` from `CollisionHull`, add `ChaseCamera`
7. Write `data/ships/{ship}.json` — all `ShipArchetype` fields including `ModelPath` → the `.tscn`
8. (Later) Add faction liveries as additional texture sets under `textures/{faction_id}/`
9. (Later) Author LOD1 and LOD2 decimated variants once LOD0 is locked
