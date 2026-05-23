# Themes & Polish

Stations with the same geometry look and sound distinct based on theme. Material swaps, lighting profiles, ambient audio layers, and decals create the atmosphere. Same modular kit, different feel per faction and station type.

Material slot convention is defined in [modular_kit.md](modular_kit.md). This doc specifies the per-theme values.

---

## Theme → Faction Mapping

| Faction (17) | Theme | Notes |
|-------------------|-------|-------|
| Sol Navy (Government) | Military | Clean, ordered, blue-white |
| Miners' Guild (Corporation) | Industrial | Functional, yellow-grey, machinery |
| Red Claw Pirates (Criminal) | Criminal | Dark, red accents, threatening |
| Frontier Alliance (Independent) | Frontier | Worn, patched, lived-in |
| (abandoned) | Derelict | Flickering, debris, damaged |

---

## Material Swapping

Per theme, define per-slot `StandardMaterial3D` resources. 5 themes × 6 slots = 30 materials. Created once at load time and cached.

**Use `MeshInstance3D.SetSurfaceOverrideMaterial(index, material)`** — per-surface override on the instance, not the shared Mesh resource. Different stations can have different themes from the same GLB assets.

### Per-theme material table

| Slot | Military | Industrial | Frontier | Criminal | Derelict |
|------|----------|-----------|----------|----------|----------|
| `mat_wall` | Clean blue-white panels | Rust-stained plates, bolts | Worn panels, patches | Dark metal + red stripes | Corroded, broken panels |
| `mat_floor` | Polished plating | Grated metal | Scuffed plating | Dark scored metal | Stained, cracked |
| `mat_ceiling` | Clean panels + strip lights | Industrial grid | Mixed panels | Dark, low-detail | Broken tiles, exposed wiring |
| `mat_trim` | Blue-white accent | Yellow hazard | Warm amber | Red warning | Rust + dried fluid |
| `mat_structure` | Matte blue-grey | Heavy yellow-grey steel | Mixed metal + wood tone | Dark gunmetal | Corroded steel |
| `mat_wall_exterior` | Same family | | | | |

### Use ORMMaterial3D where possible

Occlusion/Roughness/Metallic packed into one texture's RGB channels. 3 texture lookups vs 5 for separate textures — ~40% reduction in texture sampling. Use `ORMMaterial3D` instead of `StandardMaterial3D` for theme materials with these three maps.

### Performance rules

- Objects with identical materials batch together — minimize unique material instances
- Pre-create and cache all theme materials at load time (not per-station)
- Shader precompilation (Godot 4.4+): reference all theme materials in the scene tree at startup to force compilation before gameplay
- **Instance shader parameters** (Godot 4.2+) — per-station variation within a theme (grime level, rust amount) without duplicating materials. Preserves draw call batching.

---

## Lighting

### Warm/cool contrast per theme

Real sci-fi environments use two-tone lighting — a primary key color and a contrasting accent. Alternate lights between primary and accent across the station.

| Theme | Primary (Key) | Accent (Fill) | Energy | Ambient | Contrast |
|-------|--------------|---------------|--------|---------|----------|
| Military | Cool blue-white (0.85, 0.9, 1.0) | Warm amber (0.9, 0.8, 0.6) | 1.5 | 0.4 | 3:1 |
| Industrial | Warm orange (0.9, 0.8, 0.6) | Cool blue (0.7, 0.8, 0.9) | 1.3 | 0.3 | 2:1 |
| Frontier | Warm yellow (0.9, 0.85, 0.7) | Neutral white (0.9, 0.9, 0.9) | 1.2 | 0.3 | 2:1 |
| Criminal | Deep red (0.8, 0.2, 0.1) | Cool purple (0.5, 0.3, 0.7) | 0.8 | 0.15 | 4:1 |
| Derelict | Sickly yellow-green (0.7, 0.6, 0.3) | Cold blue (0.4, 0.5, 0.7) | 0.5 | 0.1 | 5:1 |

Higher contrast ratios = more dramatic, unsettling environments (Criminal, Derelict). Lower ratios feel safer (Frontier).

### Flicker system

Per light, driven by a shared flicker shader or AnimationPlayer with randomized keyframes seeded by light position (consistent across loads):

| Light state | Behavior |
|-------------|----------|
| Healthy | No flicker (Military, Frontier by default) |
| Aging | Subtle slow oscillation, ±5% energy over 2-4s (Industrial) |
| Damaged | Irregular flicker with brief off moments (Criminal: some lights) |
| Dying | Aggressive flicker, long off periods, buzzing audio (Derelict: most lights) |
| Dead | Permanently off; room lit only by emergency strips and emissive surfaces (Derelict: some rooms) |

### Light budget

Shadow-casting `OmniLight3D` is expensive (~1 draw call per shadow-casting light per affected mesh). Rules:

