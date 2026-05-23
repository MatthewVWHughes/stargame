using Godot;
using System;
using Starlight.Game.Runtime;
using Starlight.Ship;

namespace Starlight.Game.Combat;

/// <summary>
/// Lightweight hostile ship with constrained dogfighting locomotion.
/// </summary>
public partial class HostileEncounterController : Node3D, ISpaceCombatant
{
	private enum AiState
	{
		Patrol,
		Intercept,
		Interdict,
		Engage,
		Disengage,
		ReturnToAnchor,
	}

	[Export] public float DetectionRange { get; set; } = 1500f;
	[Export] public float FireRange { get; set; } = 560f;
	[Export] public float PreferredRangeMin { get; set; } = 260f;
	[Export] public float PreferredRangeMax { get; set; } = 440f;
	[Export] public float BreakoffRange { get; set; } = 170f;
	[Export] public float ReengageRange { get; set; } = 520f;
	[Export] public float LeashRange { get; set; } = 1700f;
	[Export] public float PatrolSpeed { get; set; } = 60f;
	[Export] public float EngageSpeed { get; set; } = 118f;
	[Export] public float DisengageSpeed { get; set; } = 140f;
	[Export] public float ReturnSpeed { get; set; } = 100f;
	[Export] public float ThrustForce { get; set; } = 90f;
	[Export] public float BrakeForce { get; set; } = 80f;
	[Export] public float SteeringForce { get; set; } = 7.5f;
	[Export] public float AngularDrag { get; set; } = 4.8f;
	[Export] public float MaxTurnRate { get; set; } = 1.7f;
	[Export] public float MaxHull { get; set; } = 90f;
	[Export] public float MaxShields { get; set; } = 55f;
	[Export] public float ShieldRegenPerSecond { get; set; } = 6f;
	[Export] public float ShieldRegenDelaySeconds { get; set; } = 2.5f;
	[Export] public float CollisionRadius { get; set; } = 7f;
	[Export] public float WeaponCooldownSeconds { get; set; } = 0.35f;
	[Export] public float WeaponDamage { get; set; } = 12f;
	[Export] public float ProjectileSpeed { get; set; } = 700f;
	[Export] public float ProjectileMaxDistance { get; set; } = 900f;
	[Export] public float WeaponEnergyCost { get; set; } = 14f;
	[Export] public float MaxWeaponEnergy { get; set; } = 100f;
	[Export] public float WeaponEnergyRegenPerSecond { get; set; } = 28f;
	[Export] public float InterdictionAcquireRange { get; set; } = 3000f;
	[Export] public float InterdictionCollapseRange { get; set; } = 850f;
	[Export] public float InterdictionOfflineSeconds { get; set; } = 30f;
	[Export] public float InterdictionLockBuildPerSecond { get; set; } = 1.9f;
	[Export] public float InterdictionAngularTolerance { get; set; } = 2.4f;
	[Export] public float InterdictionEnergyPerSecond { get; set; } = 15f;
	[Export] public float InterdictionChaseSpeed { get; set; } = 180f;

	public event Action<string> StatusChanged;
	public event Action Destroyed;
	public event Action<ProjectileFireRequest> ProjectileFired;

	public string CombatDisplayName => DisplayName;
	public CombatFaction Faction => CombatFaction.Hostile;
	public Vector3 CombatOrigin => GlobalPosition;
	public Vector3 CombatVelocity => Velocity;
	public bool IsAlive => Visible && Hull > 0f;

	public string DisplayName { get; set; } = "Hostile Contact";
	public float Hull { get; private set; }
	public float Shields { get; private set; }
	public float WeaponEnergy { get; private set; }
	public Vector3 AnchorPosition { get; private set; }
	public Vector3 Velocity { get; private set; }

	private ShipController _player;
	private ShipWeaponRig _weaponRig;
	private AiState _state = AiState.Patrol;
	private float _weaponCooldown;
	private float _patrolAngle;
	private float _strafeTimer;
	private float _strafeDirection = 1f;
	private float _shieldRegenDelayTimer;
	private float _badAngleTimer;
	private float _interdictLostTimer;
	private Vector3 _angularVelocity;
	private Vector3 _disengagePoint;

