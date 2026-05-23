using GdUnit4;
using Godot;
using static GdUnit4.Assertions;
using Starlight.Orbital;

namespace Starlight.Tests;

/// <summary>
/// Pure math tests for <see cref="OrbitalBody.ComputeOrbitPosition"/> —
/// Kepler rail orbital position from time.
/// </summary>
[TestSuite]
public class OrbitalBodyTests
{
    private const float Tolerance = 0.01f;

    [TestCase]
    public void AtT0_NoPhase_NoTilt_CircularOrbit_BodyAtPositiveX()
    {
        Vector3 pos = OrbitalBody.ComputeOrbitPosition(
            elapsedTime: 0f,
            semiMajorAxis: 100f,
            period: 60f,
            phaseOffset: 0f,
            eccentricity: 0f,
            sinInc: 0f,
            cosInc: 1f,
            out float theta);

        AssertThat(theta).IsEqualApprox(0f, Tolerance);
        AssertThat(pos.X).IsEqualApprox(100f, Tolerance);
        AssertThat(pos.Y).IsEqualApprox(0f, Tolerance);
        AssertThat(pos.Z).IsEqualApprox(0f, Tolerance);
    }

    [TestCase]
    public void AtQuarterPeriod_CircularOrbit_BodyAtPositiveZ()
    {
        Vector3 pos = OrbitalBody.ComputeOrbitPosition(
            elapsedTime: 15f,           // quarter of 60s period
            semiMajorAxis: 100f,
            period: 60f,
            phaseOffset: 0f,
            eccentricity: 0f,
            sinInc: 0f,
            cosInc: 1f,
            out _);

        AssertThat(pos.X).IsEqualApprox(0f, Tolerance);
        AssertThat(pos.Z).IsEqualApprox(100f, Tolerance);
    }

    [TestCase]
    public void AtHalfPeriod_CircularOrbit_BodyAtNegativeX()
    {
        Vector3 pos = OrbitalBody.ComputeOrbitPosition(
            elapsedTime: 30f,           // half of 60s period
            semiMajorAxis: 100f,
            period: 60f,
            phaseOffset: 0f,
            eccentricity: 0f,
            sinInc: 0f,
            cosInc: 1f,
            out _);

        AssertThat(pos.X).IsEqualApprox(-100f, Tolerance);
        AssertThat(pos.Z).IsEqualApprox(0f, Tolerance);
    }

    [TestCase]
    public void EccentricityAffectsRadiusAtPerihelion()
    {
        // theta = 0 is perihelion when phaseOffset = 0; with e>0 radius is smaller than a.
        Vector3 pos = OrbitalBody.ComputeOrbitPosition(
            elapsedTime: 0f,
            semiMajorAxis: 100f,
            period: 60f,
            phaseOffset: 0f,
            eccentricity: 0.5f,
            sinInc: 0f,
            cosInc: 1f,
            out _);

        // r = a(1-e^2)/(1+e) = 100 * 0.75 / 1.5 = 50
        AssertThat(pos.X).IsEqualApprox(50f, Tolerance);
    }

    [TestCase]
    public void InclinationTiltsTheOrbit()
    {
        // 90-degree inclination: what was positive Z becomes positive Y.
        Vector3 pos = OrbitalBody.ComputeOrbitPosition(
            elapsedTime: 15f,
            semiMajorAxis: 100f,
            period: 60f,
            phaseOffset: 0f,
            eccentricity: 0f,
            sinInc: 1f,                 // sin(90°)
            cosInc: 0f,                 // cos(90°)
            out _);

        AssertThat(pos.Y).IsEqualApprox(100f, Tolerance);
        AssertThat(pos.Z).IsEqualApprox(0f, Tolerance);
    }

    [TestCase]
    public void ZeroPeriod_ClampedDoesNotThrow()
    {
        // Period <= 0 should be clamped to avoid divide by zero.
        Vector3 pos = OrbitalBody.ComputeOrbitPosition(
            elapsedTime: 0f,
            semiMajorAxis: 100f,
            period: 0f,
            phaseOffset: 0f,
            eccentricity: 0f,
            sinInc: 0f,
            cosInc: 1f,
            out _);

        // Should still produce a finite position (the x=a default at theta=0 + small clamp drift).
        AssertThat(float.IsFinite(pos.X)).IsTrue();
        AssertThat(float.IsFinite(pos.Y)).IsTrue();
        AssertThat(float.IsFinite(pos.Z)).IsTrue();
    }

    [TestCase]
    public void PhaseOffset_ShiftsStartingTheta()
    {
        Vector3 pos = OrbitalBody.ComputeOrbitPosition(
            elapsedTime: 0f,
            semiMajorAxis: 100f,
            period: 60f,
            phaseOffset: Mathf.Pi / 2f,  // 90 degrees
            eccentricity: 0f,
            sinInc: 0f,
            cosInc: 1f,
            out float theta);

        AssertThat(theta).IsEqualApprox(Mathf.Pi / 2f, Tolerance);
        AssertThat(pos.X).IsEqualApprox(0f, Tolerance);
        AssertThat(pos.Z).IsEqualApprox(100f, Tolerance);
    }
}
