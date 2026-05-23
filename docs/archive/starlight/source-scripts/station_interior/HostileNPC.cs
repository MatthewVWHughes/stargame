using Godot;
using System.Collections.Generic;

namespace Starlight.StationInterior;

/// <summary>
/// Enemy NPC with states: Idle, Patrol, Hunting, Engage, Attack, Retreat, Dead.
/// Uses CoverManager for tactical positioning and SquadCoordinator for team behavior.
/// </summary>
public partial class HostileNPC : CharacterBody3D
{
    // --- Exports (tunable per archetype) ---
    [Export] public float WalkSpeed = 2.5f;
    [Export] public float RunSpeed = 4.5f;
    [Export] public float SightRange = 25.0f;
    [Export] public float SightConeAngle = 80.0f;
    [Export] public float MaxHP = 50.0f;
    [Export] public float Damage = 8.0f;
    [Export] public float FireInterval = 1.5f;
    [Export] public float RotationSpeed = 6.0f;
    [Export] public float Accuracy = 0.1f;
    [Export] public float PeekDuration = 1.5f;
    [Export] public float HideDuration = 3.0f;
    [Export] public float FleeThreshold = 0.3f;
    [Export] public float PatrolIdleTime = 3.0f;
    [Export] public float PreferredDistance = 10.0f;

    public enum State { Idle, Patrol, Hunting, Engage, Attack, Retreat, Dead }

    public State CurrentState { get; private set; } = State.Idle;
    public float HP { get; private set; }
    public bool HasAttackToken { get; set; }

    // --- References ---
    private Node3D _playerRef;
    private CoverManager _coverManager;
    private CombatArena _arena;
    private SquadCoordinator _squad;

    // --- Internal ---
    private NavigationAgent3D _agent;
    private NavySpecOpsCharacter _charController;
    private float _stateTimer;
    private float _fireTimer;
    private float _huntTimer;
    private float _repositionTimer;
    private Vector3 _lastKnownPlayerPos;
    private Vector3 _navTarget;
    private CoverPoint _currentCover;
    private bool _isPeeking;
    private Vector3 _peekOffset;
    private Vector3 _slideTarget;
    private bool _isSliding;

    // Patrol
    private readonly List<Vector3> _patrolPoints = new();
    private int _patrolIndex;
    private bool _patrolWaiting;
    private float _patrolStuckTimer;

    // --- Debug ---
    private MeshInstance3D _debugConeCenter;
    private MeshInstance3D _debugConeLeft;
    private MeshInstance3D _debugConeRight;
    private StandardMaterial3D _debugMat;
    private Label3D _debugLabel;
    private int _debugFrameCount;

    // ===================== SETUP =====================

    public void Initialize(Node3D player, CoverManager coverManager, CombatArena arena, SquadCoordinator squad = null)
    {
        _playerRef = player;
        _coverManager = coverManager;
        _arena = arena;
        _squad = squad;
    }

    public void SetPatrolPoints(List<Vector3> points)
    {
        _patrolPoints.Clear();
        _patrolPoints.AddRange(points);
        if (_patrolPoints.Count > 0 && CurrentState == State.Idle)
        {
            _patrolIndex = 0;
            SetState(State.Patrol);
        }
    }

    public override void _Ready()
    {
        HP = MaxHP;

        _agent = new NavigationAgent3D();
        _agent.PathDesiredDistance = 0.2f;  // tight — don't skip corner waypoints
        _agent.TargetDesiredDistance = 0.5f;
        _agent.AvoidanceEnabled = true;
        _agent.Radius = 0.35f;  // slightly larger than capsule for clearance
        AddChild(_agent);

        // Debug vision cone
        _debugMat = new StandardMaterial3D
        {
            ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
            Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
            AlbedoColor = new Color(0, 1, 0, 0.6f),
        };
        _debugConeCenter = CreateDebugBeam(_debugMat);
        _debugConeLeft = CreateDebugBeam(_debugMat);
        _debugConeRight = CreateDebugBeam(_debugMat);

        _debugLabel = new Label3D
        {
            Billboard = BaseMaterial3D.BillboardModeEnum.Enabled,
            FontSize = 48,
            OutlineSize = 8,
            Modulate = Colors.White,
            Position = new Vector3(0, 2.2f, 0),
        };
        AddChild(_debugLabel);

        CallDeferred(MethodName.FindCharController);
    }

