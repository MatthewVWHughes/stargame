using Godot;
using Starlight.Game.Combat;
using Starlight.Game.Runtime;
using Starlight.Game.Stations;

namespace Starlight.Game.Space;

public partial class GameplayRoot
{
	private void UpdateNearestStation()
	{
		StationRuntime nearest = null;
		float nearestDistance = float.MaxValue;

		foreach (StationRuntime station in _stations.Values)
		{
			float distance = _ship.GlobalPosition.DistanceTo(station.Orbit.GlobalPosition);
			if (distance < nearestDistance)
			{
				nearestDistance = distance;
				nearest = station;
			}
		}

		_nearestStation = nearest;
	}

	private void OnDockRequested()
	{
		if (!TryGetSelectedStation(out StationRuntime selectedStation))
			return;

		float distance = _ship.GlobalPosition.DistanceTo(selectedStation.Orbit.GlobalPosition);
		if (distance > DockingCommsRange)
		{
			_commsMenu.ShowForStation(selectedStation.DisplayName, distance, false);
			return;
		}

		if (ShouldBlockDockingForHostiles(selectedStation))
		{
			_combatHud.SetStatus("Cannot dock while hostile contact is too close.");
			return;
		}

		_commsMenu.HideMenu();
		BeginDocking(selectedStation);
	}

	private void BeginDocking(StationRuntime station)
	{
		_dockingStation = station;
		_isDocking = true;
		_dockingOnFinalLeg = false;
		_dockingRailElapsed = 0f;
		_ship.EngageAutopilot(station.DockApproach, DockingApproachSpeed, DockingApproachArriveRadius);
		_combatHud.SetStatus($"Docking autopilot engaged for {station.DisplayName}.");
	}

	private void AbortDocking(string reason)
	{
		_ship.SetPhysicsProcess(true);
		_ship.DisengageAutopilot();
		_isDocking = false;
		_dockingOnFinalLeg = false;
		_dockingRailElapsed = 0f;
		_dockingStation = null;
		_combatHud.SetStatus(reason);
	}

	private void BeginDockingRail()
	{
		_dockingOnFinalLeg = true;
		_dockingRailElapsed = 0f;

		// Freeze the ship's own physics so the scripted rail is the sole author of its transform.
		_ship.DisengageAutopilot();
		_ship.SteeringEnabled = false;
		_ship.SetPhysicsProcess(false);

		// Capture ship's current pose in DockFinal's local frame. Re-projecting through DockFinal's
		// live GlobalTransform each frame carries the station's orbital motion for free.
		Transform3D dockXform = _dockingStation.DockFinal.GlobalTransform;
		Transform3D invDock = dockXform.AffineInverse();
		_dockingRailLocalStart = invDock * _ship.GlobalPosition;
		Basis invDockBasis = dockXform.Basis.Orthonormalized().Inverse();
		Basis localBasis = (invDockBasis * _ship.GlobalBasis.Orthonormalized()).Orthonormalized();
		_dockingRailLocalRotStart = localBasis.GetRotationQuaternion();

		_combatHud.SetStatus($"Final docking approach: {_dockingStation.DisplayName}.");
	}

	private void UpdateDocking()
	{
		if (_dockingStation == null)
			return;

		if (ShouldBlockDockingForHostiles(_dockingStation))
		{
			AbortDocking("Hostile detected. Docking aborted.");
			return;
		}

		if (!_dockingOnFinalLeg)
		{
			float d = _ship.GlobalPosition.DistanceTo(_dockingStation.DockApproach.GlobalPosition);
			if (d <= DockingFinalLegHandoffDist)
				BeginDockingRail();
			return;
		}

		// Scripted rail: interpolate ship transform in the station's local frame so orbital motion
		// is inherited automatically. No relative-velocity chase, no brake-curve asymptote.
		_dockingRailElapsed += (float)GetPhysicsProcessDeltaTime();
		float t = Mathf.Clamp(_dockingRailElapsed / DockingRailDuration, 0f, 1f);
		float eased = t * t * (3f - 2f * t);

		Transform3D dockXform = _dockingStation.DockFinal.GlobalTransform;
		Vector3 localPos = _dockingRailLocalStart.Lerp(Vector3.Zero, eased);
		Quaternion localRot = _dockingRailLocalRotStart.Slerp(Quaternion.Identity, eased);
		Transform3D localXform = new Transform3D(new Basis(localRot), localPos);
		_ship.GlobalTransform = dockXform * localXform;

		if (t < 1f)
			return;

		GameSession.DockAt(_dockingStation.Id);
		SaveService.SaveDockedState(_dockingStation.Id);
		_ship.DisengageAutopilot();
		_isDocking = false;
		_dockingRailElapsed = 0f;
		string interiorScene = StationRegistry.ResolveInteriorScene(_dockingStation.Id);
		_ = ChangeScene(interiorScene, $"Docking at {_dockingStation.DisplayName}...");
	}

