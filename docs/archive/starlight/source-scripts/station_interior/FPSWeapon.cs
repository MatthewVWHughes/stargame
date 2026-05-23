using Godot;

namespace Starlight.StationInterior;

/// <summary>
/// Hitscan weapon with magazine system, reload, and sprint weapon-lowering.
/// </summary>
public partial class FPSWeapon : Node
{
    [Export] public float Damage = 15.0f;
    [Export] public float Range = 50.0f;
    [Export] public float FireRate = 4.0f;
    [Export] public int MagazineSize = 12;
    [Export] public int StartingMags = 5;

    // Magazine pool — each entry is bullet count in that mag
    public System.Collections.Generic.List<int> Magazines = new();
    public int CurrentMagIndex;
    public int CurrentMagAmmo => Magazines.Count > 0 ? Magazines[CurrentMagIndex] : 0;
    public int TotalMags => Magazines.Count;
    public int SpareMags => Magazines.Count - 1; // minus the one in the gun
    public bool IsReloading { get; private set; }

    private Camera3D _camera;
    private float _fireCooldown;
    private float _reloadTimer;
    private const float ReloadTime = 1.5f;
    private const float HoldThreshold = 0.3f;
    private FPSController _controller;
    private MagRadialMenu _radialMenu;
    private float _reloadHoldTimer;
    private bool _reloadKeyWasPressed;
    private int _pendingMagIndex = -1; // specific mag to swap to, -1 = auto (fullest)

    public override void _Ready()
    {
        // Start with one full mag loaded + spare mags
        for (int i = 0; i < StartingMags; i++)
            Magazines.Add(MagazineSize);
        CurrentMagIndex = 0;
    }

    public void Setup(Camera3D camera, CanvasLayer hud)
    {
        _camera = camera;
        _controller = GetParent<FPSController>();

        // Create radial menu
        _radialMenu = new MagRadialMenu();
        _radialMenu.Weapon = this;
        hud.AddChild(_radialMenu);
    }

    public override void _Process(double delta)
    {
        float dt = (float)delta;
        _fireCooldown -= dt;

        // Radial menu open — don't do anything else
        if (_radialMenu != null && _radialMenu.IsOpen)
        {
            if (!Input.IsKeyPressed(Key.R))
            {
                // Released R — apply selection and close
                _radialMenu.ApplySelection();
                _radialMenu.Close();
                _reloadKeyWasPressed = false;
            }
            return;
        }

        // Reload
        if (IsReloading)
        {
            _reloadTimer -= dt;
            if (_reloadTimer <= 0)
            {
                FinishReload();
                IsReloading = false;
            }
            return;
        }

        // R key: tap = quick reload, hold = radial menu
        bool rPressed = Input.IsKeyPressed(Key.R);
        if (rPressed && !IsReloading)
        {
            if (!_reloadKeyWasPressed)
            {
                _reloadKeyWasPressed = true;
                _reloadHoldTimer = 0;
            }
            _reloadHoldTimer += dt;

            if (_reloadHoldTimer >= HoldThreshold && _radialMenu != null && !_radialMenu.IsOpen)
            {
                // Hold — open radial menu
                _radialMenu.Open();
            }
        }
        else if (!rPressed && _reloadKeyWasPressed)
        {
            _reloadKeyWasPressed = false;
            if (_reloadHoldTimer < HoldThreshold)
            {
                // Quick tap — fast reload to fullest mag
                if (HasSpareMag())
                    StartReload();
            }
            _reloadHoldTimer = 0;
        }

        // Can't shoot while sprinting or reloading
        bool canShoot = _controller == null || !_controller.IsSprinting;

        if (Input.IsMouseButtonPressed(MouseButton.Left) && _fireCooldown <= 0 && canShoot)
        {
            if (CurrentMagAmmo > 0)
            {
                Fire();
                _fireCooldown = 1.0f / FireRate;
            }
        }
    }

    private bool HasSpareMag()
    {
        // Check if any OTHER mag exists (not just the current one)
        for (int i = 0; i < Magazines.Count; i++)
        {
            if (i != CurrentMagIndex) return true;
        }
        return false;
    }

    private void StartReload()
    {
        _pendingMagIndex = -1; // auto = fullest
        IsReloading = true;
        _reloadTimer = ReloadTime;
    }

    public void StartReloadToMag(int magIndex)
    {
        _pendingMagIndex = magIndex;
        IsReloading = true;
        _reloadTimer = ReloadTime;
    }

    private void FinishReload()
    {
        int targetIndex;

        if (_pendingMagIndex >= 0 && _pendingMagIndex < Magazines.Count && _pendingMagIndex != CurrentMagIndex)
        {
            // Specific mag selected from radial menu
            targetIndex = _pendingMagIndex;
        }
        else
        {
            // Auto: find fullest spare
            targetIndex = -1;
            int bestAmmo = -1;
            for (int i = 0; i < Magazines.Count; i++)
            {
                if (i == CurrentMagIndex) continue;
                if (Magazines[i] > bestAmmo)
                {
                    bestAmmo = Magazines[i];
                    targetIndex = i;
                }
            }
        }

        if (targetIndex >= 0)
        {
            CurrentMagIndex = targetIndex;
            GD.Print($"Swapped to mag ({Magazines[CurrentMagIndex]}/{MagazineSize})");
        }

        _pendingMagIndex = -1;

        // Drop empty mags
        Magazines.RemoveAll(m => m <= 0);
        if (CurrentMagIndex >= Magazines.Count)
            CurrentMagIndex = 0;
    }

    private void Fire()
    {
        if (_camera == null || Magazines.Count == 0) return;
        Magazines[CurrentMagIndex]--;

        var from = _camera.GlobalPosition;
        var forward = -_camera.GlobalTransform.Basis.Z;
        var to = from + forward * Range;

        var spaceState = _camera.GetWorld3D().DirectSpaceState;
        var query = PhysicsRayQueryParameters3D.Create(from, to);
        query.CollisionMask = 0b0000_0111;
        var result = spaceState.IntersectRay(query);

        if (result.Count > 0)
        {
            var collider = result["collider"].As<Node>();
            var hitPos = result["position"].AsVector3();

            if (collider is HostileNPC hostile)
                hostile.TakeDamage(Damage);
            else if (collider?.GetParent() is HostileNPC hostileParent)
                hostileParent.TakeDamage(Damage);

            SpawnHitMarker(hitPos);
        }

        // Emit gunshot noise so nearby enemies can hear
        PerceptionSystem.EmitNoise(from, 30.0f, PerceptionSystem.NoiseType.Gunshot,
            Time.GetTicksMsec() / 1000.0);
    }

    private void SpawnHitMarker(Vector3 position)
    {
        var marker = new MeshInstance3D();
        var sphere = new SphereMesh { Radius = 0.05f, Height = 0.1f };
        var mat = new StandardMaterial3D
        {
            AlbedoColor = new Color(1, 0.8f, 0),
            EmissionEnabled = true,
            Emission = new Color(1, 0.8f, 0),
            EmissionEnergyMultiplier = 3.0f,
        };
        sphere.SurfaceSetMaterial(0, mat);
        marker.Mesh = sphere;
        marker.TopLevel = true;
        GetTree().Root.AddChild(marker);
        marker.GlobalPosition = position;
        var timer = GetTree().CreateTimer(0.15);
        timer.Timeout += () => marker.QueueFree();
    }
}
