import unreal


MAP_PATH = "/Game/Maps/StellarClassPreview"
def is_star_preview_actor(actor: unreal.Actor) -> bool:
    if actor is None:
        return False

    actor_name = actor.get_name()
    class_name = actor.get_class().get_name()
    return (
        "SectorStarAnchor" in actor_name
        or "SectorStarAnchor" in class_name
        or actor_name.startswith("Class_")
    )


def set_if_present(actor: unreal.Actor, property_name: str, value) -> None:
    try:
        actor.set_editor_property(property_name, value)
    except Exception:
        pass


level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

level_subsystem.load_level(MAP_PATH)

stars = [actor for actor in actor_subsystem.get_all_level_actors() if is_star_preview_actor(actor)]
if not stars:
    raise RuntimeError("StellarClassPreview contains no existing star preview actor to keep.")

stars.sort(key=lambda actor: actor.get_name())
star = stars[0]
deleted = 0
for actor in stars[1:]:
    actor_subsystem.destroy_actor(actor)
    deleted += 1

star.set_actor_location(unreal.Vector(0.0, 0.0, 0.0), False, False)
star.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
star.set_actor_label("Single_G_Star")

stellar_class = unreal.StargameStellarClass.G
set_if_present(star, "StellarClass", stellar_class)
set_if_present(star, "bEnablePreviewControls", False)
set_if_present(star, "bUsePreviewCamera", True)
set_if_present(star, "bShowCorona", False)
set_if_present(star, "bShowHaloBillboard", False)
set_if_present(star, "bShowProminences", False)
set_if_present(star, "bShowEjections", False)
set_if_present(star, "bShowClassLabel", False)
set_if_present(star, "VisualEmissionScale", 0.035)
set_if_present(star, "LightIntensityScale", 0.035)
set_if_present(star, "HaloAuthoringScale", 0.35)

unreal.EditorLevelLibrary.save_current_level()
unreal.log(f"StellarClassPreview repaired: kept {star.get_name()}, deleted {deleted} other preview stars.")
