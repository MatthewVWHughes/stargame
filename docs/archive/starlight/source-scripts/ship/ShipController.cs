using Godot;

namespace Starlight.Ship;

/// <summary>
/// Gravity well reference for supercruise speed limiting.
/// </summary>
public struct GravityWell
{
	public Node3D Body;
	public float Radius;
	public bool IsGravitySource;
}

public struct StationAssistTarget
{
	public Node3D Body;
	public float Radius;
}

/// <summary>
/// Arcade flight model with Freelancer-style cursor steering.
/// Mouse cursor position relative to screen center steers the ship.
///
///   Normal      — force-based, combat/docking flight (~60 u/s)
///   Spool       — normal flight while the phase drive charges
///   Supercruise — inter-planetary, speed scales with gravity wells
///
/// J: engage or disengage Supercruise when outside lockout
/// Z: engine kill (drops to Normal)
/// Space: toggle mouse steering on/off
/// </summary>
public partial class ShipController : Node3D
{
	[Export] public float ThrustForce { get; set; } = 120f;
	[Export] public float StrafeForce { get; set; } = 42f;
	[Export] public float NormalMaxSpeed { get; set; } = 60f;
	[Export] public float SupercruiseEntrySpeed { get; set; } = 180f;
	[Export] public float SupercruiseSpoolTime { get; set; } = 5f;
	[Export] public float SupercruiseRecoverySeconds { get; set; } = 30f;
	[Export] public float InterdictionLockDecayPerSecond { get; set; } = 0.9f;
	[Export] public float InterdictionLockHoldSeconds { get; set; } = 0.2f;
	[Export] public float PhaseScramblerDurationSeconds { get; set; } = 3f;
	[Export] public float PhaseScramblerCooldownSeconds { get; set; } = 25f;
	[Export] public float PhaseScramblerLockDecayPerSecond { get; set; } = 2.8f;
	[Export] public float SupercruiseNearStarMax { get; set; } = 2000f;
	[Export] public float SupercruiseDeepSpaceMax { get; set; } = 20000f;
	[Export] public float SupercruiseScaleDistance { get; set; } = 3000000f;
	[Export] public float SupercruiseAccel { get; set; } = 800f;
	[Export] public float SupercruiseDecel { get; set; } = 3000f;
	[Export] public float WellRadiusMultiplier { get; set; } = 10f;
	[Export] public float CruiseTurnFactor { get; set; } = 0.25f;
	[Export] public float SteeringForce { get; set; } = 13f;
	[Export] public float RollForce { get; set; } = 13f;
	[Export] public float AngularDrag { get; set; } = 6.5f;
	[Export] public float DeadZone { get; set; } = 0.05f;
	[Export] public float SoiMultiplier { get; set; } = 5f;
	[Export] public float SupercruiseSoiMultiplier { get; set; } = 3f;
	[Export] public float PassiveGravityMinAccel { get; set; } = 0.1f;
	[Export] public float PassiveGravityMaxAccel { get; set; } = 0.9f;
	[Export] public float AutopilotAccel { get; set; } = 40f;
	[Export] public float AutopilotBrakeAccel { get; set; } = 35f;
	[Export] public float AutopilotMaxTurnRate { get; set; } = 1.4f;
	[Export] public float ThrottleRiseRate { get; set; } = 1.45f;
	[Export] public float ThrottleFallRate { get; set; } = 2.2f;
	[Export] public float ZeroThrottleBrakeAccel { get; set; } = 34f;
	[Export] public float FrameHostHysteresisFactor { get; set; } = 1.2f;
	[Export] public float StationAssistBlendInRate { get; set; } = 2.2f;
	[Export] public float StationAssistBlendOutRate { get; set; } = 4.5f;
	[Export] public float StationAssistMaxRelativeSpeed { get; set; } = 10f;
	[Export] public float StationAssistCutoffSpeed { get; set; } = 20f;
	[Export] public float StationAssistThrottleZeroPoint { get; set; } = 0.8f;
	[Export] public float StationAssistMatchAccel { get; set; } = 18f;

	public bool SteeringEnabled { get; set; } = true;