    // ===================== DAMAGE =====================

    public void TakeDamage(float amount)
    {
        if (CurrentState == State.Dead) return;

        HP -= amount;
        if (HP <= 0)
        {
            HP = 0;
            Die();
            return;
        }

        _charController?.TravelTo("Hit");
        // Animation state reset handled by AnimationTree travel

        if (_playerRef != null)
            _lastKnownPlayerPos = _playerRef.GlobalPosition;

        // Any hit forces combat
        if (CurrentState != State.Engage && CurrentState != State.Attack && CurrentState != State.Retreat)
            SetState(State.Engage);

        _squad?.OnNPCDamaged(this);
    }

    // ===================== MAIN TICK =====================

    public override void _PhysicsProcess(double delta)
    {
        float dt = (float)delta;

        _debugFrameCount++;
        if (_debugFrameCount <= 5)
            GD.Print($"[{Name}] frame={_debugFrameCount} pos={GlobalPosition} onFloor={IsOnFloor()} state={CurrentState}");

        UpdateDebugSightLine();
        if (_debugLabel != null)
        {
            string token = HasAttackToken ? " [ATK]" : "";
            string peek = (CurrentState == State.Attack && _isPeeking) ? " PEEK" : "";
            _debugLabel.Text = $"{CurrentState}{token}{peek}\nHP:{HP:F0}";
        }

        if (CurrentState == State.Dead)
        {
            HideDebugCone();
            Velocity = Vector3.Zero;
            MoveAndSlide();
            return;
        }

        if (!IsOnFloor())
        {
            var vel = Velocity;
            vel.Y -= 9.8f * dt;
            Velocity = vel;
            MoveAndSlide();
            return;
        }

        switch (CurrentState)
        {
            case State.Idle: UpdateIdle(dt); break;
            case State.Patrol: UpdatePatrol(dt); break;
            case State.Hunting: UpdateHunting(dt); break;
            case State.Engage: UpdateEngage(dt); break;
            case State.Attack: UpdateAttack(dt); break;
            case State.Retreat: UpdateRetreat(dt); break;
        }

        MoveAndSlide();
    }

    // ===================== IDLE =====================

    private void UpdateIdle(float dt)
    {
        Velocity = Vector3.Zero;
        SetAnim(NavySpecOpsCharacter.CombatState.Idle);

        if (CheckForPlayer()) return;
        if (CheckForNoise()) return;

        _stateTimer -= dt;
        if (_stateTimer <= 0 && _patrolPoints.Count > 0)
            SetState(State.Patrol);
    }

    // ===================== PATROL =====================

    private void UpdatePatrol(float dt)
    {
        if (CheckForPlayer()) return;
        if (CheckForNoise()) return;

        if (_patrolPoints.Count == 0) { SetState(State.Idle); return; }

        var target = _patrolPoints[_patrolIndex];
        float dist = new Vector2(GlobalPosition.X - target.X, GlobalPosition.Z - target.Z).Length();

        if (_patrolWaiting)
        {
            Velocity = Vector3.Zero;
            _stateTimer -= dt;
            SetAnim(NavySpecOpsCharacter.CombatState.Idle);
            if (_stateTimer <= 0)
            {
                _patrolWaiting = false;
                _patrolIndex = (_patrolIndex + 1) % _patrolPoints.Count;
            }
        }
        else if (dist < 1.0f)
        {
            _patrolWaiting = true;
            _patrolStuckTimer = 0;
            _stateTimer = PatrolIdleTime + GD.Randf() * 2.0f;
            Velocity = Vector3.Zero;
        }
        else
        {
            MoveToward(target, WalkSpeed, dt);
            SetAnim(NavySpecOpsCharacter.CombatState.Walking);
            _patrolStuckTimer += dt;
            if (_patrolStuckTimer > 6.0f)
            {
                _patrolStuckTimer = 0;
                _patrolIndex = (_patrolIndex + 1) % _patrolPoints.Count;
            }
        }
    }

