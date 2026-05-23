using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Game.Missions;
using Starlight.Game.Runtime;

namespace Starlight.Tests;

/// <summary>
/// End-to-end mission lifecycle tests that exercise Offer → Accept → AdvanceObjective → TurnIn.
/// Uses the real MissionCatalog data so changes to canonical mission shapes also validate here.
/// </summary>
[TestSuite]
public class MissionServiceFlowTests
{
    private const string MissionId = MissionCatalog.ClearDerelictOutpostId;

    private static void ResetWorld()
    {
        GameSession.StartNewGame();
    }

    [TestCase]
    public void Offer_MarksAsOffered()
    {
        ResetWorld();

        MissionService.Offer(MissionId);

        AssertThat(MissionService.GetStatus(MissionId)).IsEqual(MissionStatus.Offered);
    }

    [TestCase]
    public void Accept_OnlyTransitionsFromOffered()
    {
        ResetWorld();
        MissionService.Offer(MissionId);

        MissionService.Accept(MissionId);

        AssertThat(MissionService.GetStatus(MissionId)).IsEqual(MissionStatus.InProgress);
    }

    [TestCase]
    public void Accept_WithoutOffer_DoesNotAdvance()
    {
        ResetWorld();

        MissionService.Accept(MissionId);

        AssertThat(MissionService.GetStatus(MissionId)).IsEqual(MissionStatus.NotOffered);
    }

    [TestCase]
    public void AdvanceObjective_MatchingId_Advances()
    {
        ResetWorld();
        MissionService.Offer(MissionId);
        MissionService.Accept(MissionId);

        bool ok = MissionService.AdvanceObjective(MissionId, MissionCatalog.ObjectiveFlyToDerelict);

        AssertThat(ok).IsTrue();
        AssertThat(MissionService.GetObjectiveIndex(MissionId)).IsEqual(1);
        AssertThat(MissionService.GetStatus(MissionId)).IsEqual(MissionStatus.InProgress);
    }

    [TestCase]
    public void AdvanceObjective_WrongId_DoesNotAdvance()
    {
        ResetWorld();
        MissionService.Offer(MissionId);
        MissionService.Accept(MissionId);

        bool ok = MissionService.AdvanceObjective(MissionId, "some_other_objective");

        AssertThat(ok).IsFalse();
        AssertThat(MissionService.GetObjectiveIndex(MissionId)).IsEqual(0);
    }

    [TestCase]
    public void AdvanceObjective_PastLast_MovesToReadyToTurnIn()
    {
        ResetWorld();
        MissionService.Offer(MissionId);
        MissionService.Accept(MissionId);
        MissionService.AdvanceObjective(MissionId, MissionCatalog.ObjectiveFlyToDerelict);
        MissionService.AdvanceObjective(MissionId, MissionCatalog.ObjectiveClearHostiles);

        bool ok = MissionService.AdvanceObjective(MissionId, MissionCatalog.ObjectiveReturnToGiver);

        AssertThat(ok).IsTrue();
        AssertThat(MissionService.GetStatus(MissionId)).IsEqual(MissionStatus.ReadyToTurnIn);
    }

    [TestCase]
    public void TurnIn_ReadyMission_PaysCreditsAndCompletes()
    {
        ResetWorld();
        MissionService.Offer(MissionId);
        MissionService.Accept(MissionId);
        MissionService.AdvanceObjective(MissionId, MissionCatalog.ObjectiveFlyToDerelict);
        MissionService.AdvanceObjective(MissionId, MissionCatalog.ObjectiveClearHostiles);
        MissionService.AdvanceObjective(MissionId, MissionCatalog.ObjectiveReturnToGiver);
        float creditsBefore = PlayerState.Credits;
        float expectedReward = MissionCatalog.Get(MissionId).Reward.Credits;

        bool ok = MissionService.TurnIn(MissionId);

        AssertThat(ok).IsTrue();
        AssertThat(MissionService.GetStatus(MissionId)).IsEqual(MissionStatus.Completed);
        AssertThat(PlayerState.Credits).IsEqual(creditsBefore + expectedReward);
    }

    [TestCase]
    public void TurnIn_InProgressMission_Refuses()
    {
        ResetWorld();
        MissionService.Offer(MissionId);
        MissionService.Accept(MissionId);

        bool ok = MissionService.TurnIn(MissionId);

        AssertThat(ok).IsFalse();
        AssertThat(MissionService.GetStatus(MissionId)).IsEqual(MissionStatus.InProgress);
    }

    [TestCase]
    public void CaptureProgress_RoundTripsViaRestore()
    {
        ResetWorld();
        MissionService.Offer(MissionId);
        MissionService.Accept(MissionId);
        MissionService.AdvanceObjective(MissionId, MissionCatalog.ObjectiveFlyToDerelict);
        var snapshot = MissionService.CaptureProgress();

        MissionService.ResetForNewGame();
        AssertThat(MissionService.GetStatus(MissionId)).IsEqual(MissionStatus.NotOffered);

        MissionService.RestoreProgress(snapshot);

        AssertThat(MissionService.GetStatus(MissionId)).IsEqual(MissionStatus.InProgress);
        AssertThat(MissionService.GetObjectiveIndex(MissionId)).IsEqual(1);
    }

    [TestCase]
    public void FindMissionForGiver_StationMatch_ReturnsMission()
    {
        ResetWorld();

        MissionDefinition def = MissionService.FindMissionForGiver(
            MissionCatalog.SamKesslerNpcId, "EarthHub");

        AssertThat(def).IsNotNull();
        AssertThat(def.Id).IsEqual(MissionCatalog.ClearDerelictOutpostId);
    }

    [TestCase]
    public void FindMissionForGiver_WrongStation_ReturnsNull()
    {
        ResetWorld();

        MissionDefinition def = MissionService.FindMissionForGiver(
            MissionCatalog.SamKesslerNpcId, "MarsColony");

        AssertThat(def).IsNull();
    }

    [TestCase]
    public void FindMissionForGiver_DependencyNotMet_ReturnsNull()
    {
        ResetWorld();

        // SecureCeresApproach requires ReliefRun to be completed; it isn't.
        MissionDefinition def = MissionService.FindMissionForGiver(
            MissionCatalog.TomasValeNpcId, "CeresPort");

        AssertThat(def).IsNull();
    }
}
