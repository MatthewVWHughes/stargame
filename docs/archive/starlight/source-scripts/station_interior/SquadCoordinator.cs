using Godot;
using System.Collections.Generic;

namespace Starlight.StationInterior;

/// <summary>
/// Squad-level coordination: attack tokens, confidence, alert propagation.
/// Ticks at 1Hz.
/// </summary>
public partial class SquadCoordinator : Node
{
    [Export] public int MaxAttackTokens = 3;

    private readonly List<HostileNPC> _members = new();
    private CoverManager _coverManager;
    private float _tickTimer;
    private float _confidence = 1.0f;

    private const float TickInterval = 1.0f;
    private const float AlertRange = 15.0f;
    private const float SpotRange = 20.0f;

    public float Confidence => _confidence;

    public void Initialize(CoverManager coverManager)
    {
        _coverManager = coverManager;
    }

    public void RegisterNPC(HostileNPC npc)
    {
        if (!_members.Contains(npc))
            _members.Add(npc);
    }

    public void UnregisterNPC(HostileNPC npc)
    {
        _members.Remove(npc);
        npc.HasAttackToken = false;
    }

    public void OnNPCDamaged(HostileNPC damaged)
    {
        var pos = damaged.GlobalPosition;
        foreach (var npc in _members)
        {
            if (npc == damaged || npc.CurrentState == HostileNPC.State.Dead) continue;
            if (npc.CurrentState != HostileNPC.State.Idle && npc.CurrentState != HostileNPC.State.Patrol) continue;
            if (npc.GlobalPosition.DistanceTo(pos) <= AlertRange)
                npc.AlertToPosition(pos);
        }
    }

    public void OnNPCSpottedPlayer(HostileNPC spotter, Vector3 playerPos)
    {
        foreach (var npc in _members)
        {
            if (npc == spotter || npc.CurrentState == HostileNPC.State.Dead) continue;
            if (npc.GlobalPosition.DistanceTo(spotter.GlobalPosition) > SpotRange) continue;
            npc.AlertToPosition(playerPos);
        }
    }

    public void Update(float dt)
    {
        _tickTimer -= dt;
        if (_tickTimer > 0) return;
        _tickTimer = TickInterval;

        CleanDead();
        UpdateConfidence();
        AssignAttackTokens();
    }

    private void CleanDead()
    {
        _members.RemoveAll(m => !IsInstanceValid(m) || m.CurrentState == HostileNPC.State.Dead);
    }

    private void UpdateConfidence()
    {
        if (_members.Count == 0) { _confidence = 0; return; }

        float totalHPFraction = 0;
        foreach (var npc in _members)
            totalHPFraction += npc.HP / npc.MaxHP;

        float avgHP = totalHPFraction / _members.Count;
        _confidence = _members.Count * avgHP; // scales with both count and health

        // Normalize: 5 healthy enemies = 5.0, 1 hurt enemy = ~0.3
        // Thresholds: > 0.6 push, 0.3-0.6 hold, < 0.3 retreat
    }

    private void AssignAttackTokens()
    {
        // Count current tokens
        int tokensUsed = 0;
        foreach (var npc in _members)
            if (npc.HasAttackToken) tokensUsed++;

        // Revoke tokens from enemies not in attack/engage/retreat states
        foreach (var npc in _members)
        {
            if (npc.HasAttackToken && npc.CurrentState != HostileNPC.State.Attack
                && npc.CurrentState != HostileNPC.State.Engage
                && npc.CurrentState != HostileNPC.State.Retreat)
            {
                npc.HasAttackToken = false;
                tokensUsed--;
            }
        }

        // Assign tokens to enemies that need them (in Attack state, closest first)
        if (tokensUsed < MaxAttackTokens)
        {
            var candidates = new List<HostileNPC>();
            foreach (var npc in _members)
            {
                if (!npc.HasAttackToken &&
                    (npc.CurrentState == HostileNPC.State.Attack ||
                     npc.CurrentState == HostileNPC.State.Engage ||
                     npc.CurrentState == HostileNPC.State.Retreat))
                {
                    candidates.Add(npc);
                }
            }

            // Sort by priority: Attack state first, then closest
            candidates.Sort((a, b) =>
            {
                // Prefer enemies already in Attack state
                int stateA = a.CurrentState == HostileNPC.State.Attack ? 0 : 1;
                int stateB = b.CurrentState == HostileNPC.State.Attack ? 0 : 1;
                if (stateA != stateB) return stateA.CompareTo(stateB);
                return 0; // equal priority
            });

            foreach (var npc in candidates)
            {
                if (tokensUsed >= MaxAttackTokens) break;
                npc.HasAttackToken = true;
                tokensUsed++;
            }
        }

        // Low confidence → trigger retreat for non-token holders
        if (_confidence < 0.3f)
        {
            foreach (var npc in _members)
            {
                if (!npc.HasAttackToken && npc.CurrentState == HostileNPC.State.Attack)
                {
                    // Force retreat on enemies without tokens
                    npc.HasAttackToken = false;
                    // They'll check FleeThreshold in their own update
                }
            }
        }
    }
}
