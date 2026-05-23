using Godot;
using System.Collections.Generic;
using Starlight.Game.Inventory;

namespace Starlight.Game.Runtime;

/// <summary>
/// Tiny player-state holder for VS1.
/// This will later move behind a broader runtime/save architecture.
/// </summary>
public static class PlayerState
{
    public const int PlayerInventorySlotCount = 20;

    public static float Credits { get; private set; } = 1000f;
    public static float CargoCapacity { get; private set; } = 40f;
    public static float Hull { get; private set; } = 100f;
    public static float MaxHull { get; private set; } = 100f;
    public static float Shields { get; private set; } = 100f;
    public static float MaxShields { get; private set; } = 100f;
    public static float WeaponEnergy { get; private set; } = 100f;
    public static float MaxWeaponEnergy { get; private set; } = 100f;
    public static float OnFootHealth { get; private set; } = 100f;
    public static float MaxOnFootHealth { get; private set; } = 100f;
    public static Dictionary<string, float> Cargo { get; private set; } = new();

    public static Dictionary<EquipmentSlotId, ItemStack> PlayerEquipped { get; private set; } = new();
    public static List<ItemStack> PlayerInventory { get; private set; } = new();
    public static Dictionary<EquipmentSlotId, ItemStack> ShipEquipped { get; private set; } = new();

    public static void ResetForNewGame()
    {
        Credits = 1000f;
        CargoCapacity = 40f;
        MaxHull = 100f;
        Hull = 100f;
        MaxShields = 100f;
        Shields = 100f;
        MaxWeaponEnergy = 100f;
        WeaponEnergy = 100f;
        MaxOnFootHealth = 100f;
        OnFootHealth = 100f;
        Cargo = new Dictionary<string, float>();

        PlayerEquipped = new Dictionary<EquipmentSlotId, ItemStack>();
        PlayerInventory = new List<ItemStack>();
        ShipEquipped = BuildDefaultShipEquipment();
    }

    public static void Restore(
        float credits,
        float cargoCapacity,
        float hull,
        float maxHull,
        float shields,
        float maxShields,
        Dictionary<string, float> cargo,
        Dictionary<EquipmentSlotId, ItemStack> playerEquipped,
        List<ItemStack> playerInventory,
        Dictionary<EquipmentSlotId, ItemStack> shipEquipped)
    {
        Credits = Mathf.Max(0f, credits);
        CargoCapacity = Mathf.Max(0f, cargoCapacity);
        MaxHull = Mathf.Max(1f, maxHull);
        Hull = Mathf.Clamp(hull, 0f, MaxHull);
        MaxShields = Mathf.Max(0f, maxShields);
        Shields = Mathf.Clamp(shields, 0f, MaxShields);
        MaxWeaponEnergy = 100f;
        WeaponEnergy = MaxWeaponEnergy;
        MaxOnFootHealth = 100f;
        OnFootHealth = MaxOnFootHealth;
        Cargo = new Dictionary<string, float>();
        if (cargo != null)
        {
            foreach ((string commodityId, float amount) in cargo)
            {
                if (string.IsNullOrWhiteSpace(commodityId) || amount <= 0.001f)
                    continue;

                Cargo[commodityId] = amount;
            }
        }

        PlayerEquipped = playerEquipped ?? new Dictionary<EquipmentSlotId, ItemStack>();
        PlayerInventory = playerInventory ?? new List<ItemStack>();
        if (PlayerInventory.Count > PlayerInventorySlotCount)
            PlayerInventory = PlayerInventory.GetRange(0, PlayerInventorySlotCount);

        ShipEquipped = BuildDefaultShipEquipment();
        if (shipEquipped != null)
        {
            foreach ((EquipmentSlotId slot, ItemStack stack) in shipEquipped)
                ShipEquipped[slot] = stack;
        }
    }

    public static float GetCargoAmount(string commodityId)
    {
        return Cargo.TryGetValue(commodityId, out float amount) ? amount : 0f;
    }

    public static float GetUsedCargo()
    {
        float total = 0f;
        foreach ((_, float amount) in Cargo)
            total += amount;
        return total;
    }

    public static bool CanFit(float amount)
    {
        return GetUsedCargo() + amount <= CargoCapacity + 0.001f;
    }

    public static void AddCredits(float amount) => Credits += amount;

    public static void RepairHull()
    {
        Hull = MaxHull;
        Shields = MaxShields;
        WeaponEnergy = MaxWeaponEnergy;
    }

    public static void ResetOnFootHealth()
    {
        OnFootHealth = MaxOnFootHealth;
    }

