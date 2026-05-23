using Godot;

namespace Starlight.StationInterior;

/// <summary>
/// A cover position placed in a level. Place the node where the NPC should stand/crouch.
/// Point the -Z arrow (red arrow) toward where the threat comes from.
/// Cover is valid when the player is within a cone of the arrow direction.
///
/// Peek flags are auto-detected from raycasts at level load, or can be set manually.
/// </summary>
[Tool]
[GlobalClass]
public partial class CoverPoint : Marker3D
{
    public enum CoverType { Low, High }

    /// <summary>
    /// Direction toward the threat / combat zone. The NPC stands AT this node
    /// and peeks in this direction. Syncs with the marker's -Z in the editor.
    /// </summary>
    [Export] public Vector3 CoverNormal { get; set; } = Vector3.Forward;

    /// <summary>
    /// Half-angle in degrees for cover validity. If the player is outside this cone
    /// from the arrow direction, this cover point isn't useful against that threat.
    /// </summary>
    [Export] public float ValidityAngle { get; set; } = 60.0f;

    /// <summary>
    /// Height of the cover geometry in meters.
    /// Low cover (< 1.2m): crates, consoles, half-walls. Enemy crouches and peeks over.
    /// High cover (> 1.6m): walls, pillars, doorframes. Enemy stands and peeks around.
    /// </summary>
    [Export] public float CoverHeight { get; set; } = 1.5f;

    [Export] public CoverType Type { get; set; } = CoverType.High;

    [Export] public bool CanPeekLeft { get; set; }
    [Export] public bool CanPeekRight { get; set; }
    [Export] public bool CanPeekOver { get; set; }

    /// <summary>
    /// Set by CoverManager at runtime. Only one enemy per point.
    /// </summary>
    public HostileNPC OccupiedBy { get; set; }
    public bool IsAvailable => OccupiedBy == null;

    /// <summary>
    /// The right direction relative to the cover (perpendicular to normal, in XZ plane).
    /// </summary>
    public Vector3 CoverRight => CoverNormal.Cross(Vector3.Up).Normalized();

    /// <summary>
    /// Is this cover point valid against a threat at the given position?
    /// Returns true if the threat is within the validity cone of the arrow direction.
    /// </summary>
    public bool IsValidAgainst(Vector3 threatPos)
    {
        var toThreat = (threatPos - GlobalPosition);
        toThreat.Y = 0;
        if (toThreat.LengthSquared() < 0.01f) return true;

        var normal = CoverNormal;
        normal.Y = 0;
        if (normal.LengthSquared() < 0.01f) return true;

        float angle = normal.Normalized().AngleTo(toThreat.Normalized());
        return angle <= Mathf.DegToRad(ValidityAngle);
    }

    /// <summary>
    /// Auto-detect peek flags by raycasting from the cover position.
    /// Call this after the level geometry is loaded and physics are ready.
    /// </summary>
    public void DetectPeekFlags(PhysicsDirectSpaceState3D spaceState)
    {
        var pos = GlobalPosition;
        var right = CoverRight;
        float peekOffset = 0.8f;
        float checkDist = 2.0f;
        uint envMask = 0b0000_0001; // environment layer

        // Peek right: is there open space to the right?
        CanPeekRight = !IsBlocked(spaceState, pos + right * peekOffset,
            pos + right * peekOffset + CoverNormal * checkDist, envMask);

        // Peek left: is there open space to the left?
        CanPeekLeft = !IsBlocked(spaceState, pos - right * peekOffset,
            pos - right * peekOffset + CoverNormal * checkDist, envMask);

        // Peek over: only for low cover — is there space above?
        if (Type == CoverType.Low)
        {
            var abovePos = pos + new Vector3(0, CoverHeight + 0.3f, 0);
            CanPeekOver = !IsBlocked(spaceState, abovePos,
                abovePos + CoverNormal * checkDist, envMask);
        }
        else
        {
            CanPeekOver = false;
        }
    }

    /// <summary>
    /// Which side should an enemy peek from to see the given threat position?
    /// Returns the world-space offset to apply when peeking, or Vector3.Zero if no peek available.
    /// </summary>
    public Vector3 GetPeekOffset(Vector3 threatPos)
    {
        var right = CoverRight;
        var toThreat = (threatPos - GlobalPosition).Normalized();
        float dot = toThreat.Dot(right);

        // Threat is to the right → peek right. Threat to left → peek left.
        if (dot > 0 && CanPeekRight)
            return right * 0.7f;
        if (dot <= 0 && CanPeekLeft)
            return -right * 0.7f;
        // Fallback: try the other side
        if (CanPeekRight)
            return right * 0.7f;
        if (CanPeekLeft)
            return -right * 0.7f;
        // Low cover: peek over
        if (CanPeekOver)
            return new Vector3(0, 0.5f, 0);

        return Vector3.Zero; // no valid peek
    }

