#!/usr/bin/env python3
import json

from ws_bridge_call import call


EVENT_HOOKS = {
    "/Game/Blueprints/UI/WBP_BootMenu": [
        "OnStatusChanged",
        "OnContinueAvailabilityChanged",
    ],
    "/Game/Blueprints/UI/WBP_Comms": [
        "OnCommsOpened",
        "OnCommsClosed",
        "OnCommsViewChanged",
    ],
    "/Game/Blueprints/UI/WBP_Dialog": [
        "OnDialogShown",
        "OnDialogClosed",
        "OnDialogStatusChanged",
    ],
    "/Game/Blueprints/UI/WBP_PauseMenu": [
        "OnPauseMenuOpened",
        "OnPauseMenuClosed",
        "OnActiveTabChanged",
    ],
    "/Game/Blueprints/UI/WBP_StationInteraction": [
        "OnStationInteractionChanged",
        "OnStationInteractionClosed",
    ],
    "/Game/Blueprints/UI/WBP_SystemMap": [
        "OnMapOpened",
        "OnMapClosed",
        "OnMapViewChanged",
        "OnMapStatusChanged",
    ],
    "/Game/Blueprints/Space/BP_SystemSpacePresentation": [
        "OnConfiguredForSystem",
    ],
    "/Game/Blueprints/Space/BP_CombatShotPresentation": [
        "OnShotPresentationConfigured",
    ],
    "/Game/Blueprints/Space/BP_WayfarerTrafficShip": [
        "OnTrafficActorConfigured",
        "OnEncounterBehaviorConfigured",
    ],
    "/Game/Blueprints/Space/BP_SystemEncounterActor": [
        "OnTrafficActorConfigured",
        "OnEncounterBehaviorConfigured",
    ],
    "/Game/Blueprints/Station/BP_StationInteriorRoom": [
        "OnStationInteriorConfigured",
    ],
    "/Game/Blueprints/Station/BP_StationInteractable": [
        "OnInteractableConfigured",
        "OnFocusedByPlayerChanged",
    ],
    "/Game/Blueprints/Station/BP_MissionContact": [
        "OnContactConfigured",
        "OnFocusedByPlayerChanged",
        "OnPromptTextChanged",
    ],
    "/Game/Blueprints/Station/BP_StationHostile": [
        "OnHostileConfigured",
        "OnHostileHealthChanged",
        "OnHostileDied",
    ],
}


def run(action, payload, allow_fail=False):
    result = call(action, payload, timeout=45.0)
    response = result["response"]
    ok = response.get("success", False)
    if not ok and not allow_fail:
        print(json.dumps(result, indent=2))
        raise RuntimeError(f"{action} failed: {response.get('error') or response.get('message')}")
    status = "OK" if ok else "SKIP"
    target = payload.get("eventType") or payload.get("blueprintPath")
    print(f"{status} {action} {payload.get('action')} {target}: {response.get('message') or response.get('error')}")
    return ok


def add_event_hook(blueprint_path, event_name, index):
    payload = {
        "action": "add_event",
        "blueprintPath": blueprint_path,
        "eventType": event_name,
        "x": 80,
        "y": index * 190,
        "compile": True,
        "save": True,
    }
    run("manage_blueprint", payload, allow_fail=True)


def compile_blueprints():
    paths = sorted(EVENT_HOOKS.keys())
    code = (
        "import unreal\n"
        f"paths={paths!r}\n"
        "for path in paths:\n"
        "    bp=unreal.EditorAssetLibrary.load_asset(path)\n"
        "    if not bp:\n"
        "        raise RuntimeError(f'Failed to load {path}')\n"
        "    unreal.BlueprintEditorLibrary.compile_blueprint(bp)\n"
        "    unreal.EditorAssetLibrary.save_loaded_asset(bp)\n"
        "    print(f'Compiled and saved event-hook Blueprint {path}')\n"
    )
    run("system_control", {"action": "execute_python", "code": code})


def main():
    for blueprint_path, events in EVENT_HOOKS.items():
        for index, event_name in enumerate(events):
            add_event_hook(blueprint_path, event_name, index)
    compile_blueprints()
    print("Blueprint event hook pass complete.")


if __name__ == "__main__":
    main()
