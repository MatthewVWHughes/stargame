using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Game.Runtime;

namespace Starlight.Tests;

[TestSuite]
public class PlayerStateTests
{
    [TestCase]
    public void ResetForNewGame_SetsDefaults()
    {
        PlayerState.ResetForNewGame();

        AssertThat(PlayerState.Credits).IsEqual(1000f);
        AssertThat(PlayerState.CargoCapacity).IsEqual(40f);
        AssertThat(PlayerState.Hull).IsEqual(100f);
        AssertThat(PlayerState.MaxHull).IsEqual(100f);
        AssertThat(PlayerState.Shields).IsEqual(100f);
        AssertThat(PlayerState.MaxShields).IsEqual(100f);
        AssertThat(PlayerState.Cargo.Count).IsEqual(0);
    }

    [TestCase]
    public void ApplyHullDamage_ShieldsAbsorbFirst()
    {
        PlayerState.ResetForNewGame();

        PlayerState.ApplyHullDamage(30f);

        AssertThat(PlayerState.Shields).IsEqual(70f);
        AssertThat(PlayerState.Hull).IsEqual(100f);
    }

    [TestCase]
    public void ApplyHullDamage_OverflowHitsHull()
    {
        PlayerState.ResetForNewGame();

        PlayerState.ApplyHullDamage(130f);

        AssertThat(PlayerState.Shields).IsEqual(0f);
        AssertThat(PlayerState.Hull).IsEqual(70f);
    }

    [TestCase]
    public void ApplyHullDamage_ClampsHullAtZero()
    {
        PlayerState.ResetForNewGame();

        PlayerState.ApplyHullDamage(999f);

        AssertThat(PlayerState.Shields).IsEqual(0f);
        AssertThat(PlayerState.Hull).IsEqual(0f);
    }

    [TestCase]
    public void TrySpendCredits_Succeeds_DeductsAmount()
    {
        PlayerState.ResetForNewGame();

        bool ok = PlayerState.TrySpendCredits(400f);

        AssertThat(ok).IsTrue();
        AssertThat(PlayerState.Credits).IsEqual(600f);
    }

    [TestCase]
    public void TrySpendCredits_Insufficient_Refuses()
    {
        PlayerState.ResetForNewGame();

        bool ok = PlayerState.TrySpendCredits(5000f);

        AssertThat(ok).IsFalse();
        AssertThat(PlayerState.Credits).IsEqual(1000f);
    }

    [TestCase]
    public void TryAddCargo_WithinCapacity_Succeeds()
    {
        PlayerState.ResetForNewGame();

        bool ok = PlayerState.TryAddCargo("food", 10f);

        AssertThat(ok).IsTrue();
        AssertThat(PlayerState.GetCargoAmount("food")).IsEqual(10f);
        AssertThat(PlayerState.GetUsedCargo()).IsEqual(10f);
    }

    [TestCase]
    public void TryAddCargo_ExceedsCapacity_Refuses()
    {
        PlayerState.ResetForNewGame();

        PlayerState.TryAddCargo("food", 35f);
        bool ok = PlayerState.TryAddCargo("water", 10f);

        AssertThat(ok).IsFalse();
        AssertThat(PlayerState.GetCargoAmount("water")).IsEqual(0f);
    }

    [TestCase]
    public void TryRemoveCargo_Succeeds_UpdatesAmount()
    {
        PlayerState.ResetForNewGame();
        PlayerState.TryAddCargo("food", 20f);

        bool ok = PlayerState.TryRemoveCargo("food", 15f);

        AssertThat(ok).IsTrue();
        AssertThat(PlayerState.GetCargoAmount("food")).IsEqual(5f);
    }

    [TestCase]
    public void TryRemoveCargo_MoreThanOwned_Refuses()
    {
        PlayerState.ResetForNewGame();
        PlayerState.TryAddCargo("food", 5f);

        bool ok = PlayerState.TryRemoveCargo("food", 10f);

        AssertThat(ok).IsFalse();
        AssertThat(PlayerState.GetCargoAmount("food")).IsEqual(5f);
    }
}
