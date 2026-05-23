using Godot;
using System.Collections.Generic;
using System.Text;
using Starlight.Game.Stations;

namespace Starlight.Game.Runtime;

/// <summary>
/// Minimal VS1 economy surface for the first trade loop.
/// </summary>
public static class EconomyService
{
    public sealed record CommodityMarketInfo(string CommodityId, string DisplayName, string ExportHint, string ImportHint);
    public sealed record StationMarketProfile(string StationId, string Summary, IReadOnlyList<string> Exports, IReadOnlyList<string> Imports);

    private static readonly Dictionary<string, float> BasePrices = new()
    {
        ["food"] = 10f,
        ["water"] = 6f,
        ["iron"] = 14f,
        ["synthetic_parts"] = 40f,
    };

    private static readonly Dictionary<string, CommodityMarketInfo> CommodityInfo = new()
    {
        ["food"] = new("food", "Food", "agri surplus", "rations"),
        ["water"] = new("water", "Water", "processed water", "reserve water"),
        ["iron"] = new("iron", "Iron", "ore and feedstock", "ore"),
        ["synthetic_parts"] = new("synthetic_parts", "Synthetic Parts", "fabrication parts", "replacement parts"),
    };

    private static readonly Dictionary<string, StationMarketProfile> StationProfiles = new()
    {
        ["EarthHub"] = new("EarthHub", "High-volume inner-system exchange with broad consumer stock and steady fabrication demand.",
            new[] { "food", "synthetic_parts" }, new[] { "iron" }),
        ["ArmstrongStation"] = new("ArmstrongStation", "Lunar transfer post with tight life-support reserves and modest industrial throughput.",
            new[] { "synthetic_parts" }, new[] { "water", "food" }),
        ["MarsColony"] = new("MarsColony", "Growing industrial colony that leans on imported parts while moving processed materials.",
            new[] { "iron" }, new[] { "synthetic_parts", "food" }),
        ["CeresPort"] = new("CeresPort", "Frontier ore port with deep raw stock and fragile support logistics.",
            new[] { "iron" }, new[] { "water", "food", "synthetic_parts" }),
        ["DerelictOutpost"] = new("DerelictOutpost", "No active civilian market.", System.Array.Empty<string>(), System.Array.Empty<string>()),
    };

    private static readonly Dictionary<string, Dictionary<string, float>> MaxCapacity = new()
    {
        ["EarthHub"] = new() { ["food"] = 220f, ["water"] = 180f, ["iron"] = 100f, ["synthetic_parts"] = 80f },
        ["ArmstrongStation"] = new() { ["food"] = 120f, ["water"] = 140f, ["iron"] = 80f, ["synthetic_parts"] = 60f },
        ["MarsColony"] = new() { ["food"] = 220f, ["water"] = 180f, ["iron"] = 120f, ["synthetic_parts"] = 60f },
        ["CeresPort"] = new() { ["food"] = 100f, ["water"] = 80f, ["iron"] = 260f, ["synthetic_parts"] = 40f },
        ["DerelictOutpost"] = new(),
    };

    public static Dictionary<string, Dictionary<string, float>> StationStock { get; private set; } = new();
    public static IEnumerable<string> CommodityIds => CommodityInfo.Keys;

    public static void ResetForNewGame()
    {
        StationStock = BuildDefaultStock();
    }

    public static void Restore(Dictionary<string, Dictionary<string, float>> stock)
    {
        StationStock = BuildDefaultStock();
        if (stock == null)
            return;

        foreach ((string stationId, Dictionary<string, float> savedStock) in stock)
        {
            if (!StationStock.ContainsKey(stationId) || savedStock == null)
                continue;

            foreach ((string commodityId, float amount) in savedStock)
            {
                if (!MaxCapacity.TryGetValue(stationId, out Dictionary<string, float> stationCaps) ||
                    !stationCaps.TryGetValue(commodityId, out float maxCapacity))
                {
                    continue;
                }

                StationStock[stationId][commodityId] = Mathf.Clamp(amount, 0f, maxCapacity);
            }
        }
    }

