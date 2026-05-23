using Godot;
using Starlight.Npc;

namespace Starlight.Game.UI;

/// <summary>
/// Shared data for a trackable celestial body.
/// </summary>
public struct CelestialBody
{
	public string Name;
	public Node3D Node;
	public Color MapColor;
	public float Radius;
	public float OrbitRadius;
}

/// <summary>
/// Top-down 2D system map. Press M to toggle.
/// Scroll wheel to zoom. Click to set target.
/// Shows planets, stations, trade routes, and NPC ship markers.
/// </summary>
public partial class SystemMap : CanvasLayer
{
	private MapPanel _panel;

	public bool IsOpen => _panel?.Visible ?? false;

	private CelestialBody[] _deferBodies;
	private Node3D _deferStar, _deferShip;
	private FlightHud _deferHud;
	private NpcManager _deferNpcManager;

	public void Setup2(CelestialBody[] bodies, Node3D star, Node3D ship, FlightHud hud)
	{
		_deferBodies = bodies;
		_deferStar = star;
		_deferShip = ship;
		_deferHud = hud;
	}

	public void SetNpcManager(NpcManager npcManager)
	{
		_deferNpcManager = npcManager;
		if (_panel != null) _panel.NpcManager = npcManager;
	}

	public override void _Ready()
	{
		_panel = new MapPanel();
		_panel.Name = "MapPanel";
		_panel.Visible = false;
		AddChild(_panel);

		if (_deferBodies != null)
			_panel.Setup(_deferBodies, _deferStar, _deferShip, _deferHud);
		if (_deferNpcManager != null)
			_panel.NpcManager = _deferNpcManager;
	}

	public void Toggle()
	{
		_panel.Visible = !_panel.Visible;
		_panel.MouseFilter = _panel.Visible
			? Control.MouseFilterEnum.Stop
			: Control.MouseFilterEnum.Ignore;
		if (_panel.Visible)
			_panel.ResetView();
	}

	public override void _Process(double delta)
	{
		if (_panel.Visible)
			_panel.QueueRedraw();
	}
}

/// <summary>
/// The drawing surface + click/zoom handler for the system map.
/// </summary>
public partial class MapPanel : Control
{
	private CelestialBody[] _bodies;
	private Node3D _star;
	private Node3D _ship;
	private FlightHud _hud;
	private int _selected = -1;
	private float _zoom = 1f;
	private Vector2 _panOffset = Vector2.Zero;
	private bool _dragging;
	private const float ZoomMin = 0.1f;
	private const float ZoomMax = 20f;

	public NpcManager NpcManager { get; set; }

	public MapPanel()
	{
		SetAnchorsPreset(LayoutPreset.FullRect);
		MouseFilter = MouseFilterEnum.Ignore;
	}

	public void Setup(CelestialBody[] bodies, Node3D star, Node3D ship, FlightHud hud)
	{
		_bodies = bodies;
		_star = star;
		_ship = ship;
		_hud = hud;
	}

	public void ResetView()
	{
		_panOffset = Vector2.Zero;
		_zoom = 1f;
		_dragging = false;
	}

