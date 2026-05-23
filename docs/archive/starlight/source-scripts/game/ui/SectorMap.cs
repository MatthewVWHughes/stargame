using System.Collections.Generic;
using Godot;
using StellarClass = Starlight.Star.Star.StellarClass;

namespace Starlight.Game.UI;

/// <summary>
/// 3D sector map showing the local star network (~50 systems).
/// Real galactic positions. Sol at origin. Press N to toggle.
/// Right-drag orbit, scroll zoom.
/// </summary>
public partial class SectorMap : Node3D
{
	private struct StarData
	{
		public string Name;
		public Vector3 Pos;
		public StellarClass Class;
		public int Tier;
	}

	private StarData[] _stars;
	private List<(int a, int b, bool natural)> _links;
	private Camera3D _cam;
	private float _yaw = 0.3f, _pitch = 0.4f, _dist = 50f;
	private bool _drag, _pan;
	private Vector3 _panTarget = Vector3.Zero;
	private const float MaxPanDist = 50f;
	private int _selectedStar = -1;
	private bool _panMoved;
	private MeshInstance3D _routeLine;
	private Label3D _selectedLabel;
	private const float Ly = 2f;
	private const float ShortRange = 5f;  // ly — bonus connections for nearby systems
	private const float MaxRange = 8f;   // ly — absolute max wormhole reach (for MST edges)

	public override void _Ready()
	{
		BuildEnvironment();
		BuildStarData();
		BuildAutoLinks();
		BuildVisuals();
		BuildCamera();
	}

	private void BuildEnvironment()
	{
		var env = new Godot.Environment();
		var skyMat = new PanoramaSkyMaterial();
		skyMat.Panorama = GD.Load<Texture2D>("res://textures/planets/8k_stars_milky_way.jpg");
		var sky = new Sky { SkyMaterial = skyMat };
		env.Sky = sky;
		env.BackgroundMode = Godot.Environment.BGMode.Sky;
		env.AmbientLightSource = Godot.Environment.AmbientSource.Disabled;
		// Glow bloom lets dim star halos (M/L classes) bleed over the body
		// silhouette — without it the sphere shows through as a hard edge.
		env.GlowEnabled = true;
		env.GlowIntensity = 0.9f;
		env.GlowBloom = 0.2f;
		AddChild(new WorldEnvironment { Environment = env });
	}

	public override void _Process(double delta)
	{
		float x = _dist * Mathf.Cos(_pitch) * Mathf.Sin(_yaw);
		float y = _dist * Mathf.Sin(_pitch);
		float z = _dist * Mathf.Cos(_pitch) * Mathf.Cos(_yaw);
		_cam.GlobalPosition = _panTarget + new Vector3(x, y, z);
		_cam.LookAt(_panTarget, Vector3.Up);
	}

	public override void _UnhandledInput(InputEvent @event)
	{
		if (@event is InputEventMouseButton mb)
		{
			if (mb.ButtonIndex == MouseButton.Right) { _drag = mb.Pressed; GetViewport().SetInputAsHandled(); }
			if (mb.ButtonIndex == MouseButton.Left)
			{
				if (mb.Pressed) { _pan = true; _panMoved = false; }
				else
				{
					_pan = false;
					if (!_panMoved) TrySelectStar(mb.Position);
				}
				GetViewport().SetInputAsHandled();
			}
			if (mb.ButtonIndex == MouseButton.WheelUp) { _dist = Mathf.Max(8f, _dist * 0.9f); GetViewport().SetInputAsHandled(); }
			if (mb.ButtonIndex == MouseButton.WheelDown) { _dist = Mathf.Min(150f, _dist / 0.9f); GetViewport().SetInputAsHandled(); }
		}
		if (@event is InputEventMouseMotion mm)
		{
			if (_drag)
			{
				_yaw += mm.Relative.X * 0.005f;
				_pitch = Mathf.Clamp(_pitch + mm.Relative.Y * 0.005f, -Mathf.Pi * 0.45f, Mathf.Pi * 0.45f);
				GetViewport().SetInputAsHandled();
			}
			else if (_pan)
			{
				_panMoved = true;
				Vector3 right = _cam.GlobalTransform.Basis.X;
				Vector3 up = _cam.GlobalTransform.Basis.Y;
				float panSpeed = _dist * 0.002f;
				_panTarget -= right * mm.Relative.X * panSpeed;
				_panTarget += up * mm.Relative.Y * panSpeed;
				// Clamp pan distance from Sol.
				if (_panTarget.Length() > MaxPanDist)
					_panTarget = _panTarget.Normalized() * MaxPanDist;
				GetViewport().SetInputAsHandled();
			}
		}
	}

