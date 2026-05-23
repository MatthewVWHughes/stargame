using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Game.Inventory;

namespace Starlight.Tests;

[TestSuite]
public class ItemCatalogTests
{
    [TestCase]
    public void Get_KnownId_ReturnsDef()
    {
        ItemDef def = ItemCatalog.Get("food");

        AssertThat(def).IsNotNull();
        AssertThat(def.Id).IsEqual("food");
        AssertThat(def.DisplayName).IsEqual("Food");
        AssertThat(def.Category).IsEqual(ItemCategory.Commodity);
    }

    [TestCase]
    public void Get_UnknownId_ReturnsNull()
    {
        ItemDef def = ItemCatalog.Get("nonexistent_item");

        AssertThat(def).IsNull();
    }

    [TestCase]
    public void Exists_KnownId_True()
    {
        AssertThat(ItemCatalog.Exists("pulse_laser_mk1")).IsTrue();
    }

    [TestCase]
    public void Exists_UnknownId_False()
    {
        AssertThat(ItemCatalog.Exists("nonexistent_item")).IsFalse();
    }

    [TestCase]
    public void DisplayName_KnownId_ReturnsFriendlyName()
    {
        AssertThat(ItemCatalog.DisplayName("synthetic_parts")).IsEqual("Synthetic Parts");
    }

    [TestCase]
    public void DisplayName_UnknownId_ReturnsIdAsFallback()
    {
        AssertThat(ItemCatalog.DisplayName("nonexistent_item")).IsEqual("nonexistent_item");
    }

    [TestCase]
    public void All_ContainsAllRegisteredIds()
    {
        var all = ItemCatalog.All;

        AssertThat(all.ContainsKey("food")).IsTrue();
        AssertThat(all.ContainsKey("pulse_laser_mk1")).IsTrue();
        AssertThat(all.ContainsKey("shield_basic")).IsTrue();
        AssertThat(all.ContainsKey("engine_basic")).IsTrue();
    }
}
