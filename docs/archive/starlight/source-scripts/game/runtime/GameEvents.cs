using Godot;

namespace Starlight.Game.Runtime;

/// <summary>
/// Canonical cross-system event record types. Published via
/// <see cref="MessageBus"/>; any system can subscribe without coupling
/// to the emitter's node graph.
///
/// Node-local UI glue should keep using Godot signals — MessagePipe is for
/// signals that cross system boundaries (mission → UI → audio, combat →
/// economy, etc).
/// </summary>
public static class GameEvents
{
    // ---------- Missions ----------
    public readonly record struct MissionOffered(string MissionId);
    public readonly record struct MissionAccepted(string MissionId);
    public readonly record struct MissionObjectiveAdvanced(string MissionId, int NewObjectiveIndex);
    public readonly record struct MissionReadyToTurnIn(string MissionId);
    public readonly record struct MissionCompleted(string MissionId, float CreditsAwarded);

    // ---------- Combat ----------
    public readonly record struct HostileDestroyed(string DisplayName, Vector3 Position);
    public readonly record struct PlayerDamaged(float Amount, float RemainingHull);
    public readonly record struct PlayerShieldHit(float Amount, float RemainingShields);

    // ---------- Economy / cargo (Plan 07 hook) ----------
    public readonly record struct CargoPurchased(string CommodityId, float Amount, float Cost);
    public readonly record struct CargoSold(string CommodityId, float Amount, float Revenue);
    public readonly record struct StockChanged(string StationId, string CommodityId, float NewStock);

    // ---------- Docking / navigation ----------
    public readonly record struct PlayerDocked(string StationId);
    public readonly record struct PlayerUndocked(string StationId);

    // ---------- Player state ----------
    public readonly record struct CreditsChanged(float Delta, float NewBalance);
}
