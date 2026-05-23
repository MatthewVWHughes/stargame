using Godot;

namespace Starlight.Game.UI;

/// <summary>
/// 2D overlay that draws distant celestial bodies as colored dots.
/// Projects 3D world positions to screen coordinates — completely
/// bypasses frustum culling, far plane, and depth buffer issues.
/// When a body's real mesh is large enough on screen, the dot hides.
/// </summary>
public partial class PlanetOverlay : Control
{
	private CelestialBody[] _bodies;
	private Node3D _star;

	public void Setup(CelestialBody[] bodies, Node3D star)
	{
		_bodies = bodies;
		_star = star;
	}

	public override void _Process(double delta)
	{
		QueueRedraw();
	}

	public override void _Draw()
	{
		if (_bodies == null) return;

		var camera = GetViewport().GetCamera3D();
		if (camera == null) return;

		Vector2 screenSize = GetViewportRect().Size;
		float screenHeight = screenSize.Y;
		float tanHalfFov = Mathf.Tan(Mathf.DegToRad(camera.Fov / 2f));

		// Star dot.
		if (_star != null && IsInstanceValid(_star))
			DrawBodyDot(camera, _star.GlobalPosition, 2000f, new Color(1f, 0.9f, 0.3f),
				10f, 4f, screenSize, screenHeight, tanHalfFov, "Sol");

		// Planet/moon dots.
		foreach (var body in _bodies)
		{
			if (!IsInstanceValid(body.Node)) continue;
			DrawBodyDot(camera, body.Node.GlobalPosition, body.Radius, body.MapColor,
				body.Radius > 3000f ? 6f : 4f, // gas giants get bigger dots
				2f, screenSize, screenHeight, tanHalfFov, body.Name);
		}
	}

	private void DrawBodyDot(Camera3D camera, Vector3 worldPos, float bodyRadius,
		Color color, float minPixels, float emission, Vector2 screenSize,
		float screenHeight, float tanHalfFov, string name)
	{
		float dist = camera.GlobalPosition.DistanceTo(worldPos);
		if (dist < 1f) return;

		float meshPixels = bodyRadius * 2f / dist * screenHeight / (2f * tanHalfFov);

		// Hide the dot when the 3D mesh is within the Far plane and
		// large enough to be visible. The mesh takes over rendering.
		float cameraFar = camera.Far;
		if (dist < cameraFar && meshPixels > 2f)
			return;

		if (camera.IsPositionBehind(worldPos))
			return;

		Vector2 screenPos = camera.UnprojectPosition(worldPos);

		float margin = 30f;
		bool onScreen = screenPos.X > -margin && screenPos.X < screenSize.X + margin
			&& screenPos.Y > -margin && screenPos.Y < screenSize.Y + margin;
		if (!onScreen) return;

		float coreSize = Mathf.Max(minPixels * 0.4f, meshPixels * 0.5f);

		// Soft glow layers — bright core, fading halo.
		DrawCircle(screenPos, coreSize * 4f, new Color(color.R, color.G, color.B, 0.03f));
		DrawCircle(screenPos, coreSize * 2.5f, new Color(color.R, color.G, color.B, 0.08f));
		DrawCircle(screenPos, coreSize * 1.5f, new Color(color.R, color.G, color.B, 0.2f));
		DrawCircle(screenPos, coreSize, color); // bright core

	}
}
