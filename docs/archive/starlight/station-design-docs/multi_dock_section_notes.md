# Multi-Dock Section Notes

Goal: build the docking hardware first, then wrap station structure around it.

## Why Start With Dock Pieces

The docks define the most important constraints:

- ship clearance,
- door travel,
- clamp positions,
- approach spacing,
- module depth,
- station-facing connector size.

If the station body is built first, the docks tend to become decorative holes. The better workflow is to make each dock a reusable cartridge with a real void, then assemble those cartridges into a larger station section.

## Four-Dock Baseline

Use a `2 x 2` dock array as the first baseline.

Per dock cartridge:

- clear opening: `8m W x 6m H`,
- recommended cartridge footprint: `11m W x 8m H`,
- recommended cartridge depth: `10m` to `12m`,
- rear connection face: `8m W x 6m H`,
- actual fly-in void through the cartridge.

Four-dock array:

- two docks wide,
- two docks high,
- enough spacing between bays for heavy armor ribs and door housings,
- shared structural spine can be added after the dock cartridges work.

## Dock Cartridge Required Parts

- `STATIC_dock_*_outer_frame`
- `STATIC_dock_*_inner_pressure_frame`
- `STATIC_dock_*_door_track_housing`
- `ANIM_DOOR_dock_*`
- `ANIM_CLAMP_dock_*`
- `LIGHT_dock_*`
- `SNAP_DOCK_*_REAR_8x6`
- `HELPER_DOCK_*_CLEARANCE_8x6`

## Assembly Rule

The station body should connect to the rear faces of the dock cartridges, not swallow or reshape them. This keeps docking hardware reusable across station variants.

