import unreal

PREVIEW_MAP = "/Game/Maps/StellarClassPreview"

unreal.EditorLoadingAndSavingUtils.load_map(PREVIEW_MAP)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in actor_subsystem.get_all_level_actors():
    if "SectorStarAnchor" not in actor.get_class().get_name() and "SectorStarAnchor" not in actor.get_name():
        continue
    unreal.log(f"STAR_ACTOR {actor.get_name()} class={actor.get_class().get_name()}")
    for prop in [
        "star_prominence_material",
        "show_prominences",
        "prominence_arc_count",
        "prominence_arc_height",
        "prominence_arc_width",
        "prominence_arc_span_degrees",
        "prominence_arc_curl",
    ]:
        try:
            unreal.log(f"PROP {prop}={actor.get_editor_property(prop)}")
        except Exception as exc:
            unreal.log_warning(f"PROP {prop} failed: {exc}")
    try:
        actor.apply_stellar_class(actor.get_editor_property("stellar_class"))
    except Exception as exc:
        unreal.log_warning(f"apply_stellar_class failed: {exc}")
    for component in actor.get_components_by_class(unreal.ActorComponent):
        name = component.get_name()
        visible = None
        hidden = None
        mesh_name = None
        try:
            visible = component.get_editor_property("visible")
        except Exception:
            pass
        try:
            hidden = component.get_editor_property("hidden_in_game")
        except Exception:
            pass
        if isinstance(component, unreal.StaticMeshComponent):
            mesh = component.get_editor_property("static_mesh")
            mesh_name = mesh.get_name() if mesh else None
        if name.startswith("Prominence") or mesh_name == "SM_StarProminenceBundle":
            unreal.log(f"COMP {name} class={component.get_class().get_name()} visible={visible} hidden={hidden} mesh={mesh_name}")

unreal.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")
