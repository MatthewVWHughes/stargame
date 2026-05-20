import math

import unreal


STAR_BP = "/Game/Blueprints/Space/BP_SectorStarAnchor"
PREVIEW_MAP = "/Game/Maps/StellarClassPreview"
MESH_DIR = "/Game/Meshes/Space"
PHOTOSPHERE_MESH_NAME = "SM_StarPhotosphereDisplaced"
PHOTOSPHERE_MESH = f"{MESH_DIR}/{PHOTOSPHERE_MESH_NAME}"
PHOTOSPHERE_OBJ = "/tmp/stargame_star_photosphere_displaced.obj"
SURFACE_MATERIAL = "/Game/Materials/Space/M_StarSurface"
ATMOSPHERE_MATERIAL = "/Game/Materials/Space/M_StarAtmosphere"


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def create_or_replace_material(path):
    package_path, asset_name = path.rsplit("/", 1)
    ensure_directory(package_path)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name,
        package_path,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not material:
        raise RuntimeError(f"Failed to create material {path}")
    return material


def vector_param(material, name, value, x, y):
    expr = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, x, y
    )
    expr.set_editor_property("parameter_name", name)
    expr.set_editor_property("default_value", value)
    return expr


def scalar_param(material, name, value, x, y):
    expr = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, x, y
    )
    expr.set_editor_property("parameter_name", name)
    expr.set_editor_property("default_value", value)
    return expr


def custom_input(name):
    item = unreal.CustomInput()
    item.set_editor_property("input_name", name)
    return item


def custom_expr(material, code, output_type, inputs, x, y, description):
    expr = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionCustom, x, y
    )
    expr.set_editor_property("code", code)
    expr.set_editor_property("output_type", output_type)
    expr.set_editor_property("description", description)
    expr.set_editor_property("inputs", [custom_input(name) for name in inputs])
    return expr


def generate_photosphere_obj():
    lat_segments = 64
    lon_segments = 128
    radius = 50.0
    vertices = []
    normals = []
    uvs = []
    faces = []

    def surface_noise(x, y, z):
        a = math.sin(x * 8.7 + y * 2.1) * math.sin(z * 6.3 - x * 1.8)
        b = math.sin((x + y + z) * 13.0) * 0.45
        c = math.sin((x * 17.0 - z * 9.0 + y * 4.0)) * 0.24
        return a + b + c

    for lat in range(lat_segments + 1):
        v = lat / lat_segments
        theta = math.pi * v
        sin_theta = math.sin(theta)
        cos_theta = math.cos(theta)
        for lon in range(lon_segments + 1):
            u = lon / lon_segments
            phi = 2.0 * math.pi * u
            x = math.cos(phi) * sin_theta
            y = math.sin(phi) * sin_theta
            z = cos_theta
            n = surface_noise(x, y, z)
            displacement = 1.0 + 0.035 * n
            vertices.append((x * radius * displacement, y * radius * displacement, z * radius * displacement))
            normals.append((x, y, z))
            uvs.append((u, 1.0 - v))

    row = lon_segments + 1
    for lat in range(lat_segments):
        for lon in range(lon_segments):
            a = lat * row + lon + 1
            b = a + 1
            c = a + row
            d = c + 1
            if lat != 0:
                faces.append((a, c, b))
            if lat != lat_segments - 1:
                faces.append((b, c, d))

    with open(PHOTOSPHERE_OBJ, "w", encoding="utf-8") as obj:
        obj.write("# Stargame displaced photosphere mesh\n")
        for x, y, z in vertices:
            obj.write(f"v {x:.5f} {y:.5f} {z:.5f}\n")
        for u, v in uvs:
            obj.write(f"vt {u:.5f} {v:.5f}\n")
        for nx, ny, nz in normals:
            obj.write(f"vn {nx:.5f} {ny:.5f} {nz:.5f}\n")
        obj.write("s 1\n")
        for a, b, c in faces:
            obj.write(f"f {a}/{a}/{a} {b}/{b}/{b} {c}/{c}/{c}\n")