	public override void _Ready()
	{
		BuildVisual();
		ResetStats();
	}

	public void SetPlayer(ShipController player)
	{
		_player = player;
	}

	public void SpawnAt(Vector3 worldPosition, Vector3 anchorPosition)
	{
		GlobalPosition = worldPosition;
		AnchorPosition = anchorPosition;
		Basis = Basis.Identity;
		ResetStats();
		Visible = true;
		SetPhysicsProcess(true);
		ResetPhysicsInterpolation();
		StatusChanged?.Invoke($"{DisplayName} on patrol near the beacon.");
	}

	public void ApplyWorldOffset(Vector3 offset)
	{
		Position -= offset;
		AnchorPosition -= offset;
		_disengagePoint -= offset;
	}

	public override void _PhysicsProcess(double delta)
	{
		if (_player == null || !Visible || Hull <= 0f)
			return;

		float dt = (float)delta;
		_weaponCooldown = Mathf.Max(0f, _weaponCooldown - dt);
		_shieldRegenDelayTimer = Mathf.Max(0f, _shieldRegenDelayTimer - dt);
		_strafeTimer = Mathf.Max(0f, _strafeTimer - dt);
		WeaponEnergy = Mathf.Min(MaxWeaponEnergy, WeaponEnergy + WeaponEnergyRegenPerSecond * dt);

		if (_shieldRegenDelayTimer <= 0f && Shields < MaxShields)
			Shields = Mathf.Min(MaxShields, Shields + ShieldRegenPerSecond * dt);

		Vector3 toPlayer = _player.GlobalPosition - GlobalPosition;
		float distanceToPlayer = toPlayer.Length();
		float distanceToAnchor = GlobalPosition.DistanceTo(AnchorPosition);
		Vector3 forward = -GlobalTransform.Basis.Z;
		float angleQuality = distanceToPlayer > 0.001f ? forward.Dot(toPlayer / distanceToPlayer) : 1f;

		UpdateState(distanceToPlayer, distanceToAnchor, angleQuality);

		Vector3 aimPoint = GetAimPoint();
		Vector3 lockAimPoint = (_state == AiState.Intercept || _state == AiState.Interdict) && IsPlayerInTransit()
			? _player.GlobalPosition
			: aimPoint;
		_weaponRig?.AimAt(lockAimPoint, dt);

		Vector3 desiredDirection = ComputeDesiredFlightDirection(toPlayer, distanceToPlayer, distanceToAnchor);
		ApplySteering(desiredDirection, dt);
		ApplyFlight(desiredDirection, dt);
		TryCollapseInterdictedPlayer(distanceToPlayer);
		if (!ApplyInterdictionLock(distanceToPlayer, angleQuality, dt))
			TryFireAtPlayer(aimPoint, distanceToPlayer, angleQuality);
	}

	public void ApplyCombatDamage(float amount, Node3D source)
	{
		if (Hull <= 0f)
			return;

		float remaining = amount;
		if (Shields > 0f)
		{
			float shieldLoss = Mathf.Min(Shields, remaining);
			Shields -= shieldLoss;
			remaining -= shieldLoss;
		}

		if (remaining > 0f)
			Hull = Mathf.Max(0f, Hull - remaining);

		_shieldRegenDelayTimer = ShieldRegenDelaySeconds;
		_state = AiState.Engage;
		StatusChanged?.Invoke($"{DisplayName} hit. Shields {Shields:F0}/{MaxShields:F0}  Hull {Hull:F0}/{MaxHull:F0}");

		if (Hull > 0f)
			return;

		Visible = false;
		SetPhysicsProcess(false);
		StatusChanged?.Invoke($"{DisplayName} destroyed.");
		Destroyed?.Invoke();
		MessageBus.Publish(new GameEvents.HostileDestroyed(DisplayName, GlobalPosition));
	}

