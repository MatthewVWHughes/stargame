using System;
using System.Collections.Generic;
using Godot;

namespace Starlight.Game.UI;

/// <summary>
/// Small, reusable dialog panel for NPC conversations and multi-choice prompts in VS1.
/// Separate from CommsMenuController (which is specifically for ship-to-station docking requests).
///
/// Callers pass a title, body, and a list of labeled choices. Each choice fires a callback and
/// (by default) closes the panel.
/// </summary>
public partial class DialogMenuController : CanvasLayer
{
    public sealed record Choice(string Label, Action OnSelected, bool CloseAfter = true);

    private Label _titleLabel;
    private Label _bodyLabel;
    private VBoxContainer _choicesContainer;

    public event Action Opened;
    public event Action Closed;

    public bool IsOpen => Visible;

    public override void _Ready()
    {
        Visible = false;
        ProcessMode = ProcessModeEnum.Always;

        _titleLabel = GetNode<Label>("Panel/Margin/VBox/Title");
        _bodyLabel = GetNode<Label>("Panel/Margin/VBox/Body");
        _choicesContainer = GetNode<VBoxContainer>("Panel/Margin/VBox/Choices");
    }

    public void Show(string title, string body, IReadOnlyList<Choice> choices)
    {
        _titleLabel.Text = title ?? "";
        _bodyLabel.Text = body ?? "";

        ClearChoices();
        if (choices != null)
        {
            foreach (Choice choice in choices)
                AddChoice(choice);
        }

        if (!Visible)
        {
            Visible = true;
            Opened?.Invoke();
        }
    }

    public void Close()
    {
        if (!Visible)
            return;

        Visible = false;
        ClearChoices();
        Closed?.Invoke();
    }

    private void AddChoice(Choice choice)
    {
        var button = new Button
        {
            Text = choice.Label,
            CustomMinimumSize = new Vector2(0f, 40f),
        };
        button.Pressed += () =>
        {
            choice.OnSelected?.Invoke();
            if (choice.CloseAfter)
                Close();
        };
        _choicesContainer.AddChild(button);
    }

    private void ClearChoices()
    {
        foreach (Node child in _choicesContainer.GetChildren())
            child.QueueFree();
    }
}
