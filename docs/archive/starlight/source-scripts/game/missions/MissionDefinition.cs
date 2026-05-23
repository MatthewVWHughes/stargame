using System.Collections.Generic;

namespace Starlight.Game.Missions;

public enum MissionObjectiveKind
{
    ReachStation,
    DeliverCargo,
    ClearHostileWing,
    ClearStationHostiles,
    ReturnToGiver,
}

public sealed record MissionEncounterDefinition(
    string Id,
    string DisplayName,
    string AnchorStationId,
    int WingSize,
    float ActivationRange,
    float SpawnRadiusMin,
    float SpawnRadiusMax);

public sealed record MissionObjective(
    string Id,
    string Text,
    MissionObjectiveKind Kind,
    string WaypointStationId = "",
    string RequiredCommodityId = "",
    float RequiredCommodityAmount = 0f,
    MissionEncounterDefinition Encounter = null);

public sealed record MissionReward(float Credits);

public sealed record MissionDefinition(
    string Id,
    string Title,
    string Summary,
    string GiverNpcId,
    string GiverStationId,
    string IssuerFactionId,
    string RegionName,
    IReadOnlyList<MissionObjective> Objectives,
    MissionReward Reward,
    string OfferDialog,
    string AcceptedDialog,
    string InProgressDialog,
    string TurnInDialog,
    IReadOnlyList<string> RequiredCompletedMissionIds = null,
    IReadOnlyList<string> RequiredStationTags = null);