	private void ResetStats()
	{
		Hull = MaxHull;
		Shields = MaxShields;
		WeaponEnergy = MaxWeaponEnergy;
		Velocity = Vector3.Zero;
		_angularVelocity = Vector3.Zero;
		_weaponCooldown = 0f;
		_shieldRegenDelayTimer = 0f;
		_badAngleTimer = 0f;
		_interdictLostTimer = 0f;
		_patrolAngle = GD.Randf() * Mathf.Tau;
		_strafeTimer = 0f;
		_strafeDirection = GD.Randf() < 0.5f ? -1f : 1f;
		_state = AiState.Patrol;
		_disengagePoint = AnchorPosition;
	}

	private void UpdateState(float distanceToPlayer, float distanceToAnchor, float angleQuality)
	{
		switch (_state)
		{
			case AiState.Patrol:
				if (ShouldBeginInterception(distanceToPlayer))
				{
					_state = AiState.Intercept;
					StatusChanged?.Invoke($"{DisplayName} is moving to intercept the phase trail.");
				}
				else if (distanceToPlayer <= DetectionRange)
				{
					_state = AiState.Engage;
					StatusChanged?.Invoke($"{DisplayName} detected the player and is engaging.");
				}
				break;

			case AiState.Intercept:
				if (distanceToAnchor >= LeashRange)
				{
					_state = AiState.ReturnToAnchor;
					StatusChanged?.Invoke($"{DisplayName} is regrouping at the beacon.");
					break;
				}

				if (!IsPlayerInTransit())
				{
					_state = distanceToPlayer <= DetectionRange ? AiState.Engage : AiState.Patrol;
					StatusChanged?.Invoke($"{DisplayName} is switching from intercept to combat pursuit.");
					break;
				}

				if (distanceToPlayer <= InterdictionAcquireRange * 0.96f && angleQuality >= 0.38f)
				{
					_state = AiState.Interdict;
					_interdictLostTimer = 0f;
					StatusChanged?.Invoke($"{DisplayName} is locking the phase shell.");
				}
				break;

			case AiState.Interdict:
				if (distanceToAnchor >= LeashRange)
				{
					_state = AiState.ReturnToAnchor;
					StatusChanged?.Invoke($"{DisplayName} is regrouping at the beacon.");
					break;
				}

				if (!IsPlayerInTransit())
				{
					_state = AiState.Engage;
					StatusChanged?.Invoke($"{DisplayName} has the player in normal space and is closing to attack.");
					break;
				}

				if (distanceToPlayer > InterdictionAcquireRange * 1.12f)
				{
					_state = AiState.Intercept;
					_interdictLostTimer = 0f;
					StatusChanged?.Invoke($"{DisplayName} lost the beam and is re-acquiring.");
					break;
				}

				if (angleQuality < 0.3f)
					_interdictLostTimer += (float)GetPhysicsProcessDeltaTime();
				else
					_interdictLostTimer = Mathf.Max(0f, _interdictLostTimer - (float)GetPhysicsProcessDeltaTime());

				if (_interdictLostTimer >= 0.8f)
				{
					_state = AiState.Intercept;
					_interdictLostTimer = 0f;
					StatusChanged?.Invoke($"{DisplayName} lost lock geometry and is resetting the intercept.");
				}
				break;

			case AiState.Engage:
				if (ShouldBeginInterception(distanceToPlayer))
				{
					_state = AiState.Intercept;
					StatusChanged?.Invoke($"{DisplayName} is abandoning the gun run to interdict the transit target.");
					break;
				}

				if (distanceToAnchor >= LeashRange)
				{
					_state = AiState.ReturnToAnchor;
					StatusChanged?.Invoke($"{DisplayName} is regrouping at the beacon.");
					break;
				}

				if (ShouldPressInterdiction(distanceToPlayer))
				{
					_badAngleTimer = Mathf.Max(0f, _badAngleTimer - (float)GetPhysicsProcessDeltaTime());
					break;
				}

				if (distanceToPlayer <= BreakoffRange || angleQuality < 0.45f)
					_badAngleTimer += (float)GetPhysicsProcessDeltaTime();
				else
					_badAngleTimer = Mathf.Max(0f, _badAngleTimer - (float)GetPhysicsProcessDeltaTime() * 0.5f);

				if (_badAngleTimer >= 0.55f)
				{
					Vector3 away = (GlobalPosition - _player.GlobalPosition).Normalized();
					if (away.LengthSquared() <= 0.001f)
						away = GlobalTransform.Basis.Z;
					Vector3 lateral = away.Cross(Vector3.Up).Normalized();
					_disengagePoint = GlobalPosition + away * 420f + lateral * (_strafeDirection * 180f);
					_state = AiState.Disengage;
					_badAngleTimer = 0f;
					StatusChanged?.Invoke($"{DisplayName} is breaking off to reset the attack run.");
				}
				break;

			case AiState.Disengage:
				if (distanceToPlayer >= ReengageRange && angleQuality > -0.2f)
				{
					_state = AiState.Engage;
					StatusChanged?.Invoke($"{DisplayName} is turning back in for another pass.");
				}
				else if (distanceToAnchor >= LeashRange)
				{
					_state = AiState.ReturnToAnchor;
				}
				break;

			case AiState.ReturnToAnchor:
				if (distanceToAnchor <= 120f)
					_state = ShouldBeginInterception(distanceToPlayer)
						? AiState.Intercept
						: (distanceToPlayer <= DetectionRange ? AiState.Engage : AiState.Patrol);
				break;
		}
	}

