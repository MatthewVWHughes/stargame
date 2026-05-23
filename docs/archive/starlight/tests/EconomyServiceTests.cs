using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Game.Runtime;

namespace Starlight.Tests;

[TestSuite]
public class EconomyServiceTests
{
    [TestCase]
    public void ResetForNewGame_PopulatesDefaultStock()
    {
        EconomyService.ResetForNewGame();

        var earthHub = EconomyService.GetStock("EarthHub");
        AssertThat(earthHub.Count).IsGreater(0);
        AssertThat(earthHub["food"]).IsEqual(200f);
        AssertThat(earthHub["water"]).IsEqual(140f);
    }

    [TestCase]
    public void IsKnownStation_ValidId_True()
    {
        AssertThat(EconomyService.IsKnownStation("EarthHub")).IsTrue();
    }

    [TestCase]
    public void IsKnownStation_UnknownId_False()
    {
        AssertThat(EconomyService.IsKnownStation("NowhereStation")).IsFalse();
        AssertThat(EconomyService.IsKnownStation("")).IsFalse();
    }

    [TestCase]
    public void GetPrice_EmptyStock_IsTwiceBase()
    {
        EconomyService.ResetForNewGame();
        EconomyService.StationStock["EarthHub"]["food"] = 0f;

        float price = EconomyService.GetPrice("EarthHub", "food");

        AssertThat(price).IsEqual(20f); // basePrice=10 * 2.0
    }

    [TestCase]
    public void GetPrice_FullStock_IsHalfBase()
    {
        EconomyService.ResetForNewGame();
        EconomyService.StationStock["EarthHub"]["food"] = 220f; // max capacity

        float price = EconomyService.GetPrice("EarthHub", "food");

        AssertThat(price).IsEqual(5f); // basePrice=10 * 0.5
    }

    [TestCase]
    public void GetPrice_ScarcityRaisesRelativeToGlut()
    {
        EconomyService.ResetForNewGame();
        EconomyService.StationStock["EarthHub"]["food"] = 22f; // 10% fill
        float scarcePrice = EconomyService.GetPrice("EarthHub", "food");

        EconomyService.StationStock["EarthHub"]["food"] = 198f; // 90% fill
        float glutPrice = EconomyService.GetPrice("EarthHub", "food");

        AssertThat(scarcePrice).IsGreater(glutPrice);
    }

    [TestCase]
    public void TryBuy_SuccessfulTransaction_UpdatesPlayerAndStock()
    {
        PlayerState.ResetForNewGame();
        EconomyService.ResetForNewGame();
        float startCredits = PlayerState.Credits;
        float startStock = EconomyService.StationStock["EarthHub"]["food"];
        float expectedPrice = EconomyService.GetPrice("EarthHub", "food");

        bool ok = EconomyService.TryBuy("EarthHub", "food", 10f, out string _);

        AssertThat(ok).IsTrue();
        AssertThat(PlayerState.GetCargoAmount("food")).IsEqual(10f);
        AssertThat(PlayerState.Credits).IsEqual(startCredits - expectedPrice * 10f);
        AssertThat(EconomyService.StationStock["EarthHub"]["food"]).IsEqual(startStock - 10f);
    }

    [TestCase]
    public void TryBuy_InsufficientCredits_Refuses()
    {
        PlayerState.ResetForNewGame();
        EconomyService.ResetForNewGame();
        PlayerState.TrySpendCredits(990f); // leave 10 credits

        bool ok = EconomyService.TryBuy("EarthHub", "food", 10f, out string _);

        AssertThat(ok).IsFalse();
        AssertThat(PlayerState.GetCargoAmount("food")).IsEqual(0f);
        AssertThat(PlayerState.Credits).IsEqual(10f);
    }

    [TestCase]
    public void TryBuy_InsufficientStock_Refuses()
    {
        PlayerState.ResetForNewGame();
        EconomyService.ResetForNewGame();
        EconomyService.StationStock["EarthHub"]["food"] = 5f;

        bool ok = EconomyService.TryBuy("EarthHub", "food", 50f, out string _);

        AssertThat(ok).IsFalse();
        AssertThat(EconomyService.StationStock["EarthHub"]["food"]).IsEqual(5f);
    }

    [TestCase]
    public void TryBuy_CargoOverflow_RefundsCredits()
    {
        PlayerState.ResetForNewGame();
        EconomyService.ResetForNewGame();
        PlayerState.TryAddCargo("water", 35f); // 35/40 used
        float creditsBefore = PlayerState.Credits;

        bool ok = EconomyService.TryBuy("EarthHub", "food", 10f, out string _);

        AssertThat(ok).IsFalse();
        AssertThat(PlayerState.Credits).IsEqual(creditsBefore); // refunded
        AssertThat(PlayerState.GetCargoAmount("food")).IsEqual(0f);
    }

    [TestCase]
    public void TrySell_SuccessfulTransaction_UpdatesPlayerAndStock()
    {
        PlayerState.ResetForNewGame();
        EconomyService.ResetForNewGame();
        PlayerState.TryAddCargo("food", 20f);
        float startCredits = PlayerState.Credits;
        float startStock = EconomyService.StationStock["EarthHub"]["food"];
        float expectedPrice = EconomyService.GetPrice("EarthHub", "food");

        bool ok = EconomyService.TrySell("EarthHub", "food", 15f, out string _);

        AssertThat(ok).IsTrue();
        AssertThat(PlayerState.GetCargoAmount("food")).IsEqual(5f);
        AssertThat(PlayerState.Credits).IsEqual(startCredits + expectedPrice * 15f);
        AssertThat(EconomyService.StationStock["EarthHub"]["food"]).IsEqual(startStock + 15f);
    }

    [TestCase]
    public void TrySell_NoCargo_Refuses()
    {
        PlayerState.ResetForNewGame();
        EconomyService.ResetForNewGame();

        bool ok = EconomyService.TrySell("EarthHub", "food", 5f, out string _);

        AssertThat(ok).IsFalse();
    }
}
