using System.Collections.Generic;
using Godot;

namespace Starlight.Game.Stations;

public sealed record FactionDefinition(
    string FactionId,
    string DisplayName,
    string ShortName,
    string Description,
    Color AccentColor);

public enum StationInteriorKind
{
    /// <summary>Friendly civilian hub. Uses StationStub.tscn with services (trade/repair/launch).</summary>
    CivilianHub,

    /// <summary>Hostile boarding target. Uses HostileStation.tscn with on-foot combat.</summary>
    HostileBoarding,
}

public sealed record NpcSpawn(string NpcId, string DisplayName, Godot.Vector3 LocalPosition);

public sealed record StationDefinition(
    string StationId,
    string DisplayName,
    StationInteriorKind Kind,
    string InteriorScenePath,
    string ParentBodyName,
    string RegionName,
    string OwnerFactionId,
    string StationRole,
    string SecurityProfile,
    string MarketProfileId,
    IReadOnlyList<string> MissionTags,
    IReadOnlyList<NpcSpawn> QuestGivers);

/// <summary>
/// Single source of truth for foundation station/world data:
/// - which scene to load when the player docks
/// - which NPCs should be spawned into that scene
/// - what faction, role, region, and mission identity the station carries
///
/// Kept data-driven so future stations can be registered without touching docking / interior code.
/// </summary>
public static class StationRegistry
{
    public const string CivilianStationScene = "res://scenes/game/station/StationStub.tscn";
    public const string HostileStationScene = "res://scenes/game/station/HostileStation.tscn";

    private static readonly Dictionary<string, FactionDefinition> s_factions = BuildFactions();
    private static readonly Dictionary<string, StationDefinition> s_stations = BuildStations();

    public static FactionDefinition GetFaction(string factionId)
    {
        if (string.IsNullOrWhiteSpace(factionId))
            return null;
        return s_factions.TryGetValue(factionId, out FactionDefinition def) ? def : null;
    }

    public static StationDefinition Get(string stationId)
    {
        if (string.IsNullOrWhiteSpace(stationId))
            return null;
        return s_stations.TryGetValue(stationId, out StationDefinition def) ? def : null;
    }

    public static string ResolveInteriorScene(string stationId)
    {
        StationDefinition def = Get(stationId);
        return def != null ? def.InteriorScenePath : CivilianStationScene;
    }

    public static string DescribeStation(string stationId)
    {
        StationDefinition station = Get(stationId);
        if (station == null)
            return "Unknown station.";

        FactionDefinition faction = GetFaction(station.OwnerFactionId);
        string ownerText = faction != null ? faction.DisplayName : station.OwnerFactionId;
        string missions = station.MissionTags.Count > 0 ? string.Join(", ", station.MissionTags) : "general contracts";
        return
            $"{station.DisplayName}\n" +
            $"{station.StationRole} in {station.RegionName}, anchored to {station.ParentBodyName}.\n" +
            $"Owner: {ownerText}\n" +
            $"Security: {station.SecurityProfile}\n" +
            $"Mission focus: {missions}";
    }

    private static Dictionary<string, FactionDefinition> BuildFactions()
    {
        return new Dictionary<string, FactionDefinition>
        {
            ["uce_commonwealth"] = new(
                "uce_commonwealth",
                "United Commonwealth of Earth",
                "UCE",
                "Inner-system sovereign authority centered on Earth.",
                new Color(0.36f, 0.62f, 0.96f)),
            ["uce_navy"] = new(
                "uce_navy",
                "UCE Navy",
                "UCE Navy",
                "Fleet arm responsible for deep-system security, interdiction, and strategic response.",
                new Color(0.28f, 0.42f, 0.88f)),
            ["lunar_transit_cooperative"] = new(
                "lunar_transit_cooperative",
                "Lunar Transit Cooperative",
                "LTC",
                "Civilian transfer and support operator across Earth-Luna traffic.",
                new Color(0.78f, 0.84f, 0.92f)),
            ["mars_heavy_industry"] = new(
                "mars_heavy_industry",
                "Mars Heavy Industry",
                "MHI",
                "Major shipbuilding and heavy fabrication bloc operating across the Martian sphere.",
                new Color(0.86f, 0.42f, 0.24f)),
            ["belt_extraction_cooperative"] = new(
                "belt_extraction_cooperative",
                "Belt Extraction Cooperative",
                "BEC",
                "Mining and ore-routing consortium spanning Ceres and the inner belt.",
                new Color(0.74f, 0.68f, 0.5f)),
            ["independent_raiders"] = new(
                "independent_raiders",
                "Independent Raiders",
                "Raiders",
                "Decentralized hostile groups preying on weakly defended traffic and abandoned facilities.",
                new Color(0.9f, 0.24f, 0.24f)),
        };
    }

