#!/usr/bin/env python3
import json

from ws_bridge_call import call


def run(action, payload, allow_fail=False):
    result = call(action, payload, timeout=45.0)
    response = result["response"]
    ok = response.get("success", False)
    if not ok and not allow_fail:
        print(json.dumps(result, indent=2))
        raise RuntimeError(f"{action} failed: {response.get('error') or response.get('message')}")
    status = "OK" if ok else "SKIP"
    target = payload.get("componentName") or payload.get("blueprintPath") or payload.get("file") or payload.get("slotName")
    print(f"{status} {action} {payload.get('action')} {target}: {response.get('message') or response.get('error')}")
    return response


def add_component(blueprint, component_class, component_name, mesh=None, material=None):
    payload = {
        "action": "add_scs_component",
        "blueprintPath": blueprint,
        "componentClass": component_class,
        "componentName": component_name,
        "attachTo": "SceneRoot",
        "compile": True,
        "save": True,
    }
    if mesh:
        payload["meshPath"] = mesh
    if material:
        payload["materialPath"] = material
    run("manage_blueprint", payload, allow_fail=True)


def set_transform(blueprint, component_name, location=None, rotation=None, scale=None):
    payload = {
        "action": "set_scs_transform",
        "blueprintPath": blueprint,
        "componentName": component_name,
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


def author_room():
    bp = "/Game/Blueprints/Station/BP_StationInteriorRoom"
    add_component(bp, "StaticMeshComponent", "BackWall", "/Engine/BasicShapes/Cube.Cube")
    set_transform(bp, "BackWall", location=(0, -520, 130), scale=(10, 0.18, 2.6))
    add_component(bp, "StaticMeshComponent", "FrontWallLeft", "/Engine/BasicShapes/Cube.Cube")
    set_transform(bp, "FrontWallLeft", location=(-360, 520, 130), scale=(2.8, 0.18, 2.6))
    add_component(bp, "StaticMeshComponent", "FrontWallRight", "/Engine/BasicShapes/Cube.Cube")
    set_transform(bp, "FrontWallRight", location=(360, 520, 130), scale=(2.8, 0.18, 2.6))
    add_component(bp, "StaticMeshComponent", "LeftWall", "/Engine/BasicShapes/Cube.Cube")
    set_transform(bp, "LeftWall", location=(-520, 0, 130), scale=(0.18, 10, 2.6))
    add_component(bp, "StaticMeshComponent", "RightWall", "/Engine/BasicShapes/Cube.Cube")
    set_transform(bp, "RightWall", location=(520, 0, 130), scale=(0.18, 10, 2.6))
    add_component(bp, "StaticMeshComponent", "CeilingPanel", "/Engine/BasicShapes/Cube.Cube")
    set_transform(bp, "CeilingPanel", location=(0, 0, 285), scale=(9.8, 9.8, 0.12))
    add_component(bp, "PointLightComponent", "ServiceAreaLight")
    set_transform(bp, "ServiceAreaLight", location=(0, 0, 245), scale=(1, 1, 1))


def author_interactable():
    bp = "/Game/Blueprints/Station/BP_StationInteractable"
    set_transform(bp, "ServiceProp", location=(0, 0, 45), scale=(0.75, 0.75, 0.9))
    add_component(bp, "StaticMeshComponent", "ServiceConsole", "/Engine/BasicShapes/Cube.Cube")
    set_transform(bp, "ServiceConsole", location=(0, -42, 95), rotation=(-18, 0, 0), scale=(0.68, 0.18, 0.34))
    add_component(bp, "StaticMeshComponent", "ServiceBeacon", "/Engine/BasicShapes/Sphere.Sphere")
    set_transform(bp, "ServiceBeacon", location=(0, 0, 155), scale=(0.22, 0.22, 0.22))
    add_component(bp, "PointLightComponent", "FocusGlow")
    set_transform(bp, "FocusGlow", location=(0, 0, 150), scale=(1, 1, 1))
    add_component(bp, "TextRenderComponent", "ServiceLabel")
    set_transform(bp, "ServiceLabel", location=(0, -78, 150), rotation=(0, 180, 0), scale=(0.55, 0.55, 0.55))


def author_contact():
    bp = "/Game/Blueprints/Station/BP_MissionContact"
    set_transform(bp, "NpcBody", location=(0, 0, 70), scale=(0.45, 0.45, 1.15))
    add_component(bp, "StaticMeshComponent", "NpcHead", "/Engine/BasicShapes/Sphere.Sphere")
    set_transform(bp, "NpcHead", location=(0, 0, 175), scale=(0.32, 0.32, 0.32))
    add_component(bp, "StaticMeshComponent", "ContactMarker", "/Engine/BasicShapes/Sphere.Sphere")
    set_transform(bp, "ContactMarker", location=(0, 0, 235), scale=(0.16, 0.16, 0.16))
    add_component(bp, "PointLightComponent", "ContactGlow")
    set_transform(bp, "ContactGlow", location=(0, 0, 220), scale=(1, 1, 1))
    add_component(bp, "TextRenderComponent", "PromptText")
    set_transform(bp, "PromptText", location=(0, -60, 220), rotation=(0, 180, 0), scale=(0.5, 0.5, 0.5))


def author_hostile():
    bp = "/Game/Blueprints/Station/BP_StationHostile"
    set_transform(bp, "HostileBody", location=(0, 0, 70), scale=(0.48, 0.48, 1.08))
    add_component(bp, "StaticMeshComponent", "HostileHead", "/Engine/BasicShapes/Sphere.Sphere")
    set_transform(bp, "HostileHead", location=(0, 0, 175), scale=(0.32, 0.32, 0.32))
    add_component(bp, "StaticMeshComponent", "HostileWeapon", "/Engine/BasicShapes/Cube.Cube")
    set_transform(bp, "HostileWeapon", location=(42, -18, 112), rotation=(0, 0, -12), scale=(0.12, 0.72, 0.1))
    add_component(bp, "PointLightComponent", "HostileAlertGlow")
    set_transform(bp, "HostileAlertGlow", location=(0, 0, 205), scale=(1, 1, 1))
    add_component(bp, "TextRenderComponent", "HostileStatusText")
    set_transform(bp, "HostileStatusText", location=(0, -60, 220), rotation=(0, 180, 0), scale=(0.5, 0.5, 0.5))


def main():
    author_room()
    author_interactable()
    author_contact()
    author_hostile()
    run("system_control", {"action": "execute_python", "file": "tmp/configure_station_visual_components.py"})
    print("Station Blueprint visual authoring pass complete.")


if __name__ == "__main__":
    main()
