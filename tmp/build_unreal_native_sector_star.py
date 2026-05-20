import math

import unreal


SURFACE_MATERIAL_PATH = "/Game/Materials/Space/M_StarSurface"
LEGACY_CORONA_MATERIAL_PATH = "/Game/Materials/Space/M_StarCorona"
HALO_MATERIAL_PATH = "/Game/Materials/Space/M_StarHaloBillboard"
PROMINENCE_MATERIAL_PATH = "/Game/Materials/Space/M_StarProminence"
PROMINENCE_MESH_SOURCE = "/tmp/stargame_star_prominence_bundle.obj"
PROMINENCE_MESH_DIR = "/Game/Meshes/Space"
PROMINENCE_MESH_NAME = "SM_StarProminenceBundle"
PROMINENCE_MESH_PATH = f"{PROMINENCE_MESH_DIR}/{PROMINENCE_MESH_NAME}"
STAR_BP = "/Game/Blueprints/Space/BP_SectorStarAnchor"
SYSTEM_PRESENTATION_BP = "/Game/Blueprints/Space/BP_SystemSpacePresentation"
PREVIEW_MAP_PATH = "/Game/Maps/StellarClassPreview"


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


def texcoord(material, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, x, y
    )


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


def set_depth_test_disabled(material):
    try:
        material.set_editor_property("disable_depth_test", True)
    except Exception:
        pass