- Max 8-12 shadow-casting lights visible at once
- Only enable shadows on the 4 nearest lights to the player; disable on distant lights
- Consider baked lightmaps (`LightmapGI`) for active (non-flickering) stations — dramatic performance improvement, requires a bake step
- Emissive surfaces (screens, LEDs, strip lights on trim) are cheap secondary light — use them liberally for atmosphere

### Emergency lighting (Derelict, Criminal)

Floor-level emissive strips that stay on when main lights fail. Red (emergency) or amber (low-power). Theme-specific.

---

## Ambient Audio

Organized into layers, each on its own audio bus for independent mixing.

### Layer 1 — Constant bed (always playing)

Non-positional loops:

| Source | Character per theme |
|--------|---------------------|
| Reactor / engine hum (40-80 Hz drone) | Military: steady, clean. Derelict: stuttering, fluctuating. Industrial: louder with harmonic overtones |
| Air circulation (broadband noise) | Subtle but noticeable by absence. Pitch varies by room size — smaller = higher resonant frequency |

### Layer 2 — Positional ambience

`AudioStreamPlayer3D` at specific markers:

| Source | Marker | Profile |
|--------|--------|---------|
| Ventilation grilles | Ceiling vent positions (`Detail_Vent_Ceiling`) | White noise + slight modulation, `unit_size=3, max_distance=10` |
| Electrical panels | Junction boxes (`Detail_Panel_*`) | Quiet electrical buzz. Intermittent crackle for Derelict |
| Pipes / plumbing | Along pipe runs (`Detail_Pipe_*`) | Occasional fluid flow, deep metallic resonance |
| Computer / terminal hum | Near active consoles | Soft electronic whir, higher pitch |

### Layer 3 — Transient events (triggered, random interval)

| Source | Interval | Notes |
|--------|----------|-------|
| Distant metallic clanks / thuds | 15-60s | Sells large-station presence beyond what the player sees |
| Pressure equalization hiss/pop | 30-120s | Airlocks, sealed doors |
| Structural settling (groans, creaks) | 45-180s | Especially Derelict |
| Electrical arcs (crack/buzz) | Event-driven | Derelict, paired with sparking wire VFX |

### Layer 4 — Interaction sounds

| Source | Driven by |
|--------|-----------|
| Footsteps | Floor material slot (metal plating, grating, glass variants) |
| Door hydraulics | [doors.md](doors.md) |
| Item pickup / interaction | [props.md](props.md) interact dispatch |
| Combat — weapon fire, impacts, ricochet | [combat_ai.md](combat_ai.md) |

### Reverb zones

`Area3D` per room with reverb bus overrides. Priority: smaller/more specific areas override larger ones.

| Room | Preset | Room size | Damping | Uniformity |
|------|--------|-----------|---------|------------|
| Corridor 1x1 | Small metallic | 0.2 | 0.6 | 0.3 |
| Corridor 1x2 | Medium metallic | 0.3 | 0.5 | 0.4 |
| Room 2x2 | Medium room | 0.4 | 0.4 | 0.5 |
| Room 2x3 | Large room | 0.5 | 0.3 | 0.6 |
| Room 3x3 | Large open | 0.6 | 0.3 | 0.7 |
| Stairwell 1x2 | Tall narrow | 0.5 | 0.5 | 0.4 |
| Airlock 1x1 | Tiny sealed | 0.15 | 0.8 | 0.2 |

### Audio bus layout

```
Master
├── Music
├── Ambient
│   ├── AmbientBed            (Layer 1)
│   ├── AmbientPositional     (Layer 2)
│   └── AmbientTransient      (Layer 3)
├── SFX
│   ├── SFXFootsteps
│   ├── SFXDoors
│   ├── SFXCombat
│   └── SFXInteraction
├── Voice
└── Reverb[Small|Medium|Large]  (receives from Area3D overrides)
```

Processing chain per bus: EQ → Compressor → Reverb → Limiter.

---

## Visual Effects

### Fog volumes

Per-theme box/ellipsoid `FogVolume` per room:

| Theme | Placement | Albedo | Density |
|-------|-----------|--------|---------|
| Military | Skip or very low | — | 0.05 |
| Industrial | Ellipsoids near ceiling, concentrated near steam vents | White/grey | 0.2-0.4 |
| Frontier | Full-room box | Warm amber | 0.1-0.2 |
| Criminal | Thin ground box | Dark red/purple | 0.1-0.3 |
| Derelict | Dense low-lying box | Yellow/brown | 0.5-1.0 |

Use 3D density textures at 64³ or lower resolution — high-frequency detail is hard to see in fog. Temporal reprojection is on by default (smooths jagged edges). Cost scales with screen coverage, not total volume count — keep volumes per-room rather than one giant world volume.

### Ambient particles

