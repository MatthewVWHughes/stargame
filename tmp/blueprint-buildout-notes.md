# Blueprint Buildout Notes

Created by editor automation after the C++/Blueprint boundary cleanup.

## Created Blueprint Assets

UI:
- `/Game/Blueprints/UI/WBP_BootMenu`
- `/Game/Blueprints/UI/WBP_Comms`
- `/Game/Blueprints/UI/WBP_Dialog`
- `/Game/Blueprints/UI/WBP_PauseMenu`
- `/Game/Blueprints/UI/WBP_StationInteraction`
- `/Game/Blueprints/UI/WBP_SystemMap`
- `/Game/Blueprints/UI/WBP_FlightHUD`

Core/flight/space:
- `/Game/Blueprints/Core/BP_StargamePlayerController`
- `/Game/Blueprints/Flight/BP_WayfarerFlightPawn`
- `/Game/Blueprints/Space/BP_SystemSpacePresentation`
- `/Game/Blueprints/Space/BP_SystemEntityMarker`
- `/Game/Blueprints/Space/BP_CombatShotPresentation`
- `/Game/Blueprints/Space/BP_WayfarerTrafficShip`
- `/Game/Blueprints/Space/BP_SystemEncounterActor`

Station:
- `/Game/Blueprints/Station/BP_StationInteriorRoom`
- `/Game/Blueprints/Station/BP_StationInteriorPawn`
- `/Game/Blueprints/Station/BP_StationInteractable`
- `/Game/Blueprints/Station/BP_MissionContact`
- `/Game/Blueprints/Station/BP_StationHostile`

## Wired References

- `BP_StargameGameMode`
  - `BootMenuWidgetClass = WBP_BootMenu`
  - `PlayerControllerClass = BP_StargamePlayerController`
  - `HUDClass = None`
- `BP_StargamePlayerController`
  - `SystemMapWidgetClass = WBP_SystemMap`
  - `CommsWidgetClass = WBP_Comms`
  - `PauseMenuWidgetClass = WBP_PauseMenu`
  - `FlightHUDWidgetClass = WBP_FlightHUD`
- `BP_StationInteriorPawn`
  - `StationInteractionWidgetClass = WBP_StationInteraction`
- `BP_StationInteriorRoom`
  - `MissionContactActorClass = BP_MissionContact`
  - `InteractableActorClass = BP_StationInteractable`
  - `HostileActorClass = BP_StationHostile`
  - Authored player start and hostile spawn transforms.
- `wayfarer_mk1`
  - `PawnClass = BP_WayfarerFlightPawn`
  - `ActorClass = BP_WayfarerTrafficShip`
- `frontier_test_01` and `frontier_arrival_test_01`
  - `PresentationActorClass = BP_SystemSpacePresentation`
  - `EntityMarkerActorClass = BP_SystemEntityMarker`
  - `CombatShotPresentationActorClass = BP_CombatShotPresentation`
  - `EncounterActorClass = BP_SystemEncounterActor`
- `frontier_test_01` stations
  - `InteriorRoomClass = BP_StationInteriorRoom`
  - `InteriorPawnClass = BP_StationInteriorPawn`

## Current Fidelity

These are functional first-pass Blueprint assets, not final presentation:
- UI widgets have the correct parent classes and binding names, with placeholder text/buttons.
- Actor Blueprints use simple engine primitive meshes/lights owned by Blueprint, so C++ stays presentation-free.
- Data references are wired and verified through Unreal Python.

## Next Pass: Station Service Props

Moved/staged from C++ into Blueprint ownership:
- The old empty `AStationInteriorRoomActor::SpawnDefaultInteractables()` hook is gone.
- `AStationInteriorRoomActor` now discovers authored `ChildActorComponent` service props after `ConfigureStationInterior()`.
- `BP_StationInteriorRoom` should own the service prop list and positions as child actors.
- `BP_StationInteriorRoom` now contains these child actor service props:
  - `RepairService`: `InteractionType=repair`, `DisplayName=Repair Service`
  - `RefuelService`: `InteractionType=refuel`, `DisplayName=Refuel Service`
  - `MarketService`: `InteractionType=market`, `DisplayName=Commodity Desk`
  - `LaunchService`: `InteractionType=launch`, `DisplayName=Airlock / Launch`
