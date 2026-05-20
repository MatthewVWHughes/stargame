import unreal


STAR_BP_DIR = "/Game/Blueprints/Space"
STAR_BP_NAME = "BP_SectorStarAnchor"
STAR_BP_PATH = f"{STAR_BP_DIR}/{STAR_BP_NAME}"
SURFACE_MATERIAL_PATH = "/Game/Materials/Space/M_StarSurface"
CORONA_MATERIAL_PATH = "/Game/Materials/Space/M_StarCorona"
PREVIEW_MAP_PATH = "/Game/Maps/StellarClassPreview"
SYSTEM_PRESENTATION_BP = "/Game/Blueprints/Space/BP_SystemSpacePresentation"


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


def constant(material, value, x, y):
    expr = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, x, y
    )
    expr.set_editor_property("r", value)
    return expr


def multiply(material, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, x, y
    )


def add(material, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, x, y
    )


def build_surface_material():
    material = create_or_replace_material(SURFACE_MATERIAL_PATH)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", False)

    core = vector_param(material, "CoreColor", unreal.LinearColor(1.0, 0.85, 0.40, 1.0), -760, -160)
    hot = vector_param(material, "HotColor", unreal.LinearColor(1.0, 0.50, 0.10, 1.0), -760, 40)
    emission = scalar_param(material, "EmissionStrength", 3.8, -760, 240)
    rim_power = constant(material, 0.65, -520, 180)

    fresnel = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionFresnel, -520, 40
    )
    try:
        fresnel.set_editor_property("exponent", 2.6)
    except Exception:
        pass

    hot_rim = multiply(material, -300, 40)
    hot_rim_strength = multiply(material, -120, 80)
    base_plus_rim = add(material, 80, -80)
    final_emissive = multiply(material, 300, -40)

    unreal.MaterialEditingLibrary.connect_material_expressions(hot, "RGB", hot_rim, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(fresnel, "", hot_rim, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(hot_rim, "", hot_rim_strength, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(rim_power, "", hot_rim_strength, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(core, "RGB", base_plus_rim, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(hot_rim_strength, "", base_plus_rim, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(base_plus_rim, "", final_emissive, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(emission, "", final_emissive, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        final_emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def build_corona_material():
    material = create_or_replace_material(CORONA_MATERIAL_PATH)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    halo = vector_param(material, "HaloColor", unreal.LinearColor(1.0, 0.78, 0.30, 1.0), -760, -140)
    flare = vector_param(material, "FlareColor", unreal.LinearColor(1.0, 0.38, 0.08, 1.0), -760, 40)
    emission = scalar_param(material, "HaloEmission", 1.8, -760, 240)

    fresnel = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionFresnel, -520, 20
    )
    try:
        fresnel.set_editor_property("exponent", 4.0)
    except Exception:
        pass

    flare_rim = multiply(material, -300, 40)
    color_mix = add(material, -80, -80)
    final_emissive = multiply(material, 140, -40)
    opacity = multiply(material, 140, 160)

    unreal.MaterialEditingLibrary.connect_material_expressions(flare, "RGB", flare_rim, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(fresnel, "", flare_rim, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(halo, "RGB", color_mix, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(flare_rim, "", color_mix, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(color_mix, "", final_emissive, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(emission, "", final_emissive, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        final_emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(fresnel, "", opacity, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(emission, "", opacity, "B")
    unreal.MaterialEditingLibrary.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def create_star_blueprint(surface_material, corona_material):
    ensure_directory(STAR_BP_DIR)
    bp = unreal.EditorAssetLibrary.load_asset(STAR_BP_PATH)
    if not bp:
        parent = unreal.load_class(None, "/Script/Stargame.SectorStarAnchorActor")
        if not parent:
            raise RuntimeError(
                "SectorStarAnchorActor class is not loaded. Restart the editor after the C++ build, then rerun this script."
            )
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            STAR_BP_NAME,
            STAR_BP_DIR,
            unreal.Blueprint,
            factory,
        )
    if not bp:
        raise RuntimeError(f"Failed to create/load {STAR_BP_PATH}")

    generated_class = get_blueprint_generated_class(bp)
    cdo = unreal.get_default_object(generated_class)
    cdo.set_editor_property("star_surface_material", surface_material)
    cdo.set_editor_property("star_corona_material", corona_material)
    try:
        cdo.set_editor_property("stellar_class", unreal.EStargameStellarClass.G)
    except Exception:
        pass

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return bp


def get_blueprint_generated_class(blueprint):
    generated_class = None
    try:
        generated_class = blueprint.get_editor_property("generated_class")
    except Exception:
        generated_class = None
    if not generated_class and hasattr(blueprint, "generated_class"):
        candidate = getattr(blueprint, "generated_class")
        generated_class = candidate() if callable(candidate) else candidate
    if not generated_class:
        asset_path = blueprint.get_path_name()
        generated_class = unreal.load_class(None, f"{asset_path}_C")
    if not generated_class:
        raise RuntimeError(f"Could not resolve generated class for {blueprint.get_path_name()}")
    return generated_class


def component_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        name = str(library.get_variable_name(data))
        obj = library.get_object(data)
        if obj:
            yield name, obj


def configure_system_presentation_child(star_bp):
    bp = unreal.EditorAssetLibrary.load_asset(SYSTEM_PRESENTATION_BP)
    if not bp:
        raise RuntimeError(f"Missing {SYSTEM_PRESENTATION_BP}")

    for name, component in component_templates(bp):
        if name == "SectorStarAnchor":
            component.set_editor_property("child_actor_class", get_blueprint_generated_class(star_bp))
            component.set_editor_property("relative_location", unreal.Vector(-420000.0, -160000.0, 90000.0))
            component.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 0.0))
            component.set_editor_property("relative_scale3d", unreal.Vector(10.0, 10.0, 10.0))
        elif name == "DistantStarDisc":
            component.set_editor_property("visible", False)
            component.set_editor_property("hidden_in_game", True)
        elif name == "StarGlow":
            component.set_editor_property("intensity", 0.0)
            component.set_editor_property("visible", False)

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)


def remove_map_directional_lights(level_path):
    if not unreal.EditorAssetLibrary.does_asset_exist(level_path):
        return
    unreal.EditorLevelLibrary.load_level(level_path)
    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        if actor.get_class().get_name() == "DirectionalLight":
            unreal.EditorLevelLibrary.destroy_actor(actor)
    unreal.EditorLevelLibrary.save_current_level()


def create_preview_map(star_bp, surface_material, corona_material):
    if unreal.EditorAssetLibrary.does_asset_exist(PREVIEW_MAP_PATH):
        unreal.EditorLevelLibrary.load_level(PREVIEW_MAP_PATH)
    else:
        unreal.EditorLevelLibrary.new_level(PREVIEW_MAP_PATH)

    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        unreal.EditorLevelLibrary.destroy_actor(actor)

    star_class = get_blueprint_generated_class(star_bp)
    star = unreal.EditorLevelLibrary.spawn_actor_from_class(
        star_class,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    star.set_actor_label("Clickable_Stellar_Class_Preview")
    star.set_editor_property("star_surface_material", surface_material)
    star.set_editor_property("star_corona_material", corona_material)
    star.set_editor_property("enable_preview_controls", True)
    star.set_editor_property("use_preview_camera", True)
    try:
        star.set_editor_property("stellar_class", unreal.EStargameStellarClass.G)
    except Exception:
        pass

    text = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.TextRenderActor,
        unreal.Vector(0.0, -5200.0, -2300.0),
        unreal.Rotator(15.0, 90.0, 0.0),
    )
    text.set_actor_label("PreviewControlsLabel")
    text_component = text.get_component_by_class(unreal.TextRenderComponent)
    text_component.set_editor_property("text", unreal.Text("Left click / Space / Right Arrow: next    Right click / Left Arrow: previous"))
    text_component.set_editor_property("world_size", 220.0)
    text_component.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
    text_component.set_editor_property("text_render_color", unreal.Color(210, 220, 235, 255))

    skylight = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyLight,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    skylight.set_actor_label("PreviewSkyLight")
    sky_component = skylight.get_component_by_class(unreal.SkyLightComponent)
    if sky_component:
        sky_component.set_editor_property("intensity", 0.02)

    world = unreal.EditorLevelLibrary.get_editor_world()
    if world:
        settings = world.get_world_settings()
        settings.set_editor_property("default_game_mode", None)

    unreal.EditorLevelLibrary.save_current_level()


def main():
    surface = build_surface_material()
    corona = build_corona_material()
    star_bp = create_star_blueprint(surface, corona)
    configure_system_presentation_child(star_bp)
    remove_map_directional_lights("/Game/Maps/StargamePlayableSlice")
    create_preview_map(star_bp, surface, corona)
    print(f"Sector star anchor authored: {STAR_BP_PATH}")
    print(f"Preview map authored: {PREVIEW_MAP_PATH}")


if __name__ == "__main__":
    main()
