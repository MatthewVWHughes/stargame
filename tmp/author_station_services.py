import unreal


ROOM_PATH = "/Game/Blueprints/Station/BP_StationInteriorRoom"
INTERACTABLE_CLASS_PATH = "/Game/Blueprints/Station/BP_StationInteractable.BP_StationInteractable_C"

SERVICES = {
    "RepairService": {
        "interaction": "repair",
        "display": "Repair Service",
        "location": unreal.Vector(260.0, -260.0, 20.0),
    },
    "RefuelService": {
        "interaction": "refuel",
        "display": "Refuel Service",
        "location": unreal.Vector(260.0, 260.0, 20.0),
    },
    "MarketService": {
        "interaction": "market",
        "display": "Commodity Desk",
        "location": unreal.Vector(-260.0, -260.0, 20.0),
    },
    "LaunchService": {
        "interaction": "launch",
        "display": "Airlock / Launch",
        "location": unreal.Vector(-260.0, 260.0, 20.0),
    },
}


def load_blueprint(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError(f"Failed to load {path}")
    return asset


def get_scs_node(blueprint, component_name):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        if str(library.get_variable_name(data)) == component_name:
            return library.get_object(data)
    return None


def main():
    blueprint = load_blueprint(ROOM_PATH)
    interactable_class = unreal.load_object(None, INTERACTABLE_CLASS_PATH)
    if not interactable_class:
        raise RuntimeError(f"Failed to load {INTERACTABLE_CLASS_PATH}")

    missing = []
    for component_name, service in SERVICES.items():
        template = get_scs_node(blueprint, component_name)
        if not template:
            missing.append(component_name)
            continue

        template.set_editor_property("child_actor_class", interactable_class)
        template.set_editor_property(
            "component_tags",
            [
                unreal.Name(f"InteractionType={service['interaction']}"),
                unreal.Name(f"DisplayName={service['display']}"),
            ],
        )
        template.set_editor_property("relative_location", service["location"])

    if missing:
        raise RuntimeError(f"Missing service components: {', '.join(missing)}")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    print(f"Authored station services on {ROOM_PATH}: {', '.join(SERVICES.keys())}")


if __name__ == "__main__":
    main()