    // ===================== HUNTING =====================

    private void UpdateHunting(float dt)
    {
        if (CheckForPlayer()) return;

        float dist = GlobalPosition.DistanceTo(_lastKnownPlayerPos);
        if (dist < 1.5f)
        {
            Velocity = Vector3.Zero;
            _huntTimer -= dt;
            var rot = Rotation;
            rot.Y += 1.5f * dt;
            Rotation = rot;
            SetAnim(NavySpecOpsCharacter.CombatState.Idle);

            if (_huntTimer <= 0)
                SetState(State.Idle);
        }
        else
        {
            MoveToward(_lastKnownPlayerPos, RunSpeed, dt);
            SetAnim(NavySpecOpsCharacter.CombatState.Running);
        }
    }

    // ===================== ENGAGE (seeking cover) =====================

    private void UpdateEngage(float dt)
    {
        if (_playerRef == null) { SetState(State.Idle); return; }

        // Keep tracking player if visible
        bool canSee = PerceptionSystem.CanSeeTarget(this, _playerRef, SightRange, SightConeAngle);
        if (canSee)
            _lastKnownPlayerPos = _playerRef.GlobalPosition;

        // Re-evaluate cover while approaching — player may have moved
        _repositionTimer -= dt;
        if (_currentCover != null && _repositionTimer <= 0)
        {
            _repositionTimer = 1.0f; // check every second
            if (!_currentCover.IsValidAgainst(_lastKnownPlayerPos))
            {
                // Current cover no longer faces the threat — drop it and pick new
                ReleaseCover();
            }
        }

        // Do we have cover to go to?
        if (_currentCover == null)
        {
            var cover = _coverManager?.FindBestCover(this, _lastKnownPlayerPos);
            if (cover != null && _coverManager.Reserve(cover, this))
            {
                _currentCover = cover;
            }
            else
            {
                // No cover — rush directly at player
                if (canSee)
                {
                    FaceTarget(_playerRef.GlobalPosition, dt);
                    MoveToward(_playerRef.GlobalPosition, RunSpeed, dt);
                    SetAnim(NavySpecOpsCharacter.CombatState.Running);
                    _fireTimer -= dt;
                    if (_fireTimer <= 0 && HasAttackToken)
                    {
                        FireAtPlayer(0.15f);
                        _fireTimer = FireInterval * 0.7f;
                    }
                }
                else
                {
                    SetState(State.Hunting);
                }
                return;
            }
        }

        // Move to cover
        UpdateDebugPath();
        float distToCover = new Vector2(GlobalPosition.X - _currentCover.GlobalPosition.X,
            GlobalPosition.Z - _currentCover.GlobalPosition.Z).Length();
        if (distToCover < 1.2f)
        {
            // Snap to cover position
            StartSlideToCover();
            SetState(State.Attack);
            return;
        }

        MoveToward(_currentCover.GlobalPosition, RunSpeed, dt);
        SetAnim(NavySpecOpsCharacter.CombatState.Running);
    }

    // ===================== ATTACK (at cover, peeking + firing) =====================

