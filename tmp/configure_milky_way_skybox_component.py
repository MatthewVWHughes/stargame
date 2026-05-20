import unreal


BLUEPRINT_PATH = "/Game/Blueprints/Flight/BP_WayfarerFlightPawn"
MATERIAL_PATH = "/Game/Materials/Sky/M_MilkyWaySkybox"
MESH_PATH = "/Game/Meshes/Sky/SM_MilkyWaySkySphere"
COMPONENT_NAME = "MilkyWaySkyboxSphere"


def iter_component_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        name = str(library.get_variable_name(data))
        obj = library.get_object(data)
        if obj:
            yield name, obj


def main():
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT_PATH)
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
    if not blueprint:
        raise RuntimeError(f"Failed to load {BLUEPRINT_PATH}")
    if not material:
        raise RuntimeError(f"Failed to load {MATERIAL_PATH}")
    if not mesh:
        raise RuntimeError(f"Failed to load {MESH_PATH}")

    configured = False
    for name, component in iter_component_templates(blueprint):
        if name != COMPONENT_NAME:
            continue
        if hasattr(component, "set_material"):
            component.set_material(0, material)
        if hasattr(component, "set_static_mesh"):
            component.set_static_mesh(mesh)
        try:
            component.set_using_absolute_rotation(True)
        except Exception:
            pass
        try:
            component.set_editor_property("absolute_rotation", True)
        except Exception:
            pass
        for prop, value in [
            ("cast_shadow", False),
            ("receives_decals", False),
            ("visible_in_ray_tracing", False),
        ]:
            try:
                component.set_editor_property(prop, value)
            except Exception:
                pass
        try:
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        except Exception:
            pass
        configured = True

    if not configured:
        raise RuntimeError(f"Component {COMPONENT_NAME} not found on {BLUEPRINT_PATH}")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    print(f"Configured {COMPONENT_NAME} with {MATERIAL_PATH}")


if __name__ == "__main__":
    main()
