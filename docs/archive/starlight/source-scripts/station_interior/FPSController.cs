using Godot;

namespace Starlight.StationInterior;

/// <summary>
/// Full-featured FPS controller with sprint, crouch, slide, lean, jump, and camera effects.
/// State machine driven: Walking, Sprinting, Crouching, Sliding, Airborne.
/// </summary>
public partial class FPSController : CharacterBody3D
{
    // --- Exported Settings ---
    [Export] public float WalkSpeed = 4.0f;
    [Export] public float SprintSpeed = 6.5f;
    [Export] public float CrouchSpeed = 2.0f;
    [Export] public float MouseSensitivity = 0.002f;
    [Export] public float PitchClamp = 80.0f;
    [Export] public float InteractRange = 3.0f;
    [Export] public float MaxHP = 100.0f;
    [Export] public float MaxStamina = 100.0f;
    [Export] public float StaminaDrainRate = 20.0f;
    [Export] public float StaminaRegenRate = 15.0f;
    [Export] public float StaminaRegenDelay = 1.0f;

    // --- Integration switches (default = self-contained prototype behavior) ---
    /// <summary>Auto-spawn the prototype FPSWeapon in _Ready. Disable when the host owns weapon wiring (e.g. VS1).</summary>
    [Export] public bool AutoSpawnFPSWeapon { get; set; } = true;
    /// <summary>Hide the combat-oriented HUD rows (HP, stamina, ammo). Crosshair + prompt stay.</summary>
    [Export] public bool HideCombatHud { get; set; } = false;
    /// <summary>Route HP through Starlight.Game.Runtime.PlayerState.OnFootHealth when true; otherwise use local HP.</summary>
    [Export] public bool UseSharedOnFootHealth { get; set; } = false;

    /// <summary>When true, input (movement + look + interact) is suspended and the mouse is released.</summary>
    public bool InputLocked { get; private set; }

    // --- Movement Tuning ---
    private const float Acceleration = 50.0f;      // was 40 — snappier start
    private const float Deceleration = 35.0f;      // was 30 — crisper stop
    private const float AirAcceleration = 10.0f;   // was 8 — slightly more air control
    private const float Gravity = 14.0f;           // was 12 — less floaty jumps
    private const float JumpVelocity = 5.0f;       // was 4.5 — slightly higher jump
    private const float CoyoteTime = 0.12f;        // was 0.1

    // Sprint
    private const float BaseFov = 75.0f;
    private const float SprintFovBoost = 7.0f;

    // Crouch / Slide
    private const float StandHeight = 1.8f;
    private const float CrouchHeight = 1.0f;
    private const float StandCameraY = 1.6f;
    private const float CrouchCameraY = 0.8f;
    private const float SlideInitialSpeed = 8.0f;  // was 7.5 — more kick on entry
    private const float SlideDeceleration = 4.0f;  // was 6 — slides further
    private const float SlideEndSpeed = 2.5f;      // was 2 — exits a bit faster
    private const float SlideCooldown = 0.5f;
    private const float SlideTiltDeg = -3.0f;

    // Lean
    private const float LeanAngle = 15.0f;
    private const float LeanOffset = 0.3f;
    private const float LeanSpeed = 10.0f;

    // Bob
    private const float BobFrequency = 12.0f;
    private const float BobAmplitudeY = 0.03f;
    private const float BobAmplitudeX = 0.015f;

    // --- State ---
    public enum MoveState { Walking, Sprinting, Crouching, Sliding, Airborne }
    public MoveState State { get; private set; } = MoveState.Walking;
    public float HP { get; private set; }
    public float Stamina { get; private set; }
    public bool IsSprinting => State == MoveState.Sprinting;
    private float _staminaRegenTimer;

