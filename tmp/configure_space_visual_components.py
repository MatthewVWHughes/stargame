import unreal


BLUEPRINTS = [
    "/Game/Blueprints/Flight/BP_WayfarerFlightPawn",
    "/Game/Blueprints/Space/BP_WayfarerTrafficShip",
    "/Game/Blueprints/Space/BP_SystemEncounterActor",
    "/Game/Blueprints/Space/BP_SystemSpacePresentation",
    "/Game/Blueprints/Space/BP_SystemEntityMarker",
    "/Game/Blueprints/Space/BP_CombatShotPresentation",
]

TEXT = {
    "MarkerLabel": "OBJECT",
}

LIGHTS = {
    "EngineGlow": {"intensity": 2200.0, "attenuation_radius": 360.0, "light_color": unreal.Color(74, 168, 255, 255)},
    "RunningLight": {"intensity": 650.0, "attenuation_radius": 180.0, "light_color": unreal.Color(74, 255, 185, 255)},
    "MarkerGlow": {"intensity": 900.0, "attenuation_radius": 320.0, "light_color": unreal.Color(124, 212, 255, 255)},
    "ShotGlow": {"intensity": 1800.0, "attenuation_radius": 650.0, "light_color": unreal.Color(255, 92, 46, 255)},
    "FillLight": {"intensity": 1200.0, "attenuation_radius": 7000.0, "light_color": unreal.Color(86, 124, 255, 255)},
    "StarGlow": {"intensity": 90000.0, "attenuation_radius": 18000.0, "light_color": unreal.Color(255, 238, 190, 255)},
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
                component.set_editor_property("world_size", 28.0)
            if name in LIGHTS:
                for prop, value in LIGHTS[name].items():
                    component.set_editor_property(prop, value)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
        print(f"Configured flight/space visual components: {path}")


if __name__ == "__main__":
    main()
