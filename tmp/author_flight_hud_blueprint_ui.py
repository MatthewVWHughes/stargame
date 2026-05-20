#!/usr/bin/env python3
import json

from ws_bridge_call import call


HUD = "/Game/Blueprints/UI/WBP_FlightHUD"


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
    return ok


def wcall(action, slot, **kwargs):
    payload = {"action": action, "widgetPath": HUD, "slotName": slot}
    payload.update(kwargs)
    run("manage_widget_authoring", payload, allow_fail=True)


def place(slot, x, y, sx, sy, preset="TopLeft", align=None):
    wcall("set_anchor", slot, preset=preset)
    if align:
        wcall("set_alignment", slot, alignment={"x": align[0], "y": align[1]})
    wcall("set_position", slot, position={"x": x, "y": y})
    wcall("set_size", slot, size={"x": sx, "y": sy})
    wcall("set_visibility", slot, visibility="SelfHitTestInvisible")


def add_text(slot, text, size=13):
    wcall("add_text_block", slot, parentSlot="RootCanvas", text=text, fontSize=size, autoWrap=False)
    wcall("set_font", slot, fontSize=size)


def add_bar(slot, percent, color):
    wcall(
        "add_progress_bar",
        slot,
        parentSlot="RootCanvas",
        percent=percent,
        fillColorAndOpacity=color,
    )


def save():
    code = (
        "import unreal\n"
        f"path={HUD!r}\n"
        "bp=unreal.EditorAssetLibrary.load_asset(path)\n"
        "if not bp:\n"
        "    raise RuntimeError(f'Failed to load {path}')\n"
        "unreal.BlueprintEditorLibrary.compile_blueprint(bp)\n"
        "unreal.EditorAssetLibrary.save_loaded_asset(bp)\n"
        "print(f'Compiled and saved {path}')\n"
    )
    run("system_control", {"action": "execute_python", "code": code})


def main():
    run(
        "manage_widget_authoring",
        {
            "action": "set_widget_parent_class",
            "widgetPath": HUD,
            "parentClass": "/Script/Stargame.StargameFlightHUDWidget",
        },
    )

    # Core flight strip.
    add_text("SpeedText", "SPD 00000 m/s  ACC 0", 14)
    place("SpeedText", 36, -94, 300, 28, preset="BottomLeft")
    add_text("ThrottleText", "THR 000%", 13)
    place("ThrottleText", 36, -62, 120, 24, preset="BottomLeft")
    add_bar("ThrottleBar", 0.0, {"r": 0.20, "g": 0.74, "b": 1.0, "a": 0.86})
    place("ThrottleBar", 160, -58, 176, 14, preset="BottomLeft")
    add_text("FlightModeText", "NORMAL", 13)
    place("FlightModeText", 36, -34, 240, 24, preset="BottomLeft")

    # Top target and docking bands.
    add_text("TargetText", "TARGET NONE", 14)
    place("TargetText", -350, 32, 700, 28, preset="TopCenter", align=(0.5, 0.0))
    add_text("DockingText", "DOCKING IDLE", 13)
    place("DockingText", -310, 64, 620, 24, preset="TopCenter", align=(0.5, 0.0))

    # Combat cluster.
    add_text("WeaponText", "PRIMARY READY NO LOCK", 13)
    place("WeaponText", -342, -94, 330, 24, preset="BottomRight", align=(1.0, 1.0))
    add_bar("WeaponEnergyBar", 1.0, {"r": 0.22, "g": 1.0, "b": 0.58, "a": 0.88})
    place("WeaponEnergyBar", -342, -64, 330, 12, preset="BottomRight", align=(1.0, 1.0))
    add_bar("WeaponCooldownBar", 1.0, {"r": 1.0, "g": 0.78, "b": 0.22, "a": 0.88})
    place("WeaponCooldownBar", -342, -42, 330, 10, preset="BottomRight", align=(1.0, 1.0))

    # Radar and ship state.
    add_text("RadarText", "RADAR 0 TARGETS / 0 TRAFFIC", 13)
    place("RadarText", -242, -174, 230, 24, preset="BottomRight", align=(1.0, 1.0))
    add_text("SystemsText", "SHD / HULL / FUEL", 12)
    place("SystemsText", 36, 32, 190, 24, preset="TopLeft")
    add_bar("ShieldBar", 1.0, {"r": 0.32, "g": 0.72, "b": 1.0, "a": 0.82})
    place("ShieldBar", 36, 60, 170, 10, preset="TopLeft")
    add_bar("HullBar", 1.0, {"r": 0.28, "g": 1.0, "b": 0.52, "a": 0.82})
    place("HullBar", 36, 78, 170, 10, preset="TopLeft")
    add_bar("FuelBar", 1.0, {"r": 1.0, "g": 0.76, "b": 0.28, "a": 0.82})
    place("FuelBar", 36, 96, 170, 10, preset="TopLeft")

    # Keep staged text surfaces hidden until a Blueprint graph opts into them.
    for slot in ["EncounterText", "TrafficText", "CommsLineText", "CargoTradeText", "MissionText"]:
        wcall("set_visibility", slot, visibility="Collapsed")

    save()
    print("Flight HUD Blueprint UI authored.")


if __name__ == "__main__":
    main()
