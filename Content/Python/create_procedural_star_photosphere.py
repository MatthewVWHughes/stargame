import unreal


MATERIAL_PACKAGE_PATH = "/Game/Materials/Space"
MATERIAL_NAME = "M_StarPhotosphereProcedural"
MATERIAL_PATH = f"{MATERIAL_PACKAGE_PATH}/{MATERIAL_NAME}"


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


def connect(src, output, dst, input_name):
    if not unreal.MaterialEditingLibrary.connect_material_expressions(src, output, dst, input_name):
        raise RuntimeError(f"Failed to connect {src.get_name()}:{output} -> {dst.get_name()}:{input_name}")


material = create_or_replace_material(MATERIAL_PATH)
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("two_sided", False)
try:
    material.set_editor_property("b_always_evaluate_world_position_offset", True)
except Exception:
    pass
try:
    material.set_editor_property("max_world_position_offset_displacement", 18.0)
except Exception:
    pass

local_pos = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionLocalPosition, -1480, -500
)
pixel_normal = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionPixelNormalWS, -1480, -320
)
vertex_normal = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionVertexNormalWS, -1480, -120
)
camera = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionCameraVectorWS, -1480, 80
)
time = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionTime, -1480, 280
)

core = vector_param(material, "CoreColor", unreal.LinearColor(1.0, 0.82, 0.34, 1.0), -1480, 500)
hot = vector_param(material, "HotColor", unreal.LinearColor(1.0, 0.38, 0.06, 1.0), -1480, 700)
spot = vector_param(material, "SpotColor", unreal.LinearColor(0.16, 0.025, 0.008, 1.0), -1480, 900)

emission = scalar_param(material, "EmissionStrength", 1.35, -1480, 1120)
flow_speed = scalar_param(material, "FlowSpeed", 0.12, -1180, 500)
activity = scalar_param(material, "Activity", 0.62, -1180, 680)
churn = scalar_param(material, "ChurnStrength", 1.2, -1180, 860)
granulation = scalar_param(material, "GranulationScale", 18.0, -1180, 1040)
patch_scale = scalar_param(material, "PatchScale", 3.5, -1180, 1220)
spot_scale = scalar_param(material, "SunspotScale", 6.0, -850, 500)
spot_threshold = scalar_param(material, "SunspotThreshold", 0.76, -850, 680)
limb_darkening = scalar_param(material, "LimbDarkening", 0.35, -850, 860)
displacement_strength = scalar_param(material, "SurfaceDisplacement", 7.5, -850, 1040)

surface_code = r"""
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
    float n000 = hash31(i + float3(0.0, 0.0, 0.0));
    float n100 = hash31(i + float3(1.0, 0.0, 0.0));
    float n010 = hash31(i + float3(0.0, 1.0, 0.0));
    float n110 = hash31(i + float3(1.0, 1.0, 0.0));
    float n001 = hash31(i + float3(0.0, 0.0, 1.0));
    float n101 = hash31(i + float3(1.0, 0.0, 1.0));
    float n011 = hash31(i + float3(0.0, 1.0, 1.0));
    float n111 = hash31(i + float3(1.0, 1.0, 1.0));
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
        p = p * 2.02 + float3(7.13, 3.71, 5.97);
        a *= 0.5;
    }
    return v;
}

float3 sp = normalize(LocalPos);
float3 n = normalize(NormalWS);
float3 v = normalize(ViewWS);
float t = Time * FlowSpeed;

float3 grainBase = sp * GranulationScale;
float3 warp = float3(
    fbm(grainBase + float3(t * 1.0, 7.1, 3.4)),
    fbm(grainBase + float3(13.7, t * 0.9 + 2.3, 11.2)),
    fbm(grainBase + float3(5.9, 19.3, t * 1.1 + 6.7))) - 0.5;

float3 grainP = grainBase + warp * ChurnStrength + float3(t * 0.25, -t * 0.12, t * 0.18);
float fine = fbm(grainP + fbm(grainP * 2.0) * 0.8);

float3 patchBase = sp * PatchScale;
float3 patchWarp = float3(
    fbm(patchBase + float3(t * 0.3, 2.1, 0.0)),
    fbm(patchBase + float3(0.0, t * 0.25, 4.7)),
    fbm(patchBase + float3(3.3, 0.0, t * 0.18))) - 0.5;
float patch = fbm(patchBase + patchWarp * ChurnStrength * 0.6 + float3(t * 0.12, t * 0.06, -t * 0.08));

float surface = saturate(lerp(fine, patch, 0.35) + 0.05);

float3 spotP = sp * SunspotScale + float3(t * 0.18, -t * 0.11, t * 0.07);
float spotNoise = fbm(spotP + fbm(spotP * 1.6) * 0.4);
float spotMask = smoothstep(SunspotThreshold, SunspotThreshold + 0.06, spotNoise);
spotMask *= saturate(Activity * 1.35);

float cellEdge = smoothstep(0.10, 0.36, abs(fine - patch));
float brightFilament = smoothstep(0.58, 0.84, fine) * (1.0 - spotMask);
float3 col = lerp(HotColor, CoreColor, smoothstep(0.35, 0.72, surface));
col = lerp(col, HotColor * 1.18, brightFilament * 0.42);
col = lerp(col, SpotColor, spotMask);

float ndv = saturate(abs(dot(n, -v)));
float limb = lerp(1.0 - LimbDarkening, 1.08, pow(ndv, 0.6));
float contrast = 0.82 + cellEdge * 0.22 + brightFilament * 0.28;

return col * limb * contrast * EmissionStrength;
"""