	public Vector3 Velocity { get; private set; }
	public Vector3 LocalVelocity => Velocity;
	public Vector3 FrameVelocity => _frameHostWorldVelocity;
	public Vector3 AppliedFrameVelocity => _mode == FlightMode.Supercruise ? Vector3.Zero : _frameHostWorldVelocity;
	public Vector3 WorldVelocity => Velocity + AppliedFrameVelocity;
	public Vector3 VisualVelocity => Velocity;
	public float Speed => Velocity.Length();
	public Vector3 ShipAngularVelocity => _angularVelocity;
	public FlightMode Mode => _mode;
	public float SpoolPercent => _spool;
	public float SupercruiseSpeedLimit { get; private set; }
	public bool IsEngineKill => _engineKill;
	public bool InsideSupercruiseLockout { get; private set; }
	public float SupercruiseOfflineTimer { get; private set; }
	public bool SupercruiseDriveOffline => SupercruiseOfflineTimer > 0.001f;
	public bool IsInSupercruiseTransit => _mode == FlightMode.Supercruise || _mode == FlightMode.SupercruiseSpool;
	public float InterdictionLockStrength { get; private set; }
	public bool InterdictionLockDetected => InterdictionLockStrength > 0.01f || _interdictionSignalTimer > 0.001f;
	public string InterdictionSourceName => _interdictionSourceName;
	public float InterdictionCollapseDistance { get; private set; }
	public float PhaseScramblerActiveTimer { get; private set; }
	public float PhaseScramblerCooldownTimer { get; private set; }
	public bool PhaseScramblerActive => PhaseScramblerActiveTimer > 0.001f;
	public bool PhaseScramblerReady => PhaseScramblerCooldownTimer <= 0.001f && !PhaseScramblerActive;
	public string DominantBodyName => _frameHost != null && _frameHost != _defaultHost ? _frameHost.Name : "";
	public bool InsideReferenceSoi => _frameHost != null && _frameHost != _defaultHost;
	public string StationAssistName => _stationAssistBody?.Name ?? "";
	public bool StationAssistActive => _stationAssistBody != null && _stationAssistInfluence > 0.01f;
	public bool PassiveDriftActive { get; private set; }
	public bool AutopilotArrived { get; private set; }
	public float AutopilotDistance { get; private set; }
	public float ThrottlePercent { get; private set; }

	public enum FlightMode { Normal, SupercruiseSpool, Supercruise, Autopilot }
	private FlightMode _mode;
	private float _spool;
	private bool _engineKill;
	private float _dragCoeff;
	private float _currentSupercruiseSpeed;
	private float _interdictionSignalTimer;
	private string _interdictionSourceName = "";
	private float _interdictionCollapseEnvelope = -1f;
	private float _pendingInterdictionOfflineSeconds;

	private GravityWell[] _gravityWells;
	private StationAssistTarget[] _stationAssistTargets;
	private Node3D _star;

	// Angular velocity for momentum-based steering.
	private Vector3 _angularVelocity;

	// Reference-frame state. The ship remains a child of the gameplay root so
	// local Position always has one meaning. Inside a body's SOI, that body's
	// sampled world velocity is added numerically to root-space translation;
	// Velocity stays player-relative and WorldVelocity is exposed for combat.
	private Node3D _frameHost;
	private Node3D _defaultHost;
	private float _frameHostSoiRadius;
	private Vector3 _prevFrameHostWorldPos;
	private Vector3 _frameHostWorldVelocity;

	private Node3D _stationAssistBody;
	private Vector3 _prevStationAssistPos;
	private float _stationAssistRadius;
	private float _stationAssistInfluence;

	private Node3D _autopilotTarget;
	private float _autopilotMaxSpeed;
	private float _autopilotArriveRadius;

	public void SetGravityWells(GravityWell[] wells) => _gravityWells = wells;
	public void SetStationAssistTargets(StationAssistTarget[] targets) => _stationAssistTargets = targets;
	public void SetStar(Node3D star) => _star = star;

