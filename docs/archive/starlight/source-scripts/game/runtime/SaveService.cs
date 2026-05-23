using Godot;
using System.Collections.Generic;
using System.Text.Json;
using Starlight.Game.Inventory;
using Starlight.Game.Missions;

namespace Starlight.Game.Runtime;

/// <summary>
/// Minimal VS1 save surface for boot flow and dock resume.
/// Old saves are rejected outright during VS1 — no migration until 1.0.
/// </summary>
public static class SaveService
{
    private const int SaveVersion = 5;

    /// <summary>
    /// Backing store for save data. Defaults to Godot's user:// file store.
    /// Tests swap in <see cref="InMemorySaveStore"/> to avoid needing the
    /// Godot runtime.
    /// </summary>
    public static ISaveStore Store { get; set; } = new GodotFileSaveStore();

    private sealed class SerializedStack
    {
        public string item_id { get; set; } = "";
        public float count { get; set; }
    }

    private sealed class SaveData
    {
        public int version { get; set; }
        public string docked_station_id { get; set; } = "";
        public float credits { get; set; }
        public float cargo_capacity { get; set; }
        public float hull { get; set; }
        public float max_hull { get; set; }
        public float shields { get; set; }
        public float max_shields { get; set; }
        public Dictionary<string, float> cargo { get; set; } = new();
        public Dictionary<string, Dictionary<string, float>> station_stock { get; set; } = new();
        public Dictionary<string, SerializedStack> player_equipped { get; set; } = new();
        public List<SerializedStack> player_inventory { get; set; } = new();
        public Dictionary<string, SerializedStack> ship_equipped { get; set; } = new();
        public Dictionary<string, SerializedMissionProgress> mission_progress { get; set; } = new();
    }

    private sealed class SerializedMissionProgress
    {
        public string status { get; set; } = "";
        public int objective_index { get; set; }
    }

    public static bool SaveExists() => Store.Exists();

    public static bool HasLoadableDockedSave()
    {
        return TryReadSaveData(out _);
    }

    public static void SaveDockedState(string stationId)
    {
        var data = new SaveData
        {
            version = SaveVersion,
            docked_station_id = stationId,
            credits = PlayerState.Credits,
            cargo_capacity = PlayerState.CargoCapacity,
            hull = PlayerState.Hull,
            max_hull = PlayerState.MaxHull,
            shields = PlayerState.Shields,
            max_shields = PlayerState.MaxShields,
            cargo = new Dictionary<string, float>(PlayerState.Cargo),
            station_stock = CloneNestedStock(EconomyService.StationStock),
            player_equipped = SerializeSlotMap(PlayerState.PlayerEquipped),
            player_inventory = SerializeStackList(PlayerState.PlayerInventory),
            ship_equipped = SerializeSlotMap(PlayerState.ShipEquipped),
            mission_progress = SerializeMissionProgress(MissionService.CaptureProgress()),
        };

        Store.Write(JsonSerializer.Serialize(data));
    }

    public static bool TryLoadDockedStation(out string stationId)
    {
        stationId = "";

        if (!TryReadSaveData(out SaveData data))
            return false;

        stationId = data.docked_station_id;
        PlayerState.Restore(
            data.credits,
            data.cargo_capacity,
            data.hull,
            data.max_hull,
            data.shields,
            data.max_shields,
            data.cargo,
            DeserializeSlotMap(data.player_equipped),
            DeserializeStackList(data.player_inventory),
            DeserializeSlotMap(data.ship_equipped));
        EconomyService.Restore(data.station_stock);
        MissionService.RestoreProgress(DeserializeMissionProgress(data.mission_progress));
        return true;
    }

    private static bool TryReadSaveData(out SaveData data)
    {
        data = null;

        if (!Store.Exists())
            return false;

        string raw;
        try
        {
            raw = Store.Read();
        }
        catch
        {
            return false;
        }

        try
        {
            data = JsonSerializer.Deserialize<SaveData>(raw);
        }
        catch
        {
            return false;
        }

        if (data == null || data.version != SaveVersion)
            return false;

        if (!EconomyService.IsKnownStation(data.docked_station_id))
            return false;

        if (data.credits < 0f || data.cargo_capacity < 0f || data.max_hull <= 0f || data.hull < 0f)
            return false;

        if (data.max_shields < 0f || data.shields < 0f)
            return false;

        return true;
    }

