using Godot;

namespace Starlight.Game.Space;

/// <summary>
/// Procedurally populates a sparse asteroid belt using MultiMeshInstance3D.
/// Rocks are static geometry with no collision — Sol's belt is deliberately thin
/// and mostly empty space, so collision is deferred until hazard-rock gameplay is added.
/// </summary>
public partial class AsteroidBelt : Node3D
{
    [Export] public float InnerRadius { get; set; } = 660000f;
    [Export] public float OuterRadius { get; set; } = 960000f;
    [Export] public float VerticalSpread { get; set; } = 8000f;
    [Export] public int InstanceCount { get; set; } = 400;
    [Export] public float MinRockSize { get; set; } = 15f;
    [Export] public float MaxRockSize { get; set; } = 120f;
    [Export] public int RandomSeed { get; set; } = 13579;

    public override void _Ready()
    {
        var rockMesh = new SphereMesh
        {
            Radius = 1f,
            Height = 2f,
            RadialSegments = 8,
            Rings = 4,
        };

        var rockMaterial = new StandardMaterial3D
        {
            AlbedoColor = new Color(0.45f, 0.4f, 0.36f),
            Roughness = 0.95f,
            Metallic = 0.05f,
        };
        rockMesh.Material = rockMaterial;

        var multiMesh = new MultiMesh
        {
            TransformFormat = MultiMesh.TransformFormatEnum.Transform3D,
            Mesh = rockMesh,
            InstanceCount = InstanceCount,
        };

        var rng = new RandomNumberGenerator();
        rng.Seed = (ulong)RandomSeed;

        for (int i = 0; i < InstanceCount; i++)
        {
            float theta = rng.RandfRange(0f, Mathf.Tau);
            float radius = Mathf.Lerp(InnerRadius, OuterRadius, Mathf.Sqrt(rng.Randf()));
            float height = rng.RandfRange(-VerticalSpread, VerticalSpread);
            float size = rng.RandfRange(MinRockSize, MaxRockSize);

            var basis = new Basis(Vector3.Up, rng.RandfRange(0f, Mathf.Tau))
                * new Basis(Vector3.Right, rng.RandfRange(0f, Mathf.Tau))
                * new Basis(Vector3.Forward, rng.RandfRange(0f, Mathf.Tau));
            basis = basis.Scaled(new Vector3(size, size, size));

            var origin = new Vector3(
                radius * Mathf.Cos(theta),
                height,
                radius * Mathf.Sin(theta));

            multiMesh.SetInstanceTransform(i, new Transform3D(basis, origin));
        }

        var mmi = new MultiMeshInstance3D
        {
            Name = "BeltInstances",
            Multimesh = multiMesh,
            CastShadow = GeometryInstance3D.ShadowCastingSetting.Off,
            ExtraCullMargin = 500000f,
        };
        AddChild(mmi);
    }
}