    public static bool IsKnownStation(string stationId) =>
        !string.IsNullOrWhiteSpace(stationId) && MaxCapacity.ContainsKey(stationId);

    public static IReadOnlyDictionary<string, float> GetStock(string stationId)
    {
        return StationStock.TryGetValue(stationId, out Dictionary<string, float> stock)
            ? stock
            : new Dictionary<string, float>();
    }

    public static string GetCommodityDisplayName(string commodityId)
    {
        return CommodityInfo.TryGetValue(commodityId, out CommodityMarketInfo info)
            ? info.DisplayName
            : Capitalize(commodityId);
    }

    public static StationMarketProfile GetStationProfile(string stationId)
    {
        return StationProfiles.TryGetValue(stationId, out StationMarketProfile profile)
            ? profile
            : new StationMarketProfile(stationId, "General market.", System.Array.Empty<string>(), System.Array.Empty<string>());
    }

    public static float GetPrice(string stationId, string commodityId)
    {
        float basePrice = BasePrices[commodityId];
        float stock = GetStockAmount(stationId, commodityId);
        float max = MaxCapacity[stationId][commodityId];
        float fillRatio = max > 0f ? stock / max : 0f;
        return basePrice * (2.0f - fillRatio * 1.5f);
    }

    public static bool TryBuy(string stationId, string commodityId, float amount, out string message)
    {
        float stock = GetStockAmount(stationId, commodityId);
        if (stock + 0.001f < amount)
        {
            message = $"{stationId} is out of {commodityId}.";
            return false;
        }

        float totalPrice = GetPrice(stationId, commodityId) * amount;
        if (!PlayerState.TrySpendCredits(totalPrice))
        {
            message = "Not enough credits.";
            return false;
        }

        if (!PlayerState.TryAddCargo(commodityId, amount))
        {
            PlayerState.AddCredits(totalPrice);
            message = "Not enough cargo space.";
            return false;
        }

        StationStock[stationId][commodityId] = stock - amount;
        message = $"Bought {amount:F0} {commodityId} for {totalPrice:F0} credits.";
        return true;
    }

    public static bool TrySell(string stationId, string commodityId, float amount, out string message)
    {
        if (!PlayerState.TryRemoveCargo(commodityId, amount))
        {
            message = $"No {commodityId} in cargo.";
            return false;
        }

        float totalPrice = GetPrice(stationId, commodityId) * amount;
        PlayerState.AddCredits(totalPrice);
        StationStock[stationId][commodityId] = GetStockAmount(stationId, commodityId) + amount;
        message = $"Sold {amount:F0} {commodityId} for {totalPrice:F0} credits.";
        return true;
    }

    public static string BuildTradeSummary(string stationId)
    {
        StationMarketProfile profile = GetStationProfile(stationId);
        StationDefinition station = StationRegistry.Get(stationId);
        FactionDefinition faction = station != null ? StationRegistry.GetFaction(station.OwnerFactionId) : null;
        var sb = new StringBuilder();
        sb.AppendLine($"{ResolveStationName(stationId)} market");
        if (station != null)
            sb.AppendLine($"{station.StationRole} | {station.RegionName}");
        if (faction != null)
            sb.AppendLine($"Owner: {faction.DisplayName}");
        if (!string.IsNullOrWhiteSpace(profile.Summary))
            sb.AppendLine(profile.Summary);
        if (profile.Exports.Count > 0)
            sb.AppendLine($"Exports: {FormatCommodityList(profile.Exports)}");
        if (profile.Imports.Count > 0)
            sb.AppendLine($"Imports: {FormatCommodityList(profile.Imports)}");
        sb.AppendLine("");
        foreach ((string commodityId, float stock) in StationStock[stationId])
        {
            float price = GetPrice(stationId, commodityId);
            sb.AppendLine($"{GetCommodityDisplayName(commodityId)}  stock {stock:F0}  price {price:F0} cr  {DescribeLocalMarket(stationId, commodityId)}");
        }
        return sb.ToString().TrimEnd();
    }

