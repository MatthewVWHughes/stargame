using Godot;
using System.Collections.Generic;
using System.Text.Json;

namespace Starlight.StationInterior;

/// <summary>
/// Assembles a station interior from modular kit pieces based on a JSON layout.
/// Uses the grid-based wall-slot system defined in modular_station_kit.md.
/// [Tool] attribute allows the StationLayoutEditor plugin to detect and interact with this node.
/// </summary>
[Tool]
public partial class StationInteriorBuilder : Node3D
{
	[Export] public string LayoutPath = "res://data/station_layouts/test_station.json";

	private const float GRID = 4.0f;
	private const float WALL_T = 0.5f;
	private const string KitPiecesPath = "res://assets/station_kit/pieces/";
	private const string KitPropsPath = "res://assets/station_kit/props/";
	private const string CharSpecOpsPath = "res://assets/characters/NavySpecOps/NavySpecOps.glb";
	private const string AnimDir = "res://assets/characters/animations/";

	private const float FLOOR_HEIGHT = 3.2f; // floor-to-floor (CEIL_H + FLOOR_T)

	// Piece type → (width in cells, height in cells)
	private static readonly Dictionary<string, (int w, int h)> PieceSizes = new()
	{
		["corridor_1x1"] = (1, 1),
		["corridor_1x2"] = (1, 2),
		["room_2x2"] = (2, 2),
		["room_2x3"] = (2, 3),
		["airlock_1x1"] = (1, 1),
		["room_3x3"] = (3, 3),
		["stairwell_1x2"] = (1, 2),
	};

	// Piece type → GLB filename
	private static readonly Dictionary<string, string> PieceFiles = new()
	{
		["corridor_1x1"] = "Corridor_1x1.glb",
		["corridor_1x2"] = "Corridor_1x2.glb",
		["room_2x2"] = "Room_2x2.glb",
		["room_2x3"] = "Room_2x3.glb",
		["airlock_1x1"] = "Airlock_1x1.glb",
		["room_3x3"] = "Room_3x3.glb",
		["stairwell_1x2"] = "Stairwell_1x2.glb",
	};

	private readonly List<Node3D> _pieces = new();
	private readonly List<JsonElement> _pieceDefs = new();
	private readonly HashSet<(int x, int z, int level)> _occupiedCells = new();
	private readonly Dictionary<(int x, int z, int level), int> _cellToPieceIndex = new();
	private FPSController _player;
	private bool _hostile;
	private string _theme = "frontier";
	private string _stationName = "station";
	private NavigationRegion3D _navRegion;

	// Cached assets
	private readonly Dictionary<string, PackedScene> _pieceScenes = new();
	private readonly Dictionary<string, PackedScene> _propScenes = new();
	private PackedScene _wallBulkheadScene;
	private PackedScene _wallWindowScene;
	private PackedScene _specOpsScene;
	private AnimationLibrary _mixamoAnimLib;

	public override void _Ready()
	{
		// Skip runtime assembly in the editor — the layout editor plugin handles visualization
		if (Engine.IsEditorHint()) return;

		LoadAssets();
		BuildStation();
		SpawnPlayer();
		AddLighting();
		WireUpNPCs();
	}

	private void LoadAssets()
	{
		// Kit pieces
		foreach (var (pieceType, fileName) in PieceFiles)
		{
			var scene = ResourceLoader.Load<PackedScene>(KitPiecesPath + fileName);
			if (scene != null)
				_pieceScenes[pieceType] = scene;
			else
				GD.PrintErr($"Failed to load piece: {KitPiecesPath}{fileName}");
		}

		// Wall variants (removed — old assets deleted)
		_wallBulkheadScene = null;
		_wallWindowScene = null;

		// Character models
		_specOpsScene = ResourceLoader.Load<PackedScene>(CharSpecOpsPath);
		_mixamoAnimLib = BuildSpecOpsAnimLibrary();

		// Prop models (graceful fallback to placeholders if files don't exist)
		int propsLoaded = 0;
		foreach (var (id, info) in PropCatalog.Props)
		{
			if (info.GlbPath == null) continue;
			var propScene = ResourceLoader.Load<PackedScene>(info.GlbPath);
			if (propScene != null)
			{
				_propScenes[id] = propScene;
				propsLoaded++;
			}
		}

		GD.Print($"Kit loaded: {_pieceScenes.Count} pieces, door={_wallBulkheadScene != null}, window={_wallWindowScene != null}, props={propsLoaded}/{PropCatalog.Props.Count}");
	}

