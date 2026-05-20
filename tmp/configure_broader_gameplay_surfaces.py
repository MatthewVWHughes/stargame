import unreal


BLUEPRINTS = [
    "/Game/Blueprints/Space/BP_WayfarerTrafficShip",
    "/Game/Blueprints/Space/BP_SystemEncounterActor",
]

TEXT = {
    "TrafficStateLabel": "",
}

LIGHTS = {
    "IntentGlow": {"intensity": 720.0, "attenuation_radius": 260.0, "light_color": unreal.Color(255, 176, 64, 255)},
}


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
    for path in BLUEPRINTS:
        blueprint = unreal.EditorAssetLibrary.load_asset(path)
        if not blueprint:
            raise RuntimeError(f"Failed to load {path}")
        for name, component in iter_component_templates(blueprint):
            if name in TEXT and hasattr(component, "set_text"):
                component.set_editor_property("text", unreal.Text(TEXT[name]))
                component.set_editor_property("world_size", 24.0)
            if name in LIGHTS:
                for prop, value in LIGHTS[name].items():
                    component.set_editor_property(prop, value)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
        print(f"Configured broader gameplay surface components: {path}")


if __name__ == "__main__":
    main()
