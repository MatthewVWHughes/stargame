using Godot;
using System.Collections.Generic;
using Starlight.Orbital;

namespace Starlight.Npc;

/// <summary>
/// Station data: a named orbital anchor that NPCs can travel between.
/// </summary>
public struct StationData
{
    public string Name;
    public OrbitalBody Body;
}

/// <summary>
/// A direct map-visible route between two stations.
/// </summary>
public struct TradeRoute
{
    public int Start;
    public int End;
}

/// <summary>
/// Manages ambient ship traffic using data-only simulation plus a near-player visual pool.
/// Traffic routes are anchored to live station/orbit nodes, so moving planets carry their
/// local traffic with them.
/// </summary>
public partial class NpcManager : Node
{
    [Export] public int ShipCount { get; set; } = 72;
    [Export] public float SpawnDistance { get; set; } = 85000f;
    [Export] public float DespawnDistance { get; set; } = 120000f;
    [Export] public int MaxSpawned { get; set; } = 48;

    [Export] public float LocalTrafficSpeed { get; set; } = 48f;
    [Export] public float PatrolSpeed { get; set; } = 55f;
    [Export] public float MilitarySpeed { get; set; } = 60f;
    [Export] public float SupercruiseTrafficSpeed { get; set; } = 900f;
    [Export] public float RouteArrivalHoldSeconds { get; set; } = 4f;

    private const string BlockoutPath = "res://assets/ships/blockouts/";

    private enum TrafficRole
    {
        Civilian,
        Convoy,
        Military,
        Patrol,
    }

    private enum VisualKind
    {
        Fighter,
        Interceptor,
        Bomber,
        Shuttle,
        UtilityTug,
        LuxuryYacht,
        SurveyVessel,
        RepairTender,
        LightFreighter,
        MediumHauler,
        PassengerLiner,
        ContainerFreighter,
        HeavyTrader,
        MiningBarge,
        FuelTanker,
        ColonyTransport,
        Corvette,
        Destroyer,
        Frigate,
        Cruiser,
        Battleship,
    }

    private struct TrafficRoute
    {
        public int Start;
        public int End;
        public Vector3 StartOffset;
        public Vector3 EndOffset;
        public TrafficRole Role;
        public bool PlanetaryLockout;
        public float RouteSpeed;
    }

    private struct NpcShip
    {
        public int RouteIndex;
        public bool Forward;
        public TrafficRole Role;
        public VisualKind Visual;
        public Vector3 FormationOffset;
        public float TravelTime;
        public float Elapsed;
        public float PhaseOffset;
        public float HoldTimer;
        public bool HoldingAtEndpoint;
        public int PoolIndex;
    }

    private StationData[] _stations;
    private TradeRoute[] _routes;
    private TrafficRoute[] _trafficRoutes;
    private NpcShip[] _ships;
    private Node3D _player;

    private Node3D[] _pool;
    private bool[] _poolInUse;
    private bool[] _poolHasVisual;
    private VisualKind[] _poolVisualKind;
    private int _spawnedCount;

    private readonly RandomNumberGenerator _rng = new();

    public int ShipCountActual => _ships?.Length ?? 0;
    public StationData[] Stations => _stations;
    public TradeRoute[] Routes => _routes;

    public void Setup(StationData[] stations, TradeRoute[] routes)
    {
        _stations = stations;
        _routes = routes;
    }

    public void SetPlayer(Node3D player) => _player = player;

    public override void _Ready()
    {
        if (_stations == null || _stations.Length == 0 || _routes == null)
            return;

        _rng.Seed = 4107;
        _trafficRoutes = BuildTrafficRoutes();

        _pool = new Node3D[MaxSpawned];
        _poolInUse = new bool[MaxSpawned];
        _poolHasVisual = new bool[MaxSpawned];
        _poolVisualKind = new VisualKind[MaxSpawned];
        for (int i = 0; i < MaxSpawned; i++)
        {
            _pool[i] = new Node3D { Name = $"TrafficShip{i:000}", Visible = false };
            AddChild(_pool[i]);
        }

        _ships = BuildTrafficManifest();
    }

