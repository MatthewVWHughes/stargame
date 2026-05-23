using Godot;
using System.Collections.Generic;
using System.Threading.Tasks;
using Starlight.Game.Combat;
using Starlight.Ship;
using Starlight.Game.Missions;
using Starlight.Game.Runtime;
using Starlight.Game.Stations;
using Starlight.Game.UI;
using Starlight.Camera;
using Starlight.Orbital;
using Starlight.Npc;

namespace Starlight.Game.Space;

/// <summary>
/// Foundation gameplay root with lightweight space combat, docking, and Sol navigation.
/// </summary>
public partial class GameplayRoot : Node3D
{

	private enum ContactKind
	{
		None,
		Station,
		Hostile,
		SpawnPoint,
		Body,
	}

	private sealed class ContactTarget
	{
		public ContactKind Kind { get; init; }
		public string Id { get; init; }
		public string DisplayName { get; init; }
		public Node3D Node { get; init; }
		public float CommsRange { get; init; }
		public Color IffColor { get; init; }
	}

	private sealed class StationRuntime
	{
		public string Id { get; init; }
		public string DisplayName { get; init; }
		public OrbitalBody Orbit { get; init; }
		public Node3D Model { get; init; }
		public Marker3D DockApproach { get; init; }
		public Marker3D DockFinal { get; init; }
		public Marker3D LaunchPoint { get; init; }
	}

	private sealed class MissionEncounterRuntime
	{
		public string EncounterId { get; init; }
		public string MissionId { get; init; }
		public string ObjectiveId { get; init; }
		public string AnchorStationId { get; init; }
		public string DisplayName { get; init; }
		public int WingSize { get; init; }
		public float ActivationRange { get; init; }
		public float SpawnRadiusMin { get; init; }
		public float SpawnRadiusMax { get; init; }
		public bool HasSpawned { get; set; }
		public bool Completed { get; set; }
		public readonly List<HostileEncounterController> ActiveHostiles = new();
	}

	private sealed class PlayerCombatant : ISpaceCombatant, IInterdictableCombatant
	{
		private readonly ShipController _ship;
		private readonly System.Action _damageCallback;
		private readonly System.Action<float, string> _interdictionCallback;

		public PlayerCombatant(ShipController ship, System.Action damageCallback, System.Action<float, string> interdictionCallback)
		{
			_ship = ship;
			_damageCallback = damageCallback;
			_interdictionCallback = interdictionCallback;
		}

		public string CombatDisplayName => "Player";
		public CombatFaction Faction => CombatFaction.Player;
		public Vector3 CombatOrigin => _ship?.GlobalPosition ?? Vector3.Zero;
		public Vector3 CombatVelocity => _ship?.WorldVelocity ?? Vector3.Zero;
		public float CollisionRadius => 7.5f;
		public bool IsAlive => _ship != null && PlayerState.Hull > 0f;

		public void ApplyCombatDamage(float amount, Node3D source)
		{
			PlayerState.ApplyHullDamage(amount);
			_damageCallback?.Invoke();
		}

		public void ApplyInterdiction(float durationSeconds, Node3D source)
		{
			if (_ship == null || !_ship.ApplySupercruiseInterdiction(durationSeconds))
				return;

			string sourceName = source is SpaceProjectile projectile && !string.IsNullOrWhiteSpace(projectile.SourceDisplayName)
				? projectile.SourceDisplayName
				: "Hostile contact";
			_interdictionCallback?.Invoke(durationSeconds, sourceName);
		}
	}

	private const float OriginShiftThreshold = 20000f;
	private const string SectorScenePath = "res://scenes/game/space/StarSystem.tscn";
	private const string StarterShipScenePath = "res://scenes/game/ships/PlayerShip.tscn";
	private const string GameplayScenePath = "res://scenes/game/space/Gameplay.tscn";
	private const string DefaultSystemId = "sol";
	private const string CommsMenuScenePath = "res://scenes/game/ui/CommsMenu.tscn";
	private const string CombatHudScenePath = "res://scenes/game/ui/CombatHud.tscn";
	private const string StatusHudScenePath = "res://scenes/game/ui/StatusHud.tscn";
	private const string StationStubScenePath = "res://scenes/game/station/StationStub.tscn";
	private const float DockingCommsRange = 900f;
	private const float DockingApproachSpeed = 60f;
	private const float DockingApproachArriveRadius = 8f;
	private const float DockingFinalLegHandoffDist = 12f;
	private const float UndockCoastDuration = 3f;
	private const float PlayerWeaponDamage = 14f;
	private const float PlayerWeaponCooldownSeconds = 0.18f;
	private const float PlayerProjectileSpeed = 760f;
	private const float PlayerProjectileMaxDistance = 1200f;
	private const float PlayerWeaponEnergyCost = 9f;
	private const float PlayerWeaponEnergyRegenPerSecond = 30f;
	private const float PlayerShieldRegenPerSecond = 7f;
	private const float PlayerShieldRegenDelaySeconds = 2.75f;
	private const float PlayerWeaponMinAlignment = 0.955f;

