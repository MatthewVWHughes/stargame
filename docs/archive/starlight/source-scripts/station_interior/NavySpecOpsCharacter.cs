using Godot;

namespace Starlight.StationInterior;

/// <summary>
/// Runtime controller for the NavySpecOps character.
/// Drives the AnimationTree state machine configured in the scene file.
/// </summary>
public partial class NavySpecOpsCharacter : Node3D
{
	public enum CombatState
	{
		Idle,
		Walking,
		Running,
		Aiming,
		Firing,
		Crouching,
	}

	// Map CombatState enum to AnimationTree state machine node names
	private static readonly string[] StateNames =
	{
		"Idle",   // Idle
		"Walk",   // Walking
		"Run",    // Running
		"Aim",    // Aiming
		"Fire",   // Firing
		"Crouch", // Crouching
	};

	private AnimationTree _animTree;
	private AnimationNodeStateMachinePlayback _playback;
	private CombatState _currentState = CombatState.Idle;

	public CombatState CurrentState => _currentState;

	public override void _Ready()
	{
		_animTree = FindChildOfType<AnimationTree>(this);
		if (_animTree == null)
		{
			GD.PrintErr("NavySpecOpsCharacter: No AnimationTree found");
			return;
		}
		_playback = (AnimationNodeStateMachinePlayback)_animTree.Get("parameters/playback");
	}

	/// <summary>
	/// Travel to the animation state corresponding to the given combat state.
	/// No-op if already in that state.
	/// </summary>
	public void SetState(CombatState state)
	{
		if (_playback == null || state == _currentState) return;
		_currentState = state;
		_playback.Travel(StateNames[(int)state]);
	}

	/// <summary>
	/// Force travel to a specific state machine node by name.
	/// Use for one-shot states like Hit and Death.
	/// </summary>
	public void TravelTo(string stateName)
	{
		_playback?.Travel(stateName);
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