	private void BuildStation()
	{
		string json = LoadJson(LayoutPath);
		if (json == null) return;

		using var doc = JsonDocument.Parse(json);
		var root = doc.RootElement;

		_hostile = root.TryGetProperty("hostile", out var hp) && hp.GetBoolean();
		_theme = root.TryGetProperty("theme", out var tp) ? tp.GetString() : "frontier";
		_stationName = root.TryGetProperty("name", out var np) ? np.GetString() : "station";

		if (!root.TryGetProperty("grid", out var gridArray))
		{
			// Fall back to old "rooms" format
			if (root.TryGetProperty("rooms", out gridArray)) { }
			else return;
		}

		// Create NavigationRegion3D — all pieces go under this so nav mesh can be baked
		_navRegion = new NavigationRegion3D();
		_navRegion.Name = "StationNavRegion";
		AddChild(_navRegion);

		// First pass: collect all piece definitions and build occupancy map
		var pieceDefs = new List<(string type, int px, int pz, int level, int w, int h, int rotation, JsonElement def)>();

		foreach (var entry in gridArray.EnumerateArray())
		{
			string type = entry.GetProperty("piece").GetString();
			var pos = entry.TryGetProperty("pos", out var posArr) ? posArr : entry.GetProperty("grid");
			int px = pos[0].GetInt32();
			int pz = pos[1].GetInt32();
			int level = entry.TryGetProperty("level", out var lp) ? lp.GetInt32() : 0;
			int rotation = entry.TryGetProperty("rotation", out var rp) ? rp.GetInt32() : 0;

			int pw = 1, ph = 1;
			if (PieceSizes.TryGetValue(type, out var sz))
			{
				pw = sz.w; ph = sz.h;
				// Swap dimensions for 90/270 degree rotation
				if (rotation == 90 || rotation == 270)
					(pw, ph) = (ph, pw);
			}

			// Register all cells this piece occupies
			int pieceIndex = pieceDefs.Count;
			for (int dx = 0; dx < pw; dx++)
				for (int dz = 0; dz < ph; dz++)
				{
					_occupiedCells.Add((px + dx, pz + dz, level));
					_cellToPieceIndex[(px + dx, pz + dz, level)] = pieceIndex;
				}

			// Stairwell also occupies cells on level+1 (the upper exit)
			if (type == "stairwell_1x2")
			{
				for (int dx = 0; dx < pw; dx++)
					for (int dz = 0; dz < ph; dz++)
					{
						_occupiedCells.Add((px + dx, pz + dz, level + 1));
						_cellToPieceIndex[(px + dx, pz + dz, level + 1)] = pieceIndex;
					}
			}

			pieceDefs.Add((type, px, pz, level, pw, ph, rotation, entry));
		}

		// Second pass: instance pieces and resolve wall slots
		foreach (var (type, px, pz, level, w, h, rotation, def) in pieceDefs)
		{
			Node3D piece = InstancePiece(type, px, pz, level, w, h, rotation);
			if (piece == null) continue;

			// Add to scene tree early so GlobalPosition is available during wall resolution
			_navRegion.AddChild(piece);

			// Read wall overrides from JSON
			Dictionary<string, string> wallOverrides = null;
			if (def.TryGetProperty("walls", out var wallsObj))
			{
				wallOverrides = new Dictionary<string, string>();
				foreach (var prop in wallsObj.EnumerateObject())
					wallOverrides[prop.Name] = prop.Value.GetString();
			}

			// Resolve wall slots — pass piece index, level, and rotation to skip own cells
			int thisIndex = _pieces.Count;
			ResolveWallSlots(piece, type, px, pz, level, w, h, wallOverrides, thisIndex, rotation);

			// Hide ceiling/floor based on JSON flags
			bool noCeiling = def.TryGetProperty("no_ceiling", out var ncp) && ncp.GetBoolean();
			bool noFloor = def.TryGetProperty("no_floor", out var nfp) && nfp.GetBoolean();
			if (noCeiling || noFloor)
			{
				foreach (var child in GetAllChildren(piece))
				{
					string cname = child.Name.ToString();
					if (noCeiling && cname.StartsWith("Ceiling") && child is Node3D cn)
						cn.Visible = false;
					if (noFloor && cname.StartsWith("Floor") && child is Node3D fn)
						fn.Visible = false;
				}
			}

			// Generate collision from visible meshes
			GenerateCollision(piece);

			// Spawn NPCs
			if (_hostile)
			{
				int enemyCount = def.TryGetProperty("enemies", out var ep) ? ep.GetInt32() : 0;
				for (int i = 0; i < enemyCount; i++)
					SpawnHostileNPC(piece, i);
			}
			else if (def.TryGetProperty("npcs", out var npcsArr))
			{
				SpawnFriendlyNPCs(piece, npcsArr);
			}

			_pieces.Add(piece);
			_pieceDefs.Add(def);
		}

		// Spawn props (hand-placed from JSON + automatic scatter)
		SpawnAllProps();

		// Bake navigation mesh for NPC pathfinding across all pieces and props
		BakeNavMesh();

		GD.Print($"Station built: {_pieces.Count} pieces, {_occupiedCells.Count} cells, hostile={_hostile}");
	}

	private Node3D InstancePiece(string type, int px, int pz, int level, int w, int h, int rotation = 0)
	{
		if (!_pieceScenes.TryGetValue(type, out var scene))
		{
			GD.PrintErr($"No scene for piece type: {type}");
			return null;
		}

		var piece = scene.Instantiate<Node3D>();
		piece.Name = $"{type}_{px}_{pz}_L{level}";

		// Position: piece origin is at center of its footprint (w/h already swapped for rotation)
		float centerX = (px + w / 2.0f) * GRID;
		float centerZ = (pz + h / 2.0f) * GRID;
		float centerY = level * FLOOR_HEIGHT;
		piece.Position = new Vector3(centerX, centerY, -centerZ);

		if (rotation != 0)
			piece.RotateY(-Mathf.DegToRad(rotation));

		return piece;
	}

	/// <summary>
	/// For each Wall_* child in the piece, check if the adjacent cell is occupied.
	/// If yes: hide the wall and place a bulkhead (or other variant).
	/// If no: keep the solid wall.
	/// </summary>
	private static string RotateDirection(string direction, int rotation)
	{
		if (rotation == 0) return direction;
		string[] cycle = { "North", "East", "South", "West" };
		int idx = System.Array.IndexOf(cycle, direction);
		if (idx < 0) return direction;
		int steps = (rotation / 90) % 4;
		return cycle[(idx + steps) % 4];
	}

	/// <summary>
	/// Remap a wall slot index when a piece is rotated.
	/// The index encodes a spatial position along the wall edge that changes meaning after rotation.
	/// For example, East_0 on a 1x2 piece (top of east edge) becomes South_1 (right of south edge) after 90° CW.
	/// </summary>
	private static int RemapWallIndex(string localDirection, int index, int origW, int origH, int rotation)
	{
		if (rotation == 0) return index;

		// North/South walls span W slots, East/West walls span H slots
		int slotCount = localDirection is "North" or "South" ? origW : origH;
		if (slotCount <= 1) return index; // Single-slot walls don't need remapping

		// The index reverses when the rotation flips the traversal direction along the edge
		bool reverse = (rotation, localDirection) switch
		{
			(90, "East") => true,     // East→South: H slots, index reverses
			(90, "South") => true,    // South→West: W slots, index reverses
			(90, "North") => false,   // North→East: W slots, index stays
			(90, "West") => false,    // West→North: H slots, index stays

			(180, "North") => true,
			(180, "East") => true,
			(180, "South") => true,
			(180, "West") => true,

			(270, "North") => true,   // North→West: W slots, index reverses
			(270, "West") => true,    // West→South: H slots, index reverses
			(270, "East") => false,   // East→North: H slots, index stays
			(270, "South") => false,  // South→East: W slots, index stays

			_ => false,
		};

		return reverse ? slotCount - 1 - index : index;
	}

