using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Game.Inventory;

namespace Starlight.Tests;

[TestSuite]
public class ItemStackTests
{
    [TestCase]
    public void Default_IsEmpty()
    {
        var stack = new ItemStack();

        AssertThat(stack.IsEmpty).IsTrue();
    }

    [TestCase]
    public void ZeroCount_IsEmpty()
    {
        var stack = new ItemStack("food", 0f);

        AssertThat(stack.IsEmpty).IsTrue();
    }

    [TestCase]
    public void PopulatedStack_IsNotEmpty()
    {
        var stack = new ItemStack("food", 10f);

        AssertThat(stack.IsEmpty).IsFalse();
        AssertThat(stack.ItemId).IsEqual("food");
        AssertThat(stack.Count).IsEqual(10f);
    }

    [TestCase]
    public void NegativeCount_IsEmpty()
    {
        var stack = new ItemStack("food", -5f);

        AssertThat(stack.IsEmpty).IsTrue();
    }
}