	private Vector3 GetAimPoint()
	{
		if (_player == null)
			return GlobalPosition + (-GlobalTransform.Basis.Z * 100f);

		if (!SpaceCombatMath.TryGetInterceptPoint(
				GlobalPosition,
				Velocity,
				_player.GlobalPosition,
				_player.WorldVelocity,
				ProjectileSpeed,
				out Vector3 intercept))
		{
			intercept = _player.GlobalPosition;
		}

		return intercept;
	}

	private Vector3 ComputeDesiredFlightDirection(Vector3 toPlayer, float distanceToPlayer, float distanceToAnchor)
	{
		switch (_state)
		{
			case AiState.Patrol:
				_patrolAngle += (float)GetPhysicsProcessDeltaTime() * 0.5f;
				Vector3 patrolOffset = new(Mathf.Cos(_patrolAngle), 0f, Mathf.Sin(_patrolAngle));
				return (AnchorPosition + patrolOffset * 180f - GlobalPosition).Normalized();

			case AiState.ReturnToAnchor:
				return (AnchorPosition - GlobalPosition).Normalized();

			case AiState.Disengage:
				return (_disengagePoint - GlobalPosition).Normalized();

			case AiState.Intercept:
				Vector3 interceptTarget = GetInterceptFlightPoint();
				Vector3 interceptDirection = interceptTarget - GlobalPosition;
				return interceptDirection.LengthSquared() > 0.001f
					? interceptDirection.Normalized()
					: (distanceToPlayer > 0.001f ? toPlayer / distanceToPlayer : -GlobalTransform.Basis.Z);

			case AiState.Interdict:
				Vector3 holdDirection = distanceToPlayer > 0.001f ? toPlayer / distanceToPlayer : -GlobalTransform.Basis.Z;
				Vector3 playerWorldVelocity = _player.WorldVelocity;
				Vector3 velocityBias = playerWorldVelocity.LengthSquared() > 0.001f
					? playerWorldVelocity.Normalized() * 0.18f
					: Vector3.Zero;
				Vector3 anchorBias = distanceToAnchor > LeashRange * 0.78f
					? (AnchorPosition - GlobalPosition).Normalized() * 0.45f
					: Vector3.Zero;
				Vector3 interdictDirection = holdDirection + velocityBias + anchorBias;
				return interdictDirection.LengthSquared() > 0.001f ? interdictDirection.Normalized() : holdDirection;

			case AiState.Engage:
				if (ShouldPressInterdiction(distanceToPlayer))
					return distanceToPlayer > 0.001f ? toPlayer / distanceToPlayer : -GlobalTransform.Basis.Z;

				if (_strafeTimer <= 0f)
				{
					_strafeTimer = 1.4f + GD.Randf() * 1.4f;
					_strafeDirection = GD.Randf() < 0.5f ? -1f : 1f;
				}

				Vector3 forwardToTarget = distanceToPlayer > 0.001f ? toPlayer / distanceToPlayer : -GlobalTransform.Basis.Z;
				Vector3 lateral = forwardToTarget.Cross(Vector3.Up);
				if (lateral.LengthSquared() <= 0.001f)
					lateral = GlobalTransform.Basis.X;

				Vector3 desired = lateral.Normalized() * _strafeDirection * 0.38f;
				if (distanceToPlayer > PreferredRangeMax)
					desired += forwardToTarget;
				else if (distanceToPlayer < PreferredRangeMin)
					desired -= forwardToTarget * 0.7f;
				else
					desired += forwardToTarget * 0.15f;

				if (distanceToAnchor > LeashRange * 0.78f)
					desired += (AnchorPosition - GlobalPosition).Normalized() * 0.65f;

				return desired.Normalized();

			default:
				return -GlobalTransform.Basis.Z;
		}
	}

