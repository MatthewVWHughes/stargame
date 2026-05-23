using System.Collections.Generic;

namespace Starlight.Game.Inventory;

public static class ItemCatalog
{
    private static readonly Dictionary<string, ItemDef> _items = new()
    {
        ["food"] = new ItemDef { Id = "food", DisplayName = "Food", Category = ItemCategory.Commodity, MaxStack = 999f },
        ["water"] = new ItemDef { Id = "water", DisplayName = "Water", Category = ItemCategory.Commodity, MaxStack = 999f },
        ["iron"] = new ItemDef { Id = "iron", DisplayName = "Iron", Category = ItemCategory.Commodity, MaxStack = 999f },
        ["synthetic_parts"] = new ItemDef { Id = "synthetic_parts", DisplayName = "Synthetic Parts", Category = ItemCategory.Commodity, MaxStack = 999f },

        ["pistol_basic"] = new ItemDef { Id = "pistol_basic", DisplayName = "Basic Pistol", Category = ItemCategory.PlayerSidearm, MaxStack = 1f },
        ["rifle_basic"] = new ItemDef { Id = "rifle_basic", DisplayName = "Basic Rifle", Category = ItemCategory.PlayerWeapon, MaxStack = 1f },

        ["medkit"] = new ItemDef { Id = "medkit", DisplayName = "Medkit", Category = ItemCategory.Consumable, MaxStack = 10f },

        ["pulse_laser_mk1"] = new ItemDef { Id = "pulse_laser_mk1", DisplayName = "Pulse Laser Mk1", Category = ItemCategory.ShipWeapon, MaxStack = 1f },
        ["shield_basic"] = new ItemDef { Id = "shield_basic", DisplayName = "Basic Shield", Category = ItemCategory.ShipShield, MaxStack = 1f },
        ["engine_basic"] = new ItemDef { Id = "engine_basic", DisplayName = "Basic Engine", Category = ItemCategory.ShipEngine, MaxStack = 1f },
        ["thrusters_basic"] = new ItemDef { Id = "thrusters_basic", DisplayName = "Basic Thrusters", Category = ItemCategory.ShipThrusters, MaxStack = 1f },
        ["countermeasures_basic"] = new ItemDef { Id = "countermeasures_basic", DisplayName = "Basic Countermeasures", Category = ItemCategory.ShipCountermeasures, MaxStack = 1f },
    };

    public static ItemDef Get(string id) => _items.TryGetValue(id, out ItemDef def) ? def : null;

    public static string DisplayName(string id) => Get(id)?.DisplayName ?? id;

    public static bool Exists(string id) => _items.ContainsKey(id);

    public static IReadOnlyDictionary<string, ItemDef> All => _items;
}
