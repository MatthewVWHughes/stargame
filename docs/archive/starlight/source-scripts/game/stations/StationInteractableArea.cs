using Godot;
using System;
using Starlight.StationInterior;

namespace Starlight.Game.Stations;

/// <summary>
/// Interaction point for VS1 stations.
/// Implements the prototype IInteractable contract so the shared <see cref="FPSController"/>
/// discovers it via its forward raycast (layer 4, CollideWithAreas). The VS1 layer still
/// receives a typed <see cref="Triggered"/> event with the station-specific ActionId.
/// </summary>
public partial class StationInteractableArea : Area3D, IInteractable
{
    [Export] public string ActionId { get; set; } = "";
    [Export] public string InteractLabel { get; set; } = "Interact";
    [Export] public float InteractionRange { get; set; } = 2.75f;

    public event Action<string> Triggered;

    public override void _Ready()
    {
        AddToGroup("vs1_station_interactable");
    }

    public bool CanInteract(FPSController player) => true;

    public void OnInteract(FPSController player) => Trigger();

    public void Trigger()
    {
        Triggered?.Invoke(ActionId);
    }
}