	private void TrySelectStar(Vector2 screenPos)
	{
		if (_stars == null || _cam == null) return;

		// Find closest star to click in screen space.
		float bestDist = 20f; // pixel threshold
		int bestIdx = -1;

		for (int i = 0; i < _stars.Length; i++)
		{
			if (_cam.IsPositionBehind(_stars[i].Pos)) continue;
			Vector2 projected = _cam.UnprojectPosition(_stars[i].Pos);
			float d = screenPos.DistanceTo(projected);
			if (d < bestDist) { bestDist = d; bestIdx = i; }
		}

		_selectedStar = bestIdx;
		UpdateRouteLine();
		UpdateSelectedLabel();
	}

	private void UpdateSelectedLabel()
	{
		if (_selectedLabel != null) { _selectedLabel.QueueFree(); _selectedLabel = null; }
		if (_selectedStar < 0 || _stars == null) return;

		float distLy = _stars[_selectedStar].Pos.Length() / Ly;
		var path = FindPath(0, _selectedStar);
		string routeText;
		if (path != null && path.Count >= 2)
		{
			int jumps = path.Count - 1;
			string via = "";
			if (jumps > 1)
			{
				var hops = new List<string>();
				for (int i = 1; i < path.Count - 1; i++)
					hops.Add(_stars[path[i]].Name);
				via = $"\nvia: {string.Join(" > ", hops)}";
			}
			routeText = $"{_stars[_selectedStar].Name}\n{distLy:F1} ly — {jumps} jump{(jumps > 1 ? "s" : "")}{via}";
		}
		else
		{
			routeText = $"{_stars[_selectedStar].Name}\n{distLy:F1} ly — NO ROUTE";
		}

		_selectedLabel = new Label3D
		{
			Text = routeText,
			FontSize = 16,
			PixelSize = 0.008f,
			Position = _stars[_selectedStar].Pos + new Vector3(0f, -0.8f, 0f),
			Billboard = BaseMaterial3D.BillboardModeEnum.Enabled,
			Modulate = new Color(1f, 0.8f, 0.2f, 0.9f),
			NoDepthTest = true,
		};
		AddChild(_selectedLabel);
	}

	private void UpdateRouteLine()
	{
		if (_routeLine != null) { _routeLine.QueueFree(); _routeLine = null; }
		if (_selectedStar < 0 || _selectedStar == 0 || _stars == null || _links == null) return;

		// BFS from Sol (0) to selected star through wormhole links.
		var path = FindPath(0, _selectedStar);
		if (path == null || path.Count < 2) return;

		var imm = new ImmediateMesh();
		var mat = new StandardMaterial3D
		{
			AlbedoColor = new Color(1f, 0.8f, 0.2f, 0.7f),
			ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
			Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
			NoDepthTest = true,
		};

		imm.SurfaceBegin(Mesh.PrimitiveType.Lines, mat);
		for (int i = 0; i < path.Count - 1; i++)
		{
			imm.SurfaceAddVertex(_stars[path[i]].Pos);
			imm.SurfaceAddVertex(_stars[path[i + 1]].Pos);
		}
		imm.SurfaceEnd();

		_routeLine = new MeshInstance3D { Mesh = imm };
		AddChild(_routeLine);
	}