	private void ResolveWallSlots(Node3D piece, string type, int px, int pz, int level, int w, int h, Dictionary<string, string> overrides, int pieceIndex, int rotation = 0)
	{
		// Get original (pre-rotation) dimensions for index remapping
		int origW = 1, origH = 1;
		if (PieceSizes.TryGetValue(type, out var origSize))
		{
			origW = origSize.w;
			origH = origSize.h;
		}

		// Find all wall children
		foreach (var child in GetAllChildren(piece))
		{
			string name = child.Name;
			if (!name.ToString().StartsWith("Wall_")) continue;

			// Parse direction and index from name
			// Format: Wall_North, Wall_North_0, Wall_East_1, etc.
			// Godot converts Blender's ".003" suffix to "_003" — must strip both forms
			string cleanName = name.ToString();
			// Strip ".NNN" dot suffix
			int dotPos = cleanName.IndexOf('.');
			if (dotPos >= 0) cleanName = cleanName[..dotPos];
			// Strip "_NNN" underscore suffix (3+ digit Blender duplication, NOT real wall indices which are 0-2)
			cleanName = System.Text.RegularExpressions.Regex.Replace(cleanName, @"_\d{3,}$", "");
			var parts = cleanName.Split('_');
			if (parts.Length < 2) continue;

			// Extract direction — match against known directions
			string localDirection = null;
			foreach (var dir in new[] { "North", "South", "East", "West" })
			{
				if (parts[1].Equals(dir, System.StringComparison.OrdinalIgnoreCase))
				{
					localDirection = dir;
					break;
				}
			}
			if (localDirection == null) continue;

			// Remap direction for rotated pieces (Wall_North in a 90° piece faces East)
			string direction = RotateDirection(localDirection, rotation);

			// Compute the facing cell directly from the wall's world position.
			// This bypasses index parsing/remapping entirely — the mesh's physical location
			// in world space tells us exactly which grid cell it faces.
			(int fx, int fz) = ComputeFacingCellFromPosition(child, direction);

			// Check if that cell is occupied by a DIFFERENT piece
			// Stairwell's upper exit is always its local north wall, regardless of rotation
			int checkLevel = level;
			if (type == "stairwell_1x2" && localDirection == "North")
				checkLevel = level + 1;

			int neighborPiece = -1;
			bool inOccupied = _occupiedCells.Contains((fx, fz, checkLevel));
			bool inIndex = _cellToPieceIndex.TryGetValue((fx, fz, checkLevel), out neighborPiece);
			bool neighborExists = inOccupied && inIndex && neighborPiece != pieceIndex;

			GD.Print($"  [{type}@({px},{pz})r{rotation}] wall '{child.Name}' local={localDirection} rotDir={direction} facing=({fx},{fz},L{checkLevel}) neighbor={neighborExists}(p{neighborPiece})");

			// Check for JSON wall override — parse index from name for override key lookup
			int nameIndex = parts.Length >= 3 && int.TryParse(parts[2], out int nIdx) ? nIdx : 0;
			int slotCount = localDirection is "North" or "South" ? origW : origH;
			string overrideKey = slotCount > 1 ? $"{localDirection.ToLower()}_{nameIndex}" : localDirection.ToLower();
			string wallType = null;
			overrides?.TryGetValue(overrideKey, out wallType);

			if (wallType == "window")
			{
				// Use localDirection for replacement orientation — it's in piece-local space
				HideAndReplace(child, _wallWindowScene, localDirection);
			}
			else if (neighborExists)
			{
				if (child is Node3D n3d)
				{
					n3d.Visible = false;
					GD.Print($"  HIDE wall '{child.Name}' dir={direction} facing=({fx},{fz}) neighbor=piece{neighborPiece}");
				}

				bool ownsTheBulkhead = pieceIndex < neighborPiece;
				if (ownsTheBulkhead)
					PlaceBulkheadAtBoundary(child.GetParent(), direction, fx, fz, px, pz, w, h, rotation);
			}
			// else: keep solid wall (default)
		}
		GD.Print($"  Piece {type} at ({px},{pz}) rot={rotation}: processed {GetAllChildren(piece).FindAll(c => c.Name.ToString().StartsWith("Wall_")).Count} wall children");
	}

	/// <summary>
	/// Compute a wall's grid-space facing cell directly from its world position.
	/// Uses GlobalPosition (piece must be in scene tree) so it's robust against
	/// any Blender naming, nested node hierarchy, or mesh offset conventions.
	/// Returns the grid cell the wall faces (one cell beyond the piece boundary).
	/// </summary>
	private static (int facingX, int facingZ) ComputeFacingCellFromPosition(Node child, string worldDirection)
	{
		// Get the wall mesh's world position
		Vector3 worldPos = Vector3.Zero;
		if (child is MeshInstance3D mi && mi.Mesh != null)
		{
			// GlobalTransform includes all parent transforms + mesh instance position
			var globalXform = mi.GlobalTransform;
			// AABB center in local mesh space, transformed to world
			worldPos = globalXform * mi.Mesh.GetAabb().GetCenter();
		}
		else if (child is Node3D n3d)
		{
			worldPos = n3d.GlobalPosition;
		}

		// Convert world position to grid cell (nudge inward to avoid edge rounding issues)
		// Nudge AWAY from the wall direction so we land inside the piece, not outside
		float nudge = 0.3f;
		float nudgedX = worldPos.X + worldDirection switch { "East" => -nudge, "West" => nudge, _ => 0 };
		float nudgedZ = worldPos.Z + worldDirection switch { "North" => nudge, "South" => -nudge, _ => 0 };

		int cellX = Mathf.FloorToInt(nudgedX / GRID);
		int cellZ = Mathf.FloorToInt(-nudgedZ / GRID);

		// The facing cell is one step in the world direction
		return worldDirection switch
		{
			"North" => (cellX, cellZ + 1),
			"South" => (cellX, cellZ - 1),
			"East" => (cellX + 1, cellZ),
			"West" => (cellX - 1, cellZ),
			_ => (int.MinValue, int.MinValue),
		};
	}

