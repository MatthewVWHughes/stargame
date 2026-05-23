using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Game.Stations;

namespace Starlight.Tests;

[TestSuite]
public class StationRegistryTests
{
    [TestCase]
    public void Get_KnownStation_ReturnsDefinition()
    {
        StationDefinition def = StationRegistry.Get("EarthHub");

        AssertThat(def).IsNotNull();
        AssertThat(def.StationId).IsEqual("EarthHub");
    }

    [TestCase]
    public void Get_UnknownStation_ReturnsNull()
    {
        AssertThat(StationRegistry.Get("Nowhere")).IsNull();
        AssertThat(StationRegistry.Get("")).IsNull();
    }

    [TestCase]
    public void ResolveInteriorScene_CivilianHub_UsesStationStubScene()
    {
        string path = StationRegistry.ResolveInteriorScene("EarthHub");

        AssertThat(path).IsEqual(StationRegistry.CivilianStationScene);
    }

    [TestCase]
    public void ResolveInteriorScene_HostileBoarding_UsesHostileStationScene()
    {
        string path = StationRegistry.ResolveInteriorScene("DerelictOutpost");

        AssertThat(path).IsEqual(StationRegistry.HostileStationScene);
    }

    [TestCase]
    public void ResolveInteriorScene_UnknownStation_FallsBackToCivilian()
    {
        string path = StationRegistry.ResolveInteriorScene("Nowhere");

        AssertThat(path).IsEqual(StationRegistry.CivilianStationScene);
    }

    [TestCase]
    public void GetFaction_NullOrEmpty_ReturnsNull()
    {
        AssertThat(StationRegistry.GetFaction(null)).IsNull();
        AssertThat(StationRegistry.GetFaction("")).IsNull();
    }
}
