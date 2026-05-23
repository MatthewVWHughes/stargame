using Godot;

namespace Starlight.Game.Combat;

/// <summary>
/// Short-lived line segment drawn between two world points, auto-freed after a fixed lifetime.
/// Placeholder visual for on-foot hitscan shots in VS1 — we want a clear "something fired" cue
/// without committing to polished VFX.
/// </summary>
public partial class OnFootBeamTracer : Node3D
{
    [Export] public float Lifetime { get; set; } = 0.05f;

    private float _age;

    public static OnFootBeamTracer Spawn(Node parent, Vector3 fromWorld, Vector3 toWorld, Color color)
    {
        var tracer = new OnFootBeamTracer { Name = "OnFootBeam" };
        parent.AddChild(tracer);
        tracer.Configure(fromWorld, toWorld, color);
        return tracer;
    }

    public void Configure(Vector3 fromWorld, Vector3 toWorld, Color color)
    {
        Vector3 delta = toWorld - fromWorld;
        float length = delta.Length();
        if (length < 0.001f)
        {
            QueueFree();
            return;
        }

        GlobalPosition = fromWorld + delta * 0.5f;
        LookAt(toWorld, Vector3.Up);

        var mesh = new MeshInstance3D
        {
            Mesh = new CylinderMesh
            {
                TopRadius = 0.015f,
                BottomRadius = 0.015f,
                Height = length,
            },
            MaterialOverride = new StandardMaterial3D
            {
                AlbedoColor = color,
                EmissionEnabled = true,
                Emission = color,
                EmissionEnergyMultiplier = 2.4f,
                ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
            },
            RotationDegrees = new Vector3(90f, 0f, 0f),
        };
        AddChild(mesh);
    }

    public override void _Process(double delta)
    {
        _age += (float)delta;
        if (_age >= Lifetime)
            QueueFree();
    }
}
