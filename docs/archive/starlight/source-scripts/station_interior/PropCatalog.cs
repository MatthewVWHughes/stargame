using Godot;
using System.Collections.Generic;
using System.Linq;

namespace Starlight.StationInterior;

/// <summary>
/// Static catalog of all prop types available for station interiors.
/// Defines collision, snap, category, and scatter metadata for each prop.
/// Props without GLB files are generated as runtime primitives (placeholders).
/// </summary>
public static class PropCatalog
{
	public enum CollisionTier { Blocking, InteractableOnly, None }
	public enum SnapType { Floor, Wall, Ceiling, Freeform }
	public enum PropCategory { Structural, Furniture, Cargo, Tech, Decoration, Clutter }
	public enum PrimitiveShape { Box, Cylinder }

	public record PropInfo(
		string Id,
		string Label,
		string GlbPath,            // null = generate from primitive
		PrimitiveShape Shape,
		Vector3 Size,              // Bounding box (width, depth, height)
		CollisionTier Collision,
		SnapType Snap,
		PropCategory Category,
		string InteractLabel,      // null = not interactable
		float Weight,              // Scatter frequency (higher = more common)
		string[] Purposes,         // Room purposes this prop appears in
		Color PlaceholderColor     // Color for generated primitive
	);

	public static readonly Dictionary<string, PropInfo> Props = new()
	{
		// --- Floor props (blocking) — from purchased sci-fi kit ---
		["barrel"] = new PropInfo(
			"barrel", "Barrel", "res://assets/station_kit/props/Barrel/Barrel.glb",
			PrimitiveShape.Cylinder, new Vector3(0.58f, 0.90f, 0.58f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Cargo,
			null, 1.5f,
			new[] { "cargo_bay", "engineering", "generic" },
			new Color(0.45f, 0.35f, 0.25f)
		),
		["container"] = new PropInfo(
			"container", "Container", "res://assets/station_kit/props/Container/Container.glb",
			PrimitiveShape.Box, new Vector3(1.0f, 0.56f, 0.50f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Cargo,
			null, 1.5f,
			new[] { "cargo_bay", "engineering", "generic" },
			new Color(0.5f, 0.4f, 0.3f)
		),
		["crate_large"] = new PropInfo(
			"crate_large", "Large Crate", "res://assets/station_kit/props/Crate_Large/Crate_Large.glb",
			PrimitiveShape.Box, new Vector3(1.84f, 3.95f, 1.84f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Cargo,
			null, 0.5f,
			new[] { "cargo_bay" },
			new Color(0.55f, 0.42f, 0.3f)
		),
		["chair"] = new PropInfo(
			"chair", "Chair", "res://assets/station_kit/props/Chair/Chair.glb",
			PrimitiveShape.Box, new Vector3(0.72f, 1.30f, 0.59f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Furniture,
			null, 1.5f,
			new[] { "bar", "quarters", "bridge", "generic" },
			new Color(0.5f, 0.45f, 0.4f)
		),
		["desk"] = new PropInfo(
			"desk", "Desk", "res://assets/station_kit/props/Desk/Desk.glb",
			PrimitiveShape.Box, new Vector3(1.0f, 0.87f, 1.96f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Furniture,
			null, 1.0f,
			new[] { "bridge", "quarters", "engineering" },
			new Color(0.4f, 0.4f, 0.45f)
		),
		["cabinet"] = new PropInfo(
			"cabinet", "Cabinet", "res://assets/station_kit/props/Cabinet/Cabinet.glb",
			PrimitiveShape.Box, new Vector3(0.42f, 1.90f, 0.64f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Structural,
			null, 0.8f,
			new[] { "cargo_bay", "engineering", "quarters", "med_bay" },
			new Color(0.4f, 0.4f, 0.45f)
		),
		["pallet"] = new PropInfo(
			"pallet", "Pallet", "res://assets/station_kit/props/Pallet/Pallet.glb",
			PrimitiveShape.Box, new Vector3(1.81f, 0.26f, 2.07f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Cargo,
			null, 0.6f,
			new[] { "cargo_bay" },
			new Color(0.45f, 0.4f, 0.3f)
		),
		["table_console"] = new PropInfo(
			"table_console", "Console Table", "res://assets/station_kit/props/Table_Console/Table_Console.glb",
			PrimitiveShape.Box, new Vector3(1.35f, 0.75f, 1.95f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Tech,
			"Access Console", 0.8f,
			new[] { "bridge", "engineering" },
			new Color(0.3f, 0.35f, 0.5f)
		),
		["table_console_big"] = new PropInfo(
			"table_console_big", "Large Console Table", "res://assets/station_kit/props/Table_Console_Big/Table_Console_Big.glb",
			PrimitiveShape.Box, new Vector3(2.58f, 0.75f, 1.95f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Tech,
			"Access Console", 0.4f,
			new[] { "bridge" },
			new Color(0.3f, 0.35f, 0.5f)
		),
		["forklift"] = new PropInfo(
			"forklift", "Forklift", "res://assets/station_kit/props/Forklift/Forklift.glb",
			PrimitiveShape.Box, new Vector3(1.32f, 1.14f, 3.56f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Cargo,
			null, 0.2f,
			new[] { "cargo_bay" },
			new Color(0.6f, 0.5f, 0.2f)
		),
		["mobile_device"] = new PropInfo(
			"mobile_device", "Mobile Device", "res://assets/station_kit/props/Mobile_Device/Mobile_Device.glb",
			PrimitiveShape.Box, new Vector3(0.78f, 1.01f, 0.64f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Tech,
			"Use Device", 0.6f,
			new[] { "engineering", "med_bay", "bridge" },
			new Color(0.35f, 0.4f, 0.5f)
		),
		["tarpaulin"] = new PropInfo(
			"tarpaulin", "Tarpaulin Cover", "res://assets/station_kit/props/Tarpaulin/Tarpaulin.glb",
			PrimitiveShape.Box, new Vector3(2.97f, 1.80f, 2.67f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Cargo,
			null, 0.3f,
			new[] { "cargo_bay", "derelict" },
			new Color(0.4f, 0.35f, 0.3f)
		),
		["turbine"] = new PropInfo(
			"turbine", "Turbine", "res://assets/station_kit/props/Turbine/Turbine.glb",
			PrimitiveShape.Cylinder, new Vector3(3.36f, 1.45f, 3.36f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Structural,
			null, 0.2f,
			new[] { "engineering" },
			new Color(0.4f, 0.45f, 0.5f)
		),

		// --- Large equipment (blocking) ---
		["device_tall"] = new PropInfo(
			"device_tall", "Tall Device", "res://assets/station_kit/props/Device_Tall/Device_Tall.glb",
			PrimitiveShape.Box, new Vector3(0.96f, 3.70f, 1.81f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Tech,
			null, 0.4f,
			new[] { "engineering", "bridge" },
			new Color(0.35f, 0.35f, 0.4f)
		),
		["device_small"] = new PropInfo(
			"device_small", "Small Device", "res://assets/station_kit/props/Device_Small/Device_Small.glb",
			PrimitiveShape.Box, new Vector3(0.82f, 0.90f, 0.90f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Tech,
			null, 0.8f,
			new[] { "engineering", "med_bay", "generic" },
			new Color(0.35f, 0.4f, 0.45f)
		),
		["device_large"] = new PropInfo(
			"device_large", "Large Device", "res://assets/station_kit/props/Device_Large/Device_Large.glb",
			PrimitiveShape.Box, new Vector3(2.01f, 4.79f, 2.58f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Tech,
			null, 0.2f,
			new[] { "engineering" },
			new Color(0.3f, 0.35f, 0.4f)
		),
		["device_wide"] = new PropInfo(
			"device_wide", "Wide Device", "res://assets/station_kit/props/Device_Wide/Device_Wide.glb",
			PrimitiveShape.Box, new Vector3(2.45f, 5.71f, 1.65f),
			CollisionTier.Blocking, SnapType.Floor, PropCategory.Tech,
			null, 0.2f,
			new[] { "engineering" },
			new Color(0.3f, 0.3f, 0.4f)
		),

		// --- Wall props ---
		["screen_wall"] = new PropInfo(
			"screen_wall", "Wall Screen", "res://assets/station_kit/props/Screen_Wall/Screen_Wall.glb",
			PrimitiveShape.Box, new Vector3(0.59f, 0.47f, 0.26f),
			CollisionTier.InteractableOnly, SnapType.Wall, PropCategory.Tech,
			"Access Terminal", 1.2f,
			new[] { "bridge", "engineering", "generic" },
			new Color(0.2f, 0.3f, 0.5f)
		),
		["extinguisher"] = new PropInfo(
			"extinguisher", "Extinguisher", "res://assets/station_kit/props/Extinguisher/Extinguisher.glb",
			PrimitiveShape.Cylinder, new Vector3(0.14f, 0.67f, 0.38f),
			CollisionTier.None, SnapType.Wall, PropCategory.Decoration,
			null, 0.6f,
			new[] { "generic", "engineering", "cargo_bay" },
			new Color(0.8f, 0.15f, 0.1f)
		),
		["vent_wall"] = new PropInfo(
			"vent_wall", "Wall Vent", "res://assets/station_kit/props/Vent_Wall/Vent_Wall.glb",
			PrimitiveShape.Box, new Vector3(0.80f, 0.80f, 0.12f),
			CollisionTier.None, SnapType.Wall, PropCategory.Decoration,
			null, 1.0f,
			new[] { "generic", "engineering", "cargo_bay", "quarters" },
			new Color(0.4f, 0.4f, 0.4f)
		),
		["wall_case"] = new PropInfo(
			"wall_case", "Wall Case", "res://assets/station_kit/props/Wall_Case/Wall_Case.glb",
			PrimitiveShape.Box, new Vector3(0.60f, 0.86f, 0.25f),
			CollisionTier.None, SnapType.Wall, PropCategory.Decoration,
			null, 0.7f,
			new[] { "generic", "engineering", "quarters" },
			new Color(0.45f, 0.45f, 0.5f)
		),
		["hatch_floor"] = new PropInfo(
			"hatch_floor", "Floor Hatch", "res://assets/station_kit/props/Hatch_Floor/Hatch_Floor.glb",
			PrimitiveShape.Box, new Vector3(2.0f, 0.14f, 1.4f),
			CollisionTier.None, SnapType.Floor, PropCategory.Decoration,
			null, 0.4f,
			new[] { "engineering", "cargo_bay" },
			new Color(0.4f, 0.4f, 0.4f)
		),

		// --- Ceiling/Lighting ---
		["lamp_small"] = new PropInfo(
			"lamp_small", "Small Lamp", "res://assets/station_kit/props/Lamp_Small/Lamp_Small.glb",
			PrimitiveShape.Box, new Vector3(0.60f, 0.40f, 0.12f),
			CollisionTier.None, SnapType.Ceiling, PropCategory.Decoration,
			null, 1.0f,
			new[] { "generic", "quarters", "bar", "med_bay" },
			new Color(0.8f, 0.8f, 0.7f)
		),
		["lamp_wide"] = new PropInfo(
			"lamp_wide", "Wide Lamp", "res://assets/station_kit/props/Lamp_Wide/Lamp_Wide.glb",
			PrimitiveShape.Box, new Vector3(1.60f, 0.40f, 0.12f),
			CollisionTier.None, SnapType.Ceiling, PropCategory.Decoration,
			null, 0.8f,
			new[] { "bridge", "engineering", "cargo_bay" },
			new Color(0.8f, 0.8f, 0.7f)
		),
		["lamp_hanging"] = new PropInfo(
			"lamp_hanging", "Hanging Lamp", "res://assets/station_kit/props/Lamp_Hanging/Lamp_Hanging.glb",
			PrimitiveShape.Box, new Vector3(0.61f, 1.23f, 0.53f),
			CollisionTier.None, SnapType.Ceiling, PropCategory.Decoration,
			null, 0.6f,
			new[] { "cargo_bay", "engineering", "derelict" },
			new Color(0.7f, 0.7f, 0.6f)
		),

		// --- Ventilation ---
		["vent_large"] = new PropInfo(
			"vent_large", "Large Vent", "res://assets/station_kit/props/Vent_Large/Vent_Large.glb",
			PrimitiveShape.Box, new Vector3(1.53f, 0.77f, 0.45f),
			CollisionTier.None, SnapType.Floor, PropCategory.Structural,
			null, 0.4f,
			new[] { "engineering", "cargo_bay" },
			new Color(0.4f, 0.4f, 0.45f)
		),
		["vent_medium"] = new PropInfo(
			"vent_medium", "Medium Vent", "res://assets/station_kit/props/Vent_Medium/Vent_Medium.glb",
			PrimitiveShape.Box, new Vector3(1.03f, 0.77f, 0.45f),
			CollisionTier.None, SnapType.Floor, PropCategory.Structural,
			null, 0.5f,
			new[] { "engineering", "generic" },
			new Color(0.4f, 0.4f, 0.45f)
		),
	};

	/// <summary>
	/// Room purposes that drive scatter prop selection.
	/// </summary>
	public static readonly string[] RoomPurposes =
	{
		"cargo_bay", "bar", "bridge", "quarters", "med_bay", "engineering", "generic", "derelict"
	};

	/// <summary>
	/// Theme density multipliers — fraction of markers that scatter fills.
	/// </summary>
	public static readonly Dictionary<string, float> ThemeDensity = new()
	{
		["military"] = 0.4f,
		["industrial"] = 0.9f,
		["frontier"] = 0.7f,
		["criminal"] = 0.6f,
		["derelict"] = 0.8f,
	};

	// --- Furniture Groups ---

	public record FurnitureGroupSlot(string PropId, Vector3 Offset, float YRotation);

	public record FurnitureGroup(
		string Id,
		string[] Purposes,
		FurnitureGroupSlot[] Slots,
		float Weight
	);

	public static readonly FurnitureGroup[] FurnitureGroups =
	{
		new("workstation", new[] { "bridge", "engineering", "quarters" }, new[]
		{
			new FurnitureGroupSlot("desk", Vector3.Zero, 0),
			new FurnitureGroupSlot("chair", new Vector3(0, 0, 1.2f), Mathf.DegToRad(180)),
		}, 1.5f),

		new("console_station", new[] { "bridge" }, new[]
		{
			new FurnitureGroupSlot("table_console", Vector3.Zero, 0),
			new FurnitureGroupSlot("chair", new Vector3(0, 0, 1.2f), Mathf.DegToRad(180)),
			new FurnitureGroupSlot("chair", new Vector3(1.0f, 0, 1.2f), Mathf.DegToRad(180)),
		}, 1.0f),

		new("cargo_stack", new[] { "cargo_bay" }, new[]
		{
			new FurnitureGroupSlot("container", Vector3.Zero, 0),
			new FurnitureGroupSlot("container", new Vector3(0, 0.56f, 0), Mathf.DegToRad(15)),
			new FurnitureGroupSlot("barrel", new Vector3(1.2f, 0, 0), 0),
		}, 1.2f),

		new("storage_corner", new[] { "cargo_bay", "engineering" }, new[]
		{
			new FurnitureGroupSlot("cabinet", Vector3.Zero, 0),
			new FurnitureGroupSlot("barrel", new Vector3(0.6f, 0, 0), 0),
			new FurnitureGroupSlot("barrel", new Vector3(0.6f, 0, 0.7f), Mathf.DegToRad(20)),
		}, 0.8f),

		new("med_station", new[] { "med_bay", "quarters" }, new[]
		{
			new FurnitureGroupSlot("cabinet", Vector3.Zero, 0),
			new FurnitureGroupSlot("mobile_device", new Vector3(0.8f, 0, 0), 0),
		}, 0.9f),
	};

	/// <summary>
	/// Get furniture groups valid for a given room purpose.
	/// </summary>
	public static List<FurnitureGroup> GetGroupsForPurpose(string purpose)
	{
		return FurnitureGroups
			.Where(g => g.Purposes.Contains(purpose))
			.ToList();
	}

	/// <summary>
	/// Get all props that can appear in a given room purpose.
	/// </summary>
	public static List<PropInfo> GetPropsForPurpose(string purpose)
	{
		return Props.Values
			.Where(p => p.Purposes.Contains(purpose))
			.ToList();
	}

	/// <summary>
	/// Get all floor-snapping props for a purpose (for scatter on Marker_Prop_Floor).
	/// </summary>
	public static List<PropInfo> GetFloorPropsForPurpose(string purpose)
	{
		return Props.Values
			.Where(p => p.Snap == SnapType.Floor && p.Purposes.Contains(purpose))
			.ToList();
	}

	/// <summary>
	/// Get all wall-snapping props for a purpose (for scatter on Marker_Prop_Wall).
	/// </summary>
	public static List<PropInfo> GetWallPropsForPurpose(string purpose)
	{
		return Props.Values
			.Where(p => p.Snap == SnapType.Wall && p.Purposes.Contains(purpose))
			.ToList();
	}

	/// <summary>
	/// Create a runtime placeholder mesh for a prop (used when no GLB exists).
	/// </summary>
	public static MeshInstance3D CreatePlaceholderMesh(PropInfo prop)
	{
		Mesh mesh;
		if (prop.Shape == PrimitiveShape.Cylinder)
		{
			mesh = new CylinderMesh
			{
				TopRadius = prop.Size.X / 2.0f,
				BottomRadius = prop.Size.X / 2.0f,
				Height = prop.Size.Y,
			};
		}
		else
		{
			mesh = new BoxMesh
			{
				Size = prop.Size,
			};
		}

		var mat = new StandardMaterial3D
		{
			AlbedoColor = prop.PlaceholderColor,
			Roughness = 0.8f,
		};
		mesh.SurfaceSetMaterial(0, mat);

		return new MeshInstance3D
		{
			Mesh = mesh,
			Position = new Vector3(0, prop.Size.Y / 2.0f, 0), // Pivot at bottom
		};
	}
}