    // --- Nodes ---
    private Node3D _cameraHolder;
    private Node3D _leanPivot;
    private Camera3D _camera;
    private CollisionShape3D _collisionShape;
    private CanvasLayer _hud;
    private RayCast3D _interactRay;
    private Label _promptLabel;
    private ProgressBar _hpBar;
    private ProgressBar _staminaBar;
    private Label _hpLabel;
    private Label _ammoLabel;
    private Label _reloadLabel;
    private Label _stateLabel;
    private FPSWeapon _weapon;

    // --- Internal ---
    private bool _mouseCaptured = true;
    private bool _interactHeld;
    private float _coyoteTimer;
    private float _slideCooldownTimer;
    private float _footstepNoiseTimer;
    private float _currentLean;
    private float _bobTimer;
    private Vector3 _slideDirection;

    public Camera3D Camera => _camera;
    public CanvasLayer HUD => _hud;

    public override void _Ready()
    {
        if (UseSharedOnFootHealth)
        {
            Starlight.Game.Runtime.PlayerState.ResetOnFootHealth();
            MaxHP = Starlight.Game.Runtime.PlayerState.MaxOnFootHealth;
            HP = Starlight.Game.Runtime.PlayerState.OnFootHealth;
        }
        else
        {
            HP = MaxHP;
        }
        Stamina = MaxStamina;

        // Camera hierarchy: CharacterBody3D -> CameraHolder -> LeanPivot -> Camera3D
        _cameraHolder = new Node3D();
        _cameraHolder.Name = "CameraHolder";
        _cameraHolder.Position = new Vector3(0, StandCameraY, 0);
        AddChild(_cameraHolder);

        _leanPivot = new Node3D();
        _leanPivot.Name = "LeanPivot";
        _cameraHolder.AddChild(_leanPivot);

        _camera = new Camera3D();
        _camera.Fov = BaseFov;
        _leanPivot.AddChild(_camera);

        // Interaction raycast
        _interactRay = new RayCast3D();
        _interactRay.TargetPosition = new Vector3(0, 0, -InteractRange);
        _interactRay.Enabled = true;
        _interactRay.CollisionMask = 0b0000_0100;
        _interactRay.CollideWithAreas = true;
        _camera.AddChild(_interactRay);

        // HUD
        _hud = new CanvasLayer();
        AddChild(_hud);

        // --- CROSSHAIR (center) ---
        var crosshair = new Label();
        crosshair.Text = "+";
        crosshair.HorizontalAlignment = HorizontalAlignment.Center;
        crosshair.VerticalAlignment = VerticalAlignment.Center;
        crosshair.AnchorLeft = 0.5f; crosshair.AnchorRight = 0.5f;
        crosshair.AnchorTop = 0.5f; crosshair.AnchorBottom = 0.5f;
        crosshair.GrowHorizontal = Control.GrowDirection.Both;
        crosshair.GrowVertical = Control.GrowDirection.Both;
        crosshair.AddThemeFontSizeOverride("font_size", 24);
        _hud.AddChild(crosshair);

        // --- INTERACTION PROMPT (center, below crosshair) ---
        _promptLabel = new Label();
        _promptLabel.Text = "";
        _promptLabel.HorizontalAlignment = HorizontalAlignment.Center;
        _promptLabel.AnchorLeft = 0.5f; _promptLabel.AnchorRight = 0.5f;
        _promptLabel.AnchorTop = 0.6f;
        _promptLabel.GrowHorizontal = Control.GrowDirection.Both;
        _promptLabel.AddThemeFontSizeOverride("font_size", 18);
        _hud.AddChild(_promptLabel);

        // --- RELOAD INDICATOR (center, above crosshair) ---
        _reloadLabel = new Label();
        _reloadLabel.Text = "";
        _reloadLabel.HorizontalAlignment = HorizontalAlignment.Center;
        _reloadLabel.AnchorLeft = 0.5f; _reloadLabel.AnchorRight = 0.5f;
        _reloadLabel.AnchorTop = 0.42f;
        _reloadLabel.GrowHorizontal = Control.GrowDirection.Both;
        _reloadLabel.AddThemeFontSizeOverride("font_size", 16);
        _reloadLabel.AddThemeColorOverride("font_color", new Color(1, 0.8f, 0.2f));
        _hud.AddChild(_reloadLabel);

        // --- BOTTOM LEFT: Health + Stamina bars ---
        var bottomLeft = new VBoxContainer();
        bottomLeft.AnchorLeft = 0; bottomLeft.AnchorRight = 0;
        bottomLeft.AnchorTop = 1; bottomLeft.AnchorBottom = 1;
        bottomLeft.OffsetLeft = 20; bottomLeft.OffsetTop = -90;
        bottomLeft.OffsetRight = 220; bottomLeft.OffsetBottom = -10;
        bottomLeft.GrowVertical = Control.GrowDirection.Begin;
        _hud.AddChild(bottomLeft);

        // HP bar
        var hpContainer = new HBoxContainer();
        var hpIcon = new Label();
        hpIcon.Text = "HP";
        hpIcon.CustomMinimumSize = new Vector2(30, 0);
        hpIcon.AddThemeFontSizeOverride("font_size", 12);
        hpContainer.AddChild(hpIcon);
        _hpBar = new ProgressBar();
        _hpBar.MinValue = 0; _hpBar.MaxValue = MaxHP; _hpBar.Value = HP;
        _hpBar.ShowPercentage = false;
        _hpBar.CustomMinimumSize = new Vector2(150, 18);
        _hpBar.SizeFlagsHorizontal = Control.SizeFlags.ExpandFill;
        var hpStyle = new StyleBoxFlat { BgColor = new Color(0.8f, 0.15f, 0.1f) };
        hpStyle.SetCornerRadiusAll(2);
        _hpBar.AddThemeStyleboxOverride("fill", hpStyle);
        var hpBg = new StyleBoxFlat { BgColor = new Color(0.2f, 0.05f, 0.05f) };
        hpBg.SetCornerRadiusAll(2);
        _hpBar.AddThemeStyleboxOverride("background", hpBg);
        hpContainer.AddChild(_hpBar);
        _hpLabel = new Label();
        _hpLabel.CustomMinimumSize = new Vector2(50, 0);
        _hpLabel.HorizontalAlignment = HorizontalAlignment.Right;
        _hpLabel.AddThemeFontSizeOverride("font_size", 12);
        hpContainer.AddChild(_hpLabel);
        bottomLeft.AddChild(hpContainer);

        // Stamina bar
        var stamContainer = new HBoxContainer();
        var stamIcon = new Label();
        stamIcon.Text = "ST";
        stamIcon.CustomMinimumSize = new Vector2(30, 0);
        stamIcon.AddThemeFontSizeOverride("font_size", 12);
        stamContainer.AddChild(stamIcon);
        _staminaBar = new ProgressBar();
        _staminaBar.MinValue = 0; _staminaBar.MaxValue = MaxStamina; _staminaBar.Value = Stamina;
        _staminaBar.ShowPercentage = false;
        _staminaBar.CustomMinimumSize = new Vector2(150, 18);
        _staminaBar.SizeFlagsHorizontal = Control.SizeFlags.ExpandFill;
        var stamStyle = new StyleBoxFlat { BgColor = new Color(0.2f, 0.6f, 0.9f) };
        stamStyle.SetCornerRadiusAll(2);
        _staminaBar.AddThemeStyleboxOverride("fill", stamStyle);
        var stamBg = new StyleBoxFlat { BgColor = new Color(0.05f, 0.1f, 0.2f) };
        stamBg.SetCornerRadiusAll(2);
        _staminaBar.AddThemeStyleboxOverride("background", stamBg);
        stamContainer.AddChild(_staminaBar);
        bottomLeft.AddChild(stamContainer);

        // --- BOTTOM RIGHT: Ammo ---
        _ammoLabel = new Label();
        _ammoLabel.AnchorLeft = 1; _ammoLabel.AnchorRight = 1;
        _ammoLabel.AnchorTop = 1; _ammoLabel.AnchorBottom = 1;
        _ammoLabel.OffsetLeft = -180; _ammoLabel.OffsetTop = -50;
        _ammoLabel.OffsetRight = -20;
        _ammoLabel.HorizontalAlignment = HorizontalAlignment.Right;
        _ammoLabel.AddThemeFontSizeOverride("font_size", 22);
        _hud.AddChild(_ammoLabel);

        // --- DEBUG: State label (top left) ---
        _stateLabel = new Label();
        _stateLabel.AnchorLeft = 0; _stateLabel.AnchorTop = 0;
        _stateLabel.OffsetLeft = 10; _stateLabel.OffsetTop = 10;
        _stateLabel.AddThemeFontSizeOverride("font_size", 12);
        _stateLabel.AddThemeColorOverride("font_color", new Color(0.5f, 0.5f, 0.5f));
        _hud.AddChild(_stateLabel);

        // Weapon (prototype magazine weapon — hosts that drive their own weapon set AutoSpawnFPSWeapon=false)
        if (AutoSpawnFPSWeapon)
        {
            _weapon = new FPSWeapon();
            _weapon.Setup(_camera, _hud);
            AddChild(_weapon);
        }

        if (HideCombatHud)
        {
            bottomLeft.Visible = false;
            _ammoLabel.Visible = false;
            _reloadLabel.Visible = false;
            _stateLabel.Visible = false;
        }

        Input.MouseMode = Input.MouseModeEnum.Captured;
    }

