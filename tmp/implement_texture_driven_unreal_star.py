import math
from pathlib import Path

import unreal


STAR_BP = "/Game/Blueprints/Space/BP_SectorStarAnchor"
PREVIEW_MAP = "/Game/Maps/StellarClassPreview"

SOURCE_PHOTOSPHERE = "/home/matthew/Repos/stargame/Content/SourceArt/Space/Star/T_StarPhotosphereMasks.png"
SOURCE_CORONA = "/home/matthew/Repos/stargame/Content/SourceArt/Space/Star/T_StarCoronaMasks.png"

TEXTURE_DIR = "/Game/Textures/Space/Star"
PHOTOSPHERE_TEXTURE_NAME = "T_StarPhotosphereMasks"
CORONA_TEXTURE_NAME = "T_StarCoronaMasks"
PHOTOSPHERE_TEXTURE = f"{TEXTURE_DIR}/{PHOTOSPHERE_TEXTURE_NAME}"
CORONA_TEXTURE = f"{TEXTURE_DIR}/{CORONA_TEXTURE_NAME}"

MESH_DIR = "/Game/Meshes/Space"
SMOOTH_SPHERE_NAME = "SM_StarPhotosphereSmooth"
SMOOTH_SPHERE = f"{MESH_DIR}/{SMOOTH_SPHERE_NAME}"
SMOOTH_SPHERE_OBJ = "/tmp/stargame_star_photosphere_smooth.obj"

SURFACE_MATERIAL = "/Game/Materials/Space/M_StarSurface"
CORONA_MATERIAL = "/Game/Materials/Space/M_StarCorona"
HALO_MATERIAL = "/Game/Materials/Space/M_StarHaloBillboard"
PROMINENCE_MATERIAL = "/Game/Materials/Space/M_StarProminence"


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def import_texture(source, name):
    if not Path(source).exists():
        raise RuntimeError(f"Missing source texture {source}")
    ensure_directory(TEXTURE_DIR)
    asset_path = f"{TEXTURE_DIR}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", TEXTURE_DIR)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not texture:
        raise RuntimeError(f"Failed to import texture {asset_path}")
    texture.set_editor_property("srgb", False)
    try:
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    except Exception:
        pass
    try:
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    except Exception:
        pass
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def generate_smooth_sphere_obj():
    lat_segments = 96
    lon_segments = 192
    radius = 50.0
    with open(SMOOTH_SPHERE_OBJ, "w", encoding="utf-8") as obj:
        obj.write("# Stargame smooth high-resolution star sphere\n")
        for lat in range(lat_segments + 1):
            v = lat / lat_segments
            theta = math.pi * v
            sin_theta = math.sin(theta)
            cos_theta = math.cos(theta)
            for lon in range(lon_segments + 1):
                u = lon / lon_segments
                phi = math.tau * u
                x = math.cos(phi) * sin_theta
                y = math.sin(phi) * sin_theta
                z = cos_theta
                obj.write(f"v {x * radius:.5f} {y * radius:.5f} {z * radius:.5f}\n")
        for lat in range(lat_segments + 1):
            v = lat / lat_segments
            for lon in range(lon_segments + 1):
                u = lon / lon_segments
                obj.write(f"vt {u:.6f} {1.0 - v:.6f}\n")
        for lat in range(lat_segments + 1):
            v = lat / lat_segments
            theta = math.pi * v
            sin_theta = math.sin(theta)
            cos_theta = math.cos(theta)
            for lon in range(lon_segments + 1):
                u = lon / lon_segments
                phi = math.tau * u
                x = math.cos(phi) * sin_theta
                y = math.sin(phi) * sin_theta
                z = cos_theta
                obj.write(f"vn {x:.6f} {y:.6f} {z:.6f}\n")
        obj.write("s 1\n")
        row = lon_segments + 1
        for lat in range(lat_segments):
            for lon in range(lon_segments):
                a = lat * row + lon + 1
                b = a + 1
                c = a + row
                d = c + 1
                if lat != 0:
                    obj.write(f"f {a}/{a}/{a} {c}/{c}/{c} {b}/{b}/{b}\n")
                if lat != lat_segments - 1:
                    obj.write(f"f {b}/{b}/{b} {c}/{c}/{c} {d}/{d}/{d}\n")


def import_sphere_mesh():
    generate_smooth_sphere_obj()
    ensure_directory(MESH_DIR)
    if unreal.EditorAssetLibrary.does_asset_exist(SMOOTH_SPHERE):
        unreal.EditorAssetLibrary.delete_asset(SMOOTH_SPHERE)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", SMOOTH_SPHERE_OBJ)
    task.set_editor_property("destination_path", MESH_DIR)
    task.set_editor_property("destination_name", SMOOTH_SPHERE_NAME)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.EditorAssetLibrary.load_asset(SMOOTH_SPHERE)
    if not mesh:
        raise RuntimeError(f"Failed to import {SMOOTH_SPHERE}")
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return mesh


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
    expr = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    expr.set_editor_property("parameter_name", name)
    expr.set_editor_property("default_value", value)
    return expr


