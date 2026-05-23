using System.Collections.Generic;

namespace Starlight.Game.Missions;

/// <summary>
/// Static registry of mission definitions available in VS1.
/// Keep this lean — mission records are plain data. Any behavior belongs in MissionService
/// or the systems that advance objectives.
/// </summary>
public static class MissionCatalog
{
    public const string SamKesslerNpcId = "sam_kessler";
    public const string MiraChenNpcId = "mira_chen";
    public const string TomasValeNpcId = "tomas_vale";
    public const string RheaVossNpcId = "rhea_voss";

    public const string ClearDerelictOutpostId = "clear_derelict_outpost";
    public const string MartianFeedstockContractId = "martian_feedstock_contract";
    public const string ReliefRunToCeresId = "relief_run_to_ceres";
    public const string SecureCeresApproachId = "secure_ceres_approach";
    public const string RecoverOutpostManifestId = "recover_outpost_manifest";

    public const string ObjectiveFlyToDerelict = "fly_to_derelict";
    public const string ObjectiveClearHostiles = "clear_hostiles";
    public const string ObjectiveReturnToGiver = "return_to_giver";
    public const string ObjectiveDeliverReliefCargo = "deliver_relief_cargo";
    public const string ObjectiveClearCeresRaiders = "clear_ceres_raiders";
    public const string ObjectiveBoardOutpost = "board_outpost";
    public const string ObjectiveSecureManifest = "secure_manifest";

    private static readonly Dictionary<string, MissionDefinition> s_missions = BuildMissions();

    public static IReadOnlyDictionary<string, MissionDefinition> All => s_missions;

    public static MissionDefinition Get(string id) =>
        s_missions.TryGetValue(id, out MissionDefinition mission) ? mission : null;

