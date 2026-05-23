using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Game.Inventory;

namespace Starlight.Tests;

[TestSuite]
public class ItemDefTests
{
    [TestCase]
    public void IsStackable_MaxStackAboveOne_True()
    {
        var def = new ItemDef { Id = "food", MaxStack = 999f };

        AssertThat(def.IsStackable).IsTrue();
    }

    [TestCase]
    public void IsStackable_MaxStackOne_False()
    {
        var def = new ItemDef { Id = "pistol_basic", MaxStack = 1f };

        AssertThat(def.IsStackable).IsFalse();
    }

    [TestCase]
    public void FitsEquipmentSlot_ShipWeaponToHardpoint1_True()
    {
        ItemDef def = ItemCatalog.Get("pulse_laser_mk1");

        AssertThat(def.FitsEquipmentSlot(EquipmentSlotId.ShipWeaponHardpoint1)).IsTrue();
    }

    [TestCase]
    public void FitsEquipmentSlot_ShipWeaponToHardpoint2_True()
    {
        ItemDef def = ItemCatalog.Get("pulse_laser_mk1");

        AssertThat(def.FitsEquipmentSlot(EquipmentSlotId.ShipWeaponHardpoint2)).IsTrue();
    }

    [TestCase]
    public void FitsEquipmentSlot_ShipEngineToEngineSlot_True()
    {
        ItemDef def = ItemCatalog.Get("engine_basic");

        AssertThat(def.FitsEquipmentSlot(EquipmentSlotId.ShipEngine)).IsTrue();
    }

    [TestCase]
    public void FitsEquipmentSlot_ShipWeaponToEngineSlot_False()
    {
        ItemDef def = ItemCatalog.Get("pulse_laser_mk1");

        AssertThat(def.FitsEquipmentSlot(EquipmentSlotId.ShipEngine)).IsFalse();
    }

    [TestCase]
    public void FitsEquipmentSlot_PlayerWeaponToPlayerPrimary_True()
    {
        ItemDef def = ItemCatalog.Get("rifle_basic");

        AssertThat(def.FitsEquipmentSlot(EquipmentSlotId.PlayerPrimaryWeapon)).IsTrue();
    }

    [TestCase]
    public void FitsEquipmentSlot_CommodityToAnyEquipmentSlot_False()
    {
        ItemDef food = ItemCatalog.Get("food");

        AssertThat(food.FitsEquipmentSlot(EquipmentSlotId.ShipEngine)).IsFalse();
        AssertThat(food.FitsEquipmentSlot(EquipmentSlotId.PlayerPrimaryWeapon)).IsFalse();
    }
}