def scalar_param(material, name, value, x, y):
    expr = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    expr.set_editor_property("parameter_name", name)
    expr.set_editor_property("default_value", value)
    return expr


def texture_sample(material, name, texture, x, y):
    expr = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    expr.set_editor_property("parameter_name", name)
    expr.set_editor_property("texture", texture)
    try:
        expr.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    except Exception:
        pass
    return expr


def panner(material, speed_x, speed_y, x, y):
    expr = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionPanner, x, y)
    for prop, value in [("speed_x", speed_x), ("speed_y", speed_y)]:
        try:
            expr.set_editor_property(prop, value)
        except Exception:
            pass
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


def build_surface_material(texture):
    material = create_or_replace_material(SURFACE_MATERIAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", False)

    uv = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1280, -280)
    pan = panner(material, 0.012, 0.0, -1040, -280)
    sample = texture_sample(material, "PhotosphereMasks", texture, -800, -240)
    normal = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionPixelNormalWS, -800, 20)
    view = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -800, 200)
    core = vector_param(material, "CoreColor", unreal.LinearColor(1.0, 0.82, 0.34, 1.0), -800, 380)
    hot = vector_param(material, "HotColor", unreal.LinearColor(1.0, 0.48, 0.09, 1.0), -800, 560)
    spot = vector_param(material, "SpotColor", unreal.LinearColor(0.18, 0.04, 0.01, 1.0), -800, 740)
    emission = scalar_param(material, "EmissionStrength", 5.2, -800, 920)
    code = r"""
float heat = saturate(Mask.r);
float hotMask = saturate(Mask.g);
float spotMask = saturate(Mask.b);
float3 n = normalize(N);
float3 v = normalize(V);
float ndv = saturate(abs(dot(n, -v)));
float limb = lerp(0.38, 1.18, pow(ndv, 0.36));
float3 col = lerp(CoreColor, HotColor, saturate(heat * 0.85 + hotMask * 0.75));
col = lerp(col, SpotColor, spotMask * 0.82);
float micro = hotMask * 0.55 + smoothstep(0.56, 0.92, heat) * 0.25;
return col * (limb + micro) * EmissionStrength;
"""
    expr = custom_expr(
        material,
        code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        ["Mask", "N", "V", "CoreColor", "HotColor", "SpotColor", "EmissionStrength"],
        -340,
        140,
        "Texture-driven stellar photosphere",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(uv, "", pan, "Coordinate")
    unreal.MaterialEditingLibrary.connect_material_expressions(pan, "", sample, "Coordinates")
    for source, output, target in [
        (sample, "RGB", "Mask"),
        (normal, "", "N"),
        (view, "", "V"),
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


def build_corona_material(texture):
    material = create_or_replace_material(CORONA_MATERIAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    try:
        material.set_editor_property("disable_depth_test", False)
    except Exception:
        pass

    uv = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1280, -220)
    pan = panner(material, -0.006, 0.002, -1040, -220)
    sample = texture_sample(material, "CoronaMasks", texture, -800, -200)
    normal = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionPixelNormalWS, -800, 40)
    view = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -800, 220)
    halo = vector_param(material, "HaloColor", unreal.LinearColor(1.0, 0.58, 0.16, 1.0), -800, 400)
    flare = vector_param(material, "FlareColor", unreal.LinearColor(1.0, 0.30, 0.05, 1.0), -800, 580)
    emission = scalar_param(material, "HaloEmission", 1.15, -800, 760)
    opacity = scalar_param(material, "CoronaOpacity", 0.16, -800, 940)

    code = r"""
float3 n = normalize(N);
float3 v = normalize(V);
float rim = 1.0 - saturate(abs(dot(n, -v)));
float softRim = pow(rim, 2.15);
float ray = saturate(Mask.r * 0.75 + Mask.g * 0.55);
float shell = softRim * (0.55 + ray * 0.85);
float3 col = lerp(HaloColor, FlareColor, saturate(ray * 0.8 + softRim * 0.15));
return col * shell * HaloEmission;
"""
    color_expr = custom_expr(
        material,
        code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        ["Mask", "N", "V", "HaloColor", "FlareColor", "HaloEmission"],
        -340,
        80,
        "Texture-driven integrated stellar corona",
    )
    alpha_code = r"""
float3 n = normalize(N);
float3 v = normalize(V);
float rim = 1.0 - saturate(abs(dot(n, -v)));
return saturate(pow(rim, 2.25) * (0.55 + Mask.r * 0.75) * CoronaOpacity);
"""
    alpha_expr = custom_expr(
        material,
        alpha_code,
        unreal.CustomMaterialOutputType.CMOT_FLOAT1,
        ["Mask", "N", "V", "CoronaOpacity"],
        -340,
        420,
        "Corona shell opacity",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(uv, "", pan, "Coordinate")
    unreal.MaterialEditingLibrary.connect_material_expressions(pan, "", sample, "Coordinates")
    for expr in [color_expr, alpha_expr]:
        for source, output, target in [
            (sample, "RGB", "Mask"),
            (normal, "", "N"),
            (view, "", "V"),
        ]:
            unreal.MaterialEditingLibrary.connect_material_expressions(source, output, expr, target)
    for source, output, target in [
        (halo, "RGB", "HaloColor"),
        (flare, "RGB", "FlareColor"),
        (emission, "", "HaloEmission"),
    ]:
        unreal.MaterialEditingLibrary.connect_material_expressions(source, output, color_expr, target)
    unreal.MaterialEditingLibrary.connect_material_expressions(opacity, "", alpha_expr, "CoronaOpacity")
    unreal.MaterialEditingLibrary.connect_material_property(color_expr, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(alpha_expr, "", unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def build_hidden_black_material(path):
    material = create_or_replace_material(path)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    zero = scalar_param(material, "DisabledEmission", 0.0, -280, -60)
    unreal.MaterialEditingLibrary.connect_material_property(zero, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(zero, "", unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def set_component_disallow_nanite(component, value):
    for prop in ["disallow_nanite", "b_disallow_nanite", "bDisallowNanite"]:
        try:
            component.set_editor_property(prop, value)
            return
        except Exception:
            pass


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


def configure_star_blueprint(mesh, surface, corona):
    bp = unreal.EditorAssetLibrary.load_asset(STAR_BP)
    if not bp:
        raise RuntimeError(f"Missing {STAR_BP}")
    cdo = unreal.get_default_object(generated_class(bp))
    cdo.set_editor_property("star_surface_material", surface)
    cdo.set_editor_property("star_corona_material", corona)
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
            set_component_disallow_nanite(component, False)
        elif name == "CoronaShell":
            component.set_editor_property("static_mesh", mesh)
            component.set_material(0, corona)
            component.set_editor_property("relative_scale3d", unreal.Vector(20.8, 20.8, 20.8))
            component.set_editor_property("visible", True)
            component.set_editor_property("hidden_in_game", False)
            component.set_editor_property("cast_shadow", False)
            set_component_disallow_nanite(component, True)
        elif name in {"StarHaloBillboard", "StarHaloBillboardSCS"} or name.startswith("ProminenceLoop"):
            component.set_editor_property("visible", False)
            component.set_editor_property("hidden_in_game", True)
            set_component_disallow_nanite(component, True)

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)


def configure_preview_map():
    if hasattr(unreal.EditorLoadingAndSavingUtils, "load_map"):
        unreal.EditorLoadingAndSavingUtils.load_map(PREVIEW_MAP)
    else:
        unreal.EditorLevelLibrary.load_level(PREVIEW_MAP)
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = subsystem.get_editor_world() if subsystem else None
    if world:
        settings = world.get_world_settings()
        game_mode = unreal.load_class(None, "/Script/Engine.GameModeBase")
        if game_mode:
            settings.set_editor_property("default_game_mode", game_mode)

    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_class().get_name() == "PostProcessVolume":
            pp = actor.get_component_by_class(unreal.PostProcessComponent)
            if not pp:
                continue
            pp.set_editor_property("unbound", True)
            settings = pp.get_editor_property("settings")
            for attr, value in [
                ("override_bloom_intensity", True),
                ("bloom_intensity", 1.8),
                ("override_bloom_threshold", True),
                ("bloom_threshold", 0.18),
                ("override_auto_exposure_method", True),
                ("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL),
                ("override_auto_exposure_bias", True),
                ("auto_exposure_bias", -0.55),
            ]:
                try:
                    settings.set_editor_property(attr, value)
                except Exception:
                    pass
            pp.set_editor_property("settings", settings)
    unreal.EditorLevelLibrary.save_current_level()


def main():
    photosphere_tex = import_texture(SOURCE_PHOTOSPHERE, PHOTOSPHERE_TEXTURE_NAME)
    corona_tex = import_texture(SOURCE_CORONA, CORONA_TEXTURE_NAME)
    mesh = import_sphere_mesh()
    surface = build_surface_material(photosphere_tex)
    corona = build_corona_material(corona_tex)
    build_hidden_black_material(HALO_MATERIAL)
    build_hidden_black_material(PROMINENCE_MATERIAL)
    configure_star_blueprint(mesh, surface, corona)
    configure_preview_map()
    print("Implemented texture-driven Unreal star: smooth mesh, emissive photosphere texture, integrated corona, bloom/exposure tuning.")


if __name__ == "__main__":
    main()
