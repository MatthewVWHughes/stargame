import unreal

lines = []

def dump(name, obj):
    lines.append(f"--- {name} ---")
    for item in dir(obj):
        if "custom" in item.lower() or "input" in item.lower() or "material" in item.lower() or "connect" in item.lower() or "expression" in item.lower():
            lines.append(item)


dump("MaterialEditingLibrary", unreal.MaterialEditingLibrary)
dump("MaterialExpressionCustom", unreal.MaterialExpressionCustom)
dump("CustomInput", unreal.CustomInput if hasattr(unreal, "CustomInput") else object)

with open("/tmp/stargame_material_python_api.txt", "w", encoding="utf-8") as handle:
    handle.write("\n".join(lines))
