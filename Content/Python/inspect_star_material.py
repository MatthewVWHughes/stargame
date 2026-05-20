import unreal


MATERIAL_PATHS = [
    "/Game/Materials/Space/M_StarSurface",
    "/Game/Materials/Space/M_StarSurfaceDynamic",
    "/Game/Materials/Space/M_StarCorona",
]

REPORT_LINES = []

def report(line):
    REPORT_LINES.append(line)
    unreal.log(line)

for path in MATERIAL_PATHS:
    asset = unreal.EditorAssetLibrary.load_asset(path)
    report(f"ASSET {path}: {asset}")
    if asset is None:
        continue

    for label, getter in (
        ("scalar", unreal.MaterialEditingLibrary.get_scalar_parameter_names),
        ("vector", unreal.MaterialEditingLibrary.get_vector_parameter_names),
        ("texture", unreal.MaterialEditingLibrary.get_texture_parameter_names),
    ):
        try:
            names = getter(asset)
        except Exception as exc:
            report(f"  {label} params unavailable: {exc}")
            continue

        report(f"  {label} params ({len(names)}):")
        for info in names:
            report(f"    {info}")

with open("/tmp/stargame_star_material_report.txt", "w", encoding="utf-8") as handle:
    handle.write("\n".join(REPORT_LINES))
