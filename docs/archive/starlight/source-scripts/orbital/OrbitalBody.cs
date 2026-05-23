using Godot;

namespace Starlight.Orbital;

/// <summary>
/// Kepler rail orbit. Computes position from time using simple trig.
/// Attach to a Node3D that is a child of the body it orbits.
/// </summary>
public partial class OrbitalBody : Node3D
{
    [Export] public float SemiMajorAxis { get; set; } = 100f;
    [Export] public float Period { get; set; } = 120f;
    [Export] public float PhaseOffset { get; set; } = 0f;
    [Export] public float Eccentricity { get; set; } = 0f;
    /// <summary>Orbital inclination in degrees relative to the parent's equatorial/ecliptic plane.</summary>
    [Export] public float Inclination { get; set; } = 0f;

    public float CurrentTheta { get; private set; }

    private float _sinInc;
    private float _cosInc;
    private double _elapsedTime;

    public override void _Ready()
    {
        float incRad = Mathf.DegToRad(Inclination);
        _sinInc = Mathf.Sin(incRad);
        _cosInc = Mathf.Cos(incRad);
    }

    public override void _PhysicsProcess(double delta)
    {
        _elapsedTime += delta;
        Position = ComputeOrbitPosition(
            (float)_elapsedTime, SemiMajorAxis, Period, PhaseOffset,
            Eccentricity, _sinInc, _cosInc, out float theta);
        CurrentTheta = theta;
    }

    /// <summary>
    /// Pure Kepler-rail math. Caller supplies precomputed sin/cos of the
    /// inclination angle so tests don't have to replicate the deg-to-rad
    /// conversion every call.
    /// </summary>
    public static Vector3 ComputeOrbitPosition(
        float elapsedTime,
        float semiMajorAxis,
        float period,
        float phaseOffset,
        float eccentricity,
        float sinInc,
        float cosInc,
        out float theta)
    {
        float safePeriod = Mathf.Max(period, 0.1f);
        theta = (float)(elapsedTime * Mathf.Tau / safePeriod) + phaseOffset;

        float r;
        if (eccentricity > 0.001f)
        {
            float e2 = eccentricity * eccentricity;
            r = semiMajorAxis * (1f - e2) / (1f + eccentricity * Mathf.Cos(theta));
        }
        else
        {
            r = semiMajorAxis;
        }

        float x = r * Mathf.Cos(theta);
        float z = r * Mathf.Sin(theta);

        return new Vector3(x, z * sinInc, z * cosInc);
    }
}