    public override void _Input(InputEvent @event)
    {
        if (InputLocked)
            return;

        if (@event is InputEventMouseMotion mouseMotion && _mouseCaptured
            && Input.MouseMode == Input.MouseModeEnum.Captured)
        {
            // Yaw on the body
            RotateY(-mouseMotion.Relative.X * MouseSensitivity);

            // Pitch on the camera
            _camera.RotateX(-mouseMotion.Relative.Y * MouseSensitivity);
            var rot = _camera.Rotation;
            rot.X = Mathf.Clamp(rot.X, Mathf.DegToRad(-PitchClamp), Mathf.DegToRad(PitchClamp));
            _camera.Rotation = rot;
        }

        // Esc is intentionally not handled here — PauseMenuController (autoloaded) owns pause + mouse capture.
    }

    /// <summary>
    /// Host (dialog panels, pause menus) calls this to freeze the player in place and release the mouse,
    /// then calls it again with <c>false</c> to return to normal play.
    /// </summary>
    public void SetInputLocked(bool locked)
    {
        InputLocked = locked;
        _interactHeld = false;
        if (_promptLabel != null)
            _promptLabel.Text = "";
        if (locked)
        {
            Velocity = Vector3.Zero;
            Input.MouseMode = Input.MouseModeEnum.Visible;
        }
        else
        {
            _mouseCaptured = true;
            Input.MouseMode = Input.MouseModeEnum.Captured;
        }
    }

