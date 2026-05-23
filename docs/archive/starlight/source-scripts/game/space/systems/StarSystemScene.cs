using Godot;

namespace Starlight.Game.Space;

// Attach to the root of a star-system scene shell (StarSystem.tscn). On _Ready
// it looks up the definition for SystemId in the catalog and asks
// StarSystemBuilder to populate the scene's "Star" child node.
public partial class StarSystemScene : Node3D
{
	[Export] public string SystemId { get; set; } = "sol";

	private bool _built;
	public StarSystemBuildContext BuildContext { get; private set; }

	public override void _Ready()
	{
		EnsureBuilt();
	}

	public void EnsureBuilt()
	{
		if (_built)
			return;

		Node3D starRoot = GetNode<Node3D>("Star");
		StarSystemDefinition definition = StarSystemCatalog.Get(SystemId);
		BuildContext = StarSystemBuilder.Build(definition, this, starRoot);
		_built = true;
	}
}
