using Godot;
using Starlight.Ship;
using Starlight.Game.Runtime;
using Starlight.Game.Inventory;

namespace Starlight.Game.UI;

/// <summary>
/// Transparent fighter-style HUD for VS1.
/// Keeps telemetry at the edges and reserves the center for reticle and alerts.
/// </summary>
public partial class FlightHud : CanvasLayer
{
	private readonly Color _cyan = new(0.72f, 0.94f, 1f, 0.95f);
	private readonly Color _blue = new(0.30f, 0.72f, 1f, 0.95f);
	private readonly Color _green = new(0.42f, 1f, 0.62f, 0.95f);
	private readonly Color _yellow = new(1f, 0.86f, 0.30f, 0.95f);
	private readonly Color _red = new(1f, 0.42f, 0.42f, 0.95f);

	private ShipController _ship;
	private Label _fpsLabel;
	private Label _modeLabel;
	private Label _targetLabel;
	private Label _navLabel;
	private Label _guidanceLabel;
	private Label _speedLabel;
	private Label _speedValueLabel;
	private Label _throttleLabel;
	private Label _throttleValueLabel;
	private Label _weaponLabel;
	private Label _weaponValueLabel;
	private Label _hullValueLabel;
	private Label _shieldValueLabel;
	private Label _weaponEnergyValueLabel;
	private Label _centerAlertLabel;
	private FlightHudChrome _chrome;
	private CrosshairMarker _crosshair;
	private LeadPipMarker _leadPipMarker;
	private TargetMarker _targetMarker;
	private CelestialBody? _target;
	private Vector3? _leadPoint;
	private bool _debugVisible;
	private Vector2 _lastViewportSize;

	public void SetShip(ShipController ship)
	{
		_ship = ship;
		if (_chrome != null)
			_chrome.Ship = ship;
	}

	public void SetTarget(CelestialBody body) => _target = body;
	public void ClearTarget() => _target = null;
	public void SetLeadPoint(Vector3? leadPoint) => _leadPoint = leadPoint;

	public void SetDebugVisible(bool visible)
	{
		_debugVisible = visible;
		if (_fpsLabel != null)
			_fpsLabel.Visible = visible;
	}

	public override void _Ready()
	{
		_chrome = new FlightHudChrome();
		_chrome.Name = "HudChrome";
		_chrome.SetAnchorsPreset(Control.LayoutPreset.FullRect);
		_chrome.MouseFilter = Control.MouseFilterEnum.Ignore;
		_chrome.Ship = _ship;
		AddChild(_chrome);

		AddChild(BuildTopStrip());
		AddChild(BuildLeftFlightCluster());
		AddChild(BuildRightSystemsCluster());
		AddChild(BuildUpperLeftGuidanceCluster());
		AddChild(BuildCenterAlerts());

		_crosshair = new CrosshairMarker();
		_crosshair.SetAnchorsPreset(Control.LayoutPreset.FullRect);
		_crosshair.MouseFilter = Control.MouseFilterEnum.Ignore;
		AddChild(_crosshair);

		_leadPipMarker = new LeadPipMarker();
		_leadPipMarker.SetAnchorsPreset(Control.LayoutPreset.FullRect);
		_leadPipMarker.MouseFilter = Control.MouseFilterEnum.Ignore;
		AddChild(_leadPipMarker);

		_targetMarker = new TargetMarker();
		_targetMarker.SetAnchorsPreset(Control.LayoutPreset.FullRect);
		_targetMarker.MouseFilter = Control.MouseFilterEnum.Ignore;
		AddChild(_targetMarker);

		ApplyResponsiveLayout(true);
		SetDebugVisible(false);
	}

