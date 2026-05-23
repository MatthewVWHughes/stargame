using Godot;

namespace Starlight.Game.UI;

/// <summary>
/// Small combat/debug status panel for VS1.
/// </summary>
public partial class CombatHudController : CanvasLayer
{
	private Label _statusLabel;
	private Label _summaryLabel;
	private Label _hintLabel;

	public override void _Ready()
	{
		_statusLabel = GetNode<Label>("StatusLabel");
		_summaryLabel = GetNode<Label>("SummaryLabel");
		_hintLabel = GetNode<Label>("HintLabel");
		_statusLabel.Visible = false;
		_summaryLabel.Visible = false;
		_hintLabel.Visible = false;
		Visible = true;
	}

	public void SetStatus(string message)
	{
		_statusLabel.Text = message;
		_statusLabel.Visible = !string.IsNullOrWhiteSpace(message);
	}

	public void SetSummary(string message)
	{
		_summaryLabel.Text = message;
		_summaryLabel.Visible = !string.IsNullOrWhiteSpace(message);
	}

	public void SetHint(string message)
	{
		_hintLabel.Text = message;
		_hintLabel.Visible = !string.IsNullOrWhiteSpace(message);
	}
}
