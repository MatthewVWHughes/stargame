using GdUnit4;
using static GdUnit4.Assertions;

namespace Starlight.Tests;

[TestSuite]
public class SmokeTest
{
    [TestCase]
    public void TestFrameworkIsWired()
    {
        AssertThat(1 + 1).IsEqual(2);
    }
}
