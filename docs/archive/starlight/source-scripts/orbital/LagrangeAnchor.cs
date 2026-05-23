using Godot;

namespace Starlight.Orbital;

/// <summary>
/// Keeps this node anchored at a Lagrange-like offset relative to a planet and the star.
/// Typically used for L2 jump gates — the node sits on the anti-sunward side of the planet
/// at a fixed in-game distance and follows the planet's Kepler orbit automatically.
/// </summary>
public partial class LagrangeAnchor : Node3D
{
    public enum LagrangePoint
    {
        L1,
        L2,
    }

    [Export] public NodePath PlanetPath { get; set; }
    [Export] public NodePath StarPath { get; set; }
    [Export] public float Distance { get; set; } = 4000f;
    [Export] public LagrangePoint Point { get; set; } = LagrangePoint.L2;

    private Node3D _planet;
    private Node3D _star;

    public override void _Ready()
    {
        if (PlanetPath != null && !PlanetPath.IsEmpty)
            _planet = GetNodeOrNull<Node3D>(PlanetPath);
        if (StarPath != null && !StarPath.IsEmpty)
            _star = GetNodeOrNull<Node3D>(StarPath);
    }

    // Used by StarSystemBuilder after the anchor is in the tree: the builder
    // resolves planet/star nodes by logical path and hands them to us directly,
    // bypassing the NodePath round-trip.
    public void SetTargets(Node3D planet, Node3D star)
    {
        _planet = planet;
        _star = star;
    }

    public override void _PhysicsProcess(double delta)
    {
        // Planets/stars orbit in OrbitalBody._PhysicsProcess, so the anchor
        // must read and update in the same phase to avoid a one-frame stale
        // reference (previously this ran in _Process and occasionally lagged
        // the planet position by a tick).
        if (_planet == null || _star == null)
            return;

        if (TryComputeLagrangePosition(
            _planet.GlobalPosition, _star.GlobalPosition, Distance, Point,
            out Vector3 position))
        {
            GlobalPosition = position;
        }
    }

    /// <summary>
    /// Pure Lagrange-point offset math. Returns the world position at distance
    /// <paramref name="distance"/> along the anti-sunward (L2) or sunward (L1)
    /// direction from the planet. Returns false for degenerate cases
    /// (planet and star coincident).
    /// </summary>
    public static bool TryComputeLagrangePosition(
        Vector3 planetPosition,
        Vector3 starPosition,
        float distance,
        LagrangePoint point,
        out Vector3 position)
    {
        Vector3 radial = planetPosition - starPosition;
        if (radial.LengthSquared() < 0.0001f)
        {
            position = planetPosition;
            return false;
        }

        Vector3 direction = radial.Normalized();
        if (point == LagrangePoint.L1)
            direction = -direction;

        position = planetPosition + direction * distance;
        return true;
    }
}
