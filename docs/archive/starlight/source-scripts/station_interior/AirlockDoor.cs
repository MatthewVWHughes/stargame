using Godot;

namespace Starlight.StationInterior;

/// <summary>
/// Attach to an airlock scene root. Finds the AnimationPlayer and toggles door_open.
/// The interaction Area3D is placed in the .tscn, not generated in code.
/// </summary>
public partial class AirlockDoor : Node3D
{
	private AnimationPlayer _animPlayer;
	public bool IsOpen;

	public override void _Ready()
	{
		_animPlayer = GetNodeOrNull<AnimationPlayer>("AnimationPlayer");
		if (_animPlayer == null)
		{
			// Search children
			foreach (var child in GetChildren())
			{
				if (child is AnimationPlayer ap)
				{
					_animPlayer = ap;
					break;
				}
			}
		}

		GenerateTrimeshCollisions();
		GD.Print($"AirlockDoor ready. AnimPlayer: {_animPlayer != null}");
	}

	/// <summary>
	/// Creates trimesh StaticBody3D colliders for every visible MeshInstance3D under this airlock.
	/// Kit-bashed pieces ship without collision, so we bake one on entry. Trimesh is accurate enough
	/// for walls/floors and slides along with animated door pieces because the collider is parented
	/// to the mesh that moves.
	/// </summary>
	private void GenerateTrimeshCollisions()
	{
		int added = 0;
		foreach (Node node in GetAllDescendants(this))
		{
			if (node is not MeshInstance3D mi || !mi.Visible || mi.Mesh == null)
				continue;
			// Skip if this mesh already has a StaticBody3D child (already collided).
			bool alreadyHasCollider = false;
			foreach (Node child in mi.GetChildren())
			{
				if (child is StaticBody3D) { alreadyHasCollider = true; break; }
			}
			if (alreadyHasCollider)
				continue;

			mi.CreateTrimeshCollision();
			added++;
		}
		GD.Print($"AirlockDoor: added trimesh collision to {added} meshes.");
	}

	private static System.Collections.Generic.IEnumerable<Node> GetAllDescendants(Node parent)
	{
		foreach (Node child in parent.GetChildren())
		{
			yield return child;
			foreach (Node grand in GetAllDescendants(child))
				yield return grand;
		}
	}

	public void ToggleDoor()
	{
		if (_animPlayer == null) return;

		if (!IsOpen)
		{
			_animPlayer.Play("door_open");
			IsOpen = true;
		}
		else
		{
			_animPlayer.PlayBackwards("door_open");
			IsOpen = false;
		}
	}
}

