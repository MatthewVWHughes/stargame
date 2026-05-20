import unreal


MAP_PATH = "/Game/Maps/StargamePlayableSlice"
GM_PATH = "/Game/Blueprints/Core/BP_StargameGameMode.BP_StargameGameMode_C"


def load_class(path):
    obj = unreal.load_object(None, path)
    if not obj:
        raise RuntimeError("Could not load class/object: %s" % path)
    return obj


gm_class = load_class(GM_PATH)

settings = unreal.GameMapsSettings.get_game_maps_settings()
for prop, value in (
    ("game_default_map", unreal.SoftObjectPath(MAP_PATH)),
    ("editor_startup_map", unreal.SoftObjectPath(MAP_PATH)),
):
    try:
        settings.set_editor_property(prop, value)
    except Exception:
        settings.set_editor_property(prop, MAP_PATH)
if hasattr(settings, "save_config"):
    settings.save_config()

gm_bp = unreal.load_object(None, "/Game/Blueprints/Core/BP_StargameGameMode.BP_StargameGameMode")
if gm_bp:
    cdo = unreal.get_default_object(gm_class)
    cdo.set_editor_property("default_pawn_class", None)
    unreal.EditorAssetLibrary.save_asset("/Game/Blueprints/Core/BP_StargameGameMode")

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_editor_world()
if not world or world.get_path_name().split(".")[0] != MAP_PATH:
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = editor.get_editor_world()

world_settings = world.get_world_settings()
try:
    world_settings.set_editor_property("default_game_mode", gm_class)
except Exception as exc:
    print("WorldSettings default_game_mode not set in-memory: %s" % exc)

actors = unreal.EditorLevelLibrary.get_all_level_actors()
if not any(actor.get_class().get_name() == "PlayerStart" for actor in actors):
    unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0.0, 0.0, 220.0), unreal.Rotator(0.0, 0.0, 0.0))
if not any(actor.get_class().get_name() == "DirectionalLight" for actor in actors):
    unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 800.0), unreal.Rotator(-45.0, 35.0, 0.0))
if not any(actor.get_class().get_name() == "SkyLight" for actor in actors):
    unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0.0, 0.0, 400.0), unreal.Rotator(0.0, 0.0, 0.0))

unreal.EditorLevelLibrary.save_current_level()
print("Configured playable slice map: %s with %s" % (MAP_PATH, GM_PATH))
