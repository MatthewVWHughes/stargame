import traceback
import unreal

try:
    path = "/Game/Blueprints/UI"
    name = "WBP_BootMenu"
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)

    asset_path = path + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        bp = unreal.EditorAssetLibrary.load_asset(asset_path)
        print("exists", bp, type(bp), [x for x in dir(bp) if "widget" in x.lower() or "tree" in x.lower()][:50])
        for prop in ("widget_tree", "generated_class", "parent_class"):
            try:
                print(prop, bp.get_editor_property(prop))
            except Exception as exc:
                print(prop, "ERR", repr(exc))
    else:
        factory = unreal.WidgetBlueprintFactory()
        parent = unreal.load_class(None, "/Script/Stargame.StargameBootMenuWidget")
        try:
            factory.set_editor_property("parent_class", parent)
            print("set parent ok")
        except Exception as exc:
            print("set parent failed", repr(exc))
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, path, unreal.WidgetBlueprint, factory)
        print("created", bp, type(bp), [x for x in dir(bp) if "widget" in x.lower() or "tree" in x.lower()][:50])
        if bp:
            print("widget_tree prop", bp.get_editor_property("widget_tree"))
            unreal.EditorAssetLibrary.save_loaded_asset(bp)
except Exception:
    print(traceback.format_exc())
