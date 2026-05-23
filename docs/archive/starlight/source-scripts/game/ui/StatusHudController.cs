using Godot;

namespace Starlight.Game.UI;

/// <summary>
/// Small guidance panel so the VS1 slice is easier to navigate and verify.
/// </summary>
public partial class StatusHudController : CanvasLayer
{
    private Label _objectiveLabel;
    private Label _locationLabel;

    public override void _Ready()
    {
        _objectiveLabel = GetNode<Label>("ObjectiveLabel");
        _locationLabel = GetNode<Label>("LocationLabel");
    }

    public void SetObjective(string text)
    {
        _objectiveLabel.Text = text;
    }

    public void SetLocationSummary(string text)
    {
        _locationLabel.Text = text;
    }
}
