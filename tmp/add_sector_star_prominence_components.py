#!/usr/bin/env python3
from ws_bridge_call import call


BP = "/Game/Blueprints/Space/BP_SectorStarAnchor"


def main():
    for index in range(18):
        name = f"ProminenceLoop{index:02d}"
        result = call(
            "manage_blueprint",
            {
                "action": "add_scs_component",
                "blueprintPath": BP,
                "componentClass": "StaticMeshComponent",
                "componentName": name,
                "compile": True,
                "save": True,
            },
            timeout=45.0,
        )
        response = result["response"]
        print(("OK" if response.get("success") else "SKIP"), name, response.get("message") or response.get("error"))


if __name__ == "__main__":
    main()
