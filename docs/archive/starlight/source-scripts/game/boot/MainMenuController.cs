using Godot;
using System.Threading.Tasks;
using Starlight.Game.Runtime;
using Starlight.Game.Stations;

namespace Starlight.Game.Boot;

/// <summary>
/// Minimal foundation boot flow. This replaces the prototype scene as the normal entry point.
/// </summary>
public partial class MainMenuController : Control
{
    private const string GameplayScenePath = "res://scenes/game/space/Gameplay.tscn";
    private const string StationStubScenePath = "res://scenes/game/station/StationStub.tscn";

    private Button _newGameButton;
    private Button _continueButton;
    private Button _quitButton;
    private Label _statusLabel;

    public override void _Ready()
    {
        _newGameButton = GetNode<Button>("CenterPanel/VBox/NewGameButton");
        _continueButton = GetNode<Button>("CenterPanel/VBox/ContinueButton");
        _quitButton = GetNode<Button>("CenterPanel/VBox/QuitButton");
        _statusLabel = GetNode<Label>("CenterPanel/VBox/StatusLabel");

        _newGameButton.Pressed += OnNewGamePressed;
        _continueButton.Pressed += OnContinuePressed;
        _quitButton.Pressed += OnQuitPressed;

        bool hasSave = SaveService.HasLoadableDockedSave();
        _continueButton.Disabled = !hasSave;
        _statusLabel.Text = hasSave
            ? "Continue is available."
            : "No save found yet. Start a new game.";
    }

    private async void OnNewGamePressed()
    {
        GameSession.StartNewGame();
        await ChangeScene(GameplayScenePath, "Starting new game...");
    }

    private async void OnContinuePressed()
    {
        if (!SaveService.TryLoadDockedStation(out string stationId))
        {
            _statusLabel.Text = "Save file could not be loaded. Start a new game instead.";
            _continueButton.Disabled = true;
            return;
        }

        GameSession.ContinueGame();
        GameSession.DockAt(stationId);
        string interiorScene = StationRegistry.ResolveInteriorScene(stationId);
        await ChangeScene(interiorScene, "Continuing from dock...");
    }

    private void OnQuitPressed()
    {
        GetTree().Quit();
    }

    private Task ChangeScene(string path, string status)
    {
        if (SceneTransitionController.Instance != null)
            return SceneTransitionController.Instance.ChangeSceneWithFade(path, status);

        GetTree().ChangeSceneToFile(path);
        return Task.CompletedTask;
    }
}
