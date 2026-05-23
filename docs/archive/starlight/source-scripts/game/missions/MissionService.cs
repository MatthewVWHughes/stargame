using Godot;
using System;
using System.Collections.Generic;
using Starlight.Game.Inventory;
using Starlight.Game.Runtime;

namespace Starlight.Game.Missions;

/// <summary>
/// Static mission state service for VS1.
/// Holds per-mission status + current objective index, and raises a single event when anything changes
/// so UI can refresh without polling.
///
/// Systems that observe player actions (docking, kills, dialog) call the explicit advance/turn-in APIs —
/// this keeps trigger logic next to the behavior that detects it, while state + events live in one place.
/// </summary>
public static class MissionService
{
    public sealed record MissionProgressSnapshot(MissionStatus Status, int ObjectiveIndex);
    public sealed record ActiveEncounterRequest(
        string MissionId,
        string ObjectiveId,
        string WaypointStationId,
        MissionEncounterDefinition Encounter);

    private sealed class MissionEntry
    {
        public MissionStatus Status { get; set; } = MissionStatus.NotOffered;
        public int ObjectiveIndex { get; set; }
    }

    private static readonly Dictionary<string, MissionEntry> s_entries = new();

    /// <summary>Raised whenever any mission's status or objective index changes.</summary>
    public static event Action Changed;

    public static void ResetForNewGame()
    {
        s_entries.Clear();
        Changed?.Invoke();
    }

    public static Dictionary<string, MissionProgressSnapshot> CaptureProgress()
    {
        var snapshot = new Dictionary<string, MissionProgressSnapshot>();
        foreach ((string missionId, MissionEntry entry) in s_entries)
            snapshot[missionId] = new MissionProgressSnapshot(entry.Status, entry.ObjectiveIndex);
        return snapshot;
    }

    public static void RestoreProgress(Dictionary<string, MissionProgressSnapshot> progress)
    {
        s_entries.Clear();
        if (progress == null)
        {
            Changed?.Invoke();
            return;
        }

        foreach ((string missionId, MissionProgressSnapshot snapshot) in progress)
        {
            MissionDefinition def = MissionCatalog.Get(missionId);
            if (def == null || snapshot == null)
                continue;

            int clampedIndex = Mathf.Clamp(snapshot.ObjectiveIndex, 0, def.Objectives.Count);
            s_entries[missionId] = new MissionEntry
            {
                Status = snapshot.Status,
                ObjectiveIndex = clampedIndex,
            };
        }

        Changed?.Invoke();
    }

    public static MissionStatus GetStatus(string missionId)
    {
        return s_entries.TryGetValue(missionId, out MissionEntry entry)
            ? entry.Status
            : MissionStatus.NotOffered;
    }

    public static int GetObjectiveIndex(string missionId)
    {
        return s_entries.TryGetValue(missionId, out MissionEntry entry) ? entry.ObjectiveIndex : 0;
    }

    public static MissionObjective GetActiveObjective(string missionId)
    {
        MissionDefinition def = MissionCatalog.Get(missionId);
        if (def == null)
            return null;

        int index = GetObjectiveIndex(missionId);
        return index >= 0 && index < def.Objectives.Count ? def.Objectives[index] : null;
    }

    public static string GetWaypointStationId(string missionId)
    {
        MissionDefinition def = MissionCatalog.Get(missionId);
        MissionObjective objective = GetActiveObjective(missionId);
        if (def == null)
            return null;

        if (GetStatus(missionId) == MissionStatus.ReadyToTurnIn)
            return def.GiverStationId;

        return objective?.WaypointStationId;
    }

    public static IEnumerable<ActiveEncounterRequest> ActiveEncounterRequests()
    {
        foreach ((MissionDefinition mission, MissionStatus status) in ActiveMissions())
        {
            if (status != MissionStatus.InProgress)
                continue;

            MissionObjective active = GetActiveObjective(mission.Id);
            if (active?.Kind != MissionObjectiveKind.ClearHostileWing || active.Encounter == null)
                continue;

            yield return new ActiveEncounterRequest(
                mission.Id,
                active.Id,
                active.WaypointStationId,
                active.Encounter);
        }
    }

    /// <summary>Returns all missions in Offered / InProgress / ReadyToTurnIn states.</summary>
    public static IEnumerable<(MissionDefinition Mission, MissionStatus Status)> ActiveMissions()
    {
        foreach ((string id, MissionEntry entry) in s_entries)
        {
            if (entry.Status == MissionStatus.NotOffered || entry.Status == MissionStatus.Completed)
                continue;

            MissionDefinition def = MissionCatalog.Get(id);
            if (def != null)
                yield return (def, entry.Status);
        }
    }

