using Godot;
using Starlight.Ship;

namespace Starlight.Game.Space;

/// <summary>
/// Volumetric space-dust field that sells motion through vacuum. Particles
/// live in a cube around the ship and wrap torus-style when they cross a
/// face, so the field always surrounds the camera without ever spawning or
/// destroying anything.
///
/// Each instance is rendered by SpeedStreaks.gdshader as a screen-space
/// pixel-sized dot (stays ~DotPixels wide regardless of distance). The
/// primary motion cue is parallax — dots flying past the camera — not
/// world-space stretching of the billboard. At higher speeds the shader
/// stretches each dot into a short capsule along the screen-projected
/// velocity direction for an extra motion cue.
///
/// Reference-frame awareness: particles live as offsets in a ship-centered
/// visual volume and move opposite ShipController.VisualVelocity. Orbital
/// frame motion is deliberately excluded, so a ship parked near a moving
/// planet sees calm local dust instead of heliocentric streaking.
/// </summary>
public partial class SpeedStreaks : Node3D
{
	[Export] public NodePath ShipPath { get; set; }
	[Export] public string MultiMeshNodeName { get; set; } = "MultiMesh";

	[Export] public float VolumeSize { get; set; } = 80f;
	[Export] public int ParticleCount { get; set; } = 320;

	/// <summary>Dot diameter in screen pixels. Stays constant regardless of distance.</summary>
	[Export] public float DotPixels { get; set; } = 2.5f;

	/// <summary>Extra streak length (pixels) per m/s of ship speed above FadeInSpeed.</summary>
	[Export] public float StreakPixelsPerSpeed { get; set; } = 0.25f;

	/// <summary>Maximum streak length in screen pixels.</summary>
	[Export] public float MaxStreakPixels { get; set; } = 60f;

	/// <summary>Cap on the speed used for streak-length calculation. In warp the
	/// ship moves at thousands of m/s but it's stationary in its bubble frame —
	/// dust rushes past like normal flight, not as kilometre-long streaks.</summary>
	[Export] public float StreakSpeedCap { get; set; } = 70f;

	/// <summary>At high speed the wrap volume grows so particles don't strobe-wrap
	/// every frame. effectiveVolume = max(VolumeSize, shipSpeed * this). Units are
	/// seconds of travel per wrap — 2 s gives a clean flow at any speed.</summary>
	[Export] public float VolumeSpeedFactor { get; set; } = 2f;

	[Export] public float FadeInSpeed { get; set; } = 0.5f;
	[Export] public float FullVisibleSpeed { get; set; } = 8f;
	[Export] public float ForwardSampleDistance { get; set; } = 10f;
	[Export] public float WarpFlowBend { get; set; } = 0.2f;
	[Export] public float WarpBowStretch { get; set; } = 1.2f;
	[Export] public float WarpSternSpread { get; set; } = 0.4f;
	[Export] public float WarpBubbleRadius { get; set; } = 0.14f;
	[Export] public float WarpFieldLength { get; set; } = 70f;
	[Export] public float WarpFieldRadius { get; set; } = 28f;
	[Export] public float WarpFlowStrength { get; set; } = 26f;
	[Export] public float WarpBowPull { get; set; } = 18f;
	[Export] public float WarpSternPush { get; set; } = 12f;

	/// <summary>Faint visibility at rest so ambient dust is still subtly there.</summary>
	[Export(PropertyHint.Range, "0,1,0.01")] public float RestVisibility { get; set; } = 0.25f;

	private ShipController _ship;
	private MultiMeshInstance3D _mmi;
	private MultiMesh _multimesh;
	private ShaderMaterial _shaderMat;
	private Vector3[] _particles;
	private System.Random _rng = new System.Random(42);

