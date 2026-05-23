# Docking Module Build Checklist

Use this before accepting a docking-module iteration.

## Scale And Modularity

- [ ] Overall dimensions use `2m` grid increments.
- [ ] Rear connector is present and named, usually `SNAP_REAR_8x6`.
- [ ] Connector face is flat and unobstructed.
- [ ] Decorative armor does not block snap zones.
- [ ] Module origin is logical and documented.

## Docking Function

- [ ] Main docking void is real open geometry.
- [ ] Clear opening is no larger than `8m W x 6m H` for small craft.
- [ ] Clearance helper exists: `DOCKING_CLEARANCE_SMALL_CRAFT_8x6`.
- [ ] Ship approach path is visually obvious.
- [ ] Docking rails or guide structures are present.
- [ ] Clamp/latch points are visible and plausible.

## Doors

- [ ] Door meshes are separate from static hull.
- [ ] Door names start with `ANIM_DOOR_`.
- [ ] Door open target helpers exist.
- [ ] Door housings/tracks explain where doors move.
- [ ] Open doors do not block the ship clearance volume.
- [ ] Door origins make sense for Godot animation.

## Visual Quality

- [ ] Strong silhouette before small details.
- [ ] Blocky military-industrial shape language.
- [ ] Large armor slabs have bevels/weighted normals.
- [ ] Recesses create real depth.
- [ ] Surface detail supports function.
- [ ] Asset does not rely on pipes, antennae, or random noise.
- [ ] Avoids round NASA pressure-vessel language.

## Game Export

- [ ] `STATIC_*`, `ANIM_*`, `SNAP_*`, `COLLISION_*`, and `HELPER_*` prefixes are used.
- [ ] Collision proxies are separate.
- [ ] Snap/helper objects can be hidden or excluded.
- [ ] Materials are simple and reusable.
- [ ] No accidental tiny loose objects.

## Current Target

- [ ] `Docking_Module_A`
- [ ] Approx size: `16m W x 10m H x 12m D`
- [ ] Main front bay: `8m W x 6m H`
- [ ] Rear connector: `8m W x 6m H`
- [ ] Optional side service connector: `4m W x 3m H`

