using Godot;
using System.Collections.Generic;
using System.Threading.Tasks;
using Starlight.StationInterior;
using Starlight.Game.Missions;
using Starlight.Game.Runtime;
using Starlight.Game.Inventory;
using Starlight.Game.UI;

namespace Starlight.Game.Stations;

/// <summary>
/// First reusable station-side stub for VS1.
/// Keeps service flow simple while proving scene transition and launch return.
/// </summary>
public partial class StationStubController : Node3D
{
    private const string GameplayScenePath = "res://scenes/game/space/Gameplay.tscn";
    private const string StationPlayerScenePath = "res://scenes/game/station/StationPlayer.tscn";
    private const string DialogMenuScenePath = "res://scenes/game/ui/DialogMenu.tscn";
    private enum ServiceMode
    {
        None,
        Trade,
        Repair,
        Launch,
    }

    private Label3D _stationLabel;
    private Label _stationTitle;
    private Label _playerSummary;
    private Label _equipmentSummary;
    private Label _serviceStatus;
    private PanelContainer _serviceWindow;
    private Label _serviceWindowTitle;
    private Label _serviceWindowBody;
    private Label _tradeSummary;
    private VBoxContainer _tradeActions;
    private Label _repairSummary;
    private VBoxContainer _repairActions;
    private Label _launchSummary;
    private VBoxContainer _launchActions;
    private FPSController _playerController;
    private DialogMenuController _dialogMenu;
    private bool _interactablesBound;
    private ServiceMode _activeServiceMode;
    private readonly List<QuestGiverController> _questGivers = new();

    public override void _Ready()
    {
        _stationLabel = GetNode<Label3D>("StationLabel3D");
        CanvasLayer ui = GetNode<CanvasLayer>("UI");
        _stationTitle = ui.GetNode<Label>("QuickPanel/VBox/StationTitle");
        _playerSummary = ui.GetNode<Label>("QuickPanel/VBox/PlayerSummary");
        _equipmentSummary = ui.GetNode<Label>("QuickPanel/VBox/EquipmentSummary");
        _serviceStatus = ui.GetNode<Label>("QuickPanel/VBox/ServiceStatus");
        _serviceWindow = ui.GetNode<PanelContainer>("ServiceWindow");
        _serviceWindowTitle = ui.GetNode<Label>("ServiceWindow/Margin/VBox/Title");
        _serviceWindowBody = ui.GetNode<Label>("ServiceWindow/Margin/VBox/Body");
        _tradeSummary = ui.GetNode<Label>("ServiceWindow/Margin/VBox/TradeSummary");
        _tradeActions = ui.GetNode<VBoxContainer>("ServiceWindow/Margin/VBox/TradeActions");
        _repairSummary = ui.GetNode<Label>("ServiceWindow/Margin/VBox/RepairSummary");
        _repairActions = ui.GetNode<VBoxContainer>("ServiceWindow/Margin/VBox/RepairActions");
        _launchSummary = ui.GetNode<Label>("ServiceWindow/Margin/VBox/LaunchSummary");
        _launchActions = ui.GetNode<VBoxContainer>("ServiceWindow/Margin/VBox/LaunchActions");
        RebuildTradeActions();

        string stationText = string.IsNullOrWhiteSpace(GameSession.CurrentStationId)
            ? "Unknown Station"
            : GameSession.CurrentStationId;
        StationDefinition stationDef = StationRegistry.Get(GetStationId());
        string stationDisplayName = stationDef?.DisplayName ?? stationText;

        _stationLabel.Text = stationDisplayName;
        _stationTitle.Text = stationDisplayName;
        string arrivalMessage = GameSession.ConsumeStatusMessage();
        _serviceStatus.Text = string.IsNullOrWhiteSpace(arrivalMessage)
            ? $"Docked at {stationDisplayName}. Walk to a service point and press F."
            : arrivalMessage;
        SpawnPlayer();
        SpawnDialogMenu();
        SpawnStationNpcs();
        CallDeferred(nameof(BindInteractables));
        HideServiceWindow();
        RefreshUi();
        MarkMissionDockProgress();
    }

    public void OnCommodityButtonPressed()
    {
        _activeServiceMode = ServiceMode.Trade;
        _serviceStatus.Text = "Commodity desk active.";
        _serviceWindowTitle.Text = "Commodity Trader";
        _serviceWindowBody.Text = EconomyService.BuildMarketOverview(GetStationId());
        _tradeSummary.Text = EconomyService.BuildTradeSummary(GetStationId());
        SetServiceVisibility(showTrade: true, showRepair: false, showLaunch: false);
        ShowServiceWindow();
    }

