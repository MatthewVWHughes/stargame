using Godot;

namespace Starlight.Game.Space;

/// <summary>
/// Pushes a world-space sun_direction into the Moon's surface shader each
/// frame so the day/night terminator tracks the real star position.
/// </summary>
public partial class MoonShader : Node3D
{
	[Export] public NodePath StarPath { get; set; }
	[Export] public string MeshNode { get; set; } = "MoonMesh";

	private Node3D _star;
	private ShaderMaterial _mat;

	public override void _Ready()
	{
		if (StarPath != null && !StarPath.IsEmpty)
			_star = GetNodeOrNull<Node3D>(StarPath);

		var mi = GetNodeOrNull<MeshInstance3D>(MeshNode);
		_mat = mi?.MaterialOverride as ShaderMaterial;
	}

	// Runtime wiring for StarSystemBuilder. See EarthShader.SetStar.
	public void SetStar(Node3D star)
	{
		_star = star;
	}

	public override void _Process(double delta)
	{
		if (_star == null || !IsInstanceValid(_star)) return;

		Vector3 sunDir = (_star.GlobalPosition - GlobalPosition).Normalized();
		_mat?.SetShaderParameter("sun_direction", sunDir);
	}
}
