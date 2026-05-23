using Godot;
using Starlight.Game.Combat;
using Starlight.Game.Missions;
using Starlight.Game.Runtime;

namespace Starlight.Game.Space;

public partial class GameplayRoot
{
	private void UpdateCombat(float delta)
	{
		_weaponCooldown = Mathf.Max(0f, _weaponCooldown - delta);
		_playerShieldRegenDelayTimer = Mathf.Max(0f, _playerShieldRegenDelayTimer - delta);
		PlayerState.RegenerateWeaponEnergy(PlayerWeaponEnergyRegenPerSecond * delta);
		if (_playerShieldRegenDelayTimer <= 0f)
			PlayerState.RegenerateShields(PlayerShieldRegenPerSecond * delta);

		UpdatePlayerAimPoint();
		_playerWeaponRig?.AimAt(_playerAimPoint, delta);
		_hud?.SetLeadPoint(_playerLeadPoint);

		if (!_isDocking && Input.IsMouseButtonPressed(MouseButton.Right))
			FirePlayerWeapon();

		foreach (CombatSpawnPoint spawnPoint in _combatSpawnPoints)
		{
			spawnPoint.UpdateSpawnTimer(delta);
			int safety = 0;
			while (spawnPoint.CanSpawn(_ship.GlobalPosition) && safety < 8)
			{
				SpawnHostileAt(spawnPoint);
				safety++;
			}
		}

		UpdateMissionEncounters();

		CleanupDestroyedHostiles();
		RebuildCombatantList();
		UpdateProjectiles(delta);

		if (PlayerState.Hull <= 0f)
		{
			float recoveryCost = Mathf.Min(150f, PlayerState.Credits);
			PlayerState.TrySpendCredits(recoveryCost);
			PlayerState.RepairHull();
			GameSession.DockAt("EarthHub");
			GameSession.PendingStatusMessage = $"Emergency dock complete. Hull restored. Recovery fee: {recoveryCost:F0} credits.";
			SaveService.SaveDockedState("EarthHub");
			_ = ChangeScene(StationStubScenePath, "Emergency dock at Earth Hub...");
			return;
		}

		UpdateCombatReadout();
	}

	private void SpawnHostileAt(CombatSpawnPoint spawnPoint)
	{
		var hostile = new HostileEncounterController
		{
			Name = $"{spawnPoint.Name}Hostile",
			DisplayName = $"{spawnPoint.DisplayName} Raider",
		};
		hostile.SetPlayer(_ship);
		hostile.StatusChanged += msg => _combatHud.SetStatus(msg);
		hostile.ProjectileFired += SpawnProjectile;
		hostile.Destroyed += () =>
		{
			PlayerState.AddCredits(120f);
			spawnPoint.NotifyHostileDestroyed();
			_combatHud.SetStatus($"{hostile.DisplayName} destroyed. +120 credits.");
		};

		AddChild(hostile);
		hostile.SpawnAt(spawnPoint.GetSpawnPosition(), spawnPoint.GlobalPosition);
		_hostiles.Add(hostile);
		spawnPoint.BindHostile(hostile);
		_combatHud.SetStatus($"{spawnPoint.DisplayName}: hostile contact launching.");
	}

	private void UpdateMissionEncounters()
	{
		foreach ((string encounterId, MissionEncounterRuntime runtime) in _missionEncounters)
		{
			CleanupMissionEncounter(runtime);
		}

		foreach (MissionService.ActiveEncounterRequest request in MissionService.ActiveEncounterRequests())
		{
			MissionEncounterRuntime runtime = GetOrCreateMissionEncounter(request);
			if (runtime.Completed || runtime.HasSpawned)
				continue;
			if (!_stations.TryGetValue(runtime.AnchorStationId, out StationRuntime station))
				continue;
			if (_ship.GlobalPosition.DistanceTo(station.Orbit.GlobalPosition) > runtime.ActivationRange)
				continue;

			SpawnMissionEncounter(runtime, station);
		}
	}

