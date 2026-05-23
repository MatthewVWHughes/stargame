using Godot;
using System.Threading.Tasks;

namespace Starlight.Game.Runtime;

/// <summary>
/// Global fade transition layer for VS1 scene swaps.
/// Keeps early scene changes readable and gives us one place to harden later.
/// </summary>
public partial class SceneTransitionController : CanvasLayer
{
    public static SceneTransitionController Instance { get; private set; }

    private ColorRect _fadeRect;
    private Label _statusLabel;
    private bool _busy;

    public override void _Ready()
    {
        Instance = this;
        ProcessMode = ProcessModeEnum.Always;

        _fadeRect = GetNode<ColorRect>("FadeRect");
        _statusLabel = GetNode<Label>("StatusLabel");
        _fadeRect.Color = new Color(0f, 0f, 0f, 0f);
        _statusLabel.Visible = false;
    }

    public async Task ChangeSceneWithFade(string scenePath, string statusText = "")
    {
        if (_busy)
            return;

        _busy = true;
        GetTree().Paused = true;

        _statusLabel.Text = statusText;
        _statusLabel.Visible = !string.IsNullOrWhiteSpace(statusText);

        await FadeTo(1f, 0.2f);
        GetTree().Paused = false;
        GetTree().ChangeSceneToFile(scenePath);
        await ToSignal(GetTree(), SceneTree.SignalName.ProcessFrame);
        await FadeTo(0f, 0.2f);

        _statusLabel.Visible = false;
        _busy = false;
    }

    private async Task FadeTo(float alpha, float duration)
    {
        var tween = CreateTween();
        tween.SetPauseMode(Tween.TweenPauseMode.Process);
        tween.TweenProperty(_fadeRect, "color:a", alpha, duration);
        await ToSignal(tween, Tween.SignalName.Finished);
    }
}