    public void OnRepairButtonPressed()
    {
        _activeServiceMode = ServiceMode.Repair;
        float hullMissing = PlayerState.MaxHull - PlayerState.Hull;
        float shieldMissing = PlayerState.MaxShields - PlayerState.Shields;
        float repairCost = Mathf.Ceil(hullMissing * 2f + shieldMissing * 1.2f);
        _serviceWindowTitle.Text = "Repair Service";
        _serviceWindowBody.Text = repairCost <= 0.01f
            ? "Your ship is already at full hull and shields."
            : "Field repairs and shield recharge are available before departure.";
        _repairSummary.Text =
            $"Current hull: {PlayerState.Hull:F0}/{PlayerState.MaxHull:F0}\n" +
            $"Current shields: {PlayerState.Shields:F0}/{PlayerState.MaxShields:F0}\n" +
            $"Estimated repair fee: {repairCost:F0} credits";
        SetServiceVisibility(showTrade: false, showRepair: true, showLaunch: false);
        ShowServiceWindow();
    }

    public void OnConfirmRepairPressed()
    {
        float hullMissing = PlayerState.MaxHull - PlayerState.Hull;
        float shieldMissing = PlayerState.MaxShields - PlayerState.Shields;
        float repairCost = Mathf.Ceil(hullMissing * 2f + shieldMissing * 1.2f);

        if (repairCost > 0.01f && !PlayerState.TrySpendCredits(repairCost))
        {
            _serviceStatus.Text = "Not enough credits for repairs.";
            _repairSummary.Text =
                $"Current hull: {PlayerState.Hull:F0}/{PlayerState.MaxHull:F0}\n" +
                $"Current shields: {PlayerState.Shields:F0}/{PlayerState.MaxShields:F0}\n" +
                $"Estimated repair fee: {repairCost:F0} credits";
            return;
        }

        PlayerState.RepairHull();
        SaveService.SaveDockedState(GetStationId());
        _serviceStatus.Text = repairCost > 0.01f
            ? $"Ship repaired and shields restored for {repairCost:F0} credits."
            : "Ship already at full hull and shields.";
        HideServiceWindow();
        RefreshUi();
    }

    public void OnLaunchButtonPressed()
    {
        _activeServiceMode = ServiceMode.Launch;
        _serviceWindowTitle.Text = "Airlock / Departure";
        _serviceWindowBody.Text = "You are back at the airlock. Do you want to leave the station and launch?";
        _launchSummary.Text =
            $"Current station: {GetStationId()}\n" +
            $"Cargo used: {PlayerState.GetUsedCargo():F0}/{PlayerState.CargoCapacity:F0}\n" +
            $"Hull: {PlayerState.Hull:F0}/{PlayerState.MaxHull:F0}  |  Shields: {PlayerState.Shields:F0}/{PlayerState.MaxShields:F0}";
        SetServiceVisibility(showTrade: false, showRepair: false, showLaunch: true);
        ShowServiceWindow();
    }

    public void OnConfirmLaunchPressed()
    {
        string stationId = string.IsNullOrWhiteSpace(GameSession.CurrentStationId)
            ? "EarthHub"
            : GameSession.CurrentStationId;

        GameSession.LaunchFrom(stationId);
        _ = ChangeScene(GameplayScenePath, "Launching...");
    }

    public void OnBuyFoodPressed() => ExecuteTrade("food", true);
    public void OnSellFoodPressed() => ExecuteTrade("food", false);
    public void OnBuyWaterPressed() => ExecuteTrade("water", true);
    public void OnSellWaterPressed() => ExecuteTrade("water", false);

    private void ExecuteTrade(string commodityId, bool isBuy)
    {
        string stationId = GetStationId();
        bool success;
        string message;

        if (isBuy)
            success = EconomyService.TryBuy(stationId, commodityId, 1f, out message);
        else
            success = EconomyService.TrySell(stationId, commodityId, 1f, out message);

        _serviceStatus.Text = message;
        if (success)
            SaveService.SaveDockedState(stationId);

        RefreshUi();
        if (_activeServiceMode == ServiceMode.Trade)
            _tradeSummary.Text = EconomyService.BuildTradeSummary(stationId);
    }

