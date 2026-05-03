#!/usr/bin/env python3
"""Generate placeholder reminder icons for RemindMe.

Produces 24-bit BMPs (the format `Bmp::draw` reads) at two sizes:
  data/icons/{stretch,water,walk}.bmp        — 64×64 (popup hero)
  data/icons-small/{stretch,water,walk}.bmp  — 24×24 (Today row)

The icons are deliberately simple flat shapes — drop in nicer artwork at
the same paths and uploadfs again to swap them. The colors mirror the
display palette in src/Settings.h (Color::WATER, ::WALK, ::STRETCH).

Run:
    python3 scripts/gen_icons.py
"""

from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT_BIG   = ROOT / "data" / "icons"
OUT_SMALL = ROOT / "data" / "icons-small"

PALETTE = {
    "stretch": (0xFF, 0xB8, 0x4D),   # amber
    "water":   (0x2D, 0xD4, 0xFF),   # cyan
    "walk":    (0x34, 0xD3, 0x99),   # green
}

# Background of the icon tile — matches Color::CARD (~ very dark grey).
TILE_BG = (0x18, 0x18, 0x1E)


def stretch_glyph(d: ImageDraw.ImageDraw, s: int, fg: tuple) -> None:
    # Simple stick figure with arms out to the side.
    pad = s * 0.2
    cx, cy = s / 2, s / 2
    # Head
    r = s * 0.10
    d.ellipse((cx - r, pad - r * 0.1, cx + r, pad - r * 0.1 + 2 * r), fill=fg)
    # Body
    d.line((cx, pad + 2 * r, cx, s - pad), fill=fg, width=max(2, s // 16))
    # Arms (extended)
    d.line((pad, cy, s - pad, cy), fill=fg, width=max(2, s // 16))
    # Legs
    d.line((cx, s - pad, pad * 1.2, s - pad * 0.4), fill=fg, width=max(2, s // 16))
    d.line((cx, s - pad, s - pad * 1.2, s - pad * 0.4), fill=fg, width=max(2, s // 16))


def water_glyph(d: ImageDraw.ImageDraw, s: int, fg: tuple) -> None:
    # Teardrop: a circle with a triangular tip.
    cx, cy = s / 2, s * 0.55
    r = s * 0.30
    d.ellipse((cx - r, cy - r * 0.6, cx + r, cy + r * 1.4), fill=fg)
    # Triangle on top.
    d.polygon([(cx - r * 0.95, cy - r * 0.3),
               (cx + r * 0.95, cy - r * 0.3),
               (cx,            s * 0.10)], fill=fg)
    # Inner shine.
    d.ellipse((cx - r * 0.45, cy + r * 0.05, cx - r * 0.15, cy + r * 0.4),
              fill=(255, 255, 255, 80))


def walk_glyph(d: ImageDraw.ImageDraw, s: int, fg: tuple) -> None:
    # Footprint: rounded rectangle (sole) + 5 dots (toes).
    pad = s * 0.25
    sole_w = s - 2 * pad
    sole_h = s * 0.42
    sole_x = pad
    sole_y = s * 0.4
    d.rounded_rectangle((sole_x, sole_y, sole_x + sole_w, sole_y + sole_h),
                        radius=int(s * 0.12), fill=fg)
    # Toes — a row above the sole.
    toe_r = s * 0.045
    toe_y = sole_y - toe_r * 1.6
    for i, frac in enumerate([0.18, 0.36, 0.54, 0.72, 0.86]):
        cx = sole_x + sole_w * frac
        d.ellipse((cx - toe_r, toe_y - toe_r,
                   cx + toe_r, toe_y + toe_r), fill=fg)


GLYPHS = {
    "stretch": stretch_glyph,
    "water":   water_glyph,
    "walk":    walk_glyph,
}


def render(name: str, size: int, dst: Path) -> None:
    img = Image.new("RGB", (size, size), TILE_BG)
    d = ImageDraw.Draw(img)
    GLYPHS[name](d, size, PALETTE[name])
    dst.parent.mkdir(parents=True, exist_ok=True)
    img.save(dst, format="BMP")
    print(f"  wrote {dst.relative_to(ROOT)}  {size}x{size}")


def main() -> None:
    print("Generating placeholder icons in", ROOT)
    for name in PALETTE:
        render(name, 64, OUT_BIG   / f"{name}.bmp")
        render(name, 24, OUT_SMALL / f"{name}.bmp")
    print("Done.")


if __name__ == "__main__":
    main()
