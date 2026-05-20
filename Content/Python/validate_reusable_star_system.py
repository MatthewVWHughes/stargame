import unreal


ASSETS = [
    "/Game/Materials/Space/M_StarPhotosphereProcedural",
    "/Game/Materials/Space/M_StarCoronaProcedural",
    "/Game/Materials/Space/M_StarProminence",
    "/Game/Blueprints/Space/BP_SectorStarAnchor",
    "/Game/Maps/StellarClassPreview",
]


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


def component_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        name = str(library.get_variable_name(data))
        obj = library.get_object(data)
        if obj:
            yield name, obj


def assert_true(condition, message):
    if not condition:
        raise RuntimeError(message)


def material_name(component):
    material = component.get_material(0)
    return material.get_name() if material else ""


def main():
    missing = [path for path in ASSETS if not unreal.EditorAssetLibrary.does_asset_exist(path)]
    assert_true(not missing, f"Missing star assets: {missing}")

    bp = unreal.EditorAssetLibrary.load_asset("/Game/Blueprints/Space/BP_SectorStarAnchor")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    components = dict(component_templates(bp))

    assert_true("StarBody" in components, "BP_SectorStarAnchor missing StarBody")
    assert_true("CoronaShell" in components, "BP_SectorStarAnchor missing CoronaShell")
    assert_true("M_StarPhotosphereProcedural" in material_name(components["StarBody"]), "StarBody is not using procedural photosphere material")
    assert_true("M_StarCoronaProcedural" in material_name(components["CoronaShell"]), "CoronaShell is not using procedural corona material")

    for name in ["StarHaloBillboard", "EjectionBillboardA", "EjectionBillboardB", "EjectionBillboardC", "EjectionBillboardD"]:
        component = components.get(name)
        if component:
            assert_true(not component.get_editor_property("visible"), f"{name} should be hidden")

    if hasattr(unreal.EditorLoadingAndSavingUtils, "load_map"):
        unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/StellarClassPreview")
    else:
        unreal.EditorLevelLibrary.load_level("/Game/Maps/StellarClassPreview")

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors()
    stars = [
        actor for actor in actors
        if "SectorStarAnchor" in actor.get_class().get_name() or "SectorStarAnchor" in actor.get_name()
    ]
    assert_true(len(stars) == 1, f"Expected exactly one preview star, found {len(stars)}")
    star = stars[0]
    assert_true(star.get_editor_property("StellarClass") == unreal.StargameStellarClass.G, "Preview star should be G class")
    assert_true(star.get_editor_property("bShowCorona"), "Preview star corona is disabled")
    assert_true(star.get_editor_property("bShowProminences"), "Preview star prominences are disabled")
    assert_true(not star.get_editor_property("bShowEjections"), "Legacy ejection billboards should be disabled")
    assert_true(not star.get_editor_property("bShowHaloBillboard"), "Legacy halo billboard should be disabled")
    arc_components = [
        component for component in star.get_components_by_class(unreal.ProceduralMeshComponent)
        if component.get_name().startswith("ProminenceArc")
    ]
    assert_true(len(arc_components) >= 7, f"Expected native prominence arc components, found {len(arc_components)}")

    camera_components = [
        component for component in star.get_components_by_class(unreal.CameraComponent)
        if component.get_name() == "PreviewCamera"
    ]
    assert_true(camera_components, "Preview star missing PreviewCamera")
    settings = camera_components[0].get_editor_property("post_process_settings")
    assert_true(settings.get_editor_property("override_auto_exposure_method"), "Manual exposure override is disabled")
    assert_true(settings.get_editor_property("auto_exposure_method") == unreal.AutoExposureMethod.AEM_MANUAL, "Preview exposure is not manual")
    assert_true(settings.get_editor_property("override_bloom_intensity"), "Bloom intensity override is disabled")
    assert_true(settings.get_editor_property("bloom_intensity") <= 0.35, "Preview bloom intensity is too high for surface inspection")

    print("Reusable star system validation passed: one G-class preview star, procedural photosphere/corona, anchored prominences, controlled exposure.")


if __name__ == "__main__":
    main()