	public override void _Ready()
	{
		if (ShipPath != null && !ShipPath.IsEmpty)
			_ship = GetNodeOrNull<ShipController>(ShipPath);

		_mmi = GetNodeOrNull<MultiMeshInstance3D>(MultiMeshNodeName);
		if (_mmi == null || _mmi.Multimesh == null)
		{
			GD.PushWarning("SpeedStreaks: MultiMeshInstance3D/MultiMesh not found");
			SetProcess(false);
			return;
		}

		_multimesh = _mmi.Multimesh;
		_shaderMat = _mmi.MaterialOverride as ShaderMaterial;

		_multimesh.InstanceCount = ParticleCount;

		_particles = new Vector3[ParticleCount];
		for (int i = 0; i < ParticleCount; i++)
			_particles[i] = RandomOffset();
	}

	private Vector3 RandomOffset()
	{
		return new Vector3(
			((float)_rng.NextDouble() - 0.5f) * VolumeSize,
			((float)_rng.NextDouble() - 0.5f) * VolumeSize,
			((float)_rng.NextDouble() - 0.5f) * VolumeSize);
	}

	public override void _Process(double delta)
	{
		if (_ship == null || !IsInstanceValid(_ship) || _multimesh == null) return;
		float dt = (float)delta;

		// Use the interpolated pose so dust reads the same smoothed ship
		// position as the camera does. With physics_interpolation enabled,
		// reading raw GlobalPosition here would lag the visible ship by up to
		// one physics tick.
		Transform3D shipXform = _ship.GetGlobalTransformInterpolated();
		Vector3 shipPos = shipXform.Origin;
		Vector3 shipVel = _ship.VisualVelocity;
		float shipSpeed = shipVel.Length();

		// Streak length uses a capped reference speed so warp dust looks like
		// normal flight streaks whipping past a stationary (in-bubble) ship,
		// not impossible km-long trails.
		float streakSpeed = Mathf.Min(shipSpeed, StreakSpeedCap);
		float streakPixels = Mathf.Clamp(
			(streakSpeed - FadeInSpeed) * StreakPixelsPerSpeed,
			0f, MaxStreakPixels);

		float speedT = Mathf.Clamp(
			(shipSpeed - FadeInSpeed) / Mathf.Max(FullVisibleSpeed - FadeInSpeed, 0.01f),
			0f, 1f);
		float visibility = Mathf.Lerp(RestVisibility, 1f, speedT);
		Color custom = new Color(visibility, 0f, 0f, 0f);

		Vector3 velDir = shipSpeed > 0.001f ? shipVel / shipSpeed : Vector3.Forward;

		// Wrap volume grows with real world-space speed so particles don't
		// strobe-wrap every frame during warp. At 6 km/s with 80 m volume the
		// ship crosses the entire field every ~13 ms — unwatchable. This
		// gives ~VolumeSpeedFactor seconds between wraps at any speed.
		float effectiveVolume = Mathf.Max(VolumeSize, shipSpeed * VolumeSpeedFactor);
		float warpActivation = _ship.Mode switch
		{
			ShipController.FlightMode.Supercruise => 1f,
			ShipController.FlightMode.SupercruiseSpool => _ship.SpoolPercent,
			_ => 0f,
		};

		if (_shaderMat != null)
		{
			var cam = GetViewport().GetCamera3D();
			Vector2 shipUv = new(0.5f, 0.5f);
			Vector2 forwardUv = new(0.5f, 0.5f);
			Vector2 vp = GetViewport().GetVisibleRect().Size;
			if (cam != null && vp.X > 1f && vp.Y > 1f)
			{
				if (!cam.IsPositionBehind(shipPos))
				{
					Vector2 shipPx = cam.UnprojectPosition(shipPos);
					shipUv = new Vector2(shipPx.X / vp.X, shipPx.Y / vp.Y);
				}

				Vector3 forwardWorld = shipPos + (-shipXform.Basis.Z * ForwardSampleDistance);
				if (!cam.IsPositionBehind(forwardWorld))
				{
					Vector2 forwardPx = cam.UnprojectPosition(forwardWorld);
					forwardUv = new Vector2(forwardPx.X / vp.X, forwardPx.Y / vp.Y);
				}
			}

			_shaderMat.SetShaderParameter("velocity_dir", velDir);
			_shaderMat.SetShaderParameter("dot_pixels", DotPixels);
			_shaderMat.SetShaderParameter("streak_pixels", streakPixels);
			_shaderMat.SetShaderParameter("ship_screen", shipUv);
			_shaderMat.SetShaderParameter("forward_screen", forwardUv);
			_shaderMat.SetShaderParameter("warp_activation", warpActivation);
			_shaderMat.SetShaderParameter("flow_bend", WarpFlowBend);
			_shaderMat.SetShaderParameter("bow_stretch", WarpBowStretch);
			_shaderMat.SetShaderParameter("stern_spread", WarpSternSpread);
			_shaderMat.SetShaderParameter("bubble_radius", WarpBubbleRadius);
		}

		for (int i = 0; i < ParticleCount; i++)
		{
			Vector3 rel = _particles[i];
			rel -= shipVel * dt;
			Vector3 p = shipPos + rel;

			if (warpActivation > 0.001f)
				p += ComputeWarpFlowOffset(p, shipXform, dt, warpActivation);

			// Torus wrap on each axis. Particles are stored in ship-centered
			// visual space, so scene origin shifts and orbital frame movement
			// do not inject false motion into the dust field.
			rel = p - shipPos;
			rel.X -= Mathf.Round(rel.X / effectiveVolume) * effectiveVolume;
			rel.Y -= Mathf.Round(rel.Y / effectiveVolume) * effectiveVolume;
			rel.Z -= Mathf.Round(rel.Z / effectiveVolume) * effectiveVolume;
			p = shipPos + rel;
			_particles[i] = rel;

			// Identity basis — shader does all billboard math in clip space.
			var tf = new Transform3D(Basis.Identity, p);
			_multimesh.SetInstanceTransform(i, tf);
			_multimesh.SetInstanceCustomData(i, custom);
		}
	}