	public override void _Process(double delta)
	{
		ApplyResponsiveLayout();

		if (_debugVisible)
			_fpsLabel.Text = $"FPS {Engine.GetFramesPerSecond()}";

		if (_ship == null)
			return;

		float speed = _ship.Speed;
		float throttlePercent = _ship.ThrottlePercent * 100f;
		float hullFraction = PlayerState.MaxHull > 0.01f ? PlayerState.Hull / PlayerState.MaxHull : 0f;
		float shieldFraction = PlayerState.MaxShields > 0.01f ? PlayerState.Shields / PlayerState.MaxShields : 0f;
		float weaponEnergyFraction = PlayerState.MaxWeaponEnergy > 0.01f
			? PlayerState.WeaponEnergy / PlayerState.MaxWeaponEnergy
			: 0f;

		_speedValueLabel.Text = $"{speed:F0} u/s";
		_throttleValueLabel.Text = $"{throttlePercent:F0}%";

		string shipWeapon = ItemCatalog.DisplayName(PlayerState.GetShipEquipped(EquipmentSlotId.ShipWeaponHardpoint1)?.ItemId ?? "pulse_laser_mk1");
		_weaponValueLabel.Text = shipWeapon;

		_hullValueLabel.Text = $"HULL {PlayerState.Hull:F0}/{PlayerState.MaxHull:F0}";
		_shieldValueLabel.Text = $"SHD {PlayerState.Shields:F0}/{PlayerState.MaxShields:F0}";
		_weaponEnergyValueLabel.Text = $"WPN {weaponEnergyFraction * 100f:F0}%";

		_modeLabel.Text = BuildModeText();
		_modeLabel.AddThemeColorOverride("font_color", GetModeColor());

		_navLabel.Text = BuildNavText();
		_guidanceLabel.Text = BuildGuidanceText();
		_targetLabel.Text = BuildTargetText();
		_centerAlertLabel.Text = BuildCenterAlertText();
		_centerAlertLabel.Visible = !string.IsNullOrWhiteSpace(_centerAlertLabel.Text);
		_centerAlertLabel.AddThemeColorOverride("font_color", GetCenterAlertColor());

		_chrome.ThrottlePercent = throttlePercent / 100f;
		_chrome.HullFraction = hullFraction;
		_chrome.ShieldFraction = shieldFraction;
		_chrome.WeaponEnergyFraction = weaponEnergyFraction;
		_chrome.HudScale = GetHudScale();
		_chrome.QueueRedraw();

		_targetMarker.SetTarget(_target, _ship);
		_targetMarker.QueueRedraw();
		_leadPipMarker.SetLeadPoint(_leadPoint);
		_leadPipMarker.QueueRedraw();
	}

	private Control BuildTopStrip()
	{
		var root = new Control();
		root.SetAnchorsPreset(Control.LayoutPreset.FullRect);
		root.MouseFilter = Control.MouseFilterEnum.Ignore;

		_fpsLabel = CreateLabel(new Vector2(16f, 10f), 12, new Color(0.72f, 0.78f, 0.84f, 0.85f));
		root.AddChild(_fpsLabel);

		_modeLabel = CreateLabel(new Vector2(0f, 22f), 28, _cyan);
		_modeLabel.AnchorLeft = 0.5f;
		_modeLabel.AnchorRight = 0.5f;
		_modeLabel.OffsetLeft = -210f;
		_modeLabel.OffsetRight = 210f;
		_modeLabel.HorizontalAlignment = HorizontalAlignment.Center;
		root.AddChild(_modeLabel);

		_targetLabel = CreateLabel(new Vector2(0f, 20f), 18, _yellow);
		_targetLabel.AnchorLeft = 1f;
		_targetLabel.AnchorRight = 1f;
		_targetLabel.OffsetLeft = -330f;
		_targetLabel.OffsetRight = -20f;
		_targetLabel.HorizontalAlignment = HorizontalAlignment.Right;
		root.AddChild(_targetLabel);

		return root;
	}

	private Control BuildUpperLeftGuidanceCluster()
	{
		var root = new Control();
		root.SetAnchorsPreset(Control.LayoutPreset.FullRect);
		root.MouseFilter = Control.MouseFilterEnum.Ignore;

		_navLabel = CreateLabel(new Vector2(20f, 76f), 16, _cyan);
		_navLabel.CustomMinimumSize = new Vector2(470f, 52f);
		root.AddChild(_navLabel);

		_guidanceLabel = CreateLabel(new Vector2(20f, 128f), 14, new Color(0.78f, 0.92f, 1f, 0.88f));
		_guidanceLabel.CustomMinimumSize = new Vector2(470f, 62f);
		_guidanceLabel.AutowrapMode = TextServer.AutowrapMode.WordSmart;
		root.AddChild(_guidanceLabel);

		return root;
	}

