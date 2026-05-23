using System;

namespace Starlight.Npc;

/// <summary>
/// Tier 3 sector simulation — pure data, no 3D nodes, no rendering.
/// Ships are just timers traveling between abstract station indices.
/// Simulates an entire sector's NPC traffic with near-zero CPU cost.
/// </summary>
public class DistantSector
{
    public string Name { get; }
    public int ShipCount => _ships.Length;
    public int StationCount { get; }
    public int RouteCount { get; }

    // Stats for monitoring.
    public int ShipsInTransit { get; private set; }
    public int ShipsDocked { get; private set; }
    public int TotalArrivals { get; private set; }

    private struct DistantShip
    {
        public int RouteIndex;
        public bool Forward;
        public float TravelTime;
        public float Elapsed;
        public bool Docked;
        public float DockTimer;
    }

    private struct DistantRoute
    {
        public float TravelTime; // pre-computed, fixed for distant sectors
    }

    private DistantShip[] _ships;
    private DistantRoute[] _routes;
    private readonly Random _rng = new();

    private float RandFloat() => (float)_rng.NextDouble();
    private float RandFloatRange(float min, float max) => min + RandFloat() * (max - min);
    private int RandIntRange(int min, int max) => _rng.Next(min, max + 1); // inclusive max

    /// <summary>
    /// Create a distant sector simulation.
    /// </summary>
    /// <param name="name">Sector name for display.</param>
    /// <param name="stationCount">Number of stations in this sector.</param>
    /// <param name="routeCount">Number of trade routes.</param>
    /// <param name="shipCount">Number of NPC ships.</param>
    /// <param name="minTravelTime">Shortest route time in seconds.</param>
    /// <param name="maxTravelTime">Longest route time in seconds.</param>
    public DistantSector(string name, int stationCount, int routeCount, int shipCount,
        float minTravelTime = 60f, float maxTravelTime = 600f)
    {
        Name = name;
        StationCount = stationCount;
        RouteCount = routeCount;

        // Generate routes with random travel times.
        _routes = new DistantRoute[routeCount];
        for (int i = 0; i < routeCount; i++)
            _routes[i].TravelTime = RandFloatRange(minTravelTime, maxTravelTime);

        // Generate ships at random progress on random routes.
        _ships = new DistantShip[shipCount];
        for (int i = 0; i < shipCount; i++)
        {
            int route = RandIntRange(0, routeCount - 1);
            float tt = _routes[route].TravelTime;
            _ships[i] = new DistantShip
            {
                RouteIndex = route,
                Forward = RandFloat() > 0.5f,
                TravelTime = tt,
                Elapsed = RandFloat() * tt,
                Docked = false,
                DockTimer = 0f,
            };
        }
    }

    /// <summary>
    /// Tick the sector. Call once per frame or at a reduced rate.
    /// </summary>
    public void Update(float dt)
    {
        int inTransit = 0;
        int docked = 0;

        for (int i = 0; i < _ships.Length; i++)
        {
            ref var ship = ref _ships[i];

            if (ship.Docked)
            {
                // Ship is docked — wait, then depart on a new route.
                ship.DockTimer -= dt;
                if (ship.DockTimer <= 0f)
                {
                    ship.Docked = false;
                    ship.RouteIndex = RandIntRange(0, _routes.Length - 1);
                    ship.Forward = !ship.Forward;
                    ship.TravelTime = _routes[ship.RouteIndex].TravelTime;
                    ship.Elapsed = 0f;
                }
                else
                {
                    docked++;
                }
                continue;
            }

            // In transit.
            ship.Elapsed += dt;
            if (ship.Elapsed >= ship.TravelTime)
            {
                // Arrived — dock for a bit.
                ship.Docked = true;
                ship.DockTimer = RandFloatRange(10f, 60f);
                TotalArrivals++;
                docked++;
            }
            else
            {
                inTransit++;
            }
        }

        ShipsInTransit = inTransit;
        ShipsDocked = docked;
    }
}
