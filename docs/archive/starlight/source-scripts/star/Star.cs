using Godot;

namespace Starlight.Star;

/// <summary>
/// Attached to a Sun scene root. Applies a main-sequence stellar class color palette to the
/// sun body, halo, flares, and light on ready. Same mesh / same shaders — only the color
/// parameters change, so one Sun.tscn covers any star type across sectors.
/// </summary>
public partial class Star : Node3D
{
	public enum StellarClass
	{
		O, // 30,000K+  deep blue
		B, // 10,000K   blue-white
		A,  // 7,500K   white
		F,  // 6,000K   yellow-white
		G,  // 5,500K   yellow (Sol is G2V)
		K,  // 4,500K   orange
		M,  // 3,000K   red dwarf
		L,  // <2,200K  brown dwarf (sub-stellar, no sustained hydrogen fusion)
	}

	[Export] public StellarClass Class { get; set; } = StellarClass.M;
	[Export] public bool ApplyOnReady { get; set; } = true;

	public override void _Ready()
	{
		if (ApplyOnReady)
			ApplyClass();
	}

	/// <summary>
	/// Push the preset for the currently-selected class into every child material + light.
	/// Safe to call at runtime if the class changes.
	/// </summary>
	public void ApplyClass()
	{
		Preset p = GetPreset(Class);

		if (GetNodeOrNull<MeshInstance3D>("SunMesh") is { } mesh &&
			mesh.MaterialOverride is ShaderMaterial surf)
		{
			surf.SetShaderParameter("core_color", p.Core);
			surf.SetShaderParameter("hot_color", p.Hot);
			surf.SetShaderParameter("sunspot_color", p.Spot);
			surf.SetShaderParameter("emission_strength", p.SurfaceEmission);
		}

		if (GetNodeOrNull<MeshInstance3D>("SunHalo") is { } halo &&
			halo.MaterialOverride is ShaderMaterial haloMat)
		{
			haloMat.SetShaderParameter("flame_color", p.Halo);
			haloMat.SetShaderParameter("hot_color", p.FlareTip);
			haloMat.SetShaderParameter("emission_strength", p.HaloEmission);
		}

		// Flare material lives on the procedurally-built MultiMesh surface; drill into it.
		if (GetNodeOrNull<Node>("SunFlares") is { } flaresRoot &&
			flaresRoot.GetNodeOrNull<MultiMeshInstance3D>("FlareInstances") is { } mmi &&
			mmi.Multimesh?.Mesh is ArrayMesh arr && arr.GetSurfaceCount() > 0 &&
			arr.SurfaceGetMaterial(0) is ShaderMaterial flareMat)
		{
			flareMat.SetShaderParameter("base_color", p.FlareBase);
			flareMat.SetShaderParameter("tip_color", p.FlareTip);
			flareMat.SetShaderParameter("emission_strength", p.FlareEmission);
		}

		if (GetNodeOrNull<OmniLight3D>("StarLight") is { } light)
		{
			light.LightColor = p.LightColor;
			light.LightEnergy = p.LightEnergy;
		}
	}

	private readonly struct Preset
	{
		public Color Core { get; init; }
		public Color Hot { get; init; }
		public Color Spot { get; init; }
		public Color Halo { get; init; }
		public Color FlareBase { get; init; }
		public Color FlareTip { get; init; }
		public Color LightColor { get; init; }
		public float LightEnergy { get; init; }
		public float SurfaceEmission { get; init; }
		public float HaloEmission { get; init; }
		public float FlareEmission { get; init; }
	}