    public override void _PhysicsProcess(double delta)
    {
        float dt = (float)delta;

        if (UseSharedOnFootHealth)
            HP = Starlight.Game.Runtime.PlayerState.OnFootHealth;

        if (InputLocked)
        {
            Velocity = Vector3.Zero;
            UpdateHUD();
            return;
        }

        // --- Input ---
        var inputVec = Vector2.Zero;
        if (Input.IsKeyPressed(Key.W)) inputVec.Y -= 1;
        if (Input.IsKeyPressed(Key.S)) inputVec.Y += 1;
        if (Input.IsKeyPressed(Key.A)) inputVec.X -= 1;
        if (Input.IsKeyPressed(Key.D)) inputVec.X += 1;
        inputVec = inputVec.Normalized();
        bool hasInput = inputVec.LengthSquared() > 0.01f;
        bool hasForwardInput = inputVec.Y < -0.1f;

        bool wantsSprint = Input.IsKeyPressed(Key.Shift);
        bool wantsCrouch = Input.IsKeyPressed(Key.C) || Input.IsKeyPressed(Key.Ctrl);
        bool wantsJump = Input.IsKeyPressed(Key.Space);

        // Move direction in world space
        var forward = -GlobalTransform.Basis.Z;
        var right = GlobalTransform.Basis.X;
        forward.Y = 0; right.Y = 0;
        forward = forward.Normalized(); right = right.Normalized();
        var moveDir = forward * -inputVec.Y + right * inputVec.X;

        // --- Timers ---
        if (IsOnFloor()) _coyoteTimer = CoyoteTime;
        else _coyoteTimer -= dt;
        _slideCooldownTimer -= dt;

        // --- Gravity ---
        var velocity = Velocity;
        if (!IsOnFloor())
            velocity.Y -= Gravity * dt;

        // --- State Machine ---
        UpdateState(dt, hasInput, hasForwardInput, wantsSprint, wantsCrouch, wantsJump, ref velocity, moveDir);

        // --- Apply Movement (non-slide) ---
        if (State != MoveState.Sliding)
        {
            float targetSpeed = GetTargetSpeed();

            if (!IsOnFloor())
            {
                // Airborne: preserve existing momentum, only allow small adjustments
                if (hasInput)
                {
                    velocity.X += moveDir.X * AirAcceleration * dt;
                    velocity.Z += moveDir.Z * AirAcceleration * dt;

                    // Clamp to not exceed current speed or target speed (whichever is higher)
                    float currentHSpeed = new Vector2(velocity.X, velocity.Z).Length();
                    float maxSpeed = Mathf.Max(targetSpeed, currentHSpeed);
                    if (currentHSpeed > maxSpeed)
                    {
                        float scale = maxSpeed / currentHSpeed;
                        velocity.X *= scale;
                        velocity.Z *= scale;
                    }
                }
                // No deceleration in air — momentum preserved
            }
            else
            {
                float accel = hasInput ? Acceleration : Deceleration;
                velocity.X = Mathf.MoveToward(velocity.X, moveDir.X * targetSpeed, accel * dt);
                velocity.Z = Mathf.MoveToward(velocity.Z, moveDir.Z * targetSpeed, accel * dt);
            }
        }

        Velocity = velocity;
        MoveAndSlide();

        // --- Footstep Noise (for enemy AI hearing) ---
        if (IsOnFloor() && Velocity.LengthSquared() > 1.0f)
        {
            _footstepNoiseTimer -= dt;
            if (_footstepNoiseTimer <= 0)
            {
                float radius = State == MoveState.Sprinting ? 15.0f : 8.0f;
                PerceptionSystem.EmitNoise(GlobalPosition, radius, PerceptionSystem.NoiseType.Footstep,
                    Time.GetTicksMsec() / 1000.0);
                _footstepNoiseTimer = 0.4f;
            }
        }

        // --- Stamina ---
        UpdateStamina(dt);

        // --- Lean ---
        UpdateLean(dt);

        // --- Camera Effects ---
        UpdateCameraHeight(dt);
        UpdateCameraBob(dt);
        UpdateFov(dt);
        UpdateSlideTilt(dt);

        // --- Collision Shape ---
        UpdateCollisionShape(dt);

        // --- Interaction & HUD ---
        UpdateInteraction();
        UpdateHUD();
    }

