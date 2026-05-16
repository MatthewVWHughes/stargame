import unreal


def main():
    asset = unreal.EditorAssetLibrary.load_asset(
        "/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst"
    )
    if asset is None:
        raise RuntimeError("Could not load stock volumetric cloud material instance")

    unreal.log(f"Loaded {asset.get_name()} as {asset.get_class().get_name()}")

    for name in dir(unreal.MaterialEditingLibrary):
        if "parameter" in name.lower():
            unreal.log(f"MaterialEditingLibrary method: {name}")

    try:
        scalar_infos = unreal.MaterialEditingLibrary.get_scalar_parameter_names(asset)
        for info in scalar_infos:
            unreal.log(f"Scalar parameter: {info}")
    except Exception as exc:
        unreal.log_warning(f"Could not list scalar parameter names: {exc}")

    try:
        vector_infos = unreal.MaterialEditingLibrary.get_vector_parameter_names(asset)
        for info in vector_infos:
            unreal.log(f"Vector parameter: {info}")
    except Exception as exc:
        unreal.log_warning(f"Could not list vector parameter names: {exc}")

    try:
        texture_infos = unreal.MaterialEditingLibrary.get_texture_parameter_names(asset)
        for info in texture_infos:
            unreal.log(f"Texture parameter: {info}")
    except Exception as exc:
        unreal.log_warning(f"Could not list texture parameter names: {exc}")

    for property_name in (
        "scalar_parameter_values",
        "vector_parameter_values",
        "texture_parameter_values",
    ):
        try:
            values = asset.get_editor_property(property_name)
            unreal.log(f"{property_name}: {values}")
        except Exception as exc:
            unreal.log_warning(f"Could not read {property_name}: {exc}")


main()