    private void RefreshUi()
    {
        string stationId = GetStationId();
        StationDefinition stationDef = StationRegistry.Get(stationId);
        FactionDefinition factionDef = stationDef != null ? StationRegistry.GetFaction(stationDef.OwnerFactionId) : null;

        _playerSummary.Text =
            $"Station: {stationDef?.DisplayName ?? stationId}\n" +
            $"Owner: {factionDef?.ShortName ?? "Unknown"}\n" +
            $"Credits: {PlayerState.Credits:F0}\n" +
            $"Hull: {PlayerState.Hull:F0}/{PlayerState.MaxHull:F0}\n" +
            $"Shields: {PlayerState.Shields:F0}/{PlayerState.MaxShields:F0}\n" +
            $"Cargo Used: {PlayerState.GetUsedCargo():F0}/{PlayerState.CargoCapacity:F0}\n" +
            $"{EconomyService.BuildCargoManifestSummary()}";

        int inventoryUsed = PlayerState.PlayerInventory.Count;
        string shipWeapon = FormatStack(PlayerState.GetShipEquipped(EquipmentSlotId.ShipWeaponHardpoint1));
        string shield = FormatStack(PlayerState.GetShipEquipped(EquipmentSlotId.ShipShield));
        string engine = FormatStack(PlayerState.GetShipEquipped(EquipmentSlotId.ShipEngine));
        string thrusters = FormatStack(PlayerState.GetShipEquipped(EquipmentSlotId.ShipThrusters));
        string sidearm = FormatStack(PlayerState.GetPlayerEquipped(EquipmentSlotId.PlayerSecondarySidearm));
        string primary = FormatStack(PlayerState.GetPlayerEquipped(EquipmentSlotId.PlayerPrimaryWeapon));

        _equipmentSummary.Text =
            $"Inventory Slots: {inventoryUsed}/{PlayerState.PlayerInventorySlotCount}\n" +
            $"Player Loadout: Primary {primary}  |  Sidearm {sidearm}\n" +
            $"Ship Loadout: Weapon {shipWeapon}  |  Shield {shield}\n" +
            $"Ship Systems: Engine {engine}  |  Thrusters {thrusters}";
    }

    private string GetStationId()
    {
        return string.IsNullOrWhiteSpace(GameSession.CurrentStationId) ? "EarthHub" : GameSession.CurrentStationId;
    }

    private void SpawnPlayer()
    {
        Marker3D spawnMarker = GetNode<Marker3D>("AirlockMarker");
        _playerController = GD.Load<PackedScene>(StationPlayerScenePath).Instantiate<FPSController>();
        _playerController.Name = "Player";
        _playerController.AutoSpawnFPSWeapon = false;
        _playerController.HideCombatHud = true;
        _playerController.UseSharedOnFootHealth = false;
        _playerController.GlobalTransform = spawnMarker.GlobalTransform;
        AddChild(_playerController);
    }

    private void SpawnDialogMenu()
    {
        _dialogMenu = GD.Load<PackedScene>(DialogMenuScenePath).Instantiate<DialogMenuController>();
        _dialogMenu.Name = "DialogMenu";
        AddChild(_dialogMenu);
        _dialogMenu.Opened += () => _playerController?.SetInputLocked(true);
        _dialogMenu.Closed += () => _playerController?.SetInputLocked(false);
    }

    private void SpawnStationNpcs()
    {
        StationDefinition def = StationRegistry.Get(GetStationId());
        if (def == null)
            return;

        foreach (NpcSpawn spawn in def.QuestGivers)
        {
            var npc = new QuestGiverController
            {
                Name = "NPC_" + spawn.NpcId,
                NpcId = spawn.NpcId,
                DisplayName = spawn.DisplayName,
                Position = spawn.LocalPosition,
            };
            AddChild(npc);
            _questGivers.Add(npc);
        }
    }

    private void MarkMissionDockProgress()
    {
        string stationId = GetStationId();
        if (MissionService.TryAdvanceDockedObjectives(stationId, out string missionStatus) &&
            !string.IsNullOrWhiteSpace(missionStatus))
        {
            _serviceStatus.Text = missionStatus;
            RefreshUi();
            SaveService.SaveDockedState(stationId);
        }
        else if (!string.IsNullOrWhiteSpace(missionStatus))
        {
            _serviceStatus.Text = missionStatus;
            RefreshUi();
        }
    }

    private void BindInteractables()
    {
        if (_interactablesBound)
            return;

        foreach (Node node in GetTree().GetNodesInGroup("vs1_station_interactable"))
        {
            if (node is not StationInteractableArea interactable)
                continue;

            interactable.Triggered += OnInteractableTriggered;
        }

        _interactablesBound = true;
    }

    private void OnInteractableTriggered(string actionId)
    {
        if (QuestGiverController.TryParseTalkAction(actionId, out string npcId))
        {
            OpenNpcDialog(npcId);
            return;
        }

        switch (actionId)
        {
            case "trade":
                OnCommodityButtonPressed();
                break;
            case "repair":
                OnRepairButtonPressed();
                break;
            case "launch":
                OnLaunchButtonPressed();
                break;
        }
    }