	private Control BuildLeftFlightCluster()
	{
		var root = new Control();
		root.SetAnchorsPreset(Control.LayoutPreset.FullRect);
		root.MouseFilter = Control.MouseFilterEnum.Ignore;

		_speedLabel = CreateLabel(new Vector2(34f, 0f), 14, _cyan);
		_speedLabel.AnchorTop = 1f;
		_speedLabel.AnchorBottom = 1f;
		_speedLabel.OffsetTop = -190f;
		_speedLabel.Text = "SPEED";
		root.AddChild(_speedLabel);

		_speedValueLabel = CreateLabel(new Vector2(34f, 0f), 36, _cyan);
		_speedValueLabel.AnchorTop = 1f;
		_speedValueLabel.AnchorBottom = 1f;
		_speedValueLabel.OffsetTop = -164f;
		root.AddChild(_speedValueLabel);

		_throttleLabel = CreateLabel(new Vector2(34f, 0f), 14, _yellow);
		_throttleLabel.AnchorTop = 1f;
		_throttleLabel.AnchorBottom = 1f;
		_throttleLabel.OffsetTop = -116f;
		_throttleLabel.Text = "THROTTLE";
		root.AddChild(_throttleLabel);

		_throttleValueLabel = CreateLabel(new Vector2(34f, 0f), 30, _yellow);
		_throttleValueLabel.AnchorTop = 1f;
		_throttleValueLabel.AnchorBottom = 1f;
		_throttleValueLabel.OffsetTop = -92f;
		root.AddChild(_throttleValueLabel);

		return root;
	}

	private Control BuildRightSystemsCluster()
	{
		var root = new Control();
		root.SetAnchorsPreset(Control.LayoutPreset.FullRect);
		root.MouseFilter = Control.MouseFilterEnum.Ignore;

		_weaponLabel = CreateLabel(new Vector2(0f, 0f), 14, _yellow);
		_weaponLabel.AnchorLeft = 1f;
		_weaponLabel.AnchorRight = 1f;
		_weaponLabel.AnchorTop = 1f;
		_weaponLabel.AnchorBottom = 1f;
		_weaponLabel.OffsetLeft = -290f;
		_weaponLabel.OffsetRight = -20f;
		_weaponLabel.OffsetTop = -196f;
		_weaponLabel.HorizontalAlignment = HorizontalAlignment.Right;
		_weaponLabel.Text = "PRIMARY";
		root.AddChild(_weaponLabel);

		_weaponValueLabel = CreateLabel(new Vector2(0f, 0f), 21, new Color(1f, 0.93f, 0.72f, 0.95f));
		_weaponValueLabel.AnchorLeft = 1f;
		_weaponValueLabel.AnchorRight = 1f;
		_weaponValueLabel.AnchorTop = 1f;
		_weaponValueLabel.AnchorBottom = 1f;
		_weaponValueLabel.OffsetLeft = -350f;
		_weaponValueLabel.OffsetRight = -20f;
		_weaponValueLabel.OffsetTop = -170f;
		_weaponValueLabel.HorizontalAlignment = HorizontalAlignment.Right;
		root.AddChild(_weaponValueLabel);

		_weaponEnergyValueLabel = CreateLabel(new Vector2(0f, 0f), 15, _yellow);
		_weaponEnergyValueLabel.AnchorLeft = 1f;
		_weaponEnergyValueLabel.AnchorRight = 1f;
		_weaponEnergyValueLabel.AnchorTop = 1f;
		_weaponEnergyValueLabel.AnchorBottom = 1f;
		_weaponEnergyValueLabel.OffsetLeft = -240f;
		_weaponEnergyValueLabel.OffsetRight = -20f;
		_weaponEnergyValueLabel.OffsetTop = -114f;
		_weaponEnergyValueLabel.HorizontalAlignment = HorizontalAlignment.Right;
		root.AddChild(_weaponEnergyValueLabel);

		_shieldValueLabel = CreateLabel(new Vector2(0f, 0f), 15, _blue);
		_shieldValueLabel.AnchorLeft = 1f;
		_shieldValueLabel.AnchorRight = 1f;
		_shieldValueLabel.AnchorTop = 1f;
		_shieldValueLabel.AnchorBottom = 1f;
		_shieldValueLabel.OffsetLeft = -240f;
		_shieldValueLabel.OffsetRight = -20f;
		_shieldValueLabel.OffsetTop = -88f;
		_shieldValueLabel.HorizontalAlignment = HorizontalAlignment.Right;
		root.AddChild(_shieldValueLabel);

		_hullValueLabel = CreateLabel(new Vector2(0f, 0f), 15, _green);
		_hullValueLabel.AnchorLeft = 1f;
		_hullValueLabel.AnchorRight = 1f;
		_hullValueLabel.AnchorTop = 1f;
		_hullValueLabel.AnchorBottom = 1f;
		_hullValueLabel.OffsetLeft = -240f;
		_hullValueLabel.OffsetRight = -20f;
		_hullValueLabel.OffsetTop = -62f;
		_hullValueLabel.HorizontalAlignment = HorizontalAlignment.Right;
		root.AddChild(_hullValueLabel);

		return root;
	}