	private (int x, int z) GetFacingCell(string direction, int index, int px, int pz, int w, int h)
	{
		// Blender North (+Y) = Godot -Z = decreasing grid Z
		// Blender South (-Y) = Godot +Z = increasing grid Z
		return direction switch
		{
			"North" => (px + index, pz + h),       // beyond north edge
			"South" => (px + index, pz - 1),        // beyond south edge
			"East" => (px + w, pz + h - 1 - index), // beyond east edge
			"West" => (px - 1, pz + h - 1 - index), // beyond west edge
			_ => (int.MinValue, int.MinValue),       // won't match anything
		};
	}

	/// <summary>
	/// Place a bulkhead at the grid boundary between two pieces, straddling both.
	/// Computes world-space position then converts to piece-local space accounting for rotation.
	/// </summary>
	private void PlaceBulkheadAtBoundary(Node parent, string direction, int facingX, int facingZ, int pieceX, int pieceZ, int pieceW, int pieceH, int rotation = 0)
	{
		if (_wallBulkheadScene == null) return;

		var bulkhead = _wallBulkheadScene.Instantiate<Node3D>();

		float pieceCenterX = (pieceX + pieceW / 2.0f) * GRID;
		float pieceCenterZ = -(pieceZ + pieceH / 2.0f) * GRID;

		// Grid boundary position in world space
		float boundaryWorldX, boundaryWorldZ;
		switch (direction)
		{
			case "North":
				boundaryWorldX = (facingX + 0.5f) * GRID;
				boundaryWorldZ = -(pieceZ + pieceH) * GRID;
				break;
			case "South":
				boundaryWorldX = (facingX + 0.5f) * GRID;
				boundaryWorldZ = -pieceZ * GRID;
				break;
			case "East":
				boundaryWorldX = (pieceX + pieceW) * GRID;
				boundaryWorldZ = -(facingZ + 0.5f) * GRID;
				break;
			case "West":
				boundaryWorldX = pieceX * GRID;
				boundaryWorldZ = -(facingZ + 0.5f) * GRID;
				break;
			default:
				boundaryWorldX = pieceCenterX;
				boundaryWorldZ = pieceCenterZ;
				break;
		}

		// Convert world offset to piece-local space by applying inverse of piece rotation
		float dx = boundaryWorldX - pieceCenterX;
		float dz = boundaryWorldZ - pieceCenterZ;
		float theta = Mathf.DegToRad(rotation);
		bulkhead.Position = new Vector3(
			dx * Mathf.Cos(theta) + dz * Mathf.Sin(theta),
			0,
			-dx * Mathf.Sin(theta) + dz * Mathf.Cos(theta)
		);

		// Bulkhead world orientation based on connection direction,
		// converted to local space by subtracting piece's rotation
		float dirRotY = direction is "North" or "South" ? Mathf.Pi / 2 : 0;
		bulkhead.Rotation = new Vector3(0, dirRotY + theta, 0);

		parent.AddChild(bulkhead);
	}

	private void HideAndReplace(Node child, PackedScene replacementScene, string direction)
	{
		if (replacementScene == null) return;

		var replacement = replacementScene.Instantiate<Node3D>();

		// Get the wall mesh's AABB center for X/Z position, keep Y=0 (floor)
		Vector3 pos = Vector3.Zero;
		if (child is MeshInstance3D meshInst && meshInst.Mesh != null)
		{
			var center = meshInst.Mesh.GetAabb().GetCenter();
			pos = new Vector3(center.X, 0, center.Z);
		}
		else if (child is Node3D n3d)
		{
			var mi = FindChildOfType<MeshInstance3D>(n3d);
			if (mi?.Mesh != null)
			{
				var center = mi.Mesh.GetAabb().GetCenter();
				pos = new Vector3(center.X, 0, center.Z);
			}
		}

		replacement.Position = pos;

		// N/S bulkheads rotate 90°, E/W bulkheads stay at 0° — confirmed by editor testing
		replacement.Rotation = direction switch
		{
			"North" => new Vector3(0, Mathf.Pi / 2, 0),
			"South" => new Vector3(0, Mathf.Pi / 2, 0),
			"East" => Vector3.Zero,
			"West" => Vector3.Zero,
			_ => Vector3.Zero,
		};

		child.GetParent().AddChild(replacement);
	}

	private void GenerateCollision(Node3D piece)
	{
		foreach (var child in GetAllChildren(piece))
		{
			if (child is MeshInstance3D meshInst && meshInst.Visible && meshInst.Mesh != null)
			{
				// Skip stair step visuals — the StairRamp mesh handles collision
				if (meshInst.Name.ToString().StartsWith("Step_")) continue;

				meshInst.CreateTrimeshCollision();
			}
		}
	}

	private void BakeNavMesh()
	{
		var navMesh = new NavigationMesh();
		navMesh.CellSize = 0.15f;
		navMesh.CellHeight = 0.1f;
		navMesh.AgentRadius = 0.3f;
		navMesh.AgentHeight = 1.7f;
		navMesh.AgentMaxClimb = 0.35f;
		navMesh.AgentMaxSlope = 50.0f;
		// Use Set() to avoid C# naming conflict between property and enum type
		navMesh.Set("parsed_geometry_type", (int)NavigationMesh.ParsedGeometryType.StaticColliders);
		navMesh.Set("source_geometry_mode", (int)NavigationMesh.SourceGeometryMode.RootNodeChildren);

		_navRegion.NavigationMesh = navMesh;
		_navRegion.BakeNavigationMesh();
		GD.Print("Navigation mesh baked for station interior");
	}

	// --- PROP SPAWNING ---

