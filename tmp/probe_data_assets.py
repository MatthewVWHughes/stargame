import traceback
import unreal

try:
    for path in [
        "/Game/Data/Ships/Archetypes/wayfarer_mk1",
        "/Game/Data/Systems/frontier_test_01",
        "/Game/Blueprints/Core/BP_StargameGameMode",
        "/Game/Blueprints/Core/BP_StargamePlayerController",
    ]:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        print("ASSET", path, asset, type(asset))
        if asset:
            print("DIR", [x for x in dir(asset) if "definition" in x.lower() or "class" in x.lower()][:40])
            for prop in ["definition", "BootMenuWidgetClass", "PlayerControllerClass", "SystemMapWidgetClass"]:
                try:
                    print(prop, asset.get_editor_property(prop))
                except Exception as exc:
                    print(prop, "ERR", repr(exc))
except Exception:
    print(traceback.format_exc())