def build_surface_material():
    material = create_or_replace_material(SURFACE_MATERIAL_PATH)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", False)

    pixel_normal = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionPixelNormalWS, -1100, -300)
    vertex_normal = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVertexNormalWS, -1100, -100)
    camera = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -1100, 100)
    time = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTime, -1100, 300)
    core = vector_param(material, "CoreColor", unreal.LinearColor(1.0, 0.85, 0.40, 1.0), -1100, 500)
    hot = vector_param(material, "HotColor", unreal.LinearColor(1.0, 0.50, 0.10, 1.0), -1100, 700)
    spot = vector_param(material, "SpotColor", unreal.LinearColor(0.45, 0.14, 0.03, 1.0), -1100, 900)
    emission = scalar_param(material, "EmissionStrength", 3.8, -1100, 1100)

    surface_code = r"""
float hash31(float3 p)
{
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

float vnoise(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash31(i);
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
    [unroll] for (int i = 0; i < 6; ++i)
    {
        v += a * vnoise(p);
        p = p * 2.06 + float3(11.7, 4.2, 8.3);
        a *= 0.5;
    }
    return v;
}

float3 n = normalize(N);
float3 v = normalize(V);
float t = T * 0.10;
float3 warp = float3(
    fbm(n * 5.0 + float3(t, 3.1, 8.4)),
    fbm(n * 5.0 + float3(6.7, t * 1.2, 2.0)),
    fbm(n * 5.0 + float3(1.3, 9.9, t * 0.8))) - 0.5;

float cellular = fbm(n * 28.0 + warp * 4.0 + float3(t * 1.4, -t * 0.7, t * 0.5));
float broad = fbm(n * 6.0 + warp * 1.2 + float3(-t * 0.5, t * 0.4, t * 0.2));
float filaments = smoothstep(0.46, 0.80, abs(cellular - broad) * 1.8);
float heat = saturate(cellular * 0.55 + broad * 0.65 + filaments * 0.35);

float spotNoise = fbm(n * 9.0 + warp * 1.8 + float3(t * 0.35, -t * 0.2, t * 0.15));
float spotMask = smoothstep(0.79, 0.88, spotNoise) * smoothstep(0.18, 0.42, broad);

float ndv = saturate(abs(dot(n, -v)));
float limbDarken = lerp(0.48, 1.15, pow(ndv, 0.42));
float spark = smoothstep(0.78, 0.96, cellular) * 0.55;
float3 plasma = lerp(CoreColor, HotColor, heat);
plasma = lerp(plasma, SpotColor, spotMask);
return plasma * (limbDarken + spark) * EmissionStrength;
"""
    surface = custom_expr(
        material,
        surface_code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        ["N", "V", "T", "CoreColor", "HotColor", "SpotColor", "EmissionStrength"],
        -620,
        120,
        "Unreal-native stellar photosphere",
    )
    for source, output, target_name in [
        (pixel_normal, "", "N"),
        (camera, "", "V"),
        (time, "", "T"),
        (core, "RGB", "CoreColor"),
        (hot, "RGB", "HotColor"),
        (spot, "RGB", "SpotColor"),
        (emission, "", "EmissionStrength"),
    ]:
        unreal.MaterialEditingLibrary.connect_material_expressions(source, output, surface, target_name)

    displacement_code = r"""
float3 n = normalize(N);
float h = sin((n.x * 15.0 + T * 0.9)) * sin((n.y * 11.0 - T * 0.6));
h += sin((n.z * 17.0 + n.x * 3.0 + T * 0.4)) * 0.5;
return n * h * 5.5;
"""
    displacement = custom_expr(
        material,
        displacement_code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        ["N", "T"],
        -620,
        520,
        "Subtle stellar surface displacement",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(vertex_normal, "", displacement, "N")
    unreal.MaterialEditingLibrary.connect_material_expressions(time, "", displacement, "T")
    unreal.MaterialEditingLibrary.connect_material_property(surface, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(displacement, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def build_halo_material():
    material = create_or_replace_material(HALO_MATERIAL_PATH)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    set_depth_test_disabled(material)

    uv = texcoord(material, -1100, -160)
    time = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTime, -1100, 40)
    halo = vector_param(material, "HaloColor", unreal.LinearColor(1.0, 0.78, 0.30, 1.0), -1100, 240)
    flare = vector_param(material, "FlareColor", unreal.LinearColor(1.0, 0.38, 0.08, 1.0), -1100, 440)
    halo_emission = scalar_param(material, "HaloEmission", 1.8, -1100, 640)
    flare_emission = scalar_param(material, "FlareEmission", 5.5, -1100, 840)

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
    float2x2 rot = float2x2(0.84, 0.54, -0.54, 0.84);
    [unroll] for (int i = 0; i < 6; ++i)
    {
        v += a * noise2(p);
        p = mul(p * 2.03 + 13.7, rot);
        a *= 0.5;
    }
    return v;
}

float2 p = UV * 2.0 - 1.0;
float r = length(p);
float a = atan2(p.y, p.x);
float t = T * 0.08;
float twist = sin(a * 7.0 + t * 5.0) * 0.08 + sin(a * 13.0 - t * 3.0) * 0.035;
float flame = fbm(float2(a * 2.0 + twist + t, r * 7.5 - t * 1.6));
float rays = pow(saturate(flame), 2.8);
float inner = smoothstep(0.23, 0.36, r);
float mid = exp(-r * 2.2);
float outer = 1.0 - smoothstep(0.72, 1.0, r);
float ring = smoothstep(0.32, 0.42, r) * (1.0 - smoothstep(0.58, 0.92, r));
float alpha = saturate(inner * outer * (mid * HaloEmission + rays * ring * FlareEmission * 0.38));
float3 col = lerp(HaloColor, FlareColor, saturate(rays * 1.4 + ring * 0.35));
return float4(col * alpha, alpha);
"""
    halo_expr = custom_expr(
        material,
        code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT4,
        ["UV", "T", "HaloColor", "FlareColor", "HaloEmission", "FlareEmission"],
        -620,
        120,
        "Radial camera-facing stellar halo",
    )
    for source, output, target_name in [
        (uv, "", "UV"),
        (time, "", "T"),
        (halo, "RGB", "HaloColor"),
        (flare, "RGB", "FlareColor"),
        (halo_emission, "", "HaloEmission"),
        (flare_emission, "", "FlareEmission"),
    ]:
        unreal.MaterialEditingLibrary.connect_material_expressions(source, output, halo_expr, target_name)
    component_mask_rgb = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionComponentMask, -220, 60)
    component_mask_rgb.set_editor_property("r", True)
    component_mask_rgb.set_editor_property("g", True)
    component_mask_rgb.set_editor_property("b", True)
    component_mask_a = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionComponentMask, -220, 260)
    component_mask_a.set_editor_property("a", True)
    unreal.MaterialEditingLibrary.connect_material_expressions(halo_expr, "", component_mask_rgb, "Input")
    unreal.MaterialEditingLibrary.connect_material_expressions(halo_expr, "", component_mask_a, "Input")
    unreal.MaterialEditingLibrary.connect_material_property(component_mask_rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(component_mask_a, "", unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def build_legacy_corona_material():
    material = create_or_replace_material(LEGACY_CORONA_MATERIAL_PATH)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    color = vector_param(material, "HaloColor", unreal.LinearColor(1.0, 0.78, 0.30, 1.0), -400, -100)
    zero = scalar_param(material, "HaloEmission", 0.0, -400, 100)
    final = multiply(material, -80, -40)
    unreal.MaterialEditingLibrary.connect_material_expressions(color, "RGB", final, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(zero, "", final, "B")
    unreal.MaterialEditingLibrary.connect_material_property(final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def build_prominence_material():
    material = create_or_replace_material(PROMINENCE_MATERIAL_PATH)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    set_depth_test_disabled(material)

    uv = texcoord(material, -920, -120)
    time = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTime, -920, 80)
    flare = vector_param(material, "FlareColor", unreal.LinearColor(1.0, 0.38, 0.08, 1.0), -920, 280)
    emission = scalar_param(material, "FlareEmission", 5.5, -920, 480)
    code = r"""
float path = saturate(UV.x);
float around = abs(UV.y - 0.5) * 2.0;
float endpoint = smoothstep(0.02, 0.14, path) * (1.0 - smoothstep(0.86, 0.99, path));
float strand = 1.0 - smoothstep(0.05, 0.92, around);
float pulse = 0.72 + 0.28 * sin(T * 3.2 + path * 11.0 + UV.y * 17.0);
float core = pow(strand, 2.2) * endpoint * pulse;
float edgeHot = smoothstep(0.35, 0.95, path) * (1.0 - smoothstep(0.72, 1.0, path));
float alpha = saturate(core * (0.65 + edgeHot * 0.85));
return float4(FlareColor * alpha * FlareEmission, alpha);
"""
    expr = custom_expr(
        material,
        code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT4,
        ["UV", "T", "FlareColor", "FlareEmission"],
        -500,
        80,
        "Animated stellar prominence strands",
    )
    for source, output, target_name in [
        (uv, "", "UV"),
        (time, "", "T"),
        (flare, "RGB", "FlareColor"),
        (emission, "", "FlareEmission"),
    ]:
        unreal.MaterialEditingLibrary.connect_material_expressions(source, output, expr, target_name)
    rgb = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionComponentMask, -120, 20)
    rgb.set_editor_property("r", True)
    rgb.set_editor_property("g", True)
    rgb.set_editor_property("b", True)
    alpha = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionComponentMask, -120, 220)
    alpha.set_editor_property("a", True)
    unreal.MaterialEditingLibrary.connect_material_expressions(expr, "", rgb, "Input")
    unreal.MaterialEditingLibrary.connect_material_expressions(expr, "", alpha, "Input")
    unreal.MaterialEditingLibrary.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def generate_prominence_obj():
    radius = 930.0
    tube_radius = 9.0
    path_segments = 32
    tube_segments = 6
    strands = 14
    vertices = []
    normals = []
    uvs = []
    faces = []

    def add_tube(offset_y, offset_z, height_scale, width_scale):
        base = len(vertices) + 1
        for i in range(path_segments + 1):
            path_t = i / path_segments
            angle = math.pi * path_t
            x = math.cos(angle) * 420.0 * width_scale
            z = radius + math.sin(angle) * 470.0 * height_scale + offset_z * math.sin(angle)
            y = offset_y * math.sin(angle)
            normal = (math.cos(angle), 0.0, math.sin(angle))
            binormal = (0.0, 1.0, 0.0)
            for j in range(tube_segments):
                tube_t = j / tube_segments
                u = 2.0 * math.pi * tube_t
                nx = math.cos(u) * normal[0] + math.sin(u) * binormal[0]
                ny = math.cos(u) * normal[1] + math.sin(u) * binormal[1]
                nz = math.cos(u) * normal[2] + math.sin(u) * binormal[2]
                vertices.append((x + nx * tube_radius, y + ny * tube_radius, z + nz * tube_radius))
                normals.append((nx, ny, nz))
                uvs.append((path_t, tube_t))
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
        add_tube(
            math.cos(phase) * 95.0,
            math.sin(phase) * 70.0,
            0.72 + 0.28 * ((s % 5) / 4.0),
            0.80 + 0.22 * ((s % 7) / 6.0),
        )

    with open(PROMINENCE_MESH_SOURCE, "w", encoding="utf-8") as obj:
        obj.write("# Stargame Unreal-native star prominence bundle\n")
        for x, y, z in vertices:
            obj.write(f"v {x:.5f} {y:.5f} {z:.5f}\n")
        for u, v in uvs:
            obj.write(f"vt {u:.5f} {v:.5f}\n")
        for nx, ny, nz in normals:
            obj.write(f"vn {nx:.5f} {ny:.5f} {nz:.5f}\n")
        obj.write("s off\n")
        for a, b, c in faces:
            obj.write(f"f {a}/{a}/{a} {b}/{b}/{b} {c}/{c}/{c}\n")


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


def component_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        name = str(library.get_variable_name(data))
        obj = library.get_object(data)
        if obj:
            yield name, obj


def configure_star_blueprint(surface, legacy_corona, halo, prominence, mesh):
    bp = unreal.EditorAssetLibrary.load_asset(STAR_BP)
    if not bp:
        raise RuntimeError(f"Missing {STAR_BP}")
    cdo = unreal.get_default_object(generated_class(bp))
    for prop_name, value in [
        ("star_surface_material", surface),
        ("star_corona_material", legacy_corona),
        ("star_halo_material", halo),
        ("star_prominence_material", prominence),
    ]:
        try:
            cdo.set_editor_property(prop_name, value)
        except Exception:
            # The running editor may still have the pre-restart C++ class loaded.
            # Blueprint component templates below still make the visual pass usable.
            pass

    for name, component in component_templates(bp):
        if name == "StarBody":
            component.set_material(0, surface)
            component.set_editor_property("relative_scale3d", unreal.Vector(18.0, 18.0, 18.0))
            component.set_editor_property("cast_shadow", False)
        elif name == "CoronaShell":
            component.set_material(0, legacy_corona)
            component.set_editor_property("visible", False)
            component.set_editor_property("hidden_in_game", True)
            component.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
        elif name in {"StarHaloBillboard", "StarHaloBillboardSCS"}:
            plane = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane.Plane")
            if plane:
                component.set_editor_property("static_mesh", plane)
            component.set_material(0, halo)
            component.set_editor_property("visible", True)
            component.set_editor_property("hidden_in_game", False)
            component.set_editor_property("relative_scale3d", unreal.Vector(115.0, 115.0, 115.0))
            component.set_editor_property("relative_rotation", unreal.Rotator(90.0, 0.0, 0.0))
            component.set_editor_property("cast_shadow", False)
        elif name.startswith("ProminenceLoop"):
            component.set_editor_property("static_mesh", mesh)
            component.set_material(0, prominence)
            component.set_editor_property("cast_shadow", False)
            component.set_editor_property("hidden_in_game", False)
            component.set_editor_property("visible", True)

    prominence_layout = [
        (-16.0, 0.0, 0.0, 1.00), (12.0, 21.0, 42.0, 0.80), (30.0, 43.0, -28.0, 0.92),
        (-34.0, 64.0, 70.0, 0.68), (7.0, 86.0, -58.0, 0.76), (23.0, 108.0, 18.0, 1.04),
        (-9.0, 129.0, -84.0, 0.62), (39.0, 151.0, 35.0, 0.74), (-24.0, 172.0, 11.0, 0.88),
        (15.0, 194.0, -45.0, 0.66), (-42.0, 216.0, 25.0, 0.72), (28.0, 237.0, -12.0, 0.98),
        (-18.0, 259.0, 61.0, 0.70), (36.0, 280.0, -73.0, 0.84), (5.0, 302.0, 31.0, 0.58),
        (-31.0, 323.0, -24.0, 0.78), (19.0, 345.0, 82.0, 0.64), (44.0, 12.0, -51.0, 0.56),
    ]
    components = dict(component_templates(bp))
    for index, (pitch, yaw, roll, scale) in enumerate(prominence_layout):
        component = components.get(f"ProminenceLoop{index:02d}")
        if not component:
            continue
        component.set_editor_property("relative_rotation", unreal.Rotator(pitch, yaw, roll))
        component.set_editor_property("relative_scale3d", unreal.Vector(scale, scale, scale))

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return bp


def configure_system_presentation(star_bp):
    bp = unreal.EditorAssetLibrary.load_asset(SYSTEM_PRESENTATION_BP)
    if not bp:
        return
    for name, component in component_templates(bp):
        if name == "SectorStarAnchor":
            component.set_editor_property("child_actor_class", generated_class(star_bp))
        elif name == "DistantStarDisc":
            component.set_editor_property("visible", False)
            component.set_editor_property("hidden_in_game", True)
        elif name == "StarGlow":
            component.set_editor_property("intensity", 0.0)
            component.set_editor_property("visible", False)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)


def spawn_star(star_class, label, location, stellar_index, preview_controls=False):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        star_class,
        unreal.Vector(location[0], location[1], location[2]),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    actor.set_actor_label(label)
    try:
        actor.set_stellar_class_by_index(stellar_index)
    except Exception:
        try:
            actor.set_editor_property("stellar_class", stellar_index)
        except Exception:
            pass
    actor.set_editor_property("enable_preview_controls", preview_controls)
    actor.set_editor_property("use_preview_camera", preview_controls)
    return actor


def configure_preview_map(star_bp):
    if unreal.EditorAssetLibrary.does_asset_exist(PREVIEW_MAP_PATH):
        if hasattr(unreal.EditorLoadingAndSavingUtils, "load_map"):
            unreal.EditorLoadingAndSavingUtils.load_map(PREVIEW_MAP_PATH)
        else:
            unreal.EditorLevelLibrary.load_level(PREVIEW_MAP_PATH)
    else:
        unreal.EditorLevelLibrary.new_level(PREVIEW_MAP_PATH)

    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        unreal.EditorLevelLibrary.destroy_actor(actor)

    star_class = generated_class(star_bp)
    spawn_star(star_class, "Focused_Stellar_Preview", (0.0, 0.0, 0.0), 4, True)
    class_labels = ["O", "B", "A", "F", "G", "K", "M", "L"]
    for index, class_label in enumerate(class_labels):
        x = -12250.0 + index * 3500.0
        actor = spawn_star(star_class, f"Class_{class_label}_Preview", (x, 9800.0, -850.0), index, False)
        actor.set_actor_scale3d(unreal.Vector(0.35, 0.35, 0.35))

    post = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PostProcessVolume,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    post.set_actor_label("StarPreview_PostProcess")
    pp = post.get_component_by_class(unreal.PostProcessComponent)
    if pp:
        pp.set_editor_property("unbound", True)
        settings = pp.get_editor_property("settings")
        for attr, value in [
            ("override_bloom_intensity", True),
            ("bloom_intensity", 1.35),
            ("override_bloom_threshold", True),
            ("bloom_threshold", 0.28),
            ("override_auto_exposure_method", True),
            ("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL),
            ("override_auto_exposure_bias", True),
            ("auto_exposure_bias", -0.35),
        ]:
            try:
                settings.set_editor_property(attr, value)
            except Exception:
                pass
        pp.set_editor_property("settings", settings)

    skylight = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyLight,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    skylight.set_actor_label("StarPreview_MinimalSkyLight")
    sky_component = skylight.get_component_by_class(unreal.SkyLightComponent)
    if sky_component:
        sky_component.set_editor_property("intensity", 0.0)

    dark_ship = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(3300.0, -460.0, -50.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    dark_ship.set_actor_label("Depth_Silhouette_Check")
    mesh_component = dark_ship.get_component_by_class(unreal.StaticMeshComponent)
    cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube.Cube")
    if mesh_component and cube:
        mesh_component.set_editor_property("static_mesh", cube)
        mesh_component.set_editor_property("relative_scale3d", unreal.Vector(3.0, 0.16, 0.48))
        mesh_component.set_editor_property("cast_shadow", False)

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = editor_subsystem.get_editor_world() if editor_subsystem else None
    if world:
        settings = world.get_world_settings()
        preview_game_mode = unreal.load_class(None, "/Script/Engine.GameModeBase")
        if preview_game_mode:
            settings.set_editor_property("default_game_mode", preview_game_mode)

    unreal.EditorLevelLibrary.save_current_level()


def main():
    surface = build_surface_material()
    legacy_corona = build_legacy_corona_material()
    halo = build_halo_material()
    prominence = build_prominence_material()
    mesh = import_prominence_mesh()
    star_bp = configure_star_blueprint(surface, legacy_corona, halo, prominence, mesh)
    configure_system_presentation(star_bp)
    configure_preview_map(star_bp)
    print("Built Unreal-native sector star stack: photosphere, billboard halo, prominence mesh layer, preview map.")


if __name__ == "__main__":
    main()