    private void UpdateState(float dt, bool hasInput, bool hasForwardInput,
        bool wantsSprint, bool wantsCrouch, bool wantsJump,
        ref Vector3 velocity, Vector3 moveDir)
    {
        switch (State)
        {
            case MoveState.Walking:
                if (!IsOnFloor())
                    State = MoveState.Airborne;
                else if (wantsSprint && hasForwardInput && Stamina > 0)
                    State = MoveState.Sprinting;
                else if (wantsCrouch)
                    State = MoveState.Crouching;
                else if (wantsJump && _coyoteTimer > 0)
                {
                    velocity.Y = JumpVelocity;
                    _coyoteTimer = 0;
                    State = MoveState.Airborne;
                }
                break;

            case MoveState.Sprinting:
                if (!IsOnFloor())
                    State = MoveState.Airborne;
                else if (wantsCrouch && _slideCooldownTimer <= 0)
                {
                    // Sprint + Crouch = Slide
                    float currentSpeed = new Vector2(Velocity.X, Velocity.Z).Length();
                    if (currentSpeed > WalkSpeed)
                    {
                        _slideDirection = moveDir.Normalized();
                        if (_slideDirection.LengthSquared() < 0.5f)
                            _slideDirection = (-GlobalTransform.Basis.Z).Normalized();
                        _slideDirection.Y = 0;
                        velocity.X = _slideDirection.X * SlideInitialSpeed;
                        velocity.Z = _slideDirection.Z * SlideInitialSpeed;
                        State = MoveState.Sliding;
                    }
                    else
                        State = MoveState.Crouching;
                }
                else if (!wantsSprint || !hasForwardInput || Stamina <= 0)
                    State = MoveState.Walking;
                else if (wantsJump && _coyoteTimer > 0)
                {
                    velocity.Y = JumpVelocity;
                    _coyoteTimer = 0;
                    State = MoveState.Airborne;
                }
                break;

            case MoveState.Crouching:
                if (!IsOnFloor())
                    State = MoveState.Airborne;
                else if (!wantsCrouch && CanStandUp())
                    State = MoveState.Walking;
                break;

            case MoveState.Sliding:
            {
                // Decelerate slide
                float slideSpeed = new Vector2(velocity.X, velocity.Z).Length();
                slideSpeed = Mathf.MoveToward(slideSpeed, SlideEndSpeed, SlideDeceleration * dt);
                velocity.X = _slideDirection.X * slideSpeed;
                velocity.Z = _slideDirection.Z * slideSpeed;

                // End slide
                if (slideSpeed <= SlideEndSpeed + 0.1f)
                {
                    _slideCooldownTimer = SlideCooldown;
                    State = wantsCrouch ? MoveState.Crouching : (CanStandUp() ? MoveState.Walking : MoveState.Crouching);
                }
                else if (!wantsCrouch && CanStandUp())
                {
                    _slideCooldownTimer = SlideCooldown;
                    State = MoveState.Walking;
                }
                else if (wantsJump && _coyoteTimer > 0)
                {
                    velocity.Y = JumpVelocity;
                    _coyoteTimer = 0;
                    _slideCooldownTimer = SlideCooldown;
                    State = MoveState.Airborne;
                }

                if (!IsOnFloor())
                {
                    _slideCooldownTimer = SlideCooldown;
                    State = MoveState.Airborne;
                }
                break;
            }

            case MoveState.Airborne:
                if (IsOnFloor())
                {
                    if (wantsCrouch)
                        State = MoveState.Crouching;
                    else if (wantsSprint && hasForwardInput)
                        State = MoveState.Sprinting;
                    else
                        State = MoveState.Walking;
                }
                else if (wantsJump && _coyoteTimer > 0)
                {
                    velocity.Y = JumpVelocity;
                    _coyoteTimer = 0;
                }
                break;
        }
    }