    public override void _PhysicsProcess(double delta)
    {
        if (_ships == null || _player == null || _trafficRoutes == null)
            return;

        float dt = (float)delta;
        Vector3 playerPos = _player.GlobalPosition;
        float spawnSq = SpawnDistance * SpawnDistance;
        float despawnSq = DespawnDistance * DespawnDistance;

        for (int i = 0; i < _ships.Length; i++)
        {
            ref NpcShip ship = ref _ships[i];

            if (ship.HoldingAtEndpoint)
            {
                ship.HoldTimer -= dt;
                if (ship.HoldTimer <= 0f)
                {
                    if (ship.PoolIndex >= 0)
                    {
                        ReleasePoolSlot(ship.PoolIndex);
                        ship.PoolIndex = -1;
                    }

                    AssignNextRoute(ref ship);
                    continue;
                }
            }
            else
            {
                ship.Elapsed += dt;
                if (ship.Elapsed >= ship.TravelTime)
                {
                    ship.Elapsed = ship.TravelTime;
                    ship.HoldingAtEndpoint = true;
                    ship.HoldTimer = RouteArrivalHoldSeconds + _rng.RandfRange(-0.8f, 1.2f);
                }
            }

            GetShipPose(ship, out Vector3 worldPos, out Vector3 routeDirection);
            float distSq = playerPos.DistanceSquaredTo(worldPos);
            bool isSpawned = ship.PoolIndex >= 0;

            if (!isSpawned && distSq < spawnSq && _spawnedCount < MaxSpawned)
            {
                int poolIdx = AcquirePoolSlot();
                if (poolIdx < 0)
                    continue;

                ship.PoolIndex = poolIdx;
                EnsurePoolVisual(poolIdx, ship.Visual);
                _pool[poolIdx].GlobalPosition = worldPos;
                _pool[poolIdx].Visible = true;
                FaceDirection(_pool[poolIdx], routeDirection);
            }
            else if (isSpawned && distSq > despawnSq)
            {
                ReleasePoolSlot(ship.PoolIndex);
                ship.PoolIndex = -1;
            }
            else if (isSpawned)
            {
                _pool[ship.PoolIndex].GlobalPosition = worldPos;
                FaceDirection(_pool[ship.PoolIndex], routeDirection);
            }
        }
    }

    public Vector3 GetShipWorldPosition(int index)
    {
        if (_ships == null || index < 0 || index >= _ships.Length)
            return Vector3.Zero;

        GetShipPose(_ships[index], out Vector3 worldPos, out _);
        return worldPos;
    }

    private TrafficRoute[] BuildTrafficRoutes()
    {
        var routes = new List<TrafficRoute>();

        AddRoute(routes, 0, 2, TrafficRole.Convoy, true, new Vector3(0f, 180f, -1500f), new Vector3(0f, 140f, 1100f), LocalTrafficSpeed);
        AddRoute(routes, 0, 1, TrafficRole.Civilian, true, new Vector3(1450f, 160f, 280f), new Vector3(-1050f, 130f, 840f), LocalTrafficSpeed);
        AddRoute(routes, 2, 0, TrafficRole.Civilian, true, new Vector3(860f, 120f, -720f), new Vector3(-1160f, 150f, 680f), LocalTrafficSpeed);
        AddRoute(routes, 0, 0, TrafficRole.Patrol, true, new Vector3(-3800f, 420f, -2300f), new Vector3(4200f, 360f, 2500f), PatrolSpeed);
        AddRoute(routes, 0, 0, TrafficRole.Patrol, true, new Vector3(3600f, -260f, -3400f), new Vector3(-3400f, -180f, 3600f), PatrolSpeed);
        AddRoute(routes, 2, 2, TrafficRole.Patrol, true, new Vector3(-1900f, 240f, -1300f), new Vector3(2100f, 220f, 1380f), PatrolSpeed);
        AddRoute(routes, 0, 1, TrafficRole.Military, true, new Vector3(-5200f, 760f, -3200f), new Vector3(3600f, 500f, 2400f), MilitarySpeed);
        AddRoute(routes, 0, 2, TrafficRole.Military, true, new Vector3(4600f, 520f, 3000f), new Vector3(-3200f, 420f, -2200f), MilitarySpeed);

        // Longer trade lanes may cruise between gravity wells, but they still
        // spend more time on approach/departure and never use cruise inside the
        // local planetary legs.
        AddRoute(routes, 0, 3, TrafficRole.Convoy, false, new Vector3(-1800f, 240f, 720f), new Vector3(1300f, 180f, -950f), SupercruiseTrafficSpeed);
        AddRoute(routes, 2, 4, TrafficRole.Convoy, false, new Vector3(1300f, 180f, -980f), new Vector3(-1300f, 120f, 980f), SupercruiseTrafficSpeed);

        return routes.ToArray();
    }