	private Control BuildCenterAlerts()
	{
		var root = new Control();
		root.SetAnchorsPreset(Control.LayoutPreset.FullRect);
		root.MouseFilter = Control.MouseFilterEnum.Ignore;

		_centerAlertLabel = CreateLabel(new Vector2(0f, 0f), 30, _yellow);
		_centerAlertLabel.AnchorLeft = 0.5f;
		_centerAlertLabel.AnchorRight = 0.5f;
		_centerAlertLabel.AnchorTop = 0.5f;
		_centerAlertLabel.AnchorBottom = 0.5f;
		_centerAlertLabel.OffsetLeft = -280f;
		_centerAlertLabel.OffsetRight = 280f;
		_centerAlertLabel.OffsetTop = 92f;
		_centerAlertLabel.HorizontalAlignment = HorizontalAlignment.Center;
		root.AddChild(_centerAlertLabel);

		return root;
	}

	private Label CreateLabel(Vector2 pos, int size, Color color)
	{
		var label = new Label();
		label.Position = pos;
		label.AddThemeFontSizeOverride("font_size", size);
		label.AddThemeColorOverride("font_color", color);
		label.AddThemeColorOverride("font_outline_color", new Color(0.02f, 0.05f, 0.08f, 0.86f));
		label.AddThemeConstantOverride("outline_size", 6);
		return label;
	}

