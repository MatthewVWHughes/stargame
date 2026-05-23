namespace Starlight.Util;

public static class Format
{
    public static string Distance(float dist)
    {
        if (dist >= 1000000f) return $"{dist / 1000000f:F1}M u";
        if (dist >= 1000f) return $"{dist / 1000f:F1}k u";
        return $"{dist:F0} u";
    }
}