	// Called by GameplayRoot after a jump-gate transit: drop motion state so
	// we arrive at the new system's gate at rest, facing whatever direction
	// the gate's basis points.
	public void ResetForArrival(Transform3D spawnTransform)
	{
		GlobalTransform = spawnTransform;
		Velocity = Vector3.Zero;
		_angularVelocity = Vector3.Zero;
		_mode = FlightMode.Normal;
		_spool = 0f;
		_currentSupercruiseSpeed = 0f;
		_autopilotTarget = null;
		AutopilotArrived = false;
		SteeringEnabled = true;

		_frameHost = _defaultHost;
		_frameHostSoiRadius = 0f;
		_prevFrameHostWorldPos = _defaultHost != null ? _defaultHost.GlobalPosition : Vector3.Zero;
		_frameHostWorldVelocity = Vector3.Zero;

		ResetPhysicsInterpolation();
	}

	public void ApplyWorldOffset(Vector3 offset)
	{
		_prevFrameHostWorldPos -= offset;
		_prevStationAssistPos -= offset;
		ResetPhysicsInterpolation();
	}

	public void EngageAutopilot(Node3D target, float maxSpeed, float arriveRadius)
	{
		_autopilotTarget = target;
		_autopilotMaxSpeed = maxSpeed;
		_autopilotArriveRadius = arriveRadius;
		AutopilotArrived = false;
		_engineKill = false;
		_spool = 0f;
		_currentSupercruiseSpeed = 0f;
		_angularVelocity = Vector3.Zero;
		_mode = FlightMode.Autopilot;
		SteeringEnabled = false;
	}

	public void DisengageAutopilot()
	{
		_autopilotTarget = null;
		AutopilotArrived = false;
		SteeringEnabled = true;
		_mode = FlightMode.Normal;
		Velocity = Vector3.Zero;
		_angularVelocity = Vector3.Zero;
	}

	public override void _Ready()
	{
		_dragCoeff = ThrustForce / (NormalMaxSpeed * NormalMaxSpeed);
		_defaultHost = GetParent() as Node3D;
		_frameHost = _defaultHost;
		if (_frameHost != null)
			_prevFrameHostWorldPos = _frameHost.GlobalPosition;
	}

	public override void _UnhandledInput(InputEvent @event)
	{
		if (@event is InputEventKey key && key.Pressed && !key.Echo)
		{
			switch (key.Keycode)
			{
				case Key.Z:
				case Key.X:
					_engineKill = !_engineKill;
					if (_engineKill) DropToNormal();
					break;
				case Key.Space:
					SteeringEnabled = !SteeringEnabled;
					break;
				case Key.J:
					OnJPressed();
					break;
				case Key.R:
					ActivatePhaseScrambler();
					break;
			}
		}
	}

	public override void _PhysicsProcess(double delta)
	{
		float dt = (float)delta;
		PassiveDriftActive = false;
		SupercruiseOfflineTimer = Mathf.Max(0f, SupercruiseOfflineTimer - dt);
		PhaseScramblerActiveTimer = Mathf.Max(0f, PhaseScramblerActiveTimer - dt);
		PhaseScramblerCooldownTimer = Mathf.Max(0f, PhaseScramblerCooldownTimer - dt);
		_interdictionSignalTimer = Mathf.Max(0f, _interdictionSignalTimer - dt);
		UpdateInterdictionState(dt);
		UpdateThrottle(dt);
		UpdateSpool(dt);
		UpdateFrameHost(dt);
		UpdateStationAssist(dt);
		UpdateSupercruiseLockout();
		SupercruiseSpeedLimit = ComputeSupercruiseSpeedLimit();

		// — Torque-based steering (momentum + damping) ———————————————
		float turnScale = ComputeTurnScale();

		if (SteeringEnabled)
		{
			Vector2 screenSize = GetViewport().GetVisibleRect().Size;
			Vector2 mousePos = GetViewport().GetMousePosition();
			Vector2 center = screenSize / 2f;
			Vector2 steer = (mousePos - center) / (Mathf.Min(center.X, center.Y));

			// Clamp to unit circle.
			if (steer.Length() > 1f) steer = steer.Normalized();
			if (Mathf.Abs(steer.X) < DeadZone) steer.X = 0f;
			if (Mathf.Abs(steer.Y) < DeadZone) steer.Y = 0f;

			// Apply steering as angular acceleration.
			_angularVelocity.Y -= steer.X * SteeringForce * turnScale * dt;
			_angularVelocity.X -= steer.Y * SteeringForce * turnScale * dt;
		}

		// Roll (Q/E).
		float roll = 0f;
		if (Input.IsKeyPressed(Key.Q)) roll -= 1f;
		if (Input.IsKeyPressed(Key.E)) roll += 1f;
		_angularVelocity.Z += roll * RollForce * turnScale * dt;

		// Angular drag — bleeds off spin gradually.
		_angularVelocity -= _angularVelocity * AngularDrag * dt;

		// Apply angular velocity as rotation.
		RotateObjectLocal(Vector3.Right, _angularVelocity.X * dt);
		RotateObjectLocal(Vector3.Up, _angularVelocity.Y * dt);
		RotateObjectLocal(Vector3.Forward, _angularVelocity.Z * dt);

		// — Flight mode processing ————————————————————————
		switch (_mode)
		{
			case FlightMode.Supercruise:
				ProcessSupercruise(dt);
				break;
			case FlightMode.SupercruiseSpool:
				ProcessSupercruiseSpool(dt);
				break;
			case FlightMode.Autopilot:
				ProcessAutopilot(dt);
				break;
			default:
				ProcessNormalFlight(dt);
				break;
		}
	}

