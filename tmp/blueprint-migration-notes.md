# Blueprint Migration Notes

Working handoff for C++ presentation code removed during the boundary cleanup. These are not formal docs; they preserve what was stripped so equivalent Blueprint/content can be authored deliberately.

## Boot Menu

Removed from C++:
- Canvas root, centered border panel, size box, vertical stack.
- Hardcoded title/subtitle text: `STARGAME`, `Frontier test build`.
- Button layout and labels: `New Game`, `Continue`, `Quit`.
- Hardcoded colors, padding, font sizes, wrap widths.
- Default game mode assignment to the native boot menu class.
- Default game mode assignment to the legacy native Canvas HUD.

C++ now provides:
- `UStargameBootMenuWidget::NewGame`, `Continue`, `Quit`, `RefreshContinueState`.
- Optional bound widgets: `NewGameButton`, `ContinueButton`, `QuitButton`, `StatusText`.
- Blueprint events: `OnStatusChanged`, `OnContinueAvailabilityChanged`.
- `AStargameGameModeBase::BootMenuWidgetClass`.
- No native boot menu or HUD presentation fallback when Blueprint classes are unset.

Blueprint needed:
- `WBP_BootMenu` subclass of `UStargameBootMenuWidget`.
- Bind the optional buttons/text or call the BlueprintCallable methods directly.
- Own all visual style, layout, copy, transitions, and disabled/available states.
- Set `BootMenuWidgetClass` on the project game mode Blueprint.
- Assign any production HUD through the authored game mode/player controller Blueprint.

## Comms Panel

Removed from C++:
- Canvas root, centered panel, fixed width `560`, border color/padding.
- Text construction, font sizes, color palette, wrap widths.
- Buttons and labels: `Request Docking`, `Close`.
- C++ status color choice for accepted/error.
- Player controller default assignment to the native comms widget class.

C++ now provides:
- `FStargameCommsView` view model.
- `OpenComms`, `CloseComms`, `ToggleComms`.
- Optional bound widgets: `TitleText`, `BodyText`, `StatusText`, `RequestDockingButton`, `CloseButton`.
- Blueprint events: `OnCommsOpened`, `OnCommsClosed`, `OnCommsViewChanged`.
- No native comms widget fallback when the class is unset.

Blueprint needed:
- `WBP_Comms` subclass of `UStargameCommsWidget`.
- Build the panel, request/close buttons, status styling, and docking availability state in UMG.
- Either bind the optional widgets/buttons or respond to `OnCommsViewChanged` directly.

## Dialog / Mission Contact Panel

Removed from C++:
- Canvas root, centered panel, fixed width `640`, border color/padding.
- Title/body/status text widgets and font/color/padding choices.
- Dynamic C++ button creation for mission choices.
- Button labels/styling generated as UMG in C++.

C++ now provides:
- Mission contact body/choice view-model builders.
- `ShowDialog`, `ShowMissionContact`, `CloseDialog`, `SelectChoice`.
- Optional bound widgets: `TitleText`, `BodyText`, `StatusText`.
- Blueprint events: `OnDialogShown`, `OnDialogClosed`, `OnDialogStatusChanged`.

Blueprint needed:
- `WBP_Dialog` subclass of `UStargameDialogWidget`.
- Render title/body/status and dynamically create choice buttons in Blueprint from the `InChoices` array.
- Each choice button should call `SelectChoice(ChoiceId, bCloseAfter)`.

## Pause Menu

Removed from C++:
- Canvas root, centered panel, fixed width `820`, border color/padding.
- Tab row construction and labels: `Inventory`, `Missions`, `Settings`.
- Resume button and footer text: `Esc or I closes this panel.`
- Hardcoded text styling, sizes, wrapping, and button widths.
- Player controller default assignment to the native pause menu widget class.

C++ now provides:
- `FStargamePauseMenuView` view model.
- `OpenPauseMenu`, `ClosePauseMenu`, `TogglePauseMenu`, `SetActiveTab`.
- Optional bound widgets: `TitleText`, `BodyText`, `FooterText`, tab buttons, `ResumeButton`.
- Blueprint events: `OnPauseMenuOpened`, `OnPauseMenuClosed`, `OnActiveTabChanged`.
- No native pause menu widget fallback when the class is unset.

Blueprint needed:
- `WBP_PauseMenu` subclass of `UStargamePauseMenuWidget`.
- Render tabs, body content, footer/help text, and active tab styling in UMG.
- Hook tab buttons to `SetActiveTab` and resume to `ClosePauseMenu`.

## Station Interaction Panel

Removed from C++:
- Canvas root, centered station interaction panel, fixed width `600`, border color/padding.
- Title/body/status text construction, font sizes, colors, wrap widths.
- Close button construction and label.
- Primary service button construction and label styling.
- Market buy/sell button construction per commodity.
- Status color selection for accepted/error.