	public override void _GuiInput(InputEvent @event)
	{
		if (_bodies == null || _star == null) return;

		// Mouse drag to pan.
		if (@event is InputEventMouseButton mb2)
		{
			if (mb2.ButtonIndex == MouseButton.Right)
			{
				_dragging = mb2.Pressed;
				AcceptEvent();
				return;
			}
		}
		if (@event is InputEventMouseMotion mm && _dragging)
		{
			_panOffset += mm.Relative;
			AcceptEvent();
			return;
		}

		if (@event is InputEventMouseButton mb && mb.Pressed)
		{
			// Zoom — zoom toward mouse position.
			if (mb.ButtonIndex == MouseButton.WheelUp)
			{
				_zoom = Mathf.Clamp(_zoom * 1.3f, ZoomMin, ZoomMax);
				AcceptEvent();
				return;
			}
			if (mb.ButtonIndex == MouseButton.WheelDown)
			{
				_zoom = Mathf.Clamp(_zoom / 1.3f, ZoomMin, ZoomMax);
				AcceptEvent();
				return;
			}

			// Click — check stations first (smaller targets), then planets.
			if (mb.ButtonIndex == MouseButton.Left)
			{
				Vector2 click = mb.Position;
				MapProjection proj = GetProjection();

				// Check stations.
				if (NpcManager?.Stations != null)
				{
					for (int i = 0; i < NpcManager.Stations.Length; i++)
					{
						var st = NpcManager.Stations[i];
						if (!IsInstanceValid(st.Body)) continue;
						Vector2 sPos = proj.Project(st.Body.GlobalPosition);
						if (click.DistanceTo(sPos) < 12f)
						{
							_hud?.SetTarget(new CelestialBody
							{
								Name = st.Name,
								Node = st.Body,
								MapColor = Colors.Cyan,
								Radius = 50f,
							});
							_selected = -1;
							AcceptEvent();
							return;
						}
					}
				}

				// Check planets.
				for (int i = 0; i < _bodies.Length; i++)
				{
					Vector2 bodyPos = proj.Project(_bodies[i].Node.GlobalPosition);
					if (click.DistanceTo(bodyPos) < 18f)
					{
						_selected = i;
						_hud?.SetTarget(_bodies[i]);
						AcceptEvent();
						return;
					}
				}
			}
		}
	}

	public override void _Draw()
	{
		if (_bodies == null || _star == null) return;

		Vector2 screenSize = GetViewportRect().Size;
		MapProjection proj = GetProjection();

		// Background.
		DrawRect(new Rect2(Vector2.Zero, screenSize), new Color(0f, 0f, 0.02f, 0.9f));

		// Orbit circles.
		Vector2 center = proj.Center;
		foreach (var b in _bodies)
		{
			if (b.OrbitRadius > 0f)
			{
				float r = proj.ProjectRadius(b.OrbitRadius);
				DrawArc(center, r, 0f, Mathf.Tau, 64,
					new Color(b.MapColor.R, b.MapColor.G, b.MapColor.B, 0.1f), 1f);
			}
		}

		// Trade routes (faint lines between stations).
		if (NpcManager?.Routes != null && NpcManager?.Stations != null)
		{
			foreach (var route in NpcManager.Routes)
			{
				var stA = NpcManager.Stations[route.Start];
				var stB = NpcManager.Stations[route.End];
				if (!IsInstanceValid(stA.Body) || !IsInstanceValid(stB.Body)) continue;
				Vector2 a = proj.Project(stA.Body.GlobalPosition);
				Vector2 b = proj.Project(stB.Body.GlobalPosition);
				DrawLine(a, b, new Color(0.3f, 0.3f, 0.4f, 0.3f), 1f);
			}
		}

		// Star.
		DrawCircle(center, 6f, new Color(1f, 0.9f, 0.3f));

		// Planets.
		for (int i = 0; i < _bodies.Length; i++)
		{
			Vector2 pos = proj.Project(_bodies[i].Node.GlobalPosition);
			Color col = _bodies[i].MapColor;
			float dotSize = 4f;

			if (i == _selected)
			{
				DrawArc(pos, 14f, 0f, Mathf.Tau, 16, Colors.White, 2f);
				dotSize = 6f;
			}

			DrawCircle(pos, dotSize, col);
			if (_zoom > 0.5f)
			{
				DrawString(ThemeDB.FallbackFont, pos + new Vector2(10f, -3f),
					_bodies[i].Name, HorizontalAlignment.Left, -1, 11,
					new Color(0.7f, 0.7f, 0.7f, 0.7f));
			}
		}

		// Stations (small squares).
		if (NpcManager?.Stations != null)
		{
			foreach (var st in NpcManager.Stations)
			{
				if (!IsInstanceValid(st.Body)) continue;
				Vector2 pos = proj.Project(st.Body.GlobalPosition);
				DrawRect(new Rect2(pos - new Vector2(3f, 3f), new Vector2(6f, 6f)),
					new Color(0.2f, 0.8f, 0.8f, 0.7f));
				if (_zoom > 1.5f)
				{
					DrawString(ThemeDB.FallbackFont, pos + new Vector2(8f, -2f),
						st.Name, HorizontalAlignment.Left, -1, 9,
						new Color(0.5f, 0.8f, 0.8f, 0.5f));
				}
			}
		}

		// Radar bubble around player — shows NPC ships within radar range.
		if (NpcManager != null && _ship != null)
		{
			float radarRange = 100000f;
			Vector3 playerPos = _ship.GlobalPosition;
			Vector2 playerScreen = proj.Project(playerPos);

			// Draw radar circle.
			float radarScreenR = proj.ProjectRadius(radarRange * radarRange / proj.MaxOrbit) * 0.5f;
			// Approximate: just use a fixed screen radius scaled by zoom.
			radarScreenR = 80f * _zoom;
			DrawArc(playerScreen, radarScreenR, 0f, Mathf.Tau, 32,
				new Color(0.3f, 0.8f, 0.3f, 0.15f), 1f);

			int shipCount = NpcManager.ShipCountActual;
			float radarSq = radarRange * radarRange;
			for (int i = 0; i < shipCount; i++)
			{
				Vector3 wPos = NpcManager.GetShipWorldPosition(i);
				if (playerPos.DistanceSquaredTo(wPos) > radarSq) continue;
				Vector2 sPos = proj.Project(wPos);
				DrawCircle(sPos, 2f, new Color(0.3f, 0.8f, 0.3f, 0.6f));
			}
		}

		// Player.
		if (_ship != null)
		{
			Vector2 shipPos = proj.Project(_ship.GlobalPosition);
			DrawCircle(shipPos, 4f, Colors.White);
			DrawString(ThemeDB.FallbackFont, shipPos + new Vector2(8f, -3f),
				"YOU", HorizontalAlignment.Left, -1, 10, Colors.White);
		}

		// Title + zoom level.
		DrawString(ThemeDB.FallbackFont, new Vector2(20f, 30f),
			$"SYSTEM MAP — scroll to zoom ({_zoom:F1}x), click to target, M to close",
			HorizontalAlignment.Left, -1, 13, new Color(0.5f, 0.5f, 0.5f));

		// Target info.
		if (_selected >= 0 && _selected < _bodies.Length && _ship != null)
		{
			float dist = _ship.GlobalPosition.DistanceTo(_bodies[_selected].Node.GlobalPosition);
			DrawString(ThemeDB.FallbackFont, new Vector2(20f, screenSize.Y - 20f),
				$"Target: {_bodies[_selected].Name}  —  {FormatDistance(dist)}",
				HorizontalAlignment.Left, -1, 16, Colors.White);
		}
	}