    private void UpdateAttack(float dt)
    {
        if (_playerRef == null) { SetState(State.Idle); return; }

        // Finish sliding into cover position
        if (_isSliding)
        {
            UpdateSlide(dt);
            return;
        }

        bool canSee = PerceptionSystem.CanSeeTarget(this, _playerRef, SightRange, SightConeAngle);
        if (canSee)
            _lastKnownPlayerPos = _playerRef.GlobalPosition;

        // Flee check
        if (HP < MaxHP * FleeThreshold && FleeThreshold > 0)
        {
            SetState(State.Retreat);
            return;
        }

        // Periodic reposition check
        _repositionTimer -= dt;
        if (_repositionTimer <= 0)
        {
            _repositionTimer = 3.0f + GD.Randf() * 2.0f;
            if (_coverManager != null && _coverManager.ShouldReposition(this, _lastKnownPlayerPos))
            {
                ReleaseCover();
                SetState(State.Engage);
                return;
            }
        }

        // Peek / hide cycle
        _stateTimer -= dt;
        _fireTimer -= dt;

        if (_isPeeking)
        {
            // Peeking out — face threat through peek offset
            var peekTarget = _lastKnownPlayerPos;
            FaceTarget(peekTarget, dt);
            Velocity = Vector3.Zero;
            SetAnim(NavySpecOpsCharacter.CombatState.Idle);

            // Fire if we have LOS from peek position and an attack token
            if (HasAttackToken && _fireTimer <= 0)
            {
                var spaceState = GetWorld3D().DirectSpaceState;
                bool peekLOS = _currentCover != null
                    ? _currentCover.HasPeekLOS(spaceState, _lastKnownPlayerPos)
                    : canSee;

                if (peekLOS)
                {
                    FireAtPlayer();
                    _fireTimer = FireInterval;
                }
            }

            if (_stateTimer <= 0)
            {
                _isPeeking = false;
                _stateTimer = HideDuration + GD.Randf() * 1.0f;
            }
        }
        else
        {
            // Hiding behind cover
            Velocity = Vector3.Zero;
            SetAnim(NavySpecOpsCharacter.CombatState.Crouching);

            if (_stateTimer <= 0)
            {
                _isPeeking = true;
                _stateTimer = PeekDuration + GD.Randf() * 0.5f;
                // Determine peek side
                if (_currentCover != null)
                    _peekOffset = _currentCover.GetPeekOffset(_lastKnownPlayerPos);
            }
        }

        // Lost contact for too long while hiding — go hunt
        if (!canSee && !_isPeeking)
        {
            _huntTimer -= dt;
            if (_huntTimer <= 0)
            {
                ReleaseCover();
                SetState(State.Hunting);
            }
        }
        else
        {
            _huntTimer = 5.0f; // reset while we have contact or are peeking
        }
    }

    // ===================== RETREAT =====================

    private void UpdateRetreat(float dt)
    {
        bool canSee = PerceptionSystem.CanSeeTarget(this, _playerRef, SightRange, SightConeAngle);
        if (canSee)
            _lastKnownPlayerPos = _playerRef.GlobalPosition;

        // Do we have retreat cover?
        if (_currentCover == null)
        {
            var cover = _coverManager?.FindBestCover(this, _lastKnownPlayerPos);
            if (cover != null && _coverManager.Reserve(cover, this))
                _currentCover = cover;
        }

        if (_currentCover != null)
        {
            float dist = new Vector2(GlobalPosition.X - _currentCover.GlobalPosition.X,
                GlobalPosition.Z - _currentCover.GlobalPosition.Z).Length();
            if (dist < 1.2f)
            {
                StartSlideToCover();
                SetState(State.Attack);
                return;
            }
            MoveToward(_currentCover.GlobalPosition, RunSpeed, dt);
        }
        else
        {
            // No cover — run away from player
            var awayDir = (GlobalPosition - _lastKnownPlayerPos).Normalized();
            awayDir.Y = 0;
            MoveToward(GlobalPosition + awayDir * 10f, RunSpeed, dt);
        }

        SetAnim(NavySpecOpsCharacter.CombatState.Running);

        // Fire while retreating (inaccurate)
        if (canSee && HasAttackToken)
        {
            _fireTimer -= dt;
            if (_fireTimer <= 0)
            {
                FireAtPlayer(0.2f);
                _fireTimer = FireInterval * 1.5f; // slower while retreating
            }
        }
    }

    // ===================== SHARED CHECKS =====================

    private bool CheckForPlayer()
    {
        if (PerceptionSystem.CanSeeTarget(this, _playerRef, SightRange, SightConeAngle))
        {
            _lastKnownPlayerPos = _playerRef.GlobalPosition;
            SetState(State.Engage);
            return true;
        }
        return false;
    }

    private bool CheckForNoise()
    {
        var noises = PerceptionSystem.GetNoisesInRange(GlobalPosition, 25.0f);
        if (noises.Count > 0)
        {
            _lastKnownPlayerPos = noises[0].Position;
            SetState(State.Hunting);
            return true;
        }
        return false;
    }

    // ===================== FIRING =====================

