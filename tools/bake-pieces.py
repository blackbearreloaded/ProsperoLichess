#!/usr/bin/env python3
# ProsperoLichess - Rasterizes chess piece SVG sets into RGBA atlases.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
"""bake-pieces.py <pieces dir> <out dir>

Every <set>/{w,b}{K,Q,R,B,N,P}.svg under <pieces dir> becomes
<out dir>/<set>-<cell>.pcha for each cell size. Layout: 6 columns (K Q R B N P)
by 2 rows (white, black). Colour is bled into transparent texels so bilinear
filtering never pulls in black fringes (the batch blends straight alpha).

File format (little-endian): "PCHA" | u16 width | u16 height | u16 cell |
u16 reserved | width*height*4 RGBA bytes.
"""

import io
import re
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image
import resvg_py

ROLES = "KQRBNP"
CELLS = (128, 256)


def render(svg_path, cell):
    svg = svg_path.read_text()
    # Sets carry a viewBox and sometimes a physical size (mm); replace any
    # root size with the cell size so every piece fills its cell alike.
    head, rest = svg.split(">", 1)
    head = re.sub(r'\s(width|height)="[^"]*"', "", head)
    svg = head.replace("<svg", f'<svg width="{cell}" height="{cell}"', 1) + ">" + rest
    png = bytes(resvg_py.svg_to_bytes(svg_string=svg))
    image = Image.open(io.BytesIO(png)).convert("RGBA")
    if image.size != (cell, cell):
        image = image.resize((cell, cell), Image.LANCZOS)
    return np.asarray(image, dtype=np.float32)


def bleed(atlas):
    # Repeatedly spread the colour of opaque texels into transparent
    # neighbours (alpha stays 0), so filtering at edges keeps the piece colour.
    rgb = atlas[..., :3].copy()
    alpha = atlas[..., 3]
    known = alpha > 0
    for _ in range(8):
        if known.all():
            break
        acc = np.zeros_like(rgb)
        count = np.zeros(alpha.shape, dtype=np.float32)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, 1), (-1, 1), (1, -1)):
            shifted_known = np.roll(np.roll(known, dy, 0), dx, 1)
            shifted_rgb = np.roll(np.roll(rgb, dy, 0), dx, 1)
            acc += shifted_rgb * shifted_known[..., None]
            count += shifted_known
        grow = (~known) & (count > 0)
        rgb[grow] = acc[grow] / count[grow][:, None]
        known = known | grow
    out = atlas.copy()
    out[..., :3] = rgb
    return out


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    src = Path(sys.argv[1])
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    for set_dir in sorted(p for p in src.iterdir() if p.is_dir()):
        for cell in CELLS:
            atlas = np.zeros((cell * 2, cell * 6, 4), dtype=np.float32)
            for row, colour in enumerate("wb"):
                for col, role in enumerate(ROLES):
                    atlas[row * cell:(row + 1) * cell, col * cell:(col + 1) * cell] = render(
                        set_dir / f"{colour}{role}.svg", cell)
            pixels = np.clip(bleed(atlas), 0, 255).astype(np.uint8)
            height, width = pixels.shape[:2]
            path = out / f"{set_dir.name}-{cell}.pcha"
            with path.open("wb") as f:
                f.write(b"PCHA" + struct.pack("<HHHH", width, height, cell, 0))
                f.write(pixels.tobytes())
            print(f"{path} {width}x{height}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
