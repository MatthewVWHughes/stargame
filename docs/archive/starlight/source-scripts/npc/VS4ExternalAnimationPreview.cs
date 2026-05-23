using Godot;

namespace Starlight.Npc;

/// <summary>
/// Preview helper for testing one static VS4 character against one external
/// Mixamo-derived GLB animation.
/// </summary>
public partial class VS4ExternalAnimationPreview : Node3D
{
    [Export] public NodePath CharacterMeshPath = "CharacterMesh";
    [Export] public string AnimationSource;
    [Export] public string LibraryName = "Preview";

    public override void _Ready()
    {
        var mesh = GetNodeOrNull<Node>(CharacterMeshPath);
        if (mesh == null)
        {
            GD.PushError($"VS4ExternalAnimationPreview: CharacterMesh not found at '{CharacterMeshPath}'");
            return;
        }

        var skeleton = FindChildOfType<Skeleton3D>(mesh);
        if (skeleton == null)
        {
            GD.PushError("VS4ExternalAnimationPreview: imported character has no Skeleton3D");
            return;
        }

        var player = new AnimationPlayer
        {
            Name = "AnimationPlayer",
            RootNode = new NodePath(".."),
        };
        mesh.AddChild(player);

        var animationName = LoadExternalAnimation(
            player,
            AnimationSource,
            LibraryName,
            mesh.GetPathTo(skeleton).ToString());
        if (animationName == null)
            return;

        player.Play(animationName);
        GD.Print($"VS4ExternalAnimationPreview: playing '{animationName}' on '{Name}'");
    }

    private static string LoadExternalAnimation(
        AnimationPlayer player,
        string glbPath,
        string libraryName,
        string targetSkeletonPath)
    {
        var scene = ResourceLoader.Load<PackedScene>(glbPath);
        if (scene == null)
        {
            GD.PushWarning($"VS4ExternalAnimationPreview: could not load '{glbPath}'");
            return null;
        }

        var instance = scene.Instantiate<Node>();
        var sourcePlayer = FindChildOfType<AnimationPlayer>(instance);
        if (sourcePlayer == null)
        {
            GD.PushWarning($"VS4ExternalAnimationPreview: '{glbPath}' has no AnimationPlayer");
            instance.QueueFree();
            return null;
        }

        var library = new AnimationLibrary();
        string firstAnimation = null;
        foreach (var sourceName in sourcePlayer.GetAnimationList())
        {
            firstAnimation ??= sourceName;
            var animation = (Animation)sourcePlayer.GetAnimation(sourceName).Duplicate();
            animation.LoopMode = Animation.LoopModeEnum.Linear;
            RemapSkeletonTracks(animation, targetSkeletonPath);
            library.AddAnimation(sourceName, animation);
        }

        if (player.HasAnimationLibrary(libraryName))
            player.RemoveAnimationLibrary(libraryName);
        player.AddAnimationLibrary(libraryName, library);
        instance.QueueFree();
        return firstAnimation == null ? null : $"{libraryName}/{firstAnimation}";
    }

    private static void RemapSkeletonTracks(Animation animation, string targetSkeletonPath)
    {
        for (int i = 0; i < animation.GetTrackCount(); i++)
        {
            var pathText = animation.TrackGetPath(i).ToString();
            var colon = pathText.IndexOf(':');
            if (colon < 0) continue;

            var bonePath = pathText[colon..];
            animation.TrackSetPath(i, new NodePath($"{targetSkeletonPath}{bonePath}"));
        }
    }

    private static T FindChildOfType<T>(Node parent) where T : Node
    {
        foreach (var child in parent.GetChildren())
        {
            if (child is T found) return found;
            var result = FindChildOfType<T>(child);
            if (result != null) return result;
        }
        return null;
    }
}
