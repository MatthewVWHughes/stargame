using Godot;
using Starlight.Orbital;
using StarNode = Starlight.Star.Star;

namespace Starlight.Game.Space;

// Walks a StarSystemDefinition and materialises an equivalent scene graph
// under the provided `starRoot` node (the "Star" container inside a
// StarSystem.tscn / SolSector.tscn shell).
//
// No Action<> hooks: the builder is a pure interpretation of data. If a
// system needs a behaviour that can't be expressed in StarSystemDefinition,
// add the data field here rather than bolting on a lambda.
public static class StarSystemBuilder
{
	public static StarSystemBuildContext Build(StarSystemDefinition definition, Node3D sceneRoot, Node3D starRoot)
	{
		ClearChildren(starRoot);

		var context = new StarSystemBuildContext(sceneRoot, starRoot);

		foreach (SystemBodyDefinition body in definition.Bodies)
			BuildBody(body, starRoot, "Star", context);

		foreach (AsteroidBeltDefinition belt in definition.AsteroidBelts)
			BuildAsteroidBelt(belt, starRoot, context);

		foreach (JumpGateDefinition gate in definition.JumpGates)
			BuildJumpGate(gate, starRoot, context);

		return context;
	}

	private static void BuildBody(SystemBodyDefinition definition, Node3D parent, string parentPath, StarSystemBuildContext context)
	{
		Node3D bodyNode = definition.Orbit == null
			? new Node3D()
			: new OrbitalBody
			{
				SemiMajorAxis = definition.Orbit.SemiMajorAxis,
				Period = definition.Orbit.Period,
				PhaseOffset = definition.Orbit.PhaseOffset,
				Eccentricity = definition.Orbit.Eccentricity,
				Inclination = definition.Orbit.Inclination,
			};
		bodyNode.Name = definition.Name;
		parent.AddChild(bodyNode);

		string bodyPath = $"{parentPath}/{definition.Name}";
		context.RegisterNode(bodyPath, bodyNode);

		Node3D visualInstance = null;
		if (definition.SceneVisual != null)
		{
			visualInstance = AddSceneVisual(definition.SceneVisual, bodyNode, context);
			ApplyStellarClassIfAny(visualInstance, definition.StellarClass);
		}
		else if (definition.SphereVisual != null)
			AddSphereVisual(definition.SphereVisual, bodyNode);

		if (visualInstance != null && !string.IsNullOrWhiteSpace(definition.LabelText))
			ApplyLabelText(visualInstance, definition.LabelText);

		if (visualInstance != null)
			context.RegisterNode($"{bodyPath}/{definition.SceneVisual.NodeName}", visualInstance);

		if (definition.Ring != null)
			AddRing(definition.Ring, bodyNode);

		foreach (SystemBodyDefinition child in definition.Children)
			BuildBody(child, bodyNode, bodyPath, context);
	}

	private static Node3D AddSceneVisual(SceneVisualDefinition visual, Node3D parent, StarSystemBuildContext context)
	{
		PackedScene scene = GD.Load<PackedScene>(visual.ScenePath);
		Node3D instance = scene.Instantiate<Node3D>();
		instance.Name = visual.NodeName;
		if (visual.Scale.HasValue)
			instance.Scale = visual.Scale.Value;
		parent.AddChild(instance);

		if (instance is StarNode)
			context.SetPrimaryStar(instance);

		ConfigureStarLinkedNode(instance, context.StarNode);
		return instance;
	}

	private static void AddSphereVisual(SphereVisualDefinition visual, Node3D parent)
	{
		var mesh = new SphereMesh
		{
			Radius = visual.Radius,
			Height = visual.Radius * 2f,
			RadialSegments = visual.RadialSegments,
			Rings = visual.Rings,
		};

		var material = new StandardMaterial3D
		{
			AlbedoColor = visual.AlbedoColor,
			Roughness = visual.Roughness,
			ShadingMode = visual.Unshaded
				? BaseMaterial3D.ShadingModeEnum.Unshaded
				: BaseMaterial3D.ShadingModeEnum.PerPixel,
		};
		if (!string.IsNullOrWhiteSpace(visual.TexturePath))
			material.AlbedoTexture = GD.Load<Texture2D>(visual.TexturePath);

		var meshInstance = new MeshInstance3D
		{
			Name = visual.NodeName,
			Mesh = mesh,
			MaterialOverride = material,
			ExtraCullMargin = 100000f,
		};
		if (visual.Scale.HasValue)
			meshInstance.Scale = visual.Scale.Value;
		parent.AddChild(meshInstance);
	}

