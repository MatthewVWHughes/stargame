#!/usr/bin/env python3
import json
import sys

from ws_bridge_call import call


def run(action, payload, allow_fail=False):
    result = call(action, payload, timeout=45.0)
    response = result["response"]
    ok = response.get("success", False)
    if not ok and not allow_fail:
        print(json.dumps(result, indent=2))
        raise RuntimeError(f"{action} failed: {response.get('error') or response.get('message')}")
    print(f"{'OK' if ok else 'SKIP'} {action} {payload.get('action')} {payload.get('name') or payload.get('blueprintPath') or payload.get('widgetPath') or payload.get('slotName')}: {response.get('message') or response.get('error')}")
    return response


def create_bp(path, name, parent):
    run("manage_blueprint", {
        "action": "create",
        "name": name,
        "savePath": path,
        "parentClass": parent,
        "compile": True,
        "save": True,
    }, allow_fail=True)


def add_component(bp, cls, name, **kwargs):
    payload = {
        "action": "add_scs_component",
        "blueprintPath": bp,
        "componentClass": cls,
        "componentName": name,
        "compile": True,
        "save": True,
    }
    payload.update(kwargs)
    run("manage_blueprint", payload, allow_fail=True)


def set_transform(bp, component, location=None, rotation=None, scale=None):
    payload = {
        "action": "set_scs_transform",
        "blueprintPath": bp,
        "componentName": component,
        "compile": True,
        "save": True,
    }
    if location:
        payload["location"] = {"x": location[0], "y": location[1], "z": location[2]}
    if rotation:
        payload["rotation"] = {"pitch": rotation[0], "yaw": rotation[1], "roll": rotation[2]}
    if scale:
        payload["scale"] = {"x": scale[0], "y": scale[1], "z": scale[2]}
    run("manage_blueprint", payload, allow_fail=True)


def create_widget(path, name, parent):
    create_response = run("manage_widget_authoring", {
        "action": "create_widget_blueprint",
        "name": name,
        "path": path,
        "parentClass": parent,
    }, allow_fail=True)
    run("manage_widget_authoring", {
        "action": "set_widget_parent_class",
        "widgetPath": f"{path}/{name}",
        "parentClass": parent,
    }, allow_fail=True)
    return bool(create_response.get("success", False))


def add_widget(widget, action, slot, **kwargs):
    payload = {
        "action": action,
        "widgetPath": widget,
        "slotName": slot,
    }
    payload.update(kwargs)
    run("manage_widget_authoring", payload, allow_fail=True)


