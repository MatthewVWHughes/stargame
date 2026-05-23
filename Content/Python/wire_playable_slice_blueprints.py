import os

import unreal


REPORT = []
FAILURES = []


def log(message):
    REPORT.append(message)
    unreal.log(message)


def fail(message):
    FAILURES.append(message)
    REPORT.append("FAIL {0}".format(message))
    unreal.log_error(message)


def load_asset(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        fail("Missing asset: {0}".format(path))
        raise RuntimeError("Missing asset: {0}".format(path))
    return asset


def load_bp_class(path):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if cls is None:
        fail("Missing Blueprint class: {0}".format(path))
        raise RuntimeError("Missing Blueprint class: {0}".format(path))
    return cls


def generated_cdo(blueprint):
    generated_class = unreal.BlueprintEditorLibrary.generated_class(blueprint)
    if generated_class is None:
        fail("Blueprint has no generated class: {0}".format(blueprint.get_path_name()))
        raise RuntimeError("Blueprint has no generated class: {0}".format(blueprint.get_path_name()))
    return unreal.get_default_object(generated_class)


def set_property(target, names, value):
    last_error = None
    for name in names:
        try:
            current = target.get_editor_property(name)
            if current == value:
                log("unchanged {0}.{1}".format(target.get_name(), name))
            else:
                target.set_editor_property(name, value)
                log("set {0}.{1}".format(target.get_name(), name))
            return True
        except Exception as exc:
            last_error = exc

    fail("Could not resolve required property {0}.{1}: {2}".format(target.get_name(), names[0], last_error))
    raise RuntimeError("Could not resolve required property {0}.{1}".format(target.get_name(), names[0]))


def set_struct_property(target, names, value, label):
    last_error = None
    for name in names:
        try:
            current = target.get_editor_property(name)
            if current == value:
                log("unchanged {0}.{1}".format(label, name))
            else:
                target.set_editor_property(name, value)
                log("set {0}.{1}".format(label, name))
            return True
        except Exception as exc:
            last_error = exc

    fail("Could not resolve required property {0}.{1}: {2}".format(label, names[0], last_error))
    raise RuntimeError("Could not resolve required property {0}.{1}".format(label, names[0]))


def compile_and_save(blueprint):
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint):
        raise RuntimeError("Failed to save {0}".format(blueprint.get_path_name()))
    log("saved {0}".format(blueprint.get_path_name()))


def compile_blueprint_asset(path):
    blueprint = load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint):
        fail("Failed to save compiled Blueprint {0}".format(blueprint.get_path_name()))
        raise RuntimeError("Failed to save compiled Blueprint {0}".format(blueprint.get_path_name()))
    log("compiled {0}".format(blueprint.get_path_name()))
    return blueprint


def verify_required_asset(path):
    asset = load_asset(path)
    log("verified asset {0}".format(asset.get_path_name()))
    return asset


def verify_project_config():
    config_path = os.path.join(unreal.Paths.project_config_dir(), "DefaultEngine.ini")
    with open(config_path, "r", encoding="utf-8") as config_file:
        config_text = config_file.read()

    required_lines = [
        "GameDefaultMap=/Game/Maps/StargamePlayableSlice",
        "EditorStartupMap=/Game/Maps/StargamePlayableSlice",
        "GlobalDefaultGameMode=/Game/Blueprints/Core/BP_StargameGameMode.BP_StargameGameMode_C",
    ]
    for line in required_lines:
        if line not in config_text:
            fail("Missing required DefaultEngine.ini line: {0}".format(line))
            raise RuntimeError("Project config missing {0}".format(line))
        log("verified config {0}".format(line))


