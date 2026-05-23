import unreal


MATERIAL_PACKAGE_PATH = "/Game/Materials/Space"
SURFACE_MATERIAL_NAME = "M_StarPhotosphereProcedural"
SURFACE_MATERIAL_PATH = f"{MATERIAL_PACKAGE_PATH}/{SURFACE_MATERIAL_NAME}"
CORE_MATERIAL_NAME = "M_StarCoreEmissive"
CORE_MATERIAL_PATH = f"{MATERIAL_PACKAGE_PATH}/{CORE_MATERIAL_NAME}"
CORONA_MATERIAL_NAME = "M_StarCoronaProcedural"
CORONA_MATERIAL_PATH = f"{MATERIAL_PACKAGE_PATH}/{CORONA_MATERIAL_NAME}"
HALO_MATERIAL_NAME = "M_StarHaloProcedural"
HALO_MATERIAL_PATH = f"{MATERIAL_PACKAGE_PATH}/{HALO_MATERIAL_NAME}"
PROMINENCE_MATERIAL_NAME = "M_StarProminence"
PROMINENCE_MATERIAL_PATH = f"{MATERIAL_PACKAGE_PATH}/{PROMINENCE_MATERIAL_NAME}"


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


def scalar_param(material, name, value, x, y):
    expr = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, x, y
    )
    expr.set_editor_property("parameter_name", name)
    expr.set_editor_property("default_value", value)
    return expr


def vector_param(material, name, value, x, y):
    expr = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, x, y
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


def connect_property(src, prop):
    if not unreal.MaterialEditingLibrary.connect_material_property(src, "", prop):
        raise RuntimeError(f"Failed to connect {src.get_name()} to {prop}")


def configure_unlit_surface(material):
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)


