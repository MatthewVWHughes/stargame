using Godot;

namespace Starlight.StationInterior;

/// <summary>
/// Generic interactable Area3D for runtime-generated props.
/// Dispatches interaction based on the prop's catalog info.
/// </summary>
public partial class InteractableArea3D : Area3D, IInteractable
{
    public PropCatalog.PropInfo PropInfo { get; set; }
    public string InteractLabel { get; set; } = "Interact";

    public bool CanInteract(FPSController player) => true;

    public void OnInteract(FPSController player)
    {
        switch (PropInfo?.Id)
        {
            case "console_terminal":
                GD.Print("Accessing terminal...");
                break;
            case "panel_electric":
                GD.Print("Examining electrical panel...");
                break;
            default:
                GD.Print($"Interacted with {PropInfo?.Label ?? Name}");
                break;
        }
    }
}
