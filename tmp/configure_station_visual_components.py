import unreal


STATION_BLUEPRINTS = [
    "/Game/Blueprints/Station/BP_StationInteriorRoom",
    "/Game/Blueprints/Station/BP_StationInteractable",
    "/Game/Blueprints/Station/BP_MissionContact",
    "/Game/Blueprints/Station/BP_StationHostile",
]

TEXT = {
    "ServiceLabel": "SERVICE",
    "PromptText": "CONTACT",
    "HostileStatusText": "HOSTILE",
}

LIGHTS = {
    "RoomLight": {"intensity": 4500.0, "attenuation_radius": 950.0, "light_color": unreal.Color(199, 224, 255, 255)},
    "ServiceAreaLight": {"intensity": 2300.0, "attenuation_radius": 700.0, "light_color": unreal.Color(89, 191, 255, 255)},
    "FocusGlow": {"intensity": 900.0, "attenuation_radius": 220.0, "light_color": unreal.Color(64, 230, 255, 255)},
    "ContactGlow": {"intensity": 850.0, "attenuation_radius": 240.0, "light_color": unreal.Color(230, 217, 89, 255)},
    "HostileAlertGlow": {"intensity": 950.0, "attenuation_radius": 260.0, "light_color": unreal.Color(255, 56, 31, 255)},
}


def load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError(f"Failed to load {path}")
    return asset


def iter_component_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        name = str(library.get_variable_name(data))
        obj = library.get_object(data)
        if obj:
            yield name, obj


def configure_component(name, component):
    if name in TEXT and hasattr(component, "set_text"):
        component.set_editor_property("text", unreal.Text(TEXT[name]))
        component.set_editor_property("world_size", 32.0)
    if name in LIGHTS:
        for prop, value in LIGHTS[name].items():
            component.set_editor_property(prop, value)


def main():
    for path in STATION_BLUEPRINTS:
        blueprint = load(path)
        for name, component in iter_component_templates(blueprint):
            configure_component(name, component)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
        print(f"Configured station visual components: {path}")


if __name__ == "__main__":
    main()