- Each service uses `BP_StationInteractable` as its child actor class.
- The service identifiers/display names live in standard Blueprint `ComponentTags` so the current editor can author them safely. Newer authored interactable subclasses may alternatively set `DefaultInteractionType` and `DefaultDisplayName` after the editor reloads the rebuilt C++ module.
- C++ only registers those authored props and configures them with the current station id; the presence, placement, names, and visuals now belong to Blueprint.
- `OnStationInteriorConfigured` is available on `BP_StationInteriorRoom` for later Blueprint-only dynamic additions after C++ has finished its foundation setup.
- Authoring script: `tmp/author_station_services.py`.

## Next Pass: Station Presentation Components

Authored Blueprint-owned placeholder presentation:
- `BP_StationInteriorRoom`: added walls, ceiling, and a service-area light around the existing floor.
- `BP_StationInteractable`: added console, beacon, focus glow, and service label components around the existing service prop.
- `BP_MissionContact`: added head, contact marker, contact glow, and prompt text components around the existing NPC body.
- `BP_StationHostile`: added head, weapon block, alert glow, and status text components around the existing hostile body.
- Text/light defaults are configured by `tmp/configure_station_visual_components.py`.
- Authoring script: `tmp/author_station_blueprint_visuals.py`.

## Next Pass: UMG Layouts

Authored first usable layout pass:
- Added button labels to boot, comms, pause, and station interaction widgets.
- Positioned existing bound widgets on the canvas for boot, station interaction, comms, pause, dialog, system map, and flight HUD.
- Compiled and saved the widget Blueprints after the layout pass.
- `WBP_FlightHUD` is now an authored player-controller widget class instead of an unspawned asset.
- Authoring script: `tmp/author_ui_widget_layouts.py`.

## Next Pass: Flight / Space Presentation Components

Authored Blueprint-owned placeholder presentation:
- `BP_WayfarerFlightPawn`, `BP_WayfarerTrafficShip`, and `BP_SystemEncounterActor`: added wing blocks, cockpit canopy, engine core, and running light around the existing hull/engine components.
- `BP_SystemSpacePresentation`: added distant star disc and star glow around the existing system lighting.
- `BP_SystemEntityMarker`: added marker core and text label around the existing marker mesh/glow.
- `BP_CombatShotPresentation`: added start/end flash spheres and shot glow around the existing beam.
- Text/light defaults are configured by `tmp/configure_space_visual_components.py`.
- Authoring script: `tmp/author_space_blueprint_visuals.py`.

## Next Pass: Combat / Trading / AI Surfaces

Broadened beyond stations:
- `WBP_FlightHUD`: added Blueprint-owned text regions for encounter state, weapon state, traffic, encounter comms, and cargo/trade summaries.
- `WBP_StationInteraction`: added action-list and market-row placeholder regions for later Blueprint dynamic buy/sell row generation from `FStargameStationInteractionActionView`.
- `BP_WayfarerTrafficShip` and `BP_SystemEncounterActor`: added traffic state label and intent glow components so traffic/AI/encounter state has an authored visual surface.
- `AStargamePlayerController` now has a `FlightHUDWidgetClass` foundation hook and spawns the authored HUD on `BeginPlay`; wire it to `WBP_FlightHUD` after restarting the editor so the new C++ property is visible.
- Authoring script: `tmp/author_broader_gameplay_blueprint_surfaces.py`.
- Component config script: `tmp/configure_broader_gameplay_surfaces.py`.

## Next Pass: Sparse Default Flight UI