	private void ProcessAutopilot(float dt)
	{
		if (_autopilotTarget == null || !IsInstanceValid(_autopilotTarget))
		{
			Velocity = Velocity.MoveToward(Vector3.Zero, AutopilotBrakeAccel * dt);
			TranslateByCurrentFrame(dt);
			return;
		}

		Vector3 targetPos = _autopilotTarget.GlobalPosition;
		Vector3 toTarget = targetPos - GlobalPosition;
		float distance = toTarget.Length();
		AutopilotDistance = distance;

		if (distance <= _autopilotArriveRadius)
		{
			AutopilotArrived = true;
			// Frame velocity keeps us moving with the current orbital reference while stopped.
			Velocity = Velocity.MoveToward(Vector3.Zero, AutopilotBrakeAccel * dt);
			TranslateByCurrentFrame(dt);
			return;
		}

		Vector3 dirToTarget = toTarget / Mathf.Max(distance, 0.0001f);

		// Rotate toward the live world-space target using Godot's normal -Z forward convention.
		Basis currentBasis = GlobalBasis.Orthonormalized();
		Vector3 up = Mathf.Abs(dirToTarget.Dot(Vector3.Up)) > 0.98f ? currentBasis.Y : Vector3.Up;
		Transform3D desiredTransform = GlobalTransform.LookingAt(GlobalPosition + dirToTarget, up);
		Quaternion currentQ = currentBasis.GetRotationQuaternion();
		Quaternion targetQ = desiredTransform.Basis.Orthonormalized().GetRotationQuaternion();
		float angle = currentQ.AngleTo(targetQ);
		float step = angle > 1e-4f ? Mathf.Min(1f, (AutopilotMaxTurnRate * dt) / angle) : 1f;
		GlobalBasis = new Basis(currentQ.Slerp(targetQ, step));

		// Kinematic brake: stops at arrive radius.
		float brakeDist = Mathf.Max(distance - _autopilotArriveRadius, 0f);
		float brakeSpeed = Mathf.Sqrt(2f * AutopilotBrakeAccel * brakeDist);
		float desiredSpeed = Mathf.Min(_autopilotMaxSpeed, brakeSpeed);

		// Keep translating toward the target even while rotation catches up so waypoint travel
		// feels reliable instead of stalling on alignment. We still ease the turn visually above.
		Vector3 desiredVelocity = dirToTarget * desiredSpeed;

		Velocity = Velocity.MoveToward(desiredVelocity, AutopilotAccel * dt);
		TranslateByCurrentFrame(dt);
	}