    private float GetTargetSpeed() => State switch
    {
        MoveState.Walking => WalkSpeed,
        MoveState.Sprinting => SprintSpeed,
        MoveState.Crouching => CrouchSpeed,
        MoveState.Airborne => WalkSpeed,
        _ => WalkSpeed,
    };

    // --- Stamina ---
    private void UpdateStamina(float dt)
    {
        if (State == MoveState.Sprinting)
        {
            Stamina -= StaminaDrainRate * dt;
            _staminaRegenTimer = StaminaRegenDelay;
            if (Stamina <= 0)
            {
                Stamina = 0;
                // Force out of sprint when exhausted
                State = MoveState.Walking;
            }
        }
        else if (State == MoveState.Sliding)
        {
            _staminaRegenTimer = StaminaRegenDelay;
        }
        else
        {
            _staminaRegenTimer -= dt;
            if (_staminaRegenTimer <= 0 && Stamina < MaxStamina)
                Stamina = Mathf.Min(Stamina + StaminaRegenRate * dt, MaxStamina);
        }
    }

    // --- Lean ---
    private void UpdateLean(float dt)
    {
        float leanInput = 0;
        if (Input.IsKeyPressed(Key.Q)) leanInput = 1;  // lean left
        if (Input.IsKeyPressed(Key.E)) leanInput = -1;  // lean right

        _currentLean = Mathf.Lerp(_currentLean, leanInput, dt * LeanSpeed);

        _leanPivot.Rotation = new Vector3(0, 0, Mathf.DegToRad(LeanAngle) * _currentLean);
        _leanPivot.Position = new Vector3(-LeanOffset * _currentLean, 0, 0);
    }

