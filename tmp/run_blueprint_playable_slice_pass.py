#!/usr/bin/env python3
import sys


STEPS = [
    ("local", "tmp/build_blueprint_assets.py"),
    ("local", "tmp/author_ui_widget_layouts.py"),
    ("local", "tmp/author_flight_hud_blueprint_ui.py"),
    ("local", "tmp/author_space_blueprint_visuals.py"),
    ("local", "tmp/author_milky_way_skybox.py"),
    ("local", "tmp/author_station_blueprint_visuals.py"),
    ("editor", "tmp/author_station_services.py"),
    ("local", "tmp/author_broader_gameplay_blueprint_surfaces.py"),
    ("local", "tmp/refine_space_hud_blueprint_ui_bridge.py"),
    ("local", "tmp/author_blueprint_event_hooks.py"),
    ("editor", "tmp/wire_blueprint_assets.py"),
    ("editor", "tmp/configure_playable_slice_map.py"),
    ("editor", "tmp/verify_blueprint_wiring.py"),
    ("editor", "tmp/validate_blueprint_assets.py"),
]


def run_local(step):
    import subprocess

    subprocess.run([sys.executable, step], check=True)


def run_editor(step):
    from ws_bridge_call import call

    result = call("system_control", {"action": "execute_python", "file": step}, timeout=90.0)
    response = result["response"]
    output = response.get("result", {}).get("output", "")
    error = response.get("result", {}).get("error", "")
    if output:
        print(output)
    if error:
        print(error, file=sys.stderr)
    if not response.get("success", False):
        raise RuntimeError(f"{step} failed: {response.get('error') or response.get('message')}")


def main():
    for mode, step in STEPS:
        print(f"\n=== {step} ({mode}) ===", flush=True)
        if mode == "editor":
            run_editor(step)
        else:
            run_local(step)
    print("\nBlueprint playable slice pass complete.")


if __name__ == "__main__":
    main()
