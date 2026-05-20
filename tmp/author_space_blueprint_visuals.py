#!/usr/bin/env python3
import json

from generate_milky_way_unreal_texture import main as generate_milky_way_texture
from ws_bridge_call import call


def run(action, payload, allow_fail=False):
    result = call(action, payload, timeout=45.0)
    response = result["response"]
    ok = response.get("success", False)
    if not ok and not allow_fail:
        print(json.dumps(result, indent=2))
        raise RuntimeError(f"{action} failed: {response.get('error') or response.get('message')}")
    status = "OK" if ok else "SKIP"
    target = payload.get("componentName") or payload.get("blueprintPath") or payload.get("file")
    print(f"{status} {action} {payload.get('action')} {target}: {response.get('message') or response.get('error')}")


def add_component(blueprint, component_class, component_name, mesh=None):
    payload = {
        "action": "add_scs_component",
        "blueprintPath": blueprint,
        "componentClass": component_class,
        "componentName": component_name,
        "compile": True,
        "save": True,
    }
    if mesh:
        payload["meshPath"] = mesh
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


def author_ship(bp, large=False):
    set_transform(bp, "ShipHull", scale=(2.8 if large else 1.9, 1.05 if large else 0.75, 0.42 if large else 0.35))
    add_component(bp, "StaticMeshComponent", "LeftWing", "/Engine/BasicShapes/Cube.Cube")
    set_transform(bp, "LeftWing", location=(-20, -72 if large else -52, -8), scale=(0.85 if large else 0.58, 1.45 if large else 0.9, 0.06))
    add_component(bp, "StaticMeshComponent", "RightWing", "/Engine/BasicShapes/Cube.Cube")
    set_transform(bp, "RightWing", location=(-20, 72 if large else 52, -8), scale=(0.85 if large else 0.58, 1.45 if large else 0.9, 0.06))
    add_component(bp, "StaticMeshComponent", "CockpitCanopy", "/Engine/BasicShapes/Sphere.Sphere")
    set_transform(bp, "CockpitCanopy", location=(70 if large else 45, 0, 24), scale=(0.34, 0.24, 0.16))
    add_component(bp, "StaticMeshComponent", "EngineCore", "/Engine/BasicShapes/Sphere.Sphere")
    set_transform(bp, "EngineCore", location=(-148 if large else -112, 0, -4), scale=(0.3, 0.3, 0.16))
    add_component(bp, "PointLightComponent", "RunningLight")
    set_transform(bp, "RunningLight", location=(80 if large else 50, 0, 24), scale=(1, 1, 1))


def author_space():
    bp = "/Game/Blueprints/Space/BP_SystemSpacePresentation"
    add_component(bp, "StaticMeshComponent", "DistantStarDisc", "/Engine/BasicShapes/Sphere.Sphere")
    set_transform(bp, "DistantStarDisc", location=(-8000, -2400, 3200), scale=(18, 18, 18))
    add_component(bp, "PointLightComponent", "StarGlow")
    set_transform(bp, "StarGlow", location=(-8000, -2400, 3200), scale=(1, 1, 1))

    marker = "/Game/Blueprints/Space/BP_SystemEntityMarker"
    set_transform(marker, "MarkerMesh", scale=(0.72, 0.72, 0.72))
    add_component(marker, "StaticMeshComponent", "MarkerCore", "/Engine/BasicShapes/Sphere.Sphere")
    set_transform(marker, "MarkerCore", scale=(0.32, 0.32, 0.32))
    add_component(marker, "TextRenderComponent", "MarkerLabel")
    set_transform(marker, "MarkerLabel", location=(0, -72, 58), rotation=(0, 180, 0), scale=(0.6, 0.6, 0.6))

    shot = "/Game/Blueprints/Space/BP_CombatShotPresentation"
    set_transform(shot, "ShotBeam", scale=(8, 0.06, 0.06))
    add_component(shot, "StaticMeshComponent", "ShotStartFlash", "/Engine/BasicShapes/Sphere.Sphere")
    set_transform(shot, "ShotStartFlash", location=(-380, 0, 0), scale=(0.28, 0.28, 0.28))
    add_component(shot, "StaticMeshComponent", "ShotEndFlash", "/Engine/BasicShapes/Sphere.Sphere")
    set_transform(shot, "ShotEndFlash", location=(380, 0, 0), scale=(0.34, 0.34, 0.34))
    add_component(shot, "PointLightComponent", "ShotGlow")
    set_transform(shot, "ShotGlow", location=(0, 0, 0), scale=(1, 1, 1))


def main():
    author_ship("/Game/Blueprints/Flight/BP_WayfarerFlightPawn", large=True)
    add_component("/Game/Blueprints/Flight/BP_WayfarerFlightPawn", "StaticMeshComponent", "MilkyWaySkyboxSphere", "/Game/Meshes/Sky/SM_MilkyWaySkySphere")
    set_transform("/Game/Blueprints/Flight/BP_WayfarerFlightPawn", "MilkyWaySkyboxSphere", rotation=(0, 180, 0), scale=(250000, 250000, 250000))
    author_ship("/Game/Blueprints/Space/BP_WayfarerTrafficShip")
    author_ship("/Game/Blueprints/Space/BP_SystemEncounterActor")
    author_space()
    run("system_control", {"action": "execute_python", "file": "tmp/configure_space_visual_components.py"})
    generate_milky_way_texture()
    run("system_control", {"action": "execute_python", "file": "tmp/import_milky_way_skybox_assets.py"})
    run("system_control", {"action": "execute_python", "file": "tmp/create_milky_way_sky_sphere_mesh.py"})
    run("system_control", {"action": "execute_python", "file": "tmp/configure_milky_way_skybox_component.py"})
    print("Flight/space Blueprint visual authoring pass complete.")


if __name__ == "__main__":
    main()
