using GdUnit4;
using static GdUnit4.Assertions;
using Starlight.Game.Missions;

namespace Starlight.Tests;

/// <summary>
/// Tests for the current Ink-shaped stub. Validates that the stub's public
/// contract holds — when Ink gets swapped in later, these tests should be
/// rewritten or replaced by a real integration suite.
/// </summary>
[TestSuite]
public class MissionDialogServiceTests
{
    [TestCase]
    public void NewInstance_IsNotLoaded()
    {
        var svc = new MissionDialogService();

        AssertThat(svc.IsLoaded).IsFalse();
    }

    [TestCase]
    public void NewInstance_HasNoChoices()
    {
        var svc = new MissionDialogService();

        AssertThat(svc.HasChoices).IsFalse();
        AssertThat(svc.GetChoices().Count).IsEqual(0);
    }

    [TestCase]
    public void LoadStory_Stub_ReturnsFalse()
    {
        var svc = new MissionDialogService();

        bool loaded = svc.LoadStory("res://stories/test.ink.json");

        AssertThat(loaded).IsFalse();
        AssertThat(svc.IsLoaded).IsFalse();
    }

    [TestCase]
    public void Continue_ReturnsEmptyString()
    {
        var svc = new MissionDialogService();

        string line = svc.Continue();

        AssertThat(line).IsEqual("");
    }

    [TestCase]
    public void GetVariable_Stub_ReturnsNull()
    {
        var svc = new MissionDialogService();

        object val = svc.GetVariable("any");

        AssertThat(val).IsNull();
    }

    [TestCase]
    public void Choose_Stub_DoesNotThrow()
    {
        var svc = new MissionDialogService();

        svc.Choose(0);

        // If we got here without exception the stub is well-behaved.
        AssertThat(svc.IsLoaded).IsFalse();
    }
}