	private void ApplySteering(Vector3 desiredDirection, float dt)
	{
		if (desiredDirection.LengthSquared() <= 0.001f)
			return;

		Basis basis = GlobalBasis.Orthonormalized();
		Vector3 localDesired = basis.Inverse() * desiredDirection.Normalized();
		Vector2 steer = new(localDesired.X, -localDesired.Y);

		_angularVelocity.Y -= steer.X * SteeringForce * dt;
		_angularVelocity.X -= steer.Y * SteeringForce * dt;
		_angularVelocity -= _angularVelocity * AngularDrag * dt;
		_angularVelocity.X = Mathf.Clamp(_angularVelocity.X, -MaxTurnRate, MaxTurnRate);
		_angularVelocity.Y = Mathf.Clamp(_angularVelocity.Y, -MaxTurnRate, MaxTurnRate);

		RotateObjectLocal(Vector3.Right, _angularVelocity.X * dt);
		RotateObjectLocal(Vector3.Up, _angularVelocity.Y * dt);
	}

	private void ApplyFlight(Vector3 desiredDirection, float dt)
	{
		Vector3 forward = -GlobalTransform.Basis.Z;
		float alignment = desiredDirection.LengthSquared() > 0.001f
			? forward.Dot(desiredDirection.Normalized())
			: 1f;
		float desiredSpeed = GetDesiredSpeed();
		float speed = Velocity.Length();

		Vector3 accel = Vector3.Zero;
		if (speed < desiredSpeed || alignment > 0.8f)
		{
			float thrustFactor = Mathf.Clamp((alignment + 1f) * 0.5f, 0.15f, 1f);
			accel += forward * (ThrustForce * thrustFactor);
		}
		else if (speed > desiredSpeed)
		{
			accel += -Velocity.Normalized() * BrakeForce;
		}

		Vector3 drag = Velocity.LengthSquared() > 0.0001f
			? -Velocity.Normalized() * (ThrustForce / Mathf.Max(EngageSpeed * EngageSpeed, 1f)) * Velocity.LengthSquared()
			: Vector3.Zero;

		Velocity += (accel + drag) * dt;
		float maxSpeed = Mathf.Max(PatrolSpeed, Mathf.Max(EngageSpeed, Mathf.Max(DisengageSpeed, ReturnSpeed)));
		if (Velocity.Length() > maxSpeed)
			Velocity = Velocity.Normalized() * maxSpeed;

		Position += Velocity * dt;
	}

	private float GetDesiredSpeed()
	{
		return _state switch
		{
			AiState.Patrol => PatrolSpeed,
			AiState.Intercept => InterdictionChaseSpeed,
			AiState.Interdict => Mathf.Lerp(EngageSpeed, InterdictionChaseSpeed, 0.55f),
			AiState.Engage => EngageSpeed,
			AiState.Disengage => DisengageSpeed,
			AiState.ReturnToAnchor => ReturnSpeed,
			_ => EngageSpeed,
		};
	}

