# Modular Station Design System

Working target: a blocky, heavy, military-industrial human station kit for game use. The direction is Halo/UNSC-adjacent in mood, but not a copy: slab armor, visible mass, hard-surface paneling, drydock logic, and practical docking geometry.

## Core Goals

- Build station pieces as reusable modules, not one-off sculptures.
- Make modules snap together on fixed connector faces.
- Keep important gameplay spaces real: docking bays must be actual voids, not painted black faces.
- Separate animated parts from static hull parts.
- Keep detail intentional. Pipes, antenna packs, and micro-greebles can come later.
- Prioritize strong silhouette, depth, modular readability, and export-friendly object names.

## Grid And Scale

- Base grid increment: `2m`.
- Main station modules should use even-meter dimensions.
- Default orientation:
  - `X`: depth, front/back.
  - `Y`: width, left/right.
  - `Z`: height, up/down.
- Module origins should be centered unless a specific connector module needs a different origin.
- Decorative armor may overhang the grid, but snap faces must remain flat and unobstructed.

## Standard Connector Sizes

Use fixed connector sizes so modules can be mixed without custom fitting.

| Connector Type | Clear Size | Use |
| --- | ---: | --- |
| Small service connector | `4m W x 3m H` | corridors, maintenance, small utility blocks |
| Medium module connector | `8m W x 6m H` | main station modules, docking modules, hangar backs |
| Large structural connector | `12m W x 8m H` | big habitation, shipyard, industrial spine modules |

Every connector should include a named helper plane or transparent object:

- `SNAP_FRONT_8x6`
- `SNAP_REAR_8x6`
- `SNAP_LEFT_4x3`
- `SNAP_RIGHT_4x3`
- `SNAP_TOP_12x8`
- `SNAP_BOTTOM_12x8`

Snap helpers should be easy to hide or delete before export.

## Docking Bay Rules

- Small craft maximum clear bay opening: `8m W x 6m H`.
- Docking bay must be an actual open volume ships can fly into.
- Recommended small craft clearance volume: `8m W x 6m H x 10m D`.
- Door open state must not block the clearance volume.
- Door meshes must be separate from static hull meshes.
- Add transparent helper volume:
  - `DOCKING_CLEARANCE_SMALL_CRAFT_8x6`
- Add obvious approach/readability elements:
  - guide rails,
  - clamp points,
  - status lights,
  - inner pressure frame,
  - heavy exterior armor brow.

## Door And Animation Rules

Door objects should be named by animation role:

- `ANIM_DOOR_left_slide`
- `ANIM_DOOR_right_slide`
- `ANIM_DOOR_top_slide`
- `ANIM_DOOR_bottom_slide`
- `ANIM_DOOR_leaf_01`
- `ANIM_CLAMP_port`
- `ANIM_CLAMP_starboard`

Animation helpers:

- `ANIM_TARGET_left_door_open`
- `ANIM_TARGET_right_door_open`
- `ANIM_TARGET_top_door_open`
- `ANIM_TARGET_bottom_door_open`

Origin rules:

- Sliding doors: origin at door mesh center unless Godot setup needs a track-specific origin.
- Hinged clamps: origin at hinge line.
- Rotating locks: origin at rotation pin.

Good first door designs:

- Two vertical sliding leaves moving left/right.
- Four-part blast door moving left/right/top/bottom.
- Segmented armored shutter panels that slide into side/top housings.

Avoid for now:

- Complex mechanical linkages.
- Dozens of tiny door pieces.
- Decorative pistons that do not explain the motion.

## Object Naming

Use prefixes so Godot import and Blender cleanup are manageable.

- `STATIC_*`: normal non-animated visible mesh.
- `ANIM_*`: animated visible mesh.
- `COLLISION_*`: collision mesh or proxy.
- `SNAP_*`: connector helper.
- `HELPER_*`: non-export reference object.
- `LIGHT_*`: visual light object or emissive marker.
- `MAT_*`: material naming prefix if needed later.

Examples:

- `STATIC_hangar_brow_armor`
- `STATIC_inner_pressure_frame`
- `ANIM_DOOR_left_slide`
- `ANIM_CLAMP_starboard_lower`
- `SNAP_REAR_8x6`
- `COLLISION_module_shell_simple`
- `HELPER_small_craft_clearance`

## Reusable Kit Parts

Build modules from a small set of repeatable pieces rather than random blocks.

Initial kit:

- Armor slab: large flat rectangular plates with bevels.
- Wedge armor cap: chamfered/stepped pieces for silhouette.
- Connector frame: standardized `4x3`, `8x6`, `12x8`.
- Door leaf: heavy blast-door slab with ribs.
- Door track housing: side/top recess where doors slide away.
- Docking clamp block: chunky latch geometry around the bay.
- Guide rail: simple forward protruding alignment rail.
- Light strip: small emissive strip or block.
- Recess panel: dark inset service area.
- Structural rib: repeated support frame.

Reusable parts should be visually consistent but not identical at every scale.

## Visual Direction

Do:

- Use slab armor, stepped massing, recessed dark mechanical areas.
- Make the silhouette heavy and asymmetrical enough to avoid toy-block repetition.
- Keep flat armored faces large enough to read from distance.
- Use bevels and weighted normals on hard-surface edges.
- Use subdued military colors: dark gunmetal, cool gray, black recesses, muted hazard yellow.
- Use small emissive blue/green/red lights for docking state and orientation.

Do not:

- Use NASA-style round pressure modules as the primary form.
- Use Perihelion-style cylindrical station bodies.
- Use large spin rings.
- Cover the asset in pipes before the main form works.
- Add surface noise to compensate for weak structure.

## Collision And Export

Each module should include optional simple collision proxies:

- `COLLISION_module_shell_simple`
- `COLLISION_docking_bay_bounds`
- `COLLISION_door_left`
- `COLLISION_door_right`

Collision should be low-poly and separate from visual meshes.

Snap and helper objects should be clearly named so they can be hidden, filtered, or removed during export.

## Next Asset Target: Docking Module A

Purpose: first serious modular test asset.

Overall module:

- Name: `Docking_Module_A`
- Approximate size: `16m W x 10m H x 12m D`
- Main front bay: `8m W x 6m H`
- Rear connector: `8m W x 6m H`
- Optional side service connector: `4m W x 3m H`
- Real hollow fly-in docking volume.

Required visible parts:

- Heavy armored front frame.
- Deep inner tunnel, not a flat black face.
- Four-part animated blast door or two heavy sliding side doors.
- Door housings that visibly explain where doors go when open.
- Docking guide rails.
- Four to eight clamp blocks around the bay mouth.
- Inner pressure frame.
- Emissive approach/status lights.
- Large armor panels with stepped depth.
- A few dark service recesses.

Required helper parts:

- `SNAP_REAR_8x6`
- `SNAP_SIDE_4x3` if side connector is included.
- `DOCKING_CLEARANCE_SMALL_CRAFT_8x6`
- `COLLISION_module_shell_simple`
- `COLLISION_docking_bay_bounds`
- `ANIM_TARGET_*` empties for door open states.

Success criteria:

- Reads as a heavy military-industrial docking module from silhouette alone.
- Has a real fly-in void capped at `8m W x 6m H`.
- Can connect cleanly to another station module via the rear connector.
- Animated pieces are separate and obvious.
- Looks designed, not like decorated cubes.

