using Godot;

namespace Starlight.Game.Combat;

public enum CombatFaction
{
    Neutral,
    Player,
    Hostile,
}

public enum CombatPayloadType
{
    Damage,
    Interdiction,
}

public enum CombatVisualStyle
{
    Bolt,
    Beam,
}

public interface ISpaceCombatant
{
    string CombatDisplayName { get; }
    CombatFaction Faction { get; }
    Vector3 CombatOrigin { get; }
    Vector3 CombatVelocity { get; }
    float CollisionRadius { get; }
    bool IsAlive { get; }
    void ApplyCombatDamage(float amount, Node3D source);
}

public interface IInterdictableCombatant
{
    void ApplyInterdiction(float durationSeconds, Node3D source);
}

public readonly record struct ProjectileFireRequest(
    Transform3D MuzzleTransform,
    Vector3 Direction,
    CombatFaction Faction,
    float Damage,
    float Speed,
    float MaxDistance,
    Vector3 InheritedVelocity,
    Color Tint,
    CombatPayloadType PayloadType = CombatPayloadType.Damage,
    float EffectDuration = 0f,
    CombatVisualStyle VisualStyle = CombatVisualStyle.Bolt,
    string SourceDisplayName = "");

public static class SpaceCombatMath
{
    public static bool TryGetInterceptPoint(
        Vector3 shooterPosition,
        Vector3 shooterVelocity,
        Vector3 targetPosition,
        Vector3 targetVelocity,
        float projectileSpeed,
        out Vector3 interceptPoint)
    {
        Vector3 relativePosition = targetPosition - shooterPosition;
        Vector3 relativeVelocity = targetVelocity - shooterVelocity;

        float a = relativeVelocity.LengthSquared() - projectileSpeed * projectileSpeed;
        float b = 2f * relativePosition.Dot(relativeVelocity);
        float c = relativePosition.LengthSquared();

        float time;
        if (Mathf.Abs(a) < 0.0001f)
        {
            if (Mathf.Abs(b) < 0.0001f)
            {
                interceptPoint = targetPosition;
                return false;
            }

            time = -c / b;
        }
        else
        {
            float discriminant = b * b - 4f * a * c;
            if (discriminant < 0f)
            {
                interceptPoint = targetPosition;
                return false;
            }

            float sqrtDiscriminant = Mathf.Sqrt(discriminant);
            float t0 = (-b - sqrtDiscriminant) / (2f * a);
            float t1 = (-b + sqrtDiscriminant) / (2f * a);
            time = PickSmallestPositiveTime(t0, t1);
        }

        if (time <= 0f || float.IsNaN(time) || float.IsInfinity(time))
        {
            interceptPoint = targetPosition;
            return false;
        }

        interceptPoint = targetPosition + targetVelocity * time;
        return true;
    }

    private static float PickSmallestPositiveTime(float a, float b)
    {
        bool aValid = a > 0f && !float.IsNaN(a) && !float.IsInfinity(a);
        bool bValid = b > 0f && !float.IsNaN(b) && !float.IsInfinity(b);

        if (aValid && bValid)
            return Mathf.Min(a, b);
        if (aValid)
            return a;
        if (bValid)
            return b;

        return -1f;
    }
}
