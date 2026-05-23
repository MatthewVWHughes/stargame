using Godot;

namespace Starlight.Game.Space;

// Data-carrying runtime for a jump gate. The builder instantiates
// JumpGate.tscn as a child of a LagrangeAnchor, attaches this script, and
// populates the exported ids from the system definition.
//
// `jump_gates` is used as a scene-tree group so GameplayRoot can enumerate
// every gate in the current system without walking the node hierarchy.
public partial class JumpGateController : Node3D
{
	[Export] public string GateId { get; set; } = "";
	[Export] public string DestinationSystemId { get; set; } = "";
	[Export] public string ArrivalGateId { get; set; } = "";
	[Export] public string DisplayName { get; set; } = "Jump Gate";
	[Export] public float ActivationRange { get; set; } = 450f;

	public Marker3D GateCenter => GetNodeOrNull<Marker3D>("GateCenter");

	public override void _Ready()
	{
		AddToGroup("jump_gates");

		Label3D label = GetNodeOrNull<Label3D>("Label3D");
		if (label != null && !string.IsNullOrWhiteSpace(DisplayName))
			label.Text = DisplayName;
	}

	public Vector3 GetActivationPosition()
	{
		return GateCenter?.GlobalPosition ?? GlobalPosition;
	}
}