    public static string BuildMarketOverview(string stationId)
    {
        StationMarketProfile profile = GetStationProfile(stationId);
        StationDefinition station = StationRegistry.Get(stationId);
        FactionDefinition faction = station != null ? StationRegistry.GetFaction(station.OwnerFactionId) : null;
        string exports = profile.Exports.Count > 0 ? FormatCommodityList(profile.Exports) : "none";
        string imports = profile.Imports.Count > 0 ? FormatCommodityList(profile.Imports) : "none";
        var sb = new StringBuilder();
        if (station != null)
        {
            sb.AppendLine($"{station.StationRole} on {station.ParentBodyName}.");
            sb.AppendLine($"Region: {station.RegionName}");
            sb.AppendLine($"Security: {station.SecurityProfile}");
        }
        if (faction != null)
            sb.AppendLine($"Owner: {faction.DisplayName}");
        sb.AppendLine(profile.Summary);
        sb.AppendLine($"Exports: {exports}");
        sb.AppendLine($"Imports: {imports}");
        sb.Append("Tip: low stock means higher buy prices, high stock means better local buying.");
        return sb.ToString();
    }

    public static string BuildCargoManifestSummary()
    {
        if (PlayerState.Cargo.Count == 0)
            return "Cargo: empty";

        var sb = new StringBuilder();
        sb.Append("Cargo: ");
        bool first = true;
        foreach ((string commodityId, float amount) in PlayerState.Cargo)
        {
            if (!first)
                sb.Append("  |  ");
            sb.Append($"{GetCommodityDisplayName(commodityId)} {amount:F0}");
            first = false;
        }
        return sb.ToString();
    }

    private static float GetStockAmount(string stationId, string commodityId)
    {
        if (!StationStock.TryGetValue(stationId, out Dictionary<string, float> station))
            return 0f;

        return station.TryGetValue(commodityId, out float amount) ? amount : 0f;
    }

    private static string Capitalize(string value)
    {
        if (string.IsNullOrWhiteSpace(value))
            return value;

        return char.ToUpperInvariant(value[0]) + value[1..];
    }

    private static string DescribeLocalMarket(string stationId, string commodityId)
    {
        if (!MaxCapacity.TryGetValue(stationId, out Dictionary<string, float> caps) ||
            !caps.TryGetValue(commodityId, out float maxCapacity) ||
            maxCapacity <= 0f)
        {
            return "";
        }

        float stock = GetStockAmount(stationId, commodityId);
        float fillRatio = stock / maxCapacity;
        if (fillRatio <= 0.18f)
            return "scarce";
        if (fillRatio <= 0.42f)
            return "tight";
        if (fillRatio >= 0.82f)
            return "glut";
        if (fillRatio >= 0.62f)
            return "surplus";
        return "steady";
    }

    private static string FormatCommodityList(IReadOnlyList<string> commodityIds)
    {
        if (commodityIds == null || commodityIds.Count == 0)
            return "none";

        var names = new List<string>(commodityIds.Count);
        foreach (string commodityId in commodityIds)
            names.Add(GetCommodityDisplayName(commodityId));
        return string.Join(", ", names);
    }

    private static string ResolveStationName(string stationId)
    {
        return StationRegistry.Get(stationId)?.DisplayName ?? stationId;
    }

    private static Dictionary<string, Dictionary<string, float>> BuildDefaultStock()
    {
        return new Dictionary<string, Dictionary<string, float>>
        {
            ["EarthHub"] = new() { ["food"] = 200f, ["water"] = 140f, ["iron"] = 25f, ["synthetic_parts"] = 70f },
            ["ArmstrongStation"] = new() { ["food"] = 30f, ["water"] = 40f, ["iron"] = 55f, ["synthetic_parts"] = 50f },
            ["MarsColony"] = new() { ["food"] = 55f, ["water"] = 120f, ["iron"] = 90f, ["synthetic_parts"] = 20f },
            ["CeresPort"] = new() { ["food"] = 20f, ["water"] = 20f, ["iron"] = 230f, ["synthetic_parts"] = 10f },
        };
    }
}
