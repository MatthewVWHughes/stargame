import unreal


STAR_BP = "/Game/Blueprints/Space/BP_SectorStarAnchor"
PREVIEW_MAP = "/Game/Maps/StellarClassPreview"
MATERIAL_DIR = "/Game/Materials/Space"
SURFACE_MATERIAL = f"{MATERIAL_DIR}/M_StarPhotosphereProcedural"
CORONA_MATERIAL = f"{MATERIAL_DIR}/M_StarCoronaProcedural"
PROMINENCE_MATERIAL = f"{MATERIAL_DIR}/M_StarProminence"


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
        raise RuntimeError(f"Failed to create {path}")
    return material


def scalar_param(material, name, value, x, y):
    expr = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    expr.set_editor_property("parameter_name", name)
    expr.set_editor_property("default_value", value)
    return expr


def vector_param(material, name, value, x, y):
    expr = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    expr.set_editor_property("parameter_name", name)
    expr.set_editor_property("default_value", value)
    return expr


def custom_input(name):
    item = unreal.CustomInput()
    item.set_editor_property("input_name", name)
    return item


def custom_expr(material, code, output_type, inputs, x, y, description):
    expr = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionCustom, x, y)
    expr.set_editor_property("code", code)
    expr.set_editor_property("output_type", output_type)
    expr.set_editor_property("description", description)
    expr.set_editor_property("inputs", [custom_input(name) for name in inputs])
    return expr


def connect(src, output, dst, input_name):
    if not unreal.MaterialEditingLibrary.connect_material_expressions(src, output, dst, input_name):
        raise RuntimeError(f"Could not connect {src.get_name()}:{output} -> {dst.get_name()}:{input_name}")


def connect_property(src, prop, output=""):
    if not unreal.MaterialEditingLibrary.connect_material_property(src, output, prop):
        raise RuntimeError(f"Could not connect {src.get_name()} to {prop}")


def clear_static_mesh_component(component):
    try:
        component.set_static_mesh(None)
    except Exception:
        pass
    try:
        component.set_editor_property("static_mesh", None)
    except Exception:
        pass
    try:
        component.set_material(0, None)
    except Exception:
        pass
    try:
        component.set_editor_property("visible", False)
        component.set_editor_property("hidden_in_game", True)
    except Exception:
        pass


NOISE_HLSL = r"""
struct FStarNoise
{
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
    return lerp(
        lerp(lerp(n000, n100, f.x), lerp(n010, n110, f.x), f.y),
        lerp(lerp(n001, n101, f.x), lerp(n011, n111, f.x), f.y),
        f.z);
}

float fbm(float3 p)
{
    float v = 0.0;
    float a = 0.5;
    [unroll] for (int i = 0; i < 5; ++i)
    {
        v += a * vnoise(p);
        p = p * 2.03 + float3(5.2, 1.7, 8.3);
        a *= 0.5;
    }
    return v;
}
};
FStarNoise StarNoise;
"""