	/// <summary>BFS shortest path (fewest jumps) through the wormhole network.</summary>
	private List<int> FindPath(int from, int to)
	{
		if (from == to) return new List<int> { from };

		// Build adjacency from links.
		var adj = new Dictionary<int, List<int>>();
		foreach (var (a, b, _) in _links)
		{
			if (!adj.ContainsKey(a)) adj[a] = new List<int>();
			if (!adj.ContainsKey(b)) adj[b] = new List<int>();
			adj[a].Add(b);
			adj[b].Add(a);
		}

		// BFS.
		var prev = new Dictionary<int, int>();
		var queue = new Queue<int>();
		queue.Enqueue(from);
		prev[from] = -1;

		while (queue.Count > 0)
		{
			int cur = queue.Dequeue();
			if (cur == to)
			{
				// Reconstruct path.
				var path = new List<int>();
				for (int n = to; n != -1; n = prev[n])
					path.Add(n);
				path.Reverse();
				return path;
			}

			if (!adj.ContainsKey(cur)) continue;
			foreach (int neighbor in adj[cur])
			{
				if (prev.ContainsKey(neighbor)) continue;
				prev[neighbor] = cur;
				queue.Enqueue(neighbor);
			}
		}

		return null; // no path
	}

	private void BuildStarData()
	{
		// All known systems within ~16.3 ly. Real galactic coords (X toward center, Y rotation, Z north pole).
		// Godot mapping: game_X = galactic_X, game_Y = galactic_Z (up), game_Z = galactic_Y.
		_stars = new StarData[]
		{
			// Tier 0
			St("Sol", 0,0,0, StellarClass.G, 0),
			// Tier 1 — Inner colonies
			St("Alpha Centauri", -1.64f,-1.37f,-3.84f, StellarClass.G, 1),      // G2V primary (B is K1V)
			St("Barnard's Star", -0.01f,-5.94f,0.49f, StellarClass.M, 1),
			St("Sirius", -5.76f,-4.07f,-2.23f, StellarClass.A, 1),              // A1V + DA companion
			St("Epsilon Eridani", -6.03f,-1.88f,-7.80f, StellarClass.K, 1),     // K2V
			St("Tau Ceti", -6.87f,-0.19f,-9.25f, StellarClass.G, 1),            // G8V
			// Tier 2 — Mid-range
			St("Procyon", -7.19f,-7.72f,3.35f, StellarClass.F, 2),              // F5IV-V + DA
			St("Lalande 21185", -3.41f,-6.53f,3.69f, StellarClass.M, 2),
			St("Ross 128", -6.23f,-7.11f,2.79f, StellarClass.M, 2),
			St("Epsilon Indi", 2.19f,-4.17f,-10.37f, StellarClass.K, 2),        // K5V
			St("61 Cygni", 5.28f,-8.27f,5.13f, StellarClass.K, 2),              // K5V + K7V
			St("Ross 248", 3.37f,-9.72f,2.46f, StellarClass.M, 2),
			// Tier 3 — Frontier
			St("Wolf 359", -4.07f,-4.44f,3.36f, StellarClass.M, 3),
			St("Ross 154", 1.91f,-8.85f,-3.62f, StellarClass.M, 3),
			St("UV Ceti", -2.28f,-0.31f,-8.46f, StellarClass.M, 3),             // M5.5V flare binary
			St("Kapteyn's Star", -4.71f,4.19f,-10.25f, StellarClass.M, 3),      // M1 subdwarf
			St("Teegarden's Star", -1.37f,-10.42f,6.80f, StellarClass.M, 3),
			St("YZ Ceti", -3.17f,-0.07f,-11.63f, StellarClass.M, 3),
			St("40 Eridani", -9.34f,-2.67f,-10.44f, StellarClass.K, 3),         // K0.5V + DA + M4.5V
			St("Kruger 60", 4.45f,-9.73f,6.31f, StellarClass.M, 3),
			// Tier 4 — Deep frontier
			St("Sigma Draconis", 0.42f,-14.19f,10.30f, StellarClass.K, 4),      // K0V (borderline G9)
			St("Delta Pavonis", 1.56f,-2.29f,-19.43f, StellarClass.G, 4),       // G8IV subgiant
			St("Eta Cassiopeiae", 1.39f,-13.54f,11.98f, StellarClass.G, 4),     // G0V + K7V
			St("Altair", -1.56f,-15.49f,-4.88f, StellarClass.A, 4),             // A7V
			// New systems — filling to ~50
			St("Luhman 16", 0.49f,-0.58f,-6.47f, StellarClass.L, 3),            // binary brown dwarf
			St("WISE 0855", -0.97f,-3.39f,6.55f, StellarClass.L, 3),            // sub-brown dwarf (Y-type irl, closest we have is L)
			St("Lacaille 9352", 3.53f,-1.60f,-9.99f, StellarClass.M, 2),
			St("EZ Aquarii", 2.07f,-1.88f,-10.80f, StellarClass.M, 3),
			St("Struve 2398", 4.20f,-9.67f,4.59f, StellarClass.M, 3),
			St("Groombridge 34", 0.34f,-5.07f,10.47f, StellarClass.M, 3),
			St("DX Cancri", -5.98f,-9.37f,3.35f, StellarClass.M, 3),
			St("GJ 1061", -3.50f,2.13f,-11.16f, StellarClass.M, 3),
			St("Luyten's Star", -3.26f,-10.41f,-5.58f, StellarClass.M, 2),
			St("SCR 1845", 2.53f,-3.09f,-11.91f, StellarClass.M, 3),            // M8.5 + T6 brown dwarf; primary is M
			St("Lacaille 8760", 4.98f,-0.91f,-11.85f, StellarClass.M, 2),
			St("Ross 614", -6.95f,-9.17f,5.88f, StellarClass.M, 3),
			St("Wolf 1061", -0.07f,-7.31f,-11.73f, StellarClass.M, 2),
			St("Wolf 424", -5.53f,-12.39f,4.70f, StellarClass.M, 3),
			St("GJ 687", 2.17f,-14.27f,2.89f, StellarClass.M, 3),
			St("LHS 292", -8.42f,-8.79f,7.74f, StellarClass.M, 3),
			St("GJ 1245", 4.63f,-12.71f,5.78f, StellarClass.M, 3),
			St("Gliese 674", -0.51f,-5.07f,-13.94f, StellarClass.M, 3),
			St("GJ 440", 4.75f,0.79f,-14.33f, StellarClass.A, 3),               // DQ white dwarf — closest visual match is A
			St("Gliese 876", 5.09f,-5.77f,-12.71f, StellarClass.M, 2),
			St("Gliese 412", -2.03f,-13.33f,8.20f, StellarClass.M, 3),
			St("Groombridge 1618", -2.40f,-11.18f,10.93f, StellarClass.K, 3),
			St("AD Leonis", -5.14f,-13.72f,6.47f, StellarClass.M, 3),
			St("GJ 1002", -1.05f,1.30f,-15.22f, StellarClass.M, 4),
			St("Gliese 832", 6.64f,-2.72f,-14.40f, StellarClass.M, 3),
		};
	}