	private Node3D _sectorRoot;
	private Node3D _star;
	private string _currentSystemId = "";
	private readonly List<JumpGateController> _jumpGates = new();
	private int _pendingArrivalFrames;
	private ShipController _ship;
	private ShipWeaponRig _playerWeaponRig;
	private PlayerCombatant _playerCombatant;
	private ChaseCamera _chaseCamera;
	private FlightHud _hud;
	private CombatHudController _combatHud;
	private StatusHudController _statusHud;
	private CommsMenuController _commsMenu;
	private SystemMap _systemMap;
	private CelestialBody[] _systemBodies;
	private readonly Dictionary<string, StationRuntime> _stations = new();
	private readonly List<CombatSpawnPoint> _combatSpawnPoints = new();
	private readonly List<HostileEncounterController> _hostiles = new();
	private readonly Dictionary<string, MissionEncounterRuntime> _missionEncounters = new();
	private readonly List<SpaceProjectile> _projectiles = new();
	private readonly List<ISpaceCombatant> _combatants = new();
	private NpcManager _npcManager;

	private StationRuntime _nearestStation;
	private StationRuntime _dockingStation;
	private StationRuntime _undockingStation;
	private float _undockingTimer;
	private bool _isDocking;
	private bool _dockingOnFinalLeg;
	private const float DockingRailDuration = 2.5f;
	private float _dockingRailElapsed;
	private Vector3 _dockingRailLocalStart;
	private Quaternion _dockingRailLocalRotStart = Quaternion.Identity;
	private float _weaponCooldown;
	private int _targetCycleIndex = -1;
	private string _pendingLaunchStationId = "";
	private int _pendingLaunchFrames;
	private ContactTarget _selectedContact;
	private Vector3 _playerAimPoint;
	private Vector3? _playerLeadPoint;
	private float _playerShieldRegenDelayTimer;
	private CanvasLayer _debugOverlay;
	private Label _debugLabel;
	private bool _debugVisible;

