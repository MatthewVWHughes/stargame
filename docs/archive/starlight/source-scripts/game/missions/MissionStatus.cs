namespace Starlight.Game.Missions;

public enum MissionStatus
{
    /// <summary>Mission is known to the catalog but the player has not encountered it yet.</summary>
    NotOffered,

    /// <summary>Mission has been offered to the player; they have not accepted yet.</summary>
    Offered,

    /// <summary>Player has accepted the mission and is working through objectives.</summary>
    InProgress,

    /// <summary>All objectives complete; player needs to return to the giver to turn in.</summary>
    ReadyToTurnIn,

    /// <summary>Mission has been turned in and rewarded.</summary>
    Completed,
}