	private void SpawnAllProps()
	{
		int handPlaced = 0;
		int scattered = 0;

		for (int i = 0; i < _pieces.Count; i++)
		{
			var piece = _pieces[i];
			var def = _pieceDefs[i];

			// Track which markers have been used by hand-placed props
			var usedMarkers = new HashSet<string>();

			// Step 1: Hand-placed props from JSON
			if (def.TryGetProperty("props", out var propsArr))
			{
				foreach (var propDef in propsArr.EnumerateArray())
				{
					string propType = propDef.GetProperty("type").GetString();
					if (!PropCatalog.Props.TryGetValue(propType, out var propInfo)) continue;

					Node3D propNode;

					// Marker-based placement
					if (propDef.TryGetProperty("marker", out var markerProp))
					{
						string markerName = markerProp.GetString();
						var marker = FindChildNamed(piece, markerName) as Node3D;
						if (marker == null)
						{
							GD.PrintErr($"Prop marker '{markerName}' not found in piece '{piece.Name}'");
							continue;
						}
						propNode = CreatePropInstance(propInfo, propDef);
						propNode.Position = marker.Position;
						if (marker is Marker3D)
							propNode.Rotation = marker.Rotation;
						usedMarkers.Add(markerName);
					}
					// Offset-based placement
					else if (propDef.TryGetProperty("offset", out var offsetArr))
					{
						float ox = offsetArr[0].GetSingle();
						float oy = offsetArr[1].GetSingle();
						float oz = offsetArr[2].GetSingle();
						float rot = propDef.TryGetProperty("rotation", out var rp) ? rp.GetSingle() : 0;
						propNode = CreatePropInstance(propInfo, propDef);
						propNode.Position = new Vector3(ox, oy, oz);
						if (rot != 0) propNode.RotateY(Mathf.DegToRad(rot));
					}
					else continue;

					piece.AddChild(propNode);
					handPlaced++;
				}
			}

			// Step 2: Automatic scatter on unused markers
			bool noScatter = def.TryGetProperty("no_scatter", out var nsp) && nsp.GetBoolean();
			if (!noScatter)
			{
				string purpose = def.TryGetProperty("purpose", out var pp) ? pp.GetString() : "generic";
				scattered += ScatterPropsOnPiece(piece, purpose, usedMarkers, i);
			}
		}

		GD.Print($"Props spawned: {handPlaced} hand-placed, {scattered} scattered");
	}

	private Node3D CreatePropInstance(PropCatalog.PropInfo propInfo, JsonElement propDef = default)
	{
		var propNode = new Node3D();
		propNode.Name = $"Prop_{propInfo.Id}";

		// Try loading GLB, fall back to runtime placeholder
		MeshInstance3D meshInst;
		if (propInfo.GlbPath != null && _propScenes.TryGetValue(propInfo.Id, out var scene))
		{
			var model = scene.Instantiate<Node3D>();
			model.Name = "Model";
			propNode.AddChild(model);
			meshInst = FindChildOfType<MeshInstance3D>(model);
		}
		else
		{
			meshInst = PropCatalog.CreatePlaceholderMesh(propInfo);
			propNode.AddChild(meshInst);
		}

		// Collision based on tier — always primitive shapes, never trimesh
		bool interactable = propDef.ValueKind != JsonValueKind.Undefined &&
			propDef.TryGetProperty("interactable", out var ip) && ip.GetBoolean();
		string interactLabel = propDef.ValueKind != JsonValueKind.Undefined &&
			propDef.TryGetProperty("interact_label", out var ilp) ? ilp.GetString() : propInfo.InteractLabel;

		if (propInfo.Collision == PropCatalog.CollisionTier.Blocking)
		{
			var body = new StaticBody3D();
			body.CollisionLayer = 0b0000_0001; // Layer 1 (environment)
			body.CollisionMask = 0;
			body.AddChild(CreatePrimitiveCollision(propInfo));
			propNode.AddChild(body);
		}
		else if (propInfo.Collision == PropCatalog.CollisionTier.InteractableOnly || interactable)
		{
			var area = new InteractableArea3D();
			area.PropInfo = propInfo;
			area.InteractLabel = interactLabel ?? propInfo.InteractLabel ?? "Interact";
			area.CollisionLayer = 0b0000_0100; // Layer 3 (interactable)
			area.CollisionMask = 0;
			area.AddChild(CreatePrimitiveCollision(propInfo));
			propNode.AddChild(area);
		}

		return propNode;
	}

	private static CollisionShape3D CreatePrimitiveCollision(PropCatalog.PropInfo propInfo)
	{
		Shape3D shape;
		if (propInfo.Shape == PropCatalog.PrimitiveShape.Cylinder)
		{
			shape = new CylinderShape3D
			{
				Radius = propInfo.Size.X / 2.0f,
				Height = propInfo.Size.Y,
			};
		}
		else
		{
			shape = new BoxShape3D { Size = propInfo.Size };
		}

		return new CollisionShape3D
		{
			Shape = shape,
			Position = new Vector3(0, propInfo.Size.Y / 2.0f, 0),
		};
	}

