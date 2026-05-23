using Godot;

namespace Starlight.Game.Combat;

/// <summary>
/// Small world marker that spawns a hostile when the player approaches.
/// </summary>
public partial class CombatSpawnPoint : Node3D
{
	[Export] public string DisplayName { get; set; } = "Combat Spawn";
	[Export] public int WingSize { get; set; } = 4;
	[Export] public float ActivationRange { get; set; } = 3200f;
	[Export] public float RespawnDelay { get; set; } = 18f;
	[Export] public float SpawnRadiusMin { get; set; } = 260f;
	[Export] public float SpawnRadiusMax { get; set; } = 640f;

	public int ActiveHostileCount => _activeHostiles.Count;

	private float _respawnTimer;
	private readonly Godot.Collections.Array<HostileEncounterController> _activeHostiles = new();

	public override void _Ready()
	{
		BuildVisual();
	}

	public void UpdateSpawnTimer(float delta)
	{
		if (_respawnTimer > 0f)
			_respawnTimer = Mathf.Max(0f, _respawnTimer - delta);

		for (int i = _activeHostiles.Count - 1; i >= 0; i--)
		{
			HostileEncounterController hostile = _activeHostiles[i];
			if (hostile == null || !IsInstanceValid(hostile) || !hostile.IsAlive)
				_activeHostiles.RemoveAt(i);
		}
	}

	public bool CanSpawn(Vector3 playerPosition)
	{
		return _activeHostiles.Count < WingSize &&
			   _respawnTimer <= 0f &&
			   playerPosition.DistanceTo(GlobalPosition) <= ActivationRange;
	}

	public void BindHostile(HostileEncounterController hostile)
	{
		if (hostile != null)
			_activeHostiles.Add(hostile);
	}

	public void NotifyHostileDestroyed()
	{
		if (_activeHostiles.Count <= 1)
			_respawnTimer = RespawnDelay;
	}

	public Vector3 GetSpawnPosition()
	{
		float angle = GD.Randf() * Mathf.Tau;
		float radius = Mathf.Lerp(SpawnRadiusMin, SpawnRadiusMax, GD.Randf());
		return GlobalPosition + new Vector3(Mathf.Cos(angle) * radius, 0f, Mathf.Sin(angle) * radius);
	}

	private void BuildVisual()
	{
		if (GetNodeOrNull<Node3D>("VisualRoot") != null)
			return;

		var visualRoot = new Node3D { Name = "VisualRoot" };
		AddChild(visualRoot);

		var material = new StandardMaterial3D
		{
			AlbedoColor = new Color(0.95f, 0.42f, 0.16f),
			EmissionEnabled = true,
			Emission = new Color(0.95f, 0.42f, 0.16f),
			EmissionEnergyMultiplier = 1.3f,
			ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
		};

		var beacon = new MeshInstance3D
		{
			Name = "Beacon",
			Mesh = new CylinderMesh
			{
				TopRadius = 20f,
				BottomRadius = 20f,
				Height = 100f,
				RadialSegments = 10,
			},
			MaterialOverride = material,
			CastShadow = GeometryInstance3D.ShadowCastingSetting.Off,
		};
		visualRoot.AddChild(beacon);

		var label = new Label3D
		{
			Name = "Label",
			Position = new Vector3(0f, 80f, 0f),
			Billboard = BaseMaterial3D.BillboardModeEnum.Enabled,
			Text = DisplayName,
			FontSize = 28,
			Modulate = new Color(1f, 0.87f, 0.72f),
		};
		visualRoot.AddChild(label);
	}
}
