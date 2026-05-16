import unreal


ASSET_PATH = "/Game/Materials/Environment"
ASSET_NAME = "MI_JoolVolumetricCloud"
FULL_PATH = f"{ASSET_PATH}/{ASSET_NAME}"
PARENT_PATH = "/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud"


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def create_or_load_material_instance():
    if unreal.EditorAssetLibrary.does_asset_exist(FULL_PATH):
        return unreal.EditorAssetLibrary.load_asset(FULL_PATH)

    parent = unreal.EditorAssetLibrary.load_asset(PARENT_PATH)
    if parent is None:
        raise RuntimeError(f"Could not load parent cloud material: {PARENT_PATH}")

    factory = unreal.MaterialInstanceConstantFactoryNew()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material_instance = asset_tools.create_asset(
        ASSET_NAME,
        ASSET_PATH,
        unreal.MaterialInstanceConstant,
        factory,
    )
    if material_instance is None:
        raise RuntimeError(f"Could not create material instance: {FULL_PATH}")

    material_instance.set_editor_property("parent", parent)
    return material_instance


def set_scalar(material, name, value):
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        material,
        name,
        value,
    )


def set_vector(material, name, color):
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        material,
        name,
        color,
    )


def main():
    ensure_directory(ASSET_PATH)
    material = create_or_load_material_instance()

    # Force a dense gas-giant cloud ocean rather than broken Earth cloud cover.
    set_scalar(material, "Cloud_GlobalCoverage", 1.0)
    set_scalar(material, "Cloud_GlobalDensity", 7.5)
    set_scalar(material, "Layout_CloudGlobalScale", 1.55)
    set_scalar(material, "StormClouds", 1.0)
    set_scalar(material, "Storm_LightningTexScale", 0.18)

    set_vector(material, "Cloud_AlbedoColor", unreal.LinearColor(0.21, 0.58, 0.13, 1.0))
    set_vector(material, "Storm_AlbedoColor", unreal.LinearColor(0.42, 0.48, 0.08, 1.0))
    set_vector(material, "Storm_LightningColor", unreal.LinearColor(0.82, 0.96, 0.42, 1.0))

    # Bias toward broad, thick, layered formations. These are stock-material controls,
    # so this remains a tuning spike rather than a custom gas-giant shader.
    set_vector(material, "Noise_Bias", unreal.LinearColor(-0.62, -0.52, -0.42, 0.0))
    set_vector(material, "Noise_Strength", unreal.LinearColor(0.22, 0.18, 0.14, 0.0))
    set_vector(material, "Layout_CloudType", unreal.LinearColor(0.96, 0.92, 0.88, 0.0))
    set_vector(material, "Layout_CloudTypeMask", unreal.LinearColor(1.0, 1.0, 1.0, 0.0))
    set_vector(material, "Layout_CloudPerTypeScale", unreal.LinearColor(12.0, 18.0, 28.0, 0.0))
    set_vector(material, "Layout_WindControls", unreal.LinearColor(0.0015, 0.0, 0.0, 0.0))

    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log(f"Created/updated {FULL_PATH}")


main()