Runtime test on May 19, 2026 reached world-space after clicking New Game, but the default flight view had placeholder/debug text everywhere.
Fix applied:
- `WBP_FlightHUD` keeps the staged Blueprint text regions, but hides `TargetText`, `DockingText`, `MissionText`, `EncounterText`, `WeaponText`, `TrafficText`, `CommsLineText`, and `CargoTradeText` by default.
- `SpeedText` remains visible as a small passive readout.
- `WBP_StationInteraction` hides the staged action/market placeholder hints by default.
- Panel text font sizes were tightened for comms, dialog, pause, station interaction, and system map.
- The repeatable broader-gameplay pass now creates those staging regions blank/collapsed rather than visible placeholder words.
- Authoring script: `tmp/refine_space_hud_blueprint_ui_bridge.py`.
- `tmp/compile_blueprint_slice_assets.py` succeeded after the refinement.

## Fix: Duplicate Directional Light Warning

Runtime test after the sparse HUD pass reported a warning about multiple directional lights.
Fix applied:
- Removed the Blueprint-owned `DirectionalLightComponent` named `KeyLight` from `/Game/Blueprints/Space/BP_SystemSpacePresentation`.
- Removed the `KeyLight` creation from `tmp/build_blueprint_assets.py` so future Blueprint rebuild passes do not recreate it.
- The playable map still owns the world directional light through `tmp/configure_playable_slice_map.py`.
- `BP_SystemSpacePresentation` still owns authored presentation lighting through non-directional components such as `FillLight` and `StarGlow`.
- `tmp/compile_blueprint_slice_assets.py` succeeded after the removal.

## Next Pass: Milky Way Skybox

Imported the Godot/Starlight Milky Way skybox source into Unreal:
- Source texture: `/home/matthew/Repos/starlight/game/textures/planets/8k_stars_milky_way.jpg`
- Unreal texture: `/Game/Textures/Sky/T_8K_Stars_MilkyWay`
- Unreal material: `/Game/Materials/Sky/M_MilkyWaySkybox`
- The material is unlit, opaque, and two-sided, with the Milky Way texture feeding emissive color.
- `BP_SystemSpacePresentation` now owns `MilkyWaySkyboxSphere`, a large static-mesh sphere using that material.
- First runtime test showed it read as a small scene sphere, so `MilkyWaySkyboxSphere` was resized to scale `250000,250000,250000`.
- Second runtime test showed blocky/color-distorted texture artifacts. Fix applied:
  - `T_8K_Stars_MilkyWay` now uses sky-friendly non-block texture compression settings.
  - `M_MilkyWaySkybox` is recreated cleanly as an unlit two-sided emissive material each skybox pass.
  - Added `/Game/Meshes/Sky/SM_MilkyWaySkySphere`, a generated high-segment lat-long sphere mesh, and assigned it to `MilkyWaySkyboxSphere` instead of `/Engine/BasicShapes/Sphere`.
- Third runtime test showed the raw source was still too blue and low quality for Unreal exposure. Fix applied:
  - Added `tmp/generate_milky_way_unreal_texture.py`.
  - Generated `Content/SourceArt/Sky/T_MilkyWay_UnrealBalanced.png` from the Godot source with blue suppression, dark-floor cleanup, star contrast, and mild sharpening.
  - Unreal now imports `/Game/Textures/Sky/T_MilkyWay_UnrealBalanced` for `M_MilkyWaySkybox`.
  - Reduced material emissive multiplier from `1.35` to `0.82`.
- Fourth runtime test showed the balanced texture was too bright and the sky still did not feel distant. Fix applied:
  - Reduced material emissive multiplier again to `0.12`.
  - Removed `MilkyWaySkyboxSphere` from `BP_SystemSpacePresentation`.
  - Added `MilkyWaySkyboxSphere` to `BP_WayfarerFlightPawn` instead, using `SM_MilkyWaySkySphere`, scale `60000`, no collision, and absolute world rotation so it follows player location without rotating with the ship.
