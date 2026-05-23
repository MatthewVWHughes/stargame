using GdUnit4;
using Godot;
using static GdUnit4.Assertions;
using Starlight.Orbital;

namespace Starlight.Tests;

/// <summary>
/// Pure math tests for the L1/L2 offset helper. The real node runs the
/// same function each physics tick; production behavior is fully covered
/// by these tests.
/// </summary>
[TestSuite]
public class LagrangeAnchorTests
{
    private const float Tolerance = 0.01f;

    [TestCase]
    public void L2_Anti_Sunward_OffsetBeyondPlanet()
    {
        bool ok = LagrangeAnchor.TryComputeLagrangePosition(
            planetPosition: new Vector3(100f, 0f, 0f),
            starPosition: Vector3.Zero,
            distance: 10f,
            point: LagrangeAnchor.LagrangePoint.L2,
            out Vector3 pos);

        AssertThat(ok).IsTrue();
        AssertThat(pos.X).IsEqualApprox(110f, Tolerance);
        AssertThat(pos.Y).IsEqualApprox(0f, Tolerance);
        AssertThat(pos.Z).IsEqualApprox(0f, Tolerance);
    }

    [TestCase]
    public void L1_Sunward_OffsetBetweenStarAndPlanet()
    {
        bool ok = LagrangeAnchor.TryComputeLagrangePosition(
            planetPosition: new Vector3(100f, 0f, 0f),
            starPosition: Vector3.Zero,
            distance: 10f,
            point: LagrangeAnchor.LagrangePoint.L1,
            out Vector3 pos);

        AssertThat(ok).IsTrue();
        AssertThat(pos.X).IsEqualApprox(90f, Tolerance);
    }

    [TestCase]
    public void DiagonalOrbit_DirectionFollowsRadial()
    {
        // Planet at (30, 0, 40) from a star at origin: radial magnitude 50.
        bool ok = LagrangeAnchor.TryComputeLagrangePosition(
            planetPosition: new Vector3(30f, 0f, 40f),
            starPosition: Vector3.Zero,
            distance: 5f,
            point: LagrangeAnchor.LagrangePoint.L2,
            out Vector3 pos);

        AssertThat(ok).IsTrue();
        // Anchor at planet + 5*unitRadial = (30,0,40) + 5 * (0.6, 0, 0.8) = (33, 0, 44)
        AssertThat(pos.X).IsEqualApprox(33f, Tolerance);
        AssertThat(pos.Z).IsEqualApprox(44f, Tolerance);
    }

    [TestCase]
    public void CoincidentBodies_ReturnsFalse()
    {
        bool ok = LagrangeAnchor.TryComputeLagrangePosition(
            planetPosition: Vector3.Zero,
            starPosition: Vector3.Zero,
            distance: 10f,
            point: LagrangeAnchor.LagrangePoint.L2,
            out Vector3 pos);

        AssertThat(ok).IsFalse();
        AssertThat(pos).IsEqual(Vector3.Zero);
    }

    [TestCase]
    public void StarOffset_AnchorStillFollowsRadialFromStar()
    {
        // Planet at (200,0,0), star at (100,0,0) — radial is +X magnitude 100.
        bool ok = LagrangeAnchor.TryComputeLagrangePosition(
            planetPosition: new Vector3(200f, 0f, 0f),
            starPosition: new Vector3(100f, 0f, 0f),
            distance: 10f,
            point: LagrangeAnchor.LagrangePoint.L2,
            out Vector3 pos);

        AssertThat(ok).IsTrue();
        AssertThat(pos.X).IsEqualApprox(210f, Tolerance);
    }
}
