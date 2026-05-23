using Godot;
using System.Collections.Generic;

namespace Starlight.Game.Missions;

/// <summary>
/// Narrative script runner for bar contacts / mission dialogue.
///
/// Scaffold only. The runtime API mirrors Inkle's Ink story interface
/// (Continue / currentChoices / ChooseChoiceIndex / variablesState) so
/// swapping in the real runtime later is a localized change.
///
/// To wire the real runtime, either:
///  1. Install the paulloz/godot-ink addon (C# editor integration), OR
///  2. Reference the inkle Ink runtime NuGet and replace the stub
///     implementation below with Ink.Runtime.Story calls.
///
/// Callers should treat this class as a black box: load a story, Continue
/// until HasChoices, present choices, Choose, repeat.
/// </summary>
public sealed class MissionDialogService
{
    private readonly List<string> _scriptLog = new();
    private string _lastLine = "";
    private bool _finished = true;

    public bool IsLoaded => !_finished;
    public bool HasChoices => false;
    public string CurrentLine => _lastLine;

    public IReadOnlyList<string> GetChoices() => System.Array.Empty<string>();

    /// <summary>
    /// Load a compiled Ink story (.ink.json). Stub returns false until a
    /// real runtime is wired up — callers should fall back to static
    /// MissionDefinition dialogue.
    /// </summary>
    public bool LoadStory(string resPath)
    {
        // Stub log via stderr — the real runtime swap will replace this
        // whole method. Using System.Console so tests don't need the Godot
        // runtime (GD.Print does).
        System.Console.Error.WriteLine(
            $"MissionDialogService: story load requested for '{resPath}'. Stub — no runtime wired yet.");
        _finished = true;
        return false;
    }

    public string Continue()
    {
        _finished = true;
        return _lastLine;
    }

    public void Choose(int index)
    {
        // No-op until runtime wired.
    }

    public object GetVariable(string name) => null;
    public void SetVariable(string name, object value) { /* no-op */ }
}
