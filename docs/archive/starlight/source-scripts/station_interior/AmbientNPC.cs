using Godot;

namespace Starlight.StationInterior;

/// <summary>
/// Friendly NPC that wanders between waypoints and can be interacted with.
/// State transitions are driven by <see cref="AmbientNpcLogic"/>; movement,
/// animation, and environment queries live here.
/// </summary>
public partial class AmbientNPC : CharacterBody3D
{
    [Export] public float WalkSpeed = 2.0f;
    [Export] public float IdleTimeMin = 3.0f;
    [Export] public float IdleTimeMax = 8.0f;
    [Export] public float RotationSpeed = 5.0f;
    [Export] public bool Interactable = false;
    [Export] public string InteractLabel = "Talk";

    private NavigationAgent3D _agent;
    private AnimationPlayer _animPlayer;
    private Node3D _waypointContainer;
    private readonly AmbientNpcLogic _logic = new();
    private float _idleTimer;
    private Node3D _playerRef;
    private string _currentAnim = "";
    private bool _wasInFacingRange;

    public override void _Ready()
    {
        _agent = new NavigationAgent3D();
        _agent.PathDesiredDistance = 0.5f;
        _agent.TargetDesiredDistance = 0.5f;
        _agent.MaxSpeed = WalkSpeed;
        AddChild(_agent);

        _idleTimer = GD.Randf() * IdleTimeMax;

        if (Interactable)
        {
            SetMeta("interactable", true);
            SetMeta("interact_label", InteractLabel);
        }

        _logic.Start();

        // Find animation player (created by StationInteriorBuilder)
        CallDeferred(MethodName.FindAnimPlayer);
    }

    private void FindAnimPlayer()
    {
        _animPlayer = FindChildOfType<AnimationPlayer>(this);
        if (_animPlayer == null) return;

        // Re-set root_node now that everything is in the scene tree
        var charModel = _animPlayer.GetParent();
        _animPlayer.RootNode = _animPlayer.GetPathTo(charModel);

        PlayAnim("ual1/Idle");
    }

    public void SetWaypoints(Node3D container) => _waypointContainer = container;
    public void SetPlayer(Node3D player) => _playerRef = player;

    public override void _PhysicsProcess(double delta)
    {
        float dt = (float)delta;

        if (!IsOnFloor())
        {
            var vel = Velocity;
            vel.Y -= 9.8f * dt;
            Velocity = vel;
            MoveAndSlide();
            return;
        }

        // Feed proximity inputs to the state machine. Edge-triggered so we don't spam.
        if (Interactable && _playerRef != null)
        {
            float dist = GlobalPosition.DistanceTo(_playerRef.GlobalPosition);
            bool inRange = dist < 3.0f;
            if (inRange && !_wasInFacingRange)
                _logic.Input(new AmbientNpcLogic.Input.PlayerApproached());
            else if (!inRange && _wasInFacingRange)
                _logic.Input(new AmbientNpcLogic.Input.PlayerDeparted());
            _wasInFacingRange = inRange;
        }

        switch (_logic.Value)
        {
            case AmbientNpcLogic.State.FacingPlayer:
                PlayAnim("ual1/Idle_Talking");
                if (_playerRef != null)
                    FaceTarget(_playerRef.GlobalPosition, dt);
                Velocity = Vector3.Zero;
                break;

            case AmbientNpcLogic.State.Idle:
                Velocity = Vector3.Zero;
                _idleTimer -= dt;
                if (_idleTimer <= 0)
                    TryStartWalking();
                PlayIdleAnim();
                break;

            case AmbientNpcLogic.State.Walking:
                if (_agent.IsNavigationFinished())
                {
                    _logic.Input(new AmbientNpcLogic.Input.WaypointReached());
                    _idleTimer = IdleTimeMin + GD.Randf() * (IdleTimeMax - IdleTimeMin);
                    Velocity = Vector3.Zero;
                }
                else
                {
                    var nextPos = _agent.GetNextPathPosition();
                    var dir = (nextPos - GlobalPosition).Normalized();
                    dir.Y = 0;
                    Velocity = dir * WalkSpeed;
                    FaceTarget(nextPos, dt);
                    PlayAnim("ual1/Walk");
                }
                break;
        }

        MoveAndSlide();
    }

    private void TryStartWalking()
    {
        if (_waypointContainer == null || _waypointContainer.GetChildCount() == 0)
        {
            _idleTimer = IdleTimeMax;
            _logic.Input(new AmbientNpcLogic.Input.NoWaypointsAvailable());
            return;
        }

        int idx = (int)(GD.Randi() % _waypointContainer.GetChildCount());
        if (_waypointContainer.GetChild(idx) is not Node3D wp)
        {
            _logic.Input(new AmbientNpcLogic.Input.NoWaypointsAvailable());
            return;
        }

        _agent.TargetPosition = wp.GlobalPosition;
        _logic.Input(new AmbientNpcLogic.Input.IdleTimerExpired());
        PlayAnim("ual1/Walk");
    }

    private void PlayIdleAnim()
    {
        // Random idle variant picked once per Idle entry
        string idleAnim = GD.Randf() < 0.3f ? "ual2/Idle_FoldArms" : "ual1/Idle";
        if (_currentAnim != idleAnim)
            PlayAnim(idleAnim);
    }

    private void PlayAnim(string name)
    {
        if (_animPlayer == null || name == _currentAnim) return;
        if (_animPlayer.HasAnimation(name))
        {
            _animPlayer.Play(name);
            _currentAnim = name;
        }
    }

    private void FaceTarget(Vector3 target, float dt)
    {
        var dir = (target - GlobalPosition);
        dir.Y = 0;
        if (dir.LengthSquared() < 0.01f) return;
        float targetAngle = Mathf.Atan2(-dir.X, -dir.Z);
        var rot = Rotation;
        rot.Y = Mathf.LerpAngle(rot.Y, targetAngle, RotationSpeed * dt);
        Rotation = rot;
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
}