	private void TryFireAtPlayer(Vector3 aimPoint, float distanceToPlayer, float angleQuality)
	{
		if (_state != AiState.Engage || _weaponCooldown > 0f || distanceToPlayer > FireRange || _weaponRig == null)
			return;
		if (WeaponEnergy + 0.001f < WeaponEnergyCost)
			return;
		if (angleQuality < 0.93f)
			return;

		Transform3D muzzle = _weaponRig.GetNextMuzzleTransform();
		Vector3 shotDirection = (aimPoint - muzzle.Origin).Normalized();
		if (shotDirection.LengthSquared() <= 0.001f)
			return;

		Vector3 facingDirection = -muzzle.Basis.Z;
		if (facingDirection.Dot(shotDirection) < 0.96f)
			return;

		WeaponEnergy -= WeaponEnergyCost;
		ProjectileFired?.Invoke(new ProjectileFireRequest(
			muzzle,
			shotDirection,
			CombatFaction.Hostile,
			WeaponDamage,
			ProjectileSpeed,
			ProjectileMaxDistance,
			Velocity,
			new Color(1f, 0.35f, 0.2f)));
		_weaponCooldown = WeaponCooldownSeconds;
	}

	private bool ApplyInterdictionLock(float distanceToPlayer, float angleQuality, float dt)
	{
		if (_state != AiState.Interdict || !ShouldAttemptInterdiction(distanceToPlayer) || _weaponRig == null)
			return false;
		if (WeaponEnergy + 0.001f < InterdictionEnergyPerSecond * dt)
			return false;
		if (angleQuality < 0.68f)
			return false;

		Vector3 toPlayer = (_player.GlobalPosition - GlobalPosition).Normalized();
		Vector3 relativeVelocity = _player.WorldVelocity - Velocity;
		Vector3 lateralVelocity = relativeVelocity - toPlayer * relativeVelocity.Dot(toPlayer);
		float angularCrossingRate = lateralVelocity.Length() / Mathf.Max(distanceToPlayer, 1f);
		float alignmentFactor = Mathf.Clamp((angleQuality - 0.68f) / 0.24f, 0f, 1f);
		float rangeFactor = Mathf.Clamp(1f - ((distanceToPlayer - InterdictionCollapseRange) / Mathf.Max(InterdictionAcquireRange - InterdictionCollapseRange, 1f)), 0.5f, 1f);
		float crossingFactor = Mathf.Clamp(1f - (angularCrossingRate / Mathf.Max(InterdictionAngularTolerance, 0.01f)), 0f, 1f);
		float lockStrength = alignmentFactor * rangeFactor * crossingFactor;
		if (lockStrength <= 0.001f)
			return false;

		WeaponEnergy -= InterdictionEnergyPerSecond * dt;
		_player.ReceiveInterdictionLock(
			InterdictionLockBuildPerSecond * Mathf.Max(lockStrength, 0.22f) * dt,
			InterdictionCollapseRange,
			distanceToPlayer,
			DisplayName,
			InterdictionOfflineSeconds);

		return true;
	}

	private void TryCollapseInterdictedPlayer(float distanceToPlayer)
	{
		if (_player == null || !_player.IsInSupercruiseTransit)
			return;
		if (_state != AiState.Interdict && _state != AiState.Intercept)
			return;
		if (_player.InterdictionLockStrength < 0.999f)
			return;
		if (!_player.IsInsideInterdictionCollapseEnvelope(distanceToPlayer))
			return;

		if (_player.ApplySupercruiseInterdiction(InterdictionOfflineSeconds))
			StatusChanged?.Invoke($"{DisplayName} collapsed the player's phase shell.");
	}

	private bool ShouldAttemptInterdiction(float distanceToPlayer)
	{
		return _player != null
			&& distanceToPlayer <= InterdictionAcquireRange
			&& (_player.Mode == ShipController.FlightMode.Supercruise || _player.Mode == ShipController.FlightMode.SupercruiseSpool);
	}

	private bool ShouldPressInterdiction(float distanceToPlayer)
	{
		return _player != null
			&& (_player.Mode == ShipController.FlightMode.Supercruise
				|| _player.Mode == ShipController.FlightMode.SupercruiseSpool
				|| (_player.InterdictionLockDetected && distanceToPlayer <= InterdictionAcquireRange)
				|| (_player.SupercruiseDriveOffline && distanceToPlayer <= InterdictionCollapseRange * 1.8f));
	}

