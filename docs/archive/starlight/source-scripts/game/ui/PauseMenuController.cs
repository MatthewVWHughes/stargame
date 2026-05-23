using Godot;
using System.Collections.Generic;
using Starlight.Game.Runtime;
using Starlight.Game.Inventory;
using Starlight.Game.Missions;
using Starlight.Game.Stations;

namespace Starlight.Game.UI;

/// <summary>
/// Autoloaded cross-scene pause menu. Tabs: Inventory, Missions, Settings.
/// Opens with I (inventory tab) or Esc (last-used tab). Sector Map has its own non-pausing overlay.
/// </summary>
public partial class PauseMenuController : CanvasLayer
{
	public static PauseMenuController Instance { get; private set; }

	public enum Tab
	{
		Inventory = 0,
		Missions = 1,
		Settings = 2,
	}

	private Control _root;
	private TabContainer _tabs;
	private Button _resumeButton;
	private Button _mainMenuButton;

	private GridContainer _playerInvGrid;
	private VBoxContainer _playerEquippedList;
	private VBoxContainer _shipEquippedList;
	private VBoxContainer _shipCargoList;
	private Label _shipCargoHeader;
	private VBoxContainer _missionsList;
	private Label _missionsEmptyLabel;

	private Input.MouseModeEnum _priorMouseMode;
	private bool _isOpen;

	public override void _Ready()
	{
		Instance = this;
		ProcessMode = ProcessModeEnum.Always;

		_root = GetNode<Control>("Root");
		_tabs = GetNode<TabContainer>("Root/CenterPanel/Margin/VBox/Tabs");
		_resumeButton = GetNode<Button>("Root/CenterPanel/Margin/VBox/Footer/ResumeButton");
		_mainMenuButton = GetNode<Button>("Root/CenterPanel/Margin/VBox/Footer/MainMenuButton");

		_playerEquippedList = ResolveNode<VBoxContainer>(
			"Root/CenterPanel/Margin/VBox/Tabs/Inventory/Margin/HBox/PlayerEquippedCol/List",
			"Root/CenterPanel/Margin/VBox/Tabs/Inventory/HBox/PlayerEquippedCol/List");
		_playerInvGrid = ResolveNode<GridContainer>(
			"Root/CenterPanel/Margin/VBox/Tabs/Inventory/Margin/HBox/PlayerInventoryCol/Grid",
			"Root/CenterPanel/Margin/VBox/Tabs/Inventory/HBox/PlayerInventoryCol/Grid");
		_shipEquippedList = ResolveNode<VBoxContainer>(
			"Root/CenterPanel/Margin/VBox/Tabs/Inventory/Margin/HBox/ShipEquippedCol/List",
			"Root/CenterPanel/Margin/VBox/Tabs/Inventory/HBox/ShipEquippedCol/List");
		_shipCargoList = ResolveNode<VBoxContainer>(
			"Root/CenterPanel/Margin/VBox/Tabs/Inventory/Margin/HBox/ShipCargoCol/List",
			"Root/CenterPanel/Margin/VBox/Tabs/Inventory/HBox/ShipCargoCol/List");
		_shipCargoHeader = ResolveNode<Label>(
			"Root/CenterPanel/Margin/VBox/Tabs/Inventory/Margin/HBox/ShipCargoCol/Header",
			"Root/CenterPanel/Margin/VBox/Tabs/Inventory/HBox/ShipCargoCol/Header");
		_missionsList = ResolveNode<VBoxContainer>(
			"Root/CenterPanel/Margin/VBox/Tabs/Missions/Margin/VBox/Scroll/List");
		_missionsEmptyLabel = ResolveNode<Label>(
			"Root/CenterPanel/Margin/VBox/Tabs/Missions/Margin/VBox/EmptyLabel");

		_resumeButton.Pressed += Close;
		_mainMenuButton.Pressed += OnMainMenuPressed;
		MissionService.Changed += OnMissionChanged;

		_root.Visible = false;
	}

	public override void _ExitTree()
	{
		if (Instance == this)
			Instance = null;
		MissionService.Changed -= OnMissionChanged;
	}