	private void ProcessNormalFlight(float dt)
	{
		var totalForce = Vector3.Zero;

		if (!_engineKill)
		{
			if (ThrottlePercent > 0.001f)
				totalForce += -GlobalTransform.Basis.Z * (ThrustForce * ThrottlePercent);
			if (Input.IsKeyPressed(Key.A))
				totalForce += -GlobalTransform.Basis.X * StrafeForce;
			if (Input.IsKeyPressed(Key.D))
				totalForce += GlobalTransform.Basis.X * StrafeForce;
			if (Input.IsKeyPressed(Key.Ctrl))
				totalForce += -GlobalTransform.Basis.Y * StrafeForce;
		}
		else if (_frameHost != null && _frameHost != _defaultHost)
		{
			Vector3 toBody = _frameHost.GlobalPosition - GlobalPosition;
			float dist = toBody.Length();
			if (dist > 0.01f && _frameHostSoiRadius > 0.01f)
			{
				float depth = Mathf.Clamp(1f - dist / _frameHostSoiRadius, 0f, 1f);
				float pull = Mathf.Lerp(PassiveGravityMinAccel, PassiveGravityMaxAccel, depth * depth);
				totalForce += toBody.Normalized() * pull;
				PassiveDriftActive = true;
			}
		}

		float speed = Velocity.Length();
		if (speed > 0.01f)
			totalForce += -Velocity.Normalized() * _dragCoeff * speed * speed;

		Velocity += totalForce * dt;
		if (!_engineKill && ThrottlePercent <= 0.001f && Velocity.LengthSquared() > 0.0001f)
			Velocity = Velocity.MoveToward(Vector3.Zero, ZeroThrottleBrakeAccel * dt);
		TranslateByCurrentFrame(dt);
	}

	private void ProcessSupercruiseSpool(float dt)
	{
		// Spool is a 5 s timer before warp engages — the ship keeps flying
		// like it's in Normal flight in the meantime (throttle, strafe, drag
		// all behave as usual). No forced brake.
		ProcessNormalFlight(dt);
	}

	private void ProcessSupercruise(float dt)
	{
		float accel = _currentSupercruiseSpeed < SupercruiseSpeedLimit
			? SupercruiseAccel : SupercruiseDecel;
		_currentSupercruiseSpeed = Mathf.MoveToward(
			_currentSupercruiseSpeed, SupercruiseSpeedLimit, accel * dt);

		Velocity = -GlobalTransform.Basis.Z * _currentSupercruiseSpeed;
		// Supercruise is system-space travel. Do not inherit nearby planet or
		// moon frame velocity while the phase drive is active; that turns a
		// flyby into an orbital slingshot when entering a body's SOI.
		Position += Velocity * dt;
	}

	private void TranslateByCurrentFrame(float dt)
	{
		Position += WorldVelocity * dt;
	}

	/// <summary>
	/// Picks the best frame host (nearest enclosing gravity-well SOI, or the
	/// default host in deep space). The host's sampled velocity is then added
	/// numerically to the player-relative flight velocity.
	/// </summary>
	private void UpdateFrameHost(float dt)
	{
		if (_gravityWells == null || _defaultHost == null)
			return;

		// Find the nearest body whose SOI contains us. Hysteresis: once a host
		// is active, its SOI is treated as slightly larger so we don't flicker
		// between frames at the boundary.
		Node3D bestCandidate = null;
		float bestDist = float.MaxValue;
		float bestSoiRadius = 0f;

		foreach (GravityWell well in _gravityWells)
		{
			if (!IsInstanceValid(well.Body)) continue;
			if (!well.IsGravitySource) continue;

			float dist = GlobalPosition.DistanceTo(well.Body.GlobalPosition);
			float soi = well.Radius * SoiMultiplier;
			float effectiveSoi = well.Body == _frameHost ? soi * FrameHostHysteresisFactor : soi;

			if (dist < effectiveSoi && dist < bestDist)
			{
				bestDist = dist;
				bestCandidate = well.Body;
				bestSoiRadius = soi;
			}
		}

		Node3D newHost = bestCandidate ?? _defaultHost;

		if (newHost != _frameHost)
			TransitionToFrameHost(newHost, bestSoiRadius);

		// Track host's world velocity so consumers (station assist) can express
		// external velocities in the current frame.
		if (_frameHost != null && dt > 0.0001f)
		{
			Vector3 currentHostPos = _frameHost.GlobalPosition;
			_frameHostWorldVelocity = (currentHostPos - _prevFrameHostWorldPos) / dt;
			_prevFrameHostWorldPos = currentHostPos;
		}
		else
		{
			_frameHostWorldVelocity = Vector3.Zero;
		}
	}

