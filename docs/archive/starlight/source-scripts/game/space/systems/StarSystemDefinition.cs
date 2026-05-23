using System;
using System.Collections.Generic;
using Godot;

namespace Starlight.Game.Space;

// Data-driven world-loader types.
//
// Pure-data description of a star system. Loaded from JSON in Phase 2,
// walked by StarSystemBuilder in Phase 3. No behaviour here.
//
// Anything the builder needs per body/per system must be expressible as a
// field on these records. There is no Action<> escape hatch — if a new
// feature requires code branching in the builder, add the data field that
// drives the branch rather than threading a lambda through.

public sealed record OrbitDefinition(
	float SemiMajorAxis,
	float Period,
	float PhaseOffset = 0f,
	float Eccentricity = 0f,
	float Inclination = 0f);

public sealed record SphereVisualDefinition(
	string NodeName,
	float Radius,
	Color AlbedoColor,
	string TexturePath = "",
	int RadialSegments = 24,
	int Rings = 12,
	Vector3? Scale = null,
	float Roughness = 0.9f,
	bool Unshaded = true);

public sealed record SceneVisualDefinition(
	string NodeName,
	string ScenePath,
	Vector3? Scale = null);

public sealed record RingDefinition(
	string NodeName,
	string TexturePath,
	float Size,
	Vector3 RotationEuler);

public sealed record AsteroidBeltDefinition(
	string NodeName,
	float InnerRadius,
	float OuterRadius,
	float VerticalSpread,
	int InstanceCount,
	float MinRockSize,
	float MaxRockSize);

public sealed record JumpGateDefinition(
	string GateId,
	string DestinationSystemId,
	string ArrivalGateId,
	string DisplayName,
	string AnchorBody,
	float Distance,
	float ActivationRange = 450f,
	string ParentPath = "Star",
	string NodeName = "");

public sealed class SystemBodyDefinition
{
	public string Name { get; init; } = "";
	public string DisplayName { get; init; } = "";
	public string LabelText { get; init; } = "";
	public string StellarClass { get; init; } = "";
	public OrbitDefinition Orbit { get; init; }
	public SceneVisualDefinition SceneVisual { get; init; }
	public SphereVisualDefinition SphereVisual { get; init; }
	public RingDefinition Ring { get; init; }
	public float? GameplayRadius { get; init; }
	public Color? MapColor { get; init; }
	public IReadOnlyList<SystemBodyDefinition> Children { get; init; } = Array.Empty<SystemBodyDefinition>();
}

public sealed class StarSystemDefinition
{
	public string Id { get; init; } = "";
	public string DisplayName { get; init; } = "";
	public float PrimaryStarGravityRadius { get; init; } = 2000f;
	public IReadOnlyList<SystemBodyDefinition> Bodies { get; init; } = Array.Empty<SystemBodyDefinition>();
	public IReadOnlyList<AsteroidBeltDefinition> AsteroidBelts { get; init; } = Array.Empty<AsteroidBeltDefinition>();
	public IReadOnlyList<JumpGateDefinition> JumpGates { get; init; } = Array.Empty<JumpGateDefinition>();
}

public sealed class StarSystemBuildContext
{
	private readonly Dictionary<string, Node3D> _nodesByPath = new(StringComparer.Ordinal);

	public Node3D SceneRoot { get; }
	public Node3D StarRoot { get; }
	public Node3D StarNode { get; private set; }

	public StarSystemBuildContext(Node3D sceneRoot, Node3D starRoot)
	{
		SceneRoot = sceneRoot;
		StarRoot = starRoot;
	}

	public void RegisterNode(string path, Node3D node)
	{
		_nodesByPath[path] = node;
	}

	public void SetPrimaryStar(Node3D node)
	{
		if (StarNode == null)
			StarNode = node;
	}

	public Node3D TryGetNode(string path)
	{
		return _nodesByPath.TryGetValue(path, out Node3D node) ? node : null;
	}

	public string FindPath(Node3D node)
	{
		foreach (KeyValuePair<string, Node3D> kvp in _nodesByPath)
		{
			if (kvp.Value == node)
				return kvp.Key;
		}

		return "";
	}
}
