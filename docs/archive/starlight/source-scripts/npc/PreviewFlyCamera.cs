using Godot;

namespace Starlight.Npc;

public partial class PreviewFlyCamera : Camera3D
{
    [Export] public float MoveSpeed = 4.0f;
    [Export] public float FastMultiplier = 3.0f;
    [Export] public float MouseSensitivity = 0.0025f;

    private bool _captured;

    public override void _Ready()
    {
        Current = true;
    }

    public override void _UnhandledInput(InputEvent evt)
    {
        if (evt is InputEventMouseButton mouseButton && mouseButton.ButtonIndex == MouseButton.Right)
        {
            _captured = mouseButton.Pressed;
            Input.MouseMode = _captured ? Input.MouseModeEnum.Captured : Input.MouseModeEnum.Visible;
            GetViewport().SetInputAsHandled();
        }
        else if (_captured && evt is InputEventMouseMotion motion)
        {
            RotateY(-motion.Relative.X * MouseSensitivity);
            var rotation = Rotation;
            rotation.X = Mathf.Clamp(
                rotation.X - motion.Relative.Y * MouseSensitivity,
                Mathf.DegToRad(-85.0f),
                Mathf.DegToRad(85.0f));
            Rotation = rotation;
            GetViewport().SetInputAsHandled();
        }
        else if (evt is InputEventKey key && key.Pressed && key.Keycode == Key.Escape)
        {
            _captured = false;
            Input.MouseMode = Input.MouseModeEnum.Visible;
        }
    }

    public override void _Process(double delta)
    {
        var input = Vector3.Zero;
        if (Input.IsKeyPressed(Key.W)) input.Z -= 1.0f;
        if (Input.IsKeyPressed(Key.S)) input.Z += 1.0f;
        if (Input.IsKeyPressed(Key.A)) input.X -= 1.0f;
        if (Input.IsKeyPressed(Key.D)) input.X += 1.0f;
        if (Input.IsKeyPressed(Key.E)) input.Y += 1.0f;
        if (Input.IsKeyPressed(Key.Q)) input.Y -= 1.0f;

        if (input == Vector3.Zero)
            return;

        var speed = MoveSpeed;
        if (Input.IsKeyPressed(Key.Shift))
            speed *= FastMultiplier;
        GlobalPosition += GlobalBasis * input.Normalized() * speed * (float)delta;
    }
}
