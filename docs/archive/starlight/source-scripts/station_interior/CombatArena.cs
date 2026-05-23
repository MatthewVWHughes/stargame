using Godot;
using System.Collections.Generic;

namespace Starlight.StationInterior;

/// <summary>
/// Combat arena lifecycle manager. Bakes nav mesh, spawns player and enemies,
/// manages waves, and coordinates AI systems.
/// </summary>
public partial class CombatArena : Node3D
{
    [Export] public int EnemiesPerWave = 1;

    private const string SpecOpsTscnPath = "res://assets/characters/NavySpecOps/NavySpecOps.tscn";

    private FPSController _player;
    private CoverManager _coverManager;
    private SquadCoordinator _squad;
    private NavigationRegion3D _navRegion;
    private Node3D _coverContainer;
    private Node3D _spawnContainer;
    private PackedScene _specOpsScene;

    private readonly List<HostileNPC> _activeEnemies = new();
    private int _wave;
    private int _totalKills;

    public override void _Ready()
    {
        _specOpsScene = ResourceLoader.Load<PackedScene>(SpecOpsTscnPath);

        _spawnContainer = GetNode<Node3D>("NPCSpawns");
        _coverContainer = GetNodeOrNull<Node3D>("CoverPoints");

        PrepareNavRegion();
        SpawnPlayer();

        _coverManager = new CoverManager();
        AddChild(_coverManager);
        _coverManager.Initialize(this);

        _squad = new SquadCoordinator();
        AddChild(_squad);
        _squad.Initialize(_coverManager);

        CallDeferred(MethodName.BakeAndSpawn);
    }

    private void PrepareNavRegion()
    {
        _navRegion = new NavigationRegion3D();
        AddChild(_navRegion);

        var toReparent = new List<Node>();
        foreach (var child in GetChildren())
        {
            if (child == _navRegion) continue;
            if (child == _coverContainer) continue;
            if (child == _spawnContainer) continue;
            if (child is Marker3D) continue;
            if (child is WorldEnvironment) continue;
            if (child is DirectionalLight3D) continue;
            if (child is FPSController) continue;

            if (child is Node3D)
                toReparent.Add(child);
        }
        foreach (var child in toReparent)
            child.Reparent(_navRegion);

        var navMesh = new NavigationMesh();
        navMesh.CellSize = 0.15f;
        navMesh.CellHeight = 0.1f;
        navMesh.AgentRadius = 0.3f;
        navMesh.AgentHeight = 1.7f;
        navMesh.AgentMaxClimb = 0.35f;
        navMesh.AgentMaxSlope = 50.0f;
        navMesh.Set("parsed_geometry_type", (int)NavigationMesh.ParsedGeometryType.StaticColliders);
        navMesh.Set("source_geometry_mode", (int)NavigationMesh.SourceGeometryMode.RootNodeChildren);
        _navRegion.NavigationMesh = navMesh;
    }

    private void BakeAndSpawn()
    {
        _navRegion.BakeNavigationMesh(false);
        GD.Print("Nav mesh baked for combat arena");

        var navMap = GetWorld3D().NavigationMap;
        NavigationServer3D.MapForceUpdate(navMap);

        var spaceState = GetWorld3D().DirectSpaceState;
        _coverManager.DetectAllPeekFlags(spaceState);

        SpawnWave();
        GD.Print("Combat arena ready.");
    }

    public override void _Process(double delta)
    {
        float dt = (float)delta;
        _squad?.Update(dt);
        PerceptionSystem.ClearExpiredNoises(Time.GetTicksMsec() / 1000.0f, 2.0f);
    }

    private void SpawnPlayer()
    {
        _player = new FPSController();
        _player.Name = "Player";
        _player.CollisionLayer = 0b0000_1000;
        _player.CollisionMask = 0b0000_0111;
        _player.FloorMaxAngle = Mathf.DegToRad(50);
        _player.FloorSnapLength = 0.3f;

        var spawn = GetNodeOrNull<Marker3D>("SpawnPoint");
        _player.Position = spawn?.Position ?? new Vector3(0, 2, 25);

        var capsule = new CollisionShape3D();
        capsule.Shape = new CapsuleShape3D { Radius = 0.3f, Height = 1.7f };
        capsule.Position = new Vector3(0, 0.85f, 0);
        _player.AddChild(capsule);

        AddChild(_player);
    }

