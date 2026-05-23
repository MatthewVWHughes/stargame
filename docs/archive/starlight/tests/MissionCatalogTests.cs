using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Game.Missions;

namespace Starlight.Tests;

[TestSuite]
public class MissionCatalogTests
{
    [TestCase]
    public void Get_KnownId_ReturnsDefinition()
    {
        MissionDefinition def = MissionCatalog.Get(MissionCatalog.ClearDerelictOutpostId);

        AssertThat(def).IsNotNull();
        AssertThat(def.Id).IsEqual(MissionCatalog.ClearDerelictOutpostId);
        AssertThat(def.Title).IsEqual("Clear the Derelict Outpost");
    }

    [TestCase]
    public void Get_UnknownId_ReturnsNull()
    {
        AssertThat(MissionCatalog.Get("nonexistent_mission")).IsNull();
    }

    [TestCase]
    public void All_ContainsExpectedMissions()
    {
        var all = MissionCatalog.All;

        AssertThat(all.ContainsKey(MissionCatalog.ClearDerelictOutpostId)).IsTrue();
        AssertThat(all.ContainsKey(MissionCatalog.MartianFeedstockContractId)).IsTrue();
        AssertThat(all.ContainsKey(MissionCatalog.ReliefRunToCeresId)).IsTrue();
        AssertThat(all.ContainsKey(MissionCatalog.SecureCeresApproachId)).IsTrue();
        AssertThat(all.ContainsKey(MissionCatalog.RecoverOutpostManifestId)).IsTrue();
    }

    [TestCase]
    public void ClearDerelictOutpost_HasThreeObjectives()
    {
        MissionDefinition def = MissionCatalog.Get(MissionCatalog.ClearDerelictOutpostId);

        AssertThat(def.Objectives.Count).IsEqual(3);
        AssertThat(def.Objectives[0].Id).IsEqual(MissionCatalog.ObjectiveFlyToDerelict);
        AssertThat(def.Objectives[1].Id).IsEqual(MissionCatalog.ObjectiveClearHostiles);
        AssertThat(def.Objectives[2].Id).IsEqual(MissionCatalog.ObjectiveReturnToGiver);
    }

    [TestCase]
    public void SecureCeresApproach_RequiresReliefRun()
    {
        MissionDefinition def = MissionCatalog.Get(MissionCatalog.SecureCeresApproachId);

        AssertThat(def.RequiredCompletedMissionIds).IsNotNull();
        AssertThat(def.RequiredCompletedMissionIds).Contains(MissionCatalog.ReliefRunToCeresId);
    }

    [TestCase]
    public void MartianFeedstock_HasCreditsReward()
    {
        MissionDefinition def = MissionCatalog.Get(MissionCatalog.MartianFeedstockContractId);

        AssertThat(def.Reward).IsNotNull();
        AssertThat(def.Reward.Credits).IsEqual(700f);
    }
}
