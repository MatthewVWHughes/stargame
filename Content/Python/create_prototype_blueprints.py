import unreal


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def load_class(path):
    loaded = unreal.load_class(None, path)
    if loaded is None:
        raise RuntimeError(f"Could not load class: {path}")
    return loaded


def create_or_load_blueprint(asset_path, asset_name, parent_class):
    full_path = f"{asset_path}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        existing = unreal.EditorAssetLibrary.load_asset(full_path)
        if existing:
            return existing

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    blueprint = asset_tools.create_asset(asset_name, asset_path, unreal.Blueprint, factory)
    if blueprint is None:
        raise RuntimeError(f"Could not create Blueprint: {full_path}")
    return blueprint


def create_or_load_material_instance(asset_path, asset_name, parent_material):
    full_path = f"{asset_path}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        existing = unreal.EditorAssetLibrary.load_asset(full_path)
        if existing:
            return existing

    factory = unreal.MaterialInstanceConstantFactoryNew()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material_instance = asset_tools.create_asset(asset_name, asset_path, unreal.MaterialInstanceConstant, factory)
    if material_instance is None:
        raise RuntimeError(f"Could not create material instance: {full_path}")

    material_instance.set_editor_property("parent", parent_material)
    return material_instance


def set_default(blueprint, property_name, value):
    generated_class = unreal.BlueprintEditorLibrary.generated_class(blueprint)
    default_object = unreal.get_default_object(generated_class)
    default_object.set_editor_property(property_name, value)


def main():
    ensure_directory("/Game/Blueprints")
    ensure_directory("/Game/Blueprints/Core")
    ensure_directory("/Game/Blueprints/Environment")
    ensure_directory("/Game/Materials")
    ensure_directory("/Game/Materials/Environment")

    planet_parent = load_class("/Script/Stargame.GasGiantPlanetActor")
    game_mode_parent = load_class("/Script/Stargame.StargameGameModeBase")
    hud_class = load_class("/Script/Stargame.PrototypeFlightHud")
    planet_bp = create_or_load_blueprint(
        "/Game/Blueprints/Environment",
        "BP_GasGiantPlanet",
        planet_parent,
    )
    game_mode_bp = create_or_load_blueprint(
        "/Game/Blueprints/Core",
        "BP_StargameGameMode",
        game_mode_parent,
    )
    unreal.BlueprintEditorLibrary.compile_blueprint(planet_bp)
    unreal.BlueprintEditorLibrary.compile_blueprint(game_mode_bp)

    planet_class = unreal.EditorAssetLibrary.load_blueprint_class(
        "/Game/Blueprints/Environment/BP_GasGiantPlanet"
    )

    set_default(game_mode_bp, "PrototypeGasGiantClass", planet_class)
    set_default(game_mode_bp, "HUDClass", hud_class)
    set_default(game_mode_bp, "PrototypeGasGiantLocation", unreal.Vector(8000000.0, 0.0, 0.0))
    set_default(game_mode_bp, "bSpawnPrototypeLocalFog", False)
    set_default(game_mode_bp, "bSpawnPrototypeVolumetricCloud", True)
    set_default(planet_bp, "VisualRadius", 5000000.0)
    set_default(planet_bp, "AtmosphereEntryAltitude", 4500000.0)
    set_default(planet_bp, "DenseLayerTopAltitude", 1500000.0)
    set_default(planet_bp, "DeepLayerTopAltitude", 500000.0)
    set_default(planet_bp, "CrushDepthStartAltitude", -400000.0)
    set_default(planet_bp, "CrushDepthFatalAltitude", -1500000.0)
    set_default(planet_bp, "PlanetCoreColor", unreal.LinearColor(0.035, 0.16, 0.045, 1.0))
    set_default(planet_bp, "bEnableDebugCoreCollision", False)
    set_default(planet_bp, "bSpawnCloudField", False)

    unreal.BlueprintEditorLibrary.compile_blueprint(planet_bp)
    unreal.BlueprintEditorLibrary.compile_blueprint(game_mode_bp)

    for asset in (planet_bp, game_mode_bp):
        unreal.EditorAssetLibrary.save_loaded_asset(asset)

    unreal.EditorAssetLibrary.save_directory("/Game/Blueprints", only_if_is_dirty=False, recursive=True)
    unreal.log("Created/updated Stargame prototype Blueprint assets.")


main()
