#!/usr/bin/env python3
from ws_bridge_call import call

assets = [
    "/Game/Blueprints/Core/BP_StargameGameMode",
    "/Game/Blueprints/Core/BP_StargamePlayerController",
    "/Game/Blueprints/Flight/BP_WayfarerFlightPawn",
    "/Game/Blueprints/Space/BP_SystemSpacePresentation",
    "/Game/Blueprints/Space/BP_SectorStarAnchor",
    "/Game/Blueprints/Space/BP_SystemEntityMarker",
    "/Game/Blueprints/Space/BP_CombatShotPresentation",
    "/Game/Blueprints/Space/BP_WayfarerTrafficShip",
    "/Game/Blueprints/Space/BP_SystemEncounterActor",
    "/Game/Blueprints/Station/BP_StationInteriorRoom",
    "/Game/Blueprints/Station/BP_StationInteriorPawn",
    "/Game/Blueprints/Station/BP_StationInteractable",
    "/Game/Blueprints/Station/BP_MissionContact",
    "/Game/Blueprints/Station/BP_StationHostile",
    "/Game/Blueprints/UI/WBP_BootMenu",
    "/Game/Blueprints/UI/WBP_Comms",
    "/Game/Blueprints/UI/WBP_Dialog",
    "/Game/Blueprints/UI/WBP_PauseMenu",
    "/Game/Blueprints/UI/WBP_StationInteraction",
    "/Game/Blueprints/UI/WBP_SystemMap",
    "/Game/Blueprints/UI/WBP_FlightHUD",
]

failed = []
for asset in assets:
    result = call("manage_asset", {"action": "validate", "assetPath": asset}, timeout=90.0)
    response = result["response"]
    ok = response.get("success", False)
    print(("OK" if ok else "FAIL"), asset, response.get("message") or response.get("error"), flush=True)
    if not ok:
        failed.append(asset)

if failed:
    raise SystemExit(f"Validation failed: {failed}")
