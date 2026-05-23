extends Node

## Call this from C# to set up wiggle bones on a skeleton.
## Usage: wiggle_setup.call("setup", skeleton_node)

static func setup(skeleton: Skeleton3D) -> void:
	var RotMod = preload("res://addons/wigglebone/wiggle_rotation_modifier_3d.gd")
	var RotProps = preload("res://addons/wigglebone/wiggle_rotation_properties_3d.gd")

	# Breast bones
	for bone_name in ["LeftBreast", "RightBreast"]:
		if skeleton.find_bone(bone_name) < 0:
			print("WiggleBone: bone '%s' not found" % bone_name)
			continue

		var props = RotProps.new()
		props.spring_freq = 3.0
		props.angular_damp = 5.0
		props.force_scale = 300.0
		props.linear_scale = 500.0
		props.swing_span = deg_to_rad(30.0)
		props.gravity = Vector3(0, -3.0, 0)

		var wiggle = RotMod.new()
		skeleton.add_child(wiggle)
		wiggle.properties = props
		wiggle.bone_name = bone_name
		print("WiggleBone: %s setup (bone_idx=%d)" % [bone_name, skeleton.find_bone(bone_name)])

	# Hair bones
	var hair_settings = {
		"spring_freq": 2.0,
		"angular_damp": 4.0,
		"force_scale": 200.0,
		"linear_scale": 350.0,
		"swing_span": deg_to_rad(40.0),
		"gravity": Vector3(0, -3.0, 0),
	}

	for bone_name in [
		"Hair_Bang_1", "Hair_Bang_2",
		"Hair_Left_1", "Hair_Left_2",
		"Hair_Right_1", "Hair_Right_2",
		"Hair_Back_1", "Hair_Back_2",
		"Hair_Bun_1", "Hair_Bun_2", "Hair_Bun_3"]:

		if skeleton.find_bone(bone_name) < 0:
			continue

		var props = RotProps.new()
		props.spring_freq = hair_settings.spring_freq
		props.angular_damp = hair_settings.angular_damp
		props.force_scale = hair_settings.force_scale
		props.linear_scale = hair_settings.linear_scale
		props.swing_span = hair_settings.swing_span
		props.gravity = hair_settings.gravity

		var wiggle = RotMod.new()
		skeleton.add_child(wiggle)
		wiggle.properties = props
		wiggle.bone_name = bone_name

	print("WiggleBone: setup complete")