	private int ScatterPropsOnPiece(Node3D piece, string purpose, HashSet<string> usedMarkers, int pieceIndex)
	{
		float density = PropCatalog.ThemeDensity.GetValueOrDefault(_theme, 0.7f);

		// Deterministic seed from station name + piece index
		uint seed = (uint)(_stationName.GetHashCode() ^ (pieceIndex * 7919));
		var rng = new RandomNumberGenerator();
		rng.Seed = seed;

		// Extract piece dimensions from the name (e.g., "corridor_1x1_2_1_L0")
		string pieceType = null;
		foreach (var key in PieceSizes.Keys)
			if (piece.Name.ToString().StartsWith(key)) { pieceType = key; break; }
		if (pieceType == null || !PieceSizes.TryGetValue(pieceType, out var sz)) return 0;

		// Interior bounds in local space (inset from walls)
		float halfW = sz.w * GRID / 2.0f;
		float halfH = sz.h * GRID / 2.0f;
		float inset = 0.8f;
		float minX = -halfW + inset;
		float maxX = halfW - inset;
		float minZ = -halfH + inset;
		float maxZ = halfH - inset;

		// Track placed positions to avoid overlap
		var placedPositions = new List<(float x, float z, float radius)>();
		foreach (var child in piece.GetChildren())
		{
			if (child is Node3D n3d && child.Name.ToString().StartsWith("Prop_"))
				placedPositions.Add((n3d.Position.X, n3d.Position.Z, 0.5f));
		}

		int placed = 0;

		// --- PASS 0: Build doorway exclusion zones ---
		var exclusionZones = new List<(float cx, float cz, float hw, float hz)>();
		foreach (var child in GetAllChildren(piece))
		{
			if (child is not Node3D wallNode) continue;
			if (!child.Name.ToString().StartsWith("Wall_")) continue;
			if (wallNode.Visible) continue; // Visible = solid wall, not a doorway

			// Hidden wall = doorway — build exclusion zone 1.5m deep into the room
			string dir = null;
			foreach (var d in new[] { "North", "South", "East", "West" })
				if (child.Name.ToString().Contains(d, System.StringComparison.OrdinalIgnoreCase))
				{ dir = d; break; }
			if (dir == null) continue;

			Vector3 wPos = wallNode.Position;
			if (dir is "North" or "South")
				exclusionZones.Add((wPos.X, wPos.Z, 1.2f, 1.5f));
			else
				exclusionZones.Add((wPos.X, wPos.Z, 1.5f, 1.2f));
		}

		// --- PASS 1: Furniture groups ---
		var groups = PropCatalog.GetGroupsForPurpose(purpose);
		if (groups.Count > 0)
		{
			int area = sz.w * sz.h;
			int maxGroups = Mathf.Min(area / 3, 2);
			for (int g = 0; g < maxGroups; g++)
			{
				var group = WeightedRandomPick(groups, rng);
				bool groupPlaced = false;
				for (int attempt = 0; attempt < 20 && !groupPlaced; attempt++)
				{
					float cx = rng.RandfRange(minX + 1.2f, maxX - 1.2f);
					float cz = rng.RandfRange(minZ + 1.2f, maxZ - 1.2f);
					float groupRot = rng.RandfRange(0, Mathf.Tau);

					// Check all slots fit and don't overlap
					bool allFit = true;
					var slotPositions = new List<(float x, float z, float r)>();
					foreach (var slot in group.Slots)
					{
						float sx = cx + slot.Offset.X * Mathf.Cos(groupRot) - slot.Offset.Z * Mathf.Sin(groupRot);
						float sz2 = cz + slot.Offset.X * Mathf.Sin(groupRot) + slot.Offset.Z * Mathf.Cos(groupRot);

						if (sx < minX || sx > maxX || sz2 < minZ || sz2 > maxZ) { allFit = false; break; }
						if (IsInExclusionZone(sx, sz2, 0.3f, exclusionZones)) { allFit = false; break; }
						if (IsOverlapping(sx, sz2, 0.4f, placedPositions)) { allFit = false; break; }

						slotPositions.Add((sx, sz2, 0.4f));
					}
					if (!allFit) continue;

					// Place group
					for (int s = 0; s < group.Slots.Length; s++)
					{
						var slot = group.Slots[s];
						if (!PropCatalog.Props.TryGetValue(slot.PropId, out var propInfo)) continue;
						var propNode = CreatePropInstance(propInfo);
						propNode.Position = new Vector3(slotPositions[s].x, slot.Offset.Y, slotPositions[s].z);
						propNode.RotateY(slot.YRotation + groupRot);
						piece.AddChild(propNode);
						placedPositions.Add(slotPositions[s]);
						placed++;
					}
					groupPlaced = true;
				}
			}
		}

		// --- PASS 2: Wall props on visible (solid) wall segments ---
		var wallProps = PropCatalog.GetWallPropsForPurpose(purpose);
		if (wallProps.Count > 0)
		{
			foreach (var child in GetAllChildren(piece))
			{
				if (child is not Node3D wallNode) continue;
				if (!child.Name.ToString().StartsWith("Wall_")) continue;
				if (!wallNode.Visible) continue; // Only place on solid walls

				if (rng.Randf() > density * 0.5f) continue; // Wall props sparser

				string dir = null;
				foreach (var d in new[] { "North", "South", "East", "West" })
					if (child.Name.ToString().Contains(d, System.StringComparison.OrdinalIgnoreCase))
					{ dir = d; break; }
				if (dir == null) continue;

				var selected = WeightedRandomPick(wallProps, rng);
				var propNode = CreatePropInstance(selected);

				float mountH = selected.Category == PropCatalog.PropCategory.Tech ? 1.4f : 1.0f;
				Vector3 wPos = wallNode.Position;
				// Offset inward from wall by half prop depth
				float offset = selected.Size.Z / 2.0f + 0.05f;
				Vector3 inward = dir switch
				{
					"North" => new Vector3(0, 0, 1),
					"South" => new Vector3(0, 0, -1),
					"East" => new Vector3(-1, 0, 0),
					"West" => new Vector3(1, 0, 0),
					_ => Vector3.Zero,
				};
				propNode.Position = new Vector3(wPos.X, mountH, wPos.Z) + inward * offset;
				propNode.Rotation = new Vector3(0, dir switch
				{
					"North" => 0,
					"South" => Mathf.Pi,
					"East" => -Mathf.Pi / 2f,
					"West" => Mathf.Pi / 2f,
					_ => 0,
				}, 0);

				piece.AddChild(propNode);
				placed++;
			}
		}

		// --- PASS 3: Floor fill ---
		var floorProps = PropCatalog.GetFloorPropsForPurpose(purpose);
		if (floorProps.Count > 0)
		{
			int area = sz.w * sz.h;
			int maxFloorProps = Mathf.RoundToInt(area * density * 1.0f);
			int floorPlaced = 0;
			int attempts = 0;

			while (floorPlaced < maxFloorProps && attempts < maxFloorProps * 5)
			{
				attempts++;
				float x = rng.RandfRange(minX, maxX);
				float z = rng.RandfRange(minZ, maxZ);

				if (IsInExclusionZone(x, z, 0.3f, exclusionZones)) continue;

				var selected = WeightedRandomPick(floorProps, rng);
				float propRadius = Mathf.Max(selected.Size.X, selected.Size.Z) / 2.0f + 0.15f;

				if (IsOverlapping(x, z, propRadius, placedPositions)) continue;

				var propNode = CreatePropInstance(selected);
				propNode.Position = new Vector3(x, 0, z);
				propNode.RotateY(rng.RandfRange(0, Mathf.Tau));
				piece.AddChild(propNode);
				placedPositions.Add((x, z, propRadius));
				floorPlaced++;
				placed++;
			}
		}

		return placed;
	}