	private Vector3 ComputeWarpFlowOffset(Vector3 particlePosition, Transform3D shipXform, float dt, float activation)
	{
		if (_ship == null)
			return Vector3.Zero;

		Basis basis = shipXform.Basis;
		Vector3 forward = -basis.Z;
		Vector3 right = basis.X;
		Vector3 up = basis.Y;

		Vector3 local = particlePosition - shipXform.Origin;
		float forwardPos = local.Dot(forward);
		float rightPos = local.Dot(right);
		float upPos = local.Dot(up);
		float radial = Mathf.Sqrt(rightPos * rightPos + upPos * upPos);

		float axisFalloff = Mathf.Exp(-Mathf.Abs(forwardPos) / Mathf.Max(WarpFieldLength, 0.01f));
		float radialFalloff = Mathf.Exp(-(radial * radial) / Mathf.Max(WarpFieldRadius * WarpFieldRadius, 0.01f));
		float coreMask = 1f - Mathf.Exp(-(radial * radial) / Mathf.Max(36f, 0.01f));
		float ahead = Mathf.Clamp(forwardPos / Mathf.Max(WarpFieldLength, 0.01f), 0f, 1f);
		float behind = Mathf.Clamp(-forwardPos / Mathf.Max(WarpFieldLength, 0.01f), 0f, 1f);

		Vector3 tangent = forward;
		if (radial > 0.001f)
		{
			Vector3 radialDir = (right * rightPos + up * upPos) / radial;
			tangent = (tangent - radialDir * (rightPos / Mathf.Max(WarpFieldRadius, 0.01f)) * WarpFlowBend).Normalized();
		}

		float bowPull = ahead * WarpBowPull;
		float sternPush = behind * WarpSternPush;
		float flow = WarpFlowStrength * axisFalloff * radialFalloff * coreMask * activation;
		return tangent * (flow + bowPull - sternPush) * dt;
	}
}
