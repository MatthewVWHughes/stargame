using Godot;
using System.Collections.Generic;

namespace Starlight.StationInterior;

/// <summary>
/// Radial wheel for magazine selection. Shows all mags arranged in a circle.
/// 0 degrees (top) = repack option. Mouse angle selects a mag.
/// </summary>
public partial class MagRadialMenu : Control
{
    public FPSWeapon Weapon { get; set; }
    public int HighlightedIndex = -1; // -1 = none, -2 = repack
    public bool IsOpen { get; private set; }

    private const float Radius = 120.0f;
    private const float SlotRadius = 30.0f;
    private Vector2 _center;

    public override void _Ready()
    {
        Visible = false;
        MouseFilter = MouseFilterEnum.Ignore;
        SetAnchorsPreset(LayoutPreset.FullRect);
    }

    public void Open()
    {
        IsOpen = true;
        Visible = true;
        _center = GetViewportRect().Size / 2;
        Input.MouseMode = Input.MouseModeEnum.Visible;
        Input.WarpMouse(_center);
        QueueRedraw();
    }

    public void Close()
    {
        IsOpen = false;
        Visible = false;
        Input.MouseMode = Input.MouseModeEnum.Captured;
    }

    public override void _Process(double delta)
    {
        if (!IsOpen || Weapon == null) return;

        var mousePos = GetViewport().GetMousePosition();
        var offset = mousePos - _center;
        float dist = offset.Length();

        if (dist < 25)
        {
            HighlightedIndex = -1; // center dead zone
        }
        else
        {
            // Angle from top (0° = up, clockwise)
            float angle = Mathf.Atan2(offset.X, -offset.Y);
            if (angle < 0) angle += Mathf.Tau;

            int totalSlots = GetSlotCount();
            if (totalSlots == 0) { HighlightedIndex = -1; return; }

            float slotAngle = Mathf.Tau / totalSlots;
            int slot = Mathf.RoundToInt(angle / slotAngle) % totalSlots;

            // Slot 0 = repack (top), rest = mags
            if (slot == 0)
                HighlightedIndex = -2; // repack
            else
                HighlightedIndex = slot - 1; // mag index in spare list
        }

        QueueRedraw();
    }

    private int GetSlotCount()
    {
        if (Weapon == null) return 0;
        // 1 repack slot + spare mags (not the current one)
        return 1 + GetSpareMagCount();
    }

    private int GetSpareMagCount()
    {
        int count = 0;
        for (int i = 0; i < Weapon.Magazines.Count; i++)
            if (i != Weapon.CurrentMagIndex) count++;
        return count;
    }

    private List<(int magIndex, int ammo)> GetSpareMags()
    {
        var spares = new List<(int magIndex, int ammo)>();
        for (int i = 0; i < Weapon.Magazines.Count; i++)
            if (i != Weapon.CurrentMagIndex)
                spares.Add((i, Weapon.Magazines[i]));
        // Sort fullest first
        spares.Sort((a, b) => b.ammo.CompareTo(a.ammo));
        return spares;
    }