	private void ApplyResponsiveLayout(bool force = false)
	{
		Vector2 viewport = GetViewport().GetVisibleRect().Size;
		if (!force && viewport.IsEqualApprox(_lastViewportSize))
			return;

		_lastViewportSize = viewport;
		float scale = GetHudScale();
		float w = viewport.X;
		float h = viewport.Y;
		float margin = 24f * scale;
		float topMargin = 18f * scale;

		SetLabelStyle(_fpsLabel, 12f);
		_fpsLabel.Position = new Vector2(margin, 10f * scale);

		SetLabelStyle(_modeLabel, 28f);
		_modeLabel.OffsetLeft = -260f * scale;
		_modeLabel.OffsetRight = 260f * scale;
		_modeLabel.OffsetTop = topMargin;

		SetLabelStyle(_targetLabel, 18f);
		_targetLabel.OffsetLeft = -300f * scale;
		_targetLabel.OffsetRight = -margin;
		_targetLabel.OffsetTop = 20f * scale;

		SetLabelStyle(_navLabel, 16f);
		_navLabel.Position = new Vector2(margin, 78f * scale);
		_navLabel.CustomMinimumSize = new Vector2(w * 0.22f, 58f * scale);

		SetLabelStyle(_guidanceLabel, 14f);
		_guidanceLabel.Position = new Vector2(margin, 132f * scale);
		_guidanceLabel.CustomMinimumSize = new Vector2(w * 0.22f, 74f * scale);

		float leftBaseY = h - (220f * scale);
		SetLabelStyle(_speedLabel, 15f);
		_speedLabel.Position = new Vector2(margin + 28f * scale, leftBaseY);

		SetLabelStyle(_speedValueLabel, 52f);
		_speedValueLabel.Position = new Vector2(margin + 28f * scale, leftBaseY + 18f * scale);

		SetLabelStyle(_throttleLabel, 15f);
		_throttleLabel.Position = new Vector2(margin + 28f * scale, h - (118f * scale));

		SetLabelStyle(_throttleValueLabel, 40f);
		_throttleValueLabel.Position = new Vector2(margin + 28f * scale, h - (92f * scale));

		SetLabelStyle(_weaponLabel, 15f);
		_weaponLabel.OffsetLeft = -(300f * scale);
		_weaponLabel.OffsetRight = -(38f * scale);
		_weaponLabel.OffsetTop = -(226f * scale);

		SetLabelStyle(_weaponValueLabel, 30f);
		_weaponValueLabel.OffsetLeft = -(360f * scale);
		_weaponValueLabel.OffsetRight = -(38f * scale);
		_weaponValueLabel.OffsetTop = -(196f * scale);

		SetLabelStyle(_weaponEnergyValueLabel, 20f);
		_weaponEnergyValueLabel.OffsetLeft = -(260f * scale);
		_weaponEnergyValueLabel.OffsetRight = -(38f * scale);
		_weaponEnergyValueLabel.OffsetTop = -(132f * scale);

		SetLabelStyle(_shieldValueLabel, 20f);
		_shieldValueLabel.OffsetLeft = -(260f * scale);
		_shieldValueLabel.OffsetRight = -(38f * scale);
		_shieldValueLabel.OffsetTop = -(102f * scale);

		SetLabelStyle(_hullValueLabel, 20f);
		_hullValueLabel.OffsetLeft = -(260f * scale);
		_hullValueLabel.OffsetRight = -(38f * scale);
		_hullValueLabel.OffsetTop = -(72f * scale);

		SetLabelStyle(_centerAlertLabel, 30f);
		_centerAlertLabel.OffsetLeft = -340f * scale;
		_centerAlertLabel.OffsetRight = 340f * scale;
		_centerAlertLabel.OffsetTop = 104f * scale;
	}

	private float GetHudScale()
	{
		Vector2 viewport = GetViewport().GetVisibleRect().Size;
		return Mathf.Clamp(viewport.Y / 1440f, 0.82f, 1.35f);
	}

	private static void SetLabelStyle(Label label, float fontSize)
	{
		if (label == null)
			return;

		label.AddThemeFontSizeOverride("font_size", Mathf.RoundToInt(fontSize));
		label.AddThemeConstantOverride("outline_size", Mathf.RoundToInt(Mathf.Clamp(fontSize * 0.22f, 4f, 8f)));
	}

	private string BuildModeText()
	{
		return _ship.Mode switch
		{
			ShipController.FlightMode.Normal => _ship.IsEngineKill ? "ENGINE KILL" : "FLIGHT",
			ShipController.FlightMode.SupercruiseSpool => $"SUPERCRUISE SPOOL {_ship.SpoolPercent * 100f:F0}%",
			ShipController.FlightMode.Supercruise => "SUPERCRUISE",
			ShipController.FlightMode.Autopilot => "AUTOPILOT",
			_ => "FLIGHT",
		};
	}

	private Color GetModeColor()
	{
		return _ship.Mode switch
		{
			ShipController.FlightMode.SupercruiseSpool => _yellow,
			ShipController.FlightMode.Supercruise => _blue,
			ShipController.FlightMode.Autopilot => _cyan,
			_ => _ship.IsEngineKill ? _red : _cyan,
		};
	}