| Effect | Theme(s) | Implementation |
|--------|----------|---------------|
| Dust motes | Derelict | GPUParticles3D, 50-200 particles per large room, slow velocity + slight turbulence, small alpha quads. `visibility_aabb` bounded to room |
| Steam vents | Industrial | GPUParticles3D with collision, 100-300 particles per vent marker, directional from ceiling. Paired with hissing audio |
| Sparking wires | Derelict, Industrial | Subemitter system — primary for arc, subemitter for spark trails on collision. Emissive material for glow. Intermittent `emitting` toggle |
| Emergency light bleed | Derelict, Criminal | Emissive surfaces on trim geometry. Set `emission_energy` to 2.0-4.0. Not particles — cheap and effective |

**First-use stutter (Godot < 4.4):** fire all particle systems for one frame during scene initialization to force shader compilation before gameplay. Godot 4.4+ auto-precompiles if the system is in the tree at load.

### Decals

Clustered rendering limit (Forward+ renderer): 512 elements shared across omni lights, spot lights, decals, and reflection probes in the current camera view.

**Per-decal rules:**
- `distance_fade_begin=15.0, distance_fade_length=5.0` — decals beyond fade aren't sent to the shader
- `size.y = 0.1` for wall decals (small AABB → better culling)
- Hard-surface decals: `upper_fade = lower_fade = 0.0`
- `cull_mask` excludes dynamic objects (player, NPCs) so decals don't project onto them
- Dynamic combat decals (bullet impacts): fade and `queue_free()` after 30-60s

**Per-theme decal library:**

| Theme | Decals |
|-------|--------|
| Military | Unit designations, directional arrows, safety warnings |
| Industrial | Hazard stripes, pipe labels, heavy wear marks, oil stains |
| Frontier | Patched repair marks, hand-written signs, cargo labels |
| Criminal | Graffiti, skull marks, blacked-out signage |
| Derelict | Scorch marks, claw scratches, dried fluid stains |

**Universal grunge** (all themes except Military):
- Corner dust / cobweb decals
- Water stains on ceilings
- Rust streaks below metal fixtures
- Floor scuff/scratch decals in high-traffic areas
- Oil/fluid drips under pipes

Density scaled by theme: Military gets very few, Derelict gets many. Placement seeded by station name for consistency across loads.

**Budget:** aim for ≤30-50 decals visible per room.

---

## Environmental Storytelling — Vignette Templates

Pre-composed arrangements of decals + props that tell micro-stories. Placed by the assembler (1-3 per station, seeded by station name, in larger rooms).

| Vignette | Contents | Themes |
|----------|----------|--------|
| Barricade | Overturned furniture + bullet hole decals + blood trail | Derelict |
| Workstation | Tool props + oil stain decals + parts scattered | Industrial |
| Patrol post | Weapon rack + unit insignia decal + clean floor zone | Military |
| Contraband stash | Hidden panel + graffiti marker + scattered crates | Criminal |
| Abandoned meal | Food containers + spilled liquid decal + pushed-back chair | Frontier, Derelict |

Vignettes are the "someone was here" feeling that Dead Space and Alien: Isolation achieve.

---

## Material Wear Gradient (future)

Rather than uniform materials per theme, blend between clean and worn textures based on room type:
- Corridors and high-traffic areas: worn floor, scuffed walls
- Private rooms and offices: cleaner
- Engineering / utility: heaviest wear and grime

Implementation: `instance_shader_parameter` uniform — single `wear_level` float (0.0 pristine, 1.0 heavily worn) that blends layers. Set per-room during station build based on room type.

---

## Shader Precompilation Strategy

To avoid first-frame stutter when entering a themed station:

1. During the station loading screen, instantiate one invisible instance of every piece type with the target theme's materials applied
2. Fire all GPUParticles3D systems for one frame
3. Place one of each FogVolume type briefly
4. This forces Godot to compile all shader pipelines before the player sees anything

---

## Theme JSON

```json
{
  "name": "military",
  "faction": "sol_navy",
  "materials": {
    "wall":       "res://game/assets/station_kit/materials/military/mat_wall.tres",
    "floor":      "res://game/assets/station_kit/materials/military/mat_floor.tres",
    "ceiling":    "res://game/assets/station_kit/materials/military/mat_ceiling.tres",
    "trim":       "res://game/assets/station_kit/materials/military/mat_trim.tres",
    "structure":  "res://game/assets/station_kit/materials/military/mat_structure.tres"
  },
  "lighting": {
    "primary_color":  [0.85, 0.9, 1.0],
    "accent_color":   [0.9, 0.8, 0.6],
    "energy":         1.5,
    "ambient_energy": 0.4,
    "flicker_state":  "healthy"
  },
  "detail_density": {
    "corridor": 0.3,
    "room":     0.4
  },
  "detail_pool": {
    "wall":    ["light_strip", "panel_control", "sign_direction"],
    "ceiling": ["light_strip", "vent_ceiling"]
  },
  "decal_density": 0.2,
  "fog": { "type": "skip" }
}
```