    private static Dictionary<string, StationDefinition> BuildStations()
    {
        var result = new Dictionary<string, StationDefinition>
        {
            ["EarthHub"] = new StationDefinition(
                StationId: "EarthHub",
                DisplayName: "Earth Hub",
                Kind: StationInteriorKind.CivilianHub,
                InteriorScenePath: CivilianStationScene,
                ParentBodyName: "Earth",
                RegionName: "Earth orbital core",
                OwnerFactionId: "uce_commonwealth",
                StationRole: "Administrative and logistics hub",
                SecurityProfile: "High security civilian port",
                MarketProfileId: "earth_exchange",
                MissionTags: new[] { "enforcement", "courier", "investigation" },
                QuestGivers: new List<NpcSpawn>
                {
                    new(
                        NpcId: "sam_kessler",
                        DisplayName: "Sam Kessler",
                        LocalPosition: new Godot.Vector3(0f, 0f, -3.5f)),
                }),
            ["ArmstrongStation"] = new StationDefinition(
                StationId: "ArmstrongStation",
                DisplayName: "Armstrong Station",
                Kind: StationInteriorKind.CivilianHub,
                InteriorScenePath: CivilianStationScene,
                ParentBodyName: "Luna",
                RegionName: "Earth-Luna transfer corridor",
                OwnerFactionId: "lunar_transit_cooperative",
                StationRole: "Transfer, refuel, and support station",
                SecurityProfile: "Moderate civilian traffic control",
                MarketProfileId: "lunar_transfer",
                MissionTags: new[] { "relief", "rush cargo", "transit support" },
                QuestGivers: new List<NpcSpawn>
                {
                    new(
                        NpcId: Starlight.Game.Missions.MissionCatalog.MiraChenNpcId,
                        DisplayName: "Mira Chen",
                        LocalPosition: new Godot.Vector3(2.6f, 0f, -3.2f)),
                }),
            ["MarsColony"] = new StationDefinition(
                StationId: "MarsColony",
                DisplayName: "Mars Colony",
                Kind: StationInteriorKind.CivilianHub,
                InteriorScenePath: CivilianStationScene,
                ParentBodyName: "Mars",
                RegionName: "Martian industrial sphere",
                OwnerFactionId: "mars_heavy_industry",
                StationRole: "Shipyard and industrial colony port",
                SecurityProfile: "Corporate-secured industrial zone",
                MarketProfileId: "martian_industry",
                MissionTags: new[] { "procurement", "prototype recovery", "industrial escort" },
                QuestGivers: new List<NpcSpawn>
                {
                    new(
                        NpcId: Starlight.Game.Missions.MissionCatalog.RheaVossNpcId,
                        DisplayName: "Rhea Voss",
                        LocalPosition: new Vector3(-2.8f, 0f, -2.8f)),
                }),
            ["CeresPort"] = new StationDefinition(
                StationId: "CeresPort",
                DisplayName: "Ceres Port",
                Kind: StationInteriorKind.CivilianHub,
                InteriorScenePath: CivilianStationScene,
                ParentBodyName: "Ceres",
                RegionName: "Main asteroid belt",
                OwnerFactionId: "belt_extraction_cooperative",
                StationRole: "Ore port and frontier brokerage station",
                SecurityProfile: "Low-to-moderate frontier security",
                MarketProfileId: "belt_frontier",
                MissionTags: new[] { "convoy defense", "salvage", "recovery", "belt security" },
                QuestGivers: new List<NpcSpawn>
                {
                    new(
                        NpcId: Starlight.Game.Missions.MissionCatalog.TomasValeNpcId,
                        DisplayName: "Tomas Vale",
                        LocalPosition: new Vector3(-2.8f, 0f, -2.8f)),
                }),
            ["DerelictOutpost"] = new StationDefinition(
                StationId: "DerelictOutpost",
                DisplayName: "Derelict Outpost",
                Kind: StationInteriorKind.HostileBoarding,
                InteriorScenePath: HostileStationScene,
                ParentBodyName: "Earth",
                RegionName: "Earth L-zone fringe",
                OwnerFactionId: "independent_raiders",
                StationRole: "Captured research and cargo platform",
                SecurityProfile: "Hostile contested space",
                MarketProfileId: "none",
                MissionTags: new[] { "boarding", "clearance", "manifest recovery" },
                QuestGivers: System.Array.Empty<NpcSpawn>()),
        };

        return result;
    }
}