- Fifth runtime test still showed bad color and closeness. Fix applied:
  - `tmp/generate_milky_way_unreal_texture.py` now creates a cleaner procedural-neutral sky texture: the old Godot JPG only supplies a blurred galactic-density mask, while crisp stars are generated procedurally.
  - The generated texture is intentionally dark and near-neutral by channel stats, avoiding the old blue cast.
  - `MilkyWaySkyboxSphere` on `BP_WayfarerFlightPawn` was resized to scale `250000`.
  - Material emissive multiplier was set to `0.20` for the darker generated texture.
- This remains Blueprint presentation ownership; no C++ game foundation changes were needed.
- Authoring scripts:
  - `tmp/generate_milky_way_unreal_texture.py`
  - `tmp/import_milky_way_skybox_assets.py`
  - `tmp/create_milky_way_sky_sphere_mesh.py`
  - `tmp/configure_milky_way_skybox_component.py`
  - `tmp/author_milky_way_skybox.py`
- `tmp/compile_blueprint_slice_assets.py` succeeded after the skybox pass.

## Next Pass: Real Flight HUD

Runtime testing showed the sparse HUD went too far: world-space had no useful in-game UI.
Fix applied:
- Added `UStargameFlightHUDWidget` as a thin C++ foundation widget that polls existing pawn/session/star-system data and exposes it to Blueprint.
- `AStargamePlayerController` now spawns `TSubclassOf<UStargameFlightHUDWidget>` for `FlightHUDWidgetClass`.
- `WBP_FlightHUD` now inherits from `UStargameFlightHUDWidget`.
- Authored Blueprint-owned HUD widgets for:
  - speed and acceleration
  - throttle text and throttle bar
  - flight mode
  - selected target and distance
  - docking state
  - weapon status, energy, and cooldown
  - radar target count and traffic count
  - shield, hull, and fuel bars
- The C++ base updates optional bound widgets and fires `OnFlightHUDViewChanged` so later Blueprint graph work can replace the default text/bar handling.
- Authoring script: `tmp/author_flight_hud_blueprint_ui.py`.
- `make StargameEditor` and `tmp/compile_blueprint_slice_assets.py` succeeded after this pass.

Follow-up fix:
- The HUD was visible on the boot menu because the player controller creates it during `BeginPlay`, before the game has spawned a flight pawn.
- `UStargameFlightHUDWidget` now starts collapsed and only becomes visible while an `ASpaceFlightPawn` is possessed.
- `WBP_FlightHUD` root canvas must remain `SelfHitTestInvisible`; hiding the root prevents the HUD from appearing after flight starts.
- `make StargameEditor` and `tmp/compile_blueprint_slice_assets.py` succeeded after the visibility fix.

Second follow-up fix:
- Widget-level hiding was still too brittle because `AStargamePlayerController` created `WBP_FlightHUD` during boot-menu `BeginPlay`.
- `AStargamePlayerController` now creates the flight HUD only when it possesses `ASpaceFlightPawn`, and removes it when it no longer possesses a flight pawn.
- This keeps the HUD out of the main menu by construction while preserving the Blueprint-owned HUD layout.
- `make StargameEditor` succeeded after the possession-lifecycle fix.

## Next Pass: Blueprint Event Hooks

Staged after the editor/bridge went down:
- `AStargameGameModeBase` now marks the boot menu widget focusable before setting UI focus. This keeps the C++ foundation minimal but fixes the runtime error that blocked reliable Blueprint boot-menu interaction.
- The editor crash on May 19, 2026 happened when automation sent `add_canvas_panel RootCanvas` to an existing UMG widget. The stack was `UWidgetTree::ForWidgetAndChildren`, which indicates a bad/recursive widget-tree mutation during duplicate widget insertion.
- `tmp/build_blueprint_assets.py` is now idempotent for widgets: it sets parent classes on existing widgets but only creates the initial `RootCanvas` tree for newly created widget assets.
- `McpAutomationBridge_WidgetAuthoringHandlers.cpp` now treats duplicate `add_canvas_panel`, `add_text_block`, and `add_button` calls as successful no-ops instead of constructing duplicate named widgets. `make StargameEditor` succeeded with this bridge fix.
- `tmp/author_blueprint_event_hooks.py` adds Blueprint override event nodes for the presentation hooks already exposed by C++:
  - boot, comms, dialog, pause, station interaction, and system map widgets
  - system presentation, combat shot presentation, traffic ship, encounter actor
  - station room, interactable, mission contact, and hostile station actors
