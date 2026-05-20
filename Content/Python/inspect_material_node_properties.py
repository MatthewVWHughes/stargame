import unreal


CLASSES = [
    unreal.MaterialExpressionTextureSampleParameter2D,
    unreal.MaterialExpressionVectorParameter,
    unreal.MaterialExpressionScalarParameter,
    unreal.MaterialExpressionPanner,
    unreal.MaterialExpressionLinearInterpolate,
    unreal.MaterialExpressionMultiply,
    unreal.MaterialExpressionComponentMask,
    unreal.MaterialExpressionOneMinus,
    unreal.MaterialExpressionPower,
    unreal.MaterialExpressionFresnel,
    unreal.MaterialExpressionTime,
]

lines = []
for cls in CLASSES:
    lines.append(f"--- {cls.__name__} ---")
    for item in dir(cls):
        if not item.startswith("_"):
            lower = item.lower()
            if any(k in lower for k in ("parameter", "default", "texture", "coordinate", "input", "const", "speed", "mask", "exponent", "base", "time", "a", "b", "alpha")):
                lines.append(item)

with open("/tmp/stargame_material_node_properties.txt", "w", encoding="utf-8") as handle:
    handle.write("\n".join(lines))
