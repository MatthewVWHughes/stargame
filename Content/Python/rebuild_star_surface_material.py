import unreal


MATERIAL_PACKAGE_PATH = "/Game/Materials/Space"
MATERIAL_NAME = "M_StarSurfaceDynamic"
MATERIAL_PATH = f"{MATERIAL_PACKAGE_PATH}/{MATERIAL_NAME}"
TEXTURE_PATH = "/Game/Textures/Space/Star/T_StarPhotosphereMasks"


def set_prop(obj, name, value):
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as exc:
        unreal.log_warning(f"{obj.get_name()}: could not set {name}: {exc}")
        return False


def node(material, cls, x, y):
    expr = unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)
    if expr is None:
        raise RuntimeError(f"Could not create {cls}")
    return expr


def connect(src, dst, dst_input, src_output=""):
    if not unreal.MaterialEditingLibrary.connect_material_expressions(src, src_output, dst, dst_input):
        raise RuntimeError(f"Could not connect {src.get_name()}:{src_output} -> {dst.get_name()}:{dst_input}")


def connect_property(src, material_property, src_output=""):
    if not unreal.MaterialEditingLibrary.connect_material_property(src, src_output, material_property):
        raise RuntimeError(f"Could not connect {src.get_name()} to {material_property}")


def make_scalar(material, name, value, x, y):
    expr = node(material, unreal.MaterialExpressionScalarParameter, x, y)
    set_prop(expr, "parameter_name", name)
    set_prop(expr, "default_value", value)
    return expr


def make_vector(material, name, value, x, y):
    expr = node(material, unreal.MaterialExpressionVectorParameter, x, y)
    set_prop(expr, "parameter_name", name)
    set_prop(expr, "default_value", value)
    return expr


def make_mask(material, source, channel, x, y):
    expr = node(material, unreal.MaterialExpressionComponentMask, x, y)
    set_prop(expr, "r", channel == "r")
    set_prop(expr, "g", channel == "g")
    set_prop(expr, "b", channel == "b")
    set_prop(expr, "a", channel == "a")
    connect(source, expr, "None")
    return expr


def create_material():
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    if material is not None:
        unreal.log(f"{MATERIAL_PATH} already exists; leaving existing expressions untouched.")
        return material

    factory = unreal.MaterialFactoryNew()
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MATERIAL_NAME,
        MATERIAL_PACKAGE_PATH,
        unreal.Material,
        factory,
    )
    if material is None:
        raise RuntimeError(f"Could not create {MATERIAL_PATH}")
    return material


texture = unreal.EditorAssetLibrary.load_asset(TEXTURE_PATH)
if texture is None:
    raise RuntimeError(f"Could not load {TEXTURE_PATH}")

material = create_material()
if unreal.MaterialEditingLibrary.get_num_material_expressions(material) > 0:
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log(f"Using existing {MATERIAL_PATH}.")
    raise SystemExit(0)

set_prop(material, "blend_mode", unreal.BlendMode.BLEND_OPAQUE)
set_prop(material, "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
set_prop(material, "two_sided", False)

core = make_vector(material, "CoreColor", unreal.LinearColor(1.0, 0.74, 0.28, 1.0), -1260, -360)
hot = make_vector(material, "HotColor", unreal.LinearColor(1.0, 0.30, 0.055, 1.0), -1260, -190)
spot = make_vector(material, "SpotColor", unreal.LinearColor(0.18, 0.035, 0.010, 1.0), -1260, -20)
emission = make_scalar(material, "EmissionStrength", 0.25, -950, 520)
activity = make_scalar(material, "Activity", 0.62, -950, 660)

time = node(material, unreal.MaterialExpressionTime, -1260, 360)
flow_scale = make_scalar(material, "FlowSpeed", 0.12, -1260, 520)
flow_time = node(material, unreal.MaterialExpressionMultiply, -990, 430)
connect(time, flow_time, "A")
connect(flow_scale, flow_time, "B")

panner_a = node(material, unreal.MaterialExpressionPanner, -760, -80)
set_prop(panner_a, "speed_x", 0.055)
set_prop(panner_a, "speed_y", 0.018)
connect(flow_time, panner_a, "Time")

panner_b = node(material, unreal.MaterialExpressionPanner, -760, 190)
set_prop(panner_b, "speed_x", -0.032)
set_prop(panner_b, "speed_y", 0.041)
connect(flow_time, panner_b, "Time")

sample_a = node(material, unreal.MaterialExpressionTextureSampleParameter2D, -520, -95)
set_prop(sample_a, "parameter_name", "PhotosphereMasks")
set_prop(sample_a, "texture", texture)
set_prop(sample_a, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
connect(panner_a, sample_a, "UVs")

sample_b = node(material, unreal.MaterialExpressionTextureSampleParameter2D, -520, 185)
set_prop(sample_b, "parameter_name", "PhotosphereMasksFine")
set_prop(sample_b, "texture", texture)
set_prop(sample_b, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
connect(panner_b, sample_b, "UVs")

granulation = make_mask(material, sample_a, "r", -260, -140)
thermal = make_mask(material, sample_a, "g", -260, 20)
spots = make_mask(material, sample_b, "b", -260, 190)

hot_core = node(material, unreal.MaterialExpressionLinearInterpolate, 20, -220)
connect(core, hot_core, "A")
connect(hot, hot_core, "B")
connect(granulation, hot_core, "Alpha")

spot_mix = node(material, unreal.MaterialExpressionLinearInterpolate, 290, -90)
connect(hot_core, spot_mix, "A")
connect(spot, spot_mix, "B")
connect(spots, spot_mix, "Alpha")

thermal_boost = node(material, unreal.MaterialExpressionLinearInterpolate, 290, 110)
connect(spot_mix, thermal_boost, "A")
connect(hot, thermal_boost, "B")
connect(thermal, thermal_boost, "Alpha")

activity_emission = node(material, unreal.MaterialExpressionMultiply, 550, 260)
connect(emission, activity_emission, "A")
connect(activity, activity_emission, "B")

final_emission = node(material, unreal.MaterialExpressionMultiply, 750, -30)
connect(thermal_boost, final_emission, "A")
connect(activity_emission, final_emission, "B")

connect_property(final_emission, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

unreal.MaterialEditingLibrary.layout_material_expressions(material)
unreal.MaterialEditingLibrary.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
unreal.log(f"Created {MATERIAL_PATH} as animated dual-flow photosphere material.")