    // --- Camera ---
    private void UpdateCameraHeight(float dt)
    {
        bool isCrouchState = State == MoveState.Crouching || State == MoveState.Sliding;
        float targetY = isCrouchState ? CrouchCameraY : StandCameraY;
        var pos = _cameraHolder.Position;
        pos.Y = Mathf.Lerp(pos.Y, targetY, dt * 12.0f);
        _cameraHolder.Position = pos;
    }

    private void UpdateCameraBob(float dt)
    {
        float speed = new Vector2(Velocity.X, Velocity.Z).Length();
        if (speed > 0.5f && IsOnFloor() && State != MoveState.Sliding)
        {
            float freq = BobFrequency * (speed / WalkSpeed);
            if (State == MoveState.Sprinting) freq *= 1.3f;
            _bobTimer += dt * freq;

            float bobY = Mathf.Sin(_bobTimer) * BobAmplitudeY;
            float bobX = Mathf.Sin(_bobTimer * 0.5f) * BobAmplitudeX;
            _camera.Position = new Vector3(bobX, bobY, 0);
        }
        else
        {
            _bobTimer = 0;
            _camera.Position = _camera.Position.Lerp(Vector3.Zero, dt * 10.0f);
        }
    }

    private void UpdateFov(float dt)
    {
        float targetFov = State == MoveState.Sprinting ? BaseFov + SprintFovBoost : BaseFov;
        _camera.Fov = Mathf.Lerp(_camera.Fov, targetFov, dt * 8.0f);
    }

    private void UpdateSlideTilt(float dt)
    {
        float targetTilt = State == MoveState.Sliding ? Mathf.DegToRad(SlideTiltDeg) : 0;
        var holderRot = _cameraHolder.Rotation;
        holderRot.Z = Mathf.Lerp(holderRot.Z, targetTilt, dt * 10.0f);
        _cameraHolder.Rotation = holderRot;
    }

    // --- Collision Shape ---
    private void UpdateCollisionShape(float dt)
    {
        if (_collisionShape == null)
        {
            // Find our collision shape
            foreach (var child in GetChildren())
            {
                if (child is CollisionShape3D cs && cs.Shape is CapsuleShape3D)
                {
                    _collisionShape = cs;
                    break;
                }
            }
            if (_collisionShape == null) return;
        }

        bool isCrouchState = State == MoveState.Crouching || State == MoveState.Sliding;
        float targetHeight = isCrouchState ? CrouchHeight : StandHeight;

        var capsule = _collisionShape.Shape as CapsuleShape3D;
        if (capsule == null) return;

        capsule.Height = Mathf.MoveToward(capsule.Height, targetHeight, dt * 12.0f);
        _collisionShape.Position = new Vector3(0, capsule.Height * 0.5f, 0);
    }