	private static StarData St(string name, float gx, float gy, float gz, StellarClass cls, int tier)
	{
		return new StarData { Name = name, Pos = new Vector3(gx, gz, gy) * Ly, Class = cls, Tier = tier };
	}

	/// <summary>
	/// Label / UI accent color for a stellar class. Mirrors the Halo color from
	/// Star.cs's presets so map labels look like dim versions of the star itself.
	/// </summary>
	private static Color ClassColor(StellarClass cls) => cls switch
	{
		StellarClass.O => new Color(0.62f, 0.78f, 1.0f),
		StellarClass.B => new Color(0.78f, 0.88f, 1.0f),
		StellarClass.A => new Color(0.92f, 0.94f, 0.98f),
		StellarClass.F => new Color(1.0f, 0.90f, 0.55f),
		StellarClass.G => new Color(1.0f, 0.78f, 0.30f),
		StellarClass.K => new Color(1.0f, 0.60f, 0.20f),
		StellarClass.M => new Color(0.90f, 0.20f, 0.08f),
		StellarClass.L => new Color(0.45f, 0.08f, 0.18f),
		_ => Colors.White,
	};

	private void BuildAutoLinks()
	{
		_links = new List<(int, int, bool)>();
		int n = _stars.Length;
		var linked = new HashSet<long>();

		// Step 1: Minimum spanning tree (Prim's) — guarantees full connectivity.
		var inTree = new bool[n];
		var minEdge = new float[n];
		var minFrom = new int[n];
		for (int i = 0; i < n; i++) { minEdge[i] = float.MaxValue; minFrom[i] = -1; }
		minEdge[0] = 0f;

		for (int iter = 0; iter < n; iter++)
		{
			// Pick the cheapest node not yet in tree.
			int u = -1;
			for (int i = 0; i < n; i++)
				if (!inTree[i] && (u == -1 || minEdge[i] < minEdge[u]))
					u = i;
			if (u == -1) break;
			inTree[u] = true;

			if (minFrom[u] >= 0)
			{
				int a = Mathf.Min(u, minFrom[u]), b = Mathf.Max(u, minFrom[u]);
				long key = a * 10000L + b;
				if (linked.Add(key))
					_links.Add((a, b, false));
			}

			// Update neighbors.
			for (int v = 0; v < n; v++)
			{
				if (inTree[v]) continue;
				float d = _stars[u].Pos.DistanceTo(_stars[v].Pos);
				if (d < minEdge[v]) { minEdge[v] = d; minFrom[v] = u; }
			}
		}

		// Step 2: Add short-range bonus connections (local shortcuts).
		float shortSq = ShortRange * ShortRange * Ly * Ly;
		for (int i = 0; i < n; i++)
			for (int j = i + 1; j < n; j++)
			{
				if (_stars[i].Pos.DistanceSquaredTo(_stars[j].Pos) > shortSq) continue;
				long key = i * 10000L + j;
				if (linked.Add(key))
					_links.Add((i, j, false));
			}

		// Step 3: Natural wormhole (criminal shortcut — ignores range limits).
		int wolf359 = FindStar("Wolf 359");
		int ross154 = FindStar("Ross 154");
		if (wolf359 >= 0 && ross154 >= 0)
		{
			long key = Mathf.Min(wolf359, ross154) * 10000L + Mathf.Max(wolf359, ross154);
			if (linked.Add(key))
				_links.Add((wolf359, ross154, true));
		}
	}