	public override void _UnhandledInput(InputEvent @event)
	{
		if (@event is not InputEventKey key || !key.Pressed || key.Echo)
			return;

		if (key.Keycode == Key.I)
		{
			Toggle(Tab.Inventory);
			GetViewport().SetInputAsHandled();
			return;
		}

		if (key.Keycode == Key.Escape)
		{
			Toggle(Tab.Inventory);
			GetViewport().SetInputAsHandled();
		}
	}

	public void Toggle(Tab tab)
	{
		if (_isOpen)
			Close();
		else
			Open(tab);
	}

	public void Open(Tab tab)
	{
		if (_isOpen)
		{
			_tabs.CurrentTab = (int)tab;
			Refresh();
			return;
		}

		_priorMouseMode = Input.MouseMode;
		Input.MouseMode = Input.MouseModeEnum.Visible;
		GetTree().Paused = true;
		_root.Visible = true;
		_tabs.CurrentTab = (int)tab;
		_isOpen = true;
		Refresh();
	}

	public void Close()
	{
		if (!_isOpen)
			return;

		_root.Visible = false;
		GetTree().Paused = false;
		Input.MouseMode = _priorMouseMode;
		_isOpen = false;
	}

	private void OnMainMenuPressed()
	{
		Close();
		const string mainMenu = "res://scenes/game/MainMenu.tscn";
		if (SceneTransitionController.Instance != null)
			_ = SceneTransitionController.Instance.ChangeSceneWithFade(mainMenu, "Returning to menu...");
		else
			GetTree().ChangeSceneToFile(mainMenu);
	}

	private void Refresh()
	{
		RefreshPlayerEquipped();
		RefreshPlayerInventory();
		RefreshShipEquipped();
		RefreshShipCargo();
		RefreshMissions();
	}

	private void OnMissionChanged()
	{
		if (_isOpen)
			RefreshMissions();
	}

	private void RefreshPlayerEquipped()
	{
		ClearChildren(_playerEquippedList);
		AddEquippedRow(_playerEquippedList, EquipmentSlotId.PlayerPrimaryWeapon, "Primary Weapon", PlayerState.GetPlayerEquipped(EquipmentSlotId.PlayerPrimaryWeapon));
		AddEquippedRow(_playerEquippedList, EquipmentSlotId.PlayerSecondarySidearm, "Sidearm", PlayerState.GetPlayerEquipped(EquipmentSlotId.PlayerSecondarySidearm));
	}

	private void RefreshShipEquipped()
	{
		ClearChildren(_shipEquippedList);
		AddEquippedRow(_shipEquippedList, EquipmentSlotId.ShipWeaponHardpoint1, "Weapon 1", PlayerState.GetShipEquipped(EquipmentSlotId.ShipWeaponHardpoint1));
		AddEquippedRow(_shipEquippedList, EquipmentSlotId.ShipWeaponHardpoint2, "Weapon 2", PlayerState.GetShipEquipped(EquipmentSlotId.ShipWeaponHardpoint2));
		AddEquippedRow(_shipEquippedList, EquipmentSlotId.ShipShield, "Shield", PlayerState.GetShipEquipped(EquipmentSlotId.ShipShield));
		AddEquippedRow(_shipEquippedList, EquipmentSlotId.ShipEngine, "Engine", PlayerState.GetShipEquipped(EquipmentSlotId.ShipEngine));
		AddEquippedRow(_shipEquippedList, EquipmentSlotId.ShipThrusters, "Thrusters", PlayerState.GetShipEquipped(EquipmentSlotId.ShipThrusters));
		AddEquippedRow(_shipEquippedList, EquipmentSlotId.ShipCountermeasures, "Countermeasures", PlayerState.GetShipEquipped(EquipmentSlotId.ShipCountermeasures));
	}

	private void RefreshPlayerInventory()
	{
		ClearChildren(_playerInvGrid);
		List<ItemStack> inv = PlayerState.PlayerInventory;
		for (int i = 0; i < PlayerState.PlayerInventorySlotCount; i++)
		{
			ItemStack stack = i < inv.Count ? inv[i] : null;
			_playerInvGrid.AddChild(BuildItemCard(stack, compact: true));
		}
	}

