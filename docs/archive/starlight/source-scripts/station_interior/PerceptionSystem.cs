using Godot;
using System.Collections.Generic;

namespace Starlight.StationInterior;

/// <summary>
/// Static utility for NPC perception: sight checks and shared noise events.
/// Noise events are global — any NPC can poll them. Cleared each frame by CombatArena.
/// </summary>
public static class PerceptionSystem
{
    public enum NoiseType { Gunshot, Footstep, Alert }

    public struct NoiseEvent
    {
        public Vector3 Position;
        public float Radius;
        public NoiseType Type;
        public double TimeCreated;
    }

    private static readonly List<NoiseEvent> _activeNoises = new();

    /// <summary>
    /// Check if observer can see target within a sight cone and with clear LOS.
    /// </summary>
    public static bool CanSeeTarget(CharacterBody3D observer, Node3D target, float sightRange, float coneAngle)
    {
        if (target == null || !target.IsInsideTree()) return false;

        var toTarget = target.GlobalPosition - observer.GlobalPosition;
        float dist = toTarget.Length();
        if (dist > sightRange) return false;

        // Horizontal cone check
        var forward = -observer.GlobalTransform.Basis.Z;
        forward.Y = 0;
        var toTargetFlat = toTarget;
        toTargetFlat.Y = 0;
        if (toTargetFlat.LengthSquared() < 0.001f) return true; // directly above/below
        float angle = forward.AngleTo(toTargetFlat.Normalized());
        if (angle > Mathf.DegToRad(coneAngle / 2f)) return false;

        // LOS raycast
        var spaceState = observer.GetWorld3D().DirectSpaceState;
        var from = observer.GlobalPosition + new Vector3(0, 1.5f, 0); // eye height
        var to = target.GlobalPosition + new Vector3(0, 1.2f, 0);     // target center mass
        var query = PhysicsRayQueryParameters3D.Create(from, to);
        query.CollisionMask = 0b0000_1001; // environment (1) + player (8)
        query.Exclude = new Godot.Collections.Array<Rid> { observer.GetRid() };
        var result = spaceState.IntersectRay(query);

        if (result.Count == 0)
            return true; // Ray hit nothing — clear line of sight

        var collider = result["collider"].As<Node>();
        return collider == target || collider?.GetParent() == target;
    }

    /// <summary>
    /// Emit a noise event at a world position. Persists until cleared.
    /// </summary>
    public static void EmitNoise(Vector3 position, float radius, NoiseType type, double currentTime)
    {
        _activeNoises.Add(new NoiseEvent
        {
            Position = position,
            Radius = radius,
            Type = type,
            TimeCreated = currentTime,
        });
    }

    /// <summary>
    /// Get all active noise events within range of a listener position.
    /// </summary>
    public static List<NoiseEvent> GetNoisesInRange(Vector3 listenerPos, float maxRange)
    {
        var result = new List<NoiseEvent>();
        foreach (var noise in _activeNoises)
        {
            float dist = listenerPos.DistanceTo(noise.Position);
            if (dist <= noise.Radius && dist <= maxRange)
                result.Add(noise);
        }
        return result;
    }

    /// <summary>
    /// Remove noise events older than maxAge seconds. Called once per frame by CombatArena.
    /// </summary>
    public static void ClearExpiredNoises(double currentTime, float maxAge = 2.0f)
    {
        _activeNoises.RemoveAll(n => (currentTime - n.TimeCreated) > maxAge);
    }

    /// <summary>
    /// Clear all noise events (arena reset).
    /// </summary>
    public static void ClearAll()
    {
        _activeNoises.Clear();
    }
}
