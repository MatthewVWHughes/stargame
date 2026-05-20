import traceback
import unreal


def bp_class(path):
    return unreal.EditorAssetLibrary.load_blueprint_class(path)


def cdo(path):
    bp = unreal.EditorAssetLibrary.load_asset(path)
    if not bp:
        return None
    return unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))


def main():
    try:
        checks = {
            "GameMode.BootMenuWidgetClass": cdo("/Game/Blueprints/Core/BP_StargameGameMode").get_editor_property("boot_menu_widget_class"),
            "GameMode.PlayerControllerClass": cdo("/Game/Blueprints/Core/BP_StargameGameMode").get_editor_property("player_controller_class"),
            "PC.SystemMapWidgetClass": cdo("/Game/Blueprints/Core/BP_StargamePlayerController").get_editor_property("system_map_widget_class"),
            "PC.CommsWidgetClass": cdo("/Game/Blueprints/Core/BP_StargamePlayerController").get_editor_property("comms_widget_class"),
            "PC.PauseMenuWidgetClass": cdo("/Game/Blueprints/Core/BP_StargamePlayerController").get_editor_property("pause_menu_widget_class"),
            "PC.FlightHUDWidgetClass": cdo("/Game/Blueprints/Core/BP_StargamePlayerController").get_editor_property("flight_hud_widget_class"),
            "StationPawn.StationInteractionWidgetClass": cdo("/Game/Blueprints/Station/BP_StationInteriorPawn").get_editor_property("station_interaction_widget_class"),
            "StationRoom.MissionContactActorClass": cdo("/Game/Blueprints/Station/BP_StationInteriorRoom").get_editor_property("mission_contact_actor_class"),
            "StationRoom.InteractableActorClass": cdo("/Game/Blueprints/Station/BP_StationInteriorRoom").get_editor_property("interactable_actor_class"),
            "StationRoom.HostileActorClass": cdo("/Game/Blueprints/Station/BP_StationInteriorRoom").get_editor_property("hostile_actor_class"),
        }
        for key, value in checks.items():
            print(key, "=>", value)

        ship = unreal.EditorAssetLibrary.load_asset("/Game/Data/Ships/Archetypes/wayfarer_mk1").get_editor_property("definition")
        print("Ship.PawnClass =>", ship.get_editor_property("pawn_class"))
        print("Ship.ActorClass =>", ship.get_editor_property("actor_class"))

        for path in ["/Game/Data/Systems/frontier_test_01", "/Game/Data/Systems/frontier_arrival_test_01"]:
            definition = unreal.EditorAssetLibrary.load_asset(path).get_editor_property("definition")
            print(path, "PresentationActorClass =>", definition.get_editor_property("presentation_actor_class"))
            print(path, "EntityMarkerActorClass =>", definition.get_editor_property("entity_marker_actor_class"))
            print(path, "CombatShotPresentationActorClass =>", definition.get_editor_property("combat_shot_presentation_actor_class"))
            print(path, "EncounterActorClass =>", definition.get_editor_property("encounter_actor_class"))
            for station in definition.get_editor_property("stations"):
                print(path, "Station", station.get_editor_property("station_id"), "Room", station.get_editor_property("interior_room_class"), "Pawn", station.get_editor_property("interior_pawn_class"))
    except Exception:
        print(traceback.format_exc())


main()