C++ now provides:
- Station object and mission contact panel view construction.
- `ShowStationObject`, `ShowMissionContact`, `ExecutePrimaryAction`, `ExecuteMarketAction`, `ClosePanel`.
- Optional bound widgets: `TitleText`, `BodyText`, `StatusText`, `PrimaryButton`, `CloseButton`.
- `FStargameStationInteractionActionView` action descriptions for primary and market actions.
- Blueprint events: `OnStationInteractionChanged`, `OnStationInteractionClosed`.

Blueprint needed:
- `WBP_StationInteraction` subclass of `UStargameStationInteractionWidget`.
- Render title/body/status and dynamically create action buttons from the `Actions` array.
- Primary actions should call `ExecutePrimaryAction`.
- Market actions should call `ExecuteMarketAction(CommodityId, bBuy)`.
- Close button should call `ClosePanel`.

## System Map

Removed from C++:
- Root canvas construction.
- All Slate painting: background fill, route lines, orbit rings, entry/marker circles, selected/mission rings, player local bubble, labels, title, help text, selected summary, status text.
- Hardcoded map colors, font sizes, marker radii, label visibility rules, and UX instruction copy.
- Player controller default assignment to the native system map widget class.

C++ now provides:
- `FSystemMapOverviewView` view model from the session.
- Map open/close/toggle, pan offset, zoom, static projection helpers, selectable target hit testing, and target selection.
- Blueprint events: `OnMapOpened`, `OnMapClosed`, `OnMapViewChanged`, `OnMapStatusChanged`.
- Blueprint callable helpers: `SetMapPanOffset`, `SetMapZoom`, `SelectAtLocalPosition`.
- No native system map widget fallback when the class is unset.

Blueprint needed:
- `WBP_SystemMap` subclass of `UStargameSystemMapWidget`.
- Render entries, routes, markers, player bubble, labels, selection/highlight states, help/status text.
- Use `ProjectMapPosition`, `CalculateContentBounds`, map pan, and zoom to place widgets or draw in Blueprint/UMG.
- Use `SelectAtLocalPosition` for click selection, or `FindNearestSelectableTarget` plus custom command handling.

## Flight HUD

Removed/gated from normal runtime:
- The legacy Canvas HUD with labels, layout, colors, panels, and weapon/status strings.

C++ now provides:
- Existing telemetry and view-model query methods.
- Legacy Canvas drawing only when `stargame.DebugCanvasHud=1`.

Blueprint needed:
- `WBP_FlightHUD` or equivalent HUD presentation.
- Pull telemetry from `ASpaceFlightPawn`, `UStargameSessionSubsystem`, and `UStarSystemSubsystem`.
- Own all HUD layout, typography, panel visibility, color, weapon labels, and targeting presentation.

## Space Presentation

Removed from C++:
- Starfield instanced sphere generation count/radius/scale.
- Directional/fill light components and color/intensity values.
- Engine basic sphere/material references.

C++ now provides:
- `ASystemSpacePresentationActor` base with `ConfigureForSystem`.
- Blueprint event `OnConfiguredForSystem`.
- `FStarSystemDefinition::PresentationActorClass`.

Blueprint needed:
- `BP_SystemSpacePresentation` subclass.
- Own skybox/starfield, lights, VFX, postprocess, and system-specific visual policy.
- Set `PresentationActorClass` on authored system data.

## Entity Markers

Removed from C++:
- Sphere mesh, basic material, point light.
- Per-type marker colors for station/gate/resource/default.
- Scale and light attenuation policy.

C++ now provides:
- `AM0SystemMarkerActor` base with gameplay/entity IDs and `OnMarkerInitialized`.
- `FStarSystemDefinition::EntityMarkerActorClass`.

Blueprint needed:
- `BP_SystemEntityMarker` subclass or per-type subclasses.
- Own mesh/icon/light/material/style, per-type visual mapping, and scale policy.
- Set `EntityMarkerActorClass` on authored system data or later resolve per profile/type.

## Traffic Ships

Removed from C++:
- Engine cone mesh and fixed ship scale.

C++ now provides:
- `ALogicalTrafficActor` base with traffic/encounter IDs, tags, and behavior/comms state.
- Blueprint events `OnTrafficActorConfigured` and `OnEncounterBehaviorConfigured`.
- Ship archetype `ActorClass` resolution already drives traffic actor class when authored.

Blueprint needed:
- Traffic/encounter ship Blueprint subclasses per ship archetype.
- Own mesh, materials, lights, VFX, animations, and any presentation reaction to behavior/comms state.
- Set ship archetype `ActorClass` to the Blueprint subclass.

## Combat Shot Presentation

Removed from C++:
- Engine cube trace mesh and fixed trace thickness.

C++ now provides:
- `ACombatShotPresentationActor` base with shot ID/type/timing.
- Blueprint event `OnShotPresentationConfigured`.
- `FStarSystemDefinition::CombatShotPresentationActorClass`.