	private int FindStar(string name)
	{
		for (int i = 0; i < _stars.Length; i++)
			if (_stars[i].Name == name) return i;
		return -1;
	}

	private void BuildVisuals()
	{
		// Galactic plane grid.
		var gridImm = new ImmediateMesh();
		var gridMat = new StandardMaterial3D
		{
			AlbedoColor = new Color(0.3f, 0.3f, 0.5f, 0.3f),
			ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
			Transparency = BaseMaterial3D.TransparencyEnum.Alpha, NoDepthTest = true,
		};
		float ext = 50f, step = 2f;
		gridImm.SurfaceBegin(Mesh.PrimitiveType.Lines, gridMat);
		for (float i = -ext; i <= ext; i += step)
		{
			gridImm.SurfaceAddVertex(new Vector3(i, 0f, -ext));
			gridImm.SurfaceAddVertex(new Vector3(i, 0f, ext));
			gridImm.SurfaceAddVertex(new Vector3(-ext, 0f, i));
			gridImm.SurfaceAddVertex(new Vector3(ext, 0f, i));
		}
		gridImm.SurfaceEnd();
		AddChild(new MeshInstance3D { Mesh = gridImm });

		// Thick white axis lines.
		var axisImm = new ImmediateMesh();
		var axisMat = new StandardMaterial3D
		{
			AlbedoColor = new Color(1f, 1f, 1f, 0.35f),
			ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
			Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
		};
		axisImm.SurfaceBegin(Mesh.PrimitiveType.Lines, axisMat);
		for (float o = -0.03f; o <= 0.03f; o += 0.015f)
		{
			axisImm.SurfaceAddVertex(new Vector3(-ext, 0f, o));
			axisImm.SurfaceAddVertex(new Vector3(ext, 0f, o));
			axisImm.SurfaceAddVertex(new Vector3(o, 0f, -ext));
			axisImm.SurfaceAddVertex(new Vector3(o, 0f, ext));
		}
		axisImm.SurfaceEnd();
		AddChild(new MeshInstance3D { Mesh = axisImm });

		// Drop lines + star orbs + labels.
		var dropImm = new ImmediateMesh();
		var dropMat = new StandardMaterial3D
		{
			AlbedoColor = new Color(0.5f, 0.5f, 0.6f, 0.2f),
			ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
			Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
		};
		dropImm.SurfaceBegin(Mesh.PrimitiveType.Lines, dropMat);

		var sunMapScene = GD.Load<PackedScene>("res://scenes/game/space/Sun_map.tscn");

		for (int i = 0; i < _stars.Length; i++)
		{
			ref var s = ref _stars[i];
			float sz = s.Tier switch { 0 => 0.5f, 1 => 0.35f, 2 => 0.25f, 3 => 0.2f, _ => 0.15f };
			Color accent = ClassColor(s.Class);

			// Shader-based star body + halo. Star.cs applies the class palette on _Ready.
			var sun = sunMapScene.Instantiate<Starlight.Star.Star>();
			sun.Class = s.Class;
			sun.Position = s.Pos;
			sun.Scale = Vector3.One * sz;
			AddChild(sun);

			AddChild(new Label3D
			{
				Text = s.Name, FontSize = 14, PixelSize = 0.01f,
				Position = s.Pos + new Vector3(0f, sz + 0.12f, 0f),
				Billboard = BaseMaterial3D.BillboardModeEnum.Enabled,
				Modulate = new Color(accent.R, accent.G, accent.B, 0.7f),
				NoDepthTest = true,
			});

			// Drop line to galactic plane + circle where it meets.
			if (Mathf.Abs(s.Pos.Y) > 0.1f)
			{
				dropImm.SurfaceAddVertex(s.Pos);
				dropImm.SurfaceAddVertex(new Vector3(s.Pos.X, 0f, s.Pos.Z));
			}

			// Small circle on the plane showing the star's XZ position.
			var dot = new MeshInstance3D
			{
				Mesh = new TorusMesh
				{
					InnerRadius = 0.12f,
					OuterRadius = 0.18f,
					Rings = 16,
					RingSegments = 4,
				},
				Position = new Vector3(s.Pos.X, 0f, s.Pos.Z),
				MaterialOverride = new StandardMaterial3D
				{
					AlbedoColor = new Color(accent.R, accent.G, accent.B, 0.4f),
					ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
					Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
					NoDepthTest = true,
				},
			};
			AddChild(dot);
		}

		dropImm.SurfaceEnd();
		AddChild(new MeshInstance3D { Mesh = dropImm });
	}

	private void BuildCamera()
	{
		_cam = new Camera3D { Fov = 50f, Near = 0.1f, Far = 500f };
		AddChild(_cam);
		_cam.Current = true;
	}
}