	private void ApplyEntryMode()
	{
		if (GameSession.PendingEntryMode != GameSession.EntryMode.LaunchFromStation)
			return;

		if (!_stations.TryGetValue(GameSession.CurrentStationId, out StationRuntime station))
			return;

		_pendingLaunchStationId = station.Id;
		_pendingLaunchFrames = 2;
		GameSession.PendingEntryMode = GameSession.EntryMode.NewGame;
	}

	private void ResolvePendingLaunchPlacement()
	{
		if (string.IsNullOrWhiteSpace(_pendingLaunchStationId))
			return;

		if (_pendingLaunchFrames > 0)
		{
			_pendingLaunchFrames--;
			return;
		}

		if (!_stations.TryGetValue(_pendingLaunchStationId, out StationRuntime station))
		{
			_pendingLaunchStationId = "";
			return;
		}

		_ship.GlobalPosition = station.LaunchPoint.GlobalPosition;
		_ship.LookAt(station.Orbit.GlobalPosition, Vector3.Up);
		_ship.RotateObjectLocal(Vector3.Up, Mathf.Pi);
		_ship.SetPhysicsProcess(true);
		_ship.SetProcessUnhandledInput(true);

		// Hand control straight to the player — no autopilot on undock.
		// The previous autopilot-to-DockApproach flow flew the ship back toward the station and
		// could hang on stations with fast orbital tangential speed (e.g. Derelict Outpost).
		_ship.DisengageAutopilot();
		_ship.SteeringEnabled = true;
		_undockingStation = station;
		_undockingTimer = UndockCoastDuration;
		_pendingLaunchStationId = "";
	}

	private void UpdateUndocking()
	{
		if (_undockingStation == null)
			return;

		_undockingTimer -= (float)GetPhysicsProcessDeltaTime();
		if (_undockingTimer <= 0f)
			_undockingStation = null;
	}

	private bool TryGetSelectedStation(out StationRuntime station)
	{
		station = null;
		if (_selectedContact == null || _selectedContact.Kind != ContactKind.Station || string.IsNullOrWhiteSpace(_selectedContact.Id))
			return false;

		return _stations.TryGetValue(_selectedContact.Id, out station);
	}

	private bool IsHostileNearby(float range)
	{
		foreach (HostileEncounterController hostile in _hostiles)
		{
			if (hostile == null || !IsInstanceValid(hostile) || !hostile.IsAlive)
				continue;

			if (_ship.GlobalPosition.DistanceTo(hostile.GlobalPosition) <= range)
				return true;
		}

		return false;
	}

	private bool ShouldBlockDockingForHostiles(StationRuntime station)
	{
		if (station == null)
			return false;

		StationDefinition stationDef = StationRegistry.Get(station.Id);
		bool hostileNearShip = IsHostileNearby(DockingCommsRange);
		bool hostileNearStation = IsHostileNearPosition(station.Model?.GlobalPosition ?? station.Orbit.GlobalPosition, DockingCommsRange);

		if (stationDef?.Kind == StationInteriorKind.HostileBoarding)
			return false;

		return hostileNearShip || hostileNearStation;
	}

	private bool IsHostileNearPosition(Vector3 position, float range)
	{
		foreach (HostileEncounterController hostile in _hostiles)
		{
			if (hostile == null || !IsInstanceValid(hostile) || !hostile.IsAlive)
				continue;

			if (position.DistanceTo(hostile.GlobalPosition) <= range)
				return true;
		}

		return false;
	}
}