    private static void AddRoute(
        List<TrafficRoute> routes,
        int start,
        int end,
        TrafficRole role,
        bool planetaryLockout,
        Vector3 startOffset,
        Vector3 endOffset,
        float routeSpeed)
    {
        routes.Add(new TrafficRoute
        {
            Start = start,
            End = end,
            Role = role,
            PlanetaryLockout = planetaryLockout,
            StartOffset = startOffset,
            EndOffset = endOffset,
            RouteSpeed = routeSpeed,
        });
    }

    private NpcShip[] BuildTrafficManifest()
    {
        var ships = new List<NpcShip>(ShipCount);

        AddConvoy(ships, 0, new[] { VisualKind.ContainerFreighter, VisualKind.MediumHauler, VisualKind.LightFreighter }, 0.10f);
        AddConvoy(ships, 0, new[] { VisualKind.FuelTanker, VisualKind.UtilityTug }, 0.48f);
        AddConvoy(ships, 8, new[] { VisualKind.HeavyTrader, VisualKind.MiningBarge, VisualKind.Corvette }, 0.28f);
        AddConvoy(ships, 2, new[] { VisualKind.PassengerLiner, VisualKind.Shuttle }, 0.68f);

        AddFormation(ships, 6, new[] { VisualKind.Destroyer, VisualKind.Corvette, VisualKind.Fighter, VisualKind.Fighter }, 0.18f);
        AddFormation(ships, 7, new[] { VisualKind.Cruiser, VisualKind.Frigate, VisualKind.Fighter }, 0.72f);
        AddFormation(ships, 4, new[] { VisualKind.Corvette, VisualKind.Interceptor, VisualKind.Interceptor }, 0.38f);

        while (ships.Count < ShipCount)
        {
            TrafficRole role = _rng.Randf() < 0.78f ? TrafficRole.Civilian : TrafficRole.Patrol;
            int routeIndex = PickRouteForRole(role);
            VisualKind visual = PickVisual(role);
            float travel = EstimateTravelTime(routeIndex, visual);
            ships.Add(new NpcShip
            {
                RouteIndex = routeIndex,
                Forward = _rng.Randf() > 0.5f,
                Role = role,
                Visual = visual,
                FormationOffset = RandomLooseOffset(role),
                TravelTime = travel,
                Elapsed = PickQuietStartingProgress(ships.Count) * travel,
                PhaseOffset = _rng.Randf() * travel,
                HoldTimer = 0f,
                HoldingAtEndpoint = false,
                PoolIndex = -1,
            });
        }

        return ships.ToArray();
    }

    private void AddConvoy(List<NpcShip> ships, int routeIndex, VisualKind[] visuals, float routeProgress)
    {
        for (int i = 0; i < visuals.Length; i++)
        {
            float travel = EstimateTravelTime(routeIndex, visuals[i]);
            ships.Add(new NpcShip
            {
                RouteIndex = routeIndex,
                Forward = true,
                Role = TrafficRole.Convoy,
                Visual = visuals[i],
                FormationOffset = new Vector3((i % 2 == 0 ? -1f : 1f) * (70f + i * 24f), 0f, i * 360f),
                TravelTime = travel,
                Elapsed = Mathf.Clamp(routeProgress + i * 0.018f, 0f, 0.95f) * travel,
                PhaseOffset = i * 2.3f,
                HoldTimer = 0f,
                HoldingAtEndpoint = false,
                PoolIndex = -1,
            });
        }
    }

    private void AddFormation(List<NpcShip> ships, int routeIndex, VisualKind[] visuals, float routeProgress)
    {
        Vector3[] offsets =
        {
            Vector3.Zero,
            new Vector3(-140f, 24f, 360f),
            new Vector3(140f, 24f, 360f),
            new Vector3(-240f, -18f, 720f),
            new Vector3(240f, -18f, 720f),
            new Vector3(0f, 60f, 1080f),
        };

        for (int i = 0; i < visuals.Length; i++)
        {
            float travel = EstimateTravelTime(routeIndex, visuals[i]);
            ships.Add(new NpcShip
            {
                RouteIndex = routeIndex,
                Forward = true,
                Role = TrafficRole.Military,
                Visual = visuals[i],
                FormationOffset = offsets[Mathf.Min(i, offsets.Length - 1)],
                TravelTime = travel,
                Elapsed = Mathf.Clamp(routeProgress + i * 0.006f, 0f, 0.98f) * travel,
                PhaseOffset = i * 1.1f,
                HoldTimer = 0f,
                HoldingAtEndpoint = false,
                PoolIndex = -1,
            });
        }
    }

