using Godot;
using System;

namespace Starlight.Game.UI;

/// <summary>
/// Minimal VS1 comms panel for docking requests.
/// </summary>
public partial class CommsMenuController : CanvasLayer
{
    private Label _titleLabel;
    private Label _bodyLabel;
    private Button _requestDockButton;
    private Button _closeButton;

    public event Action RequestDockPressed;
    public event Action Closed;

    public override void _Ready()
    {
        Visible = false;

        _titleLabel = GetNode<Label>("Panel/VBox/Title");
        _bodyLabel = GetNode<Label>("Panel/VBox/Body");
        _requestDockButton = GetNode<Button>("Panel/VBox/RequestDockButton");
        _closeButton = GetNode<Button>("Panel/VBox/CloseButton");

        _requestDockButton.Pressed += () => RequestDockPressed?.Invoke();
        _closeButton.Pressed += HideMenu;
    }

    public void ShowForStation(string stationName, float distanceMeters, bool canDock)
    {
        Visible = true;
        _titleLabel.Text = "Comms";
        _bodyLabel.Text = canDock
            ? $"{stationName}\nRequest docking clearance?\nDistance: {distanceMeters:F0} m"
            : "No station in docking range.\nMove closer to a station and try again.";
        _requestDockButton.Disabled = !canDock;
    }

    public void ShowMessage(string title, string body)
    {
        Visible = true;
        _titleLabel.Text = title;
        _bodyLabel.Text = body;
        _requestDockButton.Disabled = true;
    }

    public void HideMenu()
    {
        Visible = false;
        Closed?.Invoke();
    }
}
