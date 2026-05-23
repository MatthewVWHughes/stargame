using Starlight.Game.Missions;

namespace Starlight.Game.Runtime;

/// <summary>
/// Tiny runtime session holder for the VS1 slice.
/// Keeps scene handoff simple while the broader save/runtime architecture is still being built.
/// </summary>
public static class GameSession
{
    public enum EntryMode
    {
        NewGame,
        Continue,
        LaunchFromStation,
    }

    public static EntryMode PendingEntryMode { get; set; } = EntryMode.NewGame;
    public static string CurrentStationId { get; set; } = "";
    public static string PendingStatusMessage { get; set; } = "";

    // Multi-system state. Updated by gate transit; consumed by GameplayRoot
    // on the next scene load to pick which system to build and where to
    // place the ship.
    public static string CurrentSystemId { get; set; } = "";
    public static string PendingArrivalGateId { get; set; } = "";

    public static void StartNewGame()
    {
        PlayerState.ResetForNewGame();
        EconomyService.ResetForNewGame();
        MissionService.ResetForNewGame();
        PendingEntryMode = EntryMode.NewGame;
        CurrentStationId = "";
        PendingStatusMessage = "";
        CurrentSystemId = "";
        PendingArrivalGateId = "";
    }

    public static void JumpToSystem(string destinationSystemId, string arrivalGateId, string statusMessage = "")
    {
        CurrentSystemId = destinationSystemId;
        PendingArrivalGateId = arrivalGateId;
        if (!string.IsNullOrWhiteSpace(statusMessage))
            PendingStatusMessage = statusMessage;
    }

    public static void ContinueGame()
    {
        PendingEntryMode = EntryMode.Continue;
    }

    public static void DockAt(string stationId)
    {
        CurrentStationId = stationId;
    }

    public static void LaunchFrom(string stationId)
    {
        CurrentStationId = stationId;
        PendingEntryMode = EntryMode.LaunchFromStation;
    }

    public static string ConsumeStatusMessage()
    {
        string message = PendingStatusMessage;
        PendingStatusMessage = "";
        return message;
    }
}
