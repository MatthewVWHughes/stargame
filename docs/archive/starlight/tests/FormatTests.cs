using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Util;

namespace Starlight.Tests;

[TestSuite]
public class FormatTests
{
    [TestCase]
    public void Distance_SmallValue_UsesUnits()
    {
        AssertThat(Format.Distance(420f)).IsEqual("420 u");
    }

    [TestCase]
    public void Distance_ThousandsRange_UsesK()
    {
        AssertThat(Format.Distance(1500f)).IsEqual("1.5k u");
    }

    [TestCase]
    public void Distance_MillionsRange_UsesM()
    {
        AssertThat(Format.Distance(2_400_000f)).IsEqual("2.4M u");
    }

    [TestCase]
    public void Distance_BoundaryAtThousand_SwitchesToK()
    {
        AssertThat(Format.Distance(1000f)).IsEqual("1.0k u");
    }

    [TestCase]
    public void Distance_BoundaryAtMillion_SwitchesToM()
    {
        AssertThat(Format.Distance(1_000_000f)).IsEqual("1.0M u");
    }
}
