import math
import os

import unreal


SURFACE_MATERIAL_PATH = "/Game/Materials/Space/M_StarSurface"
CORONA_MATERIAL_PATH = "/Game/Materials/Space/M_StarCorona"
PROMINENCE_MATERIAL_PATH = "/Game/Materials/Space/M_StarProminence"
PROMINENCE_MESH_SOURCE = "/tmp/stargame_star_prominence_bundle.obj"
PROMINENCE_MESH_DIR = "/Game/Meshes/Space"
PROMINENCE_MESH_NAME = "SM_StarProminenceBundle"
PROMINENCE_MESH_PATH = f"{PROMINENCE_MESH_DIR}/{PROMINENCE_MESH_NAME}"
STAR_BP = "/Game/Blueprints/Space/BP_SectorStarAnchor"


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


def multiply(material, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, x, y
    )


def build_surface_material():
    material = create_or_replace_material(SURFACE_MATERIAL_PATH)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", False)

    normal = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionPixelNormalWS, -1050, -260)
    view = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -1050, -80)
    time = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTime, -1050, 100)
    core = vector_param(material, "CoreColor", unreal.LinearColor(1.0, 0.85, 0.40, 1.0), -1050, 280)
    hot = vector_param(material, "HotColor", unreal.LinearColor(1.0, 0.50, 0.10, 1.0), -1050, 460)
    spot = vector_param(material, "SpotColor", unreal.LinearColor(0.45, 0.14, 0.03, 1.0), -1050, 640)
    emission = scalar_param(material, "EmissionStrength", 3.8, -1050, 820)

    code = r"""
float hash31(float3 p)
{
    p = frac(p * 0.3183099 + float3(0.1, 0.2, 0.3));
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float vnoise(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash31(i + float3(0,0,0));
    float n100 = hash31(i + float3(1,0,0));
    float n010 = hash31(i + float3(0,1,0));
    float n110 = hash31(i + float3(1,1,0));
    float n001 = hash31(i + float3(0,0,1));
    float n101 = hash31(i + float3(1,0,1));
    float n011 = hash31(i + float3(0,1,1));
    float n111 = hash31(i + float3(1,1,1));
    float nx00 = lerp(n000, n100, f.x);
    float nx10 = lerp(n010, n110, f.x);
    float nx01 = lerp(n001, n101, f.x);
    float nx11 = lerp(n011, n111, f.x);
    return lerp(lerp(nx00, nx10, f.y), lerp(nx01, nx11, f.y), f.z);
}

float fbm(float3 p)
{
    float v = 0.0;
    float a = 0.5;
    [unroll] for (int i = 0; i < 5; ++i)
    {
        v += a * vnoise(p);
        p *= 2.03;
        a *= 0.5;
    }
    return v;
}

float3 n = normalize(N);
float3 v = normalize(V);
float t = T * 0.12;
float3 grainBase = n * 18.0;
float3 warp = float3(
    fbm(grainBase + float3(t, 7.1, 3.4)),
    fbm(grainBase + float3(13.7, t * 0.9 + 2.3, 11.2)),
    fbm(grainBase + float3(5.9, 19.3, t * 1.1 + 6.7))) - 0.5;

float fine = fbm(grainBase + warp * 1.25 + float3(t * 0.25, -t * 0.12, t * 0.18));
float patch = fbm(n * 3.5 + warp * 0.45 + float3(t * 0.12, t * 0.06, -t * 0.08));
float surface = saturate(lerp(fine, patch, 0.36) + 0.05);

float spotNoise = fbm(n * 6.0 + float3(t * 0.18, -t * 0.11, t * 0.07) + fbm(n * 9.0) * 0.25);
float spotMask = smoothstep(0.76, 0.82, spotNoise);

float3 col = lerp(HotColor, CoreColor, smoothstep(0.28, 0.74, surface));
col = lerp(col, SpotColor, spotMask);

float ndv = saturate(abs(dot(n, v)));
float limb = lerp(0.68, 1.18, pow(ndv, 0.55));
float cellSpark = smoothstep(0.72, 0.95, fine) * 0.35;
return col * (limb + cellSpark) * EmissionStrength;
"""
    plasma = custom_expr(
        material,
        code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        ["N", "V", "T", "CoreColor", "HotColor", "SpotColor", "EmissionStrength"],
        -560,
        80,
        "Procedural stellar plasma",
    )
    for source, output, target_name in [
        (normal, "", "N"),
        (view, "", "V"),
        (time, "", "T"),
        (core, "RGB", "CoreColor"),
        (hot, "RGB", "HotColor"),
        (spot, "RGB", "SpotColor"),
        (emission, "", "EmissionStrength"),
    ]:
        unreal.MaterialEditingLibrary.connect_material_expressions(source, output, plasma, target_name)
    unreal.MaterialEditingLibrary.connect_material_property(plasma, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def build_corona_material():
    material = create_or_replace_material(CORONA_MATERIAL_PATH)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    normal = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionPixelNormalWS, -1050, -220)
    view = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -1050, -40)
    time = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTime, -1050, 140)
    halo = vector_param(material, "HaloColor", unreal.LinearColor(1.0, 0.78, 0.30, 1.0), -1050, 320)
    flare = vector_param(material, "FlareColor", unreal.LinearColor(1.0, 0.38, 0.08, 1.0), -1050, 500)
    emission = scalar_param(material, "HaloEmission", 1.8, -1050, 680)

    code = r"""
float hash21(float2 p)
{
    p = frac(p * float2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return frac(p.x * p.y);
}

float vnoise2(float2 p)
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

float fbmRidged(float2 p)
{
    float rz = 0.0;
    float z = 2.0;
    float2x2 m = float2x2(0.80, 0.60, -0.60, 0.80);
    p *= 0.5;
    [unroll] for (int i = 0; i < 5; ++i)
    {
        rz += abs((vnoise2(p) - 0.5) * 2.0) / z;
        z *= 2.0;
        p = mul(p * 2.0, m);
    }
    return saturate(rz);
}

float3 n = normalize(N);
float3 v = normalize(V);
float rim = 1.0 - saturate(abs(dot(n, v)));
float t = T * 0.04;
float2 p = float2(
    n.x * 3.0 + n.z * 1.4 + t,
    n.y * 3.0 - n.z * 0.9 - t * 0.7);
float flame = pow(fbmRidged(p), 2.1);
float edge = pow(rim, 2.4);
float soft = pow(rim, 5.0) * 1.4;
float intensity = (soft + flame * edge * 2.4) * HaloEmission;
float3 col = lerp(HaloColor, FlareColor, saturate(flame * 1.2 + edge * 0.2));
return col * intensity;
"""
    corona = custom_expr(
        material,
        code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        ["N", "V", "T", "HaloColor", "FlareColor", "HaloEmission"],
        -560,
        80,
        "Procedural stellar corona",
    )
    fresnel = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionFresnel, -560, 420)
    try:
        fresnel.set_editor_property("exponent", 2.4)
    except Exception:
        pass
    opacity_scale = scalar_param(material, "CoronaOpacity", 0.42, -560, 600)
    opacity = multiply(material, -280, 520)

    for source, output, target_name in [
        (normal, "", "N"),
        (view, "", "V"),
        (time, "", "T"),
        (halo, "RGB", "HaloColor"),
        (flare, "RGB", "FlareColor"),
        (emission, "", "HaloEmission"),
    ]:
        unreal.MaterialEditingLibrary.connect_material_expressions(source, output, corona, target_name)
    unreal.MaterialEditingLibrary.connect_material_expressions(fresnel, "", opacity, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(opacity_scale, "", opacity, "B")
    unreal.MaterialEditingLibrary.connect_material_property(corona, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def build_prominence_material():
    material = create_or_replace_material(PROMINENCE_MATERIAL_PATH)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    flare = vector_param(material, "FlareColor", unreal.LinearColor(1.0, 0.38, 0.08, 1.0), -520, -120)
    emission = scalar_param(material, "FlareEmission", 5.5, -520, 80)
    final = multiply(material, -120, -40)
    unreal.MaterialEditingLibrary.connect_material_expressions(flare, "RGB", final, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(emission, "", final, "B")
    unreal.MaterialEditingLibrary.connect_material_property(final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def generate_prominence_obj():
    radius = 900.0
    tube_radius = 10.0
    path_segments = 28
    tube_segments = 6
    strands = 9
    vertices = []
    normals = []
    faces = []

    def add_tube(strand_index, offset_y, offset_z):
        base = len(vertices) + 1
        for i in range(path_segments + 1):
            t = i / path_segments
            angle = math.pi * t
            x = math.cos(angle) * 360.0
            z = radius + math.sin(angle) * 360.0 + offset_z * math.sin(angle)
            y = offset_y * math.sin(angle)
            tangent = (-math.sin(angle), 0.0, math.cos(angle))
            normal = (math.cos(angle), 0.0, math.sin(angle))
            binormal = (0.0, 1.0, 0.0)
            for j in range(tube_segments):
                u = 2.0 * math.pi * j / tube_segments
                nx = math.cos(u) * normal[0] + math.sin(u) * binormal[0]
                ny = math.cos(u) * normal[1] + math.sin(u) * binormal[1]
                nz = math.cos(u) * normal[2] + math.sin(u) * binormal[2]
                vertices.append((x + nx * tube_radius, y + ny * tube_radius, z + nz * tube_radius))
                normals.append((nx, ny, nz))
        for i in range(path_segments):
            for j in range(tube_segments):
                a = base + i * tube_segments + j
                b = base + i * tube_segments + (j + 1) % tube_segments
                c = base + (i + 1) * tube_segments + j
                d = base + (i + 1) * tube_segments + (j + 1) % tube_segments
                faces.append((a, c, d))
                faces.append((a, d, b))

    for s in range(strands):
        phase = 2.0 * math.pi * s / strands
        add_tube(s, math.cos(phase) * 70.0, math.sin(phase) * 55.0)

    with open(PROMINENCE_MESH_SOURCE, "w", encoding="utf-8") as obj:
        obj.write("# Stargame star prominence bundle\n")
        for x, y, z in vertices:
            obj.write(f"v {x:.5f} {y:.5f} {z:.5f}\n")
        for nx, ny, nz in normals:
            obj.write(f"vn {nx:.5f} {ny:.5f} {nz:.5f}\n")
        obj.write("s off\n")
        for a, b, c in faces:
            obj.write(f"f {a}//{a} {b}//{b} {c}//{c}\n")


def import_prominence_mesh():
    generate_prominence_obj()
    ensure_directory(PROMINENCE_MESH_DIR)
    if unreal.EditorAssetLibrary.does_asset_exist(PROMINENCE_MESH_PATH):
        unreal.EditorAssetLibrary.delete_asset(PROMINENCE_MESH_PATH)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", PROMINENCE_MESH_SOURCE)
    task.set_editor_property("destination_path", PROMINENCE_MESH_DIR)
    task.set_editor_property("destination_name", PROMINENCE_MESH_NAME)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.EditorAssetLibrary.load_asset(PROMINENCE_MESH_PATH)
    if not mesh:
        raise RuntimeError(f"Failed to import {PROMINENCE_MESH_PATH}")
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return mesh


def add_or_update_prominence_components(mesh, material):
    # Add via bridge is more reliable for named SCS components, but direct template
    # configuration happens here once the components exist.
    bp = unreal.EditorAssetLibrary.load_asset(STAR_BP)
    if not bp:
        raise RuntimeError(f"Missing {STAR_BP}")

    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    components = {}
    for handle in handles:
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = library.get_object(data)
        if obj:
            components[str(library.get_variable_name(data))] = obj

    for index in range(10):
        name = f"ProminenceLoop{index:02d}"
        component = components.get(name)
        if not component:
            continue
        component.set_editor_property("static_mesh", mesh)
        component.set_material(0, material)
        yaw = index * 36.0
        pitch = [-18.0, 12.0, 32.0, -34.0, 6.0, 24.0, -8.0, 41.0, -26.0, 15.0][index]
        roll = [0.0, 42.0, -28.0, 70.0, -58.0, 18.0, -84.0, 35.0, 11.0, -45.0][index]
        scale = [1.0, 0.72, 0.88, 0.56, 0.68, 0.92, 0.48, 0.62, 0.76, 0.54][index]
        component.set_editor_property("relative_rotation", unreal.Rotator(pitch, yaw, roll))
        component.set_editor_property("relative_scale3d", unreal.Vector(scale, scale, scale))
        component.set_editor_property("cast_shadow", False)
        component.set_editor_property("hidden_in_game", False)
        component.set_editor_property("visible", True)

    # Make the body/corona proportions closer to the Godot version.
    for name, scale in [("StarBody", 18.0), ("CoronaShell", 42.0)]:
        component = components.get(name)
        if component:
            component.set_editor_property("relative_scale3d", unreal.Vector(scale, scale, scale))

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)


def main():
    surface = build_surface_material()
    corona = build_corona_material()
    prominence = build_prominence_material()
    mesh = import_prominence_mesh()
    add_or_update_prominence_components(mesh, prominence)
    print("Refined sector star surface, corona, and prominence visuals.")


if __name__ == "__main__":
    main()
