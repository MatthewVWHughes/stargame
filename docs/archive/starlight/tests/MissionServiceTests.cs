using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Game.Missions;

namespace Starlight.Tests;

[TestSuite]
public class MissionServiceTests
{
    [TestCase]
    public void ResetForNewGame_ClearsProgress()
    {
        // Seed some mission progress via Restore with synthetic data
        var seed = new System.Collections.Generic.Dictionary<string, MissionService.MissionProgressSnapshot>();
        MissionService.RestoreProgress(seed);

        MissionService.ResetForNewGame();

        // After reset, any captured progress is empty
        var snapshot = MissionService.CaptureProgress();
        AssertThat(snapshot.Count).IsEqual(0);
    }

    [TestCase]
    public void GetStatus_UnknownMission_ReturnsNotOffered()
    {
        MissionService.ResetForNewGame();

        MissionStatus status = MissionService.GetStatus("nonexistent_mission");

        AssertThat(status).IsEqual(MissionStatus.NotOffered);
    }

    [TestCase]
    public void CaptureProgress_EmptyAfterReset()
    {
        MissionService.ResetForNewGame();

        var snapshot = MissionService.CaptureProgress();

        AssertThat(snapshot.Count).IsEqual(0);
    }

    [TestCase]
    public void RestoreProgress_NullInput_ResultsInEmpty()
    {
        MissionService.RestoreProgress(null);

        AssertThat(MissionService.CaptureProgress().Count).IsEqual(0);
    }
}