    private static Dictionary<string, MissionDefinition> BuildMissions()
    {
        var clearDerelict = new MissionDefinition(
            Id: ClearDerelictOutpostId,
            Title: "Clear the Derelict Outpost",
            Summary: "Raiders seized an old research outpost near Earth. Board, clear them out, and come back.",
            GiverNpcId: SamKesslerNpcId,
            GiverStationId: "EarthHub",
            IssuerFactionId: "uce_commonwealth",
            RegionName: "Earth orbital core",
            Objectives: new List<MissionObjective>
            {
                new(ObjectiveFlyToDerelict, "Fly to the Derelict Outpost and dock.", MissionObjectiveKind.ReachStation, WaypointStationId: "DerelictOutpost"),
                new(ObjectiveClearHostiles, "Clear all hostiles on board.", MissionObjectiveKind.ClearStationHostiles, WaypointStationId: "DerelictOutpost"),
                new(ObjectiveReturnToGiver, "Return to Earth Hub and report to Sam Kessler.", MissionObjectiveKind.ReturnToGiver, WaypointStationId: "EarthHub"),
            },
            Reward: new MissionReward(Credits: 500f),
            OfferDialog: "Sam: \"Raiders took over the old research outpost in Earth's L-zone. I need someone to clear them out. Pays five hundred. You in?\"",
            AcceptedDialog: "Sam: \"Good. Dock with the outpost, clear the hostiles, get back here.\"",
            InProgressDialog: "Sam: \"The outpost isn't going to clear itself. Dock, clear the hostiles, come back.\"",
            TurnInDialog: "Sam: \"Nice work. Here's your five hundred credits. I'll be in touch.\"");

        var martianFeedstock = new MissionDefinition(
            Id: MartianFeedstockContractId,
            Title: "Martian Feedstock Contract",
            Summary: "Mars Heavy Industry needs a rush iron shipment delivered to Earth Hub for a frame assembly line already in dock.",
            GiverNpcId: RheaVossNpcId,
            GiverStationId: "MarsColony",
            IssuerFactionId: "mars_heavy_industry",
            RegionName: "Martian industrial sphere",
            Objectives: new List<MissionObjective>
            {
                new("deliver_feedstock", "Deliver 16 units of iron to Earth Hub.", MissionObjectiveKind.DeliverCargo, WaypointStationId: "EarthHub", RequiredCommodityId: "iron", RequiredCommodityAmount: 16f),
                new(ObjectiveReturnToGiver, "Return to Mars Colony and report to Rhea Voss.", MissionObjectiveKind.ReturnToGiver, WaypointStationId: "MarsColony"),
            },
            Reward: new MissionReward(Credits: 700f),
            OfferDialog: "Rhea: \"One of our Earth-side frame lines is stalled waiting on belt iron. Move sixteen units to Earth Hub and I'll cut you in as a rush contractor.\"",
            AcceptedDialog: "Rhea: \"Get the iron to Earth Hub, then get back here for settlement.\"",
            InProgressDialog: "Rhea: \"The assembly line is still waiting. Earth Hub needs that iron delivery before this shift burns out.\"",
            TurnInDialog: "Rhea: \"Earth confirmed receipt. Contract closed and paid.\"",
            RequiredStationTags: new[] { "procurement", "industrial escort" });

        var reliefRun = new MissionDefinition(
            Id: ReliefRunToCeresId,
            Title: "Relief Run to Ceres",
            Summary: "Armstrong Station is short on water reserves. Pick up cargo and move a relief shipment to Ceres Port.",
            GiverNpcId: MiraChenNpcId,
            GiverStationId: "ArmstrongStation",
            IssuerFactionId: "lunar_transit_cooperative",
            RegionName: "Earth-Luna transfer corridor",
            Objectives: new List<MissionObjective>
            {
                new(ObjectiveDeliverReliefCargo, "Deliver 12 units of water to Ceres Port.", MissionObjectiveKind.DeliverCargo, WaypointStationId: "CeresPort", RequiredCommodityId: "water", RequiredCommodityAmount: 12f),
                new(ObjectiveReturnToGiver, "Return to Armstrong Station and report to Mira Chen.", MissionObjectiveKind.ReturnToGiver, WaypointStationId: "ArmstrongStation"),
            },
            Reward: new MissionReward(Credits: 650f),
            OfferDialog: "Mira: \"Ceres is burning through reserve water faster than the haulers can cycle. If you can move twelve units out there, I'll make it worth your while.\"",
            AcceptedDialog: "Mira: \"Load the water, get it to Ceres Port, then come back and debrief.\"",
            InProgressDialog: "Mira: \"The relief contract is still open. Ceres needs that water shipment delivered intact.\"",
            TurnInDialog: "Mira: \"Ceres confirmed receipt. Clean work. Here's your payment.\"");

        var ceresSweep = new MissionDefinition(
            Id: SecureCeresApproachId,
            Title: "Secure Ceres Approach",
            Summary: "Raiders are hunting inbound freighters near Ceres Port. Sweep the approach lane, then report back to Tomas Vale.",
            GiverNpcId: TomasValeNpcId,
            GiverStationId: "CeresPort",
            IssuerFactionId: "belt_extraction_cooperative",
            RegionName: "Main asteroid belt",
            Objectives: new List<MissionObjective>
            {
                new(
                    ObjectiveClearCeresRaiders,
                    "Clear the raider wing massing near Ceres Port.",
                    MissionObjectiveKind.ClearHostileWing,
                    WaypointStationId: "CeresPort",
                    Encounter: new MissionEncounterDefinition(
                        Id: "ceres_approach_sweep",
                        DisplayName: "Ceres Approach Sweep",
                        AnchorStationId: "CeresPort",
                        WingSize: 4,
                        ActivationRange: 2200f,
                        SpawnRadiusMin: 520f,
                        SpawnRadiusMax: 880f)),
                new(ObjectiveReturnToGiver, "Return to Ceres Port and report to Tomas Vale.", MissionObjectiveKind.ReturnToGiver, WaypointStationId: "CeresPort"),
            },
            Reward: new MissionReward(Credits: 750f),
            OfferDialog: "Tomas: \"Your relief run got here in one piece, which is more than I can say for the last two freighters. Clear the raider picket off our approach lane and I'll pay combat rates.\"",
            AcceptedDialog: "Tomas: \"Head back out, break the raider wing, then return to Ceres for payment.\"",
            InProgressDialog: "Tomas: \"Ceres traffic is still pinned. Clear that raider wing and then we can talk business.\"",
            TurnInDialog: "Tomas: \"Approach lane is open again. Here's your cut.\"",
            RequiredCompletedMissionIds: new[] { ReliefRunToCeresId },
            RequiredStationTags: new[] { "convoy defense", "belt security" });

        var recoverManifest = new MissionDefinition(
            Id: RecoverOutpostManifestId,
            Title: "Recover the Outpost Manifest",
            Summary: "A Ceres broker wants the derelict outpost's surviving cargo manifest. Board the station, clear resistance, and return with the data.",
            GiverNpcId: TomasValeNpcId,
            GiverStationId: "CeresPort",
            IssuerFactionId: "belt_extraction_cooperative",
            RegionName: "Main asteroid belt",
            Objectives: new List<MissionObjective>
            {
                new(ObjectiveBoardOutpost, "Dock with the Derelict Outpost and board the station.", MissionObjectiveKind.ReachStation, WaypointStationId: "DerelictOutpost"),
                new(ObjectiveSecureManifest, "Clear the hostiles and secure the surviving manifest data.", MissionObjectiveKind.ClearStationHostiles, WaypointStationId: "DerelictOutpost"),
                new(ObjectiveReturnToGiver, "Return to Ceres Port and hand the manifest to Tomas Vale.", MissionObjectiveKind.ReturnToGiver, WaypointStationId: "CeresPort"),
            },
            Reward: new MissionReward(Credits: 900f),
            OfferDialog: "Tomas: \"The raiders left the outpost's cargo records in limbo. I need that manifest before the insurers write the whole lane off. Board the place, clear it, and bring the data back.\"",
            AcceptedDialog: "Tomas: \"Get in, secure the manifest, and come straight back to Ceres.\"",
            InProgressDialog: "Tomas: \"No manifest, no payout. The outpost is still waiting on you.\"",
            TurnInDialog: "Tomas: \"That manifest just reopened half the lane's claims. Nicely done.\"",
            RequiredCompletedMissionIds: new[] { SecureCeresApproachId },
            RequiredStationTags: new[] { "salvage", "recovery", "belt security" });

        return new Dictionary<string, MissionDefinition>
        {
            [clearDerelict.Id] = clearDerelict,
            [martianFeedstock.Id] = martianFeedstock,
            [reliefRun.Id] = reliefRun,
            [ceresSweep.Id] = ceresSweep,
            [recoverManifest.Id] = recoverManifest,
        };
    }
}
