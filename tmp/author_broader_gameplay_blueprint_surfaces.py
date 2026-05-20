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
    target = payload.get("slotName") or payload.get("componentName") or payload.get("blueprintPath") or payload.get("file")
    print(f"{status} {action} {payload.get('action')} {target}: {response.get('message') or response.get('error')}")


def widget_call(widget_path, action, slot, **kwargs):
    payload = {"action": action, "widgetPath": widget_path, "slotName": slot}
    payload.update(kwargs)
    run("manage_widget_authoring", payload, allow_fail=True)


def place(widget_path, slot, x, y, sx, sy, preset="TopLeft", align=None):
    widget_call(widget_path, "set_anchor", slot, preset=preset)
    if align:
        widget_call(widget_path, "set_alignment", slot, alignment={"x": align[0], "y": align[1]})
    widget_call(widget_path, "set_position", slot, position={"x": x, "y": y})
    widget_call(widget_path, "set_size", slot, size={"x": sx, "y": sy})


def add_text(widget_path, slot, parent, text, font_size=16, wrap=True):
    widget_call(widget_path, "add_text_block", slot, parentSlot=parent, text=text, fontSize=font_size, autoWrap=wrap)


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


def set_transform(blueprint, component, location=None, rotation=None, scale=None):
    payload = {
        "action": "set_scs_transform",
        "blueprintPath": blueprint,
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


def author_flight_hud_surfaces():
    hud = "/Game/Blueprints/UI/WBP_FlightHUD"
    add_text(hud, "EncounterText", "RootCanvas", "", 13)
    place(hud, "EncounterText", -360, 86, 720, 32, preset="TopCenter", align=(0.5, 0.0))
    widget_call(hud, "set_visibility", "EncounterText", visibility="Collapsed")
    add_text(hud, "WeaponText", "RootCanvas", "", 13)
    place(hud, "WeaponText", -360, 124, 720, 32, preset="TopCenter", align=(0.5, 0.0))
    widget_call(hud, "set_visibility", "WeaponText", visibility="Collapsed")
    add_text(hud, "TrafficText", "RootCanvas", "", 13)
    place(hud, "TrafficText", -300, 166, 600, 30, preset="TopCenter", align=(0.5, 0.0))
    widget_call(hud, "set_visibility", "TrafficText", visibility="Collapsed")
    add_text(hud, "CommsLineText", "RootCanvas", "", 13)
    place(hud, "CommsLineText", -360, -128, 720, 30, preset="BottomCenter", align=(0.5, 1.0))
    widget_call(hud, "set_visibility", "CommsLineText", visibility="Collapsed")
    add_text(hud, "CargoTradeText", "RootCanvas", "", 13)
    place(hud, "CargoTradeText", -456, -52, 420, 28, preset="BottomRight", align=(1.0, 1.0))
    widget_call(hud, "set_visibility", "CargoTradeText", visibility="Collapsed")


def author_station_trade_surface():
    panel = "/Game/Blueprints/UI/WBP_StationInteraction"
    add_text(panel, "ActionListHintText", "RootCanvas", "", 13)
    place(panel, "ActionListHintText", -280, 128, 560, 34, preset="Center", align=(0.5, 0.5))
    widget_call(panel, "set_visibility", "ActionListHintText", visibility="Collapsed")
    add_text(panel, "MarketRowsHintText", "RootCanvas", "", 13)
    place(panel, "MarketRowsHintText", -280, 168, 560, 34, preset="Center", align=(0.5, 0.5))
    widget_call(panel, "set_visibility", "MarketRowsHintText", visibility="Collapsed")


def author_traffic_labels():
    for bp in ["/Game/Blueprints/Space/BP_WayfarerTrafficShip", "/Game/Blueprints/Space/BP_SystemEncounterActor"]:
        add_component(bp, "TextRenderComponent", "TrafficStateLabel")
        set_transform(bp, "TrafficStateLabel", location=(0, -86, 72), rotation=(0, 180, 0), scale=(0.5, 0.5, 0.5))
        add_component(bp, "PointLightComponent", "IntentGlow")
        set_transform(bp, "IntentGlow", location=(0, 0, 58), scale=(1, 1, 1))
    run("system_control", {"action": "execute_python", "file": "tmp/configure_broader_gameplay_surfaces.py"})


def compile_widgets():
    code = (
        "import unreal\n"
        "paths=['/Game/Blueprints/UI/WBP_FlightHUD','/Game/Blueprints/UI/WBP_StationInteraction']\n"
        "for path in paths:\n"
        "    bp=unreal.EditorAssetLibrary.load_asset(path)\n"
        "    if bp:\n"
        "        unreal.BlueprintEditorLibrary.compile_blueprint(bp)\n"
        "        unreal.EditorAssetLibrary.save_loaded_asset(bp)\n"
        "        print(f'Compiled and saved {path}')\n"
    )
    run("system_control", {"action": "execute_python", "code": code})


def main():
    author_flight_hud_surfaces()
    author_station_trade_surface()
    author_traffic_labels()
    compile_widgets()
    print("Broader gameplay Blueprint surfaces authored.")


if __name__ == "__main__":
    main()
