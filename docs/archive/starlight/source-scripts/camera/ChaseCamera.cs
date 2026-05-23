using Godot;
using Starlight.Ship;

namespace Starlight.Camera;

/// <summary>
/// Chase camera for the Freelancer-style flight model.
///
/// Physics-interpolation contract (Godot 4.6):
/// runs in _Process at render rate, engine interpolation is off on this node,
/// and the target pose is read once per frame via GetGlobalTransformInterpolated.
/// Basis and origin come from the same interpolated sample so they never
/// disagree.
///
/// Camera lag is kept in target-local space, not world space. World-space
/// Lerp state accumulates float-precision error when the ship is swept through
/// world space far from the origin; local offsets keep the Freelancer-style
/// weight without reintroducing orbital-frame shimmer.
///
/// FOV is also smoothed as a scalar on the camera itself.
/// </summary>
public partial class ChaseCamera : Camera3D
{
	[Export] public float DistanceBehind { get; set; } = 7.0f;
	[Export] public float HeightAbove { get; set; } = 2.0f;
	[Export] public float SupercruiseDistanceBehind { get; set; } = 11.0f;
	[Export] public float SupercruiseHeightAbove { get; set; } = 2.6f;

	[Export] public float LookAheadDistance { get; set; } = 6.0f;
	[Export] public float LookHeightOffset { get; set; } = 0.4f;
	[Export] public float PositionLagTime { get; set; } = 0.18f;
	[Export] public float LookLagTime { get; set; } = 0.12f;
	[Export] public float MaxPositionLag { get; set; } = 1.8f;
	[Export] public float MaxLookLag { get; set; } = 3.0f;
	[Export] public float TurnLookOffset { get; set; } = 0.9f;
	[Export] public float StrafeLeanOffset { get; set; } = 0.45f;

	[Export] public float BaseFov { get; set; } = 45f;
	[Export] public float SpeedFovRange { get; set; } = 8f;
	[Export] public float FovSmoothTime { get; set; } = 0.25f;

	private Node3D _target;
	private ShipController _ship;
	private float _smoothedSpeedRatio;
	private float _smoothedFov;
	private Vector3 _smoothedLocalPosition;
	private Vector3 _smoothedLocalLook;
	private bool _hasSmoothedPose;

	public void SetTarget(Node3D target)
	{
		_target = target;
		_ship = target as ShipController;
		_smoothedFov = BaseFov;
		_smoothedSpeedRatio = 0f;
		Fov = BaseFov;
		if (_target == null) return;

		Transform3D xform = _target.GetGlobalTransformInterpolated();
		_smoothedLocalPosition = ComputeDesiredLocalPosition();
		_smoothedLocalLook = ComputeDesiredLocalLook();
		_hasSmoothedPose = true;
		GlobalPosition = xform * _smoothedLocalPosition;
	}

	public override void _Ready()
	{
		TopLevel = true;

		// The camera writes its own GlobalPosition every _Process frame. With
		// physics_interpolation=true, leaving interpolation enabled here would
		// have the engine blend the camera transform between physics-tick
		// snapshots — the exact feedback that caused the original jitter.
		PhysicsInterpolationMode = PhysicsInterpolationModeEnum.Off;

		var fillLight = new OmniLight3D
		{
			LightColor = new Color(0.7f, 0.75f, 0.85f),
			LightEnergy = 0.8f,
			OmniRange = 20f,
			OmniAttenuation = 1.0f,
		};
		AddChild(fillLight);
	}

