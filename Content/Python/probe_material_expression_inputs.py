import unreal

MATERIAL_PATH = "/Game/Materials/Space/M_StarSurface"
material = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
classes = [
    unreal.MaterialExpressionTextureSampleParameter2D,
    unreal.MaterialExpressionPanner,
    unreal.MaterialExpressionLinearInterpolate,
    unreal.MaterialExpressionMultiply,
    unreal.MaterialExpressionComponentMask,
    unreal.MaterialExpressionFresnel,
]
lines = []
for cls in classes:
    expr = unreal.MaterialEditingLibrary.create_material_expression(material, cls, -400, -400)
    lines.append(f"--- {cls.__name__}:{expr.get_name()} ---")
    try:
        lines.append("inputs=" + repr([str(x) for x in unreal.MaterialEditingLibrary.get_material_expression_input_names(expr)]))
        lines.append("types=" + repr([str(x) for x in unreal.MaterialEditingLibrary.get_material_expression_input_types(expr)]))
    except Exception as exc:
        lines.append(f"error={exc}")
    unreal.MaterialEditingLibrary.delete_material_expression(material, expr)

unreal.MaterialEditingLibrary.recompile_material(material)
with open("/tmp/stargame_material_expression_inputs.txt", "w", encoding="utf-8") as handle:
    handle.write("\n".join(lines))
