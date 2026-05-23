using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Game.Space;

namespace Starlight.Tests;

[TestSuite]
public class StarSystemCatalogTests
{
    [TestCase]
    public void KnownIds_IncludesSolAndAlphaCentauri()
    {
        var ids = StarSystemCatalog.KnownIds;

        AssertThat(ids).Contains("sol");
        AssertThat(ids).Contains("alpha_centauri");
    }
}