def build_surface_material():
    material = create_or_replace_material(SURFACE_MATERIAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVertexColor, -500, 0)
    brightness = scalar_param(material, "SurfaceBrightness", 2.8, -500, 220)
    multiply = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionMultiply, -180, 70)
    connect(vertex_color, "", multiply, "A")
    connect(brightness, "", multiply, "B")
    connect_property(multiply, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material
    try:
        material.set_editor_property("b_always_evaluate_world_position_offset", True)
        material.set_editor_property("max_world_position_offset_displacement", 18.0)
    except Exception:
        pass

    local_pos = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionLocalPosition, -1520, -520)
    pixel_normal = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionPixelNormalWS, -1520, -340)
    vertex_normal = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVertexNormalWS, -1520, -160)
    camera = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -1520, 20)
    time = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTime, -1520, 200)
    camera_pos = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionCameraPositionWS, -1520, 390)
    object_pos = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionObjectPositionWS, -1520, 570)
    core = vector_param(material, "CoreColor", unreal.LinearColor(0.95, 0.46, 0.10, 1.0), -1520, 750)
    hot = vector_param(material, "HotColor", unreal.LinearColor(1.0, 0.88, 0.30, 1.0), -1520, 930)
    spot = vector_param(material, "SpotColor", unreal.LinearColor(0.11, 0.025, 0.005, 1.0), -1520, 1110)
    emission = scalar_param(material, "EmissionStrength", 0.62, -1180, 390)
    flow = scalar_param(material, "FlowSpeed", 0.12, -1180, 570)
    activity = scalar_param(material, "Activity", 0.70, -1180, 750)
    churn = scalar_param(material, "ChurnStrength", 1.75, -1180, 930)
    granulation = scalar_param(material, "GranulationScale", 34.0, -840, 390)
    patch = scalar_param(material, "PatchScale", 4.4, -840, 570)
    spot_scale = scalar_param(material, "SunspotScale", 5.4, -840, 750)
    spot_threshold = scalar_param(material, "SunspotThreshold", 0.61, -840, 930)
    limb = scalar_param(material, "LimbDarkening", 0.58, -520, 390)
    displacement_strength = scalar_param(material, "SurfaceDisplacement", 22.0, -520, 570)
    object_radius = scalar_param(material, "ObjectRadius", 2000.0, -520, 750)

    surface_code = NOISE_HLSL + r"""
float3 sp = normalize(LocalPos);
float3 n = normalize(NormalWS);
float3 v = normalize(ViewWS);
float t = Time * FlowSpeed;

float camDist = distance(CameraPos, ObjectPos);
float distanceFade = saturate((camDist - ObjectRadius * 2.2) / max(ObjectRadius * 6.0, 1.0));

float3 grainBase = sp * GranulationScale;
float3 warp = float3(
    StarNoise.fbm(grainBase + float3(t, 7.1, 3.4)),
    StarNoise.fbm(grainBase + float3(13.7, t * 0.9 + 2.3, 11.2)),
    StarNoise.fbm(grainBase + float3(5.9, 19.3, t * 1.1 + 6.7))) - 0.5;
float3 grainP = grainBase + warp * ChurnStrength + float3(t * 0.25, -t * 0.12, t * 0.18);
float fine = StarNoise.fbm(grainP + StarNoise.fbm(grainP * 2.0) * 0.8);
float fine2 = StarNoise.fbm(grainP * 2.7 + float3(-t * 0.45, t * 0.18, t * 0.28));

float3 patchBase = sp * PatchScale;
float3 patchWarp = float3(
    StarNoise.fbm(patchBase + float3(t * 0.30, 2.1, 0.0)),
    StarNoise.fbm(patchBase + float3(0.0, t * 0.25, 4.7)),
    StarNoise.fbm(patchBase + float3(3.3, 0.0, t * 0.18))) - 0.5;
float broad = StarNoise.fbm(patchBase + patchWarp * ChurnStrength * 0.65 + float3(t * 0.12, t * 0.06, -t * 0.08));
float surface = saturate(lerp(fine, broad, 0.42) + 0.08);

float3 spotP = sp * SunspotScale + float3(t * 0.18, -t * 0.11, t * 0.07);
float macroSpot = StarNoise.fbm(spotP + StarNoise.fbm(spotP * 1.7) * 0.65);
float spotInterior = StarNoise.fbm(spotP * 2.9 + float3(t * 0.08, -t * 0.19, 1.7));
float spotMask = smoothstep(SunspotThreshold, SunspotThreshold + 0.085, macroSpot) * smoothstep(0.34, 0.68, spotInterior) * saturate(Activity * 1.45);
spotMask *= 1.0 - distanceFade * 0.78;

float cellEdge = smoothstep(0.045, 0.19, abs(fine - broad));
float cellularDark = smoothstep(0.18, 0.48, fine2) * smoothstep(0.18, 0.62, 1.0 - broad);
float brightFilament = smoothstep(0.50, 0.78, fine) * smoothstep(0.42, 0.82, broad) * (1.0 - spotMask);
float3 coolPlasma = CoreColor * 0.42;
float3 warmPlasma = CoreColor * 0.95;
float3 col = lerp(coolPlasma, warmPlasma, smoothstep(0.24, 0.78, surface));
col = lerp(col, HotColor * 1.12, brightFilament * 0.70);
col = lerp(col, CoreColor * 0.25, cellularDark * 0.22 * (1.0 - brightFilament));
col = lerp(col, SpotColor, spotMask);

float ndv = saturate(abs(dot(n, -v)));
float limbTerm = lerp(1.0 - LimbDarkening, 1.04, pow(ndv, 0.58));
float rimHeat = pow(1.0 - ndv, 3.0) * 0.18;
float contrast = 0.62 + cellEdge * 0.34 + brightFilament * 0.42;
float distanceLift = distanceFade * 0.30;
float3 finalCol = col * limbTerm * contrast + HotColor * rimHeat + CoreColor * distanceLift;
return finalCol * EmissionStrength;
"""
    surface = custom_expr(
        material,
        surface_code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        [
            "LocalPos", "NormalWS", "ViewWS", "CameraPos", "ObjectPos", "Time", "CoreColor", "HotColor", "SpotColor",
            "EmissionStrength", "FlowSpeed", "Activity", "ChurnStrength", "GranulationScale",
            "PatchScale", "SunspotScale", "SunspotThreshold", "LimbDarkening", "ObjectRadius",
        ],
        -170,
        -130,
        "Domain-warped animated stellar photosphere",
    )
    for source, output, target in [
        (local_pos, "", "LocalPos"), (pixel_normal, "", "NormalWS"), (camera, "", "ViewWS"),
        (camera_pos, "", "CameraPos"), (object_pos, "", "ObjectPos"),
        (time, "", "Time"), (core, "RGB", "CoreColor"), (hot, "RGB", "HotColor"),
        (spot, "RGB", "SpotColor"), (emission, "", "EmissionStrength"), (flow, "", "FlowSpeed"),
        (activity, "", "Activity"), (churn, "", "ChurnStrength"), (granulation, "", "GranulationScale"),
        (patch, "", "PatchScale"), (spot_scale, "", "SunspotScale"),
        (spot_threshold, "", "SunspotThreshold"), (limb, "", "LimbDarkening"),
        (object_radius, "", "ObjectRadius"),
    ]:
        connect(source, output, surface, target)

    displacement_code = NOISE_HLSL + r"""
float3 sp = normalize(LocalPos);
float t = Time * FlowSpeed * 0.35;
float disp = StarNoise.fbm(sp * 4.2 + float3(t, -t * 0.55, t * 0.28) + StarNoise.fbm(sp * 7.5) * 0.35) - 0.5;
return normalize(NormalWS) * disp * SurfaceDisplacement;
"""
    displacement = custom_expr(
        material,
        displacement_code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        ["LocalPos", "NormalWS", "Time", "FlowSpeed", "SurfaceDisplacement"],
        -170,
        470,
        "Subtle procedural surface displacement",
    )
    for source, output, target in [
        (local_pos, "", "LocalPos"), (vertex_normal, "", "NormalWS"), (time, "", "Time"),
        (flow, "", "FlowSpeed"), (displacement_strength, "", "SurfaceDisplacement"),
    ]:
        connect(source, output, displacement, target)

    connect_property(surface, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    connect_property(displacement, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def build_corona_material():
    material = create_or_replace_material(CORONA_MATERIAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    try:
        material.set_editor_property("disable_depth_test", True)
    except Exception:
        pass

    local_pos = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionLocalPosition, -1180, -360)
    normal = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionPixelNormalWS, -1180, -180)
    camera = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -1180, 0)
    time = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTime, -1180, 180)
    halo = vector_param(material, "HaloColor", unreal.LinearColor(0.95, 0.34, 0.06, 1.0), -1180, 370)
    flare = vector_param(material, "FlareColor", unreal.LinearColor(1.0, 0.82, 0.28, 1.0), -1180, 550)
    emission = scalar_param(material, "CoronaEmission", 0.18, -840, 370)
    activity = scalar_param(material, "Activity", 0.70, -840, 550)
    speed = scalar_param(material, "TurbulenceSpeed", 0.055, -840, 730)

    code = NOISE_HLSL + r"""
float3 sp = normalize(LocalPos);
float3 n = normalize(NormalWS);
float3 v = normalize(ViewWS);
float t = Time * TurbulenceSpeed;
float3 flameP = sp * 6.5 + float3(t, -t * 0.6, t * 0.3);
float warp = StarNoise.fbm(flameP * 0.5) * 1.4;
float flame = saturate(StarNoise.fbm(flameP + warp) + 0.04);
float rim = 1.0 - saturate(abs(dot(n, -v)));
float shell = pow(rim, 3.6);
float tongues = smoothstep(0.55, 0.88, flame) * saturate(Activity * 1.25);
float alpha = shell * (0.10 + tongues * 0.82);
float3 col = lerp(HaloColor, FlareColor, tongues * 0.65 + shell * 0.18);
return col * alpha * CoronaEmission;
"""
    color = custom_expr(
        material,
        code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        ["LocalPos", "NormalWS", "ViewWS", "Time", "HaloColor", "FlareColor", "CoronaEmission", "Activity", "TurbulenceSpeed"],
        -260,
        -40,
        "Procedural Fresnel corona shell",
    )
    for source, output, target in [
        (local_pos, "", "LocalPos"), (normal, "", "NormalWS"), (camera, "", "ViewWS"),
        (time, "", "Time"), (halo, "RGB", "HaloColor"), (flare, "RGB", "FlareColor"),
        (emission, "", "CoronaEmission"), (activity, "", "Activity"), (speed, "", "TurbulenceSpeed"),
    ]:
        connect(source, output, color, target)
    connect_property(color, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    connect_property(color, unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def build_prominence_material():
    material = create_or_replace_material(PROMINENCE_MATERIAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    try:
        material.set_editor_property("disable_depth_test", True)
    except Exception:
        pass

    texcoord = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1040, -260)
    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVertexColor, -1040, -420)
    time = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTime, -1040, -80)
    base = vector_param(material, "FlareColor", unreal.LinearColor(0.95, 0.20, 0.035, 1.0), -1040, 110)
    tip = vector_param(material, "TipColor", unreal.LinearColor(1.0, 0.86, 0.35, 1.0), -1040, 290)
    emission = scalar_param(material, "FlareEmission", 4.0, -1040, 470)
    activity = scalar_param(material, "Activity", 0.70, -740, 110)
    seed = scalar_param(material, "SeedOffset", 0.0, -740, 290)

    code = NOISE_HLSL + r"""
float u = saturate(UV.x);
float v = abs(UV.y * 2.0 - 1.0);
float t = Time * 0.10 + SeedOffset;
float filament = StarNoise.fbm(float3(u * 9.0 + t, v * 3.0, SeedOffset));
float core = 0.68 + 0.32 * pow(saturate(1.0 - v), 0.45);
float endpoints = smoothstep(0.02, 0.20, u) * smoothstep(0.02, 0.20, 1.0 - u);
float pulse = 0.72 + 0.28 * sin(Time * (0.6 + frac(SeedOffset) * 0.35) + SeedOffset);
float vertexFade = saturate(VertexColor.a);
float ragged = lerp(0.42, 1.0, smoothstep(0.12, 0.82, filament));
float alpha = core * endpoints * ragged * saturate(Activity * 1.35) * pulse * vertexFade;
float3 authoredColor = lerp(FlareColor, TipColor, smoothstep(0.35, 0.92, filament) * (1.0 - v * 0.35));
float3 col = lerp(authoredColor, VertexColor.rgb, 0.55);
return col * alpha * FlareEmission * 2.35;
"""
    color = custom_expr(
        material,
        code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        ["UV", "VertexColor", "Time", "FlareColor", "TipColor", "FlareEmission", "Activity", "SeedOffset"],
        -240,
        70,
        "Procedural arcing prominence ribbon",
    )
    for source, output, target in [
        (texcoord, "", "UV"), (vertex_color, "", "VertexColor"), (time, "", "Time"), (base, "RGB", "FlareColor"),
        (tip, "RGB", "TipColor"), (emission, "", "FlareEmission"),
        (activity, "", "Activity"), (seed, "", "SeedOffset"),
    ]:
        connect(source, output, color, target)
    connect_property(color, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity_code = NOISE_HLSL + r"""
float u = saturate(UV.x);
float v = abs(UV.y * 2.0 - 1.0);
float t = Time * 0.10 + SeedOffset;
float filament = StarNoise.fbm(float3(u * 9.0 + t, v * 3.0, SeedOffset));
float core = 0.70 + 0.30 * pow(saturate(1.0 - v), 0.45);
float endpoints = smoothstep(0.02, 0.16, u) * smoothstep(0.02, 0.16, 1.0 - u);
float pulse = 0.78 + 0.22 * sin(Time * (0.6 + frac(SeedOffset) * 0.35) + SeedOffset);
float ragged = lerp(0.55, 1.0, smoothstep(0.10, 0.78, filament));
return saturate(core * endpoints * ragged * saturate(Activity * 1.45) * pulse * saturate(VertexColor.a) * 1.95);
"""
    opacity = custom_expr(
        material,
        opacity_code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT1,
        ["UV", "VertexColor", "Time", "Activity", "SeedOffset"],
        -240,
        310,
        "Procedural prominence opacity",
    )
    for source, output, target in [
        (texcoord, "", "UV"), (vertex_color, "", "VertexColor"), (time, "", "Time"),
        (activity, "", "Activity"), (seed, "", "SeedOffset"),
    ]:
        connect(source, output, opacity, target)
    connect_property(opacity, unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
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
        pass
    if not cls:
        cls = unreal.load_class(None, f"{blueprint.get_path_name()}_C")
    if not cls:
        raise RuntimeError(f"Could not resolve generated class for {blueprint.get_path_name()}")
    return cls


def set_component_bool(component, name, value):
    try:
        component.set_editor_property(name, value)
    except Exception:
        pass


def set_any_property(obj, names, value):
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return True
        except Exception:
            pass
    unreal.log_warning(f"{obj.get_name()}: could not set any of {names}")
    return False


def configure_star_blueprint(surface, corona, prominence):
    bp = unreal.EditorAssetLibrary.load_asset(STAR_BP)
    if not bp:
        raise RuntimeError(f"Missing {STAR_BP}")
    cdo = unreal.get_default_object(generated_class(bp))
    set_any_property(cdo, ["StarSurfaceMaterial", "star_surface_material"], surface)
    set_any_property(cdo, ["StarCoronaMaterial", "star_corona_material"], corona)
    set_any_property(cdo, ["StarProminenceMaterial", "star_prominence_material"], prominence)
    set_any_property(cdo, ["StellarClass", "stellar_class"], unreal.StargameStellarClass.G)
    set_any_property(cdo, ["VisualEmissionScale", "visual_emission_scale"], 0.08)
    set_any_property(cdo, ["LightIntensityScale", "light_intensity_scale"], 0.05)
    set_any_property(cdo, ["bShowCorona", "show_corona", "b_show_corona"], False)
    set_any_property(cdo, ["bShowProminences", "show_prominences", "b_show_prominences"], True)
    set_any_property(cdo, ["bShowEjections", "show_ejections", "b_show_ejections"], False)
    set_any_property(cdo, ["bShowHaloBillboard", "show_halo_billboard", "b_show_halo_billboard"], False)
    set_any_property(cdo, ["bUsePreviewCamera", "b_use_preview_camera"], True)
    set_any_property(cdo, ["PhotosphereLatitudeSegments", "photosphere_latitude_segments"], 192)
    set_any_property(cdo, ["PhotosphereLongitudeSegments", "photosphere_longitude_segments"], 384)
    set_any_property(cdo, ["PhotosphereRefreshInterval", "photosphere_refresh_interval"], 0.16)
    set_any_property(cdo, ["PhotosphereYawDegreesPerSecond", "photosphere_yaw_degrees_per_second"], 0.16)
    set_any_property(cdo, ["PhotosphereRollDegreesPerSecond", "photosphere_roll_degrees_per_second"], 0.025)
    set_any_property(cdo, ["PhotosphereBroadScale", "photosphere_broad_scale"], 4.8)
    set_any_property(cdo, ["PhotosphereMidScale", "photosphere_mid_scale"], 18.0)
    set_any_property(cdo, ["PhotosphereFineScale", "photosphere_fine_scale"], 72.0)
    set_any_property(cdo, ["PhotosphereMicroScale", "photosphere_micro_scale"], 156.0)
    set_any_property(cdo, ["PhotosphereMicroSparkStrength", "photosphere_micro_spark_strength"], 0.30)
    set_any_property(cdo, ["PhotosphereCoolCellStrength", "photosphere_cool_cell_strength"], 0.40)
    set_any_property(cdo, ["PhotosphereSunspotStrength", "photosphere_sunspot_strength"], 0.72)
    set_any_property(cdo, ["PhotosphereAnchoredSpotStrength", "photosphere_anchored_spot_strength"], 1.0)
    set_any_property(cdo, ["PhotosphereSunspotSeed", "photosphere_sunspot_seed"], 37.0)
    set_any_property(cdo, ["PhotosphereSunspotDriftDegreesPerSecond", "photosphere_sunspot_drift_degrees_per_second"], 0.55)
    set_any_property(cdo, ["PhotosphereDynamicSunspotCount", "photosphere_dynamic_sunspot_count"], 5)
    set_any_property(cdo, ["PhotosphereSunspotLifetimeSeconds", "photosphere_sunspot_lifetime_seconds"], 18.0)
    set_any_property(cdo, ["PhotosphereSunspotAngularSize", "photosphere_sunspot_angular_size"], 2.1)
    set_any_property(cdo, ["PhotosphereLimbDarkening", "photosphere_limb_darkening"], 0.58)
    set_any_property(cdo, ["PhotosphereDisplacementCm", "photosphere_displacement_cm"], 54.0)
    set_any_property(cdo, ["CoronaYawDegreesPerSecond", "corona_yaw_degrees_per_second"], -0.55)
    set_any_property(cdo, ["CoronaRollDegreesPerSecond", "corona_roll_degrees_per_second"], 0.09)
    set_any_property(cdo, ["ProminenceArcCount", "prominence_arc_count"], 4)
    set_any_property(cdo, ["ProminenceArcHeight", "prominence_arc_height"], 0.38)
    set_any_property(cdo, ["ProminenceArcWidth", "prominence_arc_width"], 0.09)
    set_any_property(cdo, ["ProminenceArcSpanDegrees", "prominence_arc_span_degrees"], 34.0)
    set_any_property(cdo, ["ProminenceArcCurl", "prominence_arc_curl"], 0.10)
    set_any_property(cdo, ["ProminenceSeed", "prominence_seed"], 11.0)

    for name, component in component_templates(bp):
        if name == "StarBody":
            component.set_material(0, surface)
            component.set_editor_property("visible", False)
            component.set_editor_property("hidden_in_game", True)
            set_component_bool(component, "cast_shadow", False)
        elif name == "CoronaShell":
            component.set_material(0, corona)
            component.set_editor_property("visible", False)
            component.set_editor_property("hidden_in_game", True)
            set_component_bool(component, "cast_shadow", False)
            set_component_bool(component, "b_disallow_nanite", True)
        elif name == "PhotosphereSurface":
            component.set_material(0, surface)
            component.set_editor_property("visible", True)
            component.set_editor_property("hidden_in_game", False)
        elif name == "CoronaSurface":
            component.set_material(0, corona)
            component.set_editor_property("visible", True)
            component.set_editor_property("hidden_in_game", False)
        elif name == "ProminenceShell":
            clear_static_mesh_component(component)
        elif name.startswith("EjectionBillboard") or name == "StarHaloBillboard":
            component.set_editor_property("visible", False)
            component.set_editor_property("hidden_in_game", True)

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return bp


def configure_preview_map(bp, surface, corona, prominence):
    if hasattr(unreal.EditorLoadingAndSavingUtils, "load_map"):
        unreal.EditorLoadingAndSavingUtils.load_map(PREVIEW_MAP)
    else:
        unreal.EditorLevelLibrary.load_level(PREVIEW_MAP)
    bp_class = generated_class(bp)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors()
    star_actors = [
        actor for actor in actors
        if "SectorStarAnchor" in actor.get_class().get_name() or "SectorStarAnchor" in actor.get_name()
    ]
    if star_actors:
        star_actors.sort(key=lambda actor: actor.get_name())
        star = star_actors[0]
        for actor in star_actors[1:]:
            actor_subsystem.destroy_actor(actor)
    else:
        star = actor_subsystem.spawn_actor_from_class(bp_class, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    star.set_actor_label("Single_G_Star")
    star.set_actor_location(unreal.Vector(0.0, 0.0, 0.0), False, False)
    star.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
    for names, value in [
        (["StarSurfaceMaterial", "star_surface_material"], surface),
        (["StarCoronaMaterial", "star_corona_material"], corona),
        (["StarProminenceMaterial", "star_prominence_material"], prominence),
        (["StellarClass", "stellar_class"], unreal.StargameStellarClass.G),
        (["VisualEmissionScale", "visual_emission_scale"], 0.08),
        (["LightIntensityScale", "light_intensity_scale"], 0.05),
        (["bShowCorona", "show_corona", "b_show_corona"], False),
        (["bShowProminences", "show_prominences", "b_show_prominences"], True),
        (["bShowEjections", "show_ejections", "b_show_ejections"], False),
        (["bShowHaloBillboard", "show_halo_billboard", "b_show_halo_billboard"], False),
        (["bShowClassLabel", "show_class_label", "b_show_class_label"], False),
        (["bUsePreviewCamera", "b_use_preview_camera"], True),
        (["PhotosphereLatitudeSegments", "photosphere_latitude_segments"], 192),
        (["PhotosphereLongitudeSegments", "photosphere_longitude_segments"], 384),
        (["PhotosphereRefreshInterval", "photosphere_refresh_interval"], 0.16),
        (["PhotosphereYawDegreesPerSecond", "photosphere_yaw_degrees_per_second"], 0.16),
        (["PhotosphereRollDegreesPerSecond", "photosphere_roll_degrees_per_second"], 0.025),
        (["PhotosphereBroadScale", "photosphere_broad_scale"], 4.8),
        (["PhotosphereMidScale", "photosphere_mid_scale"], 18.0),
        (["PhotosphereFineScale", "photosphere_fine_scale"], 72.0),
        (["PhotosphereMicroScale", "photosphere_micro_scale"], 156.0),
        (["PhotosphereMicroSparkStrength", "photosphere_micro_spark_strength"], 0.30),
        (["PhotosphereCoolCellStrength", "photosphere_cool_cell_strength"], 0.40),
        (["PhotosphereSunspotStrength", "photosphere_sunspot_strength"], 0.72),
        (["PhotosphereAnchoredSpotStrength", "photosphere_anchored_spot_strength"], 1.0),
        (["PhotosphereSunspotSeed", "photosphere_sunspot_seed"], 37.0),
        (["PhotosphereSunspotDriftDegreesPerSecond", "photosphere_sunspot_drift_degrees_per_second"], 0.55),
        (["PhotosphereDynamicSunspotCount", "photosphere_dynamic_sunspot_count"], 5),
        (["PhotosphereSunspotLifetimeSeconds", "photosphere_sunspot_lifetime_seconds"], 18.0),
        (["PhotosphereSunspotAngularSize", "photosphere_sunspot_angular_size"], 2.1),
        (["PhotosphereLimbDarkening", "photosphere_limb_darkening"], 0.58),
        (["PhotosphereDisplacementCm", "photosphere_displacement_cm"], 54.0),
        (["CoronaYawDegreesPerSecond", "corona_yaw_degrees_per_second"], -0.55),
        (["CoronaRollDegreesPerSecond", "corona_roll_degrees_per_second"], 0.09),
        (["ProminenceArcCount", "prominence_arc_count"], 4),
        (["ProminenceArcHeight", "prominence_arc_height"], 0.38),
        (["ProminenceArcWidth", "prominence_arc_width"], 0.09),
        (["ProminenceArcSpanDegrees", "prominence_arc_span_degrees"], 34.0),
        (["ProminenceArcCurl", "prominence_arc_curl"], 0.10),
        (["ProminenceSeed", "prominence_seed"], 11.0),
    ]:
        set_any_property(star, names, value)
    try:
        star.apply_stellar_class(unreal.StargameStellarClass.G)
    except Exception:
        pass

    for component in star.get_components_by_class(unreal.StaticMeshComponent):
        mesh = None
        try:
            mesh = component.get_editor_property("static_mesh")
        except Exception:
            pass
        if component.get_name() == "ProminenceShell" or (mesh and mesh.get_name() == "SM_StarProminenceBundle"):
            clear_static_mesh_component(component)

    pp_volumes = [actor for actor in actor_subsystem.get_all_level_actors() if actor.get_class().get_name() == "PostProcessVolume"]
    if pp_volumes:
        pp = pp_volumes[0]
    else:
        pp_class = unreal.load_class(None, "/Script/Engine.PostProcessVolume")
        pp = actor_subsystem.spawn_actor_from_class(pp_class, unreal.Vector(4500.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    pp.set_actor_label("StarPreview_ManualExposure")
    pp_component = pp.get_component_by_class(unreal.PostProcessComponent)
    if pp_component:
        pp_component.set_editor_property("unbound", True)
        settings = pp_component.get_editor_property("settings")
        for attr, value in [
            ("override_auto_exposure_method", True),
            ("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL),
            ("override_auto_exposure_bias", True),
            ("auto_exposure_bias", 0.0),
            ("override_bloom_intensity", True),
            ("bloom_intensity", 0.05),
            ("override_bloom_threshold", True),
            ("bloom_threshold", 4.0),
        ]:
            try:
                settings.set_editor_property(attr, value)
            except Exception:
                pass
        pp_component.set_editor_property("settings", settings)

    world = unreal.EditorLevelLibrary.get_editor_world()
    if world:
        game_mode = unreal.load_class(None, "/Script/Engine.GameModeBase")
        if game_mode:
            world.get_world_settings().set_editor_property("default_game_mode", game_mode)

    unreal.EditorLevelLibrary.save_current_level()


def main():
    surface = build_surface_material()
    corona = build_corona_material()
    prominence = build_prominence_material()
    bp = configure_star_blueprint(surface, corona, prominence)
    configure_preview_map(bp, surface, corona, prominence)
    print("Built reusable procedural star system and configured StellarClassPreview with one G-class star.")


if __name__ == "__main__":
    main()