	private static bool IsInExclusionZone(float x, float z, float radius,
		List<(float cx, float cz, float hw, float hz)> zones)
	{
		foreach (var (cx, cz, hw, hz) in zones)
		{
			if (Mathf.Abs(x - cx) < hw + radius && Mathf.Abs(z - cz) < hz + radius)
				return true;
		}
		return false;
	}

	private static bool IsOverlapping(float x, float z, float radius,
		List<(float x, float z, float radius)> positions)
	{
		foreach (var (px, pz, pr) in positions)
		{
			float dx = x - px, dz2 = z - pz;
			if (dx * dx + dz2 * dz2 < (radius + pr) * (radius + pr))
				return true;
		}
		return false;
	}

	private static T WeightedRandomPick<T>(List<T> pool, RandomNumberGenerator rng) where T : class
	{
		float totalWeight = 0;
		foreach (var p in pool)
		{
			float w = p switch
			{
				PropCatalog.PropInfo pi => pi.Weight,
				PropCatalog.FurnitureGroup fg => fg.Weight,
				_ => 1.0f,
			};
			totalWeight += w;
		}
		float roll = rng.Randf() * totalWeight;
		float cumulative = 0;
		foreach (var p in pool)
		{
			float w = p switch
			{
				PropCatalog.PropInfo pi => pi.Weight,
				PropCatalog.FurnitureGroup fg => fg.Weight,
				_ => 1.0f,
			};
			cumulative += w;
			if (roll <= cumulative) return p;
		}
		return pool[pool.Count - 1];
	}

	// --- NPC SPAWNING ---

	private void SpawnFriendlyNPCs(Node3D piece, JsonElement npcsArr)
	{
		var waypoints = FindChildByPrefix(piece, "Marker_Waypoint");
		int wpIndex = 0;

		foreach (var npcDef in npcsArr.EnumerateArray())
		{
			bool interactable = npcDef.TryGetProperty("interactable", out var ip) && ip.GetBoolean();
			string service = npcDef.TryGetProperty("service", out var sp) ? sp.GetString() : "";

			var npc = new AmbientNPC();
			npc.Name = interactable ? $"NPC_{service}" : $"NPC_ambient_{wpIndex}";
			npc.Interactable = interactable;
			npc.InteractLabel = service switch { "trader" => "Trade", "bartender" => "Talk", _ => "Interact" };
			npc.CollisionLayer = interactable ? 0b0000_0100u : 0b0000_0010u;
			npc.CollisionMask = 1;

			AddCharacterVisual(npc);
			AddCapsuleCollision(npc);

			// Position at a waypoint
			var wp = FindNthChildByPrefix(piece, "Marker_Waypoint", wpIndex) as Node3D;
			if (wp != null) npc.Position = wp.Position;
			wpIndex++;

			// Give waypoint container reference
			var wpContainer = FindChildNamed(piece, "Marker_Waypoint_01")?.GetParent() as Node3D;
			if (wpContainer != null) npc.SetWaypoints(wpContainer);

			piece.AddChild(npc);
		}
	}

	private void SpawnHostileNPC(Node3D piece, int index)
	{
		var npc = new HostileNPC();
		npc.Name = $"Enemy_{index}";
		npc.CollisionLayer = 0b0000_0010;
		npc.CollisionMask = 0b0000_1001;

		var meshInst = AddSpecOpsVisual(npc);
		npc.SetMeshInstance(meshInst);
		AddCapsuleCollision(npc);

		var cp = FindNthChildByPrefix(piece, "Marker_CoverPoint", index) as Node3D;
		if (cp != null) npc.Position = cp.Position;
		else
		{
			var wp = FindNthChildByPrefix(piece, "Marker_Waypoint", index) as Node3D;
			if (wp != null) npc.Position = wp.Position;
		}

		var wpParent = FindChildNamed(piece, "Marker_Waypoint_01")?.GetParent() as Node3D;
		if (wpParent != null) npc.SetWaypoints(wpParent);

		var cpParent = FindChildNamed(piece, "Marker_CoverPoint_01")?.GetParent() as Node3D;
		if (cpParent != null) npc.SetCoverPoints(cpParent);

		piece.AddChild(npc);
	}

	private void SpawnPlayer()
	{
		_player = new FPSController();
		_player.Name = "Player";
		_player.CollisionLayer = 0b0000_1000;
		_player.CollisionMask = 0b0000_0111;

		// Spawn at the airlock — the station entry point
		Node3D spawnPiece = null;
		foreach (var piece in _pieces)
		{
			if (piece.Name.ToString().StartsWith("airlock_1x1"))
			{
				spawnPiece = piece;
				break;
			}
		}
		spawnPiece ??= _pieces.Count > 0 ? _pieces[0] : null;

		if (spawnPiece != null)
			_player.Position = spawnPiece.Position + new Vector3(0, 0.1f, 0);

		AddCapsuleCollision(_player);
		AddChild(_player);
	}

	private void WireUpNPCs()
	{
		foreach (var piece in _pieces)
		{
			foreach (var child in piece.GetChildren())
			{
				if (child is AmbientNPC ambient) ambient.SetPlayer(_player);
				else if (child is HostileNPC hostile) hostile.SetPlayer(_player);
			}
		}
	}

