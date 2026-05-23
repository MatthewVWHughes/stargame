using Godot;

namespace Starlight.Game.Stations;

/// <summary>
/// Cylinder-placeholder quest giver NPC for the VS1 slice.
/// HumanGen3D character art is not yet usable, so this is intentionally a cylinder + label.
/// The actual character model swap is a later pass; the NPC id + interaction wiring is the framework
/// we want to stabilize now.
///
/// Built programmatically so StationStubController can instance, position, and configure one per
/// station from data (StationRegistry). The interactable routes "talk:{npcId}" action ids so the
/// station controller can look up the right mission/dialog response.
/// </summary>
public partial class QuestGiverController : Node3D
{
    public const string TalkActionPrefix = "talk:";

    [Export] public string NpcId { get; set; } = "";
    [Export] public string DisplayName { get; set; } = "NPC";

    public override void _Ready()
    {
        BuildVisual();
        BuildInteractable();
        BuildNameLabel();
    }

    public static string BuildTalkAction(string npcId) => TalkActionPrefix + npcId;

    public static bool TryParseTalkAction(string actionId, out string npcId)
    {
        npcId = null;
        if (string.IsNullOrEmpty(actionId) || !actionId.StartsWith(TalkActionPrefix))
            return false;
        npcId = actionId.Substring(TalkActionPrefix.Length);
        return !string.IsNullOrEmpty(npcId);
    }

    private void BuildVisual()
    {
        var body = new MeshInstance3D
        {
            Name = "Body",
            Position = new Vector3(0f, 0.9f, 0f),
            Mesh = new CylinderMesh
            {
                TopRadius = 0.35f,
                BottomRadius = 0.45f,
                Height = 1.8f,
            },
            MaterialOverride = new StandardMaterial3D
            {
                AlbedoColor = new Color(0.85f, 0.75f, 0.3f),
                Roughness = 0.7f,
            },
        };
        AddChild(body);

        var head = new MeshInstance3D
        {
            Name = "Head",
            Position = new Vector3(0f, 2.0f, 0f),
            Mesh = new SphereMesh
            {
                Radius = 0.22f,
                Height = 0.44f,
            },
            MaterialOverride = new StandardMaterial3D
            {
                AlbedoColor = new Color(0.95f, 0.85f, 0.4f),
                Roughness = 0.6f,
            },
        };
        AddChild(head);
    }

    private void BuildInteractable()
    {
        var area = new StationInteractableArea
        {
            Name = "TalkArea",
            ActionId = BuildTalkAction(NpcId),
            InteractLabel = $"Talk to {DisplayName}",
            InteractionRange = 3.0f,
            CollisionLayer = 4,
            CollisionMask = 0,
        };
        AddChild(area);

        var shape = new CollisionShape3D
        {
            Shape = new CylinderShape3D { Radius = 0.9f, Height = 2.2f },
            Position = new Vector3(0f, 1.1f, 0f),
        };
        area.AddChild(shape);
    }

    private void BuildNameLabel()
    {
        var label = new Label3D
        {
            Name = "NameLabel",
            Position = new Vector3(0f, 2.6f, 0f),
            Billboard = BaseMaterial3D.BillboardModeEnum.Enabled,
            Text = DisplayName,
            FontSize = 26,
        };
        AddChild(label);
    }
}