	private MissionEncounterRuntime GetOrCreateMissionEncounter(MissionService.ActiveEncounterRequest request)
	{
		if (_missionEncounters.TryGetValue(request.Encounter.Id, out MissionEncounterRuntime existing))
			return existing;

		var created = new MissionEncounterRuntime
		{
			EncounterId = request.Encounter.Id,
			MissionId = request.MissionId,
			ObjectiveId = request.ObjectiveId,
			AnchorStationId = request.Encounter.AnchorStationId,
			DisplayName = request.Encounter.DisplayName,
			WingSize = request.Encounter.WingSize,
			ActivationRange = request.Encounter.ActivationRange,
			SpawnRadiusMin = request.Encounter.SpawnRadiusMin,
			SpawnRadiusMax = request.Encounter.SpawnRadiusMax,
		};
		_missionEncounters[created.EncounterId] = created;
		return created;
	}

	private void SpawnMissionEncounter(MissionEncounterRuntime runtime, StationRuntime station)
	{
		runtime.HasSpawned = true;
		for (int i = 0; i < runtime.WingSize; i++)
		{
			Vector3 spawnPosition = GetMissionEncounterSpawnPosition(runtime, station, i);
			var hostile = new HostileEncounterController
			{
				Name = $"{runtime.EncounterId}_Hostile_{i}",
				DisplayName = $"{runtime.DisplayName} Raider",
			};
			hostile.SetPlayer(_ship);
			hostile.StatusChanged += msg => _combatHud.SetStatus(msg);
			hostile.ProjectileFired += SpawnProjectile;
			hostile.Destroyed += () =>
			{
				PlayerState.AddCredits(120f);
				_combatHud.SetStatus($"{hostile.DisplayName} destroyed. +120 credits.");
				OnMissionEncounterHostileDestroyed(runtime);
			};

			AddChild(hostile);
			hostile.SpawnAt(spawnPosition, station.Orbit.GlobalPosition);
			_hostiles.Add(hostile);
			runtime.ActiveHostiles.Add(hostile);
		}

		_combatHud.SetStatus($"{runtime.DisplayName}: hostile wing launching near {station.DisplayName}.");
	}

	private Vector3 GetMissionEncounterSpawnPosition(MissionEncounterRuntime runtime, StationRuntime station, int index)
	{
		float t = runtime.WingSize <= 1 ? 0f : (float)index / runtime.WingSize;
		float angle = t * Mathf.Tau + GD.Randf() * 0.35f;
		float radius = Mathf.Lerp(runtime.SpawnRadiusMin, runtime.SpawnRadiusMax, 0.35f + GD.Randf() * 0.65f);
		return station.Orbit.GlobalPosition + new Vector3(Mathf.Cos(angle) * radius, 0f, Mathf.Sin(angle) * radius);
	}

	private void CleanupMissionEncounter(MissionEncounterRuntime runtime)
	{
		for (int i = runtime.ActiveHostiles.Count - 1; i >= 0; i--)
		{
			HostileEncounterController hostile = runtime.ActiveHostiles[i];
			if (hostile == null || !IsInstanceValid(hostile) || !hostile.IsAlive)
				runtime.ActiveHostiles.RemoveAt(i);
		}
	}

	private void OnMissionEncounterHostileDestroyed(MissionEncounterRuntime runtime)
	{
		CleanupMissionEncounter(runtime);
		if (runtime.Completed || runtime.ActiveHostiles.Count > 0)
			return;

		runtime.Completed = MissionService.TryAdvanceEncounterObjective(runtime.MissionId, runtime.ObjectiveId);
		if (runtime.Completed)
			_combatHud.SetStatus($"{runtime.DisplayName} cleared. Mission updated.");
	}

	private void CleanupDestroyedHostiles()
	{
		for (int i = _hostiles.Count - 1; i >= 0; i--)
		{
			HostileEncounterController hostile = _hostiles[i];
			if (hostile != null && IsInstanceValid(hostile) && hostile.IsAlive)
				continue;

			if (_selectedContact?.Node == hostile)
				SetSelectedContact(null);

			_hostiles.RemoveAt(i);
			if (hostile != null && IsInstanceValid(hostile))
				hostile.QueueFree();
		}
	}