    private void FireAtPlayer(float accuracyPenalty = 0f)
    {
        if (_playerRef == null) return;

        float totalSpread = Accuracy + accuracyPenalty;
        var eyePos = GlobalPosition + new Vector3(0, 1.5f, 0);
        if (_isPeeking && _peekOffset != Vector3.Zero)
            eyePos += _peekOffset;

        var targetPos = _playerRef.GlobalPosition + new Vector3(0, 1.2f, 0);
        var dir = (targetPos - eyePos).Normalized();
        dir += new Vector3(
            (GD.Randf() - 0.5f) * totalSpread,
            (GD.Randf() - 0.5f) * totalSpread,
            (GD.Randf() - 0.5f) * totalSpread
        );

        var spaceState = GetWorld3D().DirectSpaceState;
        var query = PhysicsRayQueryParameters3D.Create(eyePos, eyePos + dir.Normalized() * SightRange);
        query.CollisionMask = 0b0000_1001;
        query.Exclude = new Godot.Collections.Array<Rid> { GetRid() };
        var result = spaceState.IntersectRay(query);

        if (result.Count > 0)
        {
            var collider = result["collider"].As<Node>();
            if (collider is FPSController player)
                player.TakeDamage(Damage);
            else if (collider?.GetParent() is FPSController playerParent)
                playerParent.TakeDamage(Damage);
        }

        SetAnim(NavySpecOpsCharacter.CombatState.Firing);
        // Animation state reset handled by AnimationTree travel

        PerceptionSystem.EmitNoise(GlobalPosition, 20.0f, PerceptionSystem.NoiseType.Gunshot,
            Time.GetTicksMsec() / 1000.0);
    }

    // ===================== DEATH =====================

    private void Die()
    {
        SetState(State.Dead);
        _charController?.TravelTo("Death");

        ReleaseCover();
        HasAttackToken = false;

        SetDeferred("collision_layer", (uint)0);
        SetDeferred("collision_mask", (uint)0);

        _arena?.OnEnemyDied(this);
        GD.Print($"{Name} killed");
    }

    // ===================== STATE TRANSITIONS =====================

    private void SetState(State newState)
    {
        // Clean up old state
        if (CurrentState == State.Attack || CurrentState == State.Engage)
        {
            if (newState != State.Attack && newState != State.Engage && newState != State.Retreat)
                ReleaseCover();
        }

        CurrentState = newState;

        switch (newState)
        {
            case State.Idle:
                _stateTimer = 2.0f + GD.Randf() * 2.0f;
                break;
            case State.Patrol:
                _patrolWaiting = false;
                _stateTimer = PatrolIdleTime;
                break;
            case State.Hunting:
                _huntTimer = 5.0f;
                break;
            case State.Engage:
                // Keep existing cover if we have one (repositioning)
                break;
            case State.Attack:
                _isPeeking = false;
                _stateTimer = 0.5f; // brief hide before first peek
                _repositionTimer = 3.0f + GD.Randf() * 2.0f;
                _huntTimer = 5.0f;
                _squad?.OnNPCSpottedPlayer(this, _lastKnownPlayerPos);
                break;
            case State.Retreat:
                ReleaseCover();
                _fireTimer = 0.5f;
                break;
        }
    }

    public void AlertToPosition(Vector3 position)
    {
        if (CurrentState == State.Dead) return;
        _lastKnownPlayerPos = position;
        if (CurrentState == State.Idle || CurrentState == State.Patrol)
            SetState(State.Hunting);
    }

    // --- Backward compat for StationInteriorBuilder ---
    public void SetPlayer(Node3D player) => _playerRef = player;
    public void SetMeshInstance(MeshInstance3D mesh) { }
    public void SetWaypoints(Node3D container) { }
    public void SetCoverPoints(Node3D container) { }

    // ===================== HELPERS =====================

