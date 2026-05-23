using Godot;
using System.Collections.Generic;

namespace Starlight.Game.Combat;

/// <summary>
/// Lightweight visual projectile that performs cheap segment-vs-sphere hit checks.
/// </summary>
public partial class SpaceProjectile : Node3D
{
    [Export] public float Speed { get; set; } = 680f;
    [Export] public float Damage { get; set; } = 12f;
    [Export] public float MaxDistance { get; set; } = 900f;
    [Export] public float CollisionRadius { get; set; } = 1.5f;

    public CombatFaction SourceFaction { get; private set; } = CombatFaction.Neutral;
    public CombatPayloadType PayloadType { get; private set; } = CombatPayloadType.Damage;
    public float EffectDuration { get; private set; }
    public string SourceDisplayName { get; private set; } = "";

    private Vector3 _direction = Vector3.Forward;
    private Vector3 _inheritedVelocity;
    private float _travelled;
    private CombatVisualStyle _visualStyle = CombatVisualStyle.Bolt;

    public void Configure(ProjectileFireRequest request)
    {
        GlobalTransform = request.MuzzleTransform;
        Speed = request.Speed;
        Damage = request.Damage;
        MaxDistance = request.MaxDistance;
        SourceFaction = request.Faction;
        PayloadType = request.PayloadType;
        EffectDuration = request.EffectDuration;
        SourceDisplayName = request.SourceDisplayName;
        _visualStyle = request.VisualStyle;
        _direction = request.Direction.Normalized();
        _inheritedVelocity = request.InheritedVelocity;
        if (_direction.LengthSquared() <= 0.001f)
            _direction = -request.MuzzleTransform.Basis.Z;

        BuildVisual(request.Tint);
        LookAt(GlobalPosition + _direction, Vector3.Up);
    }

    public bool Advance(float delta, IReadOnlyList<ISpaceCombatant> combatants)
    {
        Vector3 start = GlobalPosition;
        Vector3 end = start + (_direction * Speed + _inheritedVelocity) * delta;

        ISpaceCombatant hitCombatant = null;
        float bestT = float.MaxValue;

        if (combatants != null)
        {
            foreach (ISpaceCombatant combatant in combatants)
            {
                if (combatant == null || !combatant.IsAlive || combatant.Faction == SourceFaction)
                    continue;

                if (!TryIntersectSegmentSphere(
                        start,
                        end,
                        combatant.CombatOrigin,
                        combatant.CollisionRadius + CollisionRadius,
                        out float hitT))
                {
                    continue;
                }

                if (hitT >= bestT)
                    continue;

                bestT = hitT;
                hitCombatant = combatant;
            }
        }

        if (hitCombatant != null)
        {
            GlobalPosition = start.Lerp(end, bestT);
            if (PayloadType == CombatPayloadType.Interdiction && hitCombatant is IInterdictableCombatant interdictable)
                interdictable.ApplyInterdiction(EffectDuration, this);
            else
                hitCombatant.ApplyCombatDamage(Damage, this);
            QueueFree();
            return false;
        }

        GlobalPosition = end;
        _travelled += start.DistanceTo(end);
        if (_travelled >= MaxDistance)
        {
            QueueFree();
            return false;
        }

        return true;
    }

    public void ApplyWorldOffset(Vector3 offset)
    {
        Position -= offset;
    }

    private void BuildVisual(Color tint)
    {
        if (GetNodeOrNull<MeshInstance3D>("Visual") != null)
            return;

        var material = new StandardMaterial3D
        {
            AlbedoColor = tint,
            EmissionEnabled = true,
            Emission = tint,
            EmissionEnergyMultiplier = 1.8f,
            ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
        };

        var mesh = new CylinderMesh
        {
            TopRadius = _visualStyle == CombatVisualStyle.Beam ? 0.12f : 0.18f,
            BottomRadius = _visualStyle == CombatVisualStyle.Beam ? 0.12f : 0.18f,
            Height = _visualStyle == CombatVisualStyle.Beam ? 8.5f : 3.2f,
            RadialSegments = 8,
            Rings = 1,
        };

        var visual = new MeshInstance3D
        {
            Name = "Visual",
            Mesh = mesh,
            MaterialOverride = material,
            Rotation = new Vector3(Mathf.Pi * 0.5f, 0f, 0f),
            CastShadow = GeometryInstance3D.ShadowCastingSetting.Off,
        };

        AddChild(visual);
    }

    private static bool TryIntersectSegmentSphere(
        Vector3 segmentStart,
        Vector3 segmentEnd,
        Vector3 sphereCenter,
        float sphereRadius,
        out float t)
    {
        Vector3 segment = segmentEnd - segmentStart;
        float segmentLengthSquared = segment.LengthSquared();
        if (segmentLengthSquared <= 0.0001f)
        {
            t = 0f;
            return false;
        }

        Vector3 toCenter = segmentStart - sphereCenter;
        float a = segmentLengthSquared;
        float b = 2f * segment.Dot(toCenter);
        float c = toCenter.LengthSquared() - sphereRadius * sphereRadius;
        float discriminant = b * b - 4f * a * c;
        if (discriminant < 0f)
        {
            t = 0f;
            return false;
        }

        float sqrtDiscriminant = Mathf.Sqrt(discriminant);
        float invDenominator = 1f / (2f * a);
        float t0 = (-b - sqrtDiscriminant) * invDenominator;
        float t1 = (-b + sqrtDiscriminant) * invDenominator;

        if (t0 >= 0f && t0 <= 1f)
        {
            t = t0;
            return true;
        }

        if (t1 >= 0f && t1 <= 1f)
        {
            t = t1;
            return true;
        }

        t = 0f;
        return false;
    }
}
