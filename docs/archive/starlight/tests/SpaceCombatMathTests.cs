using GdUnit4;
using Godot;
using static GdUnit4.Assertions;
using Starlight.Game.Combat;

namespace Starlight.Tests;

/// <summary>
/// Tests for the pure intercept-point solver used by hostile AI and point
/// defense. Ships a fighter-style lead-shot calculation — worth locking in.
/// </summary>
[TestSuite]
public class SpaceCombatMathTests
{
    private const float Tolerance = 0.5f;

    [TestCase]
    public void StationaryTarget_StationaryShooter_InterceptEqualsTarget()
    {
        bool ok = SpaceCombatMath.TryGetInterceptPoint(
            shooterPosition: Vector3.Zero,
            shooterVelocity: Vector3.Zero,
            targetPosition: new Vector3(100f, 0f, 0f),
            targetVelocity: Vector3.Zero,
            projectileSpeed: 50f,
            out Vector3 intercept);

        AssertThat(ok).IsTrue();
        AssertThat(intercept.X).IsEqualApprox(100f, Tolerance);
        AssertThat(intercept.Y).IsEqualApprox(0f, Tolerance);
        AssertThat(intercept.Z).IsEqualApprox(0f, Tolerance);
    }

    [TestCase]
    public void TargetCrossingPath_LeadsTheTarget()
    {
        // Target at (100,0,0) moving +Z at 10 u/s; projectile at 50 u/s.
        // Answer: intercept must be somewhere with positive Z > 0, because
        // the projectile needs lead time.
        bool ok = SpaceCombatMath.TryGetInterceptPoint(
            shooterPosition: Vector3.Zero,
            shooterVelocity: Vector3.Zero,
            targetPosition: new Vector3(100f, 0f, 0f),
            targetVelocity: new Vector3(0f, 0f, 10f),
            projectileSpeed: 50f,
            out Vector3 intercept);

        AssertThat(ok).IsTrue();
        AssertThat(intercept.Z).IsGreater(0f);
        // Sanity: intercept shouldn't be wildly far from target.
        AssertThat(intercept.X).IsEqualApprox(100f, Tolerance);
    }

    [TestCase]
    public void TargetFasterThanProjectile_FleeingDirectly_ReturnsFalse()
    {
        // Target is running from shooter at 100 u/s; projectile is 50 u/s.
        // Cannot catch.
        bool ok = SpaceCombatMath.TryGetInterceptPoint(
            shooterPosition: Vector3.Zero,
            shooterVelocity: Vector3.Zero,
            targetPosition: new Vector3(100f, 0f, 0f),
            targetVelocity: new Vector3(100f, 0f, 0f),  // away from shooter
            projectileSpeed: 50f,
            out _);

        AssertThat(ok).IsFalse();
    }

    [TestCase]
    public void TargetApproachingHeadOn_InterceptOnLine()
    {
        // Target approaches shooter along +X→-X; any intercept should be on the X axis.
        bool ok = SpaceCombatMath.TryGetInterceptPoint(
            shooterPosition: Vector3.Zero,
            shooterVelocity: Vector3.Zero,
            targetPosition: new Vector3(100f, 0f, 0f),
            targetVelocity: new Vector3(-10f, 0f, 0f),
            projectileSpeed: 50f,
            out Vector3 intercept);

        AssertThat(ok).IsTrue();
        AssertThat(intercept.X).IsGreater(0f);
        AssertThat(intercept.X).IsLess(100f);
        AssertThat(intercept.Y).IsEqualApprox(0f, Tolerance);
        AssertThat(intercept.Z).IsEqualApprox(0f, Tolerance);
    }

    [TestCase]
    public void ShooterMoving_RelativeFrameHandled()
    {
        // Shooter moves +X at 10; target stationary at (100,0,0).
        // From shooter's frame, target is moving -X at 10. Projectile at 50.
        // Should still find an intercept in front of the shooter.
        bool ok = SpaceCombatMath.TryGetInterceptPoint(
            shooterPosition: Vector3.Zero,
            shooterVelocity: new Vector3(10f, 0f, 0f),
            targetPosition: new Vector3(100f, 0f, 0f),
            targetVelocity: Vector3.Zero,
            projectileSpeed: 50f,
            out Vector3 intercept);

        AssertThat(ok).IsTrue();
        AssertThat(intercept.X).IsEqualApprox(100f, Tolerance); // stationary target in world frame
    }

    [TestCase]
    public void TargetAtShooter_DegenerateCase_DoesNotCrash()
    {
        bool ok = SpaceCombatMath.TryGetInterceptPoint(
            shooterPosition: Vector3.Zero,
            shooterVelocity: Vector3.Zero,
            targetPosition: Vector3.Zero,
            targetVelocity: Vector3.Zero,
            projectileSpeed: 50f,
            out Vector3 intercept);

        // Behavior: falls into the "degenerate time" branch; must not throw or NaN.
        AssertThat(float.IsFinite(intercept.X)).IsTrue();
        AssertThat(float.IsFinite(intercept.Y)).IsTrue();
        AssertThat(float.IsFinite(intercept.Z)).IsTrue();
        // ok may be true or false here; either is acceptable.
    }
}