	private void RefreshShipCargo()
	{
		ClearChildren(_shipCargoList);
		float used = PlayerState.GetUsedCargo();
		float cap = PlayerState.CargoCapacity;
		_shipCargoHeader.Text = $"Cargo Hold  {used:F0} / {cap:F0}";

		if (PlayerState.Cargo.Count == 0)
		{
			var empty = new Label { Text = "(empty)" };
			empty.AddThemeColorOverride("font_color", new Color(0.6f, 0.6f, 0.65f));
			_shipCargoList.AddChild(empty);
			return;
		}

		foreach ((string commodityId, float count) in PlayerState.Cargo)
		{
			var stack = new ItemStack(commodityId, count);
			_shipCargoList.AddChild(BuildItemCard(stack, compact: false));
		}
	}

	private void RefreshMissions()
	{
		ClearChildren(_missionsList);
		bool hasMission = false;

		foreach ((MissionDefinition mission, MissionStatus status) in MissionService.ActiveMissions())
		{
			_missionsList.AddChild(BuildMissionCard(mission, status));
			hasMission = true;
		}

		if (_missionsEmptyLabel != null)
			_missionsEmptyLabel.Visible = !hasMission;
	}

	private Control BuildMissionCard(MissionDefinition mission, MissionStatus status)
	{
		var card = new PanelContainer
		{
			CustomMinimumSize = new Vector2(0, 108),
		};

		var margin = new MarginContainer();
		margin.AddThemeConstantOverride("margin_left", 10);
		margin.AddThemeConstantOverride("margin_right", 10);
		margin.AddThemeConstantOverride("margin_top", 8);
		margin.AddThemeConstantOverride("margin_bottom", 8);
		card.AddChild(margin);

		var vbox = new VBoxContainer();
		vbox.AddThemeConstantOverride("separation", 4);
		margin.AddChild(vbox);

		var title = new Label { Text = mission.Title };
		title.AddThemeFontSizeOverride("font_size", 17);
		vbox.AddChild(title);

		var statusLabel = new Label { Text = BuildMissionStatusText(mission, status) };
		statusLabel.AddThemeFontSizeOverride("font_size", 12);
		statusLabel.AddThemeColorOverride("font_color", new Color(0.70f, 0.84f, 0.96f));
		vbox.AddChild(statusLabel);

		var objectiveLabel = new Label
		{
			Text = BuildMissionObjectiveText(mission, status),
			AutowrapMode = TextServer.AutowrapMode.WordSmart,
		};
		objectiveLabel.AddThemeFontSizeOverride("font_size", 13);
		vbox.AddChild(objectiveLabel);

		var rewardLabel = new Label
		{
			Text = mission.Reward != null && mission.Reward.Credits > 0f
				? $"Reward: {mission.Reward.Credits:F0} credits"
				: "Reward: none listed",
		};
		rewardLabel.AddThemeFontSizeOverride("font_size", 12);
		rewardLabel.AddThemeColorOverride("font_color", new Color(0.86f, 0.83f, 0.62f));
		vbox.AddChild(rewardLabel);

		return card;
	}

	private static string BuildMissionStatusText(MissionDefinition mission, MissionStatus status)
	{
		string stationName = ResolveStationDisplayName(mission.GiverStationId);
		string issuerName = ResolveFactionDisplayName(mission.IssuerFactionId);
		return status switch
		{
			MissionStatus.Offered => $"Available at {stationName}   |   {issuerName}",
			MissionStatus.InProgress => $"In progress   |   {issuerName}",
			MissionStatus.ReadyToTurnIn => $"Ready to turn in at {stationName}   |   {issuerName}",
			_ => $"{stationName}   |   {issuerName}",
		};
	}

	private static string BuildMissionObjectiveText(MissionDefinition mission, MissionStatus status)
	{
		string stationName = ResolveStationDisplayName(mission.GiverStationId);
		string contextText = MissionService.BuildMissionContextText(mission);
		return status switch
		{
			MissionStatus.Offered => string.IsNullOrWhiteSpace(contextText) ? mission.Summary : $"{mission.Summary}\n{contextText}",
			MissionStatus.InProgress => MissionService.GetActiveObjective(mission.Id)?.Text ?? mission.Summary,
			MissionStatus.ReadyToTurnIn => $"Return to {stationName} and report in.",
			_ => mission.Summary,
		};
	}

