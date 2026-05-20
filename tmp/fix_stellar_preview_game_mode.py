import unreal


PREVIEW_MAP_PATH = "/Game/Maps/StellarClassPreview"


def main():
    if hasattr(unreal.EditorLevelLibrary, "editor_end_play"):
        try:
            unreal.EditorLevelLibrary.editor_end_play()
        except Exception:
            pass

    if hasattr(unreal.EditorLoadingAndSavingUtils, "load_map"):
        unreal.EditorLoadingAndSavingUtils.load_map(PREVIEW_MAP_PATH)
    else:
        unreal.EditorLevelLibrary.load_level(PREVIEW_MAP_PATH)

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = editor_subsystem.get_editor_world() if editor_subsystem else None
    if not world:
        raise RuntimeError("No editor world after loading StellarClassPreview")

    settings = world.get_world_settings()
    preview_game_mode = unreal.load_class(None, "/Script/Engine.GameModeBase")
    if not preview_game_mode:
        raise RuntimeError("Could not load /Script/Engine.GameModeBase")

    settings.set_editor_property("default_game_mode", preview_game_mode)
    unreal.EditorLevelLibrary.save_current_level()
    print("StellarClassPreview default game mode set to GameModeBase.")


if __name__ == "__main__":
    main()
