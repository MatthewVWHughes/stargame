using Godot;
using System;

namespace Starlight.Star;

/// <summary>
/// Debug-only class picker for the Sun preview scene. Renders one button per
/// <see cref="Star.StellarClass"/>; clicking a button retargets the referenced Star and
/// re-applies its palette/light immediately — no scene reload required.
/// </summary>
public partial class StarPreviewUI : CanvasLayer
{
    [Export] public NodePath StarPath { get; set; }

    private Label _currentLabel;

    public override void _Ready()
    {
        var star = StarPath != null && !StarPath.IsEmpty ? GetNodeOrNull<Star>(StarPath) : null;
        if (star == null)
        {
            GD.PushWarning("StarPreviewUI: StarPath is not set or does not resolve to a Star node.");
            return;
        }

        var panel = new PanelContainer { Position = new Vector2(20, 20) };
        AddChild(panel);

        var margin = new MarginContainer();
        margin.AddThemeConstantOverride("margin_left", 12);
        margin.AddThemeConstantOverride("margin_right", 12);
        margin.AddThemeConstantOverride("margin_top", 8);
        margin.AddThemeConstantOverride("margin_bottom", 8);
        panel.AddChild(margin);

        var hbox = new HBoxContainer();
        margin.AddChild(hbox);

        var title = new Label { Text = "Star Class:" };
        title.AddThemeColorOverride("font_color", new Color(0.8f, 0.85f, 0.95f));
        hbox.AddChild(title);

        foreach (Star.StellarClass cls in Enum.GetValues<Star.StellarClass>())
        {
            Star.StellarClass captured = cls;
            var btn = new Button { Text = cls.ToString() };
            btn.CustomMinimumSize = new Vector2(40, 0);
            btn.Pressed += () =>
            {
                star.Class = captured;
                star.ApplyClass();
                UpdateCurrentLabel(captured);
            };
            hbox.AddChild(btn);
        }

        _currentLabel = new Label();
        _currentLabel.AddThemeColorOverride("font_color", new Color(1f, 0.9f, 0.5f));
        hbox.AddChild(_currentLabel);
        UpdateCurrentLabel(star.Class);
    }

    private void UpdateCurrentLabel(Star.StellarClass cls)
    {
        if (_currentLabel != null)
            _currentLabel.Text = $"(current: {cls})";
    }
}
