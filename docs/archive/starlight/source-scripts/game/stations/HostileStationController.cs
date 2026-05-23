using Godot;
using System.Collections.Generic;
using System.Threading.Tasks;
using Starlight.StationInterior;
using Starlight.Game.Combat;
using Starlight.Game.Missions;
using Starlight.Game.Runtime;

namespace Starlight.Game.Stations;

/// <summary>
/// Interior controller for hostile boarding stations in VS1.
/// Parallels StationStubController in shape (spawns the station player at AirlockMarker, owns a small HUD,
/// handles the launch-back-to-space interactable) but replaces the trade/repair services with on-foot combat:
///
/// - Spawns <see cref="StationHostile"/> cylinders at scene-defined HostileSpawn markers.
/// - Arms the player's OnFootWeapon for the duration of the boarding.
/// - Advances the active mission's "clear_hostiles" objective when the last hostile dies.
/// - Does not yet allow launch until the area is clear (framework enforces the gameplay loop).
/// </summary>
public partial class HostileStationController : Node3D
{
    private const string GameplayScenePath = "res://scenes/game/space/Gameplay.tscn";
    private const string StationPlayerScenePath = "res://scenes/game/station/StationPlayer.tscn";

    private Label3D _stationLabel;
    private Label _stationTitle;
    private Label _statusLabel;
    private Label _healthLabel;
    private FPSController _playerController;
    private OnFootWeapon _weapon;
    private readonly List<StationHostile> _hostiles = new();
    private bool _interactablesBound;
    private bool _combatComplete;

    public override void _Ready()
    {
        _stationLabel = GetNode<Label3D>("StationLabel3D");
        CanvasLayer ui = GetNode<CanvasLayer>("UI");
        _stationTitle = ui.GetNode<Label>("Panel/VBox/Title");
        _statusLabel = ui.GetNode<Label>("Panel/VBox/Status");
        _healthLabel = ui.GetNode<Label>("Panel/VBox/Health");

        string stationId = string.IsNullOrWhiteSpace(GameSession.CurrentStationId)
            ? "DerelictOutpost"
            : GameSession.CurrentStationId;
        StationDefinition stationDef = StationRegistry.Get(stationId);
        string stationText = stationDef?.DisplayName ?? "Derelict Outpost";
        _stationLabel.Text = stationText;
        _stationTitle.Text = stationText;

        PlayerState.ResetOnFootHealth();
        SpawnPlayer();
        SpawnWeapon();
        SpawnHostiles();
        MarkBoardingObjective();
        CallDeferred(nameof(BindInteractables));
        UpdateStatus();
    }

    public override void _Process(double delta)
    {
        if (_playerController == null)
            return;

        // Player downed: emergency launch back to Earth Hub with recovery penalty.
        if (!PlayerState.OnFootIsAlive)
        {
            HandlePlayerDown();
            return;
        }

        UpdateStatus();
    }

    private void SpawnPlayer()
    {
        Marker3D spawnMarker = GetNode<Marker3D>("AirlockMarker");
        _playerController = GD.Load<PackedScene>(StationPlayerScenePath).Instantiate<FPSController>();
        _playerController.Name = "Player";
        _playerController.AutoSpawnFPSWeapon = false;
        _playerController.HideCombatHud = false;
        _playerController.UseSharedOnFootHealth = true;
        _playerController.GlobalTransform = spawnMarker.GlobalTransform;
        AddChild(_playerController);
    }

    private void SpawnWeapon()
    {
        Camera3D camera = _playerController.Camera;
        _weapon = new OnFootWeapon
        {
            Name = "OnFootWeapon",
        };
        camera.AddChild(_weapon);
        _weapon.Attach(camera, _playerController);
        _weapon.SetArmed(true);
    }

    private void SpawnHostiles()
    {
        Camera3D playerCamera = _playerController.Camera;
        Node3D spawnRoot = GetNode<Node3D>("HostileSpawns");
        int index = 0;
        foreach (Node child in spawnRoot.GetChildren())
        {
            if (child is not Marker3D marker)
                continue;

            var hostile = new StationHostile
            {
                Name = $"Hostile_{index++}",
            };
            AddChild(hostile);
            hostile.GlobalTransform = marker.GlobalTransform;
            hostile.SetPlayer(_playerController, playerCamera);
            hostile.Killed += OnHostileKilled;
            _hostiles.Add(hostile);
        }
    }

    private void OnHostileKilled()
    {
        UpdateStatus();

        int remaining = CountLiveHostiles();
        if (remaining > 0)
            return;

        _combatComplete = true;
        _weapon?.SetArmed(false);
        MissionService.TryAdvanceStationClearObjectives(GetStationId());

        UpdateStatus();
    }

    private void MarkBoardingObjective()
    {
        MissionService.TryAdvanceHostileBoardingEntry(GetStationId());
    }

    private int CountLiveHostiles()
    {
        int count = 0;
        foreach (StationHostile h in _hostiles)
        {
            if (h != null && IsInstanceValid(h) && h.IsAlive)
                count++;
        }
        return count;
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
        if (actionId != "launch")
            return;

        if (!_combatComplete)
        {
            _statusLabel.Text = $"Hostiles remaining: {CountLiveHostiles()}. Clear them before disembarking.";
            return;
        }

        LaunchBackToSpace();
    }

    private void LaunchBackToSpace()
    {
        string stationId = GetStationId();

        GameSession.LaunchFrom(stationId);
        GameSession.PendingStatusMessage = "Undocking from hostile station.";
        _weapon?.SetArmed(false);
        _ = ChangeScene(GameplayScenePath, "Launching...");
    }

    private void HandlePlayerDown()
    {
        _weapon?.SetArmed(false);
        float recoveryCost = Mathf.Min(150f, PlayerState.Credits);
        PlayerState.TrySpendCredits(recoveryCost);
        PlayerState.ResetOnFootHealth();
        PlayerState.RepairHull();
        GameSession.DockAt("EarthHub");
        GameSession.PendingStatusMessage = $"Extraction complete. Recovered at Earth Hub. Medical fee: {recoveryCost:F0} credits.";
        _ = ChangeScene(StationRegistry.CivilianStationScene, "Emergency extraction to Earth Hub...");
    }

    private void UpdateStatus()
    {
        int remaining = CountLiveHostiles();
        if (_combatComplete || remaining == 0)
            _statusLabel.Text = "All hostiles neutralized. Head to the launch pad to leave.";
        else
            _statusLabel.Text = $"Hostiles remaining: {remaining}. Left click to fire.";

        _healthLabel.Text = $"Health: {PlayerState.OnFootHealth:F0}/{PlayerState.MaxOnFootHealth:F0}";
    }

    private string GetStationId()
    {
        return string.IsNullOrWhiteSpace(GameSession.CurrentStationId)
            ? "DerelictOutpost"
            : GameSession.CurrentStationId;
    }

    private Task ChangeScene(string path, string status)
    {
        if (SceneTransitionController.Instance != null)
            return SceneTransitionController.Instance.ChangeSceneWithFade(path, status);

        GetTree().ChangeSceneToFile(path);
        return Task.CompletedTask;
    }
}