	private string BuildNavText()
	{
		string soiName = _ship.InsideReferenceSoi ? _ship.DominantBodyName : "None";
		string supercruiseState = _ship.SupercruiseDriveOffline
			? $"OFFLINE {_ship.SupercruiseOfflineTimer:F0}s"
			: (_ship.InsideSupercruiseLockout ? "LOCKED" : "CLEAR");
		string scramblerState = _ship.PhaseScramblerActive
			? $"ACTIVE {_ship.PhaseScramblerActiveTimer:F0}s"
			: (_ship.PhaseScramblerReady ? "READY" : $"CD {_ship.PhaseScramblerCooldownTimer:F0}s");
		string autopilotState;
		if (_ship.StationAssistActive)
		{
			autopilotState = $"Station keeping: {_ship.StationAssistName}";
		}
		else if (_ship.InsideReferenceSoi)
		{
			autopilotState = _ship.PassiveDriftActive
				? $"Local frame / drifting: {soiName}"
				: $"Local frame: {soiName}";
		}
		else
		{
			autopilotState = "Free flight";
		}
		return $"NAV  {autopilotState}\nSOI  {soiName}   |   SUPERCRUISE {supercruiseState}   |   SCRAMBLER {scramblerState}";
	}

	private string BuildGuidanceText()
	{
		if (_ship.InterdictionLockDetected)
			return $"Phase interdiction lock detected from {_ship.InterdictionSourceName}. Lock {_ship.InterdictionLockStrength * 100f:F0}%  |  Collapse envelope {_ship.InterdictionCollapseDistance:F0} m  |  Press R to scramble or J to drop early.";
		if (_ship.SupercruiseDriveOffline)
			return $"Phase shell overloaded. Supercruise recalibrating for {_ship.SupercruiseOfflineTimer:F0}s before re-entry is possible.";
		if (_ship.Mode == ShipController.FlightMode.SupercruiseSpool)
			return "Supercruise is spooling. Hold your line and the drive will engage once the charge completes.";
		if (_ship.IsEngineKill)
			return "Engine kill active. Use throttle input or toggle engine kill to resume powered flight.";
		return "Target with Tab, use comms with C, use J to enter or leave supercruise, and press R to fire the phase scrambler if you get painted.";
	}

	private string BuildTargetText()
	{
		if (_target == null || _ship == null)
			return "NO TARGET";

		CelestialBody target = _target.Value;
		if (!IsInstanceValid(target.Node))
			return "NO TARGET";

		float dist = _ship.GlobalPosition.DistanceTo(target.Node.GlobalPosition);
		return $"{target.Name}  {Starlight.Util.Format.Distance(dist)}";
	}

	private string BuildCenterAlertText()
	{
		if (_ship.IsEngineKill)
			return "ENGINE KILL";
		if (_ship.SupercruiseDriveOffline)
			return $"PHASE DRIVE OFFLINE  {_ship.SupercruiseOfflineTimer:F0}s";
		if (_ship.InterdictionLockDetected)
			return $"PHASE INTERDICTION LOCK  {_ship.InterdictionLockStrength * 100f:F0}%";
		if (_ship.Mode == ShipController.FlightMode.SupercruiseSpool)
			return $"SUPERCRUISE SPOOLING  {_ship.SpoolPercent * 100f:F0}%";
		if (_ship.Mode == ShipController.FlightMode.Autopilot)
			return "AUTOPILOT ENGAGED";
		return "";
	}

	private Color GetCenterAlertColor()
	{
		if (_ship.IsEngineKill)
			return _red;
		if (_ship.SupercruiseDriveOffline)
			return _red;
		if (_ship.InterdictionLockDetected)
			return _yellow;
		if (_ship.Mode == ShipController.FlightMode.SupercruiseSpool)
			return _yellow;
		if (_ship.Mode == ShipController.FlightMode.Autopilot)
			return _cyan;
		return _yellow;
	}
}

public partial class FlightHudChrome : Control
{
	private readonly Color _line = new(0.72f, 0.94f, 1f, 0.78f);
	private readonly Color _lineSoft = new(0.72f, 0.94f, 1f, 0.22f);
	private readonly Color _shield = new(0.30f, 0.72f, 1f, 0.92f);
	private readonly Color _hull = new(0.42f, 1f, 0.62f, 0.92f);
	private readonly Color _weapon = new(1f, 0.86f, 0.30f, 0.92f);

	public ShipController Ship { get; set; }
	public float ThrottlePercent { get; set; }
	public float HullFraction { get; set; }
	public float ShieldFraction { get; set; }
	public float WeaponEnergyFraction { get; set; } = 1f;
	public float HudScale { get; set; } = 1f;

	public override void _Process(double delta) => QueueRedraw();