    private void OpenNpcDialog(string npcId)
    {
        if (_dialogMenu == null)
            return;

        QuestGiverController giver = FindQuestGiver(npcId);
        string displayName = giver != null ? giver.DisplayName : npcId;

        string stationId = GetStationId();
        MissionDefinition mission = MissionService.FindMissionForGiver(npcId, stationId);
        if (mission == null)
        {
            _dialogMenu.Show(
                displayName,
                $"{displayName}: \"Nothing for you right now. Check back later.\"",
                new[] { new DialogMenuController.Choice("Leave", null) });
            return;
        }

        MissionStatus status = MissionService.GetStatus(mission.Id);
        switch (status)
        {
            case MissionStatus.NotOffered:
                MissionService.Offer(mission.Id);
                ShowMissionOffer(displayName, mission);
                break;
            case MissionStatus.Offered:
                ShowMissionOffer(displayName, mission);
                break;
            case MissionStatus.InProgress:
                _dialogMenu.Show(
                    displayName,
                    $"{mission.InProgressDialog}\n\nObjective: {MissionService.GetActiveObjective(mission.Id)?.Text}",
                    new[] { new DialogMenuController.Choice("Leave", null) });
                break;
            case MissionStatus.ReadyToTurnIn:
                _dialogMenu.Show(
                    displayName,
                    mission.TurnInDialog + $"\n\nReward: {mission.Reward.Credits:F0} credits.",
                    new[]
                    {
                        new DialogMenuController.Choice("Accept reward", () =>
                        {
                            MissionService.TurnIn(mission.Id);
                            _serviceStatus.Text = $"Mission complete. +{mission.Reward.Credits:F0} credits.";
                            RefreshUi();
                        }),
                    });
                break;
        }
    }

    private void ShowMissionOffer(string speakerName, MissionDefinition mission)
    {
        string contextText = MissionService.BuildMissionContextText(mission);
        string body =
            mission.OfferDialog +
            $"\n\nObjective summary: {mission.Summary}\nReward: {mission.Reward.Credits:F0} credits." +
            (string.IsNullOrWhiteSpace(contextText) ? "" : $"\n{contextText}");
        _dialogMenu.Show(
            speakerName,
            body,
            new[]
            {
                new DialogMenuController.Choice("Accept", () =>
                {
                    MissionService.Accept(mission.Id);
                    _serviceStatus.Text = mission.AcceptedDialog;
                }),
                new DialogMenuController.Choice("Decline", null),
            });
    }

    private QuestGiverController FindQuestGiver(string npcId)
    {
        foreach (QuestGiverController giver in _questGivers)
        {
            if (giver != null && giver.NpcId == npcId)
                return giver;
        }
        return null;
    }

    private void RebuildTradeActions()
    {
        if (_tradeActions == null)
            return;

        foreach (Node child in _tradeActions.GetChildren())
            child.QueueFree();

        foreach (string commodityId in EconomyService.CommodityIds)
        {
            string displayName = EconomyService.GetCommodityDisplayName(commodityId);
            var row = new HBoxContainer();
            row.AddThemeConstantOverride("separation", 8);

            var buyButton = new Button
            {
                Text = $"Buy {displayName}",
                SizeFlagsHorizontal = Control.SizeFlags.ExpandFill,
            };
            string capturedCommodityId = commodityId;
            buyButton.Pressed += () => ExecuteTrade(capturedCommodityId, true);
            row.AddChild(buyButton);

            var sellButton = new Button
            {
                Text = $"Sell {displayName}",
                SizeFlagsHorizontal = Control.SizeFlags.ExpandFill,
            };
            sellButton.Pressed += () => ExecuteTrade(capturedCommodityId, false);
            row.AddChild(sellButton);

            _tradeActions.AddChild(row);
        }
    }

    public void OnCloseServiceWindowPressed()
    {
        HideServiceWindow();
    }

    private void ShowServiceWindow()
    {
        _serviceWindow.Visible = true;
        _playerController?.SetInputLocked(true);
    }

    private void HideServiceWindow()
    {
        _activeServiceMode = ServiceMode.None;
        _serviceWindow.Visible = false;
        _playerController?.SetInputLocked(false);
    }

    private void SetServiceVisibility(bool showTrade, bool showRepair, bool showLaunch)
    {
        _tradeSummary.Visible = showTrade;
        _tradeActions.Visible = showTrade;
        _repairSummary.Visible = showRepair;
        _repairActions.Visible = showRepair;
        _launchSummary.Visible = showLaunch;
        _launchActions.Visible = showLaunch;
    }

    private static string FormatStack(ItemStack stack)
    {
        if (stack == null || stack.IsEmpty)
            return "Empty";

        string name = ItemCatalog.DisplayName(stack.ItemId);
        return stack.Count > 1f ? $"{name} x{stack.Count:F0}" : name;
    }

    private Task ChangeScene(string path, string status)
    {
        if (SceneTransitionController.Instance != null)
            return SceneTransitionController.Instance.ChangeSceneWithFade(path, status);

        GetTree().ChangeSceneToFile(path);
        return Task.CompletedTask;
    }
}