    private void AssignNextRoute(ref NpcShip ship)
    {
        int newRoute = PickRouteForRole(ship.Role);
        ship.RouteIndex = newRoute;
        ship.Forward = _rng.Randf() > 0.5f;
        ship.TravelTime = EstimateTravelTime(newRoute, ship.Visual);
        ship.Elapsed = ship.PhaseOffset % ship.TravelTime;
        ship.HoldTimer = 0f;
        ship.HoldingAtEndpoint = false;
    }

    private int PickRouteForRole(TrafficRole role)
    {
        var candidates = new List<int>();
        for (int i = 0; i < _trafficRoutes.Length; i++)
        {
            if (_trafficRoutes[i].Role == role)
                candidates.Add(i);
        }

        if (candidates.Count == 0)
            return _rng.RandiRange(0, _trafficRoutes.Length - 1);

        return candidates[_rng.RandiRange(0, candidates.Count - 1)];
    }

    private VisualKind PickVisual(TrafficRole role)
    {
        VisualKind[] options = role switch
        {
            TrafficRole.Patrol => new[] { VisualKind.Fighter, VisualKind.Interceptor, VisualKind.Corvette, VisualKind.SurveyVessel },
            TrafficRole.Military => new[] { VisualKind.Fighter, VisualKind.Interceptor, VisualKind.Bomber, VisualKind.Corvette, VisualKind.Destroyer },
            TrafficRole.Convoy => new[] { VisualKind.ContainerFreighter, VisualKind.MediumHauler, VisualKind.HeavyTrader, VisualKind.FuelTanker, VisualKind.MiningBarge },
            _ => new[] { VisualKind.Shuttle, VisualKind.UtilityTug, VisualKind.LuxuryYacht, VisualKind.SurveyVessel, VisualKind.RepairTender, VisualKind.LightFreighter, VisualKind.PassengerLiner },
        };

        return options[_rng.RandiRange(0, options.Length - 1)];
    }

    private float EstimateTravelTime(int routeIndex, VisualKind visual)
    {
        TrafficRoute route = _trafficRoutes[routeIndex];
        Vector3 a = GetStationPos(route.Start) + route.StartOffset;
        Vector3 b = GetStationPos(route.End) + route.EndOffset;
        float distance = a.DistanceTo(b);
        float speed = route.PlanetaryLockout
            ? Mathf.Min(route.RouteSpeed, LocalTrafficSpeed * GetVisualSpeedFactor(visual))
            : EstimateCruiseAwareSpeed(route, distance, visual);

        return Mathf.Max(distance / Mathf.Max(speed, 1f), 18f);
    }

    private float EstimateCruiseAwareSpeed(TrafficRoute route, float distance, VisualKind visual)
    {
        float visualFactor = GetVisualSpeedFactor(visual);
        float approachDistance = 18000f;
        float localLegs = Mathf.Min(distance, approachDistance * 2f);
        float cruiseLeg = Mathf.Max(distance - localLegs, 0f);
        float localSpeed = LocalTrafficSpeed * visualFactor;
        float cruiseSpeed = route.RouteSpeed * Mathf.Clamp(visualFactor, 0.38f, 1.2f);
        float seconds = localLegs / Mathf.Max(localSpeed, 1f) + cruiseLeg / Mathf.Max(cruiseSpeed, 1f);
        return distance / Mathf.Max(seconds, 1f);
    }