    public override void _Draw()
    {
        if (!IsOpen || Weapon == null) return;

        // Dim background
        DrawRect(GetViewportRect(), new Color(0, 0, 0, 0.5f));

        int totalSlots = GetSlotCount();
        if (totalSlots == 0) return;

        float slotAngle = Mathf.Tau / totalSlots;
        var spares = GetSpareMags();

        // Draw center circle
        DrawCircle(_center, 20, new Color(0.3f, 0.3f, 0.3f, 0.8f));

        // Current mag info in center
        DrawString(ThemeDB.FallbackFont, _center + new Vector2(-15, 5),
            $"{Weapon.CurrentMagAmmo}", HorizontalAlignment.Center, -1, 14,
            new Color(1, 1, 1));

        for (int i = 0; i < totalSlots; i++)
        {
            float angle = i * slotAngle - Mathf.Pi / 2; // start from top
            float posAngle = (i * slotAngle); // 0 = top
            var slotPos = _center + new Vector2(
                Mathf.Sin(posAngle) * Radius,
                -Mathf.Cos(posAngle) * Radius
            );

            bool highlighted;
            Color bgColor;
            string label;
            string subLabel = "";

            if (i == 0)
            {
                // Repack slot
                highlighted = HighlightedIndex == -2;
                bgColor = highlighted ? new Color(0.2f, 0.7f, 0.3f, 0.9f) : new Color(0.15f, 0.4f, 0.2f, 0.8f);
                label = "PACK";
            }
            else
            {
                int spareIdx = i - 1;
                highlighted = HighlightedIndex == spareIdx;

                if (spareIdx < spares.Count)
                {
                    int ammo = spares[spareIdx].ammo;
                    float fill = (float)ammo / Weapon.MagazineSize;

                    bgColor = highlighted
                        ? new Color(0.3f, 0.5f, 0.8f, 0.9f)
                        : new Color(0.15f, 0.25f, 0.4f, 0.8f);

                    if (ammo == 0) bgColor = highlighted
                        ? new Color(0.5f, 0.2f, 0.2f, 0.9f)
                        : new Color(0.3f, 0.1f, 0.1f, 0.8f);

                    label = $"{ammo}";
                    subLabel = $"/{Weapon.MagazineSize}";
                }
                else
                {
                    bgColor = new Color(0.2f, 0.2f, 0.2f, 0.5f);
                    label = "—";
                }
            }

            float r = highlighted ? SlotRadius + 5 : SlotRadius;
            DrawCircle(slotPos, r, bgColor);
            DrawArc(slotPos, r, 0, Mathf.Tau, 32,
                highlighted ? new Color(1, 1, 1, 0.8f) : new Color(0.5f, 0.5f, 0.5f, 0.5f), 2);

            DrawString(ThemeDB.FallbackFont, slotPos + new Vector2(-10, 3),
                label, HorizontalAlignment.Center, -1, 16,
                highlighted ? new Color(1, 1, 1) : new Color(0.8f, 0.8f, 0.8f));

            if (subLabel != "")
                DrawString(ThemeDB.FallbackFont, slotPos + new Vector2(-10, 18),
                    subLabel, HorizontalAlignment.Center, -1, 10,
                    new Color(0.6f, 0.6f, 0.6f));
        }

        // Draw lines from center to slots
        for (int i = 0; i < totalSlots; i++)
        {
            float posAngle = i * slotAngle;
            var slotPos = _center + new Vector2(
                Mathf.Sin(posAngle) * Radius,
                -Mathf.Cos(posAngle) * Radius
            );
            DrawLine(_center, slotPos, new Color(0.4f, 0.4f, 0.4f, 0.3f), 1);
        }
    }

    /// <summary>
    /// Apply the current selection. Queues a reload or repack via the weapon.
    /// </summary>
    public bool ApplySelection()
    {
        if (Weapon == null) return false;

        if (HighlightedIndex == -2)
        {
            Repack();
            return true;
        }
        else if (HighlightedIndex >= 0)
        {
            var spares = GetSpareMags();
            if (HighlightedIndex < spares.Count)
            {
                int targetMagIndex = spares[HighlightedIndex].magIndex;
                // Queue a reload to this specific mag (goes through the reload delay)
                Weapon.StartReloadToMag(targetMagIndex);
                return true;
            }
        }

        return false;
    }

    private void Repack()
    {
        // Count all bullets across spare mags (not the current one)
        int currentAmmo = Weapon.Magazines[Weapon.CurrentMagIndex];
        int totalBullets = 0;

        for (int i = Weapon.Magazines.Count - 1; i >= 0; i--)
        {
            if (i == Weapon.CurrentMagIndex) continue;
            totalBullets += Weapon.Magazines[i];
            Weapon.Magazines.RemoveAt(i);
        }

        // Fix current index after removal
        Weapon.CurrentMagIndex = Weapon.Magazines.IndexOf(currentAmmo);
        if (Weapon.CurrentMagIndex < 0) Weapon.CurrentMagIndex = 0;

        // Create full mags from the total bullets
        while (totalBullets >= Weapon.MagazineSize)
        {
            Weapon.Magazines.Add(Weapon.MagazineSize);
            totalBullets -= Weapon.MagazineSize;
        }
        // One partial mag with remainder
        if (totalBullets > 0)
            Weapon.Magazines.Add(totalBullets);

        GD.Print($"Repacked: {Weapon.Magazines.Count} mags total, {Weapon.CurrentMagAmmo} in current");
    }
}
