import unreal


ASSETS = [
    "/Game/Materials/Space/M_StarSurface",
    "/Game/Materials/Space/M_StarCorona",
    "/Game/Materials/Space/M_StarHaloBillboard",
    "/Game/Materials/Space/M_StarProminence",
    "/Game/Textures/Space/Star/T_StarPhotosphereMasks",
    "/Game/Textures/Space/Star/T_StarCoronaMasks",
    "/Game/Meshes/Space/SM_StarPhotosphereSmooth",
    "/Game/Meshes/Space/SM_StarProminenceBundle",
    "/Game/Blueprints/Space/BP_SectorStarAnchor",
    "/Game/Blueprints/Space/BP_SystemSpacePresentation",
    "/Game/Maps/StellarClassPreview",
]


def component_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        name = str(library.get_variable_name(data))
        obj = library.get_object(data)
        if obj:
            yield name, obj


def main():
    missing = [path for path in ASSETS if not unreal.EditorAssetLibrary.does_asset_exist(path)]
    if missing:
        raise RuntimeError(f"Missing sector star assets: {missing}")

    for path in ["/Game/Blueprints/Space/BP_SectorStarAnchor", "/Game/Blueprints/Space/BP_SystemSpacePresentation"]:
        bp = unreal.EditorAssetLibrary.load_asset(path)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp)

    presentation = unreal.EditorAssetLibrary.load_asset("/Game/Blueprints/Space/BP_SystemSpacePresentation")
    components = dict(component_templates(presentation))
    sector_anchor = components.get("SectorStarAnchor")
    if not sector_anchor:
        raise RuntimeError("BP_SystemSpacePresentation is missing SectorStarAnchor component")
    child_class = sector_anchor.get_editor_property("child_actor_class")
    if not child_class or "BP_SectorStarAnchor" not in child_class.get_name():
        raise RuntimeError(f"SectorStarAnchor child class is not BP_SectorStarAnchor: {child_class}")

    star_bp = unreal.EditorAssetLibrary.load_asset("/Game/Blueprints/Space/BP_SectorStarAnchor")
    star_components = dict(component_templates(star_bp))
    body = star_components.get("StarBody")
    if not body:
        raise RuntimeError("BP_SectorStarAnchor is missing StarBody")
    body_mesh = body.get_editor_property("static_mesh")
    if not body_mesh or "SM_StarPhotosphereSmooth" not in body_mesh.get_name():
        raise RuntimeError(f"StarBody must use smooth photosphere mesh, found {body_mesh}")
    body_material = body.get_material(0)
    if not body_material or "M_StarSurface" not in body_material.get_name():
        raise RuntimeError(f"StarBody must use M_StarSurface, found {body_material}")

    if "CoronaShell" in star_components:
        corona = star_components["CoronaShell"]
        if not corona.get_editor_property("visible") or corona.get_editor_property("hidden_in_game"):
            raise RuntimeError("CoronaShell should be visible as the integrated corona shell")
        corona_material = corona.get_material(0)
        if not corona_material or "M_StarCorona" not in corona_material.get_name():
            raise RuntimeError(f"CoronaShell must use M_StarCorona, found {corona_material}")

    for name in ["StarHaloBillboard", "StarHaloBillboardSCS"]:
        component = star_components.get(name)
        if component and (component.get_editor_property("visible") or not component.get_editor_property("hidden_in_game")):
            raise RuntimeError(f"{name} should be hidden in the reset retry")

    prominence_count = sum(1 for name in star_components if name.startswith("ProminenceLoop"))
    if prominence_count < 18:
        raise RuntimeError(f"Expected at least 18 prominence loop components, found {prominence_count}")
    visible_prominence_count = sum(
        1
        for name, component in star_components.items()
        if name.startswith("ProminenceLoop")
        and component.get_editor_property("visible")
        and not component.get_editor_property("hidden_in_game")
    )
    if visible_prominence_count:
        raise RuntimeError(f"Prominence loops should be hidden in the reset retry, found {visible_prominence_count} visible")

    unreal.EditorLevelLibrary.load_level("/Game/Maps/StellarClassPreview")
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    preview_star_count = sum(1 for actor in actors if actor.get_class().get_name().startswith("BP_SectorStarAnchor"))
    if preview_star_count < 9:
        raise RuntimeError(f"Expected focused star plus class row, found {preview_star_count} star actors")

    post_process_count = sum(1 for actor in actors if actor.get_class().get_name() == "PostProcessVolume")
    if post_process_count < 1:
        raise RuntimeError("Preview map is missing a PostProcessVolume for exposure/bloom validation")

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = editor_subsystem.get_editor_world() if editor_subsystem else None
    if world:
        settings = world.get_world_settings()
        game_mode = settings.get_editor_property("default_game_mode")
        if not game_mode or game_mode.get_name() != "GameModeBase":
            raise RuntimeError(f"Preview map must override game mode to GameModeBase, found {game_mode}")

    directional_lights = [actor.get_name() for actor in actors if actor.get_class().get_name() == "DirectionalLight"]
    if directional_lights:
        raise RuntimeError(f"Preview map has stray DirectionalLight actors: {directional_lights}")

    print("Sector star assets verified.")


if __name__ == "__main__":
    main()
