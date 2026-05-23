using Godot;
using Starlight.Ship;

namespace Starlight.Game.Space;

/// <summary>
/// Ship-local supercruise bubble. The old implementation warped the already
/// rendered screen; this one renders actual geometry around the ship/camera:
/// a calm bubble shell plus a forward flow tunnel.
/// </summary>
public partial class SupercruiseWarp : Node3D
{
	[Export] public NodePath ShipPath { get; set; }
	[Export] public string ShaderPath { get; set; } = "res://shaders/SupercruiseWarp.gdshader";

	[Export] public float FadeInTime { get; set; } = 0.65f;
	[Export] public float FadeOutTime { get; set; } = 0.35f;
	[Export] public float BubbleRadius { get; set; } = 22f;
	[Export] public float TunnelLength { get; set; } = 170f;
	[Export] public float TunnelRadius { get; set; } = 24f;
	[Export] public float TunnelOffset { get; set; } = 82f;
	[Export] public float TunnelIntensity { get; set; } = 0.55f;
	[Export] public float BubbleRimIntensity { get; set; } = 0.7f;
	[Export] public float FlowSpeed { get; set; } = 3.2f;

	private ShipController _ship;
	private MeshInstance3D _bubble;
	private MeshInstance3D _tunnel;
	private ShaderMaterial _bubbleMaterial;
	private ShaderMaterial _tunnelMaterial;
	private float _activation;

	public override void _Ready()
	{
		if (ShipPath != null && !ShipPath.IsEmpty)
			_ship = GetNodeOrNull<ShipController>(ShipPath);

		// Legacy child from the old fullscreen post-process scene. Keep the
		// scene compatible, but make this implementation own the visible effect.
		if (GetNodeOrNull<MeshInstance3D>("WarpQuad") is { } legacyQuad)
			legacyQuad.Visible = false;

		Shader shader = GD.Load<Shader>(ShaderPath);
		if (shader == null)
		{
			GD.PushWarning($"SupercruiseWarp: shader not found at {ShaderPath}");
			SetProcess(false);
			return;
		}

		_bubbleMaterial = BuildMaterial(shader, effectMode: 0);
		_tunnelMaterial = BuildMaterial(shader, effectMode: 1);
		BuildBubble();
		BuildTunnel();
		SetWarpVisible(false);
	}

	public override void _Process(double delta)
	{
		if (_ship == null || !IsInstanceValid(_ship) || _bubbleMaterial == null || _tunnelMaterial == null)
			return;

		float dt = (float)delta;
		float target = _ship.Mode switch
		{
			ShipController.FlightMode.Supercruise => 1f,
			ShipController.FlightMode.SupercruiseSpool => _ship.SpoolPercent,
			_ => 0f,
		};

		float rate = target > _activation
			? 1f / Mathf.Max(FadeInTime, 0.001f)
			: 1f / Mathf.Max(FadeOutTime, 0.001f);
		_activation = Mathf.MoveToward(_activation, target, rate * dt);

		bool visible = _activation > 0.001f || target > 0f;
		SetWarpVisible(visible);
		if (!visible)
			return;

		float speedFactor = GetSpeedFactor();
		UpdateMaterial(_bubbleMaterial, speedFactor);
		UpdateMaterial(_tunnelMaterial, speedFactor);
	}

	private ShaderMaterial BuildMaterial(Shader shader, int effectMode)
	{
		var mat = new ShaderMaterial { Shader = shader };
		mat.SetShaderParameter("effect_mode", effectMode);
		mat.SetShaderParameter("tint_color", new Color(0.52f, 0.78f, 1f, 1f));
		return mat;
	}

	private void BuildBubble()
	{
		_bubble = new MeshInstance3D
		{
			Name = "WarpBubble",
			Mesh = new SphereMesh
			{
				Radius = BubbleRadius,
				Height = BubbleRadius * 2f,
				RadialSegments = 72,
				Rings = 36,
			},
			MaterialOverride = _bubbleMaterial,
			ExtraCullMargin = 512f,
			CastShadow = GeometryInstance3D.ShadowCastingSetting.Off,
		};
		AddChild(_bubble);
	}

	private void BuildTunnel()
	{
		_tunnel = new MeshInstance3D
		{
			Name = "WarpTunnel",
			Mesh = new CylinderMesh
			{
				TopRadius = TunnelRadius,
				BottomRadius = TunnelRadius,
				Height = TunnelLength,
				RadialSegments = 96,
				Rings = 28,
			},
			Position = new Vector3(0f, 0f, -TunnelOffset),
			Rotation = new Vector3(Mathf.Pi / 2f, 0f, 0f),
			MaterialOverride = _tunnelMaterial,
			ExtraCullMargin = 512f,
			CastShadow = GeometryInstance3D.ShadowCastingSetting.Off,
		};
		AddChild(_tunnel);
	}

	private void UpdateMaterial(ShaderMaterial mat, float speedFactor)
	{
		mat.SetShaderParameter("activation", _activation);
		mat.SetShaderParameter("speed_factor", speedFactor);
		mat.SetShaderParameter("bubble_radius", BubbleRadius);
		mat.SetShaderParameter("tunnel_length", TunnelLength);
		mat.SetShaderParameter("tunnel_radius", TunnelRadius);
		mat.SetShaderParameter("bubble_rim_intensity", BubbleRimIntensity);
		mat.SetShaderParameter("tunnel_intensity", TunnelIntensity);
		mat.SetShaderParameter("flow_speed", FlowSpeed);
	}

	private float GetSpeedFactor()
	{
		float reference = _ship.Mode == ShipController.FlightMode.Supercruise
			? Mathf.Max(_ship.SupercruiseSpeedLimit, _ship.NormalMaxSpeed)
			: Mathf.Max(_ship.SupercruiseEntrySpeed, _ship.NormalMaxSpeed);
		return Mathf.Clamp(_ship.Speed / Mathf.Max(reference, 1f), 0f, 1f);
	}

	private void SetWarpVisible(bool visible)
	{
		if (_bubble != null) _bubble.Visible = visible;
		if (_tunnel != null) _tunnel.Visible = visible;
	}
}