	private float GetEngagementRange()
	{
		if (_player == null)
			return DetectionRange;

		return (_player.Mode == ShipController.FlightMode.Supercruise || _player.Mode == ShipController.FlightMode.SupercruiseSpool)
			? Mathf.Max(DetectionRange, InterdictionAcquireRange)
			: DetectionRange;
	}

	private bool ShouldBeginInterception(float distanceToPlayer)
	{
		return IsPlayerInTransit() && distanceToPlayer <= GetEngagementRange();
	}

	private bool IsPlayerInTransit()
	{
		return _player != null
			&& (_player.Mode == ShipController.FlightMode.Supercruise || _player.Mode == ShipController.FlightMode.SupercruiseSpool);
	}

	private Vector3 GetInterceptFlightPoint()
	{
		if (_player == null)
			return GlobalPosition + (-GlobalTransform.Basis.Z * 100f);

		Vector3 toPlayer = _player.GlobalPosition - GlobalPosition;
		float distance = toPlayer.Length();
		if (distance <= 0.001f)
			return _player.GlobalPosition;

		float leadTime = Mathf.Clamp(distance / Mathf.Max(InterdictionChaseSpeed, 1f), 0.5f, 1.9f);
		return _player.GlobalPosition + _player.WorldVelocity * leadTime;
	}

	private void BuildVisual()
	{
		if (GetNodeOrNull<Node3D>("Hull") != null)
		{
			_weaponRig = GetNodeOrNull<ShipWeaponRig>("WeaponRig");
			return;
		}

		var hull = new MeshInstance3D
		{
			Name = "Hull",
			Mesh = new BoxMesh { Size = new Vector3(5.2f, 1.7f, 9.2f) },
			MaterialOverride = new StandardMaterial3D { AlbedoColor = new Color(0.88f, 0.24f, 0.24f) },
		};
		AddChild(hull);

		var canopy = new MeshInstance3D
		{
			Name = "Canopy",
			Position = new Vector3(0f, 0.55f, -0.8f),
			Mesh = new BoxMesh { Size = new Vector3(1.9f, 0.7f, 2.2f) },
			MaterialOverride = new StandardMaterial3D { AlbedoColor = new Color(0.18f, 0.24f, 0.3f) },
		};
		AddChild(canopy);

		var engineGlow = new MeshInstance3D
		{
			Name = "EngineGlow",
			Position = new Vector3(0f, 0f, 5f),
			Mesh = new BoxMesh { Size = new Vector3(1.2f, 0.35f, 0.35f) },
			MaterialOverride = new StandardMaterial3D
			{
				AlbedoColor = new Color(1f, 0.48f, 0.14f),
				EmissionEnabled = true,
				Emission = new Color(1f, 0.48f, 0.14f),
				EmissionEnergyMultiplier = 1.6f,
			},
		};
		AddChild(engineGlow);

		_weaponRig = new ShipWeaponRig
		{
			Name = "WeaponRig",
			Position = new Vector3(0f, 0.08f, -1.25f),
		};
		CreateBarrel(_weaponRig, "LeftBarrel", new Vector3(-0.6f, 0f, 0f));
		CreateBarrel(_weaponRig, "RightBarrel", new Vector3(0.6f, 0f, 0f));
		AddChild(_weaponRig);
	}

	private static void CreateBarrel(Node3D parent, string name, Vector3 localPosition)
	{
		var mount = new Node3D
		{
			Name = name,
			Position = localPosition,
		};
		parent.AddChild(mount);

		var barrel = new MeshInstance3D
		{
			Name = "BarrelMesh",
			Position = new Vector3(0f, -0.03f, -0.32f),
			Rotation = new Vector3(Mathf.Pi * 0.5f, 0f, 0f),
			Mesh = new CylinderMesh
			{
				TopRadius = 0.08f,
				BottomRadius = 0.1f,
				Height = 1.15f,
				RadialSegments = 10,
			},
			MaterialOverride = new StandardMaterial3D { AlbedoColor = new Color(0.2f, 0.2f, 0.22f) },
		};
		mount.AddChild(barrel);

		var muzzle = new Marker3D
		{
			Name = $"{name}Muzzle",
			Position = new Vector3(0f, -0.03f, -0.9f),
		};
		mount.AddChild(muzzle);
	}
}