    private bool CanStandUp()
    {
        var spaceState = GetWorld3D().DirectSpaceState;
        var from = GlobalPosition + new Vector3(0, CrouchHeight, 0);
        var to = GlobalPosition + new Vector3(0, StandHeight + 0.1f, 0);
        var query = PhysicsRayQueryParameters3D.Create(from, to);
        query.Exclude = new Godot.Collections.Array<Rid> { GetRid() };
        return spaceState.IntersectRay(query).Count == 0;
    }

    // --- Interaction ---
    private void UpdateInteraction()
    {
        if (_interactRay.IsColliding())
        {
            var collider = _interactRay.GetCollider();
            var interactable = FindInteractable(collider as Node);

            if (interactable != null && interactable.CanInteract(this))
            {
                _promptLabel.Text = $"[F] {interactable.InteractLabel}";

                if (Input.IsKeyPressed(Key.F) && !_interactHeld)
                {
                    _interactHeld = true;
                    interactable.OnInteract(this);
                }
                else if (!Input.IsKeyPressed(Key.F))
                    _interactHeld = false;
                return;
            }
        }
        _interactHeld = false;
        _promptLabel.Text = "";
    }

    private static IInteractable FindInteractable(Node node)
    {
        Node current = node;
        while (current != null)
        {
            if (current is IInteractable i) return i;
            current = current.GetParent();
        }
        if (node != null && node.HasMeta("interactable"))
            return new LegacyInteractableWrapper(node);
        return null;
    }

    // --- HUD ---
    private void UpdateHUD()
    {
        // Health bar
        _hpBar.Value = HP;
        _hpLabel.Text = $"{HP:F0}";
        _hpLabel.AddThemeColorOverride("font_color",
            HP < MaxHP * 0.3f ? new Color(1, 0.3f, 0.2f) : new Color(0.9f, 0.9f, 0.9f));

        // Stamina bar
        _staminaBar.Value = Stamina;

        // Ammo — magazine system
        if (_weapon != null)
        {
            int magAmmo = _weapon.CurrentMagAmmo;
            int spareMags = _weapon.SpareMags;
            _ammoLabel.Text = $"{magAmmo}  [{spareMags} mags]";
            _ammoLabel.AddThemeColorOverride("font_color",
                magAmmo == 0 ? new Color(1, 0.3f, 0.2f) :
                magAmmo <= 3 ? new Color(1, 0.7f, 0.2f) :
                new Color(0.9f, 0.9f, 0.9f));

            // Reload indicator
            if (_weapon.IsReloading)
                _reloadLabel.Text = "RELOADING...";
            else if (magAmmo == 0 && spareMags > 0)
                _reloadLabel.Text = "[R] RELOAD";
            else if (magAmmo < _weapon.MagazineSize && spareMags > 0)
                _reloadLabel.Text = "";  // Can reload but don't nag
            else
                _reloadLabel.Text = "";
        }

        // Debug state
        float speed = new Vector2(Velocity.X, Velocity.Z).Length();
        _stateLabel.Text = $"{State} | {speed:F1} m/s | Stamina: {Stamina:F0}";
    }

    public void TakeDamage(float amount)
    {
        if (UseSharedOnFootHealth)
        {
            Starlight.Game.Runtime.PlayerState.ApplyOnFootDamage(amount);
            HP = Starlight.Game.Runtime.PlayerState.OnFootHealth;
            return;
        }

        HP -= amount;
        if (HP <= 0)
        {
            HP = 0;
            GD.Print("Player died!");
        }
    }

    private class LegacyInteractableWrapper : IInteractable
    {
        private readonly Node _node;
        public LegacyInteractableWrapper(Node node) { _node = node; }
        public string InteractLabel => _node.GetMeta("interact_label", "Interact").AsString();
        public bool CanInteract(FPSController player) => true;
        public void OnInteract(FPSController player) => GD.Print($"Interacted with: {_node.Name}");
    }
}