def build_widgets():
    ui = "/Game/Blueprints/UI"
    widgets = [
        ("WBP_BootMenu", "/Script/Stargame.StargameBootMenuWidget"),
        ("WBP_Comms", "/Script/Stargame.StargameCommsWidget"),
        ("WBP_Dialog", "/Script/Stargame.StargameDialogWidget"),
        ("WBP_PauseMenu", "/Script/Stargame.StargamePauseMenuWidget"),
        ("WBP_StationInteraction", "/Script/Stargame.StargameStationInteractionWidget"),
        ("WBP_SystemMap", "/Script/Stargame.StargameSystemMapWidget"),
        ("WBP_FlightHUD", "/Script/Stargame.StargameFlightHUDWidget"),
    ]
    newly_created = {}
    for name, parent in widgets:
        newly_created[name] = create_widget(ui, name, parent)

    specs = {
        "WBP_BootMenu": [
            ("add_canvas_panel", "RootCanvas", {}),
            ("add_text_block", "TitleText", {"parentSlot": "RootCanvas", "text": "STARGAME", "fontSize": 42}),
            ("add_text_block", "StatusText", {"parentSlot": "RootCanvas", "text": "Ready", "fontSize": 18}),
            ("add_button", "NewGameButton", {"parentSlot": "RootCanvas"}),
            ("add_button", "ContinueButton", {"parentSlot": "RootCanvas"}),
            ("add_button", "QuitButton", {"parentSlot": "RootCanvas"}),
        ],
        "WBP_Comms": [
            ("add_canvas_panel", "RootCanvas", {}),
            ("add_text_block", "TitleText", {"parentSlot": "RootCanvas", "text": "Comms", "fontSize": 26}),
            ("add_text_block", "BodyText", {"parentSlot": "RootCanvas", "text": "No channel", "fontSize": 16, "autoWrap": True}),
            ("add_text_block", "StatusText", {"parentSlot": "RootCanvas", "text": "", "fontSize": 14}),
            ("add_button", "RequestDockingButton", {"parentSlot": "RootCanvas"}),
            ("add_button", "CloseButton", {"parentSlot": "RootCanvas"}),
        ],
        "WBP_Dialog": [
            ("add_canvas_panel", "RootCanvas", {}),
            ("add_text_block", "TitleText", {"parentSlot": "RootCanvas", "text": "Dialog", "fontSize": 26}),
            ("add_text_block", "BodyText", {"parentSlot": "RootCanvas", "text": "", "fontSize": 16, "autoWrap": True}),
            ("add_text_block", "StatusText", {"parentSlot": "RootCanvas", "text": "", "fontSize": 14}),
        ],
        "WBP_PauseMenu": [
            ("add_canvas_panel", "RootCanvas", {}),
            ("add_text_block", "TitleText", {"parentSlot": "RootCanvas", "text": "Pause", "fontSize": 30}),
            ("add_text_block", "BodyText", {"parentSlot": "RootCanvas", "text": "", "fontSize": 16, "autoWrap": True}),
            ("add_text_block", "FooterText", {"parentSlot": "RootCanvas", "text": "", "fontSize": 12}),
            ("add_button", "InventoryTabButton", {"parentSlot": "RootCanvas"}),
            ("add_button", "MissionsTabButton", {"parentSlot": "RootCanvas"}),
            ("add_button", "SettingsTabButton", {"parentSlot": "RootCanvas"}),
            ("add_button", "ResumeButton", {"parentSlot": "RootCanvas"}),
        ],
        "WBP_StationInteraction": [
            ("add_canvas_panel", "RootCanvas", {}),
            ("add_text_block", "TitleText", {"parentSlot": "RootCanvas", "text": "Station", "fontSize": 26}),
            ("add_text_block", "BodyText", {"parentSlot": "RootCanvas", "text": "", "fontSize": 16, "autoWrap": True}),
            ("add_text_block", "StatusText", {"parentSlot": "RootCanvas", "text": "", "fontSize": 14}),
            ("add_button", "PrimaryButton", {"parentSlot": "RootCanvas"}),
            ("add_button", "CloseButton", {"parentSlot": "RootCanvas"}),
        ],
        "WBP_SystemMap": [
            ("add_canvas_panel", "RootCanvas", {}),
            ("add_text_block", "TitleText", {"parentSlot": "RootCanvas", "text": "System Map", "fontSize": 28}),
            ("add_text_block", "StatusText", {"parentSlot": "RootCanvas", "text": "", "fontSize": 14}),
        ],
        "WBP_FlightHUD": [
            ("add_canvas_panel", "RootCanvas", {}),
            ("add_text_block", "SpeedText", {"parentSlot": "RootCanvas", "text": "SPD", "fontSize": 18}),
            ("add_text_block", "TargetText", {"parentSlot": "RootCanvas", "text": "TARGET", "fontSize": 18}),
            ("add_text_block", "DockingText", {"parentSlot": "RootCanvas", "text": "DOCK", "fontSize": 14}),
            ("add_text_block", "MissionText", {"parentSlot": "RootCanvas", "text": "MISSION", "fontSize": 14}),
        ],
    }
    for name, entries in specs.items():
        if not newly_created.get(name, False):
            print(f"SKIP initial widget tree for existing widget {name}")
            continue
        widget = f"{ui}/{name}"
        for action, slot, kwargs in entries:
            add_widget(widget, action, slot, **kwargs)


