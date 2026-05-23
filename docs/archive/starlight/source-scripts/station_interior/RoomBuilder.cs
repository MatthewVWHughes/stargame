using Godot;
using System.Collections.Generic;

namespace Starlight.StationInterior;

/// <summary>
/// Builds a single grey-box room from flat planes.
/// Walls are quads with doorway gaps. Floor and ceiling are flat planes.
/// </summary>
public static class RoomBuilder
{
    public const float RoomSize = 8.0f;    // meters per side
    public const float RoomHeight = 3.0f;
    public const float DoorWidth = 2.0f;
    public const float DoorHeight = 2.5f;
    public const float WallThickness = 0.1f;

    /// <summary>
    /// Build a room node with floor, ceiling, walls, and door openings.
    /// </summary>
    /// <param name="doors">Which sides have doors: "north", "south", "east", "west"</param>
    /// <param name="roomName">Name for the root node</param>
    public static Node3D BuildRoom(string roomName, HashSet<string> doors)
    {
        var root = new Node3D();
        root.Name = roomName;

        // Floor
        root.AddChild(CreatePlane("Floor",
            new Vector3(RoomSize, WallThickness, RoomSize),
            new Vector3(0, 0, 0),
            new Color(0.3f, 0.3f, 0.35f)));

        // Ceiling
        root.AddChild(CreatePlane("Ceiling",
            new Vector3(RoomSize, WallThickness, RoomSize),
            new Vector3(0, RoomHeight, 0),
            new Color(0.25f, 0.25f, 0.28f)));

        // Walls — each side, with door cutout if needed
        BuildWall(root, "North", new Vector3(0, 0, -RoomSize / 2), Vector3.Zero, doors.Contains("north"));
        BuildWall(root, "South", new Vector3(0, 0, RoomSize / 2), new Vector3(0, Mathf.Pi, 0), doors.Contains("south"));
        BuildWall(root, "East", new Vector3(RoomSize / 2, 0, 0), new Vector3(0, -Mathf.Pi / 2, 0), doors.Contains("east"));
        BuildWall(root, "West", new Vector3(-RoomSize / 2, 0, 0), new Vector3(0, Mathf.Pi / 2, 0), doors.Contains("west"));

        // Static body for collision
        var body = new StaticBody3D();
        body.Name = "RoomCollision";
        body.CollisionLayer = 1; // layer 1: environment
        body.CollisionMask = 0;

        // Floor collision
        var floorShape = new CollisionShape3D();
        var floorBox = new BoxShape3D();
        floorBox.Size = new Vector3(RoomSize, WallThickness, RoomSize);
        floorShape.Shape = floorBox;
        floorShape.Position = new Vector3(0, -WallThickness / 2, 0);
        body.AddChild(floorShape);

        root.AddChild(body);

        // Navigation region
        var navRegion = new NavigationRegion3D();
        var navMesh = new NavigationMesh();
        navMesh.AgentRadius = 0.3f;
        navMesh.AgentHeight = 1.8f;

        // Bake a simple floor polygon for navigation
        var verts = new Vector3[]
        {
            new(-RoomSize / 2, 0.01f, -RoomSize / 2),
            new(RoomSize / 2, 0.01f, -RoomSize / 2),
            new(RoomSize / 2, 0.01f, RoomSize / 2),
            new(-RoomSize / 2, 0.01f, RoomSize / 2),
        };
        navMesh.Vertices = verts;
        navMesh.AddPolygon(new int[] { 0, 1, 2, 3 });
        navRegion.NavigationMesh = navMesh;
        root.AddChild(navRegion);

        return root;
    }