    private static Dictionary<string, Dictionary<string, float>> CloneNestedStock(
        Dictionary<string, Dictionary<string, float>> source)
    {
        var clone = new Dictionary<string, Dictionary<string, float>>();
        if (source == null)
            return clone;

        foreach ((string stationId, Dictionary<string, float> stock) in source)
            clone[stationId] = stock == null ? new Dictionary<string, float>() : new Dictionary<string, float>(stock);

        return clone;
    }

    private static Dictionary<string, SerializedStack> SerializeSlotMap(Dictionary<EquipmentSlotId, ItemStack> map)
    {
        var clone = new Dictionary<string, SerializedStack>();
        if (map == null)
            return clone;

        foreach ((EquipmentSlotId slot, ItemStack stack) in map)
        {
            if (stack == null || stack.IsEmpty)
                continue;
            clone[slot.ToString()] = new SerializedStack { item_id = stack.ItemId, count = stack.Count };
        }
        return clone;
    }

    private static List<SerializedStack> SerializeStackList(List<ItemStack> list)
    {
        var clone = new List<SerializedStack>();
        if (list == null)
            return clone;

        foreach (ItemStack stack in list)
        {
            if (stack == null || stack.IsEmpty)
                continue;
            clone.Add(new SerializedStack { item_id = stack.ItemId, count = stack.Count });
        }
        return clone;
    }

    private static Dictionary<EquipmentSlotId, ItemStack> DeserializeSlotMap(Dictionary<string, SerializedStack> map)
    {
        var result = new Dictionary<EquipmentSlotId, ItemStack>();
        if (map == null)
            return result;

        foreach ((string key, SerializedStack serialized) in map)
        {
            if (serialized == null || string.IsNullOrEmpty(serialized.item_id) || serialized.count <= 0f)
                continue;
            if (!System.Enum.TryParse(key, out EquipmentSlotId slot))
                continue;
            ItemDef def = ItemCatalog.Get(serialized.item_id);
            if (def == null || !def.FitsEquipmentSlot(slot))
                continue;
            float count = def.IsStackable ? Mathf.Min(def.MaxStack, serialized.count) : 1f;
            result[slot] = new ItemStack(serialized.item_id, count);
        }
        return result;
    }

    private static List<ItemStack> DeserializeStackList(List<SerializedStack> list)
    {
        var result = new List<ItemStack>();
        if (list == null)
            return result;

        foreach (SerializedStack serialized in list)
        {
            if (serialized == null || string.IsNullOrEmpty(serialized.item_id) || serialized.count <= 0f)
                continue;
            ItemDef def = ItemCatalog.Get(serialized.item_id);
            if (def == null)
                continue;

            float remaining = serialized.count;
            while (remaining > 0.001f)
            {
                float stackCount = def.IsStackable ? Mathf.Min(def.MaxStack, remaining) : 1f;
                result.Add(new ItemStack(serialized.item_id, stackCount));
                remaining -= stackCount;
            }

            if (result.Count >= PlayerState.PlayerInventorySlotCount)
                break;
        }

        if (result.Count > PlayerState.PlayerInventorySlotCount)
            result = result.GetRange(0, PlayerState.PlayerInventorySlotCount);

        return result;
    }

    private static Dictionary<string, SerializedMissionProgress> SerializeMissionProgress(
        Dictionary<string, MissionService.MissionProgressSnapshot> progress)
    {
        var result = new Dictionary<string, SerializedMissionProgress>();
        if (progress == null)
            return result;

        foreach ((string missionId, MissionService.MissionProgressSnapshot snapshot) in progress)
        {
            result[missionId] = new SerializedMissionProgress
            {
                status = snapshot.Status.ToString(),
                objective_index = snapshot.ObjectiveIndex,
            };
        }

        return result;
    }

    private static Dictionary<string, MissionService.MissionProgressSnapshot> DeserializeMissionProgress(
        Dictionary<string, SerializedMissionProgress> progress)
    {
        var result = new Dictionary<string, MissionService.MissionProgressSnapshot>();
        if (progress == null)
            return result;

        foreach ((string missionId, SerializedMissionProgress serialized) in progress)
        {
            if (serialized == null || !System.Enum.TryParse(serialized.status, out MissionStatus status))
                continue;

            result[missionId] = new MissionService.MissionProgressSnapshot(status, serialized.objective_index);
        }

        return result;
    }
}