	private void RebuildCombatantList()
	{
		_combatants.Clear();
		_combatants.Add(_playerCombatant);
		foreach (HostileEncounterController hostile in _hostiles)
		{
			if (hostile != null && IsInstanceValid(hostile) && hostile.IsAlive)
				_combatants.Add(hostile);
		}
	}

	private void UpdateProjectiles(float delta)
	{
		for (int i = _projectiles.Count - 1; i >= 0; i--)
		{
			SpaceProjectile projectile = _projectiles[i];
			if (projectile == null || !IsInstanceValid(projectile))
			{
				_projectiles.RemoveAt(i);
				continue;
			}

			if (!projectile.Advance(delta, _combatants))
				_projectiles.RemoveAt(i);
		}
	}

	private void SpawnProjectile(ProjectileFireRequest request)
	{
		var projectile = new SpaceProjectile
		{
			Name = "Projectile",
		};
		AddChild(projectile);
		projectile.Configure(request);
		_projectiles.Add(projectile);
	}

	private void FirePlayerWeapon()
	{
		if (_weaponCooldown > 0f || _playerWeaponRig == null)
			return;
		if (!PlayerState.TryConsumeWeaponEnergy(PlayerWeaponEnergyCost))
			return;

		Transform3D muzzle = _playerWeaponRig.GetNextMuzzleTransform();
		Vector3 direction = (_playerAimPoint - muzzle.Origin).Normalized();
		if (direction.LengthSquared() <= 0.001f)
			direction = -muzzle.Basis.Z;
		if ((-muzzle.Basis.Z).Dot(direction) < PlayerWeaponMinAlignment)
		{
			PlayerState.RegenerateWeaponEnergy(PlayerWeaponEnergyCost);
			return;
		}

		SpawnProjectile(new ProjectileFireRequest(
			muzzle,
			direction,
			CombatFaction.Player,
			PlayerWeaponDamage,
			PlayerProjectileSpeed,
			PlayerProjectileMaxDistance,
			_ship.WorldVelocity,
			new Color(0.25f, 0.85f, 1f)));
		_weaponCooldown = PlayerWeaponCooldownSeconds;
	}

	private void OnPlayerDamaged()
	{
		_playerShieldRegenDelayTimer = PlayerShieldRegenDelaySeconds;
	}

	private void OnPlayerInterdicted(float durationSeconds, string sourceName)
	{
		_combatHud?.SetStatus($"{sourceName} collapsed the phase shell. Supercruise offline for {durationSeconds:F0}s.");
	}

	private void UpdatePlayerAimPoint()
	{
		if (_ship == null)
			return;

		Camera3D camera = GetViewport().GetCamera3D();
		if (camera == null)
		{
			_playerAimPoint = _ship.GlobalPosition + (-_ship.GlobalTransform.Basis.Z * 1200f);
			return;
		}

		Vector2 mousePos = GetViewport().GetMousePosition();
		Vector3 rayOrigin = camera.ProjectRayOrigin(mousePos);
		Vector3 rayDirection = camera.ProjectRayNormal(mousePos).Normalized();

		HostileEncounterController targetHostile = null;
		float depth = 1200f;
		if (_selectedContact?.Node is HostileEncounterController selectedHostile && IsInstanceValid(selectedHostile) && selectedHostile.IsAlive)
		{
			targetHostile = selectedHostile;
			depth = Mathf.Clamp(camera.GlobalPosition.DistanceTo(selectedHostile.GlobalPosition), 300f, 2600f);
		}
		else if (TryGetClosestHostile(out HostileEncounterController hostile, out float hostileDistance))
		{
			targetHostile = hostile;
			depth = Mathf.Clamp(hostileDistance, 300f, 2600f);
		}

		_playerAimPoint = rayOrigin + rayDirection * depth;
		_playerLeadPoint = null;
		if (targetHostile != null &&
			SpaceCombatMath.TryGetInterceptPoint(
				_ship.GlobalPosition,
				_ship.WorldVelocity,
				targetHostile.GlobalPosition,
				targetHostile.Velocity,
				PlayerProjectileSpeed,
				out Vector3 leadPoint))
		{
			_playerLeadPoint = leadPoint;
		}
	}