    /// <summary>First not-yet-turned-in mission this NPC is tied to, or null.</summary>
    public static MissionDefinition FindMissionForGiver(string npcId, string stationId)
    {
        foreach (MissionDefinition def in MissionCatalog.All.Values)
        {
            if (def.GiverNpcId != npcId)
                continue;
            if (def.GiverStationId != stationId)
                continue;
            if (GetStatus(def.Id) == MissionStatus.Completed)
                continue;
            if (!CanOfferOrContinue(def, stationId))
                continue;
            return def;
        }
        return null;
    }

    public static string BuildMissionContextText(MissionDefinition mission)
    {
        if (mission == null)
            return "";

        var lines = new List<string>();
        if (!string.IsNullOrWhiteSpace(mission.RegionName))
            lines.Add($"Region: {mission.RegionName}");

        if (!string.IsNullOrWhiteSpace(mission.IssuerFactionId))
        {
            var faction = Stations.StationRegistry.GetFaction(mission.IssuerFactionId);
            lines.Add($"Issuer: {faction?.DisplayName ?? mission.IssuerFactionId}");
        }

        return string.Join("\n", lines);
    }

    /// <summary>Marks a mission as Offered so it appears in dialog. Idempotent.</summary>
    public static void Offer(string missionId)
    {
        MissionEntry entry = GetOrCreate(missionId);
        if (entry.Status != MissionStatus.NotOffered)
            return;

        entry.Status = MissionStatus.Offered;
        entry.ObjectiveIndex = 0;
        Changed?.Invoke();
        MessageBus.Publish(new GameEvents.MissionOffered(missionId));
    }

    public static void Accept(string missionId)
    {
        MissionEntry entry = GetOrCreate(missionId);
        if (entry.Status != MissionStatus.Offered)
            return;

        entry.Status = MissionStatus.InProgress;
        entry.ObjectiveIndex = 0;
        Changed?.Invoke();
        MessageBus.Publish(new GameEvents.MissionAccepted(missionId));
    }

    /// <summary>
    /// Advances the current objective if it matches <paramref name="expectedObjectiveId"/>.
    /// Callers pass the objective id they believe they're completing so a stale event can't skip ahead.
    /// If the advance lands past the last objective, the mission moves to ReadyToTurnIn.
    /// </summary>
    public static bool AdvanceObjective(string missionId, string expectedObjectiveId)
    {
        MissionDefinition def = MissionCatalog.Get(missionId);
        if (def == null)
            return false;
        if (!s_entries.TryGetValue(missionId, out MissionEntry entry))
            return false;
        if (entry.Status != MissionStatus.InProgress)
            return false;
        if (entry.ObjectiveIndex >= def.Objectives.Count)
            return false;

        MissionObjective active = def.Objectives[entry.ObjectiveIndex];
        if (!string.IsNullOrEmpty(expectedObjectiveId) && active.Id != expectedObjectiveId)
            return false;

        entry.ObjectiveIndex++;
        bool nowReady = entry.ObjectiveIndex >= def.Objectives.Count;
        if (nowReady)
            entry.Status = MissionStatus.ReadyToTurnIn;

        Changed?.Invoke();
        MessageBus.Publish(new GameEvents.MissionObjectiveAdvanced(missionId, entry.ObjectiveIndex));
        if (nowReady)
            MessageBus.Publish(new GameEvents.MissionReadyToTurnIn(missionId));
        return true;
    }

    public static bool TurnIn(string missionId)
    {
        MissionDefinition def = MissionCatalog.Get(missionId);
        if (def == null)
            return false;
        if (!s_entries.TryGetValue(missionId, out MissionEntry entry))
            return false;
        if (entry.Status != MissionStatus.ReadyToTurnIn)
            return false;

        entry.Status = MissionStatus.Completed;
        float credits = def.Reward?.Credits ?? 0f;
        if (credits > 0f)
            PlayerState.AddCredits(credits);

        Changed?.Invoke();
        MessageBus.Publish(new GameEvents.MissionCompleted(missionId, credits));
        return true;
    }

