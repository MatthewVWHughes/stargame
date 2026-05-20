import os

import unreal


SOURCE_TEXTURE = "/home/matthew/Repos/stargame/Content/SourceArt/Sky/T_MilkyWay_UnrealBalanced.png"
TEXTURE_DIR = "/Game/Textures/Sky"
TEXTURE_NAME = "T_MilkyWay_UnrealBalanced"
TEXTURE_PATH = f"{TEXTURE_DIR}/{TEXTURE_NAME}"
MATERIAL_DIR = "/Game/Materials/Sky"
MATERIAL_NAME = "M_MilkyWaySkybox"
MATERIAL_PATH = f"{MATERIAL_DIR}/{MATERIAL_NAME}"


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def import_texture():
    if not os.path.exists(SOURCE_TEXTURE):
        raise RuntimeError(f"Missing source texture: {SOURCE_TEXTURE}")
    ensure_directory(TEXTURE_DIR)
    if unreal.EditorAssetLibrary.does_asset_exist(TEXTURE_PATH):
        texture = unreal.EditorAssetLibrary.load_asset(TEXTURE_PATH)
    else:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", SOURCE_TEXTURE)
        task.set_editor_property("destination_path", TEXTURE_DIR)
        task.set_editor_property("destination_name", TEXTURE_NAME)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.EditorAssetLibrary.load_asset(TEXTURE_PATH)
    if not texture:
        raise RuntimeError(f"Failed to import/load {TEXTURE_PATH}")
    texture.set_editor_property("srgb", True)
    try:
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_USER_INTERFACE2D)
    except Exception:
        pass
    try:
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_SKYBOX)
    except Exception:
        pass
    try:
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    except Exception:
        pass
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def create_or_load_material():
    ensure_directory(MATERIAL_DIR)
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PATH):
        unreal.EditorAssetLibrary.delete_asset(MATERIAL_PATH)
    factory = unreal.MaterialFactoryNew()
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MATERIAL_NAME,
        MATERIAL_DIR,
        unreal.Material,
        factory,
    )
    if not material:
        raise RuntimeError(f"Failed to create/load {MATERIAL_PATH}")
    return material


def configure_material(material, texture):
    material.set_editor_property("two_sided", True)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    tex = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSample,
        -420,
        -60,
    )
    tex.set_editor_property("texture", texture)

    multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionMultiply,
        -140,
        -40,
    )
    brightness = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        -360,
        140,
    )
    brightness.set_editor_property("r", 0.20)

    unreal.MaterialEditingLibrary.connect_material_expressions(tex, "RGB", multiply, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(brightness, "", multiply, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        multiply,
        "",
        unreal.MaterialProperty.MP_EMISSIVE_COLOR,
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)


def main():
    texture = import_texture()
    material = create_or_load_material()
    configure_material(material, texture)
    print(f"Milky Way skybox assets ready: {TEXTURE_PATH}, {MATERIAL_PATH}")


if __name__ == "__main__":
    main()