    /// <summary>
    /// Start a smooth slide to the cover position instead of teleporting.
    /// </summary>
    private void StartSlideToCover()
    {
        if (_currentCover == null) return;

        var targetPos = _currentCover.GlobalPosition;
        targetPos.Y = GlobalPosition.Y;

        // Raycast toward wall to find pushback position
        var spaceState = GetWorld3D().DirectSpaceState;
        var checkFrom = targetPos + new Vector3(0, 0.5f, 0);
        var checkDir = -_currentCover.CoverNormal.Normalized();
        var query = PhysicsRayQueryParameters3D.Create(checkFrom, checkFrom + checkDir * 0.5f);
        query.CollisionMask = 0b0000_0001;
        var result = spaceState.IntersectRay(query);

        if (result.Count > 0)
        {
            var wallPos = result["position"].AsVector3();
            var wallNormal = result["normal"].AsVector3();
            wallNormal.Y = 0;
            targetPos = wallPos + wallNormal.Normalized() * 0.4f;
            targetPos.Y = GlobalPosition.Y;
        }

        _slideTarget = targetPos;
        _isSliding = true;
    }

    /// <summary>
    /// Smoothly move toward the slide target. Returns true when arrived.
    /// </summary>
    private bool UpdateSlide(float dt)
    {
        if (!_isSliding) return true;

        var dir = (_slideTarget - GlobalPosition);
        dir.Y = 0;
        float dist = dir.Length();

        if (dist < 0.1f)
        {
            GlobalPosition = new Vector3(_slideTarget.X, GlobalPosition.Y, _slideTarget.Z);
            _isSliding = false;
            Velocity = Vector3.Zero;

            // Face the cover normal
            if (_currentCover != null)
            {
                var normal = _currentCover.CoverNormal;
                normal.Y = 0;
                if (normal.LengthSquared() > 0.01f)
                    Rotation = new Vector3(0, Mathf.Atan2(-normal.X, -normal.Z), 0);
            }
            return true;
        }

        Velocity = dir.Normalized() * RunSpeed * 1.5f; // quick slide
        FaceTarget(_slideTarget, dt);
        return false;
    }

    private void ReleaseCover()
    {
        if (_currentCover != null)
        {
            _coverManager?.Release(this);
            _currentCover = null;
        }
    }

    private void MoveToward(Vector3 target, float speed, float dt)
    {
        if (_agent != null && target.DistanceTo(_navTarget) > 1.0f)
        {
            _navTarget = target;
            // Snap target to nav mesh — cover points may be in the erosion zone
            var navMap = GetWorld3D().NavigationMap;
            var snapped = NavigationServer3D.MapGetClosestPoint(navMap, target);
            _agent.TargetPosition = snapped;
        }

        Vector3 nextPos;
        if (_agent != null && _agent.IsTargetReachable())
            nextPos = _agent.GetNextPathPosition();
        else
            nextPos = target;

        var dir = (nextPos - GlobalPosition);
        dir.Y = 0;
        if (dir.LengthSquared() < 0.01f) { Velocity = Vector3.Zero; return; }
        dir = dir.Normalized();
        Velocity = dir * speed;
        FaceTarget(nextPos, dt);
    }

    private void FaceTarget(Vector3 target, float dt)
    {
        var dir = target - GlobalPosition;
        dir.Y = 0;
        if (dir.LengthSquared() < 0.01f) return;
        float targetAngle = Mathf.Atan2(-dir.X, -dir.Z);
        var rot = Rotation;
        rot.Y = Mathf.LerpAngle(rot.Y, targetAngle, RotationSpeed * dt);
        Rotation = rot;
    }

    private void SetAnim(NavySpecOpsCharacter.CombatState state)
    {
        _charController?.SetState(state);
    }

    private void FindCharController()
    {
        _charController = FindChildOfType<NavySpecOpsCharacter>(this);
        if (_charController != null)
            SetAnim(NavySpecOpsCharacter.CombatState.Idle);
    }

    private static T FindChildOfType<T>(Node parent) where T : Node
    {
        foreach (var child in parent.GetChildren())
        {
            if (child is T found) return found;
            var result = FindChildOfType<T>(child);
            if (result != null) return result;
        }
        return null;
    }

    // ===================== DEBUG PATH =====================

    private readonly List<MeshInstance3D> _debugPathMarkers = new();

