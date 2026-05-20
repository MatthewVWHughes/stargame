import unreal


STAR_BP = "/Game/Blueprints/Space/BP_SectorStarAnchor"
PREVIEW_MAP = "/Game/Maps/StellarClassPreview"


def component_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        name = str(library.get_variable_name(data))
        obj = library.get_object(data)
        if obj:
            yield name, obj


def mat_name(component):
    try:
        mat = component.get_material(0)
        return mat.get_path_name() if mat else None
    except Exception:
        return None


def dump_component(name, component):
    print(
        name,
        "class=", component.get_class().get_name(),
        "visible=", getattr(component, "get_editor_property")("visible") if hasattr(component, "get_editor_property") else "?",
        "hidden=", component.get_editor_property("hidden_in_game") if hasattr(component, "get_editor_property") else "?",
        "scale=", component.get_editor_property("relative_scale3d") if hasattr(component, "get_editor_property") else "?",
        "rot=", component.get_editor_property("relative_rotation") if hasattr(component, "get_editor_property") else "?",
        "mesh=", component.get_editor_property("static_mesh").get_path_name() if hasattr(component, "get_editor_property") and component.get_editor_property("static_mesh") else None,
        "mat=", mat_name(component),
    )


def main():
    bp = unreal.EditorAssetLibrary.load_asset(STAR_BP)
    print("BP", bp)
    components = dict(component_templates(bp))
    for name in sorted(components):
        if name in {"StarBody", "CoronaShell", "StarHaloBillboard", "StarHaloBillboardSCS"} or name.startswith("ProminenceLoop"):
            dump_component(name, components[name])

    if hasattr(unreal.EditorLoadingAndSavingUtils, "load_map"):
        unreal.EditorLoadingAndSavingUtils.load_map(PREVIEW_MAP)
    else:
        unreal.EditorLevelLibrary.load_level(PREVIEW_MAP)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    print("Preview actors", len(actors))
    for actor in actors:
        if actor.get_class().get_name().startswith("BP_SectorStarAnchor"):
            print("PreviewStar", actor.get_actor_label(), actor.get_actor_location(), actor.get_actor_scale3d())
            for component in actor.get_components_by_class(unreal.StaticMeshComponent):
                name = component.get_name()
                if name in {"StarBody", "CoronaShell", "StarHaloBillboard", "StarHaloBillboardSCS"} or name.startswith("ProminenceLoop"):
                    print(
                        "  runtime",
                        name,
                        "visible=", component.is_visible(),
                        "hidden=", component.bHiddenInGame,
                        "scale=", component.get_relative_scale3d(),
                        "mat=", mat_name(component),
                    )


if __name__ == "__main__":
    main()
