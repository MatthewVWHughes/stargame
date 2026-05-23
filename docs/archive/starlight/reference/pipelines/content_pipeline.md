# Content Pipeline

Rules for authoring gameplay data and asset-linked content for Starlight.

This doc is about workflow and ownership boundaries:

- what belongs in JSON
- what belongs in `.tscn`
- how IDs and paths should be written
- how asset markers and data definitions stay in sync

---

## Purpose

The game is intentionally hybrid:

- Godot scenes own spatial structure, authored geometry, markers, and editor-friendly setup
- JSON owns gameplay data, mutable state, and designer-tuned numbers

If a piece of content is awkward to diff, validate, or tune in the Godot editor, it probably belongs in JSON. If it is fundamentally spatial, visual, or marker-driven, it probably belongs in a scene or imported asset.

---

## Source Of Truth

- [data_contracts.md](../data_contracts.md) defines shared runtime types and IDs
- [16_godot_architecture.md](../../roadmap/16_godot_architecture.md) defines scene vs JSON ownership at the architecture level
- [validation_requirements.md](validation_requirements.md) defines what content/tooling should eventually verify
- [content_authoring_guide.md](content_authoring_guide.md) shows how to add common content types in practice
- This doc defines authoring conventions and file-layout expectations

---

## Scene vs JSON

Use `.tscn` / imported assets for:

- geometry and hierarchy
- authored marker nodes
- lighting and environment setup
- collision setup
- station bay markers, weapon markers, engine markers, camera anchors
- anything best edited visually in Godot or Blender

Use JSON for:

- commodity definitions
- production profiles
- ship archetype stats
- equipment stats
- faction relationships
- mission templates
- dialogue templates
- sector dynamic state
- station economic state

Do not duplicate the same fact in both places unless one copy is explicitly derived from the other.

---

## IDs

All gameplay IDs are strings.

Rules:

- file name without extension is the canonical ID
- use lowercase `snake_case`
- keep IDs stable once referenced by saves, missions, or routes
- do not encode mutable state into IDs

Examples:

- `data/factions/sol_navy.json` → `sol_navy`
- `data/ships/starling.json` → `starling`
- `data/sectors/sol.json` → `sol`

---

## Path Rules

Use repo-relative paths in documentation when describing layout:

- `game/assets/ships/starling/starling.tscn`
- `data/ships/starling.json`
- `scenes/sectors/Sol.tscn`

Use `res://...` only when documenting runtime Godot loading behavior or when a serialized Godot resource path is required.

Examples:

- prose/layout doc: `game/assets/ships/starling/starling.tscn`
- runtime load example: `res://game/assets/ships/starling/starling.tscn`

Avoid mixing repo-relative and `res://` styles in the same example unless the distinction is the point of the example.

---

## Ship Authoring Contract

Ship archetype JSON owns:

- hull stats
- speed stats
- slot counts
- cargo capacity
- signature
- model path

Ship scene / imported model owns:

- hardpoint marker positions
- engine marker positions
- shield center
- audio center
- cockpit anchor
- collision hull geometry

Important rule:

- counts live in JSON
- positions live in markers

Do not store hardpoint positions in ship archetype JSON.

---

## Station Authoring Contract

Sector scene owns:

- orbital hierarchy
- positioned planets, moons, stations, wormhole stations
- static station shell placement

Sector JSON owns:

- dynamic station state
- faction influence
- zone data
- mutable economic state

Station layout JSON for Pillar B owns:

- modular interior layout
- prop placement
- room markers
- spawn markers
- smart-object references

The Pillar A V1 docked menu does not require full station-layout authoring to ship.

---

## Numeric Data Rules

Canonical rule:

- cargo quantities are `float`
- station stock is `float`
- production/consumption math is `float`
- UI may round or abbreviate for readability

Use integers only where the concept is truly discrete:

- equipment tier
- slot count
- ammo count
- level
- fixed array lengths

---

## Economy Rules

Production semantics:

- partial inputs can floor to reduced output
- zero inputs means zero output

Resilience should come from:

- partial-input floor
- baseline drift
- price clamp
- NPC rerouting

Not from pretending a station can manufacture goods from nothing.

---

## Save-Schema Implications

Any change to the following requires checking save docs and examples:

- player equipment shape
- ammo-limited item list
- cargo quantity types
- station stock types
- mission progress shape
- criminal-status shape

Before changing a shared gameplay schema, update:

1. `data_contracts.md`
2. affected roadmap docs
3. save example docs if the serialized shape changes

---

## Minimal Authoring Checklist

When adding a new ship:

1. Create ship archetype JSON
2. Create or import ship scene/model
3. Add required markers
4. Verify marker counts match slot counts
5. Verify runtime path is valid

When adding a new station:

1. Add station shell to sector scene
2. Add station entry to sector JSON
3. Add economic state defaults
4. Add station type/services metadata
5. Add interior layout only if the station will use Pillar B

When adding a new commodity:

1. Add commodity definition
2. Add legal-status rules
3. Add production/consumption references
4. Verify pricing and UI category usage

---

## Validation Expectations

Before content is considered valid, tooling should be able to verify:

- schema version exists
- IDs match filenames
- required fields exist
- referenced IDs exist
- referenced scene/resource paths exist
- ship marker counts satisfy archetype slot counts
- station IDs are globally unique

Validation can start as a lightweight offline script; it does not need to be editor-integrated on day one.