surface = custom_expr(
    material,
    surface_code,
    unreal.CustomMaterialOutputType.CMOT_FLOAT3,
    [
        "LocalPos",
        "NormalWS",
        "ViewWS",
        "Time",
        "CoreColor",
        "HotColor",
        "SpotColor",
        "EmissionStrength",
        "FlowSpeed",
        "Activity",
        "ChurnStrength",
        "GranulationScale",
        "PatchScale",
        "SunspotScale",
        "SunspotThreshold",
        "LimbDarkening",
    ],
    -360,
    -120,
    "Procedural animated stellar photosphere",
)

for source, output, target in [
    (local_pos, "", "LocalPos"),
    (pixel_normal, "", "NormalWS"),
    (camera, "", "ViewWS"),
    (time, "", "Time"),
    (core, "RGB", "CoreColor"),
    (hot, "RGB", "HotColor"),
    (spot, "RGB", "SpotColor"),
    (emission, "", "EmissionStrength"),
    (flow_speed, "", "FlowSpeed"),
    (activity, "", "Activity"),
    (churn, "", "ChurnStrength"),
    (granulation, "", "GranulationScale"),
    (patch_scale, "", "PatchScale"),
    (spot_scale, "", "SunspotScale"),
    (spot_threshold, "", "SunspotThreshold"),
    (limb_darkening, "", "LimbDarkening"),
]:
    connect(source, output, surface, target)

displacement_code = r"""
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
    return lerp(
        lerp(lerp(n000, n100, f.x), lerp(n010, n110, f.x), f.y),
        lerp(lerp(n001, n101, f.x), lerp(n011, n111, f.x), f.y),
        f.z);
}

float fbm(float3 p)
{
    float v = 0.0;
    float a = 0.5;
    [unroll] for (int i = 0; i < 4; ++i)
    {
        v += a * vnoise(p);
        p = p * 2.03 + float3(4.7, 9.2, 1.6);
        a *= 0.5;
    }
    return v;
}

float3 sp = normalize(LocalPos);
float t = Time * FlowSpeed * 0.35;
float disp = fbm(sp * 4.2 + float3(t, -t * 0.55, t * 0.28) + fbm(sp * 7.5) * 0.35) - 0.5;
return normalize(NormalWS) * disp * SurfaceDisplacement;
"""

displacement = custom_expr(
    material,
    displacement_code,
    unreal.CustomMaterialOutputType.CMOT_FLOAT3,
    ["LocalPos", "NormalWS", "Time", "FlowSpeed", "SurfaceDisplacement"],
    -360,
    500,
    "Procedural stellar surface displacement",
)

for source, output, target in [
    (local_pos, "", "LocalPos"),
    (vertex_normal, "", "NormalWS"),
    (time, "", "Time"),
    (flow_speed, "", "FlowSpeed"),
    (displacement_strength, "", "SurfaceDisplacement"),
]:
    connect(source, output, displacement, target)

if not unreal.MaterialEditingLibrary.connect_material_property(
    surface, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
):
    raise RuntimeError("Failed to connect photosphere emissive output")
if not unreal.MaterialEditingLibrary.connect_material_property(
    displacement, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET
):
    raise RuntimeError("Failed to connect photosphere world-position offset")

unreal.MaterialEditingLibrary.layout_material_expressions(material)
unreal.MaterialEditingLibrary.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
unreal.log(f"Created {MATERIAL_PATH} as procedural animated photosphere material.")
