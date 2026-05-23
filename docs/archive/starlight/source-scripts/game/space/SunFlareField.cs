using Godot;

namespace Starlight.Game.Space;

/// <summary>
/// Animated solar prominence field. Each flare is a bundle of many thin strands arranged
/// along a semicircular arch anchored to two footpoints on the sun's surface — mirroring
/// real coronal loop photography where plasma follows magnetic field lines as a fibrous
/// cluster rather than a single smooth tube.
/// Per-instance phase/frequency drives an apex rise-and-fall pulse independently per arch.
/// </summary>
public partial class SunFlareField : Node3D
{
    private const string FlareShaderPath = "res://shaders/SunFlare.gdshader";

    [Export] public int FlareCount { get; set; } = 22;
    [Export] public float SphereRadius { get; set; } = 2000f;
    [Export] public float MinHalfAngleDeg { get; set; } = 3f;
    [Export] public float MaxHalfAngleDeg { get; set; } = 14f;
    [Export] public int StrandsPerBundle { get; set; } = 22;
    [Export] public float StrandSpread { get; set; } = 0.18f;
    [Export] public float TubeRadius { get; set; } = 0.012f;
    [Export] public int PathSegments { get; set; } = 26;
    [Export] public int TubeSegments { get; set; } = 4;
    [Export] public Color BaseColor { get; set; } = new Color(1f, 0.38f, 0.08f);
    [Export] public Color TipColor { get; set; } = new Color(1f, 0.85f, 0.45f);
    [Export] public float EmissionStrength { get; set; } = 5.5f;
    [Export] public float SwayAmount { get; set; } = 0.2f;
    [Export] public float SwaySpeed { get; set; } = 0.1f;
    [Export] public float RimSharpness { get; set; } = 2.5f;
    [Export] public float MinPulseFrequency { get; set; } = 0.12f;
    [Export] public float MaxPulseFrequency { get; set; } = 0.35f;
    [Export] public int MeshSeed { get; set; } = 31415;
    [Export] public int PlacementSeed { get; set; } = 98765;

    public override void _Ready()
    {
        var archMesh = (ArrayMesh)BuildBundleMesh(
            PathSegments, TubeSegments, TubeRadius,
            StrandsPerBundle, StrandSpread, MeshSeed);

        var shader = GD.Load<Shader>(FlareShaderPath);
        var material = new ShaderMaterial { Shader = shader };
        material.SetShaderParameter("base_color", BaseColor);
        material.SetShaderParameter("tip_color", TipColor);
        material.SetShaderParameter("emission_strength", EmissionStrength);
        material.SetShaderParameter("sway_amount", SwayAmount);
        material.SetShaderParameter("sway_speed", SwaySpeed);
        material.SetShaderParameter("rim_sharpness", RimSharpness);
        material.SetShaderParameter("min_scale", 0.12f);
        archMesh.SurfaceSetMaterial(0, material);

        var multiMesh = new MultiMesh
        {
            TransformFormat = MultiMesh.TransformFormatEnum.Transform3D,
            UseCustomData = true,
            Mesh = archMesh,
            InstanceCount = FlareCount,
        };

        var rng = new RandomNumberGenerator { Seed = (ulong)PlacementSeed };
        float minAlpha = Mathf.DegToRad(MinHalfAngleDeg);
        float maxAlpha = Mathf.DegToRad(MaxHalfAngleDeg);

        for (int i = 0; i < FlareCount; i++)
        {
            Vector3 dirM = SampleUnitSphere(rng);

            Vector3 seedPerp = Mathf.Abs(dirM.Y) > 0.95f ? Vector3.Right : Vector3.Up;
            Vector3 chordDir = dirM.Cross(seedPerp).Normalized();
            float tilt = rng.RandfRange(0f, Mathf.Tau);
            chordDir = chordDir.Rotated(dirM, tilt);

            float alpha = rng.RandfRange(minAlpha, maxAlpha);
            float archSize = SphereRadius * Mathf.Sin(alpha);
            float midRadius = SphereRadius * Mathf.Cos(alpha);

            Vector3 chordMid = dirM * midRadius;
            Vector3 up = dirM;
            Vector3 binormal = chordDir.Cross(up).Normalized();

            Basis basis = new Basis(chordDir * archSize, up * archSize, binormal * archSize);
            multiMesh.SetInstanceTransform(i, new Transform3D(basis, chordMid));

            float phase = rng.Randf();
            float freq = rng.RandfRange(MinPulseFrequency, MaxPulseFrequency);
            float seed = rng.RandfRange(0f, 100f);
            multiMesh.SetInstanceCustomData(i, new Color(phase, freq, seed, 1f));
        }

        var mmi = new MultiMeshInstance3D
        {
            Name = "FlareInstances",
            Multimesh = multiMesh,
            CastShadow = GeometryInstance3D.ShadowCastingSetting.Off,
            ExtraCullMargin = 100000f,
        };
        AddChild(mmi);
    }

