#!/usr/bin/env python3
from pathlib import Path

import random

from PIL import Image, ImageDraw, ImageFilter


SOURCE = Path("/home/matthew/Repos/starlight/game/textures/planets/8k_stars_milky_way.jpg")
OUTPUT = Path("/home/matthew/Repos/stargame/Content/SourceArt/Sky/T_MilkyWay_UnrealBalanced.png")
SIZE = (4096, 2048)


def main():
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)

    # Use the old Godot texture only as a broad Milky Way density mask. The JPG is
    # too compressed/blue to use directly as final color.
    source = Image.open(SOURCE).convert("L").resize(SIZE, Image.Resampling.LANCZOS)
    band = source.filter(ImageFilter.GaussianBlur(radius=14))
    band_pixels = band.load()

    image = Image.new("RGB", SIZE, (0, 0, 0))
    pixels = image.load()
    width, height = SIZE

    for y in range(height):
        for x in range(width):
            value = band_pixels[x, y]
            if value < 5:
                continue
            # Neutral, very low-intensity galactic haze. Keep blue below red/green.
            haze = min(34, int((value / 255.0) ** 1.55 * 58))
            pixels[x, y] = (haze, int(haze * 0.96), int(haze * 0.82))

    rng = random.Random(640812)
    draw = ImageDraw.Draw(image, "RGB")

    # Crisp stars are generated procedurally so they are not JPG blocks.
    for _ in range(15000):
        x = rng.randrange(width)
        y = rng.randrange(height)
        base = rng.random()
        if base < 0.82:
            brightness = rng.randrange(46, 115)
            radius = 0
        elif base < 0.975:
            brightness = rng.randrange(116, 190)
            radius = 0
        else:
            brightness = rng.randrange(185, 255)
            radius = 1

        warmth = rng.uniform(-0.08, 0.10)
        r = int(brightness * (1.00 + warmth))
        g = int(brightness * (0.96 + warmth * 0.25))
        b = int(brightness * (0.88 - warmth * 0.30))
        color = (max(0, min(255, r)), max(0, min(255, g)), max(0, min(255, b)))
        if radius:
            draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=color)
        else:
            pixels[x, y] = color

    image = image.filter(ImageFilter.UnsharpMask(radius=0.7, percent=90, threshold=3))
    image.save(OUTPUT, optimize=True)
    print(f"Wrote {OUTPUT}")


if __name__ == "__main__":
    main()