def build_actor_blueprints():
    create_bp("/Game/Blueprints/Core", "BP_StargamePlayerController", "/Script/Stargame.StargamePlayerController")
    create_bp("/Game/Blueprints/Flight", "BP_WayfarerFlightPawn", "/Script/Stargame.SpaceFlightPawn")
    add_component("/Game/Blueprints/Flight/BP_WayfarerFlightPawn", "StaticMeshComponent", "ShipHull", meshPath="/Engine/BasicShapes/Cone.Cone")
    set_transform("/Game/Blueprints/Flight/BP_WayfarerFlightPawn", "ShipHull", scale=(2.6, 1.2, 0.45))
    add_component("/Game/Blueprints/Flight/BP_WayfarerFlightPawn", "PointLightComponent", "EngineGlow")
    set_transform("/Game/Blueprints/Flight/BP_WayfarerFlightPawn", "EngineGlow", location=(-180, 0, -20), scale=(1, 1, 1))

    space = [
        ("BP_SystemSpacePresentation", "/Script/Stargame.SystemSpacePresentationActor"),
        ("BP_SystemEntityMarker", "/Script/Stargame.M0SystemMarkerActor"),
        ("BP_CombatShotPresentation", "/Script/Stargame.CombatShotPresentationActor"),
        ("BP_WayfarerTrafficShip", "/Script/Stargame.LogicalTrafficActor"),
        ("BP_SystemEncounterActor", "/Script/Stargame.LogicalTrafficActor"),
    ]
    for name, parent in space:
        create_bp("/Game/Blueprints/Space", name, parent)
    add_component("/Game/Blueprints/Space/BP_SystemSpacePresentation", "PointLightComponent", "FillLight")
    add_component("/Game/Blueprints/Space/BP_SystemEntityMarker", "StaticMeshComponent", "MarkerMesh", meshPath="/Engine/BasicShapes/Sphere.Sphere")
    add_component("/Game/Blueprints/Space/BP_SystemEntityMarker", "PointLightComponent", "MarkerGlow")
    add_component("/Game/Blueprints/Space/BP_CombatShotPresentation", "StaticMeshComponent", "ShotBeam", meshPath="/Engine/BasicShapes/Cube.Cube")
    set_transform("/Game/Blueprints/Space/BP_CombatShotPresentation", "ShotBeam", scale=(8, 0.08, 0.08))
    for bp in ("/Game/Blueprints/Space/BP_WayfarerTrafficShip", "/Game/Blueprints/Space/BP_SystemEncounterActor"):
        add_component(bp, "StaticMeshComponent", "ShipHull", meshPath="/Engine/BasicShapes/Cone.Cone")
        set_transform(bp, "ShipHull", scale=(1.8, 0.75, 0.35))
        add_component(bp, "PointLightComponent", "EngineGlow")
        set_transform(bp, "EngineGlow", location=(-120, 0, 0))

    station = [
        ("BP_StationInteriorRoom", "/Script/Stargame.StationInteriorRoomActor"),
        ("BP_StationInteriorPawn", "/Script/Stargame.StationInteriorPawn"),
        ("BP_StationInteractable", "/Script/Stargame.StationInteriorInteractableActor"),
        ("BP_MissionContact", "/Script/Stargame.StationMissionContactActor"),
        ("BP_StationHostile", "/Script/Stargame.StationInteriorHostileActor"),
    ]
    for name, parent in station:
        create_bp("/Game/Blueprints/Station", name, parent)
    add_component("/Game/Blueprints/Station/BP_StationInteriorRoom", "StaticMeshComponent", "Floor", meshPath="/Engine/BasicShapes/Cube.Cube")
    set_transform("/Game/Blueprints/Station/BP_StationInteriorRoom", "Floor", location=(0, 0, -20), scale=(10, 10, 0.2))
    add_component("/Game/Blueprints/Station/BP_StationInteriorRoom", "PointLightComponent", "RoomLight")
    set_transform("/Game/Blueprints/Station/BP_StationInteriorRoom", "RoomLight", location=(0, 0, 320))
    add_component("/Game/Blueprints/Station/BP_StationInteriorPawn", "StaticMeshComponent", "BodyPlaceholder", meshPath="/Engine/BasicShapes/Cube.Cube")
    set_transform("/Game/Blueprints/Station/BP_StationInteriorPawn", "BodyPlaceholder", location=(0, 0, -50), scale=(0.4, 0.25, 0.9))
    add_component("/Game/Blueprints/Station/BP_StationInteractable", "StaticMeshComponent", "ServiceProp", meshPath="/Engine/BasicShapes/Cube.Cube")
    add_component("/Game/Blueprints/Station/BP_MissionContact", "StaticMeshComponent", "NpcBody", meshPath="/Engine/BasicShapes/Cylinder.Cylinder")
    add_component("/Game/Blueprints/Station/BP_StationHostile", "StaticMeshComponent", "HostileBody", meshPath="/Engine/BasicShapes/Cylinder.Cylinder")


def main():
    build_widgets()
    build_actor_blueprints()
    print("Blueprint asset creation pass complete.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
