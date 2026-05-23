using Godot;

namespace Starlight.Camera;

/// <summary>
/// Free-fly camera. WASD + QE for movement, right-click + mouse for look.
/// Scroll wheel adjusts speed.
/// </summary>
public partial class FreeCamera : Camera3D
{
    [Export] public float MoveSpeed { get; set; } = 50f;
    [Export] public float MinSpeed { get; set; } = 5f;
    [Export] public float MaxSpeed { get; set; } = 500f;
    [Export] public float LookSensitivity { get; set; } = 0.002f;
    [Export] public float SpeedScrollStep { get; set; } = 10f;

    private bool _looking;

    public override void _Ready()
    {
    }

    public override void _UnhandledInput(InputEvent @event)
    {
        if (@event is InputEventMouseButton mb)
        {
            if (mb.ButtonIndex == MouseButton.Right)
            {
                _looking = mb.Pressed;
                Input.MouseMode = _looking ? Input.MouseModeEnum.Captured : Input.MouseModeEnum.Visible;
            }
            else if (mb.Pressed && mb.ButtonIndex == MouseButton.WheelUp)
            {
                MoveSpeed = Mathf.Min(MoveSpeed + SpeedScrollStep, MaxSpeed);
            }
            else if (mb.Pressed && mb.ButtonIndex == MouseButton.WheelDown)
            {
                MoveSpeed = Mathf.Max(MoveSpeed - SpeedScrollStep, MinSpeed);
            }
        }

        if (@event is InputEventMouseMotion mm && _looking)
        {
            RotateY(-mm.Relative.X * LookSensitivity);
            RotateObjectLocal(Vector3.Right, -mm.Relative.Y * LookSensitivity);
        }
    }

    public override void _Process(double delta)
    {
        var velocity = Vector3.Zero;
        float dt = (float)delta;

        if (Input.IsKeyPressed(Key.W)) velocity -= Transform.Basis.Z;
        if (Input.IsKeyPressed(Key.S)) velocity += Transform.Basis.Z;
        if (Input.IsKeyPressed(Key.A)) velocity -= Transform.Basis.X;
        if (Input.IsKeyPressed(Key.D)) velocity += Transform.Basis.X;
        if (Input.IsKeyPressed(Key.E) || Input.IsKeyPressed(Key.Space)) velocity += Transform.Basis.Y;
        if (Input.IsKeyPressed(Key.Q) || Input.IsKeyPressed(Key.Shift)) velocity -= Transform.Basis.Y;

        float speedMult = Input.IsKeyPressed(Key.Ctrl) ? 3f : 1f;

        if (velocity.LengthSquared() > 0)
            Position += velocity.Normalized() * MoveSpeed * speedMult * dt;
    }
}