	private static Preset GetPreset(StellarClass cls) => cls switch
	{
		StellarClass.O => new Preset
		{
			Core = new Color(0.72f, 0.84f, 1.0f),
			Hot = new Color(0.38f, 0.55f, 0.95f),
			Spot = new Color(0.10f, 0.18f, 0.40f),
			Halo = new Color(0.62f, 0.78f, 1.0f),
			FlareBase = new Color(0.40f, 0.60f, 1.0f),
			FlareTip = new Color(0.85f, 0.92f, 1.0f),
			LightColor = new Color(0.75f, 0.88f, 1.0f),
			LightEnergy = 15.0f,
			SurfaceEmission = 1.6f,
			HaloEmission = 0.9f,
			FlareEmission = 3.2f,
		},
		StellarClass.B => new Preset
		{
			Core = new Color(0.82f, 0.90f, 1.0f),
			Hot = new Color(0.58f, 0.72f, 0.96f),
			Spot = new Color(0.20f, 0.28f, 0.50f),
			Halo = new Color(0.78f, 0.88f, 1.0f),
			FlareBase = new Color(0.55f, 0.72f, 0.98f),
			FlareTip = new Color(0.90f, 0.95f, 1.0f),
			LightColor = new Color(0.85f, 0.92f, 1.0f),
			LightEnergy = 12.0f,
			SurfaceEmission = 1.9f,
			HaloEmission = 1.0f,
			FlareEmission = 3.6f,
		},
		StellarClass.A => new Preset
		{
			Core = new Color(0.95f, 0.96f, 1.0f),
			Hot = new Color(0.82f, 0.84f, 0.92f),
			Spot = new Color(0.35f, 0.35f, 0.45f),
			Halo = new Color(0.92f, 0.94f, 0.98f),
			FlareBase = new Color(0.80f, 0.84f, 0.96f),
			FlareTip = new Color(0.98f, 0.98f, 1.0f),
			LightColor = new Color(0.95f, 0.96f, 1.0f),
			LightEnergy = 10.0f,
			SurfaceEmission = 2.2f,
			HaloEmission = 1.2f,
			FlareEmission = 4.0f,
		},
		StellarClass.F => new Preset
		{
			Core = new Color(1.0f, 0.96f, 0.78f),
			Hot = new Color(1.0f, 0.78f, 0.45f),
			Spot = new Color(0.50f, 0.32f, 0.15f),
			Halo = new Color(1.0f, 0.90f, 0.55f),
			FlareBase = new Color(1.0f, 0.65f, 0.25f),
			FlareTip = new Color(1.0f, 0.94f, 0.65f),
			LightColor = new Color(1.0f, 0.97f, 0.88f),
			LightEnergy = 8.5f,
			SurfaceEmission = 3.0f,
			HaloEmission = 1.5f,
			FlareEmission = 4.8f,
		},
		StellarClass.G => new Preset
		{
			Core = new Color(1.0f, 0.85f, 0.40f),
			Hot = new Color(1.0f, 0.50f, 0.10f),
			Spot = new Color(0.45f, 0.14f, 0.03f),
			Halo = new Color(1.0f, 0.78f, 0.30f),
			FlareBase = new Color(1.0f, 0.38f, 0.08f),
			FlareTip = new Color(1.0f, 0.85f, 0.45f),
			LightColor = new Color(1.0f, 0.95f, 0.80f),
			LightEnergy = 8.0f,
			SurfaceEmission = 3.8f,
			HaloEmission = 1.8f,
			FlareEmission = 5.5f,
		},
		StellarClass.K => new Preset
		{
			Core = new Color(1.0f, 0.72f, 0.32f),
			Hot = new Color(1.0f, 0.40f, 0.08f),
			Spot = new Color(0.40f, 0.10f, 0.02f),
			Halo = new Color(1.0f, 0.60f, 0.20f),
			FlareBase = new Color(1.0f, 0.28f, 0.05f),
			FlareTip = new Color(1.0f, 0.70f, 0.30f),
			LightColor = new Color(1.0f, 0.82f, 0.60f),
			LightEnergy = 6.5f,
			SurfaceEmission = 4.2f,
			HaloEmission = 2.0f,
			FlareEmission = 6.0f,
		},
		StellarClass.M => new Preset
		{
			Core = new Color(1.0f, 0.25f, 0.10f),
			Hot = new Color(0.80f, 0.12f, 0.04f),
			Spot = new Color(0.28f, 0.04f, 0.01f),
			Halo = new Color(0.90f, 0.20f, 0.08f),
			FlareBase = new Color(1.0f, 0.18f, 0.04f),
			FlareTip = new Color(1.0f, 0.40f, 0.12f),
			LightColor = new Color(1.0f, 0.42f, 0.22f),
			LightEnergy = 4.5f,
			SurfaceEmission = 3.2f,
			HaloEmission = 1.9f,
			FlareEmission = 5.5f,
		},
		StellarClass.L => new Preset
		{
			// Brown dwarf: sub-stellar, no sustained hydrogen fusion.
			// Real spectra peak in IR; artistic rendering leans deep maroon
			// with faint magenta because trace visible emission is that hue.
			Core = new Color(0.55f, 0.10f, 0.12f),
			Hot = new Color(0.30f, 0.04f, 0.08f),
			Spot = new Color(0.10f, 0.02f, 0.04f),
			Halo = new Color(0.45f, 0.08f, 0.18f),
			FlareBase = new Color(0.50f, 0.08f, 0.14f),
			FlareTip = new Color(0.70f, 0.20f, 0.30f),
			LightColor = new Color(0.70f, 0.25f, 0.30f),
			LightEnergy = 1.5f,
			SurfaceEmission = 1.4f,
			HaloEmission = 0.7f,
			FlareEmission = 2.0f,
		},
		_ => GetPreset(StellarClass.G),
	};
}