	public override void _Process(double delta)
	{
		if (_target == null) return;
		float dt = (float)delta;
		if (dt <= 0f) return;

		Transform3D xform = _target.GetGlobalTransformInterpolated();

		Vector3 desiredLocalPosition = ComputeDesiredLocalPosition();
		Vector3 desiredLocalLook = ComputeDesiredLocalLook();
		if (!_hasSmoothedPose)
		{
			_smoothedLocalPosition = desiredLocalPosition;
			_smoothedLocalLook = desiredLocalLook;
			_hasSmoothedPose = true;
		}
		else
		{
			_smoothedLocalPosition = SmoothLocalOffset(
				_smoothedLocalPosition, desiredLocalPosition, PositionLagTime, MaxPositionLag, dt);
			_smoothedLocalLook = SmoothLocalOffset(
				_smoothedLocalLook, desiredLocalLook, LookLagTime, MaxLookLag, dt);
		}

		GlobalPosition = xform * _smoothedLocalPosition;

		Vector3 desiredLook = xform * _smoothedLocalLook;
		if (desiredLook.DistanceSquaredTo(GlobalPosition) > 1f)
			LookAt(desiredLook, xform.Basis.Y);

		UpdateFov(dt);
	}

	/// <summary>
	/// Called by GameplayRoot when the world origin shifts.
	/// Local smoothing state is origin-independent; only the written global
	/// transform needs the same nudge as the rest of the scene.
	/// </summary>
	public void ApplyWorldOffset(Vector3 offset)
	{
		GlobalPosition -= offset;
	}

	private Vector3 ComputeDesiredLocalPosition()
	{
		bool supercruise = _ship != null && IsSupercruiseActive(_ship);
		float dist = supercruise ? SupercruiseDistanceBehind : DistanceBehind;
		float height = supercruise ? SupercruiseHeightAbove : HeightAbove;
		float strafeOffset = 0f;
		if (_ship != null)
			strafeOffset = Mathf.Clamp(_ship.LocalVelocity.Dot(_ship.GlobalTransform.Basis.X) / Mathf.Max(_ship.NormalMaxSpeed, 1f), -1f, 1f)
				* StrafeLeanOffset;

		// Godot local basis: +Z is backward, +Y is up, +X is right.
		return new Vector3(strafeOffset, height, dist);
	}

	private Vector3 ComputeDesiredLocalLook()
	{
		Vector3 look = new(0f, LookHeightOffset, -LookAheadDistance);
		if (_ship != null)
		{
			Vector3 angular = _ship.ShipAngularVelocity;
			look.X += Mathf.Clamp(-angular.Y * TurnLookOffset, -MaxLookLag, MaxLookLag);
			look.Y += Mathf.Clamp(angular.X * TurnLookOffset, -MaxLookLag, MaxLookLag);
		}
		return look;
	}

	private static Vector3 SmoothLocalOffset(Vector3 current, Vector3 target, float lagTime, float maxDelta, float dt)
	{
		float t = 1f - Mathf.Exp(-dt / Mathf.Max(lagTime, 0.0001f));
		Vector3 next = current.Lerp(target, t);
		Vector3 delta = next - target;
		if (delta.Length() > maxDelta)
			next = target + delta.Normalized() * maxDelta;
		return next;
	}

	private void UpdateFov(float dt)
	{
		if (_ship == null || SpeedFovRange <= 0f)
		{
			Fov = BaseFov;
			return;
		}

		float rawRatio = GetSpeedRatio(_ship);
		_smoothedSpeedRatio = Mathf.Lerp(_smoothedSpeedRatio, rawRatio,
			1f - Mathf.Exp(-dt / 0.2f));

		float targetFov = BaseFov + SpeedFovRange * _smoothedSpeedRatio;
		_smoothedFov = Mathf.Lerp(_smoothedFov, targetFov,
			1f - Mathf.Exp(-dt / Mathf.Max(FovSmoothTime, 0.0001f)));
		Fov = _smoothedFov;
	}

	private static bool IsSupercruiseActive(ShipController ship)
	{
		return ship.Mode == ShipController.FlightMode.Supercruise
			|| ship.Mode == ShipController.FlightMode.SupercruiseSpool;
	}

	private static float GetSpeedRatio(ShipController ship)
	{
		float referenceSpeed = IsSupercruiseActive(ship)
			? Mathf.Max(ship.SupercruiseSpeedLimit, ship.NormalMaxSpeed)
			: ship.NormalMaxSpeed;
		return Mathf.Clamp(ship.Speed / Mathf.Max(referenceSpeed, 1f), 0f, 1f);
	}
}