def build_photosphere_material():
    material = create_or_replace_material(SURFACE_MATERIAL_PATH)
    configure_unlit_surface(material)

    local_pos = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionLocalPosition, -940, -360
    )
    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVertexColor, -940, -180
    )
    normal_ws = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionPixelNormalWS, -940, 0
    )
    camera_ws = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionCameraVectorWS, -940, 180
    )
    time = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTime, -940, 360
    )
    surface_brightness = scalar_param(material, "SurfaceBrightness", 1.12, -940, 560)
    core_color = vector_param(material, "CoreColor", unreal.LinearColor(1.0, 0.62, 0.18, 1.0), -940, 760)
    hot_color = vector_param(material, "HotColor", unreal.LinearColor(1.0, 0.92, 0.54, 1.0), -940, 960)
    spot_color = vector_param(material, "SpotColor", unreal.LinearColor(0.055, 0.018, 0.006, 1.0), -940, 1160)
    emission_strength = scalar_param(material, "EmissionStrength", 2.35, -600, 560)
    flow_speed = scalar_param(material, "FlowSpeed", 0.34, -600, 760)
    activity = scalar_param(material, "Activity", 0.64, -600, 960)
    granulation_scale = scalar_param(material, "GranulationScale", 42.0, -600, 1160)
    patch_scale = scalar_param(material, "PatchScale", 5.2, -250, 560)
    sunspot_scale = scalar_param(material, "SunspotScale", 5.8, -250, 760)
    limb_brightness = scalar_param(material, "LimbBrightness", 0.38, -250, 960)
    facula_strength = scalar_param(material, "FaculaStrength", 0.18, -250, 1160)

    code = NOISE_HLSL + r"""
float3 sp = normalize(LocalPos);
float3 n = normalize(NormalWS);
float3 v = normalize(ViewWS);
float t = Time * FlowSpeed;

float3 broadP = sp * PatchScale + float3(t * 0.34, -t * 0.22, t * 0.27);
float3 broadWarp = float3(
    StarNoise.fbm(broadP + float3(1.3, t * 0.16, 4.7)),
    StarNoise.fbm(broadP + float3(9.1, 3.4, -t * 0.13)),
    StarNoise.fbm(broadP + float3(-t * 0.10, 6.2, 2.8))) - 0.5;
float broad = StarNoise.fbm(broadP + broadWarp * 0.95);

float3 grainP = sp * GranulationScale + broadWarp * 2.9 + float3(t * 1.18, -t * 0.82, t * 0.71);
float grainA = StarNoise.fbm(grainP);
float grainB = StarNoise.fbm(grainP * 1.92 + float3(-t * 0.46, t * 0.33, 2.4));
float cellWall = smoothstep(0.035, 0.19, abs(grainA - broad));
float cellDark = smoothstep(0.30, 0.78, grainB) * smoothstep(0.12, 0.54, 1.0 - broad);
float hotFilament = smoothstep(0.50, 0.86, grainA) * smoothstep(0.34, 0.84, broad);
float magneticLane = smoothstep(0.82, 0.97, StarNoise.fbm(sp * 11.5 + broadWarp * 1.7 + float3(-t * 0.44, t * 0.29, 3.1)));

float3 spotP = sp * SunspotScale + float3(t * 0.05, -t * 0.09, 1.7);
float spotMacro = StarNoise.fbm(spotP + StarNoise.fbm(spotP * 1.55) * 0.6);
float spotFine = StarNoise.fbm(spotP * 3.0 + float3(-t * 0.24, 2.1, t * 0.17));
float spotMask = smoothstep(0.74, 0.89, spotMacro) * smoothstep(0.42, 0.76, spotFine) * saturate(Activity * 1.15);
float spotPenumbra = smoothstep(0.64, 0.84, spotMacro) * (1.0 - spotMask) * saturate(Activity);
float facula = smoothstep(0.58, 0.90, broad) * smoothstep(0.45, 0.82, grainA) * saturate(Activity * 1.25);

float thermal = saturate(broad * 0.42 + grainA * 0.38 + grainB * 0.20);
float3 plasma = lerp(CoreColor * 0.48, CoreColor * 0.92, smoothstep(0.20, 0.78, thermal));
plasma = lerp(plasma, HotColor * 1.14, hotFilament * 0.54);
plasma = lerp(plasma, HotColor * 1.26, magneticLane * Activity * 0.20);
plasma = lerp(plasma, CoreColor * 0.34, cellDark * 0.22);
plasma = lerp(plasma, HotColor * 1.18, facula * FaculaStrength);
plasma = lerp(plasma, SpotColor * 1.15, spotPenumbra * 0.45);
plasma = lerp(plasma, SpotColor * 0.82, spotMask);

float center = saturate(abs(dot(n, v)));
float limb = lerp(LimbBrightness, 1.0, pow(center, 0.62));
float rimHeat = pow(1.0 - center, 4.0) * 0.10;
float contrast = 0.74 + cellWall * 0.30 + hotFilament * 0.22 + magneticLane * 0.16;
float vertexLift = saturate(dot(max(VertexColor.rgb, 0.0), float3(0.2126, 0.7152, 0.0722)));
float boil = 0.92 + 0.08 * sin(Time * 0.72 + broad * 5.4 + grainA * 2.1);
float3 authoredLift = lerp(float3(1.0, 1.0, 1.0), max(VertexColor.rgb, 0.0) * 1.08, saturate(vertexLift * 0.16));
return (plasma * authoredLift * limb * contrast * boil + HotColor * rimHeat) * SurfaceBrightness * EmissionStrength;
"""
    emissive = custom_expr(
        material,
        code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        [
            "LocalPos", "VertexColor", "NormalWS", "ViewWS", "Time", "SurfaceBrightness",
            "CoreColor", "HotColor", "SpotColor", "EmissionStrength", "FlowSpeed", "Activity",
            "GranulationScale", "PatchScale", "SunspotScale", "LimbBrightness", "FaculaStrength",
        ],
        -220,
        210,
        "Animated procedural photosphere with granulation, sunspots, faculae, and limb darkening",
    )

    connect(local_pos, "", emissive, "LocalPos")
    connect(vertex_color, "", emissive, "VertexColor")
    connect(normal_ws, "", emissive, "NormalWS")
    connect(camera_ws, "", emissive, "ViewWS")
    connect(time, "", emissive, "Time")
    connect(surface_brightness, "", emissive, "SurfaceBrightness")
    connect(core_color, "RGB", emissive, "CoreColor")
    connect(hot_color, "RGB", emissive, "HotColor")
    connect(spot_color, "RGB", emissive, "SpotColor")
    connect(emission_strength, "", emissive, "EmissionStrength")
    connect(flow_speed, "", emissive, "FlowSpeed")
    connect(activity, "", emissive, "Activity")
    connect(granulation_scale, "", emissive, "GranulationScale")
    connect(patch_scale, "", emissive, "PatchScale")
    connect(sunspot_scale, "", emissive, "SunspotScale")
    connect(limb_brightness, "", emissive, "LimbBrightness")
    connect(facula_strength, "", emissive, "FaculaStrength")
    connect_property(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log(f"Created {SURFACE_MATERIAL_PATH} as authoritative emissive photosphere material.")
    return material


def build_halo_material():
    material = create_or_replace_material(HALO_MATERIAL_PATH)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    texcoord = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -760, -160
    )
    time = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTime, -760, 20
    )
    halo_color = vector_param(material, "HaloColor", unreal.LinearColor(1.0, 0.48, 0.12, 1.0), -760, 210)
    flare_color = vector_param(material, "FlareColor", unreal.LinearColor(1.0, 0.82, 0.30, 1.0), -760, 400)
    emission = scalar_param(material, "HaloEmission", 0.42, -760, 590)
    activity = scalar_param(material, "Activity", 0.64, -760, 780)
    flow = scalar_param(material, "FlowSpeed", 0.055, -760, 970)

    code = NOISE_HLSL + r"""
float2 q = UV - 0.5;
float r = length(q) * 2.0;
float a = atan2(q.y, q.x);
float t = Time * FlowSpeed;
float circleMask = 1.0 - smoothstep(0.88, 0.98, r);
float radial = smoothstep(0.54, 0.76, r) * (1.0 - smoothstep(0.82, 0.98, r));
float softOuter = 1.0 - smoothstep(0.80, 0.98, r);
float innerCut = smoothstep(0.50, 0.82, r);
float streamerP = StarNoise.fbm(float3(cos(a) * 3.5, sin(a) * 3.5, t));
float fine = StarNoise.fbm(float3(q * 9.0, t * 0.7 + streamerP));
float rays = pow(saturate(streamerP * 0.85 + fine * 0.45), 2.4);
float alpha = circleMask * radial * innerCut * softOuter * (0.18 + rays * 0.82) * saturate(Activity * 1.25);
float3 col = lerp(HaloColor, FlareColor, saturate(rays * 0.75 + (1.0 - r) * 0.20));
return col * alpha * HaloEmission;
"""
    color = custom_expr(
        material,
        code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        ["UV", "Time", "HaloColor", "FlareColor", "HaloEmission", "Activity", "FlowSpeed"],
        -220,
        210,
        "Additive billboard corona/glare with radial streamer breakup",
    )
    connect(texcoord, "", color, "UV")
    connect(time, "", color, "Time")
    connect(halo_color, "RGB", color, "HaloColor")
    connect(flare_color, "RGB", color, "FlareColor")
    connect(emission, "", color, "HaloEmission")
    connect(activity, "", color, "Activity")
    connect(flow, "", color, "FlowSpeed")
    connect_property(color, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    alpha = custom_expr(
        material,
        r"""
float2 q = UV - 0.5;
float r = length(q) * 2.0;
return saturate(1.0 - smoothstep(0.88, 0.98, r));
""",
        unreal.CustomMaterialOutputType.CMOT_FLOAT1,
        ["UV"],
        -220,
        520,
        "Hard safety alpha to prevent the square billboard silhouette from rendering",
    )
    connect(texcoord, "", alpha, "UV")
    connect_property(alpha, unreal.MaterialProperty.MP_OPACITY)

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log(f"Created {HALO_MATERIAL_PATH} as additive procedural star halo material.")
    return material


def build_prominence_material():
    material = create_or_replace_material(PROMINENCE_MATERIAL_PATH)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    texcoord = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -860, -260
    )
    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVertexColor, -860, -80
    )
    time = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTime, -860, 100
    )
    flare_color = vector_param(material, "FlareColor", unreal.LinearColor(1.0, 0.24, 0.045, 1.0), -860, 290)
    tip_color = vector_param(material, "TipColor", unreal.LinearColor(1.0, 0.78, 0.28, 1.0), -860, 480)
    emission = scalar_param(material, "FlareEmission", 1.05, -860, 670)
    activity = scalar_param(material, "Activity", 0.64, -860, 860)
    seed = scalar_param(material, "SeedOffset", 0.0, -860, 1050)

    code = NOISE_HLSL + r"""
float u = saturate(UV.x);
float v = abs(UV.y * 2.0 - 1.0);
float t = Time * 0.16 + SeedOffset;
float body = pow(saturate(1.0 - v), 1.55);
float endpointFade = smoothstep(0.02, 0.22, u) * smoothstep(0.02, 0.22, 1.0 - u);
float flow = StarNoise.fbm(float3(u * 7.0 + t, v * 2.8, SeedOffset * 0.37));
float thread = StarNoise.fbm(float3(u * 18.0 - t * 0.55, v * 5.5 + flow, SeedOffset));
float ragged = smoothstep(0.18, 0.86, flow * 0.62 + thread * 0.38);
float pulse = 0.78 + 0.22 * sin(Time * (0.52 + frac(SeedOffset) * 0.31) + SeedOffset);
float filamentA = smoothstep(0.18, 0.86, StarNoise.fbm(float3(u * 32.0 + t * 0.22, v * 9.0, SeedOffset + 4.1)));
float filamentB = smoothstep(0.24, 0.92, StarNoise.fbm(float3(u * 21.0 - t * 0.31, v * 13.0, SeedOffset + 9.7)));
float alpha = body * endpointFade * ragged * pulse * saturate(Activity * 1.35) * (0.55 + 0.45 * max(filamentA, filamentB));
float core = pow(saturate(1.0 - v), 4.0);
float3 color = lerp(FlareColor, TipColor, saturate(core * 0.55 + thread * 0.45));
color = lerp(color, max(VertexColor.rgb, 0.0), 0.25);
return color * alpha * FlareEmission;
"""
    color = custom_expr(
        material,
        code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        ["UV", "VertexColor", "Time", "FlareColor", "TipColor", "FlareEmission", "Activity", "SeedOffset"],
        -260,
        260,
        "Additive animated prominence and coronal ejection ribbon",
    )
    connect(texcoord, "", color, "UV")
    connect(vertex_color, "", color, "VertexColor")
    connect(time, "", color, "Time")
    connect(flare_color, "RGB", color, "FlareColor")
    connect(tip_color, "RGB", color, "TipColor")
    connect(emission, "", color, "FlareEmission")
    connect(activity, "", color, "Activity")
    connect(seed, "", color, "SeedOffset")
    connect_property(color, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log(f"Created {PROMINENCE_MATERIAL_PATH} as additive animated prominence material.")
    return material


def build_core_material():
    material = create_or_replace_material(CORE_MATERIAL_PATH)
    configure_unlit_surface(material)

    core_color = vector_param(material, "CoreColor", unreal.LinearColor(1.0, 0.72, 0.22, 1.0), -520, -80)
    emission_strength = scalar_param(material, "EmissionStrength", 4.0, -520, 140)
    emissive = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -180, 20
    )

    connect(core_color, "RGB", emissive, "A")
    connect(emission_strength, "", emissive, "B")
    connect_property(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log(f"Created {CORE_MATERIAL_PATH} as constant emissive star core material.")
    return material


def build_corona_material():
    material = create_or_replace_material(CORONA_MATERIAL_PATH)
    configure_unlit_surface(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    local_pos = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionLocalPosition, -760, -220
    )
    normal_ws = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionPixelNormalWS, -760, -40
    )
    camera_ws = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionCameraVectorWS, -760, 140
    )
    time = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTime, -760, 300
    )
    halo_color = vector_param(material, "HaloColor", unreal.LinearColor(1.0, 0.52, 0.13, 1.0), -760, 330)
    flare_color = vector_param(material, "FlareColor", unreal.LinearColor(1.0, 0.82, 0.30, 1.0), -760, 520)
    emission = scalar_param(material, "CoronaEmission", 1.55, -760, 710)
    opacity_strength = scalar_param(material, "CoronaOpacity", 0.13, -760, 900)
    activity = scalar_param(material, "Activity", 0.64, -760, 1090)
    flow = scalar_param(material, "FlowSpeed", 0.045, -760, 1280)

    color_code = NOISE_HLSL + r"""
float3 n = normalize(NormalWS);
float3 v = normalize(ViewWS);
float3 p = normalize(LocalPos);
float center = saturate(abs(dot(n, v)));
float rim = pow(saturate(1.0 - center), 4.4);
float t = Time * FlowSpeed;
float a = atan2(p.y, p.z);
float latitudeBias = pow(saturate(1.0 - abs(p.x) * 0.55), 1.7);
float streamerBase = StarNoise.fbm(float3(cos(a) * 2.2 + t * 0.44, sin(a) * 2.2 - t * 0.25, p.x * 1.6));
float streamerFine = StarNoise.fbm(float3(cos(a) * 11.0 - t * 0.60, sin(a) * 11.0 + t * 0.39, p.x * 2.4));
float streamerVeins = smoothstep(0.48, 0.83, streamerBase * 0.62 + streamerFine * 0.48) * latitudeBias;
float streamer = pow(saturate(streamerVeins), 1.65) * saturate(Activity * 1.30);
float brokenLimb = rim * (0.58 + 0.42 * streamerFine);
float outer = pow(saturate(1.0 - center), 2.0) * streamer * 0.92;
float3 color = lerp(HaloColor * 0.58, FlareColor * 1.08, saturate(streamer * 0.82 + rim * 0.28));
return color * (brokenLimb * 0.68 + outer) * CoronaEmission;
"""
    opacity_code = NOISE_HLSL + r"""
float3 n = normalize(NormalWS);
float3 v = normalize(ViewWS);
float3 p = normalize(LocalPos);
float center = saturate(abs(dot(n, v)));
float rim = pow(saturate(1.0 - center), 4.2);
float t = Time * FlowSpeed;
float a = atan2(p.y, p.z);
float latitudeBias = pow(saturate(1.0 - abs(p.x) * 0.55), 1.7);
float streamerBase = StarNoise.fbm(float3(cos(a) * 2.2 + t * 0.44, sin(a) * 2.2 - t * 0.25, p.x * 1.6));
float streamerFine = StarNoise.fbm(float3(cos(a) * 11.0 - t * 0.60, sin(a) * 11.0 + t * 0.39, p.x * 2.4));
float streamer = smoothstep(0.48, 0.83, streamerBase * 0.62 + streamerFine * 0.48) * latitudeBias * saturate(Activity * 1.30);
float brokenLimb = rim * (0.52 + 0.48 * streamerFine);
return saturate((brokenLimb * 0.72 + streamer * pow(saturate(1.0 - center), 1.85) * 0.56) * CoronaOpacity);
"""
    color = custom_expr(
        material,
        color_code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        ["LocalPos", "NormalWS", "ViewWS", "Time", "HaloColor", "FlareColor", "CoronaEmission", "Activity", "FlowSpeed"],
        -220,
        40,
        "Non-occluding Fresnel corona color",
    )
    opacity = custom_expr(
        material,
        opacity_code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT1,
        ["LocalPos", "NormalWS", "ViewWS", "Time", "CoronaOpacity", "Activity", "FlowSpeed"],
        -220,
        260,
        "Non-occluding Fresnel corona opacity",
    )
    for source, output, target in [
        (local_pos, "", "LocalPos"), (normal_ws, "", "NormalWS"), (camera_ws, "", "ViewWS"), (time, "", "Time"),
        (halo_color, "RGB", "HaloColor"), (flare_color, "RGB", "FlareColor"), (emission, "", "CoronaEmission"),
        (activity, "", "Activity"), (flow, "", "FlowSpeed"),
    ]:
        connect(source, output, color, target)
    for source, output, target in [
        (local_pos, "", "LocalPos"), (normal_ws, "", "NormalWS"), (camera_ws, "", "ViewWS"), (time, "", "Time"),
        (opacity_strength, "", "CoronaOpacity"), (activity, "", "Activity"), (flow, "", "FlowSpeed"),
    ]:
        connect(source, output, opacity, target)
    connect_property(color, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    connect_property(opacity, unreal.MaterialProperty.MP_OPACITY)

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log(f"Created {CORONA_MATERIAL_PATH} as translucent non-occluding corona material.")
    return material


build_photosphere_material()
build_core_material()
build_corona_material()
build_halo_material()
build_prominence_material()