	private void AddLighting()
	{
		var env = new WorldEnvironment();
		var envRes = new Godot.Environment();
		envRes.BackgroundMode = Godot.Environment.BGMode.Color;
		envRes.BackgroundColor = new Color(0.05f, 0.05f, 0.08f);
		envRes.AmbientLightSource = Godot.Environment.AmbientSource.Color;
		envRes.AmbientLightColor = new Color(0.15f, 0.15f, 0.2f);
		envRes.AmbientLightEnergy = 0.3f;
		env.Environment = envRes;
		AddChild(env);

		// Add a light at each piece's light markers, or one per piece as fallback
		foreach (var piece in _pieces)
		{
			bool foundLight = false;
			foreach (var child in GetAllChildren(piece))
			{
				if (child.Name.ToString().StartsWith("Marker_Prop_Ceiling") || child.Name.ToString().StartsWith("Light_"))
				{
					var light = new OmniLight3D();
					light.Position = (child as Node3D)?.Position ?? Vector3.Zero;
					light.OmniRange = 5.0f;
					light.LightEnergy = _hostile ? 0.8f : 1.5f;
					light.LightColor = _hostile ? new Color(0.7f, 0.4f, 0.3f) : new Color(0.9f, 0.85f, 0.7f);
					light.ShadowEnabled = true;
					piece.AddChild(light);
					foundLight = true;
				}
			}
			// Fallback: one light at piece center, ceiling height
			if (!foundLight)
			{
				var light = new OmniLight3D();
				light.Position = new Vector3(0, 2.7f, 0);
				light.OmniRange = 5.0f;
				light.LightEnergy = _hostile ? 0.8f : 1.5f;
				light.LightColor = _hostile ? new Color(0.7f, 0.4f, 0.3f) : new Color(0.9f, 0.85f, 0.7f);
				light.ShadowEnabled = true;
				piece.AddChild(light);
			}
		}
	}

	// --- CHARACTER VISUALS ---

	private MeshInstance3D AddCharacterVisual(Node3D parent)
	{
		return AddSpecOpsVisual(parent);
	}

	private MeshInstance3D AddCapsuleFallback(Node3D parent, Color color)
	{
		var mesh = new CapsuleMesh { Radius = 0.3f, Height = 1.7f };
		var mat = new StandardMaterial3D { AlbedoColor = color };
		mesh.SurfaceSetMaterial(0, mat);
		var inst = new MeshInstance3D { Mesh = mesh, Position = new Vector3(0, 0.85f, 0) };
		parent.AddChild(inst);
		return inst;
	}

	private void AddCapsuleCollision(CharacterBody3D parent)
	{
		var shape = new CollisionShape3D();
		var capsule = new CapsuleShape3D { Radius = 0.3f, Height = 1.7f };
		shape.Shape = capsule;
		shape.Position = new Vector3(0, 0.85f, 0);
		parent.AddChild(shape);
	}

	// --- SPEC OPS CHARACTER ---

	private MeshInstance3D AddSpecOpsVisual(Node3D parent)
	{
		if (_specOpsScene == null)
			return AddCapsuleFallback(parent, new Color(0.2f, 0.3f, 0.5f));

		var model = _specOpsScene.Instantiate<Node3D>();
		model.Name = "CharacterModel";
		model.RotateY(Mathf.Pi);
		parent.AddChild(model);

		var animPlayer = FindChildOfType<AnimationPlayer>(model);
		if (animPlayer == null)
		{
			animPlayer = new AnimationPlayer();
			animPlayer.Name = "AnimationPlayer";
			model.AddChild(animPlayer);
		}

		// Add ual1-compatible library so NPC animation names work
		if (_mixamoAnimLib != null && !animPlayer.HasAnimationLibrary("ual1"))
			animPlayer.AddAnimationLibrary("ual1", _mixamoAnimLib);

		return FindChildOfType<MeshInstance3D>(model);
	}

	private AnimationLibrary BuildSpecOpsAnimLibrary()
	{
		var dir = DirAccess.Open(AnimDir);
		if (dir == null) return null;

		var lib = new AnimationLibrary();
		dir.ListDirBegin();
		string fileName = dir.GetNext();
		while (!string.IsNullOrEmpty(fileName))
		{
			if (!dir.CurrentIsDir() && fileName.EndsWith(".glb"))
			{
				string animName = fileName.Replace(".glb", "");
				var scene = ResourceLoader.Load<PackedScene>(AnimDir + fileName);
				if (scene != null)
				{
					var inst = scene.Instantiate<Node3D>();
					var ap = FindChildOfType<AnimationPlayer>(inst);
					if (ap != null)
					{
						foreach (var libName in ap.GetAnimationLibraryList())
						{
							var srcLib = ap.GetAnimationLibrary(libName);
							foreach (var srcAnimName in srcLib.GetAnimationList())
							{
								var anim = srcLib.GetAnimation(srcAnimName);
								anim.LoopMode = Animation.LoopModeEnum.Linear;
								if (!lib.HasAnimation(animName))
									lib.AddAnimation(animName, anim);
							}
						}
					}
					inst.QueueFree();
				}
			}
			fileName = dir.GetNext();
		}
		dir.ListDirEnd();

		GD.Print($"Loaded {lib.GetAnimationList().Count} spec ops animations");
		return lib.GetAnimationList().Count > 0 ? lib : null;
	}

	// --- HELPERS ---

	private static List<Node> GetAllChildren(Node parent)
	{
		var result = new List<Node>();
		foreach (var child in parent.GetChildren())
		{
			result.Add(child);
			result.AddRange(GetAllChildren(child));
		}
		return result;
	}

	private static Node FindChildNamed(Node parent, string name)
	{
		foreach (var child in GetAllChildren(parent))
			if (child.Name == name) return child;
		return null;
	}

	private static Node FindChildByPrefix(Node parent, string prefix)
	{
		foreach (var child in GetAllChildren(parent))
			if (child.Name.ToString().StartsWith(prefix)) return child;
		return null;
	}

	private static Node FindNthChildByPrefix(Node parent, string prefix, int n)
	{
		int count = 0;
		foreach (var child in GetAllChildren(parent))
		{
			if (child.Name.ToString().StartsWith(prefix))
			{
				if (count == n) return child;
				count++;
			}
		}
		return null;
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

	private string LoadJson(string path)
	{
		if (!FileAccess.FileExists(path))
		{
			GD.PrintErr($"Layout file not found: {path}");
			return null;
		}
		using var file = FileAccess.Open(path, FileAccess.ModeFlags.Read);
		return file.GetAsText();
	}
}