    public static bool TryAdvanceDockedObjectives(string stationId, out string statusMessage)
    {
        statusMessage = "";
        bool advancedAny = false;

        foreach ((MissionDefinition mission, MissionStatus status) in ActiveMissions())
        {
            if (status != MissionStatus.InProgress)
                continue;

            MissionObjective active = GetActiveObjective(mission.Id);
            if (active == null || active.WaypointStationId != stationId)
                continue;

            switch (active.Kind)
            {
                case MissionObjectiveKind.ReachStation:
                case MissionObjectiveKind.ReturnToGiver:
                    advancedAny |= AdvanceObjective(mission.Id, active.Id);
                    if (advancedAny)
                        statusMessage = $"Mission updated: {mission.Title}.";
                    break;

                case MissionObjectiveKind.DeliverCargo:
                    if (TryDeliverCargo(active, out string deliveryMessage))
                    {
                        advancedAny |= AdvanceObjective(mission.Id, active.Id);
                        statusMessage = deliveryMessage;
                    }
                    else if (string.IsNullOrEmpty(statusMessage))
                    {
                        statusMessage = deliveryMessage;
                    }
                    break;
            }
        }

        return advancedAny;
    }

    public static bool TryAdvanceHostileBoardingEntry(string stationId)
    {
        bool advancedAny = false;

        foreach ((MissionDefinition mission, MissionStatus status) in ActiveMissions())
        {
            if (status != MissionStatus.InProgress)
                continue;

            MissionObjective active = GetActiveObjective(mission.Id);
            if (active == null ||
                active.Kind != MissionObjectiveKind.ReachStation ||
                active.WaypointStationId != stationId)
            {
                continue;
            }

            advancedAny |= AdvanceObjective(mission.Id, active.Id);
        }

        return advancedAny;
    }

    public static bool TryAdvanceStationClearObjectives(string stationId)
    {
        bool advancedAny = false;

        foreach ((MissionDefinition mission, MissionStatus status) in ActiveMissions())
        {
            if (status != MissionStatus.InProgress)
                continue;

            MissionObjective active = GetActiveObjective(mission.Id);
            if (active == null ||
                active.Kind != MissionObjectiveKind.ClearStationHostiles ||
                active.WaypointStationId != stationId)
            {
                continue;
            }

            advancedAny |= AdvanceObjective(mission.Id, active.Id);
        }

        return advancedAny;
    }

    public static bool TryAdvanceEncounterObjective(string missionId, string expectedObjectiveId)
    {
        return AdvanceObjective(missionId, expectedObjectiveId);
    }

    private static MissionEntry GetOrCreate(string missionId)
    {
        if (!s_entries.TryGetValue(missionId, out MissionEntry entry))
        {
            entry = new MissionEntry();
            s_entries[missionId] = entry;
        }
        return entry;
    }

    private static bool CanOfferOrContinue(MissionDefinition mission, string stationId)
    {
        if (mission == null)
            return false;

        if (!string.IsNullOrWhiteSpace(stationId) && mission.GiverStationId != stationId)
            return false;

        if (mission.RequiredCompletedMissionIds != null)
        {
            foreach (string requiredMissionId in mission.RequiredCompletedMissionIds)
            {
                if (GetStatus(requiredMissionId) != MissionStatus.Completed)
                    return false;
            }
        }

        if (mission.RequiredStationTags != null && mission.RequiredStationTags.Count > 0)
        {
            var station = Stations.StationRegistry.Get(mission.GiverStationId);
            if (station == null)
                return false;

            foreach (string requiredTag in mission.RequiredStationTags)
            {
                bool found = false;
                foreach (string stationTag in station.MissionTags)
                {
                    if (stationTag == requiredTag)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                    return false;
            }
        }

        return true;
    }

    private static bool TryDeliverCargo(MissionObjective objective, out string statusMessage)
    {
        statusMessage = "";
        if (objective == null ||
            string.IsNullOrWhiteSpace(objective.RequiredCommodityId) ||
            objective.RequiredCommodityAmount <= 0f)
        {
            return false;
        }

        float cargoAmount = PlayerState.GetCargoAmount(objective.RequiredCommodityId);
        if (cargoAmount + 0.001f < objective.RequiredCommodityAmount)
        {
            string commodityName = ItemCatalog.DisplayName(objective.RequiredCommodityId);
            statusMessage = $"Mission cargo missing: need {objective.RequiredCommodityAmount:F0} {commodityName}.";
            return false;
        }

        PlayerState.TryRemoveCargo(objective.RequiredCommodityId, objective.RequiredCommodityAmount);
        string deliveredName = ItemCatalog.DisplayName(objective.RequiredCommodityId);
        statusMessage = $"Delivered {objective.RequiredCommodityAmount:F0} {deliveredName}. Mission updated.";
        return true;
    }
}
