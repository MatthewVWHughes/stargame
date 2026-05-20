import unreal


STAR_BP = "/Game/Blueprints/Space/BP_SectorStarAnchor"
SURFACE_MATERIAL = "/Game/Materials/Space/M_StarSurface"
HALO_MATERIAL = "/Game/Materials/Space/M_StarHaloBillboard"
LEGACY_CORONA_MATERIAL = "/Game/Materials/Space/M_StarCorona"
PROMINENCE_MATERIAL = "/Game/Materials/Space/M_StarProminence"
PROMINENCE_MESH = "/Game/Meshes/Space/SM_StarProminenceBundle"


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


def build_prominence_material_depth_tested():
    material = unreal.EditorAssetLibrary.load_asset(PROMINENCE_MATERIAL)
    if not material:
        raise RuntimeError(f"Missing {PROMINENCE_MATERIAL}")

    # Keep the material additive, but let the opaque photosphere depth-buffer hide
    # back-side loops. The earlier depth-disabled setting made every loop draw
    # through the star, which looked like random geometry stuck to a ball.
    try:
        material.set_editor_property("disable_depth_test", False)
    except Exception:
        pass
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def main():
    bp = unreal.EditorAssetLibrary.load_asset(STAR_BP)
    if not bp:
        raise RuntimeError(f"Missing {STAR_BP}")

    surface = unreal.EditorAssetLibrary.load_asset(SURFACE_MATERIAL)
    halo = unreal.EditorAssetLibrary.load_asset(HALO_MATERIAL)
    legacy_corona = unreal.EditorAssetLibrary.load_asset(LEGACY_CORONA_MATERIAL)
    prominence = build_prominence_material_depth_tested()
    prominence_mesh = unreal.EditorAssetLibrary.load_asset(PROMINENCE_MESH)

    cdo = unreal.get_default_object(generated_class(bp))
    for prop_name, value in [
        ("star_surface_material", surface),
        ("star_corona_material", legacy_corona),
        ("star_halo_material", halo),
        ("star_prominence_material", prominence),
    ]:
        cdo.set_editor_property(prop_name, value)

    components = dict(component_templates(bp))
    if "StarBody" in components:
        body = components["StarBody"]
        body.set_material(0, surface)
        body.set_editor_property("relative_scale3d", unreal.Vector(18.0, 18.0, 18.0))
        body.set_editor_property("visible", True)
        body.set_editor_property("hidden_in_game", False)

    if "CoronaShell" in components:
        shell = components["CoronaShell"]
        shell.set_material(0, legacy_corona)
        shell.set_editor_property("visible", False)
        shell.set_editor_property("hidden_in_game", True)

    # After editor restart the native component exists and can use C++ camera
    # alignment. Prefer it and hide the temporary Blueprint fallback plane.
    if "StarHaloBillboard" in components:
        native_halo = components["StarHaloBillboard"]
        native_halo.set_material(0, halo)
        native_halo.set_editor_property("relative_scale3d", unreal.Vector(135.0, 135.0, 135.0))
        native_halo.set_editor_property("visible", True)
        native_halo.set_editor_property("hidden_in_game", False)
        native_halo.set_editor_property("cast_shadow", False)

    if "StarHaloBillboardSCS" in components:
        fallback = components["StarHaloBillboardSCS"]
        fallback.set_editor_property("visible", False)
        fallback.set_editor_property("hidden_in_game", True)

    # Keep prominences as a restrained limb detail. Depth test is now enabled,
    # so back-side loops should no longer draw through the body.
    visible_indices = {0, 2, 5, 7, 11, 13}
    for index in range(18):
        name = f"ProminenceLoop{index:02d}"
        component = components.get(name)
        if not component:
            continue
        if prominence_mesh:
            component.set_editor_property("static_mesh", prominence_mesh)
        component.set_material(0, prominence)
        component.set_editor_property("visible", index in visible_indices)
        component.set_editor_property("hidden_in_game", index not in visible_indices)
        component.set_editor_property("cast_shadow", False)
        if index in visible_indices:
            component.set_editor_property("relative_scale3d", unreal.Vector(0.55, 0.55, 0.55))

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    print("Sector star fixed after restart: native halo active, fallback hidden, prominence depth test restored.")


if __name__ == "__main__":
    main()
