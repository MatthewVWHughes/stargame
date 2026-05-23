using Godot;

namespace Starlight.StationInterior;

/// <summary>
/// Place this as a child of an Area3D in the airlock scene.
/// Finds the AirlockDoor on a parent and calls ToggleDoor on interact.
/// </summary>
public partial class DoorInteractable : Area3D, IInteractable
{
	private AirlockDoor _door;

	public string InteractLabel => _door != null && !_door.IsOpen ? "Open Door" : "Close Door";
	public bool CanInteract(FPSController player) => true;

	public override void _Ready()
	{
		// Walk up to find AirlockDoor
		Node current = GetParent();
		while (current != null)
		{
			if (current is AirlockDoor door)
			{
				_door = door;
				break;
			}
			current = current.GetParent();
		}
		GD.Print($"DoorInteractable ready. Door found: {_door != null}");
	}

	public void OnInteract(FPSController player)
	{
		_door?.ToggleDoor();
	}
}