    /// <summary>
    /// Check if a peek from this cover toward a threat position has line of sight.
    /// </summary>
    public bool HasPeekLOS(PhysicsDirectSpaceState3D spaceState, Vector3 threatPos)
    {
        var peekOffset = GetPeekOffset(threatPos);
        if (peekOffset == Vector3.Zero) return false;

        float eyeHeight = (Type == CoverType.Low && peekOffset.Y > 0) ? CoverHeight + 0.5f : 1.5f;
        var from = GlobalPosition + peekOffset + new Vector3(0, eyeHeight, 0);
        var to = threatPos + new Vector3(0, 1.2f, 0);

        return !IsBlocked(spaceState, from, to, 0b0000_0001);
    }

    private static bool IsBlocked(PhysicsDirectSpaceState3D spaceState, Vector3 from, Vector3 to, uint mask)
    {
        var query = PhysicsRayQueryParameters3D.Create(from, to);
        query.CollisionMask = mask;
        var result = spaceState.IntersectRay(query);
        return result.Count > 0;
    }

    // --- Visual gizmo ---

    private MeshInstance3D _vizPlane;
    private MeshInstance3D _vizArrow;

    public override void _Ready()
    {
        if (Engine.IsEditorHint())
            CallDeferred(MethodName.BuildVisual);
        else
            CallDeferred(MethodName.BuildVisual);
    }

    public override void _Process(double delta)
    {
        if (!Engine.IsEditorHint()) return;

        // Keep CoverNormal synced with the marker's forward direction in the editor
        CoverNormal = -GlobalTransform.Basis.Z.Normalized();
    }

    private void BuildVisual()
    {
        // Plane = NPC footprint (capsule diameter 0.6m). Must not overlap geometry.
        var coverColor = Type == CoverType.Low
            ? new Color(0.2f, 0.6f, 1.0f, 0.35f)   // blue = low cover
            : new Color(1.0f, 0.5f, 0.1f, 0.35f);   // orange = high cover

        var planeMat = new StandardMaterial3D
        {
            ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
            Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
            CullMode = BaseMaterial3D.CullModeEnum.Disabled,
            AlbedoColor = coverColor,
        };

        // Circle mesh matching capsule diameter — this is the NPC's exact standing area
        var planeMesh = new CylinderMesh
        {
            TopRadius = 0.3f,   // capsule radius
            BottomRadius = 0.3f,
            Height = 0.05f,     // thin disc
        };
        planeMesh.SurfaceSetMaterial(0, planeMat);
        _vizPlane = new MeshInstance3D
        {
            Mesh = planeMesh,
            Position = new Vector3(0, 0.03f, 0),
        };
        AddChild(_vizPlane);

        // Red arrow pointing toward threat direction (-Z = marker forward = CoverNormal)
        var arrowMat = new StandardMaterial3D
        {
            ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
            Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
            AlbedoColor = new Color(1, 0.15f, 0.1f, 0.7f),
        };

        // Shaft
        var shaftMesh = new BoxMesh { Size = new Vector3(0.06f, 0.06f, 1.2f) };
        shaftMesh.SurfaceSetMaterial(0, arrowMat);
        _vizArrow = new MeshInstance3D
        {
            Mesh = shaftMesh,
            Position = new Vector3(0, 0.5f, -0.6f),
        };
        AddChild(_vizArrow);

        // Arrowhead
        var headMesh = new PrismMesh { Size = new Vector3(0.3f, 0.3f, 0.35f) };
        headMesh.SurfaceSetMaterial(0, arrowMat);
        var arrowHead = new MeshInstance3D
        {
            Mesh = headMesh,
            Position = new Vector3(0, 0.5f, -1.35f),
            Rotation = new Vector3(Mathf.DegToRad(-90), 0, 0),
        };
        AddChild(arrowHead);

        // Height bar on the BACK edge (behind NPC, where the wall is)
        var heightMat = new StandardMaterial3D
        {
            ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
            Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
            AlbedoColor = coverColor with { A = 0.5f },
        };
        var heightMesh = new BoxMesh { Size = new Vector3(0.8f, CoverHeight, 0.04f) };
        heightMesh.SurfaceSetMaterial(0, heightMat);
        var heightBar = new MeshInstance3D
        {
            Mesh = heightMesh,
            Position = new Vector3(0, CoverHeight / 2.0f, 0.4f), // behind the NPC (+Z = back)
        };
        AddChild(heightBar);
    }

    /// <summary>
    /// Call this to hide visuals at runtime (e.g. when starting gameplay).
    /// </summary>
    public void HideVisual()
    {
        foreach (var child in GetChildren())
        {
            if (child is MeshInstance3D mesh)
                mesh.Visible = false;
        }
    }

    /// <summary>
    /// Call this to show visuals (e.g. debug mode).
    /// </summary>
    public void ShowVisual()
    {
        foreach (var child in GetChildren())
        {
            if (child is MeshInstance3D mesh)
                mesh.Visible = true;
        }
    }
}