def save_asset(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError("Failed to save {0}".format(asset.get_path_name()))
    log("saved {0}".format(asset.get_path_name()))


def wire_system_asset(path, classes):
    asset = load_asset(path)
    definition = asset.get_editor_property("Definition")
    asset_name = asset.get_name()

    set_struct_property(definition, ["PresentationActorClass", "presentation_actor_class"], classes["system_presentation"], asset_name)
    set_struct_property(definition, ["EntityMarkerActorClass", "entity_marker_actor_class"], classes["system_marker"], asset_name)
    set_struct_property(definition, ["CombatShotPresentationActorClass", "combat_shot_presentation_actor_class"], classes["combat_shot"], asset_name)
    set_struct_property(definition, ["EncounterActorClass", "encounter_actor_class"], classes["encounter_actor"], asset_name)

    stations = list(definition.get_editor_property("Stations"))
    for index, station in enumerate(stations):
        station_id = station.get_editor_property("StationId")
        label = "{0}.Stations[{1}:{2}]".format(asset_name, index, station_id)
        set_struct_property(station, ["InteriorRoomClass", "interior_room_class"], classes["station_room"], label)
        set_struct_property(station, ["InteriorPawnClass", "interior_pawn_class"], classes["station_pawn"], label)

    definition.set_editor_property("Stations", stations)
    asset.set_editor_property("Definition", definition)
    save_asset(asset)


def wire_ship_archetype(path, classes):
    asset = load_asset(path)
    definition = asset.get_editor_property("Definition")
    asset_name = asset.get_name()

    set_struct_property(definition, ["PawnClass", "pawn_class"], classes["flight_pawn"], asset_name)
    set_struct_property(definition, ["ActorClass", "actor_class"], classes["traffic_ship"], asset_name)

    asset.set_editor_property("Definition", definition)
    save_asset(asset)


def write_report():
    project_saved_dir = unreal.Paths.project_saved_dir()
    report_dir = os.path.join(project_saved_dir, "Automation")
    os.makedirs(report_dir, exist_ok=True)
    report_path = os.path.join(report_dir, "wire_playable_slice_blueprints_report.txt")
    with open(report_path, "w", encoding="utf-8") as report_file:
        report_file.write("\n".join(REPORT))
        report_file.write("\n")
    unreal.log_warning("Blueprint wiring report written to {0}".format(report_path))


def main():
    verify_project_config()
    verify_required_asset("/Game/Maps/StargamePlayableSlice")
    verify_required_asset("/Game/Data/StartProfiles/frontier_test_start")
    verify_required_asset("/Game/Data/Catalog/frontier_catalog")

    boot_menu_cls = load_bp_class("/Game/Blueprints/UI/WBP_BootMenu")
    flight_hud_cls = load_bp_class("/Game/Blueprints/UI/WBP_FlightHUD")
    system_map_cls = load_bp_class("/Game/Blueprints/UI/WBP_SystemMap")
    comms_cls = load_bp_class("/Game/Blueprints/UI/WBP_Comms")
    pause_menu_cls = load_bp_class("/Game/Blueprints/UI/WBP_PauseMenu")
    station_interaction_cls = load_bp_class("/Game/Blueprints/UI/WBP_StationInteraction")

    player_controller_cls = load_bp_class("/Game/Blueprints/Core/BP_StargamePlayerController")
    mission_contact_cls = load_bp_class("/Game/Blueprints/Station/BP_MissionContact")
    station_interactable_cls = load_bp_class("/Game/Blueprints/Station/BP_StationInteractable")
    station_hostile_cls = load_bp_class("/Game/Blueprints/Station/BP_StationHostile")
    station_room_cls = load_bp_class("/Game/Blueprints/Station/BP_StationInteriorRoom")
    station_pawn_cls = load_bp_class("/Game/Blueprints/Station/BP_StationInteriorPawn")
    flight_pawn_cls = load_bp_class("/Game/Blueprints/Flight/BP_WayfarerFlightPawn")
    traffic_ship_cls = load_bp_class("/Game/Blueprints/Space/BP_WayfarerTrafficShip")
    system_presentation_cls = load_bp_class("/Game/Blueprints/Space/BP_SystemSpacePresentation")
    system_marker_cls = load_bp_class("/Game/Blueprints/Space/BP_SystemEntityMarker")
    combat_shot_cls = load_bp_class("/Game/Blueprints/Space/BP_CombatShotPresentation")
    encounter_actor_cls = load_bp_class("/Game/Blueprints/Space/BP_SystemEncounterActor")

    game_mode_bp = load_asset("/Game/Blueprints/Core/BP_StargameGameMode")
    player_controller_bp = load_asset("/Game/Blueprints/Core/BP_StargamePlayerController")
    station_pawn_bp = load_asset("/Game/Blueprints/Station/BP_StationInteriorPawn")
    station_room_bp = load_asset("/Game/Blueprints/Station/BP_StationInteriorRoom")

    game_mode_cdo = generated_cdo(game_mode_bp)
    set_property(game_mode_cdo, ["PlayerControllerClass", "player_controller_class"], player_controller_cls)
    set_property(game_mode_cdo, ["BootMenuWidgetClass", "boot_menu_widget_class"], boot_menu_cls)
    set_property(game_mode_cdo, ["bAutoStartWhenBootMenuMissing", "auto_start_when_boot_menu_missing"], True)

    player_controller_cdo = generated_cdo(player_controller_bp)
    set_property(player_controller_cdo, ["FlightHUDWidgetClass", "flight_hud_widget_class"], flight_hud_cls)
    set_property(player_controller_cdo, ["SystemMapWidgetClass", "system_map_widget_class"], system_map_cls)
    set_property(player_controller_cdo, ["CommsWidgetClass", "comms_widget_class"], comms_cls)
    set_property(player_controller_cdo, ["PauseMenuWidgetClass", "pause_menu_widget_class"], pause_menu_cls)

    station_pawn_cdo = generated_cdo(station_pawn_bp)
    set_property(station_pawn_cdo, ["StationInteractionWidgetClass", "station_interaction_widget_class"], station_interaction_cls)

    station_room_cdo = generated_cdo(station_room_bp)
    set_property(station_room_cdo, ["MissionContactActorClass", "mission_contact_actor_class"], mission_contact_cls)
    set_property(station_room_cdo, ["InteractableActorClass", "interactable_actor_class"], station_interactable_cls)
    set_property(station_room_cdo, ["HostileActorClass", "hostile_actor_class"], station_hostile_cls)

    for blueprint in [game_mode_bp, player_controller_bp, station_pawn_bp, station_room_bp]:
        compile_and_save(blueprint)

    for path in [
        "/Game/Blueprints/Flight/BP_WayfarerFlightPawn",
        "/Game/Blueprints/Space/BP_CombatShotPresentation",
        "/Game/Blueprints/Space/BP_SectorStarAnchor",
        "/Game/Blueprints/Space/BP_SystemEncounterActor",
        "/Game/Blueprints/Space/BP_SystemEntityMarker",
        "/Game/Blueprints/Space/BP_SystemSpacePresentation",
        "/Game/Blueprints/Space/BP_WayfarerTrafficShip",
        "/Game/Blueprints/Station/BP_MissionContact",
        "/Game/Blueprints/Station/BP_StationHostile",
        "/Game/Blueprints/Station/BP_StationInteractable",
        "/Game/Blueprints/UI/WBP_BootMenu",
        "/Game/Blueprints/UI/WBP_Comms",
        "/Game/Blueprints/UI/WBP_Dialog",
        "/Game/Blueprints/UI/WBP_FlightHUD",
        "/Game/Blueprints/UI/WBP_PauseMenu",
        "/Game/Blueprints/UI/WBP_StationInteraction",
        "/Game/Blueprints/UI/WBP_SystemMap",
    ]:
        compile_blueprint_asset(path)

    authored_classes = {
        "station_room": station_room_cls,
        "station_pawn": station_pawn_cls,
        "flight_pawn": flight_pawn_cls,
        "traffic_ship": traffic_ship_cls,
        "system_presentation": system_presentation_cls,
        "system_marker": system_marker_cls,
        "combat_shot": combat_shot_cls,
        "encounter_actor": encounter_actor_cls,
    }

    wire_system_asset("/Game/Data/Systems/frontier_test_01", authored_classes)
    wire_system_asset("/Game/Data/Systems/frontier_arrival_test_01", authored_classes)
    wire_ship_archetype("/Game/Data/Ships/Archetypes/wayfarer_mk1", authored_classes)

    if FAILURES:
        raise RuntimeError("Blueprint playable slice wiring failed with {0} issue(s).".format(len(FAILURES)))

    log("Blueprint playable slice wiring complete. {0} report entries.".format(len(REPORT)))


if __name__ == "__main__":
    try:
        main()
    finally:
        write_report()