Blueprint needed:
- `BP_CombatShotPresentation` subclass.
- Own beam/projectile mesh, Niagara/audio, impact flashes, timing/fade, and per weapon presentation.
- Set `CombatShotPresentationActorClass` on authored system data or later resolve by weapon profile.

## Flight Pawn Presentation

Removed from C++:
- Cube ship mesh assignment and scale.
- Ship visibility point light.
- Atmospheric dust engine cube/basic material fallback and dust color.
- Native `UStaticMeshComponent` ship visual component.
- Dormant C++ atmospheric dust instance movement/scale simulation and tuning values.

C++ now provides:
- Movement, input, collision root, camera boom/camera anchor, telemetry, docking state.
- Non-rendering `ShipVisualRoot` scene anchor for Blueprint ship art.

Blueprint needed:
- `BP_WayfarerFlightPawn` subclass.
- Attach ship mesh/art, lights, VFX/audio, camera tuning defaults, and local particle visuals to `ShipVisualRoot`.
- Recreate atmospheric dust as Niagara/Blueprint-owned local VFX if wanted. Old C++ behavior used 90 streaks, 3000 cm/s minimum visibility, 2600 cm forward range, 700 cm rear range, 1150 cm horizontal spread, 560 cm vertical spread, and 1.25 travel multiplier.
- Set ship archetype `PawnClass` to the Blueprint subclass.

## Station Interior Room

Removed from C++:
- Cube floor/walls room construction.
- Default service interactable list and positions: repair, refuel, market, launch.
- Hardcoded hostile spawn positions.

C++ now provides:
- `AStationInteriorRoomActor` base with station/session state, contact/interactable/hostile registries, combat hooks.
- Configurable `PlayerStartLocalTransform`, actor class hooks, and `HostileSpawnLocalTransforms`.
- Registration functions for authored child actors.
- Automatic registration/configuration of authored `BP_StationInteractable` child actors after station setup.
- Child actor component tags `InteractionType=<name>` and `DisplayName=<label>` are consumed as Blueprint-authored service metadata.
- `OnStationInteriorConfigured` Blueprint event for additional dynamic room setup after C++ state/contact initialization.
- `FStationDefinition::InteriorRoomClass` and `InteriorPawnClass`; session entry now requires authored subclasses.

Blueprint needed:
- `BP_StationInteriorRoom` subclass.
- `BP_StationInteriorPawn` subclass.
- Own room geometry, spawn markers, service props/interactables, mission contact placements, hostile spawn points.
- Add service interactables as child actors of `BP_StationInteriorRoom` and set their `ComponentTags` or interactable defaults for type/display names.
- Use `OnStationInteriorConfigured` for any later Blueprint-spawned dynamic interactables/contacts/hostiles.
- Set station data `InteriorRoomClass` and `InteriorPawnClass`.

## Station Interactables / Contacts / Hostiles

Removed from C++:
- Interactable cube marker mesh and focus scaling.
- Interactable text render labels/prompts.
- NPC body/head placeholder meshes and focus scaling.
- Hostile body/head placeholder meshes and status text label.
- Hardcoded label/prompt visual presentation.

C++ now provides:
- Interaction/contact/hostile IDs, overlap/collision where needed, focus state, combat state, damage/death callbacks.
- Blueprint events for configured/focus/health/death/prompt changes.

Blueprint needed:
- `BP_StationInteractable`, `BP_MissionContact`, `BP_StationHostile` subclasses.
- Own meshes, labels, prompts, focus highlight, damage/death animations, NPC visuals, audio.

## Station Interaction Panel Class

Removed from C++:
- Direct creation of `UStargameStationInteractionWidget::StaticClass()` from the station interior pawn.

C++ now provides:
- `AStationInteriorPawn::StationInteractionWidgetClass` as an authored widget class hook.
- Runtime interaction commands and input locking.
- No native widget fallback when the class is unset.

Blueprint needed:
- Set `StationInteractionWidgetClass` on the authored station interior pawn Blueprint.
- Point it at the station interaction widget Blueprint that owns the layout, buttons, prompts, and visual state.

## Systemic Encounter Actors

Removed from C++:
- Direct spawning of `ALogicalTrafficActor::StaticClass()` for pirate/patrol systemic encounter actors.

C++ now provides:
- `FStarSystemDefinition::EncounterActorClass` as the authored encounter actor class hook.
- Encounter IDs, role, steering/behavior/comms state, and transform updates.
- Missing encounter presentation class is non-fatal and simply does not spawn a visual actor.

Blueprint needed:
- `BP_SystemicEncounterActor` subclass of `ALogicalTrafficActor`.
- Own ship mesh, role-specific visuals, VFX/audio, behavior/comms presentation, and steering animation.
- Set `EncounterActorClass` on authored system data.

## Known Remaining C++ Presentation Debt

- Encounter behavior/comms variants, local offsets, and comms lines still live in `StarSystemSubsystem.cpp`; they should move to encounter presentation data assets or tables.
- Playable fixture content still has C++ fallback authoring. Keep it for tests/fallback, but runtime should prefer authored assets.