    private void UpdateDebugPath()
    {
        // Clear old markers
        foreach (var m in _debugPathMarkers)
            m.QueueFree();
        _debugPathMarkers.Clear();

        if (_agent == null || CurrentState == State.Idle || CurrentState == State.Dead) return;

        // Draw target position
        var targetMarker = CreateDebugSphere(new Color(1, 1, 0, 0.6f), 0.2f);
        targetMarker.GlobalPosition = _agent.TargetPosition + new Vector3(0, 0.3f, 0);
        _debugPathMarkers.Add(targetMarker);

        // Draw next path position
        if (_agent.IsTargetReachable())
        {
            var nextPos = _agent.GetNextPathPosition();
            var nextMarker = CreateDebugSphere(new Color(0, 1, 1, 0.6f), 0.15f);
            nextMarker.GlobalPosition = nextPos + new Vector3(0, 0.2f, 0);
            _debugPathMarkers.Add(nextMarker);

            // Draw "reachable" label at target
            GD.Print($"[{Name}] nav: target={_agent.TargetPosition} next={nextPos} dist={GlobalPosition.DistanceTo(_agent.TargetPosition):F1} reachable={_agent.IsTargetReachable()}");
        }
        else
        {
            GD.Print($"[{Name}] nav: target={_agent.TargetPosition} NOT REACHABLE — using direct movement");
        }
    }

    private MeshInstance3D CreateDebugSphere(Color color, float radius)
    {
        var mat = new StandardMaterial3D
        {
            ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
            Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
            AlbedoColor = color,
        };
        var mesh = new SphereMesh { Radius = radius, Height = radius * 2 };
        mesh.SurfaceSetMaterial(0, mat);
        var inst = new MeshInstance3D { Mesh = mesh, TopLevel = true };
        GetTree().Root.AddChild(inst);
        return inst;
    }

    // ===================== DEBUG VISUALS =====================

    private MeshInstance3D CreateDebugBeam(StandardMaterial3D mat)
    {
        var cyl = new CylinderMesh { TopRadius = 0.015f, BottomRadius = 0.015f, Height = 1.0f };
        cyl.SurfaceSetMaterial(0, mat);
        var inst = new MeshInstance3D { Mesh = cyl, TopLevel = true };
        AddChild(inst);
        return inst;
    }

    private void PositionBeam(MeshInstance3D beam, Vector3 from, Vector3 to)
    {
        var diff = to - from;
        float dist = diff.Length();
        if (dist < 0.01f) return;
        if (beam.Mesh is CylinderMesh cyl) cyl.Height = dist;
        var mid = (from + to) / 2.0f;
        var dir = diff.Normalized();
        var arb = Mathf.Abs(dir.Dot(Vector3.Up)) > 0.99f ? Vector3.Right : Vector3.Up;
        var right = dir.Cross(arb).Normalized();
        var fwd = right.Cross(dir).Normalized();
        beam.GlobalTransform = new Transform3D(new Basis(right, dir, fwd), mid);
    }

    private void UpdateDebugSightLine()
    {
        if (_debugConeCenter == null) return;
        var eyePos = GlobalPosition + new Vector3(0, 1.5f, 0);
        var forward = -GlobalTransform.Basis.Z;
        forward.Y = 0;
        if (forward.LengthSquared() < 0.001f) forward = Vector3.Forward;
        forward = forward.Normalized();
        float halfAngle = Mathf.DegToRad(SightConeAngle / 2.0f);
        PositionBeam(_debugConeCenter, eyePos, eyePos + forward * SightRange);
        PositionBeam(_debugConeLeft, eyePos, eyePos + forward.Rotated(Vector3.Up, halfAngle) * SightRange);
        PositionBeam(_debugConeRight, eyePos, eyePos + forward.Rotated(Vector3.Up, -halfAngle) * SightRange);

        bool canSee = _playerRef != null && PerceptionSystem.CanSeeTarget(this, _playerRef, SightRange, SightConeAngle);
        _debugMat.AlbedoColor = canSee ? new Color(1, 0, 0, 0.8f) : new Color(0, 1, 0, 0.4f);
    }

    private void HideDebugCone()
    {
        if (_debugConeCenter != null) _debugConeCenter.Visible = false;
        if (_debugConeLeft != null) _debugConeLeft.Visible = false;
        if (_debugConeRight != null) _debugConeRight.Visible = false;
    }
}