	private void TransitionToFrameHost(Node3D newHost, float newHostSoiRadius)
	{
		// Velocity is intentionally not converted: local flight speed remains
		// player-relative, while root-space movement gains the new frame's
		// sampled world velocity on following ticks.
		_frameHost = newHost;
		_frameHostSoiRadius = newHostSoiRadius;
		_prevFrameHostWorldPos = newHost != null ? newHost.GlobalPosition : Vector3.Zero;
		_frameHostWorldVelocity = Vector3.Zero;
	}

	private void UpdateStationAssist(float dt)
	{
		if (_stationAssistTargets == null || _stationAssistTargets.Length == 0)
			return;

		Node3D newAssistBody = null;
		float bestDist = float.MaxValue;
		float newAssistRadius = 0f;

		foreach (var target in _stationAssistTargets)
		{
			if (!IsInstanceValid(target.Body)) continue;
			float dist = GlobalPosition.DistanceTo(target.Body.GlobalPosition);
			if (dist < target.Radius && dist < bestDist)
			{
				bestDist = dist;
				newAssistBody = target.Body;
				newAssistRadius = target.Radius;
			}
		}

		if (newAssistBody != _stationAssistBody)
		{
			_stationAssistBody = newAssistBody;
			_stationAssistRadius = newAssistRadius;
			_stationAssistInfluence = 0f;
			if (_stationAssistBody != null)
				_prevStationAssistPos = _stationAssistBody.GlobalPosition;
			return;
		}

		if (_stationAssistBody == null)
		{
			_stationAssistInfluence = Mathf.MoveToward(_stationAssistInfluence, 0f, StationAssistBlendOutRate * dt);
			return;
		}

		_stationAssistRadius = newAssistRadius;
		Vector3 currentPos = _stationAssistBody.GlobalPosition;
		Vector3 stationDelta = currentPos - _prevStationAssistPos;
		_prevStationAssistPos = currentPos;

		if (stationDelta.LengthSquared() >= 10000f)
			return;

		// Station velocity in world, then expressed in the ship's frame by
		// subtracting the frame host's own world velocity (zero when the host
		// is the default deep-space host).
		Vector3 stationWorldVelocity = dt > 0.0001f ? stationDelta / dt : Vector3.Zero;
		Vector3 stationLocalVelocity = stationWorldVelocity - _frameHostWorldVelocity;
		float relativeSpeed = (Velocity - stationLocalVelocity).Length();
		float targetInfluence = ComputeStationAssistInfluence(relativeSpeed);
		float rate = targetInfluence >= _stationAssistInfluence ? StationAssistBlendInRate : StationAssistBlendOutRate;
		_stationAssistInfluence = Mathf.MoveToward(_stationAssistInfluence, targetInfluence, rate * dt);

		float assistAccel = StationAssistMatchAccel * Mathf.Max(_stationAssistInfluence, 0.05f);
		Velocity = Velocity.MoveToward(stationLocalVelocity, assistAccel * _stationAssistInfluence * dt);
	}

	private float ComputeStationAssistInfluence(float relativeSpeed)
	{
		if (_mode != FlightMode.Normal)
			return 0f;

		if (Input.IsKeyPressed(Key.A)
			|| Input.IsKeyPressed(Key.D)
			|| Input.IsKeyPressed(Key.Ctrl))
			return 0f;

		if (relativeSpeed >= StationAssistCutoffSpeed)
			return 0f;

		float throttleFactor = 1f - Mathf.Clamp(ThrottlePercent / Mathf.Max(StationAssistThrottleZeroPoint, 0.01f), 0f, 1f);
		float relativeSpeedFactor = 1f - 0.9f * Mathf.Clamp(relativeSpeed / Mathf.Max(StationAssistMaxRelativeSpeed, 0.01f), 0f, 1f);

		if (_engineKill)
			throttleFactor = 1f;

		return Mathf.Clamp(throttleFactor * relativeSpeedFactor, 0f, 1f);
	}

