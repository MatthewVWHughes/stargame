#!/usr/bin/env python3
import json

from generate_milky_way_unreal_texture import main as generate_milky_way_texture
from ws_bridge_call import call


BP = "/Game/Blueprints/Flight/BP_WayfarerFlightPawn"
OLD_BP = "/Game/Blueprints/Space/BP_SystemSpacePresentation"


def run(action, payload, allow_fail=False):
    result = call(action, payload, timeout=90.0)
    response = result["response"]
    ok = response.get("success", False)
    if not ok and not allow_fail:
        print(json.dumps(result, indent=2))
        raise RuntimeError(f"{action} failed: {response.get('error') or response.get('message')}")
    status = "OK" if ok else "SKIP"
    target = payload.get("componentName") or payload.get("blueprintPath") or payload.get("file")
    print(f"{status} {action} {payload.get('action')} {target}: {response.get('message') or response.get('error')}")


def main():
    generate_milky_way_texture()
    run("system_control", {"action": "execute_python", "file": "tmp/import_milky_way_skybox_assets.py"})
    run("system_control", {"action": "execute_python", "file": "tmp/create_milky_way_sky_sphere_mesh.py"})
    run(
        "manage_blueprint",
        {
            "action": "remove_scs_component",
            "blueprintPath": OLD_BP,
            "componentName": "MilkyWaySkyboxSphere",
            "compile": True,
            "save": True,
        },
        allow_fail=True,
    )
    run(
        "manage_blueprint",
        {
            "action": "add_scs_component",
            "blueprintPath": BP,
            "componentClass": "StaticMeshComponent",
            "componentName": "MilkyWaySkyboxSphere",
            "meshPath": "/Game/Meshes/Sky/SM_MilkyWaySkySphere",
            "compile": True,
            "save": True,
        },
        allow_fail=True,
    )
    run(
        "manage_blueprint",
        {
            "action": "set_scs_transform",
            "blueprintPath": BP,
            "componentName": "MilkyWaySkyboxSphere",
            "location": {"x": 0, "y": 0, "z": 0},
            "rotation": {"pitch": 0, "yaw": 180, "roll": 0},
            "scale": {"x": 250000, "y": 250000, "z": 250000},
            "compile": True,
            "save": True,
        },
    )
    run("system_control", {"action": "execute_python", "file": "tmp/configure_milky_way_skybox_component.py"})
    print("Milky Way skybox authored on BP_WayfarerFlightPawn.")


if __name__ == "__main__":
    main()
