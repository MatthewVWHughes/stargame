using System;
using Godot;
using Starlight.Game.Runtime;

namespace Starlight.Game.Combat;

/// <summary>
/// Cylinder-placeholder on-foot enemy for the VS1 boarding slice.
/// HumanGen3D NPC models aren't usable yet, so this is intentionally a colored cylinder.
/// The AI is deliberately dumb: stand, detect player, turn to face, fire hitscan on cooldown.
/// More tactical behavior (cover, flanking) lives in the prototype squad AI and will be adopted later.
/// </summary>
public partial class StationHostile : CharacterBody3D
{
    [Export] public float MaxHealth { get; set; } = 45f;
    [Export] public float FireDamage { get; set; } = 8f;
    [Export] public float DetectionRange { get; set; } = 35f;
    [Export] public float FireRange { get; set; } = 28f;
    [Export] public float FireCooldownSeconds { get; set; } = 1.3f;
    [Export] public float InaccuracyRadians { get; set; } = 0.04f;

    public event Action Killed;

    public float Health { get; private set; }
    public bool IsAlive => Health > 0f;

    private Node3D _player;
    private Camera3D _playerCamera;
    private float _fireCooldown;
    private MeshInstance3D _body;
    private StandardMaterial3D _bodyMaterial;
    private bool _dead;

    public override void _Ready()
    {
        Health = MaxHealth;
        BuildVisual();
        BuildCollision();
        AddToGroup("vs1_station_hostile");
    }

    public void SetPlayer(Node3D player, Camera3D playerCamera)
    {
        _player = player;
        _playerCamera = playerCamera;
    }

    public void ApplyOnFootDamage(float amount)
    {
        if (_dead)
            return;

        Health = Mathf.Max(0f, Health - amount);
        FlashHit();

        if (Health <= 0f)
            Die();
    }

    public override void _PhysicsProcess(double delta)
    {
        if (_dead || _player == null)
            return;

        float dt = (float)delta;
        _fireCooldown = Mathf.Max(0f, _fireCooldown - dt);

        Vector3 toPlayer = _player.GlobalPosition - GlobalPosition;
        float distance = toPlayer.Length();
        if (distance > DetectionRange || distance < 0.001f)
            return;

        if (!HasLineOfSight(_player.GlobalPosition))
            return;

        // Face the player (horizontal).
        Vector3 flat = toPlayer;
        flat.Y = 0f;
        if (flat.LengthSquared() > 0.001f)
        {
            Vector3 lookTarget = GlobalPosition + flat;
            LookAt(lookTarget, Vector3.Up);
        }

        if (distance <= FireRange && _fireCooldown <= 0f)
        {
            FireAtPlayer();
            _fireCooldown = FireCooldownSeconds;
        }
    }

    private bool HasLineOfSight(Vector3 targetWorld)
    {
        Vector3 origin = GlobalPosition + new Vector3(0f, 1.2f, 0f);
        PhysicsDirectSpaceState3D space = GetWorld3D().DirectSpaceState;
        var query = PhysicsRayQueryParameters3D.Create(origin, targetWorld);
        query.Exclude = new Godot.Collections.Array<Rid> { GetRid() };

        var hit = space.IntersectRay(query);
        if (hit.Count == 0)
            return true;

        Node collider = hit["collider"].As<Node>();
        return collider is CharacterBody3D && collider == _player;
    }

    private void FireAtPlayer()
    {
        Vector3 origin = GlobalPosition + new Vector3(0f, 1.2f, 0f);
        Vector3 aim = _playerCamera != null ? _playerCamera.GlobalPosition : _player.GlobalPosition + new Vector3(0f, 1.3f, 0f);
        Vector3 direction = (aim - origin).Normalized();

        // Random inaccuracy.
        if (InaccuracyRadians > 0f)
        {
            float yaw = (GD.Randf() - 0.5f) * 2f * InaccuracyRadians;
            float pitch = (GD.Randf() - 0.5f) * 2f * InaccuracyRadians;
            direction = direction.Rotated(Vector3.Up, yaw).Rotated(Vector3.Right, pitch).Normalized();
        }

        Vector3 end = origin + direction * FireRange * 1.2f;

        PhysicsDirectSpaceState3D space = GetWorld3D().DirectSpaceState;
        var query = PhysicsRayQueryParameters3D.Create(origin, end);
        query.Exclude = new Godot.Collections.Array<Rid> { GetRid() };

        var hit = space.IntersectRay(query);
        Vector3 hitPoint = end;
        if (hit.Count > 0)
        {
            hitPoint = hit["position"].AsVector3();
            Node collider = hit["collider"].As<Node>();
            if (collider is CharacterBody3D body && body == _player)
                PlayerState.ApplyOnFootDamage(FireDamage);
        }

        OnFootBeamTracer.Spawn(GetTree().CurrentScene ?? GetTree().Root, origin, hitPoint, new Color(1f, 0.35f, 0.3f));
    }

    private void Die()
    {
        _dead = true;
        SetPhysicsProcess(false);
        if (_bodyMaterial != null)
            _bodyMaterial.AlbedoColor = new Color(0.25f, 0.1f, 0.1f);
        // Tip over so the kill is readable.
        RotateObjectLocal(Vector3.Right, Mathf.DegToRad(80f));
        Killed?.Invoke();
    }

    private void FlashHit()
    {
        if (_bodyMaterial == null)
            return;

        Color hitColor = new Color(1f, 0.9f, 0.5f);
        _bodyMaterial.Emission = hitColor;
        _bodyMaterial.EmissionEnergyMultiplier = 1.5f;
        var timer = GetTree().CreateTimer(0.12f);
        timer.Timeout += () =>
        {
            if (IsInstanceValid(_bodyMaterial))
            {
                _bodyMaterial.Emission = new Color(0f, 0f, 0f);
                _bodyMaterial.EmissionEnergyMultiplier = 0f;
            }
        };
    }

    private void BuildVisual()
    {
        _bodyMaterial = new StandardMaterial3D
        {
            AlbedoColor = new Color(0.9f, 0.25f, 0.2f),
            Roughness = 0.6f,
        };

        _body = new MeshInstance3D
        {
            Name = "Body",
            Position = new Vector3(0f, 0.9f, 0f),
            Mesh = new CylinderMesh
            {
                TopRadius = 0.4f,
                BottomRadius = 0.5f,
                Height = 1.8f,
            },
            MaterialOverride = _bodyMaterial,
        };
        AddChild(_body);

        var head = new MeshInstance3D
        {
            Name = "Head",
            Position = new Vector3(0f, 2f, 0f),
            Mesh = new SphereMesh { Radius = 0.22f, Height = 0.44f },
            MaterialOverride = new StandardMaterial3D { AlbedoColor = new Color(0.55f, 0.18f, 0.15f) },
        };
        AddChild(head);

        var forwardMarker = new MeshInstance3D
        {
            Name = "ForwardMarker",
            Position = new Vector3(0f, 1.3f, -0.52f),
            Mesh = new BoxMesh { Size = new Vector3(0.16f, 0.16f, 0.3f) },
            MaterialOverride = new StandardMaterial3D { AlbedoColor = new Color(0.25f, 0.25f, 0.28f) },
        };
        AddChild(forwardMarker);
    }

    private void BuildCollision()
    {
        var shape = new CollisionShape3D
        {
            Name = "Collision",
            Position = new Vector3(0f, 0.9f, 0f),
            Shape = new CapsuleShape3D { Radius = 0.45f, Height = 1.8f },
        };
        AddChild(shape);
    }
}
