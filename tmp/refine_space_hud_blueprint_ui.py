import unreal


UI_BLUEPRINTS = [
    "/Game/Blueprints/UI/WBP_BootMenu",
    "/Game/Blueprints/UI/WBP_Comms",
    "/Game/Blueprints/UI/WBP_Dialog",
    "/Game/Blueprints/UI/WBP_PauseMenu",
    "/Game/Blueprints/UI/WBP_StationInteraction",
    "/Game/Blueprints/UI/WBP_SystemMap",
    "/Game/Blueprints/UI/WBP_FlightHUD",
]

HUD_TEXT = {
    "SpeedText": ("0 m/s", 14, unreal.LinearColor(0.68, 0.86, 1.0, 0.78)),
    "TargetText": ("", 13, unreal.LinearColor(0.78, 0.92, 1.0, 0.72)),
    "DockingText": ("", 13, unreal.LinearColor(0.92, 0.78, 0.46, 0.76)),
    "MissionText": ("", 13, unreal.LinearColor(0.78, 0.88, 0.76, 0.70)),
}

HUD_HIDDEN_TEXT = [
    "EncounterText",
    "WeaponText",
    "TrafficText",
    "CommsLineText",
    "CargoTradeText",
]

PANEL_TEXT = {
    "/Game/Blueprints/UI/WBP_Comms": {
        "TitleText": ("Comms", 22),
        "BodyText": ("", 15),
        "StatusText": ("", 13),
    },
    "/Game/Blueprints/UI/WBP_Dialog": {
        "TitleText": ("", 21),
        "BodyText": ("", 15),
        "StatusText": ("", 13),
    },
    "/Game/Blueprints/UI/WBP_PauseMenu": {
        "TitleText": ("Paused", 24),
        "BodyText": ("", 15),
        "FooterText": ("", 12),
    },
    "/Game/Blueprints/UI/WBP_StationInteraction": {
        "TitleText": ("Station", 22),
        "BodyText": ("", 15),
        "StatusText": ("", 13),
        "ActionListHintText": ("", 13),
        "MarketRowsHintText": ("", 13),
    },
    "/Game/Blueprints/UI/WBP_SystemMap": {
        "TitleText": ("System Map", 22),
        "StatusText": ("", 13),
    },
}


def find_widget(blueprint, name):
    tree = blueprint.get_editor_property("widget_tree")
    if not tree:
        return None
    return tree.find_widget(name)


def set_text(widget, value):
    if hasattr(widget, "set_text"):
        widget.set_text(unreal.Text(value))
    else:
        widget.set_editor_property("text", unreal.Text(value))


def set_font_size(widget, size):
    if not hasattr(widget, "get_font"):
        return
    font = widget.get_font()
    font.size = size
    widget.set_font(font)


def set_text_color(widget, color):
    if hasattr(widget, "set_color_and_opacity"):
        widget.set_color_and_opacity(unreal.SlateColor(color))
    else:
        widget.set_editor_property("color_and_opacity", unreal.SlateColor(color))


def set_visibility(widget, visibility):
    widget.set_visibility(visibility)


def refine_flight_hud():
    blueprint = unreal.EditorAssetLibrary.load_asset("/Game/Blueprints/UI/WBP_FlightHUD")
    if not blueprint:
        raise RuntimeError("Failed to load WBP_FlightHUD")

    for name, (text, size, color) in HUD_TEXT.items():
        widget = find_widget(blueprint, name)
        if not widget:
            continue
        set_text(widget, text)
        set_font_size(widget, size)
        set_text_color(widget, color)
        set_visibility(widget, unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)

    for name in HUD_HIDDEN_TEXT:
        widget = find_widget(blueprint, name)
        if not widget:
            continue
        set_text(widget, "")
        set_visibility(widget, unreal.SlateVisibility.COLLAPSED)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    print("Refined sparse default flight HUD")


def refine_panel_text():
    muted = unreal.LinearColor(0.78, 0.86, 0.92, 0.86)
    for path, widgets in PANEL_TEXT.items():
        blueprint = unreal.EditorAssetLibrary.load_asset(path)
        if not blueprint:
            raise RuntimeError(f"Failed to load {path}")
        for name, (text, size) in widgets.items():
            widget = find_widget(blueprint, name)
            if not widget:
                continue
            set_text(widget, text)
            set_font_size(widget, size)
            set_text_color(widget, muted)
            if name in ("ActionListHintText", "MarketRowsHintText"):
                set_visibility(widget, unreal.SlateVisibility.COLLAPSED)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
        print(f"Refined text defaults for {path}")


def main():
    refine_flight_hud()
    refine_panel_text()


if __name__ == "__main__":
    main()
