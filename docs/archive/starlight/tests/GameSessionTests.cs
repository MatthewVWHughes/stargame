using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Game.Runtime;

namespace Starlight.Tests;

[TestSuite]
public class GameSessionTests
{
    [TestCase]
    public void StartNewGame_ResetsAllSessionState()
    {
        GameSession.PendingEntryMode = GameSession.EntryMode.LaunchFromStation;
        GameSession.CurrentStationId = "MarsColony";
        GameSession.CurrentSystemId = "Sol";
        GameSession.PendingArrivalGateId = "Gate1";
        GameSession.PendingStatusMessage = "leftover";

        GameSession.StartNewGame();

        AssertThat(GameSession.PendingEntryMode).IsEqual(GameSession.EntryMode.NewGame);
        AssertThat(GameSession.CurrentStationId).IsEqual("");
        AssertThat(GameSession.CurrentSystemId).IsEqual("");
        AssertThat(GameSession.PendingArrivalGateId).IsEqual("");
        AssertThat(GameSession.PendingStatusMessage).IsEqual("");
    }

    [TestCase]
    public void DockAt_SetsCurrentStation()
    {
        GameSession.StartNewGame();

        GameSession.DockAt("EarthHub");

        AssertThat(GameSession.CurrentStationId).IsEqual("EarthHub");
    }

    [TestCase]
    public void ConsumeStatusMessage_ReturnsAndClears()
    {
        GameSession.StartNewGame();
        GameSession.PendingStatusMessage = "hello";

        string first = GameSession.ConsumeStatusMessage();
        string second = GameSession.ConsumeStatusMessage();

        AssertThat(first).IsEqual("hello");
        AssertThat(second).IsEqual("");
    }
}
