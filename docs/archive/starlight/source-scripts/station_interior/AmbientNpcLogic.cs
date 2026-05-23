using Chickensoft.LogicBlocks;

namespace Starlight.StationInterior;

/// <summary>
/// LogicBlocks state machine for AmbientNPC.
///
/// The node holds movement, animation, and waypoint logic; this class owns
/// the state graph (Idle / Walking / FacingPlayer) and validates transitions.
/// Pattern-match on <c>_logic.Value is State.Idle</c> to check current state.
/// </summary>
[LogicBlock(typeof(State))]
public partial class AmbientNpcLogic : LogicBlock<AmbientNpcLogic.State>
{
    public override Transition GetInitialState() => To<State.Idle>();

    public static class Input
    {
        public readonly record struct IdleTimerExpired;
        public readonly record struct WaypointReached;
        public readonly record struct PlayerApproached;
        public readonly record struct PlayerDeparted;
        public readonly record struct NoWaypointsAvailable;
    }

    public abstract record State : StateLogic<State>
    {
        public record Idle : State,
            IGet<Input.IdleTimerExpired>,
            IGet<Input.PlayerApproached>
        {
            public Transition On(in Input.IdleTimerExpired _) => To<Walking>();
            public Transition On(in Input.PlayerApproached _) => To<FacingPlayer>();
        }

        public record Walking : State,
            IGet<Input.WaypointReached>,
            IGet<Input.PlayerApproached>,
            IGet<Input.NoWaypointsAvailable>
        {
            public Transition On(in Input.WaypointReached _) => To<Idle>();
            public Transition On(in Input.PlayerApproached _) => To<FacingPlayer>();
            public Transition On(in Input.NoWaypointsAvailable _) => To<Idle>();
        }

        public record FacingPlayer : State,
            IGet<Input.PlayerDeparted>
        {
            public Transition On(in Input.PlayerDeparted _) => To<Idle>();
        }
    }
}