	private struct MapProjection
	{
		public Vector2 Center;
		public float MaxOrbit;
		public float MaxScreenR;
		public float Zoom;
		public Vector3 StarPos;

		public readonly Vector2 Project(Vector3 globalPos)
		{
			Vector3 rel = globalPos - StarPos;
			Vector2 relXZ = new Vector2(rel.X, rel.Z);
			float dist = relXZ.Length();
			if (dist < 1f) return Center;

			float scaledR = Mathf.Sqrt(dist / MaxOrbit) * MaxScreenR * Zoom;
			Vector2 dir = relXZ / dist;
			return Center + dir * scaledR;
		}

		public readonly float ProjectRadius(float orbitRadius)
		{
			return Mathf.Sqrt(orbitRadius / MaxOrbit) * MaxScreenR * Zoom;
		}
	}

	private MapProjection GetProjection()
	{
		Vector2 screenSize = GetViewportRect().Size;
		Vector2 center = screenSize / 2f + _panOffset;
		float maxOrbit = GetMaxOrbit();
		return new MapProjection
		{
			Center = center,
			MaxOrbit = maxOrbit,
			MaxScreenR = Mathf.Min(screenSize.X, screenSize.Y) * 0.425f,
			Zoom = _zoom,
			StarPos = _star.GlobalPosition,
		};
	}

	private float GetMaxOrbit()
	{
		float max = 0f;
		foreach (var b in _bodies)
			if (b.OrbitRadius > max) max = b.OrbitRadius;
		return max;
	}

	private static string FormatDistance(float dist) => Starlight.Util.Format.Distance(dist);
}
