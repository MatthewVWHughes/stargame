using Godot;

namespace Starlight.Game.Space;

/// <summary>
/// Drives the Earth's shader suite and daily rotation. Pushes a world-space
/// sun_direction vector into the surface / cloud / atmosphere materials each
/// frame so the day/night terminator, night-light emission, cloud dimming,
/// and atmosphere scattering align with the star's real position. Applies
/// axial tilt once at _Ready, then spins around the tilted Y axis on a
/// configurable period.
/// </summary>
public partial class EarthShader : Node3D
{
	[Export] public NodePath StarPath { get; set; }
	[Export] public string SurfaceNode { get; set; } = "EarthSurface";
	[Export] public string CloudsNode { get; set; } = "EarthClouds";
	[Export] public string AtmoNode { get; set; } = "EarthAtmo";
	[Export] public float RotationPeriod { get; set; } = 1200f;
	[Export] public float AxialTiltDegrees { get; set; } = 23.44f;

	private Node3D _star;
	private ShaderMaterial _surfaceMat;
	private ShaderMaterial _cloudsMat;
	private ShaderMaterial _atmoMat;
	private Basis _axialTilt = Basis.Identity;
	private Vector3 _localOrigin;
	private double _elapsed;

	public override void _Ready()
	{
		if (StarPath != null && !StarPath.IsEmpty)
			_star = GetNodeOrNull<Node3D>(StarPath);

		_surfaceMat = GetMat(SurfaceNode);
		_cloudsMat = GetMat(CloudsNode);
		_atmoMat = GetMat(AtmoNode);

		_axialTilt = Basis.Identity.Rotated(Vector3.Right, Mathf.DegToRad(AxialTiltDegrees));
		_localOrigin = Position;
	}

	// Used by StarSystemBuilder after the scene is instantiated. The NodePath
	// route only works when StarPath is set in the scene file (tscn); the
	// builder adds the Earth instance to the tree first, firing _Ready with a
	// null path, so it must hand us the star reference directly.
	public void SetStar(Node3D star)
	{
		_star = star;
	}

	private ShaderMaterial GetMat(string childName)
	{
		var mi = GetNodeOrNull<MeshInstance3D>(childName);
		return mi?.MaterialOverride as ShaderMaterial;
	}

	public override void _Process(double delta)
	{
		_elapsed += delta;

		float period = Mathf.Max(RotationPeriod, 0.1f);
		float spinAngle = (float)(_elapsed * Mathf.Tau / period);
		Basis spin = Basis.Identity.Rotated(Vector3.Up, spinAngle);
		Transform = new Transform3D(_axialTilt * spin, _localOrigin);

		if (_star == null || !IsInstanceValid(_star)) return;

		Vector3 sunDir = (_star.GlobalPosition - GlobalPosition).Normalized();
		_surfaceMat?.SetShaderParameter("sun_direction", sunDir);
		_cloudsMat?.SetShaderParameter("sun_direction", sunDir);
		_atmoMat?.SetShaderParameter("sun_direction", sunDir);
	}
}
