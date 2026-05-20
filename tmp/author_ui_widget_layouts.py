#!/usr/bin/env python3
import json

from ws_bridge_call import call


UI = "/Game/Blueprints/UI"


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


def widget(name):
    return f"{UI}/{name}"


def wcall(widget_path, subaction, slot, **kwargs):
    payload = {"action": subaction, "widgetPath": widget_path, "slotName": slot}
    payload.update(kwargs)
    run("manage_widget_authoring", payload, allow_fail=True)


def place(widget_path, slot, x, y, sx, sy, preset="TopLeft", align=None):
    wcall(widget_path, "set_anchor", slot, preset=preset)
    if align:
        wcall(widget_path, "set_alignment", slot, alignment={"x": align[0], "y": align[1]})
    wcall(widget_path, "set_position", slot, position={"x": x, "y": y})
    wcall(widget_path, "set_size", slot, size={"x": sx, "y": sy})


def button_label(widget_path, button, label):
    wcall(widget_path, "add_text_block", f"{button}Label", parentSlot=button, text=label, fontSize=18)


def boot_menu():
    w = widget("WBP_BootMenu")
    place(w, "TitleText", 64, 70, 520, 70)
    place(w, "StatusText", 64, 138, 520, 40)
    for index, (button, label) in enumerate([
        ("NewGameButton", "New Game"),
        ("ContinueButton", "Continue"),
        ("QuitButton", "Quit"),
    ]):
        place(w, button, 64, 220 + index * 74, 280, 54)
        button_label(w, button, label)


def station_interaction():
    w = widget("WBP_StationInteraction")
    place(w, "TitleText", -280, -190, 560, 42, preset="Center", align=(0.5, 0.5))
    place(w, "BodyText", -280, -130, 560, 120, preset="Center", align=(0.5, 0.5))
    place(w, "StatusText", -280, 0, 560, 36, preset="Center", align=(0.5, 0.5))
    place(w, "PrimaryButton", -280, 58, 260, 54, preset="Center", align=(0.5, 0.5))
    place(w, "CloseButton", 20, 58, 260, 54, preset="Center", align=(0.5, 0.5))
    button_label(w, "PrimaryButton", "Confirm")
    button_label(w, "CloseButton", "Close")


def comms():
    w = widget("WBP_Comms")
    place(w, "TitleText", 48, 46, 520, 44)
    place(w, "BodyText", 48, 104, 560, 150)
    place(w, "StatusText", 48, 270, 560, 34)
    place(w, "RequestDockingButton", 48, 330, 240, 54)
    place(w, "CloseButton", 310, 330, 170, 54)
    button_label(w, "RequestDockingButton", "Request Docking")
    button_label(w, "CloseButton", "Close")


def pause_menu():
    w = widget("WBP_PauseMenu")
    place(w, "TitleText", -320, -230, 640, 48, preset="Center", align=(0.5, 0.5))
    for index, (button, label) in enumerate([
        ("InventoryTabButton", "Inventory"),
        ("MissionsTabButton", "Missions"),
        ("SettingsTabButton", "Settings"),
    ]):
        place(w, button, -320 + index * 212, -160, 190, 46, preset="Center", align=(0.5, 0.5))
        button_label(w, button, label)
    place(w, "BodyText", -320, -90, 640, 220, preset="Center", align=(0.5, 0.5))
    place(w, "FooterText", -320, 150, 420, 34, preset="Center", align=(0.5, 0.5))
    place(w, "ResumeButton", 110, 150, 210, 50, preset="Center", align=(0.5, 0.5))
    button_label(w, "ResumeButton", "Resume")


def dialog():
    w = widget("WBP_Dialog")
    place(w, "TitleText", -280, -170, 560, 44, preset="Center", align=(0.5, 0.5))
    place(w, "BodyText", -280, -110, 560, 180, preset="Center", align=(0.5, 0.5))
    place(w, "StatusText", -280, 86, 560, 36, preset="Center", align=(0.5, 0.5))


def system_map():
    w = widget("WBP_SystemMap")
    place(w, "TitleText", 36, 28, 460, 44)
    place(w, "StatusText", 36, 82, 760, 34)


def flight_hud():
    w = widget("WBP_FlightHUD")
    place(w, "SpeedText", 36, -92, 220, 32, preset="BottomLeft")
    place(w, "TargetText", -300, 32, 600, 32, preset="TopCenter", align=(0.5, 0))
    place(w, "DockingText", -300, -86, 600, 28, preset="BottomCenter", align=(0.5, 1))
    place(w, "MissionText", 36, -52, 420, 28, preset="BottomLeft")


def save_widgets():
    code = (
        "import unreal\n"
        f"paths={[widget(name) for name in ['WBP_BootMenu','WBP_StationInteraction','WBP_Comms','WBP_PauseMenu','WBP_Dialog','WBP_SystemMap','WBP_FlightHUD']]!r}\n"
        "for path in paths:\n"
        "    bp=unreal.EditorAssetLibrary.load_asset(path)\n"
        "    if bp:\n"
        "        unreal.BlueprintEditorLibrary.compile_blueprint(bp)\n"
        "        unreal.EditorAssetLibrary.save_loaded_asset(bp)\n"
        "        print(f'Compiled and saved {path}')\n"
    )
    run("system_control", {"action": "execute_python", "code": code})


def main():
    boot_menu()
    station_interaction()
    comms()
    pause_menu()
    dialog()
    system_map()
    flight_hud()
    save_widgets()
    print("UI widget layout authoring pass complete.")


if __name__ == "__main__":
    main()
