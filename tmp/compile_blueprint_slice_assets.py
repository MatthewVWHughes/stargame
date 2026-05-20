import unreal


ASSETS = [
    "/Game/Blueprints/Core/BP_StargameGameMode",
    "/Game/Blueprints/Core/BP_StargamePlayerController",
    "/Game/Blueprints/Flight/BP_WayfarerFlightPawn",
    "/Game/Blueprints/Space/BP_SystemSpacePresentation",
    "/Game/Blueprints/Space/BP_SectorStarAnchor",
    "/Game/Blueprints/Space/BP_SystemEntityMarker",
    "/Game/Blueprints/Space/BP_CombatShotPresentation",
    "/Game/Blueprints/Space/BP_WayfarerTrafficShip",
    "/Game/Blueprints/Space/BP_SystemEncounterActor",
    "/Game/Blueprints/Station/BP_StationInteriorRoom",
    "/Game/Blueprints/Station/BP_StationInteriorPawn",
    "/Game/Blueprints/Station/BP_StationInteractable",
    "/Game/Blueprints/Station/BP_MissionContact",
    "/Game/Blueprints/Station/BP_StationHostile",
    "/Game/Blueprints/UI/WBP_BootMenu",
    "/Game/Blueprints/UI/WBP_Comms",
    "/Game/Blueprints/UI/WBP_Dialog",
    "/Game/Blueprints/UI/WBP_PauseMenu",
    "/Game/Blueprints/UI/WBP_StationInteraction",
    "/Game/Blueprints/UI/WBP_SystemMap",
    "/Game/Blueprints/UI/WBP_FlightHUD",
]


failures = []
for path in ASSETS:
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        failures.append(f"{path}: failed to load")
        continue
    if isinstance(asset, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        failures.append(f"{path}: failed to save")
    print(f"COMPILED {path}")

if failures:
    raise RuntimeError("\n".join(failures))

print("Blueprint slice assets compiled and saved.")