    public static void ApplyOnFootDamage(float amount)
    {
        if (amount <= 0f)
            return;

        OnFootHealth = Mathf.Max(0f, OnFootHealth - amount);
    }

    public static bool OnFootIsAlive => OnFootHealth > 0f;

    public static void ApplyHullDamage(float amount)
    {
        float remaining = amount;
        if (Shields > 0f)
        {
            float shieldLoss = Mathf.Min(Shields, remaining);
            Shields -= shieldLoss;
            remaining -= shieldLoss;
        }

        if (remaining > 0f)
            Hull = Mathf.Max(0f, Hull - remaining);
    }

    public static void RegenerateShields(float amount)
    {
        if (amount <= 0f || MaxShields <= 0f)
            return;

        Shields = Mathf.Min(MaxShields, Shields + amount);
    }

    public static void RegenerateWeaponEnergy(float amount)
    {
        if (amount <= 0f || MaxWeaponEnergy <= 0f)
            return;

        WeaponEnergy = Mathf.Min(MaxWeaponEnergy, WeaponEnergy + amount);
    }

    public static bool TryConsumeWeaponEnergy(float amount)
    {
        if (amount <= 0f)
            return true;
        if (WeaponEnergy + 0.001f < amount)
            return false;

        WeaponEnergy -= amount;
        return true;
    }

    public static bool TrySpendCredits(float amount)
    {
        if (Credits + 0.001f < amount)
            return false;

        Credits -= amount;
        return true;
    }

    public static bool TryAddCargo(string commodityId, float amount)
    {
        if (!CanFit(amount))
            return false;

        Cargo[commodityId] = GetCargoAmount(commodityId) + amount;
        return true;
    }

    public static bool TryRemoveCargo(string commodityId, float amount)
    {
        float current = GetCargoAmount(commodityId);
        if (current + 0.001f < amount)
            return false;

        float updated = current - amount;
        if (updated <= 0.001f)
            Cargo.Remove(commodityId);
        else
            Cargo[commodityId] = updated;

        return true;
    }

    public static ItemStack GetPlayerEquipped(EquipmentSlotId slot) =>
        PlayerEquipped.TryGetValue(slot, out ItemStack stack) ? stack : null;

    public static ItemStack GetShipEquipped(EquipmentSlotId slot) =>
        ShipEquipped.TryGetValue(slot, out ItemStack stack) ? stack : null;

    public static bool TryAddToPlayerInventory(string itemId, float amount)
    {
        ItemDef def = ItemCatalog.Get(itemId);
        if (def == null || amount <= 0f)
            return false;

        float remaining = amount;
        int freeSlotCount = PlayerInventorySlotCount - PlayerInventory.Count;

        if (def.IsStackable)
        {
            foreach (ItemStack stack in PlayerInventory)
            {
                if (stack.ItemId != itemId)
                    continue;

                float available = def.MaxStack - stack.Count;
                if (available <= 0.001f)
                    continue;

                remaining -= available;
                if (remaining <= 0.001f)
                {
                    remaining = 0f;
                    break;
                }
            }
        }

        int requiredNewSlots = def.IsStackable
            ? (int)Mathf.Ceil(remaining / def.MaxStack)
            : (int)Mathf.Ceil(remaining);

        if (requiredNewSlots > freeSlotCount)
            return false;

        remaining = amount;

        if (def.IsStackable)
        {
            foreach (ItemStack stack in PlayerInventory)
            {
                if (stack.ItemId != itemId)
                    continue;

                float available = def.MaxStack - stack.Count;
                if (available <= 0.001f)
                    continue;

                float toAdd = Mathf.Min(available, remaining);
                stack.Count += toAdd;
                remaining -= toAdd;
                if (remaining <= 0.001f)
                    return true;
            }
        }

        while (remaining > 0.001f)
        {
            float stackCount = def.IsStackable ? Mathf.Min(def.MaxStack, remaining) : 1f;
            PlayerInventory.Add(new ItemStack(itemId, stackCount));
            remaining -= stackCount;
        }

        return true;
    }

    private static Dictionary<EquipmentSlotId, ItemStack> BuildDefaultShipEquipment()
    {
        return new Dictionary<EquipmentSlotId, ItemStack>
        {
            [EquipmentSlotId.ShipWeaponHardpoint1] = new ItemStack("pulse_laser_mk1", 1f),
            [EquipmentSlotId.ShipShield] = new ItemStack("shield_basic", 1f),
            [EquipmentSlotId.ShipEngine] = new ItemStack("engine_basic", 1f),
            [EquipmentSlotId.ShipThrusters] = new ItemStack("thrusters_basic", 1f),
        };
    }
}