- `tmp/run_blueprint_playable_slice_pass.py` runs the full Blueprint playable-slice buildout in order: asset creation, layout/visual authoring, event hook creation, data wiring, map setup, wiring verification, and validation.
- These scripts require the Unreal editor bridge to be running. They are staged on disk now so the next editor restart can run the full pass without losing the migration context.
- `make StargameEditor` succeeded after the focus fix.

Next Blueprint work:
- Add advanced styling, animations, and dynamic list/button generation.
- Run `tmp/run_blueprint_playable_slice_pass.py` after the editor is restarted.
- Fill in the event graph bodies for traffic/encounter/combat shot configured events, station market action rows, system-map marker rendering, and HUD polling from session/pawn view models.
- Replace primitive meshes with actual ship, station, NPC, marker, and combat VFX assets.
- Add Niagara/audio and event logic for the presentation Blueprint events listed in `tmp/blueprint-migration-notes.md`.

## Next Pass: Sector Star Anchors

Goal:
- Replace the old placeholder star disc with a reusable sector star anchor.
- Provide a preview map where stellar classes can be clicked through quickly.

Godot reference:
- `starlight/game/scenes/game/space/Sun.tscn` had a reusable sun body, halo, flare field, and light.
- `starlight/game/scripts/star/Star.cs` drove `O/B/A/F/G/K/M/L` class colors and emissions.
- `starlight/game/scripts/game/space/SunFlareField.cs` procedurally built coronal loop strands.

Unreal implementation:
- Added `ASectorStarAnchorActor` as a thin foundation actor for safe component setup, dynamic material parameters, and preview input.
- Authored `/Game/Blueprints/Space/BP_SectorStarAnchor`.
- Authored `/Game/Materials/Space/M_StarSurface` and `/Game/Materials/Space/M_StarCorona`.
- Wired a `SectorStarAnchor` child actor into `/Game/Blueprints/Space/BP_SystemSpacePresentation`.
- Disabled the old `DistantStarDisc` and `StarGlow` placeholder components in `BP_SystemSpacePresentation`.
- Removed standalone `DirectionalLight` actors from `/Game/Maps/StargamePlayableSlice`; the sector star anchor owns its directional star light.
- Authored `/Game/Maps/StellarClassPreview`.

Preview controls:
- Left click, Space, or Right Arrow cycles to the next stellar class.
- Right click or Left Arrow cycles to the previous stellar class.

Verification:
- `make StargameEditor` succeeded after the C++ foundation actor.
- `tmp/author_sector_star_anchor_assets.py` authored the Blueprint/material/map assets successfully after editor restart.
- `tmp/verify_sector_star_anchor_assets.py` passed.

Follow-up:
- First user preview said this looked worse than Godot. Fix applied:
  - Replaced the simple Fresnel-only star material with a custom-expression procedural plasma material driven by normal, view vector, time, and stellar-class color parameters.
  - Replaced the corona material with a noisier additive custom-expression material so the halo breaks up instead of reading as a plain shell.
  - Added `/Game/Materials/Space/M_StarProminence`.
  - Generated and imported `/Game/Meshes/Space/SM_StarProminenceBundle`.
  - Added ten `ProminenceLoopXX` static mesh components to `BP_SectorStarAnchor` and configured varied rotations/scales around the limb.
  - Resized `CoronaShell` to better match the old Godot body-to-halo proportion.
  - `tmp/refine_sector_star_visuals.py` applies the refined materials, mesh, and component tuning.
  - `tmp/add_sector_star_prominence_components.py` stages the loop components if the Blueprint is rebuilt from scratch.
