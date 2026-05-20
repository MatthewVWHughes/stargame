# Sector Star Anchor Plan

## Godot Reference

The Godot implementation uses one reusable star scene and changes its look through parameters:

- `starlight/game/scenes/game/space/Sun.tscn`
  - `SunMesh`: procedural emissive surface sphere.
  - `SunHalo`: camera-facing additive halo quad.
  - `SunFlares`: procedural prominence bundles.
  - `StarLight`: OmniLight for local scene lighting.
- `starlight/game/scripts/star/Star.cs`
  - Defines `StellarClass` values `O/B/A/F/G/K/M/L`.
  - `ApplyClass()` pushes class-specific colors and emission strengths into the surface, halo, flares, and light.
- `starlight/game/scripts/game/space/SunFlareField.cs`
  - Builds coronal-loop strand meshes procedurally and instances them around the star with per-instance phase/frequency.
- `starlight/game/scripts/game/ui/SectorMap.cs`
  - Contains the nearby-sector star list, positions, class, tier, and class accent colors.

The important idea to keep: one star actor with reusable materials and class-driven visual parameters.

## Unreal Direction

Build a stronger Unreal version as Blueprint-authored presentation:

- `BP_SectorStarAnchor`
  - Actor Blueprint responsible for rendering the sector's primary star.
  - Components:
    - `StarBody`: static mesh sphere with unlit emissive procedural material.
    - `CoronaShell`: larger sphere or camera-facing billboard with additive/fresnel corona material.
    - `ProminenceFX`: Niagara component for loop/flare particles.
    - `StarLight`: movable DirectionalLight for system lighting, not PointLight, because Unreal directional lights are the correct sun-light model.
    - Optional later: `SkyAtmosphere`/planet atmosphere sun binding.
- Materials:
  - `M_StarSurface`: procedural emissive plasma, parameterized by class color, spot color, flow speed, noise scale, emission.
  - `M_StarCorona`: additive/fresnel rim and halo.
  - `M_StarBillboardFlare`: camera-facing soft glare if the body is distant.
- Profiles:
  - Start with Blueprint/data-table profile rows for `O/B/A/F/G/K/M/L`.
  - Keep colors close to Godot initially but tune exposure/bloom in Unreal, not by overdriving the texture.

## Unreal Tooling Notes

- Use Material Instances or Dynamic Material Instances for per-star parameters.
- Use a Material Parameter Collection only for global scene values shared by many star-related materials, such as global star exposure/bloom strength.
- Use Niagara for corona/prominence/flare work instead of C++ procedural mesh generation.
- Use a DirectionalLight for system lighting. Use PointLight only for short-range artistic fill if needed.
- Use post-process bloom/exposure carefully. Emissive stars should bloom, but the base material should stay readable.

## Integration

No immediate C++ foundation is required:

- Existing `FBodyDefinition` already has `BodyType`, `Transform`, `VisualRadiusCm`, and `VisualProfileId`.
- Existing `FStarSystemDefinition` already has `Bodies` and `PresentationActorClass`.
- First pass can add a star component/child actor to `BP_SystemSpacePresentation` or create `BP_SectorStarAnchor` and have the presentation Blueprint own/place it.
- Later, if we need true per-system stellar profiles, add or extend a data asset. Do that only after the Blueprint surface proves out.

## First Implementation Pass

1. Create/import a high-segment sphere mesh for the star body if the engine sphere is too coarse.
2. Author `M_StarSurface` and `M_StarCorona` with Blueprint-exposed parameters.
3. Create `BP_SectorStarAnchor` with body, corona, flare placeholder, and directional light.
4. Wire one instance into the current playable slice presentation as a G/K-class sector anchor.
5. Tune in PIE with the player near normal spawn:
   - Star reads as distant and large.
   - Surface has motion/detail without texture blocking.
   - Halo does not wash out the skybox.
   - No multiple directional-light warning.

## Later Passes

- Add Niagara prominence loops and sparse flare tongues.
- Add sector-map star markers using the Godot nearby-star data.
- Add profile rows for binary systems and compact remnants.
- Bind star color/light direction into planet/atmosphere materials.
