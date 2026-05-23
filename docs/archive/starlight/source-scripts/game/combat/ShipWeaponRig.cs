using Godot;
using System.Collections.Generic;

namespace Starlight.Game.Combat;

/// <summary>
/// Small helper that rotates attached barrels toward a world-space aim point.
/// </summary>
public partial class ShipWeaponRig : Node3D
{
    [Export] public float TurnSpeed { get; set; } = 12f;
    [Export] public float MaxAimAngleDegrees { get; set; } = 18f;

    private readonly List<Marker3D> _muzzles = new();
    private int _nextMuzzleIndex;

    public override void _Ready()
    {
        _muzzles.Clear();
        CollectMuzzles(this);
    }

    public void AimAt(Vector3 worldPoint, float delta)
    {
        Vector3 localTarget = ClampToAimCone(ToLocal(worldPoint));
        if (localTarget.LengthSquared() <= 0.001f)
            return;

        Basis currentBasis = Basis.Orthonormalized();
        Basis targetBasis = Basis.LookingAt(localTarget.Normalized(), Vector3.Up).Orthonormalized();
        Basis = currentBasis.Slerp(targetBasis, Mathf.Clamp(TurnSpeed * delta, 0f, 1f)).Orthonormalized();
    }

    public float GetAimAlignment(Vector3 worldPoint)
    {
        Vector3 desiredDirection = (worldPoint - GlobalPosition).Normalized();
        if (desiredDirection.LengthSquared() <= 0.001f)
            return 0f;

        return (-GlobalTransform.Basis.Z).Dot(desiredDirection);
    }

    public Transform3D GetNextMuzzleTransform()
    {
        if (_muzzles.Count == 0)
            return GlobalTransform;

        Marker3D muzzle = _muzzles[_nextMuzzleIndex % _muzzles.Count];
        _nextMuzzleIndex = (_nextMuzzleIndex + 1) % _muzzles.Count;
        return muzzle.GlobalTransform;
    }

    private void CollectMuzzles(Node node)
    {
        foreach (Node child in node.GetChildren())
        {
            if (child is Marker3D marker && marker.Name.ToString().Contains("Muzzle"))
                _muzzles.Add(marker);

            CollectMuzzles(child);
        }
    }

    private Vector3 ClampToAimCone(Vector3 localTarget)
    {
        if (localTarget.LengthSquared() <= 0.001f)
            return localTarget;

        Vector3 dir = localTarget.Normalized();
        float forwardDepth = Mathf.Max(-dir.Z, 0.001f);
        Vector2 lateral = new(dir.X, dir.Y);
        float lateralLength = lateral.Length();
        float maxLateral = forwardDepth * Mathf.Tan(Mathf.DegToRad(MaxAimAngleDegrees));
        if (lateralLength <= maxLateral || lateralLength <= 0.0001f)
            return dir;

        lateral *= maxLateral / lateralLength;
        return new Vector3(lateral.X, lateral.Y, -forwardDepth).Normalized();
    }
}
