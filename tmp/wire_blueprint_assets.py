import traceback
import unreal


def load_bp_class(path):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if cls is None:
        raise RuntimeError(f"Could not load Blueprint class: {path}")
    return cls


def load_asset(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError(f"Could not load asset: {path}")
    return asset


def compile_save_blueprint(path):
    bp = load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return bp


def set_blueprint_default(bp_path, prop, value):
    bp = load_asset(bp_path)
    cls = unreal.BlueprintEditorLibrary.generated_class(bp)
    cdo = unreal.get_default_object(cls)
    cdo.set_editor_property(prop, value)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    print(f"SET {bp_path}.{prop} = {value}")


def transform(location, rotation=(0.0, 0.0, 0.0), scale=(1.0, 1.0, 1.0)):
    return unreal.Transform(
        location=unreal.Vector(*location),
        rotation=unreal.Rotator(*rotation),
        scale=unreal.Vector(*scale),
    )


def wire_blueprint_defaults():
    boot = load_bp_class("/Game/Blueprints/UI/WBP_BootMenu")
    system_map = load_bp_class("/Game/Blueprints/UI/WBP_SystemMap")
    comms = load_bp_class("/Game/Blueprints/UI/WBP_Comms")
    pause = load_bp_class("/Game/Blueprints/UI/WBP_PauseMenu")
    flight_hud = load_bp_class("/Game/Blueprints/UI/WBP_FlightHUD")
    station_interaction = load_bp_class("/Game/Blueprints/UI/WBP_StationInteraction")
    player_controller = load_bp_class("/Game/Blueprints/Core/BP_StargamePlayerController")

    mission_contact = load_bp_class("/Game/Blueprints/Station/BP_MissionContact")
    interactable = load_bp_class("/Game/Blueprints/Station/BP_StationInteractable")
    hostile = load_bp_class("/Game/Blueprints/Station/BP_StationHostile")

    set_blueprint_default("/Game/Blueprints/Core/BP_StargameGameMode", "boot_menu_widget_class", boot)
    set_blueprint_default("/Game/Blueprints/Core/BP_StargameGameMode", "player_controller_class", player_controller)
    set_blueprint_default("/Game/Blueprints/Core/BP_StargameGameMode", "hud_class", None)

    set_blueprint_default("/Game/Blueprints/Core/BP_StargamePlayerController", "system_map_widget_class", system_map)
    set_blueprint_default("/Game/Blueprints/Core/BP_StargamePlayerController", "comms_widget_class", comms)
    set_blueprint_default("/Game/Blueprints/Core/BP_StargamePlayerController", "pause_menu_widget_class", pause)
    set_blueprint_default("/Game/Blueprints/Core/BP_StargamePlayerController", "flight_hud_widget_class", flight_hud)

    set_blueprint_default("/Game/Blueprints/Station/BP_StationInteriorPawn", "station_interaction_widget_class", station_interaction)
    set_blueprint_default("/Game/Blueprints/Station/BP_StationInteriorRoom", "mission_contact_actor_class", mission_contact)
    set_blueprint_default("/Game/Blueprints/Station/BP_StationInteriorRoom", "interactable_actor_class", interactable)
    set_blueprint_default("/Game/Blueprints/Station/BP_StationInteriorRoom", "hostile_actor_class", hostile)
    set_blueprint_default("/Game/Blueprints/Station/BP_StationInteriorRoom", "player_start_local_transform", transform((-350.0, 0.0, 100.0)))
    set_blueprint_default("/Game/Blueprints/Station/BP_StationInteriorRoom", "hostile_spawn_local_transforms", [
        transform((260.0, 260.0, 0.0)),
        transform((260.0, -260.0, 0.0)),
        transform((-120.0, 360.0, 0.0)),
    ])


def wire_ship_archetype():
    asset = load_asset("/Game/Data/Ships/Archetypes/wayfarer_mk1")
    definition = asset.get_editor_property("definition")
    definition.set_editor_property("pawn_class", load_bp_class("/Game/Blueprints/Flight/BP_WayfarerFlightPawn"))
    definition.set_editor_property("actor_class", load_bp_class("/Game/Blueprints/Space/BP_WayfarerTrafficShip"))
    asset.set_editor_property("definition", definition)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    print("WIRED wayfarer_mk1 PawnClass/ActorClass")


def wire_system_asset(path):
    asset = load_asset(path)
    definition = asset.get_editor_property("definition")
    definition.set_editor_property("presentation_actor_class", load_bp_class("/Game/Blueprints/Space/BP_SystemSpacePresentation"))
    definition.set_editor_property("entity_marker_actor_class", load_bp_class("/Game/Blueprints/Space/BP_SystemEntityMarker"))
    definition.set_editor_property("combat_shot_presentation_actor_class", load_bp_class("/Game/Blueprints/Space/BP_CombatShotPresentation"))
    definition.set_editor_property("encounter_actor_class", load_bp_class("/Game/Blueprints/Space/BP_SystemEncounterActor"))

    room_cls = load_bp_class("/Game/Blueprints/Station/BP_StationInteriorRoom")
    pawn_cls = load_bp_class("/Game/Blueprints/Station/BP_StationInteriorPawn")
    stations = list(definition.get_editor_property("stations"))
    for station in stations:
        station.set_editor_property("interior_room_class", room_cls)
        station.set_editor_property("interior_pawn_class", pawn_cls)
    definition.set_editor_property("stations", stations)

    asset.set_editor_property("definition", definition)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    print(f"WIRED {path} presentation/station classes ({len(stations)} stations)")


def main():
    try:
        wire_blueprint_defaults()
        wire_ship_archetype()
        for system_path in [
            "/Game/Data/Systems/frontier_test_01",
            "/Game/Data/Systems/frontier_arrival_test_01",
        ]:
            wire_system_asset(system_path)
        unreal.EditorAssetLibrary.save_directory("/Game/Blueprints", only_if_is_dirty=False, recursive=True)
        unreal.EditorAssetLibrary.save_directory("/Game/Data", only_if_is_dirty=False, recursive=True)
        print("Blueprint wiring complete.")
    except Exception:
        print(traceback.format_exc())


main()
