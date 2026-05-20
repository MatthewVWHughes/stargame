#!/usr/bin/env python3
import math
import random
from pathlib import Path

from PIL import Image, ImageFilter


OUT_DIR = Path("Content/SourceArt/Space/Star")


def smooth_noise(width, height, cell, seed):
    rng = random.Random(seed)
    gw = width // cell + 3
    gh = height // cell + 3
    grid = [[rng.random() for _ in range(gw)] for _ in range(gh)]
    data = [[0.0 for _ in range(width)] for _ in range(height)]
    for y in range(height):
        gy = y / cell
        y0 = int(gy)
        fy = gy - y0
        fy = fy * fy * (3.0 - 2.0 * fy)
        for x in range(width):
            gx = x / cell
            x0 = int(gx)
            fx = gx - x0
            fx = fx * fx * (3.0 - 2.0 * fx)
            a = grid[y0][x0]
            b = grid[y0][x0 + 1]
            c = grid[y0 + 1][x0]
            d = grid[y0 + 1][x0 + 1]
            data[y][x] = (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy
    return data


def normalize(values):
    flat = [v for row in values for v in row]
    lo = min(flat)
    hi = max(flat)
    scale = 1.0 / max(0.0001, hi - lo)
    return [[(v - lo) * scale for v in row] for row in values]


def generate_photosphere():
    width, height = 2048, 1024
    layers = [
        smooth_noise(width, height, 16, 101),
        smooth_noise(width, height, 32, 202),
        smooth_noise(width, height, 96, 303),
    ]
    pixels = []
    for y in range(height):
        v = y / (height - 1)
        lat_fade = math.sin(math.pi * v) ** 0.35
        for x in range(width):
            u = x / (width - 1)
            fine = layers[0][y][x]
            medium = layers[1][y][x]
            broad = layers[2][y][x]
            filament = abs(fine - medium)
            heat = max(0.0, min(1.0, 0.44 * fine + 0.36 * medium + 0.38 * filament + 0.18 * broad))
            hot = max(0.0, min(1.0, (filament * 2.6 + fine * 0.25 - 0.42) * 1.5))
            spot_seed = broad * 0.72 + medium * 0.28
            spot = max(0.0, min(1.0, (spot_seed - 0.76) / 0.13)) * lat_fade
            # Slight longitudinal streaking avoids a tiled noise look once wrapped on a sphere.
            wave = 0.5 + 0.5 * math.sin(u * math.tau * 18.0 + broad * 3.5)
            heat = max(0.0, min(1.0, heat * (0.86 + 0.18 * wave)))
            pixels.append((
                int(heat * 255),
                int(hot * 255),
                int(spot * 255),
                255,
            ))
    img = Image.new("RGBA", (width, height))
    img.putdata(pixels)
    img = img.filter(ImageFilter.UnsharpMask(radius=1.4, percent=85, threshold=3))
    img.save(OUT_DIR / "T_StarPhotosphereMasks.png")


def generate_corona():
    size = 1024
    noise_a = smooth_noise(size, size, 24, 404)
    noise_b = smooth_noise(size, size, 96, 505)
    pixels = []
    center = (size - 1) * 0.5
    for y in range(size):
        py = (y - center) / center
        for x in range(size):
            px = (x - center) / center
            r = math.sqrt(px * px + py * py)
            angle = math.atan2(py, px)
            inner = max(0.0, min(1.0, (r - 0.26) / 0.12))
            outer = 1.0 - max(0.0, min(1.0, (r - 0.92) / 0.08))
            radial = math.exp(-r * 2.35)
            rays = max(0.0, noise_a[y][x] * 0.75 + noise_b[y][x] * 0.45 - 0.48)
            spoke = 0.5 + 0.5 * math.sin(angle * 17.0 + noise_b[y][x] * 5.0)
            alpha = max(0.0, min(1.0, inner * outer * (radial * 0.62 + rays * spoke * 0.74)))
            pixels.append((int(alpha * 255), int(rays * 255), int(spoke * 255), int(alpha * 255)))
    img = Image.new("RGBA", (size, size))
    img.putdata(pixels)
    img = img.filter(ImageFilter.GaussianBlur(radius=1.15))
    img.save(OUT_DIR / "T_StarCoronaMasks.png")


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    generate_photosphere()
    generate_corona()
    print(f"Generated star source textures in {OUT_DIR}")


if __name__ == "__main__":
    main()