    private void SpawnWave()
    {
        _wave++;

        int spawnCount = GetSpawnCount();
        var spawnPoints = GetSpawnPoints(spawnCount);

        for (int i = 0; i < spawnPoints.Count; i++)
        {
            var npc = CreateEnemy(spawnPoints[i].GlobalPosition, i);
            _activeEnemies.Add(npc);
        }

        GD.Print($"Wave {_wave}: spawned {spawnPoints.Count} enemies");
    }

    private int GetSpawnCount()
    {
        int available = _spawnContainer.GetChildCount();
        int wanted = Mathf.Min(EnemiesPerWave + (_wave - 1), available);
        return Mathf.Max(wanted, 1);
    }

    private List<Node3D> GetSpawnPoints(int count)
    {
        var all = new List<Node3D>();
        foreach (var child in _spawnContainer.GetChildren())
        {
            if (child is Node3D n)
                all.Add(n);
        }

        for (int i = all.Count - 1; i > 0; i--)
        {
            int j = (int)(GD.Randi() % (uint)(i + 1));
            (all[i], all[j]) = (all[j], all[i]);
        }

        return all.GetRange(0, Mathf.Min(count, all.Count));
    }

    private HostileNPC CreateEnemy(Vector3 position, int index)
    {
        var npc = new HostileNPC();
        npc.Name = $"Enemy_{_wave}_{index}";
        npc.CollisionLayer = 0b0000_0010;
        npc.CollisionMask = 0b0000_1001;
        npc.Position = position;

        // Face toward player spawn
        var toPlayer = (_player.GlobalPosition - position);
        toPlayer.Y = 0;
        if (toPlayer.LengthSquared() > 0.01f)
            npc.Rotation = new Vector3(0, Mathf.Atan2(-toPlayer.X, -toPlayer.Z), 0);

        var capsuleShape = new CollisionShape3D();
        capsuleShape.Shape = new CapsuleShape3D { Radius = 0.3f, Height = 1.7f };
        capsuleShape.Position = new Vector3(0, 0.85f, 0);
        npc.AddChild(capsuleShape);

        // Instance the character scene (has AnimationPlayer, AnimationTree, weapon)
        if (_specOpsScene != null)
        {
            var model = _specOpsScene.Instantiate<Node3D>();
            model.Name = "CharacterModel";
            model.RotateY(Mathf.Pi); // Godot -Z forward
            npc.AddChild(model);
        }
        else
        {
            // Fallback capsule
            var mesh = new MeshInstance3D();
            var capsuleMesh = new CapsuleMesh { Radius = 0.3f, Height = 1.7f };
            var mat = new StandardMaterial3D { AlbedoColor = new Color(0.8f, 0.15f, 0.1f) };
            capsuleMesh.SurfaceSetMaterial(0, mat);
            mesh.Mesh = capsuleMesh;
            mesh.Position = new Vector3(0, 0.85f, 0);
            npc.AddChild(mesh);
        }

        AddChild(npc);

        npc.Initialize(_player, _coverManager, this, _squad);
        _squad?.RegisterNPC(npc);

        var patrolPoints = new List<Vector3>();
        for (int p = 0; p < 3; p++)
        {
            float rx = (GD.Randf() - 0.5f) * 16.0f;
            float rz = (GD.Randf() - 0.5f) * 16.0f;
            var pt = position + new Vector3(rx, 0, rz);
            pt.X = Mathf.Clamp(pt.X, -28f, 28f);
            pt.Z = Mathf.Clamp(pt.Z, -28f, 28f);
            pt.Y = position.Y;
            patrolPoints.Add(pt);
        }
        npc.SetPatrolPoints(patrolPoints);

        return npc;
    }

    public void OnEnemyDied(HostileNPC npc)
    {
        _activeEnemies.Remove(npc);
        _squad?.UnregisterNPC(npc);
        _totalKills++;

        GD.Print($"Kills: {_totalKills} | Remaining: {_activeEnemies.Count}");

        if (_activeEnemies.Count == 0)
        {
            var timer = GetTree().CreateTimer(2.0);
            timer.Timeout += SpawnWave;
        }
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