	/// <summary>
	/// Phase Compression Drive speed limit. Two layers:
	///  1. Star distance sets the ceiling — further from the star,
	///     the compression field is more efficient (less gravitational drag).
	///  2. Planet proximity pulls you down within that ceiling —
	///     nearby gravity wells disrupt the compression field.
	/// </summary>
	private float ComputeSupercruiseSpeedLimit()
	{
		// Layer 1: star distance ceiling.
		// The drive gets more efficient the further from the star's gravity.
		float ceiling = SupercruiseDeepSpaceMax;
		if (_star != null && IsInstanceValid(_star))
		{
			float starDist = GlobalPosition.DistanceTo(_star.GlobalPosition);
			float t = Mathf.Clamp(starDist / SupercruiseScaleDistance, 0f, 1f);
			ceiling = Mathf.Lerp(SupercruiseNearStarMax, SupercruiseDeepSpaceMax, Mathf.Sqrt(t));
		}

		// Layer 2: planet gravity wells pull speed down within the ceiling.
		float lowest = ceiling;

		if (_gravityWells != null)
		{
			foreach (var well in _gravityWells)
			{
				if (!IsInstanceValid(well.Body)) continue;
				float dist = GlobalPosition.DistanceTo(well.Body.GlobalPosition);
				float surfaceDist = Mathf.Max(0f, dist - well.Radius);
				float rampDist = well.Radius * WellRadiusMultiplier;
				float ramp = Mathf.Clamp(surfaceDist / rampDist, 0f, 1f);
				// Ramp from cruise speed up to the current ceiling.
				float limit = NormalMaxSpeed + (ceiling - NormalMaxSpeed) * Mathf.Sqrt(ramp);
				lowest = Mathf.Min(lowest, limit);
			}
		}

		return lowest;
	}

	private float ComputeTurnScale()
	{
		return _mode switch
		{
			FlightMode.Supercruise =>
				Mathf.Max(0.01f, NormalMaxSpeed / Mathf.Max(1f, _currentSupercruiseSpeed) * CruiseTurnFactor),
			FlightMode.SupercruiseSpool =>
				CruiseTurnFactor,
			_ => 1f,
		};
	}

	private void OnJPressed()
	{
		if (_engineKill) return;
		if (SupercruiseDriveOffline) return;
		if (InsideSupercruiseLockout && _mode == FlightMode.Normal)
			return;

		switch (_mode)
		{
			case FlightMode.Normal:
				_mode = FlightMode.SupercruiseSpool;
				_spool = 0f;
				_currentSupercruiseSpeed = SupercruiseEntrySpeed;
				break;
			case FlightMode.SupercruiseSpool:
			case FlightMode.Supercruise:
				DropToNormal();
				break;
		}
	}

	/// <summary>
	/// Check if the ship is inside any body's supercruise-lockout SOI.
	/// If so, force dropout from supercruise to normal flight.
	/// </summary>
	private void UpdateSupercruiseLockout()
	{
		InsideSupercruiseLockout = false;
		if (_gravityWells == null) return;

		foreach (var well in _gravityWells)
		{
			if (!IsInstanceValid(well.Body)) continue;
			float dist = GlobalPosition.DistanceTo(well.Body.GlobalPosition);
			if (dist < well.Radius * SupercruiseSoiMultiplier)
			{
				InsideSupercruiseLockout = true;
				break;
			}
		}

		// Force dropout if currently in supercruise inside a lockout zone.
		if (InsideSupercruiseLockout && (_mode == FlightMode.Supercruise || _mode == FlightMode.SupercruiseSpool))
		{
			DropToNormal();
		}
	}

	private void DropToNormal()
	{
		_mode = FlightMode.Normal;
		_spool = 0f;
		_currentSupercruiseSpeed = 0f;
		if (Speed > NormalMaxSpeed)
			Velocity = Velocity.Normalized() * NormalMaxSpeed;
	}

	public bool ApplySupercruiseInterdiction(float offlineSeconds)
	{
		float duration = offlineSeconds > 0.01f ? offlineSeconds : SupercruiseRecoverySeconds;
		bool wasOnline = !SupercruiseDriveOffline;
		bool wasInTransit = IsInSupercruiseTransit;
		SupercruiseOfflineTimer = Mathf.Max(SupercruiseOfflineTimer, duration);
		InterdictionLockStrength = 0f;
		InterdictionCollapseDistance = 0f;
		_interdictionSignalTimer = 0f;
		_pendingInterdictionOfflineSeconds = 0f;
		if (wasInTransit)
			DropToNormal();
		return wasOnline || wasInTransit;
	}