- Verification after refinement:
  - `tmp/compile_blueprint_slice_assets.py` passed.
  - `tmp/verify_sector_star_anchor_assets.py` passed.

Preview map boot-menu fix:
- `/Game/Maps/StellarClassPreview` must explicitly override its GameMode to `/Script/Engine.GameModeBase`.
- Leaving the map GameMode unset falls back to `Config/DefaultEngine.ini` project default `BP_StargameGameMode`, which creates `WBP_BootMenu` on BeginPlay.
- Added `tmp/fix_stellar_preview_game_mode.py` as a targeted repair script.
- Updated `tmp/build_unreal_native_sector_star.py` to set the same override whenever the preview map is rebuilt.
- Updated `tmp/verify_sector_star_anchor_assets.py` to fail if the preview map does not override to `GameModeBase`.
- Verification after the fix: `tmp/fix_stellar_preview_game_mode.py` passed and `tmp/verify_sector_star_anchor_assets.py` passed.

Restart visual fix:
- After editor restart, the native `StarHaloBillboard` component existed but was still hidden/unassigned, while the temporary `StarHaloBillboardSCS` fallback plane was visible without C++ camera alignment.
- Prominence material also had depth test disabled, which made back-side loops draw through the opaque photosphere and read as strange geometry on a ball.
- Added and ran `tmp/fix_sector_star_after_restart.py`:
  - Assigns `StarHaloMaterial` and `StarProminenceMaterial` on the now-loaded C++ class defaults.
  - Enables the native camera-aligned `StarHaloBillboard`.
  - Hides the temporary `StarHaloBillboardSCS` fallback.
  - Restores depth testing on `M_StarProminence`.
  - Restricts visible prominence loops to six restrained limb details.
- Verification after this restart fix: `tmp/verify_sector_star_anchor_assets.py` passed.

Reset/retry pass:
- User rejected the plane halo and prominence-loop approach. Reset the visual stack instead of tuning it.
- Added `tmp/reset_retry_sector_star_base.py`.
- New baseline:
  - Imports `/Game/Meshes/Space/SM_StarPhotosphereDisplaced`, a higher-resolution displaced sphere mesh.
  - Rebuilds `/Game/Materials/Space/M_StarSurface` as the photosphere material.
  - Adds `/Game/Materials/Space/M_StarAtmosphere` and uses `CoronaShell` only as a subtle integrated atmosphere shell.
  - Hides both `StarHaloBillboard` and `StarHaloBillboardSCS`.
  - Hides all `ProminenceLoopXX` components.
- Updated `tmp/verify_sector_star_anchor_assets.py` to enforce the reset contract: displaced photosphere mesh active, atmosphere shell active, plane halo hidden, and prominences hidden.
- Verification after reset retry:
  - `tmp/reset_retry_sector_star_base.py` passed.
  - `tmp/compile_blueprint_slice_assets.py` passed.
  - `tmp/verify_sector_star_anchor_assets.py` passed.

Follow-up:
- If the refined version still falls short, add Niagara prominence loops and animated particle flare tongues instead of static loop meshes.

Second follow-up:
- User preview correctly identified that the refined version still read as a sphere with color variation.
- Replanned with two peer reviews and stopped treating Godot as a port target. The Unreal-native architecture is now layered:
  - `StarBody`: photosphere only, opaque unlit emissive material with procedural plasma and subtle World Position Offset.
  - `StarHaloBillboard` / `StarHaloBillboardSCS`: camera-facing/billboard-style additive radial halo, replacing the old sphere-shell corona as the visible broad glow.
  - `ProminenceLoopXX`: UV-authored loop mesh layer with animated additive prominence material.
  - `CoronaShell`: retained only as a hidden legacy component so the previous sphere-shell corona no longer contributes visually.
