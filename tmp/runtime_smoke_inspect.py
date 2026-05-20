import json
import unreal


def class_name(obj):
    if not obj:
        return None
    cls = obj.get_class() if hasattr(obj, "get_class") else obj.__class__
    return cls.get_name() if hasattr(cls, "get_name") else str(cls)


def asset_path(obj):
    if not obj:
        return None
    try:
        return obj.get_path_name()
    except Exception:
        return str(obj)


def find_pie_world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    for method in ("get_game_world", "get_play_world"):
        if hasattr(editor, method):
            try:
                world = getattr(editor, method)()
                if world:
                    return world
            except Exception:
                pass

    if hasattr(unreal, "EditorLevelLibrary"):
        ell = unreal.EditorLevelLibrary
        for method in ("get_play_world", "get_game_world"):
            if hasattr(ell, method):
                try:
                    world = getattr(ell, method)()
                    if world:
                        return world
                except Exception:
                    pass
    return None


world = find_pie_world()
report = {
    "pie_world": world.get_name() if world else None,
    "player_controller": None,
    "pawn": None,
    "runtime_actor_counts": {},
    "player_controller_widget_classes": {},
}

if world:
    gameplay = unreal.GameplayStatics
    pc = gameplay.get_player_controller(world, 0)
    pawn = gameplay.get_player_pawn(world, 0)
    report["player_controller"] = class_name(pc)
    report["pawn"] = class_name(pawn)

    if pc:
        for prop in (
            "system_map_widget_class",
            "comms_widget_class",
            "pause_menu_widget_class",
            "flight_hud_widget_class",
            "flight_hud_widget",
        ):
            try:
                value = pc.get_editor_property(prop)
                report["player_controller_widget_classes"][prop] = asset_path(value)
            except Exception as exc:
                report["player_controller_widget_classes"][prop] = "ERROR: %s" % exc

    actors = gameplay.get_all_actors_of_class(world, unreal.Actor)
    wanted = (
        "BP_SystemSpacePresentation",
        "BP_SystemEntityMarker",
        "BP_CombatShotPresentation",
        "BP_WayfarerTrafficShip",
        "BP_SystemEncounterActor",
        "BP_StationInteriorRoom",
        "BP_StationInteriorPawn",
        "BP_StationInteractable",
        "BP_MissionContact",
        "BP_StationHostile",
    )
    for actor in actors:
        name = class_name(actor)
        for token in wanted:
            if name and token in name:
                report["runtime_actor_counts"][token] = report["runtime_actor_counts"].get(token, 0) + 1

print("STARGAME_RUNTIME_SMOKE=" + json.dumps(report, sort_keys=True))