    /// <summary>
    /// Builds a bundle of thin strand-tubes that together trace a semicircular arch from
    /// (-1,0,0) through (0,1,0) to (+1,0,0). Each strand has a randomized perpendicular
    /// offset that fades to zero at the footpoints (strands converge where they meet the
    /// sun surface and fan out in the middle). A small per-strand sinusoidal wobble along
    /// the path adds hairiness. UV.x stores the arch-path parameter (0 = foot A, 1 = foot B)
    /// so the shader can do endpoint fades + apex-based color without touching VERTEX.
    /// </summary>
    private static Mesh BuildBundleMesh(
        int pathSegments,
        int tubeSegments,
        float tubeRadius,
        int strandCount,
        float strandSpread,
        int meshSeed)
    {
        int vertsPerStrand = (pathSegments + 1) * tubeSegments;
        int vertexCount = strandCount * vertsPerStrand;
        var vertices = new Vector3[vertexCount];
        var normals = new Vector3[vertexCount];
        var uvs = new Vector2[vertexCount];

        int quadsPerStrand = pathSegments * tubeSegments;
        int indexCount = strandCount * quadsPerStrand * 6;
        var indices = new int[indexCount];
        int triIdx = 0;

        var rng = new RandomNumberGenerator { Seed = (ulong)meshSeed };

        for (int s = 0; s < strandCount; s++)
        {
            float offsetAngle = rng.RandfRange(0f, Mathf.Tau);
            float offsetMag = Mathf.Sqrt(rng.Randf()) * strandSpread;
            float offNormal = Mathf.Cos(offsetAngle) * offsetMag;
            float offBinormal = Mathf.Sin(offsetAngle) * offsetMag;

            float wavePhase = rng.RandfRange(0f, Mathf.Tau);
            float waveFreq = rng.RandfRange(2f, 6f);
            float waveAmp = strandSpread * 0.12f * rng.RandfRange(0.4f, 1.0f);

            int vOffset = s * vertsPerStrand;

            for (int i = 0; i <= pathSegments; i++)
            {
                float t = (float)i / pathSegments;
                float angle = t * Mathf.Pi;

                Vector3 pathPos = new Vector3(-Mathf.Cos(angle), Mathf.Sin(angle), 0f);
                Vector3 tangent = new Vector3(Mathf.Sin(angle), Mathf.Cos(angle), 0f).Normalized();
                Vector3 inPlaneNormal = new Vector3(-tangent.Y, tangent.X, 0f);
                Vector3 binormal = Vector3.Forward;

                float spread = Mathf.Sin(angle);
                float waveN = Mathf.Sin(t * waveFreq * Mathf.Pi + wavePhase) * waveAmp * spread;
                float waveB = Mathf.Cos(t * waveFreq * Mathf.Pi + wavePhase + 1.1f) * waveAmp * spread;

                Vector3 strandPath = pathPos
                    + inPlaneNormal * (offNormal * spread + waveN)
                    + binormal * (offBinormal * spread + waveB);

                for (int j = 0; j < tubeSegments; j++)
                {
                    float u = (float)j / tubeSegments * Mathf.Tau;
                    Vector3 offset = (Mathf.Cos(u) * inPlaneNormal + Mathf.Sin(u) * binormal) * tubeRadius;
                    int idx = vOffset + i * tubeSegments + j;
                    vertices[idx] = strandPath + offset;
                    normals[idx] = offset.Normalized();
                    uvs[idx] = new Vector2(t, (float)j / tubeSegments);
                }
            }

            for (int i = 0; i < pathSegments; i++)
            {
                for (int j = 0; j < tubeSegments; j++)
                {
                    int a = vOffset + i * tubeSegments + j;
                    int b = vOffset + i * tubeSegments + (j + 1) % tubeSegments;
                    int c = vOffset + (i + 1) * tubeSegments + j;
                    int d = vOffset + (i + 1) * tubeSegments + (j + 1) % tubeSegments;

                    indices[triIdx++] = a;
                    indices[triIdx++] = b;
                    indices[triIdx++] = d;
                    indices[triIdx++] = a;
                    indices[triIdx++] = d;
                    indices[triIdx++] = c;
                }
            }
        }

        var arrays = new Godot.Collections.Array();
        arrays.Resize((int)Mesh.ArrayType.Max);
        arrays[(int)Mesh.ArrayType.Vertex] = vertices;
        arrays[(int)Mesh.ArrayType.Normal] = normals;
        arrays[(int)Mesh.ArrayType.TexUV] = uvs;
        arrays[(int)Mesh.ArrayType.Index] = indices;

        var mesh = new ArrayMesh();
        mesh.AddSurfaceFromArrays(Mesh.PrimitiveType.Triangles, arrays);
        return mesh;
    }

    private static Vector3 SampleUnitSphere(RandomNumberGenerator rng)
    {
        float u = rng.RandfRange(-1f, 1f);
        float theta = rng.RandfRange(0f, Mathf.Tau);
        float r = Mathf.Sqrt(1f - u * u);
        return new Vector3(r * Mathf.Cos(theta), u, r * Mathf.Sin(theta));
    }
}
