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
    target = payload.get("slotName") or payload.get("widgetPath") or payload.get("file")
    print(f"{status} {action} {payload.get('action')} {target}: {response.get('message') or response.get('error')}")


def widget_call(widget_path, action, slot, allow_fail=True, **kwargs):
    payload = {"action": action, "widgetPath": widget_path, "slotName": slot}
    payload.update(kwargs)
    run("manage_widget_authoring", payload, allow_fail=allow_fail)


def hide(widget_path, *slots):
    for slot in slots:
        widget_call(widget_path, "set_visibility", slot, visibility="Collapsed")


def show_passive(widget_path, slot, font_size=13):
    widget_call(widget_path, "set_visibility", slot, visibility="SelfHitTestInvisible")
    widget_call(widget_path, "set_font", slot, fontSize=font_size)


def save_widgets(paths):
    code = (
        "import unreal\n"
        f"paths={paths!r}\n"
        "for path in paths:\n"
        "    bp=unreal.EditorAssetLibrary.load_asset(path)\n"
        "    if bp:\n"
        "        unreal.BlueprintEditorLibrary.compile_blueprint(bp)\n"
        "        unreal.EditorAssetLibrary.save_loaded_asset(bp)\n"
        "        print(f'Compiled and saved {path}')\n"
    )
    run("system_control", {"action": "execute_python", "code": code})


def main():
    hud = "/Game/Blueprints/UI/WBP_FlightHUD"
    # These are staging surfaces for later Blueprint graph work. They should not
    # be visible in the default flight view until driven by real state.
    hide(
        hud,
        "TargetText",
        "DockingText",
        "MissionText",
        "EncounterText",
        "WeaponText",
        "TrafficText",
        "CommsLineText",
        "CargoTradeText",
    )
    show_passive(hud, "SpeedText", 13)

    station = "/Game/Blueprints/UI/WBP_StationInteraction"
    hide(station, "ActionListHintText", "MarketRowsHintText")

    for widget_path in [
        "/Game/Blueprints/UI/WBP_Comms",
        "/Game/Blueprints/UI/WBP_Dialog",
        "/Game/Blueprints/UI/WBP_PauseMenu",
        "/Game/Blueprints/UI/WBP_StationInteraction",
        "/Game/Blueprints/UI/WBP_SystemMap",
    ]:
        for slot in ["TitleText", "BodyText", "StatusText", "FooterText"]:
            widget_call(widget_path, "set_font", slot, fontSize=14, allow_fail=True)

    save_widgets(
        [
            hud,
            station,
            "/Game/Blueprints/UI/WBP_Comms",
            "/Game/Blueprints/UI/WBP_Dialog",
            "/Game/Blueprints/UI/WBP_PauseMenu",
            "/Game/Blueprints/UI/WBP_SystemMap",
        ]
    )
    print("Sparse default flight HUD and panel text refinement complete.")


if __name__ == "__main__":
    main()
