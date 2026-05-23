using System.Collections.Generic;
using Godot;
using Starlight.Game.Combat;
using Starlight.Game.UI;

namespace Starlight.Game.Space;

public partial class GameplayRoot
{
	private void CycleTarget()
	{
		List<ContactTarget> contacts = BuildSelectableContacts(includeBodies: true);
		if (contacts.Count == 0)
			return;

		_targetCycleIndex = (_targetCycleIndex + 1) % contacts.Count;
		SetSelectedContact(contacts[_targetCycleIndex]);
		_combatHud.SetStatus($"Target set: {contacts[_targetCycleIndex].DisplayName}");
	}

	private bool TrySelectContactAtScreenPosition(Vector2 screenPos)
	{
		Camera3D camera = GetViewport().GetCamera3D();
		if (camera == null)
			return false;

		ContactTarget best = null;
		float bestDistance = float.MaxValue;

		foreach (ContactTarget contact in BuildSelectableContacts(includeBodies: false))
		{
			if (contact?.Node == null || !IsInstanceValid(contact.Node))
				continue;
			if (camera.IsPositionBehind(contact.Node.GlobalPosition))
				continue;

			Vector2 projected = camera.UnprojectPosition(contact.Node.GlobalPosition);
			float dist = projected.DistanceTo(screenPos);
			if (dist > 48f || dist >= bestDistance)
				continue;

			bestDistance = dist;
			best = contact;
		}

		if (best == null)
			return false;

		SetSelectedContact(best);
		_combatHud.SetStatus($"Selected contact: {best.DisplayName}");
		return true;
	}

	private List<ContactTarget> BuildSelectableContacts(bool includeBodies)
	{
		var contacts = new List<ContactTarget>();

		foreach ((string id, StationRuntime station) in _stations)
		{
			contacts.Add(new ContactTarget
			{
				Kind = ContactKind.Station,
				Id = id,
				DisplayName = station.DisplayName,
				Node = station.Model,
				CommsRange = DockingCommsRange,
				IffColor = Colors.White,
			});
		}

		foreach (HostileEncounterController hostile in _hostiles)
		{
			if (hostile == null || !IsInstanceValid(hostile) || !hostile.IsAlive)
				continue;

			contacts.Add(new ContactTarget
			{
				Kind = ContactKind.Hostile,
				Id = hostile.Name,
				DisplayName = hostile.DisplayName,
				Node = hostile,
				CommsRange = 0f,
				IffColor = new Color(1f, 0.35f, 0.35f),
			});
		}

		foreach (CombatSpawnPoint spawnPoint in _combatSpawnPoints)
		{
			contacts.Add(new ContactTarget
			{
				Kind = ContactKind.SpawnPoint,
				Id = spawnPoint.Name,
				DisplayName = spawnPoint.DisplayName,
				Node = spawnPoint,
				CommsRange = 0f,
				IffColor = new Color(1f, 0.65f, 0.35f),
			});
		}

		if (includeBodies && _systemBodies != null)
		{
			foreach (CelestialBody body in _systemBodies)
			{
				contacts.Add(new ContactTarget
				{
					Kind = ContactKind.Body,
					Id = body.Name,
					DisplayName = body.Name,
					Node = body.Node,
					CommsRange = 0f,
					IffColor = body.MapColor,
				});
			}
		}

		return contacts;
	}

	private int CountActiveHostiles()
	{
		int count = 0;
		foreach (HostileEncounterController hostile in _hostiles)
		{
			if (hostile != null && IsInstanceValid(hostile) && hostile.IsAlive)
				count++;
		}

		return count;
	}

	private bool TryGetClosestHostile(out HostileEncounterController bestHostile, out float bestDistance)
	{
		bestHostile = null;
		bestDistance = float.MaxValue;

		foreach (HostileEncounterController hostile in _hostiles)
		{
			if (hostile == null || !IsInstanceValid(hostile) || !hostile.IsAlive)
				continue;

			float distance = _ship.GlobalPosition.DistanceTo(hostile.GlobalPosition);
			if (distance >= bestDistance)
				continue;

			bestDistance = distance;
			bestHostile = hostile;
		}

		return bestHostile != null;
	}

	private CombatSpawnPoint GetNearestSpawnPoint()
	{
		CombatSpawnPoint nearest = null;
		float bestDistance = float.MaxValue;

		foreach (CombatSpawnPoint spawnPoint in _combatSpawnPoints)
		{
			float distance = _ship.GlobalPosition.DistanceTo(spawnPoint.GlobalPosition);
			if (distance >= bestDistance)
				continue;

			bestDistance = distance;
			nearest = spawnPoint;
		}

		return nearest;
	}
}
