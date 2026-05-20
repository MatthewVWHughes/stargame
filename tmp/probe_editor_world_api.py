import unreal


print("EditorLevelLibrary.get_editor_world", unreal.EditorLevelLibrary.get_editor_world())
subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
print("UnrealEditorSubsystem", subsystem)
if subsystem:
    print("subsystem world", subsystem.get_editor_world())
print("EditorLoadingAndSavingUtils has load_map", hasattr(unreal.EditorLoadingAndSavingUtils, "load_map"))
if hasattr(unreal.EditorLoadingAndSavingUtils, "load_map"):
    print("load_map result", unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/StellarClassPreview"))
    print("after load_map world", unreal.EditorLevelLibrary.get_editor_world())
    if subsystem:
        print("after load_map subsystem world", subsystem.get_editor_world())