	public override void _Draw()
	{
		Vector2 size = GetViewportRect().Size;
		DrawThrottleGauge(size);
		DrawSystemsBars(size);
		DrawRadarReserve(size);
	}

	private void DrawThrottleGauge(Vector2 size)
	{
		float margin = 52f * HudScale;
		Vector2 origin = new(margin, size.Y - (60f * HudScale));
		Vector2 end = new(margin, size.Y - (224f * HudScale));
		DrawLine(origin, end, _lineSoft, 2.4f * HudScale);

		for (int i = 0; i <= 10; i++)
		{
			float t = i / 10f;
			float y = Mathf.Lerp(origin.Y, end.Y, t);
			float tick = i == 5 ? 22f * HudScale : 14f * HudScale;
			DrawLine(new Vector2(origin.X, y), new Vector2(origin.X + tick, y), _lineSoft, 1.6f * HudScale);
		}

		float throttleY = Mathf.Lerp(origin.Y, end.Y, ThrottlePercent);
		DrawLine(new Vector2(origin.X - 3f * HudScale, throttleY), new Vector2(origin.X + 74f * HudScale, throttleY), _weapon, 3.2f * HudScale);
	}

	private void DrawSystemsBars(Vector2 size)
	{
		float right = size.X - (42f * HudScale);
		float width = 220f * HudScale;
		float height = 14f * HudScale;
		DrawBar(new Rect2(right - width, size.Y - (112f * HudScale), width, height), ShieldFraction, _shield);
		DrawBar(new Rect2(right - width, size.Y - (78f * HudScale), width, height), HullFraction, _hull);
		DrawBar(new Rect2(right - width, size.Y - (146f * HudScale), width, height), WeaponEnergyFraction, _weapon);
	}

	private void DrawBar(Rect2 rect, float fill, Color color)
	{
		DrawRect(rect, new Color(0f, 0f, 0f, 0f), false, 1.4f, true);
		DrawRect(rect, _lineSoft, false, 1.4f, true);
		Rect2 fillRect = new(rect.Position, new Vector2(rect.Size.X * Mathf.Clamp(fill, 0f, 1f), rect.Size.Y));
		DrawRect(fillRect, color);
	}

	private void DrawRadarReserve(Vector2 size)
	{
		Vector2 center = new(size.X * 0.5f, size.Y - (74f * HudScale));
		float radius = 42f * HudScale;
		DrawArc(center, radius, 0f, Mathf.Tau, 48, _lineSoft, 1.4f);
		DrawArc(center, radius * 0.66f, 0f, Mathf.Tau, 40, _lineSoft, 1.0f);
		DrawLine(center + new Vector2(-radius, 0f), center + new Vector2(radius, 0f), _lineSoft, 1f);
		DrawLine(center + new Vector2(0f, -radius), center + new Vector2(0f, radius), _lineSoft, 1f);
	}
}

/// <summary>
/// Draws the navigation target marker — on-screen bracket or off-screen arrow.
/// </summary>
public partial class TargetMarker : Control
{
	private CelestialBody? _target;
	private ShipController _ship;

	public void SetTarget(CelestialBody? target, ShipController ship)
	{
		_target = target;
		_ship = ship;
	}