	public void ReceiveInterdictionLock(
		float lockDelta,
		float collapseEnvelopeDistance,
		float distanceToSource,
		string sourceName,
		float offlineSeconds = 0f)
	{
		if (lockDelta <= 0f || collapseEnvelopeDistance <= 0f)
			return;

		_interdictionSignalTimer = Mathf.Max(_interdictionSignalTimer, InterdictionLockHoldSeconds);
		_interdictionSourceName = sourceName ?? "";
		_interdictionCollapseEnvelope = collapseEnvelopeDistance;
		_pendingInterdictionOfflineSeconds = Mathf.Max(_pendingInterdictionOfflineSeconds, offlineSeconds);
		InterdictionCollapseDistance = Mathf.Max(0f, distanceToSource - collapseEnvelopeDistance);

		float appliedDelta = PhaseScramblerActive ? lockDelta * 0.15f : lockDelta;
		InterdictionLockStrength = Mathf.Clamp(InterdictionLockStrength + appliedDelta, 0f, 1f);
	}

	public bool IsInsideInterdictionCollapseEnvelope(float distanceToSource)
	{
		return _interdictionCollapseEnvelope > 0f && distanceToSource <= _interdictionCollapseEnvelope;
	}

	public bool ActivatePhaseScrambler()
	{
		if (!PhaseScramblerReady)
			return false;

		PhaseScramblerActiveTimer = PhaseScramblerDurationSeconds;
		PhaseScramblerCooldownTimer = PhaseScramblerCooldownSeconds;
		InterdictionLockStrength = Mathf.Max(0f, InterdictionLockStrength - 0.4f);
		return true;
	}

	private void UpdateInterdictionState(float dt)
	{
		float decayRate = PhaseScramblerActive ? PhaseScramblerLockDecayPerSecond : InterdictionLockDecayPerSecond;

		if (!IsInSupercruiseTransit)
			decayRate = Mathf.Max(decayRate, 1.8f);

		if (IsInSupercruiseTransit
			&& InterdictionLockStrength >= 0.999f
			&& InterdictionCollapseDistance <= 0.001f
			&& _pendingInterdictionOfflineSeconds > 0f)
		{
			ApplySupercruiseInterdiction(_pendingInterdictionOfflineSeconds);
			return;
		}

		if (_interdictionSignalTimer <= 0f || PhaseScramblerActive)
			InterdictionLockStrength = Mathf.Max(0f, InterdictionLockStrength - decayRate * dt);

		if (_interdictionSignalTimer <= 0f)
		{
			InterdictionCollapseDistance = 0f;
			_interdictionCollapseEnvelope = -1f;
			_pendingInterdictionOfflineSeconds = 0f;
			if (InterdictionLockStrength <= 0.001f)
				_interdictionSourceName = "";
		}
	}

	private void UpdateSpool(float dt)
	{
		switch (_mode)
		{
			case FlightMode.SupercruiseSpool:
				_spool += dt / SupercruiseSpoolTime;
				if (_spool >= 1f)
				{
					_spool = 1f;
					_mode = FlightMode.Supercruise;
					_currentSupercruiseSpeed = Mathf.Max(SupercruiseEntrySpeed * 3f, NormalMaxSpeed);
				}
				break;
		}
	}

	private void UpdateThrottle(float dt)
	{
		if (_engineKill) return;

		// Throttle controls work in both Normal and SupercruiseSpool — the
		// spool is a background timer, not a flight-state lockout.
		if (_mode != FlightMode.Normal && _mode != FlightMode.SupercruiseSpool) return;

		if (Input.IsKeyPressed(Key.W))
		{
			ThrottlePercent = Mathf.MoveToward(ThrottlePercent, 1f, ThrottleRiseRate * dt);
		}
		else if (Input.IsKeyPressed(Key.S))
		{
			ThrottlePercent = Mathf.MoveToward(ThrottlePercent, 0f, ThrottleFallRate * dt);
		}
	}
}
