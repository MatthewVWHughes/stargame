using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Npc;

namespace Starlight.Tests;

/// <summary>
/// Tests for the Tier 3 NPC simulation data structure.
/// Uses Godot's RandomNumberGenerator (pure managed; no runtime needed).
/// </summary>
[TestSuite]
public class DistantSectorTests
{
    [TestCase]
    public void Construction_PopulatesCountsFromArgs()
    {
        var sector = new DistantSector("TestSector", stationCount: 5, routeCount: 8, shipCount: 20);

        AssertThat(sector.Name).IsEqual("TestSector");
        AssertThat(sector.StationCount).IsEqual(5);
        AssertThat(sector.RouteCount).IsEqual(8);
        AssertThat(sector.ShipCount).IsEqual(20);
    }

    [TestCase]
    public void Construction_ZeroInitialArrivals()
    {
        var sector = new DistantSector("TestSector", 5, 8, 20);

        AssertThat(sector.TotalArrivals).IsEqual(0);
    }

    [TestCase]
    public void Update_ZeroDelta_DoesNotAdvanceArrivals()
    {
        var sector = new DistantSector("TestSector", 5, 8, 20);

        sector.Update(0f);

        AssertThat(sector.TotalArrivals).IsEqual(0);
    }

    [TestCase]
    public void Update_AllShipsAccountedForAfterTick()
    {
        var sector = new DistantSector("TestSector", 5, 8, 20);

        sector.Update(0f);

        AssertThat(sector.ShipsInTransit + sector.ShipsDocked).IsEqual(20);
    }

    [TestCase]
    public void Update_LargeDeltaProducesArrivals()
    {
        // Generous travel times are 60–600s; a 1000s tick should push
        // every ship past its arrival threshold at least once.
        var sector = new DistantSector("TestSector", 5, 8, shipCount: 50);

        sector.Update(1000f);

        AssertThat(sector.TotalArrivals).IsGreater(0);
    }
}