	private static string ResolveStationDisplayName(string stationId)
	{
		return StationRegistry.Get(stationId)?.DisplayName ?? stationId;
	}

	private static string ResolveFactionDisplayName(string factionId)
	{
		return StationRegistry.GetFaction(factionId)?.DisplayName ?? factionId;
	}

	private void AddEquippedRow(VBoxContainer parent, EquipmentSlotId slot, string slotLabel, ItemStack stack)
	{
		var row = new PanelContainer { CustomMinimumSize = new Vector2(0, 44) };
		var margin = new MarginContainer();
		margin.AddThemeConstantOverride("margin_left", 8);
		margin.AddThemeConstantOverride("margin_right", 8);
		margin.AddThemeConstantOverride("margin_top", 4);
		margin.AddThemeConstantOverride("margin_bottom", 4);
		row.AddChild(margin);

		var vbox = new VBoxContainer();
		margin.AddChild(vbox);

		var slotName = new Label { Text = slotLabel };
		slotName.AddThemeColorOverride("font_color", new Color(0.65f, 0.7f, 0.78f));
		slotName.AddThemeFontSizeOverride("font_size", 11);
		vbox.AddChild(slotName);

		var content = new Label
		{
			Text = stack == null || stack.IsEmpty ? "— empty —" : FormatStack(stack),
		};
		content.AddThemeFontSizeOverride("font_size", 14);
		vbox.AddChild(content);

		parent.AddChild(row);
	}

	private Control BuildItemCard(ItemStack stack, bool compact)
	{
		var card = new PanelContainer
		{
			CustomMinimumSize = compact ? new Vector2(76, 76) : new Vector2(0, 38),
		};
		var margin = new MarginContainer();
		margin.AddThemeConstantOverride("margin_left", 6);
		margin.AddThemeConstantOverride("margin_right", 6);
		margin.AddThemeConstantOverride("margin_top", 4);
		margin.AddThemeConstantOverride("margin_bottom", 4);
		card.AddChild(margin);

		if (stack == null || stack.IsEmpty)
		{
			var empty = new Label { Text = "—", HorizontalAlignment = HorizontalAlignment.Center };
			empty.AddThemeColorOverride("font_color", new Color(0.45f, 0.45f, 0.5f));
			margin.AddChild(empty);
			return card;
		}

		if (compact)
		{
			var vbox = new VBoxContainer { Alignment = BoxContainer.AlignmentMode.Center };
			margin.AddChild(vbox);

			var name = new Label
			{
				Text = ItemCatalog.DisplayName(stack.ItemId),
				HorizontalAlignment = HorizontalAlignment.Center,
				AutowrapMode = TextServer.AutowrapMode.WordSmart,
			};
			name.AddThemeFontSizeOverride("font_size", 11);
			vbox.AddChild(name);

			if (stack.Count > 1f)
			{
				var count = new Label
				{
					Text = $"× {stack.Count:F0}",
					HorizontalAlignment = HorizontalAlignment.Center,
				};
				count.AddThemeFontSizeOverride("font_size", 14);
				vbox.AddChild(count);
			}
		}
		else
		{
			var hbox = new HBoxContainer();
			margin.AddChild(hbox);

			var name = new Label
			{
				Text = ItemCatalog.DisplayName(stack.ItemId),
				SizeFlagsHorizontal = Control.SizeFlags.ExpandFill,
			};
			hbox.AddChild(name);

			var count = new Label { Text = $"× {stack.Count:F0}" };
			hbox.AddChild(count);
		}

		return card;
	}

	private static string FormatStack(ItemStack stack)
	{
		string name = ItemCatalog.DisplayName(stack.ItemId);
		return stack.Count > 1f ? $"{name} × {stack.Count:F0}" : name;
	}

	private static void ClearChildren(Node node)
	{
		if (node == null)
			return;

		foreach (Node child in node.GetChildren())
			child.QueueFree();
	}

	private T ResolveNode<T>(params string[] paths) where T : Node
	{
		foreach (string path in paths)
		{
			T node = GetNodeOrNull<T>(path);
			if (node != null)
				return node;
		}

		GD.PushError($"PauseMenuController could not resolve any of the expected node paths for {typeof(T).Name}.");
		return null;
	}
}
