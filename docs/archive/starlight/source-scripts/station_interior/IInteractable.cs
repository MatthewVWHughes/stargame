namespace Starlight.StationInterior;

/// <summary>
/// Interface for any node that can be interacted with by the player.
/// FPSController walks up the node tree from the raycast hit to find this.
/// </summary>
public interface IInteractable
{
    string InteractLabel { get; }
    bool CanInteract(FPSController player);
    void OnInteract(FPSController player);
}
