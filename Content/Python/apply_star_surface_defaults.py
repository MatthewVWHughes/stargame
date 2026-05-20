import unreal


TEXTURE_PATH = "/Game/Textures/Space/Star/T_StarPhotosphereMasks"
SURFACE_MATERIAL_PATH = "/Game/Materials/Space/M_StarPhotosphereProcedural"
BLUEPRINT_PATH = "/Game/Blueprints/Space/BP_SectorStarAnchor"
MAP_PATH = "/Game/Maps/StellarClassPreview"


def set_if_possible(obj, name, value):
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as exc:
        unreal.log_warning(f"Could not set {name} on {obj}: {exc}")
        return False


texture = unreal.EditorAssetLibrary.load_asset(TEXTURE_PATH)
if texture is None:
    raise RuntimeError(f"Could not load {TEXTURE_PATH}")
surface_material = unreal.EditorAssetLibrary.load_asset(SURFACE_MATERIAL_PATH)
if surface_material is None:
    raise RuntimeError(f"Could not load {SURFACE_MATERIAL_PATH}; run rebuild_star_surface_material.py first")

bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BLUEPRINT_PATH)
if bp_class is None:
    raise RuntimeError(f"Could not load blueprint class {BLUEPRINT_PATH}")

cdo = unreal.get_default_object(bp_class)
set_if_possible(cdo, "StarSurfaceMaterial", surface_material)
set_if_possible(cdo, "StarPhotosphereMaskTexture", texture)
set_if_possible(cdo, "VisualEmissionScale", 0.14)
set_if_possible(cdo, "LightIntensityScale", 0.035)
set_if_possible(cdo, "HaloAuthoringScale", 0.35)
set_if_possible(cdo, "bShowCorona", True)
set_if_possible(cdo, "bShowHaloBillboard", False)
set_if_possible(cdo, "bShowProminences", True)
set_if_possible(cdo, "bShowEjections", False)
set_if_possible(cdo, "bShowClassLabel", False)
unreal.EditorAssetLibrary.save_asset(BLUEPRINT_PATH, only_if_is_dirty=False)

unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
updated = 0
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    actor_class_name = actor.get_class().get_name()
    actor_name = actor.get_name()
    if actor_class_name == bp_class.get_name() or "SectorStarAnchor" in actor_class_name or "SectorStarAnchor" in actor_name:
        set_if_possible(actor, "StarSurfaceMaterial", surface_material)
        set_if_possible(actor, "StarPhotosphereMaskTexture", texture)
        set_if_possible(actor, "VisualEmissionScale", 0.14)
        set_if_possible(actor, "LightIntensityScale", 0.035)
        set_if_possible(actor, "HaloAuthoringScale", 0.35)
        set_if_possible(actor, "bShowCorona", True)
        set_if_possible(actor, "bShowHaloBillboard", False)
        set_if_possible(actor, "bShowProminences", True)
        set_if_possible(actor, "bShowEjections", False)
        set_if_possible(actor, "bShowClassLabel", False)
        try:
            actor.apply_stellar_class(actor.get_editor_property("StellarClass"))
        except Exception as exc:
            unreal.log_warning(f"Could not re-apply stellar class on {actor.get_name()}: {exc}")
        updated += 1

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log(f"Applied star surface defaults with {TEXTURE_PATH}; updated {updated} level actor(s).")