    private static float GetVisualSpeedFactor(VisualKind visual)
    {
        return visual switch
        {
            VisualKind.Battleship => 0.22f,
            VisualKind.Cruiser => 0.32f,
            VisualKind.Destroyer => 0.48f,
            VisualKind.Frigate => 0.6f,
            VisualKind.Corvette => 0.78f,
            VisualKind.HeavyTrader or VisualKind.FuelTanker or VisualKind.MiningBarge or VisualKind.ColonyTransport => 0.42f,
            VisualKind.ContainerFreighter or VisualKind.PassengerLiner => 0.54f,
            VisualKind.MediumHauler or VisualKind.RepairTender => 0.68f,
            VisualKind.LightFreighter or VisualKind.UtilityTug or VisualKind.SurveyVessel => 0.82f,
            VisualKind.Fighter or VisualKind.Interceptor => 1.18f,
            VisualKind.Bomber => 0.92f,
            _ => 1f,
        };
    }

    private void GetShipPose(NpcShip ship, out Vector3 worldPos, out Vector3 routeDirection)
    {
        TrafficRoute route = _trafficRoutes[ship.RouteIndex];
        Vector3 a = GetStationPos(route.Start) + route.StartOffset;
        Vector3 b = GetStationPos(route.End) + route.EndOffset;
        Vector3 start = ship.Forward ? a : b;
        Vector3 end = ship.Forward ? b : a;

        float t = Mathf.Clamp(ship.Elapsed / ship.TravelTime, 0f, 1f);
        float progress = route.PlanetaryLockout
            ? SmoothApproachProgress(t)
            : CruiseAwareProgress(t);

        routeDirection = end - start;
        worldPos = start.Lerp(end, progress) + BuildFormationOffset(routeDirection, ship.FormationOffset);
    }

    private static float SmoothApproachProgress(float t)
    {
        return t * t * (3f - 2f * t);
    }

    private static float CruiseAwareProgress(float t)
    {
        if (t < 0.18f)
            return 0.16f * SmoothApproachProgress(t / 0.18f);
        if (t > 0.82f)
            return 0.84f + 0.16f * SmoothApproachProgress((t - 0.82f) / 0.18f);
        return Mathf.Lerp(0.16f, 0.84f, (t - 0.18f) / 0.64f);
    }

    private static Vector3 BuildFormationOffset(Vector3 routeDirection, Vector3 localOffset)
    {
        if (localOffset.LengthSquared() < 0.001f)
            return Vector3.Zero;

        Vector3 forward = routeDirection.LengthSquared() > 0.01f ? routeDirection.Normalized() : Vector3.Forward;
        Vector3 up = Mathf.Abs(forward.Dot(Vector3.Up)) > 0.96f ? Vector3.Right : Vector3.Up;
        Vector3 right = up.Cross(forward).Normalized();
        up = forward.Cross(right).Normalized();
        return right * localOffset.X + up * localOffset.Y + forward * localOffset.Z;
    }

    private Vector3 RandomLooseOffset(TrafficRole role)
    {
        float width = role == TrafficRole.Patrol ? 260f : 90f;
        float height = role == TrafficRole.Patrol ? 80f : 45f;
        float trail = role == TrafficRole.Patrol ? 420f : 240f;
        return new Vector3(
            _rng.RandfRange(-width, width),
            _rng.RandfRange(-height, height),
            _rng.RandfRange(-trail, trail));
    }

    private float PickQuietStartingProgress(int shipIndex)
    {
        int laneSlot = shipIndex % 18;
        float evenSpacing = (laneSlot + 0.5f) / 18f;
        float jitter = _rng.RandfRange(-0.012f, 0.012f);
        return Mathf.PosMod(evenSpacing + jitter, 1f);
    }

    private Vector3 GetStationPos(int stationIndex)
    {
        return _stations[stationIndex].Body.GlobalPosition;
    }

    private static void FaceDirection(Node3D node, Vector3 direction)
    {
        if (direction.LengthSquared() < 0.01f) return;
        direction = direction.Normalized();
        Vector3 up = Vector3.Up;
        if (Mathf.Abs(direction.Dot(up)) > 0.99f)
            up = Vector3.Right;
        node.LookAt(node.GlobalPosition + direction, up);
    }

    private int AcquirePoolSlot()
    {
        for (int i = 0; i < _pool.Length; i++)
        {
            if (_poolInUse[i])
                continue;

            _poolInUse[i] = true;
            _spawnedCount++;
            return i;
        }

        return -1;
    }

    private void ReleasePoolSlot(int index)
    {
        if (index < 0 || index >= _pool.Length) return;
        _poolInUse[index] = false;
        _pool[index].Visible = false;
        _spawnedCount--;
    }