    private static void BuildWall(Node3D parent, string name, Vector3 position, Vector3 rotation, bool hasDoor)
    {
        if (!hasDoor)
        {
            // Full wall — single plane
            var wall = CreatePlane($"Wall_{name}",
                new Vector3(RoomSize, RoomHeight, WallThickness),
                position + new Vector3(0, RoomHeight / 2, 0),
                new Color(0.35f, 0.35f, 0.38f));
            wall.Rotation = rotation;
            parent.AddChild(wall);

            // Wall collision
            AddWallCollision(parent, $"WallCol_{name}",
                new Vector3(RoomSize, RoomHeight, WallThickness),
                position + new Vector3(0, RoomHeight / 2, 0),
                rotation);
        }
        else
        {
            // Wall with door opening — left panel, right panel, and header above door
            float sideWidth = (RoomSize - DoorWidth) / 2;
            float headerHeight = RoomHeight - DoorHeight;

            // Left panel
            var left = CreatePlane($"Wall_{name}_L",
                new Vector3(sideWidth, RoomHeight, WallThickness),
                new Vector3(-(DoorWidth / 2 + sideWidth / 2), RoomHeight / 2, 0),
                new Color(0.35f, 0.35f, 0.38f));
            var leftContainer = new Node3D();
            leftContainer.Position = position;
            leftContainer.Rotation = rotation;
            leftContainer.AddChild(left);
            parent.AddChild(leftContainer);

            // Right panel
            var right = CreatePlane($"Wall_{name}_R",
                new Vector3(sideWidth, RoomHeight, WallThickness),
                new Vector3(DoorWidth / 2 + sideWidth / 2, RoomHeight / 2, 0),
                new Color(0.35f, 0.35f, 0.38f));
            var rightContainer = new Node3D();
            rightContainer.Position = position;
            rightContainer.Rotation = rotation;
            rightContainer.AddChild(right);
            parent.AddChild(rightContainer);

            // Header above door
            if (headerHeight > 0.01f)
            {
                var header = CreatePlane($"Wall_{name}_H",
                    new Vector3(DoorWidth, headerHeight, WallThickness),
                    new Vector3(0, DoorHeight + headerHeight / 2, 0),
                    new Color(0.35f, 0.35f, 0.38f));
                var headerContainer = new Node3D();
                headerContainer.Position = position;
                headerContainer.Rotation = rotation;
                headerContainer.AddChild(header);
                parent.AddChild(headerContainer);
            }

            // Wall collision for side panels
            AddWallCollision(parent, $"WallCol_{name}_L",
                new Vector3(sideWidth, RoomHeight, WallThickness),
                position + new Vector3(0, RoomHeight / 2, 0), rotation,
                new Vector3(-(DoorWidth / 2 + sideWidth / 2), 0, 0));

            AddWallCollision(parent, $"WallCol_{name}_R",
                new Vector3(sideWidth, RoomHeight, WallThickness),
                position + new Vector3(0, RoomHeight / 2, 0), rotation,
                new Vector3(DoorWidth / 2 + sideWidth / 2, 0, 0));
        }
    }

    private static MeshInstance3D CreatePlane(string name, Vector3 size, Vector3 position, Color color)
    {
        var mesh = new BoxMesh();
        mesh.Size = size;

        var mat = new StandardMaterial3D();
        mat.AlbedoColor = color;
        mat.Roughness = 0.9f;
        mesh.SurfaceSetMaterial(0, mat);

        var instance = new MeshInstance3D();
        instance.Name = name;
        instance.Mesh = mesh;
        instance.Position = position;

        return instance;
    }

    private static void AddWallCollision(Node3D parent, string name, Vector3 size, Vector3 position, Vector3 rotation, Vector3 localOffset = default)
    {
        // Find or create the room's static body
        var body = parent.GetNodeOrNull<StaticBody3D>("RoomCollision");
        if (body == null) return;

        var shape = new CollisionShape3D();
        shape.Name = name;
        var box = new BoxShape3D();
        box.Size = size;
        shape.Shape = box;

        // Apply rotation to the local offset
        var basis = Basis.Identity;
        basis = basis.Rotated(Vector3.Up, rotation.Y);
        shape.Position = position + basis * localOffset;
        shape.Rotation = rotation;

        body.AddChild(shape);
    }

    /// <summary>
    /// Add waypoint markers to a room for NPC pathing.
    /// </summary>
    public static void AddWaypoints(Node3D room, int count = 4)
    {
        var wpRoot = new Node3D();
        wpRoot.Name = "Waypoints";

        float margin = 1.5f;
        float half = RoomSize / 2 - margin;

        for (int i = 0; i < count; i++)
        {
            var wp = new Marker3D();
            wp.Name = $"WP_{i:D2}";
            // Distribute waypoints around the room
            float angle = (Mathf.Pi * 2 * i) / count;
            float radius = half * 0.7f;
            wp.Position = new Vector3(
                Mathf.Cos(angle) * radius,
                0.1f,
                Mathf.Sin(angle) * radius
            );
            wpRoot.AddChild(wp);
        }

        room.AddChild(wpRoot);
    }

    /// <summary>
    /// Add cover point markers for combat AI.
    /// </summary>
    public static void AddCoverPoints(Node3D room, int count = 3)
    {
        var cpRoot = new Node3D();
        cpRoot.Name = "CoverPoints";

        float margin = 1.0f;
        float half = RoomSize / 2 - margin;

        for (int i = 0; i < count; i++)
        {
            var cp = new Marker3D();
            cp.Name = $"Cover_{i:D2}";
            float angle = (Mathf.Pi * 2 * i) / count + 0.5f; // offset from waypoints
            float radius = half * 0.85f; // near walls
            cp.Position = new Vector3(
                Mathf.Cos(angle) * radius,
                0.1f,
                Mathf.Sin(angle) * radius
            );
            cpRoot.AddChild(cp);
        }

        room.AddChild(cpRoot);
    }
}