def import_photosphere_mesh():
    generate_photosphere_obj()
    ensure_directory(MESH_DIR)
    if unreal.EditorAssetLibrary.does_asset_exist(PHOTOSPHERE_MESH):
        unreal.EditorAssetLibrary.delete_asset(PHOTOSPHERE_MESH)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", PHOTOSPHERE_OBJ)
    task.set_editor_property("destination_path", MESH_DIR)
    task.set_editor_property("destination_name", PHOTOSPHERE_MESH_NAME)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.EditorAssetLibrary.load_asset(PHOTOSPHERE_MESH)
    if not mesh:
        raise RuntimeError(f"Failed to import {PHOTOSPHERE_MESH}")
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return mesh


def build_surface_material():
    material = create_or_replace_material(SURFACE_MATERIAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    uv = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1120, -260)
    normal = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionPixelNormalWS, -1120, -60)
    view = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -1120, 140)
    time = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTime, -1120, 340)
    core = vector_param(material, "CoreColor", unreal.LinearColor(1.0, 0.78, 0.28, 1.0), -1120, 540)
    hot = vector_param(material, "HotColor", unreal.LinearColor(1.0, 0.42, 0.08, 1.0), -1120, 740)
    spot = vector_param(material, "SpotColor", unreal.LinearColor(0.30, 0.06, 0.01, 1.0), -1120, 940)
    emission = scalar_param(material, "EmissionStrength", 5.4, -1120, 1140)

    code = r"""
float hash21(float2 p)
{
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float noise2(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + float2(1, 0));
    float c = hash21(i + float2(0, 1));
    float d = hash21(i + float2(1, 1));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float fbm(float2 p)
{
    float v = 0.0;
    float a = 0.5;
    float2x2 rot = float2x2(0.8, 0.6, -0.6, 0.8);
    [unroll] for (int i = 0; i < 6; ++i)
    {
        v += a * noise2(p);
        p = mul(p * 2.03 + 7.1, rot);
        a *= 0.5;
    }
    return v;
}

float2 p = UV;
float t = T * 0.035;
float2 flow1 = p * float2(18.0, 9.0) + float2(t * 2.1, -t * 0.8);
float2 flow2 = p * float2(42.0, 19.0) + float2(-t * 1.1, t * 2.4);
float broad = fbm(flow1);
float fine = fbm(flow2 + broad * 1.8);
float veins = smoothstep(0.18, 0.78, abs(fine - broad) * 2.2);
float heat = saturate(broad * 0.65 + fine * 0.45 + veins * 0.50);

float spots = smoothstep(0.82, 0.91, fbm(p * float2(8.0, 4.0) + float2(t * 0.6, -t * 0.3)));
float3 n = normalize(N);
float3 v = normalize(V);
float ndv = saturate(abs(dot(n, -v)));
float limb = lerp(0.42, 1.25, pow(ndv, 0.34));
float3 col = lerp(CoreColor, HotColor, heat);
col = lerp(col, SpotColor, spots * smoothstep(0.18, 0.62, broad));
return col * (limb + veins * 0.35) * EmissionStrength;
"""
    expr = custom_expr(
        material,
        code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        ["UV", "N", "V", "T", "CoreColor", "HotColor", "SpotColor", "EmissionStrength"],
        -650,
        120,
        "Reset retry photosphere surface",
    )
    for source, output, target in [
        (uv, "", "UV"),
        (normal, "", "N"),
        (view, "", "V"),
        (time, "", "T"),
        (core, "RGB", "CoreColor"),
        (hot, "RGB", "HotColor"),
        (spot, "RGB", "SpotColor"),
        (emission, "", "EmissionStrength"),
    ]:
        unreal.MaterialEditingLibrary.connect_material_expressions(source, output, expr, target)
    unreal.MaterialEditingLibrary.connect_material_property(expr, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def build_atmosphere_material():
    material = create_or_replace_material(ATMOSPHERE_MATERIAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    try:
        material.set_editor_property("disable_depth_test", False)
    except Exception:
        pass

    normal = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionPixelNormalWS, -920, -120)
    view = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -920, 80)
    time = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTime, -920, 280)
    halo = vector_param(material, "HaloColor", unreal.LinearColor(1.0, 0.58, 0.14, 1.0), -920, 480)
    emission = scalar_param(material, "HaloEmission", 1.45, -920, 680)
    code = r"""
float hash21(float2 p)
{
    p = frac(p * float2(234.5, 987.1));
    p += dot(p, p + 21.7);
    return frac(p.x * p.y);
}
float noise2(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + float2(1,0));
    float c = hash21(i + float2(0,1));
    float d = hash21(i + float2(1,1));
    return lerp(lerp(a,b,f.x), lerp(c,d,f.x), f.y);
}
float3 n = normalize(N);
float3 v = normalize(V);
float rim = 1.0 - saturate(abs(dot(n, -v)));
float t = T * 0.04;
float2 p = float2(atan2(n.y, n.x) * 2.2 + t, n.z * 4.0 - t * 0.7);
float turbulence = noise2(p * 3.0) * 0.45 + noise2(p * 8.0) * 0.25;
float glow = pow(rim, 2.0) * (0.55 + turbulence);
return HaloColor * glow * HaloEmission;
"""
    expr = custom_expr(
        material,
        code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        ["N", "V", "T", "HaloColor", "HaloEmission"],
        -480,
        120,
        "Integrated stellar atmosphere shell",
    )
    opacity = scalar_param(material, "AtmosphereOpacity", 0.22, -480, 460)
    for source, output, target in [
        (normal, "", "N"),
        (view, "", "V"),
        (time, "", "T"),
        (halo, "RGB", "HaloColor"),
        (emission, "", "HaloEmission"),
    ]:
        unreal.MaterialEditingLibrary.connect_material_expressions(source, output, expr, target)
    unreal.MaterialEditingLibrary.connect_material_property(expr, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def component_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        name = str(library.get_variable_name(data))
        obj = library.get_object(data)
        if obj:
            yield name, obj


def generated_class(blueprint):
    cls = None
    try:
        cls = blueprint.get_editor_property("generated_class")
    except Exception:
        cls = None
    if not cls:
        cls = unreal.load_class(None, f"{blueprint.get_path_name()}_C")
    if not cls:
        raise RuntimeError(f"Could not resolve generated class for {blueprint.get_path_name()}")
    return cls


def configure_star_blueprint(mesh, surface, atmosphere):
    bp = unreal.EditorAssetLibrary.load_asset(STAR_BP)
    if not bp:
        raise RuntimeError(f"Missing {STAR_BP}")

    cdo = unreal.get_default_object(generated_class(bp))
    cdo.set_editor_property("star_surface_material", surface)
    cdo.set_editor_property("star_corona_material", atmosphere)
    try:
        cdo.set_editor_property("star_halo_material", None)
        cdo.set_editor_property("star_prominence_material", None)
    except Exception:
        pass

    for name, component in component_templates(bp):
        if name == "StarBody":
            component.set_editor_property("static_mesh", mesh)
            component.set_material(0, surface)
            component.set_editor_property("relative_scale3d", unreal.Vector(18.0, 18.0, 18.0))
            component.set_editor_property("visible", True)
            component.set_editor_property("hidden_in_game", False)
            component.set_editor_property("cast_shadow", False)
        elif name == "CoronaShell":
            component.set_editor_property("static_mesh", mesh)
            component.set_material(0, atmosphere)
            component.set_editor_property("relative_scale3d", unreal.Vector(23.5, 23.5, 23.5))
            component.set_editor_property("visible", True)
            component.set_editor_property("hidden_in_game", False)
            component.set_editor_property("cast_shadow", False)
        elif name in {"StarHaloBillboard", "StarHaloBillboardSCS"}:
            component.set_editor_property("visible", False)
            component.set_editor_property("hidden_in_game", True)
        elif name.startswith("ProminenceLoop"):
            component.set_editor_property("visible", False)
            component.set_editor_property("hidden_in_game", True)

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)


def main():
    mesh = import_photosphere_mesh()
    surface = build_surface_material()
    atmosphere = build_atmosphere_material()
    configure_star_blueprint(mesh, surface, atmosphere)
    print("Reset/retry sector star: disabled plane/prominence layers, rebuilt displaced photosphere and integrated atmosphere shell.")


if __name__ == "__main__":
    main()