    private void EnsurePoolVisual(int poolIndex, VisualKind visual)
    {
        if (_poolHasVisual[poolIndex] && _poolVisualKind[poolIndex] == visual)
            return;

        Node3D holder = _pool[poolIndex];
        foreach (Node child in holder.GetChildren())
        {
            holder.RemoveChild(child);
            child.QueueFree();
        }

        PackedScene scene = GD.Load<PackedScene>(GetVisualPath(visual));
        Node3D model = scene?.Instantiate<Node3D>() ?? CreateFallbackVisual(visual);
        model.Name = visual.ToString();
        // Blender authored these blockouts lengthwise on Z. The glTF import
        // converts that into Godot's Y axis, so rotate the visual child so the
        // ship's nose follows the traffic node's local -Z forward.
        model.Rotation = new Vector3(Mathf.Pi * 0.5f, 0f, 0f);
        holder.AddChild(model);
        AddVisibilityLight(holder, visual);

        _poolHasVisual[poolIndex] = true;
        _poolVisualKind[poolIndex] = visual;
    }

    private static string GetVisualPath(VisualKind visual)
    {
        return visual switch
        {
            VisualKind.Battleship => BlockoutPath + "battleship_blockout_1000m.glb",
            VisualKind.Cruiser => BlockoutPath + "cruiser_blockout_600m.glb",
            VisualKind.Destroyer => BlockoutPath + "destroyer_blockout_300m.glb",
            VisualKind.Frigate => BlockoutPath + "frigate_blockout_180m.glb",
            VisualKind.Corvette => BlockoutPath + "corvette_blockout_100m.glb",
            VisualKind.HeavyTrader => BlockoutPath + "heavy_bulk_trader_blockout_200m.glb",
            VisualKind.ContainerFreighter => BlockoutPath + "container_freighter_blockout_150m.glb",
            VisualKind.MediumHauler => BlockoutPath + "medium_hauler_blockout_100m.glb",
            VisualKind.LightFreighter => BlockoutPath + "light_courier_freighter_blockout_50m.glb",
            VisualKind.PassengerLiner => BlockoutPath + "passenger_liner_blockout_120m.glb",
            VisualKind.Fighter => BlockoutPath + "fighter_blockout_18m.glb",
            VisualKind.Interceptor => BlockoutPath + "interceptor_blockout_14m.glb",
            VisualKind.Bomber => BlockoutPath + "bomber_blockout_32m.glb",
            VisualKind.Shuttle => BlockoutPath + "shuttle_blockout_25m.glb",
            VisualKind.UtilityTug => BlockoutPath + "utility_tug_blockout_35m.glb",
            VisualKind.LuxuryYacht => BlockoutPath + "luxury_yacht_blockout_45m.glb",
            VisualKind.SurveyVessel => BlockoutPath + "survey_vessel_blockout_70m.glb",
            VisualKind.RepairTender => BlockoutPath + "repair_tender_blockout_80m.glb",
            VisualKind.MiningBarge => BlockoutPath + "mining_barge_blockout_180m.glb",
            VisualKind.FuelTanker => BlockoutPath + "fuel_tanker_blockout_220m.glb",
            VisualKind.ColonyTransport => BlockoutPath + "colony_transport_blockout_260m.glb",
            _ => BlockoutPath + "shuttle_blockout_25m.glb",
        };
    }

    private static void AddVisibilityLight(Node3D holder, VisualKind visual)
    {
        float range = visual is VisualKind.Battleship or VisualKind.Cruiser or VisualKind.Destroyer ? 140f : 48f;
        float energy = visual is VisualKind.Battleship or VisualKind.Cruiser ? 0.5f : 0.22f;
        holder.AddChild(new OmniLight3D
        {
            Name = "MarkerLight",
            LightColor = new Color(0.55f, 0.68f, 1f),
            LightEnergy = energy,
            OmniRange = range,
            OmniAttenuation = 1.4f,
        });
    }

    private static Node3D CreateFallbackVisual(VisualKind visual)
    {
        Vector3 size = visual is VisualKind.Battleship ? new Vector3(90f, 40f, 1000f) : new Vector3(12f, 6f, 45f);
        var ship = new Node3D();
        ship.AddChild(new MeshInstance3D
        {
            Mesh = new BoxMesh { Size = size },
            MaterialOverride = new StandardMaterial3D { AlbedoColor = new Color(0.45f, 0.5f, 0.6f) },
        });
        return ship;
    }
}