	public override void _Ready()
	{
		// Freelancer-style cursor steering needs a visible, free-moving mouse. Station interiors
		// capture the cursor for FPS look; assert the visible mode on entry so we never inherit
		// a captured cursor from the previous scene (e.g. undocking from a hostile station).
		Input.MouseMode = Input.MouseModeEnum.Visible;

		// Pick the star system from session state (set by JumpToSystem on gate
		// transit). First boot leaves CurrentSystemId empty, which falls
		// through to Sol.
		string systemId = string.IsNullOrWhiteSpace(GameSession.CurrentSystemId)
			? DefaultSystemId
			: GameSession.CurrentSystemId;
		_currentSystemId = systemId;

		Node3D sectorRoot = GD.Load<PackedScene>(SectorScenePath).Instantiate<Node3D>();
		sectorRoot.Name = "SectorRoot";

		// StarSystemScene._Ready runs inside AddChild and consumes SystemId —
		// set it first so the builder sees the right id on the first frame.
		if (sectorRoot is StarSystemScene systemScene)
			systemScene.SystemId = systemId;

		AddChild(sectorRoot);

		// EnsureBuilt is idempotent; second call documents the dependency for
		// anything that might instantiate StarSystem.tscn without using _Ready.
		if (sectorRoot is StarSystemScene builtScene)
			builtScene.EnsureBuilt();

		_sectorRoot = sectorRoot;
		_star = sectorRoot.GetNode<Node3D>("Star");
		BindStationRuntime(sectorRoot, "EarthHub", "Earth Hub", "Star/Earth", "Star/Earth/EarthHubOrbit", "Star/Earth/EarthHubOrbit/EarthHub");
		BindStationRuntime(sectorRoot, "DerelictOutpost", "Derelict Outpost", "Star/Earth", "Star/Earth/DerelictOutpostOrbit", "Star/Earth/DerelictOutpostOrbit/DerelictOutpost");
		BindStationRuntime(sectorRoot, "ArmstrongStation", "Armstrong Station", "Star/Earth/Luna", "Star/Earth/Luna/ArmstrongStationOrbit", "Star/Earth/Luna/ArmstrongStationOrbit/ArmstrongStation");
		BindStationRuntime(sectorRoot, "MarsColony", "Mars Colony", "Star/Mars", "Star/Mars/MarsColonyOrbit", "Star/Mars/MarsColonyOrbit/MarsColony");
		BindStationRuntime(sectorRoot, "CeresPort", "Ceres Port", "Star/Ceres", "Star/Ceres/CeresPortOrbit", "Star/Ceres/CeresPortOrbit/CeresPort");

		_ship = BuildShip();
		_ship.SetStar(_star);
		_playerWeaponRig = _ship.GetNodeOrNull<ShipWeaponRig>("WeaponRig");
		_playerCombatant = new PlayerCombatant(_ship, OnPlayerDamaged, OnPlayerInterdicted);

		var gravityWells = new List<GravityWell>
		{
			new() { Body = _star, Radius = 2000f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Mercury"), Radius = 306f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Venus"), Radius = 760f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Earth"), Radius = 800f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Earth/Luna"), Radius = 218f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Mars"), Radius = 426f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Mars/Phobos"), Radius = 24f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Mars/Deimos"), Radius = 16f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Ceres"), Radius = 60f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Vesta"), Radius = 45f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Pallas"), Radius = 40f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Hygiea"), Radius = 48f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Jupiter"), Radius = 8785f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Jupiter/Io"), Radius = 260f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Jupiter/Europa"), Radius = 245f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Jupiter/Ganymede"), Radius = 310f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Jupiter/Callisto"), Radius = 285f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Saturn"), Radius = 7317f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Saturn/Enceladus"), Radius = 72f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Saturn/Rhea"), Radius = 120f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Saturn/Titan"), Radius = 290f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Saturn/Iapetus"), Radius = 112f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Uranus"), Radius = 3185f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Uranus/Miranda"), Radius = 58f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Uranus/Ariel"), Radius = 92f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Uranus/Umbriel"), Radius = 96f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Uranus/Titania"), Radius = 125f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Uranus/Oberon"), Radius = 120f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Neptune"), Radius = 3093f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Neptune/Triton"), Radius = 155f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Neptune/Nereid"), Radius = 40f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Pluto"), Radius = 95f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Pluto/Charon"), Radius = 50f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Haumea"), Radius = 88f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Makemake"), Radius = 68f, IsGravitySource = true },
			new() { Body = sectorRoot.GetNode<Node3D>("Star/Eris"), Radius = 76f, IsGravitySource = true },
		};

		_ship.SetGravityWells(gravityWells.ToArray());
		var stationAssistTargets = new List<StationAssistTarget>();
		foreach (StationRuntime station in _stations.Values)
		{
			stationAssistTargets.Add(new StationAssistTarget
			{
				Body = station.Model,
				Radius = 60f,
			});
		}
		_ship.SetStationAssistTargets(stationAssistTargets.ToArray());

		_chaseCamera = new ChaseCamera
		{
			Name = "ChaseCamera",
			Near = 0.5f,
			Far = 500000f,
		};
		_ship.AddChild(_chaseCamera);
		_chaseCamera.SetTarget(_ship);
		_chaseCamera.Current = true;

		_hud = new FlightHud();
		_hud.Name = "FlightHud";
		_hud.SetShip(_ship);
		AddChild(_hud);

		_combatHud = GD.Load<PackedScene>(CombatHudScenePath).Instantiate<CombatHudController>();
		_combatHud.Name = "CombatHud";
		AddChild(_combatHud);

		_statusHud = GD.Load<PackedScene>(StatusHudScenePath).Instantiate<StatusHudController>();
		_statusHud.Name = "VS1StatusHud";
		AddChild(_statusHud);
		_statusHud.Visible = false;

		_commsMenu = GD.Load<PackedScene>(CommsMenuScenePath).Instantiate<CommsMenuController>();
		_commsMenu.Name = "CommsMenu";
		_commsMenu.RequestDockPressed += OnDockRequested;
		_commsMenu.Closed += () => _ship.SteeringEnabled = true;
		AddChild(_commsMenu);

		CreateCombatSpawnPoints(sectorRoot);
		CreateNpcTraffic();
		SetupSystemMap(sectorRoot);
		CollectJumpGates();
		ApplyEntryMode();
		CreateDebugOverlay();
		UpdateCombatReadout();

		// Arrival placement needs to run a couple of frames after scene load
		// so OrbitalBody and LagrangeAnchor have ticked and placed gates at
		// their live positions.
		if (!string.IsNullOrWhiteSpace(GameSession.PendingArrivalGateId))
			_pendingArrivalFrames = 3;
	}

	private void CollectJumpGates()
	{
		_jumpGates.Clear();
		foreach (Node node in GetTree().GetNodesInGroup("jump_gates"))
		{
			if (node is JumpGateController gate)
				_jumpGates.Add(gate);
		}
	}

	private void TryUseJumpGate()
	{
		if (_ship == null || _jumpGates.Count == 0)
			return;

		JumpGateController nearest = null;
		float bestDistance = float.MaxValue;
		foreach (JumpGateController gate in _jumpGates)
		{
			if (gate == null || !IsInstanceValid(gate))
				continue;
			float distance = _ship.GlobalPosition.DistanceTo(gate.GetActivationPosition());
			if (distance < bestDistance)
			{
				bestDistance = distance;
				nearest = gate;
			}
		}

		if (nearest == null)
			return;

		if (bestDistance > nearest.ActivationRange)
		{
			_combatHud?.SetStatus($"{nearest.DisplayName} is out of range ({bestDistance:F0}m > {nearest.ActivationRange:F0}m).");
			return;
		}

		if (string.IsNullOrWhiteSpace(nearest.DestinationSystemId))
		{
			_combatHud?.SetStatus($"{nearest.DisplayName} has no destination configured.");
			return;
		}

		StarSystemDefinition destination = StarSystemCatalog.Get(nearest.DestinationSystemId);
		GameSession.JumpToSystem(
			nearest.DestinationSystemId,
			nearest.ArrivalGateId,
			$"Transit complete: {destination.DisplayName}.");
		_ = ChangeScene(GameplayScenePath, $"Jumping to {destination.DisplayName}...");
	}

	private void ApplyArrivalPlacementIfReady()
	{
		if (string.IsNullOrWhiteSpace(GameSession.PendingArrivalGateId))
			return;

		if (_pendingArrivalFrames > 0)
		{
			_pendingArrivalFrames--;
			return;
		}

		JumpGateController arrival = FindGateById(GameSession.PendingArrivalGateId);
		if (arrival == null)
		{
			// Gate didn't materialise in the expected window — give up so we
			// don't loop forever. Ship keeps whatever spawn the scene defaulted to.
			GameSession.PendingArrivalGateId = "";
			return;
		}

		Vector3 gatePosition = arrival.GetActivationPosition();
		Basis gateBasis = arrival.GlobalTransform.Basis.Orthonormalized();
		Vector3 gateForward = (-gateBasis.Z).Normalized();
		if (gateForward.LengthSquared() < 0.001f)
			gateForward = Vector3.Forward;

		// Drop in a few hundred metres in front of the gate (on the
		// anti-sunward side the LagrangeAnchor keeps us on), slightly
		// elevated so we don't clip through the ring torus.
		Vector3 spawnPosition = gatePosition - gateForward * 360f + Vector3.Up * 8f;
		_ship.ResetForArrival(new Transform3D(gateBasis, spawnPosition));
		GameSession.PendingArrivalGateId = "";
	}

	private JumpGateController FindGateById(string gateId)
	{
		foreach (JumpGateController gate in _jumpGates)
		{
			if (gate != null && IsInstanceValid(gate) && gate.GateId == gateId)
				return gate;
		}
		return null;
	}

	public override void _PhysicsProcess(double delta)
	{
		if (_ship == null || _star == null)
			return;

		ApplyArrivalPlacementIfReady();
		ResolvePendingLaunchPlacement();
		UpdateNearestStation();
		UpdateCombat((float)delta);
		UpdateGuidance();
		UpdateDebugOverlay();

		if (_isDocking)
		{
			UpdateDocking();
			return;
		}

		if (_undockingStation != null)
		{
			UpdateUndocking();
			return;
		}

		if (_ship.Position.LengthSquared() <= OriginShiftThreshold * OriginShiftThreshold)
			return;

		Vector3 offset = _ship.Position;
		_star.Position -= offset;
		_ship.Position = Vector3.Zero;

		// Origin shift is a same-tick teleport. Without a reset, the physics
		// interpolator will lerp from the pre-shift world position to zero. The
		// ship also tracks previous frame-host positions, so keep those samples
		// in the shifted coordinate space.
		_ship.ApplyWorldOffset(offset);

		foreach (HostileEncounterController hostile in _hostiles)
		{
			if (hostile != null && IsInstanceValid(hostile))
				hostile.ApplyWorldOffset(offset);
		}

		foreach (SpaceProjectile projectile in _projectiles)
		{
			if (projectile != null && IsInstanceValid(projectile))
				projectile.ApplyWorldOffset(offset);
		}

		if (_chaseCamera != null)
			_chaseCamera.ApplyWorldOffset(offset);
	}

	public override void _UnhandledInput(InputEvent @event)
	{
		if (@event is InputEventMouseButton mouseButton &&
			mouseButton.Pressed &&
			!mouseButton.IsEcho() &&
			!_isDocking)
		{
			if (mouseButton.ButtonIndex == MouseButton.Left)
			{
				TrySelectContactAtScreenPosition(mouseButton.Position);
				return;
			}

			if (mouseButton.ButtonIndex == MouseButton.Right)
			{
				FirePlayerWeapon();
				return;
			}
		}

		if (@event is not InputEventKey key || !key.Pressed || key.Echo)
			return;

		if (key.Keycode == Key.M)
		{
			ToggleSystemMap();
			return;
		}

		if (key.Keycode == Key.F2)
		{
			ToggleDebugOverlay();
			return;
		}

		if (_systemMap?.IsOpen ?? false)
			return;

		if (key.Keycode == Key.C && !_isDocking)
		{
			ToggleComms();
			return;
		}

		if (key.Keycode == Key.Tab && !_isDocking)
		{
			CycleTarget();
			return;
		}

		if (key.Keycode == Key.L && !_isDocking)
		{
			TryUseJumpGate();
		}
	}

	private void BindStationRuntime(Node3D sectorRoot, string id, string displayName, string bodyPath, string orbitPath, string stationPath)
	{
		var station = sectorRoot.GetNode<Node3D>(stationPath);
		_stations[id] = new StationRuntime
		{
			Id = id,
			DisplayName = displayName,
			Orbit = sectorRoot.GetNode<OrbitalBody>(orbitPath),
			Model = station,
			DockApproach = station.GetNode<Marker3D>("DockApproach"),
			DockFinal = station.GetNode<Marker3D>("DockFinal"),
			LaunchPoint = station.GetNode<Marker3D>("LaunchPoint"),
		};
	}

	private ShipController BuildShip()
	{
		var shipScene = GD.Load<PackedScene>(StarterShipScenePath);
		var ship = shipScene.Instantiate<ShipController>();
		ship.Name = "PlayerShip";

		float earthPhase = Mathf.Pi * 0.3f;
		float spawnDistance = 300000f + 1000f;
		ship.Position = new Vector3(
			spawnDistance * Mathf.Cos(earthPhase),
			0f,
			spawnDistance * Mathf.Sin(earthPhase));

		AddChild(ship);
		return ship;
	}

	private void CreateCombatSpawnPoints(Node3D sectorRoot)
	{
		AddSpawnPoint(sectorRoot.GetNode<Node3D>("Star/Earth"), "Earth Watch", new Vector3(3400f, 0f, -2500f));
		AddSpawnPoint(sectorRoot.GetNode<Node3D>("Star/Mars"), "Mars Drift", new Vector3(-3100f, 0f, 1800f));
		AddSpawnPoint(sectorRoot.GetNode<Node3D>("Star/Ceres"), "Ceres Fringe", new Vector3(2400f, 0f, -1800f));
	}

	private void AddSpawnPoint(Node3D parent, string displayName, Vector3 localPosition)
	{
		var spawnPoint = new CombatSpawnPoint
		{
			Name = displayName.Replace(" ", "") + "Spawn",
			DisplayName = displayName,
			Position = localPosition,
		};
		parent.AddChild(spawnPoint);
		_combatSpawnPoints.Add(spawnPoint);
	}

	private void CreateNpcTraffic()
	{
		StationData[] npcStations =
		{
			new() { Name = "Earth Hub", Body = _stations["EarthHub"].Orbit },
			new() { Name = "Derelict Outpost", Body = _stations["DerelictOutpost"].Orbit },
			new() { Name = "Armstrong Station", Body = _stations["ArmstrongStation"].Orbit },
			new() { Name = "Mars Colony", Body = _stations["MarsColony"].Orbit },
			new() { Name = "Ceres Port", Body = _stations["CeresPort"].Orbit },
		};

		TradeRoute[] tradeRoutes =
		{
			new() { Start = 0, End = 2 },
			new() { Start = 0, End = 1 },
			new() { Start = 0, End = 3 },
			new() { Start = 2, End = 4 },
		};

		_npcManager = new NpcManager
		{
			Name = "NpcTraffic",
			ShipCount = 72,
			MaxSpawned = 48,
			SpawnDistance = 85000f,
			DespawnDistance = 120000f,
		};
		_npcManager.Setup(npcStations, tradeRoutes);
		_npcManager.SetPlayer(_ship);
		AddChild(_npcManager);
	}

	private void SetupSystemMap(Node3D sectorRoot)
	{
		_systemBodies = new[]
		{
			new CelestialBody
			{
				Name = "Mercury",
				Node = sectorRoot.GetNode<Node3D>("Star/Mercury"),
				MapColor = new Color(0.7f, 0.65f, 0.6f),
				Radius = 306f,
				OrbitRadius = 117000f,
			},
			new CelestialBody
			{
				Name = "Venus",
				Node = sectorRoot.GetNode<Node3D>("Star/Venus"),
				MapColor = new Color(0.95f, 0.85f, 0.55f),
				Radius = 760f,
				OrbitRadius = 216000f,
			},
			new CelestialBody
			{
				Name = "Earth",
				Node = sectorRoot.GetNode<Node3D>("Star/Earth"),
				MapColor = new Color(0.3f, 0.5f, 1f),
				Radius = 800f,
				OrbitRadius = 300000f,
			},
			new CelestialBody
			{
				Name = "Luna",
				Node = sectorRoot.GetNode<Node3D>("Star/Earth/Luna"),
				MapColor = new Color(0.78f, 0.78f, 0.84f),
				Radius = 218f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Mars",
				Node = sectorRoot.GetNode<Node3D>("Star/Mars"),
				MapColor = new Color(0.9f, 0.4f, 0.2f),
				Radius = 426f,
				OrbitRadius = 456000f,
			},
			new CelestialBody
			{
				Name = "Phobos",
				Node = sectorRoot.GetNode<Node3D>("Star/Mars/Phobos"),
				MapColor = new Color(0.62f, 0.58f, 0.52f),
				Radius = 24f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Deimos",
				Node = sectorRoot.GetNode<Node3D>("Star/Mars/Deimos"),
				MapColor = new Color(0.56f, 0.53f, 0.5f),
				Radius = 16f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Ceres",
				Node = sectorRoot.GetNode<Node3D>("Star/Ceres"),
				MapColor = new Color(0.7f, 0.65f, 0.55f),
				Radius = 60f,
				OrbitRadius = 831000f,
			},
			new CelestialBody
			{
				Name = "Vesta",
				Node = sectorRoot.GetNode<Node3D>("Star/Vesta"),
				MapColor = new Color(0.64f, 0.61f, 0.56f),
				Radius = 45f,
				OrbitRadius = 705000f,
			},
			new CelestialBody
			{
				Name = "Pallas",
				Node = sectorRoot.GetNode<Node3D>("Star/Pallas"),
				MapColor = new Color(0.58f, 0.56f, 0.54f),
				Radius = 40f,
				OrbitRadius = 768000f,
			},
			new CelestialBody
			{
				Name = "Hygiea",
				Node = sectorRoot.GetNode<Node3D>("Star/Hygiea"),
				MapColor = new Color(0.52f, 0.54f, 0.5f),
				Radius = 48f,
				OrbitRadius = 900000f,
			},
			new CelestialBody
			{
				Name = "Jupiter",
				Node = sectorRoot.GetNode<Node3D>("Star/Jupiter"),
				MapColor = new Color(0.9f, 0.7f, 0.4f),
				Radius = 8785f,
				OrbitRadius = 1560000f,
			},
			new CelestialBody
			{
				Name = "Io",
				Node = sectorRoot.GetNode<Node3D>("Star/Jupiter/Io"),
				MapColor = new Color(0.92f, 0.74f, 0.36f),
				Radius = 260f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Europa",
				Node = sectorRoot.GetNode<Node3D>("Star/Jupiter/Europa"),
				MapColor = new Color(0.8f, 0.84f, 0.9f),
				Radius = 245f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Ganymede",
				Node = sectorRoot.GetNode<Node3D>("Star/Jupiter/Ganymede"),
				MapColor = new Color(0.72f, 0.76f, 0.8f),
				Radius = 310f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Callisto",
				Node = sectorRoot.GetNode<Node3D>("Star/Jupiter/Callisto"),
				MapColor = new Color(0.48f, 0.48f, 0.54f),
				Radius = 285f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Saturn",
				Node = sectorRoot.GetNode<Node3D>("Star/Saturn"),
				MapColor = new Color(0.9f, 0.8f, 0.5f),
				Radius = 7317f,
				OrbitRadius = 2862000f,
			},
			new CelestialBody
			{
				Name = "Enceladus",
				Node = sectorRoot.GetNode<Node3D>("Star/Saturn/Enceladus"),
				MapColor = new Color(0.82f, 0.88f, 0.96f),
				Radius = 72f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Rhea",
				Node = sectorRoot.GetNode<Node3D>("Star/Saturn/Rhea"),
				MapColor = new Color(0.78f, 0.82f, 0.88f),
				Radius = 120f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Titan",
				Node = sectorRoot.GetNode<Node3D>("Star/Saturn/Titan"),
				MapColor = new Color(0.82f, 0.68f, 0.4f),
				Radius = 290f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Iapetus",
				Node = sectorRoot.GetNode<Node3D>("Star/Saturn/Iapetus"),
				MapColor = new Color(0.46f, 0.46f, 0.5f),
				Radius = 112f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Uranus",
				Node = sectorRoot.GetNode<Node3D>("Star/Uranus"),
				MapColor = new Color(0.5f, 0.8f, 0.9f),
				Radius = 3185f,
				OrbitRadius = 5760000f,
			},
			new CelestialBody
			{
				Name = "Miranda",
				Node = sectorRoot.GetNode<Node3D>("Star/Uranus/Miranda"),
				MapColor = new Color(0.68f, 0.76f, 0.82f),
				Radius = 58f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Ariel",
				Node = sectorRoot.GetNode<Node3D>("Star/Uranus/Ariel"),
				MapColor = new Color(0.72f, 0.8f, 0.86f),
				Radius = 92f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Umbriel",
				Node = sectorRoot.GetNode<Node3D>("Star/Uranus/Umbriel"),
				MapColor = new Color(0.5f, 0.54f, 0.62f),
				Radius = 96f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Titania",
				Node = sectorRoot.GetNode<Node3D>("Star/Uranus/Titania"),
				MapColor = new Color(0.7f, 0.78f, 0.84f),
				Radius = 125f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Oberon",
				Node = sectorRoot.GetNode<Node3D>("Star/Uranus/Oberon"),
				MapColor = new Color(0.66f, 0.74f, 0.8f),
				Radius = 120f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Neptune",
				Node = sectorRoot.GetNode<Node3D>("Star/Neptune"),
				MapColor = new Color(0.3f, 0.4f, 1f),
				Radius = 3093f,
				OrbitRadius = 9030000f,
			},
			new CelestialBody
			{
				Name = "Triton",
				Node = sectorRoot.GetNode<Node3D>("Star/Neptune/Triton"),
				MapColor = new Color(0.58f, 0.68f, 0.82f),
				Radius = 155f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Nereid",
				Node = sectorRoot.GetNode<Node3D>("Star/Neptune/Nereid"),
				MapColor = new Color(0.46f, 0.58f, 0.74f),
				Radius = 40f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Pluto",
				Node = sectorRoot.GetNode<Node3D>("Star/Pluto"),
				MapColor = new Color(0.76f, 0.72f, 0.68f),
				Radius = 95f,
				OrbitRadius = 10600000f,
			},
			new CelestialBody
			{
				Name = "Charon",
				Node = sectorRoot.GetNode<Node3D>("Star/Pluto/Charon"),
				MapColor = new Color(0.82f, 0.86f, 0.9f),
				Radius = 50f,
				OrbitRadius = 0f,
			},
			new CelestialBody
			{
				Name = "Haumea",
				Node = sectorRoot.GetNode<Node3D>("Star/Haumea"),
				MapColor = new Color(0.78f, 0.74f, 0.7f),
				Radius = 88f,
				OrbitRadius = 12000000f,
			},
			new CelestialBody
			{
				Name = "Makemake",
				Node = sectorRoot.GetNode<Node3D>("Star/Makemake"),
				MapColor = new Color(0.8f, 0.76f, 0.72f),
				Radius = 68f,
				OrbitRadius = 13200000f,
			},
			new CelestialBody
			{
				Name = "Eris",
				Node = sectorRoot.GetNode<Node3D>("Star/Eris"),
				MapColor = new Color(0.84f, 0.8f, 0.78f),
				Radius = 76f,
				OrbitRadius = 14500000f,
			},
		};

		_systemMap = new SystemMap
		{
			Name = "SystemMap",
		};
		_systemMap.Setup2(_systemBodies, _star, _ship, _hud);
		AddChild(_systemMap);
		_systemMap.SetNpcManager(_npcManager);

		var overlay = new PlanetOverlay
		{
			Name = "PlanetOverlay",
		};
		overlay.SetAnchorsPreset(Control.LayoutPreset.FullRect);
		overlay.MouseFilter = Control.MouseFilterEnum.Ignore;
		overlay.Setup(_systemBodies, _star);
		_hud.AddChild(overlay);
	}

	private void ToggleSystemMap()
	{
		if (_systemMap == null)
			return;

		_systemMap.Toggle();
		_ship.SteeringEnabled = !_systemMap.IsOpen;

		if (_systemMap.IsOpen && _commsMenu.Visible)
			_commsMenu.HideMenu();
	}

	private void ToggleComms()
	{
		if (_selectedContact == null)
		{
			_commsMenu.ShowMessage("Comms", "No contact selected.\nClick a station or contact first.");
			_ship.SteeringEnabled = false;
			return;
		}

		if (TryGetSelectedStation(out StationRuntime selectedStationForComms) && ShouldBlockDockingForHostiles(selectedStationForComms))
			_combatHud.SetStatus("Hostile nearby. Clear the area before docking.");

		if (_commsMenu.Visible)
		{
			_commsMenu.HideMenu();
			return;
		}

		if (_selectedContact.Kind == ContactKind.Station && TryGetSelectedStation(out StationRuntime selectedStation))
		{
			float distance = _ship.GlobalPosition.DistanceTo(selectedStation.Orbit.GlobalPosition);
			bool canDock = distance <= DockingCommsRange;
			_commsMenu.ShowForStation(selectedStation.DisplayName, distance, canDock);
		}
		else
		{
			_commsMenu.ShowMessage("Comms", $"{_selectedContact.DisplayName}\nNo docking services available on this contact.");
		}

		_ship.SteeringEnabled = false;
	}

	private void CreateDebugOverlay()
	{
		_debugOverlay = new CanvasLayer
		{
			Name = "DebugOverlay",
			Visible = false,
		};
		AddChild(_debugOverlay);

		var panel = new PanelContainer
		{
			OffsetLeft = 16f,
			OffsetTop = 48f,
			OffsetRight = 560f,
			OffsetBottom = 330f,
		};
		_debugOverlay.AddChild(panel);

		var margin = new MarginContainer();
		margin.AddThemeConstantOverride("margin_left", 12);
		margin.AddThemeConstantOverride("margin_top", 12);
		margin.AddThemeConstantOverride("margin_right", 12);
		margin.AddThemeConstantOverride("margin_bottom", 12);
		panel.AddChild(margin);

		_debugLabel = new Label
		{
			Name = "DebugLabel",
			AutowrapMode = TextServer.AutowrapMode.WordSmart,
		};
		_debugLabel.AddThemeColorOverride("font_color", new Color(0.88f, 0.93f, 1f, 0.96f));
		_debugLabel.AddThemeFontSizeOverride("font_size", 14);
		margin.AddChild(_debugLabel);
	}

	private void ToggleDebugOverlay()
	{
		_debugVisible = !_debugVisible;
		if (_debugOverlay != null)
			_debugOverlay.Visible = _debugVisible;
		_hud?.SetDebugVisible(_debugVisible);
	}

	private void UpdateDebugOverlay()
	{
		if (!_debugVisible || _debugLabel == null || _ship == null)
			return;

		string stationName = _nearestStation?.DisplayName ?? "None";
		float stationDistance = _nearestStation != null
			? _ship.GlobalPosition.DistanceTo(_nearestStation.Orbit.GlobalPosition)
			: 0f;
		string hostileText = TryGetClosestHostile(out HostileEncounterController hostile, out float hostileDistance)
			? $"{hostile.DisplayName} ({hostileDistance:F0} m)"
			: "None";

		_debugLabel.Text =
			"DEBUG\n" +
			"F2 toggles this panel\n\n" +
			"Controls\n" +
			"Hold W throttle up  |  Hold S throttle down\n" +
			"J supercruise  |  R phase scrambler  |  C comms  |  M map\n" +
			"Left Mouse select  |  Hold Right Mouse fire  |  Tab cycle target  |  Esc menu\n\n" +
			"Runtime\n" +
			$"Mode: {_ship.Mode}\n" +
			$"Throttle: {_ship.ThrottlePercent * 100f:F0}%\n" +
			$"Speed: {_ship.Speed:F1} u/s\n" +
			$"Supercruise offline: {(_ship.SupercruiseDriveOffline ? $"{_ship.SupercruiseOfflineTimer:F1}s" : "READY")}\n" +
			$"Interdiction lock: {_ship.InterdictionLockStrength * 100f:F0}%   |   Scrambler: {(_ship.PhaseScramblerActive ? $"ACTIVE {_ship.PhaseScramblerActiveTimer:F1}s" : (_ship.PhaseScramblerReady ? "READY" : $"{_ship.PhaseScramblerCooldownTimer:F1}s CD"))}\n" +
			$"Nearest station: {stationName} ({stationDistance:F0} m)\n" +
			$"Nearest hostile: {hostileText}\n" +
			$"Active hostiles: {CountActiveHostiles()}\n" +
			$"Projectiles: {_projectiles.Count}\n" +
			$"Weapon energy: {PlayerState.WeaponEnergy:F0}/{PlayerState.MaxWeaponEnergy:F0}\n" +
			$"Hull/Shields: {PlayerState.Hull:F0}/{PlayerState.MaxHull:F0} | {PlayerState.Shields:F0}/{PlayerState.MaxShields:F0}\n" +
			$"Credits: {PlayerState.Credits:F0}";
	}

	private Task ChangeScene(string path, string status)
	{
		if (SceneTransitionController.Instance != null)
			return SceneTransitionController.Instance.ChangeSceneWithFade(path, status);

		GetTree().ChangeSceneToFile(path);
		return Task.CompletedTask;
	}
}
