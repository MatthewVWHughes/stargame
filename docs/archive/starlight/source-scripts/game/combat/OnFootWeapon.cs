using System;
using Godot;

namespace Starlight.Game.Combat;

/// <summary>
/// Hitscan sidearm for the VS1 on-foot slice.
/// Attached to a Camera3D — raycasts from camera forward, applies damage to whatever it hits
/// that implements IOnFootDamageable. Spawns a brief tracer beam as visual feedback.
///
/// Armed externally (HostileStationController arms it on boarding; the civilian station interior
/// leaves the player unarmed). Kept as a plain Node3D so it can be attached/detached at runtime.
/// </summary>
public partial class OnFootWeapon : Node3D
{
    [Export] public float Damage { get; set; } = 20f;
    [Export] public float MaxRange { get; set; } = 60f;
    [Export] public float FireCooldownSeconds { get; set; } = 0.35f;

    [Export] public string ModelScenePath { get; set; } = "res://assets/weapons/ScannDMR/ScannDMR.glb";
    [Export] public Vector3 ModelLocalPosition { get; set; } = new(0.28f, -0.22f, -0.45f);
    [Export] public Vector3 ModelLocalRotationDegrees { get; set; } = new(0f, 0f, 0f);
    [Export] public float ModelScale { get; set; } = 1.0f;
    /// <summary>Muzzle position relative to the gun model's local origin, in meters.</summary>
    [Export] public Vector3 MuzzleLocalOffset { get; set; } = new(0f, 0f, -0.6f);

    public event Action Fired;

    private Camera3D _camera;
    private CharacterBody3D _ownerBody;
    private Node3D _gunModel;
    private Marker3D _muzzle;
    private float _cooldown;
    private float _recoilTimer;
    private bool _armed;

    public override void _Ready()
    {
        BuildGunModel();
    }

    public void Attach(Camera3D camera, CharacterBody3D ownerBody)
    {
        _camera = camera;
        _ownerBody = ownerBody;
    }

    private void BuildGunModel()
    {
        if (_gunModel != null || string.IsNullOrWhiteSpace(ModelScenePath))
            return;

        PackedScene scene = ResourceLoader.Load<PackedScene>(ModelScenePath);
        if (scene == null)
            return;

        _gunModel = scene.Instantiate<Node3D>();
        _gunModel.Name = "GunModel";
        _gunModel.Position = ModelLocalPosition;
        _gunModel.RotationDegrees = ModelLocalRotationDegrees;
        _gunModel.Scale = new Vector3(ModelScale, ModelScale, ModelScale);
        AddChild(_gunModel);

        _muzzle = new Marker3D
        {
            Name = "Muzzle",
            Position = MuzzleLocalOffset,
        };
        _gunModel.AddChild(_muzzle);
    }

    public void SetArmed(bool armed)
    {
        _armed = armed;
        if (!armed)
            _cooldown = 0f;
    }

    public override void _PhysicsProcess(double delta)
    {
        float dt = (float)delta;
        _cooldown = Mathf.Max(0f, _cooldown - dt);
        UpdateRecoil(dt);

        if (!_armed || _camera == null)
            return;
        if (Input.MouseMode != Input.MouseModeEnum.Captured)
            return;
        if (_cooldown > 0f)
            return;
        if (!Input.IsMouseButtonPressed(MouseButton.Left))
            return;

        Fire();
        _cooldown = FireCooldownSeconds;
        _recoilTimer = 0.12f;
    }

    private void UpdateRecoil(float dt)
    {
        if (_gunModel == null)
            return;

        float targetOffset = 0f;
        if (_recoilTimer > 0f)
        {
            targetOffset = _recoilTimer * 0.45f;
            _recoilTimer = Mathf.Max(0f, _recoilTimer - dt);
        }

        Vector3 rest = ModelLocalPosition;
        _gunModel.Position = new Vector3(rest.X, rest.Y, rest.Z + targetOffset);
    }

    private void Fire()
    {
        Vector3 origin = _camera.GlobalPosition;
        Vector3 direction = -_camera.GlobalTransform.Basis.Z;
        Vector3 end = origin + direction * MaxRange;

        PhysicsDirectSpaceState3D space = _camera.GetWorld3D().DirectSpaceState;
        var query = PhysicsRayQueryParameters3D.Create(origin, end);
        if (_ownerBody != null)
            query.Exclude = new Godot.Collections.Array<Rid> { _ownerBody.GetRid() };

        var hit = space.IntersectRay(query);
        Vector3 hitPoint = end;
        if (hit.Count > 0)
        {
            hitPoint = hit["position"].AsVector3();
            Node node = hit["collider"].As<Node>();
            if (node != null)
            {
                StationHostile hostile = node as StationHostile ?? node.GetParentOrNull<StationHostile>();
                if (hostile != null)
                    hostile.ApplyOnFootDamage(Damage);
            }
        }

        Vector3 tracerOrigin = _muzzle != null ? _muzzle.GlobalPosition : origin;
        OnFootBeamTracer.Spawn(GetTree().CurrentScene ?? GetTree().Root, tracerOrigin, hitPoint, new Color(0.6f, 0.9f, 1f));
        Fired?.Invoke();
    }
}
