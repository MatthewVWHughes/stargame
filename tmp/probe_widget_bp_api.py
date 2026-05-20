import unreal

bp = unreal.EditorAssetLibrary.load_asset("/Game/Blueprints/UI/WBP_FlightHUD")
print(type(bp))
print([name for name in dir(bp) if "tree" in name.lower() or "widget" in name.lower()][:120])
print([name for name in dir(bp) if "property" in name.lower()][:120])
try:
    print(bp.get_editor_property("WidgetTree"))
except Exception as exc:
    print(f"WidgetTree property failed: {exc}")
