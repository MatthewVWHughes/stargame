using Godot;
using System.Collections.Generic;

namespace Starlight.StationInterior;

/// <summary>
/// Central registry for CoverPoint nodes. Handles reservation and tactical scoring.
/// One instance per arena, shared by all HostileNPCs.
/// </summary>
public partial class CoverManager : Node
{
    private readonly List<CoverPoint> _coverPoints = new();

    /// <summary>
    /// Scan a container (and its descendants) for CoverPoint nodes and register them.
    /// Plain Marker3D nodes are ignored — only intentional CoverPoints are used.
    /// </summary>
    public void Initialize(Node3D coverContainer)
    {
        _coverPoints.Clear();
        CollectCoverPoints(coverContainer);
        GD.Print($"CoverManager: registered {_coverPoints.Count} cover points");
    }

    private void CollectCoverPoints(Node parent)
    {
        foreach (var child in parent.GetChildren())
        {
            if (child is CoverPoint cp)
                _coverPoints.Add(cp);
            else if (child is Node node)
                CollectCoverPoints(node); // search nested children too
        }
    }

    /// <summary>
    /// Auto-detect peek flags on all cover points. Call after physics are synced.
    /// </summary>
    public void DetectAllPeekFlags(PhysicsDirectSpaceState3D spaceState)
    {
        foreach (var cp in _coverPoints)
            cp.DetectPeekFlags(spaceState);

        int peekable = 0;
        foreach (var cp in _coverPoints)
            if (cp.CanPeekLeft || cp.CanPeekRight || cp.CanPeekOver) peekable++;
        GD.Print($"CoverManager: {peekable}/{_coverPoints.Count} cover points have peek capability");
    }

    /// <summary>
    /// Score all available cover points and return the best one for this enemy.
    /// Returns null if no valid cover exists.
    /// </summary>
    public CoverPoint FindBestCover(HostileNPC requester, Vector3 threatPos)
    {
        var spaceState = requester.GetWorld3D().DirectSpaceState;
        var requesterPos = requester.GlobalPosition;

        CoverPoint best = null;
        float bestScore = float.MinValue;

        foreach (var cp in _coverPoints)
        {
            if (!cp.IsAvailable && cp.OccupiedBy != requester)
                continue;

            // Skip if threat is outside this cover's validity cone
            if (!cp.IsValidAgainst(threatPos))
                continue;

            float score = ScoreCover(cp, requesterPos, threatPos, spaceState);
            if (score > bestScore)
            {
                bestScore = score;
                best = cp;
            }
        }

        return best;
    }

    private float ScoreCover(CoverPoint cp, Vector3 requesterPos, Vector3 threatPos,
        PhysicsDirectSpaceState3D spaceState)
    {
        // --- GATE: Does this cover block LOS from threat? ---
        // Check from hiding height: crouching (0.8m) for low cover, standing (1.5m) for high
        float hideHeight = cp.Type == CoverPoint.CoverType.Low ? 0.8f : 1.5f;
        var coverEye = cp.GlobalPosition + new Vector3(0, hideHeight, 0);
        var threatEye = threatPos + new Vector3(0, 1.2f, 0);
        var query = PhysicsRayQueryParameters3D.Create(coverEye, threatEye);
        query.CollisionMask = 0b0000_0001; // environment only
        var result = spaceState.IntersectRay(query);

        // If ray reaches threat unblocked, this cover is useless
        if (result.Count == 0)
            return -1000f;

        // --- GATE: Can I shoot from here by peeking? ---
        if (!cp.HasPeekLOS(spaceState, threatPos))
            return -1000f;

        float score = 0f;

        // --- Distance to self (closer = less time exposed) ---
        float distToSelf = requesterPos.DistanceTo(cp.GlobalPosition);
        score += (1.0f - Mathf.Clamp(distToSelf / 25.0f, 0f, 1f)) * 0.3f;

        // --- Distance from threat (sweet spot 8-15m) ---
        float distToThreat = cp.GlobalPosition.DistanceTo(threatPos);
        if (distToThreat < 5.0f)
            score -= 0.2f;
        else if (distToThreat >= 8.0f && distToThreat <= 15.0f)
            score += 0.15f;

        // --- Spread from allies ---
        float closestAllyDist = float.MaxValue;
        foreach (var other in _coverPoints)
        {
            if (other == cp || other.OccupiedBy == null) continue;
            float d = cp.GlobalPosition.DistanceTo(other.GlobalPosition);
            if (d < closestAllyDist) closestAllyDist = d;
        }
        if (closestAllyDist < 3.0f)
            score -= 0.4f;
        else if (closestAllyDist > 6.0f)
            score += 0.1f;

        // --- Flanking angle bonus ---
        var coverToThreat = (threatPos - cp.GlobalPosition).Normalized();
        var requesterToThreat = (threatPos - requesterPos).Normalized();
        float angleDiff = coverToThreat.AngleTo(requesterToThreat);
        score += Mathf.Clamp(angleDiff / Mathf.DegToRad(90f), 0f, 1f) * 0.15f;

        // --- Height advantage ---
        if (cp.GlobalPosition.Y > requesterPos.Y + 1.0f)
            score += 0.1f;

        return score;
    }

    public bool Reserve(CoverPoint point, HostileNPC npc)
    {
        if (point == null) return false;
        if (!point.IsAvailable && point.OccupiedBy != npc) return false;
        point.OccupiedBy = npc;
        return true;
    }

    public void Release(HostileNPC npc)
    {
        foreach (var cp in _coverPoints)
            if (cp.OccupiedBy == npc)
                cp.OccupiedBy = null;
    }

    public void ReleaseAll()
    {
        foreach (var cp in _coverPoints)
            cp.OccupiedBy = null;
    }

    /// <summary>
    /// Check if an enemy's current cover is flanked or has lost LOS for peeking.
    /// </summary>
    public bool ShouldReposition(HostileNPC npc, Vector3 currentThreatPos)
    {
        CoverPoint held = null;
        foreach (var cp in _coverPoints)
            if (cp.OccupiedBy == npc) { held = cp; break; }

        if (held == null) return true;

        // Flanked: threat is within 4m
        if (held.GlobalPosition.DistanceTo(currentThreatPos) < 4.0f)
            return true;

        // Lost peek LOS
        var spaceState = npc.GetWorld3D().DirectSpaceState;
        if (!held.HasPeekLOS(spaceState, currentThreatPos))
            return true;

        return false;
    }

    /// <summary>
    /// Get the CoverPoint currently held by an NPC, or null.
    /// </summary>
    public CoverPoint GetHeldCover(HostileNPC npc)
    {
        foreach (var cp in _coverPoints)
            if (cp.OccupiedBy == npc) return cp;
        return null;
    }
}
