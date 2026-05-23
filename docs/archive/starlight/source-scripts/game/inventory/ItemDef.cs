namespace Starlight.Game.Inventory;

public enum ItemCategory
{
    Commodity,
    PlayerWeapon,
    PlayerSidearm,
    ShipWeapon,
    ShipShield,
    ShipEngine,
    ShipThrusters,
    ShipCountermeasures,
    Consumable,
    Misc,
}

public sealed class ItemDef
{
    public string Id { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public ItemCategory Category { get; init; } = ItemCategory.Misc;
    public float MaxStack { get; init; } = 1f;

    public bool IsStackable => MaxStack > 1f;

    public bool FitsEquipmentSlot(EquipmentSlotId slot) => (slot, Category) switch
    {
        (EquipmentSlotId.PlayerPrimaryWeapon, ItemCategory.PlayerWeapon) => true,
        (EquipmentSlotId.PlayerSecondarySidearm, ItemCategory.PlayerSidearm) => true,
        (EquipmentSlotId.ShipWeaponHardpoint1, ItemCategory.ShipWeapon) => true,
        (EquipmentSlotId.ShipWeaponHardpoint2, ItemCategory.ShipWeapon) => true,
        (EquipmentSlotId.ShipShield, ItemCategory.ShipShield) => true,
        (EquipmentSlotId.ShipEngine, ItemCategory.ShipEngine) => true,
        (EquipmentSlotId.ShipThrusters, ItemCategory.ShipThrusters) => true,
        (EquipmentSlotId.ShipCountermeasures, ItemCategory.ShipCountermeasures) => true,
        _ => false,
    };
}