	public override void _Draw()
	{
		if (_target == null || _ship == null) return;

		var target = _target.Value;
		if (!IsInstanceValid(target.Node)) return;

		var camera = GetViewport().GetCamera3D();
		if (camera == null) return;

		Vector3 targetPos = target.Node.GlobalPosition;
		float dist = _ship.GlobalPosition.DistanceTo(targetPos);
		string distStr = FormatDistance(dist);
		string label = $"{target.Name}  {distStr}";
		Color color = target.MapColor;

		Vector2 screenSize = GetViewportRect().Size;
		bool behind = camera.IsPositionBehind(targetPos);
		Vector2 screenPos = camera.UnprojectPosition(targetPos);

		if (behind)
			screenPos = screenSize / 2f + (screenSize / 2f - screenPos);

		float margin = 54f;
		bool onScreen = !behind
			&& screenPos.X > margin && screenPos.X < screenSize.X - margin
			&& screenPos.Y > margin && screenPos.Y < screenSize.Y - margin;

		if (onScreen)
		{
			float s = 18f;
			Vector2 p = screenPos;
			DrawLine(p + new Vector2(-s, 0), p + new Vector2(-4f, -s), color, 2.2f);
			DrawLine(p + new Vector2(-4f, -s), p + new Vector2(s, 0), color, 2.2f);
			DrawLine(p + new Vector2(s, 0), p + new Vector2(4f, s), color, 2.2f);
			DrawLine(p + new Vector2(4f, s), p + new Vector2(-s, 0), color, 2.2f);
			DrawString(ThemeDB.FallbackFont, p + new Vector2(-52f, s + 22f),
				label, HorizontalAlignment.Left, -1, 14, color);
		}
		else
		{
			Vector2 center = screenSize / 2f;
			Vector2 dir = (screenPos - center).Normalized();
			Vector2 edgePos = center + dir * Mathf.Min(center.X - margin, center.Y - margin);
			edgePos = new Vector2(
				Mathf.Clamp(edgePos.X, margin, screenSize.X - margin),
				Mathf.Clamp(edgePos.Y, margin, screenSize.Y - margin));

			float arrowSize = 12f;
			Vector2 perp = new Vector2(-dir.Y, dir.X);
			Vector2 tip = edgePos + dir * arrowSize;
			DrawPolygon(new Vector2[]
			{
				tip,
				edgePos + perp * arrowSize * 0.55f,
				edgePos - perp * arrowSize * 0.55f,
			}, new Color[] { color, color, color });

			DrawString(ThemeDB.FallbackFont, edgePos + new Vector2(18f, -6f),
				label, HorizontalAlignment.Left, -1, 13, color);
		}
	}

	private static string FormatDistance(float dist) => Starlight.Util.Format.Distance(dist);
}

/// <summary>
/// Crosshair that follows the mouse cursor position.
/// </summary>
public partial class CrosshairMarker : Control
{
	public override void _Process(double delta) => QueueRedraw();

	public override void _Draw()
	{
		Vector2 mouse = GetViewport().GetMousePosition();
		var col = new Color(0.82f, 0.96f, 1f, 0.8f);
		float s = 14f;
		float gap = 6f;

		DrawArc(mouse, 28f, Mathf.DegToRad(25f), Mathf.DegToRad(155f), 18, col, 1.5f);
		DrawArc(mouse, 28f, Mathf.DegToRad(205f), Mathf.DegToRad(335f), 18, col, 1.5f);
		DrawLine(mouse + new Vector2(-s, 0), mouse + new Vector2(-gap, 0), col, 1.8f);
		DrawLine(mouse + new Vector2(gap, 0), mouse + new Vector2(s, 0), col, 1.8f);
		DrawLine(mouse + new Vector2(0, -s), mouse + new Vector2(0, -gap), col, 1.8f);
		DrawLine(mouse + new Vector2(0, gap), mouse + new Vector2(0, s), col, 1.8f);
	}
}

/// <summary>
/// Predictive aim pip projected from a 3D intercept point.
/// </summary>
public partial class LeadPipMarker : Control
{
	private Vector3? _leadPoint;

	public void SetLeadPoint(Vector3? leadPoint)
	{
		_leadPoint = leadPoint;
	}

	public override void _Process(double delta) => QueueRedraw();

	public override void _Draw()
	{
		if (_leadPoint == null)
			return;

		Camera3D camera = GetViewport().GetCamera3D();
		if (camera == null || camera.IsPositionBehind(_leadPoint.Value))
			return;

		Vector2 pos = camera.UnprojectPosition(_leadPoint.Value);
		Vector2 size = GetViewportRect().Size;
		if (pos.X < 0f || pos.Y < 0f || pos.X > size.X || pos.Y > size.Y)
			return;

		Color color = new(1f, 0.92f, 0.46f, 0.9f);
		float radius = 12f;
		DrawArc(pos, radius, 0f, Mathf.Tau, 24, color, 1.8f);
		DrawLine(pos + new Vector2(-5f, 0f), pos + new Vector2(5f, 0f), color, 1.4f);
		DrawLine(pos + new Vector2(0f, -5f), pos + new Vector2(0f, 5f), color, 1.4f);
	}
}
