using Godot;
using System.Collections.Generic;

/// <summary>
/// Spring physics for bones — attach to a Skeleton3D to add jiggle to breast and hair bones.
/// Bones are configured in groups with shared spring settings.
/// </summary>
public partial class JiggleBone : Node
{
	[Export] public NodePath SkeletonPath { get; set; }

	private Skeleton3D _skeleton;
	private readonly List<SpringBone> _springs = new();
	private Vector3 _prevPosition;

	public override void _Ready()
	{
		_skeleton = GetNode<Skeleton3D>(SkeletonPath);
		if (_skeleton == null)
		{
			GD.PrintErr("JiggleBone: No skeleton found");
			return;
		}

		_prevPosition = _skeleton.GlobalPosition;

		// Breast bones — slow, heavy bounce
		AddSpring("LeftBreast", stiffness: 18f, damping: 7f, gravity: 0.8f, isBreast: true);
		AddSpring("RightBreast", stiffness: 18f, damping: 7f, gravity: 0.8f, isBreast: true);

		// Bang chain
		AddSpring("Hair_Bang_1", stiffness: 35f, damping: 6f, gravity: 0.4f);
		AddSpring("Hair_Bang_2", stiffness: 28f, damping: 5f, gravity: 0.5f);

		// Side hair
		AddSpring("Hair_Left_1", stiffness: 35f, damping: 6f, gravity: 0.4f);
		AddSpring("Hair_Left_2", stiffness: 28f, damping: 5f, gravity: 0.5f);
		AddSpring("Hair_Right_1", stiffness: 35f, damping: 6f, gravity: 0.4f);
		AddSpring("Hair_Right_2", stiffness: 28f, damping: 5f, gravity: 0.5f);

		// Back hair
		AddSpring("Hair_Back_1", stiffness: 35f, damping: 6f, gravity: 0.4f);
		AddSpring("Hair_Back_2", stiffness: 28f, damping: 5f, gravity: 0.5f);

		GD.Print($"JiggleBone: {_springs.Count} spring bones active");
	}

	private void AddSpring(string boneName, float stiffness, float damping, float gravity, bool isBreast = false)
	{
		int idx = _skeleton.FindBone(boneName);
		if (idx < 0)
		{
			GD.PrintErr($"JiggleBone: Bone '{boneName}' not found");
			return;
		}

		_springs.Add(new SpringBone
		{
			BoneIndex = idx,
			BoneName = boneName,
			Stiffness = stiffness,
			Damping = damping,
			Gravity = gravity,
			IsBreast = isBreast,
			Velocity = Vector3.Zero,
			CurrentOffset = Vector3.Zero,
			RestPosition = _skeleton.GetBoneRest(idx).Origin,
		});
	}

	public override void _PhysicsProcess(double delta)
	{
		if (_skeleton == null || _springs.Count == 0) return;

		float dt = (float)delta;
		dt = Mathf.Min(dt, 0.05f); // clamp to avoid instability

		// Calculate character velocity for inertia
		Vector3 currentPos = _skeleton.GlobalPosition;
		Vector3 charVelocity = (currentPos - _prevPosition) / dt;
		_prevPosition = currentPos;

		Vector3 scaledVelocity = charVelocity * 100f;
		float speed = scaledVelocity.Length();

		foreach (var spring in _springs)
		{
			if (spring.IsBreast)
			{
				// Simple: bounce up/down based on movement speed
				float amplitude = Mathf.Clamp(speed * 0.005f, 0f, 0.3f);
				float bounce = Mathf.Sin((float)Time.GetTicksMsec() * 0.01f) * amplitude;
				// Always offset from stored rest position
				_skeleton.SetBonePosePosition(spring.BoneIndex, spring.RestPosition + new Vector3(0, bounce, 0));
				if (amplitude > 0.001f)
					GD.Print($"{spring.BoneName}: amp={amplitude:F4} bounce={bounce:F4} speed={speed:F2}");
			}
			else
			{
				// Hair: spring physics with rotation
				Vector3 springForce = -spring.Stiffness * spring.CurrentOffset;
				Vector3 dampingForce = -spring.Damping * spring.Velocity;
				Vector3 inertiaForce = -scaledVelocity * 0.1f;
				Vector3 gravityForce = Vector3.Down * spring.Gravity;

				Vector3 acceleration = springForce + dampingForce + inertiaForce + gravityForce;
				spring.Velocity += acceleration * dt;
				spring.CurrentOffset += spring.Velocity * dt;

				float maxOffset = 0.025f;
				if (spring.CurrentOffset.Length() > maxOffset)
					spring.CurrentOffset = spring.CurrentOffset.Normalized() * maxOffset;

				Transform3D bonePose = _skeleton.GetBonePose(spring.BoneIndex);
				var extraRotation = new Basis(
					Vector3.Right, spring.CurrentOffset.Z * 2f
				) * new Basis(
					Vector3.Forward, spring.CurrentOffset.X * 2f
				);
				Transform3D newPose = bonePose;
				newPose.Basis = extraRotation * bonePose.Basis;
				_skeleton.SetBonePoseRotation(spring.BoneIndex, newPose.Basis.GetRotationQuaternion());
			}
		}
	}

	private class SpringBone
	{
		public int BoneIndex;
		public string BoneName;
		public float Stiffness;
		public float Damping;
		public float Gravity;
		public bool IsBreast;
		public Vector3 Velocity;
		public Vector3 CurrentOffset;
		public Vector3 RestPosition;
	}
}
