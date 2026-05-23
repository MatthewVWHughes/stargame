using Godot;
using Starlight.Game.Combat;
using Starlight.Game.Missions;
using Starlight.Game.Stations;
using Starlight.Game.UI;

namespace Starlight.Game.Space;

public partial class GameplayRoot
{
	private void UpdateGuidance()
	{
		if (_statusHud == null || _ship == null)
			return;

		StationRuntime missionWaypoint = GetMissionWaypointStation();
		UpdateHudTarget(missionWaypoint);

		string objective;
		string missionLine = BuildMissionGuidance();
		if (_isDocking && _dockingStation != null)
		{
			objective = _dockingOnFinalLeg
				? $"Docking objective: final approach into {_dockingStation.DisplayName}. Autopilot is handling the last meters."
				: $"Docking objective: autopilot is flying the approach path to {_dockingStation.DisplayName}.";
		}
		else if (_undockingStation != null)
		{
			objective = $"Launch objective: clearing {_undockingStation.DisplayName} departure corridor.";
		}
		else if (!string.IsNullOrEmpty(missionLine))
		{
			objective = missionLine;
		}
		else if (CountActiveHostiles() > 0)
		{
			objective = "Combat objective: hostile patrols are active. Keep moving, track the cursor, and clear the contact.";
		}
		else
		{
			objective = "Combat objective: fly toward an orange patrol beacon to trigger a hostile ship.";
		}

		string location = "";
		if (!_isDocking && _undockingStation == null && missionWaypoint != null)
		{
			float waypointDistance = _ship.GlobalPosition.DistanceTo(missionWaypoint.Orbit.GlobalPosition);
			location = $"Mission waypoint: {missionWaypoint.DisplayName} ({waypointDistance:F0} m)";
		}

		if (_nearestStation != null)
		{
			float stationDistance = _ship.GlobalPosition.DistanceTo(_nearestStation.Orbit.GlobalPosition);
			string nearestStationLine = $"Nearest station: {_nearestStation.DisplayName} ({stationDistance:F0} m)";
			if (string.IsNullOrEmpty(location))
				location = nearestStationLine;
			else if (_nearestStation != missionWaypoint)
				location += $"\n{nearestStationLine}";
		}

		if (_isDocking && _dockingStation != null)
		{
			float waypointDistance = _dockingOnFinalLeg
				? _ship.GlobalPosition.DistanceTo(_dockingStation.DockFinal.GlobalPosition)
				: _ship.GlobalPosition.DistanceTo(_dockingStation.DockApproach.GlobalPosition);
			location = $"Docking target: {_dockingStation.DisplayName} ({waypointDistance:F1} m to waypoint)";
		}
		else if (_undockingStation != null)
		{
			float clearDistance = _ship.GlobalPosition.DistanceTo(_undockingStation.DockApproach.GlobalPosition);
			location = $"Departure lane: {_undockingStation.DisplayName} ({clearDistance:F1} m to clear point)";
		}

		CombatSpawnPoint nearestSpawn = GetNearestSpawnPoint();
		if (nearestSpawn != null)
		{
			float spawnDistance = _ship.GlobalPosition.DistanceTo(nearestSpawn.GlobalPosition);
			if (!string.IsNullOrEmpty(location))
				location += "\n";
			location += $"Patrol beacon: {nearestSpawn.DisplayName} ({spawnDistance:F0} m)";
		}

		_statusHud.SetObjective(objective);
		_statusHud.SetLocationSummary(location);
	}

	private string BuildMissionGuidance()
	{
		foreach ((MissionDefinition mission, MissionStatus status) in MissionService.ActiveMissions())
		{
			MissionObjective active = MissionService.GetActiveObjective(mission.Id);
			string prefix;
			string text;
			switch (status)
			{
				case MissionStatus.InProgress:
					if (active == null)
						continue;
					prefix = "Mission";
					text = $"{mission.Title}: {active.Text}";
					break;
				case MissionStatus.ReadyToTurnIn:
					prefix = "Mission";
					text = $"{mission.Title}: return to {GetStationDisplayName(mission.GiverStationId)} to report in.";
					break;
				default:
					continue;
			}
			return $"{prefix}: {text}";
		}
		return "";
	}

	private void SetSelectedContact(ContactTarget contact)
	{
		_selectedContact = contact;
		UpdateHudTarget(GetMissionWaypointStation());
	}

	private void UpdateHudTarget(StationRuntime missionWaypoint)
	{
		if (_hud == null)
			return;

		if (_selectedContact == null || _selectedContact.Node == null || !IsInstanceValid(_selectedContact.Node))
		{
			if (missionWaypoint == null || missionWaypoint.Model == null || !IsInstanceValid(missionWaypoint.Model))
			{
				_hud.ClearTarget();
				return;
			}

			_hud.SetTarget(new CelestialBody
			{
				Name = $"{missionWaypoint.DisplayName} [MISSION]",
				Node = missionWaypoint.Model,
				MapColor = new Color(1f, 0.82f, 0.30f),
				Radius = 50f,
				OrbitRadius = missionWaypoint.Model.GlobalPosition.Length(),
			});
			return;
		}

		_hud.SetTarget(new CelestialBody
		{
			Name = _selectedContact.DisplayName,
			Node = _selectedContact.Node,
			MapColor = _selectedContact.IffColor,
			Radius = 50f,
			OrbitRadius = _selectedContact.Node.GlobalPosition.Length(),
		});
	}

	private StationRuntime GetMissionWaypointStation()
	{
		foreach ((MissionDefinition mission, MissionStatus status) in MissionService.ActiveMissions())
		{
			string stationId = MissionService.GetWaypointStationId(mission.Id);
			if (!string.IsNullOrWhiteSpace(stationId) && _stations.TryGetValue(stationId, out StationRuntime station))
				return station;
		}

		return null;
	}

	private string GetStationDisplayName(string stationId)
	{
		if (!string.IsNullOrWhiteSpace(stationId) && _stations.TryGetValue(stationId, out StationRuntime station))
			return station.DisplayName;

		return StationRegistry.Get(stationId)?.DisplayName ?? stationId;
	}
}