	private void UpdateCombatReadout()
	{
		if (_combatHud == null || _ship == null)
			return;

		CombatSpawnPoint nearestSpawn = GetNearestSpawnPoint();
		string spawnSummary = nearestSpawn == null
			? "No patrol beacons loaded."
			: $"{nearestSpawn.DisplayName}: {_ship.GlobalPosition.DistanceTo(nearestSpawn.GlobalPosition):F0} m";
		string phaseDriveSummary = _ship.SupercruiseDriveOffline
			? $"Phase drive: OFFLINE {_ship.SupercruiseOfflineTimer:F0}s"
			: (_ship.InsideSupercruiseLockout ? "Phase drive: LOCKED" : "Phase drive: READY");
		string interdictionSummary = _ship.InterdictionLockDetected
			? $"Interdiction: {_ship.InterdictionLockStrength * 100f:F0}%   |   Envelope {_ship.InterdictionCollapseDistance:F0} m   |   Scrambler {(_ship.PhaseScramblerReady ? "READY" : (_ship.PhaseScramblerActive ? "ACTIVE" : $"{_ship.PhaseScramblerCooldownTimer:F0}s"))}"
			: $"Interdiction: clear   |   Scrambler {(_ship.PhaseScramblerReady ? "READY" : (_ship.PhaseScramblerActive ? "ACTIVE" : $"{_ship.PhaseScramblerCooldownTimer:F0}s"))}";

		if (!TryGetClosestHostile(out HostileEncounterController hostile, out float hostileDistance))
		{
			_combatHud.SetSummary(
				$"Patrol beacon: {spawnSummary}\n" +
				$"{phaseDriveSummary}\n" +
				$"{interdictionSummary}\n" +
				$"Weapons: {PlayerState.WeaponEnergy:F0}/{PlayerState.MaxWeaponEnergy:F0}   |   Shields: {PlayerState.Shields:F0}/{PlayerState.MaxShields:F0}");
			_combatHud.SetHint("Left click selects contacts. Hold right click to fire. In supercruise, press R to scramble an interdiction lock or J to drop early.");
			return;
		}

		_combatHud.SetSummary(
			$"Patrol beacon: {spawnSummary}\n" +
			$"Hostile: {hostile.DisplayName} ({hostileDistance:F0} m)\n" +
			$"Enemy shields/hull: {hostile.Shields:F0}/{hostile.MaxShields:F0} | {hostile.Hull:F0}/{hostile.MaxHull:F0}\n" +
			$"{phaseDriveSummary}\n" +
			$"{interdictionSummary}\n" +
			$"Weapons: {PlayerState.WeaponEnergy:F0}/{PlayerState.MaxWeaponEnergy:F0}   |   Shields: {PlayerState.Shields:F0}/{PlayerState.MaxShields:F0}\n" +
			$"{BuildPlayerFireSolutionText(hostile, hostileDistance)}");
		_combatHud.SetHint("Lead the target with the yellow pip. If raiders collapse your phase shell, keep moving and clear distance before trying to spool back up.");
	}

	private string BuildPlayerFireSolutionText(HostileEncounterController hostile, float hostileDistance)
	{
		bool inRange = hostileDistance <= PlayerProjectileMaxDistance * 0.9f;
		bool hasEnergy = PlayerState.WeaponEnergy >= PlayerWeaponEnergyCost;
		bool hasLeadPoint = _playerLeadPoint != null;
		float alignment = _playerLeadPoint != null && _playerWeaponRig != null
			? _playerWeaponRig.GetAimAlignment(_playerLeadPoint.Value)
			: 0f;
		bool aligned = alignment >= PlayerWeaponMinAlignment;

		string rangeText = inRange ? "RANGE OK" : "OUT OF RANGE";
		string energyText = hasEnergy ? "CAP OK" : "CAP LOW";
		string aimText = hasLeadPoint ? (aligned ? "AIM READY" : "AIM OFF") : "NO LEAD";

		return $"Solution: {rangeText}   |   {energyText}   |   {aimText}";
	}
}