	private static void AddRing(RingDefinition ring, Node3D parent)
	{
		var mesh = new QuadMesh { Size = new Vector2(ring.Size, ring.Size) };
		var material = new StandardMaterial3D
		{
			AlbedoTexture = GD.Load<Texture2D>(ring.TexturePath),
			Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
			ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
			CullMode = BaseMaterial3D.CullModeEnum.Disabled,
		};

		var meshInstance = new MeshInstance3D
		{
			Name = ring.NodeName,
			Mesh = mesh,
			MaterialOverride = material,
			Rotation = ring.RotationEuler,
			ExtraCullMargin = 100000f,
		};
		parent.AddChild(meshInstance);
	}

	private static void BuildAsteroidBelt(AsteroidBeltDefinition definition, Node3D starRoot, StarSystemBuildContext context)
	{
		var belt = new AsteroidBelt
		{
			Name = definition.NodeName,
			InnerRadius = definition.InnerRadius,
			OuterRadius = definition.OuterRadius,
			VerticalSpread = definition.VerticalSpread,
			InstanceCount = definition.InstanceCount,
			MinRockSize = definition.MinRockSize,
			MaxRockSize = definition.MaxRockSize,
		};
		starRoot.AddChild(belt);
		context.RegisterNode($"Star/{definition.NodeName}", belt);
	}

	private const string JumpGateScenePath = "res://scenes/game/space/JumpGate.tscn";

	private static void BuildJumpGate(JumpGateDefinition definition, Node3D starRoot, StarSystemBuildContext context)
	{
		// Anchor + gate are a single logical "gate" in the data, but
		// structurally split into (a) a LagrangeAnchor that keeps us on the
		// anti-sunward side of a body, and (b) the JumpGate scene instance
		// carrying the JumpGateController script.
		string anchorName = string.IsNullOrWhiteSpace(definition.NodeName)
			? $"{definition.GateId}_Anchor"
			: definition.NodeName;

		var anchor = new LagrangeAnchor
		{
			Name = anchorName,
			Distance = definition.Distance,
			Point = LagrangeAnchor.LagrangePoint.L2,
		};

		// By default the anchor sits under the "Star" container; a multi-star
		// system can parent a gate under a specific primary (e.g.
		// Alpha Centauri A) by setting parent_path.
		Node3D anchorParent = definition.ParentPath == "Star"
			? starRoot
			: context.TryGetNode(definition.ParentPath) ?? starRoot;
		anchorParent.AddChild(anchor);

		Node3D planet = context.TryGetNode(definition.AnchorBody);
		Node3D star = context.StarNode;
		if (planet != null)
			anchor.PlanetPath = anchor.GetPathTo(planet);
		if (star != null)
			anchor.StarPath = anchor.GetPathTo(star);
		if (planet != null || star != null)
			anchor.SetTargets(planet, star);

		string anchorLogicalPath = definition.ParentPath == "Star"
			? $"Star/{anchorName}"
			: $"{definition.ParentPath}/{anchorName}";
		context.RegisterNode(anchorLogicalPath, anchor);

		PackedScene gateScene = GD.Load<PackedScene>(JumpGateScenePath);
		JumpGateController controller = gateScene.Instantiate<JumpGateController>();
		controller.Name = "JumpGate";
		controller.GateId = definition.GateId;
		controller.DestinationSystemId = definition.DestinationSystemId;
		controller.ArrivalGateId = definition.ArrivalGateId;
		controller.DisplayName = definition.DisplayName;
		controller.ActivationRange = definition.ActivationRange;

		// AddChild fires _Ready which adds the controller to the jump_gates
		// group and sets the Label3D text from controller.DisplayName — so
		// all wiring is complete on return.
		anchor.AddChild(controller);

		context.RegisterNode($"{anchorLogicalPath}/JumpGate", controller);
	}

	private static void ConfigureStarLinkedNode(Node3D node, Node3D starNode)
	{
		if (starNode == null)
			return;

		// _Ready has already fired on the instance by the time we get here
		// (AddChild fires it synchronously), so writing StarPath alone does
		// nothing — the shader already resolved it to null. Hand the star
		// reference in directly via SetStar instead.
		switch (node)
		{
			case EarthShader earth:
				earth.StarPath = earth.GetPathTo(starNode);
				earth.SetStar(starNode);
				break;
			case MoonShader moon:
				moon.StarPath = moon.GetPathTo(starNode);
				moon.SetStar(starNode);
				break;
		}
	}

	private static void ApplyLabelText(Node3D instance, string labelText)
	{
		Label3D label = instance.GetNodeOrNull<Label3D>("Label3D");
		if (label != null)
			label.Text = labelText;
	}

	private static void ApplyStellarClassIfAny(Node3D instance, string stellarClass)
	{
		if (instance is not StarNode star || string.IsNullOrWhiteSpace(stellarClass))
			return;

		if (System.Enum.TryParse(stellarClass, ignoreCase: true, out StarNode.StellarClass parsed))
		{
			star.Class = parsed;
			star.ApplyClass();
		}
	}

	private static void ClearChildren(Node node)
	{
		foreach (Node child in node.GetChildren())
		{
			node.RemoveChild(child);
			child.QueueFree();
		}
	}
}
