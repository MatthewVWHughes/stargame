using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Game.Runtime;

namespace Starlight.Tests;

/// <summary>
/// Pure-C# tests for <see cref="SaveService"/>. Uses <see cref="InMemorySaveStore"/>
/// via <see cref="SaveService.Store"/> so the Godot runtime is not required.
/// </summary>
[TestSuite]
public class SaveServiceTests
{
    private static void UseFreshInMemoryStore()
    {
        SaveService.Store = new InMemorySaveStore();
        GameSession.StartNewGame(); // also resets PlayerState/EconomyService/MissionService
    }

    [TestCase]
    public void SaveExists_EmptyStore_ReturnsFalse()
    {
        UseFreshInMemoryStore();

        AssertThat(SaveService.SaveExists()).IsFalse();
    }

    [TestCase]
    public void SaveDockedState_PopulatesStore()
    {
        UseFreshInMemoryStore();

        SaveService.SaveDockedState("EarthHub");

        AssertThat(SaveService.SaveExists()).IsTrue();
    }

    [TestCase]
    public void TryLoadDockedStation_NoSave_ReturnsFalse()
    {
        UseFreshInMemoryStore();

        bool loaded = SaveService.TryLoadDockedStation(out string stationId);

        AssertThat(loaded).IsFalse();
        AssertThat(stationId).IsEqual("");
    }

    [TestCase]
    public void RoundTrip_RestoresCreditsAndCargo()
    {
        UseFreshInMemoryStore();
        PlayerState.TrySpendCredits(250f);          // 750 credits
        PlayerState.TryAddCargo("food", 15f);
        SaveService.SaveDockedState("EarthHub");

        // Mutate in-memory state so the load has something to restore over
        PlayerState.ResetForNewGame();
        EconomyService.ResetForNewGame();
        AssertThat(PlayerState.Credits).IsEqual(1000f);
        AssertThat(PlayerState.GetCargoAmount("food")).IsEqual(0f);

        bool loaded = SaveService.TryLoadDockedStation(out string stationId);

        AssertThat(loaded).IsTrue();
        AssertThat(stationId).IsEqual("EarthHub");
        AssertThat(PlayerState.Credits).IsEqual(750f);
        AssertThat(PlayerState.GetCargoAmount("food")).IsEqual(15f);
    }

    [TestCase]
    public void TryLoadDockedStation_CorruptedJson_ReturnsFalse()
    {
        UseFreshInMemoryStore();
        SaveService.Store.Write("not valid json {{{");

        bool loaded = SaveService.TryLoadDockedStation(out string _);

        AssertThat(loaded).IsFalse();
    }

    [TestCase]
    public void TryLoadDockedStation_UnknownStation_ReturnsFalse()
    {
        UseFreshInMemoryStore();
        SaveService.Store.Write("{\"version\":5,\"docked_station_id\":\"Nowhere\",\"credits\":100,\"cargo_capacity\":40,\"hull\":100,\"max_hull\":100,\"shields\":0,\"max_shields\":0,\"cargo\":{},\"station_stock\":{},\"player_equipped\":{},\"player_inventory\":[],\"ship_equipped\":{},\"mission_progress\":{}}");

        bool loaded = SaveService.TryLoadDockedStation(out string _);

        AssertThat(loaded).IsFalse();
    }

    [TestCase]
    public void TryLoadDockedStation_NegativeCredits_ReturnsFalse()
    {
        UseFreshInMemoryStore();
        SaveService.Store.Write("{\"version\":5,\"docked_station_id\":\"EarthHub\",\"credits\":-50,\"cargo_capacity\":40,\"hull\":100,\"max_hull\":100,\"shields\":0,\"max_shields\":0,\"cargo\":{},\"station_stock\":{},\"player_equipped\":{},\"player_inventory\":[],\"ship_equipped\":{},\"mission_progress\":{}}");

        bool loaded = SaveService.TryLoadDockedStation(out string _);

        AssertThat(loaded).IsFalse();
    }

    [TestCase]
    public void TryLoadDockedStation_OldVersion_ReturnsFalse()
    {
        UseFreshInMemoryStore();
        SaveService.Store.Write("{\"version\":4,\"docked_station_id\":\"EarthHub\",\"credits\":100,\"cargo_capacity\":40,\"hull\":100,\"max_hull\":100,\"shields\":0,\"max_shields\":0,\"cargo\":{},\"station_stock\":{},\"player_equipped\":{},\"player_inventory\":[],\"ship_equipped\":{},\"mission_progress\":{}}");

        bool loaded = SaveService.TryLoadDockedStation(out string _);

        AssertThat(loaded).IsFalse();
    }

    [TestCase]
    public void HasLoadableDockedSave_ValidSave_ReturnsTrue()
    {
        UseFreshInMemoryStore();
        SaveService.SaveDockedState("EarthHub");

        AssertThat(SaveService.HasLoadableDockedSave()).IsTrue();
    }

    [TestCase]
    public void HasLoadableDockedSave_EmptyStore_ReturnsFalse()
    {
        UseFreshInMemoryStore();

        AssertThat(SaveService.HasLoadableDockedSave()).IsFalse();
    }
}