- Added native C++ support for a future editor restart:
  - `ASectorStarAnchorActor` now has a native `StarHaloBillboard` component, tick-based camera alignment, `StarHaloMaterial`, `StarProminenceMaterial`, and class-driven prominence material instances.
  - The running editor still had the old class loaded, so `StarHaloBillboardSCS` was added as a live Blueprint fallback component for this pass.
- Added `/Game/Materials/Space/M_StarHaloBillboard`.
- Rebuilt `/Game/Materials/Space/M_StarSurface`, `/Game/Materials/Space/M_StarCorona`, `/Game/Materials/Space/M_StarProminence`, and `/Game/Meshes/Space/SM_StarProminenceBundle`.
- Expanded prominence loops from 10 to 18 components.
- Rebuilt `/Game/Maps/StellarClassPreview` as a validation scene with one focused interactive star, an O/B/A/F/G/K/M/L class row, a post-process volume for exposure/bloom validation, and a silhouette object for depth/sorting checks.
- `tmp/build_unreal_native_sector_star.py` is the new authoritative authoring pass for the star stack.
- `tmp/verify_sector_star_anchor_assets.py` now validates the new contract: billboard halo present, legacy corona shell hidden, at least 18 prominence loops, preview class row present, and post process present.
- Verification:
  - `make StargameEditor` passed after the C++ support change.
  - `tmp/add_sector_star_prominence_components.py` added loops 10-17 and skipped existing loops 00-09.
  - `tmp/build_unreal_native_sector_star.py` passed through the Unreal bridge.
  - `tmp/compile_blueprint_slice_assets.py` passed.
  - `tmp/verify_sector_star_anchor_assets.py` passed.

Texture-driven Unreal star reset:
- User rejected the static prominence loops, plane halo, and displaced/ridged sphere approach. Those layers are no longer part of the active star look.
- Active visual stack is now:
  - `StarBody`: high-resolution smooth sphere mesh `/Game/Meshes/Space/SM_StarPhotosphereSmooth`.
  - `M_StarSurface`: opaque unlit emissive photosphere material driven by imported mask texture `/Game/Textures/Space/Star/T_StarPhotosphereMasks`.
  - `CoronaShell`: same smooth sphere mesh, scaled slightly larger, using additive unlit `/Game/Materials/Space/M_StarCorona`.
  - `M_StarCorona`: integrated rim/corona shell driven by `/Game/Textures/Space/Star/T_StarCoronaMasks`.
- Disabled visual layers:
  - `StarHaloBillboard`
  - `StarHaloBillboardSCS`
  - all `ProminenceLoopXX` static mesh components
- The disabled halo/prominence assets remain only as staged legacy placeholders so existing Blueprint references do not break. They should be replaced later by Niagara ribbons/particles after the base photosphere and corona read correctly.
- Generated source texture masks:
  - `Content/SourceArt/Space/Star/T_StarPhotosphereMasks.png`
  - `Content/SourceArt/Space/Star/T_StarCoronaMasks.png`
- Authoring scripts:
  - `tmp/generate_star_source_textures.py`
  - `tmp/implement_texture_driven_unreal_star.py`
- Verification contract updated in `tmp/verify_sector_star_anchor_assets.py`:
  - `StarBody` must use `SM_StarPhotosphereSmooth` and `M_StarSurface`.
  - `CoronaShell` must be visible and use `M_StarCorona`.
  - billboard halo and static prominence loops must remain hidden.
  - `StellarClassPreview` must retain its `GameModeBase` override and post-process volume.
- Verification result:
  - `tmp/verify_sector_star_anchor_assets.py` passed through Unreal commandlet with 0 errors.
- Tooling caveat:
  - The live Unreal bridge was unavailable/refusing connections during this pass.
  - The first commandlet application saved the texture-driven assets successfully.
  - A later NullRHI commandlet reimport attempt crashed inside Unreal's Slate/AssetImportTask path, so do not use NullRHI commandlets for texture/mesh import here. Use the live editor bridge for future import passes.
