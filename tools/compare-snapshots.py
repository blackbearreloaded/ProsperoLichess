#!/usr/bin/env python3
# ProsperoLichess - Compare two folders of PC pictures of the app's screens.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
"""compare-snapshots.py <reference folder> <folder>

Compares every picture of <folder> with the one of the same name in the reference: how many are
identical and, for each that differs, how many pixels and by how much at most. The sign-in screens
draw a QR code that is made anew at each run, so they always differ. Exits 1 when another picture
differs by more than a few pixels of rounding.
"""

import os
import sys

from PIL import Image, ImageChops

# A code that changes at every run is drawn on these.
EXPECTED = ("signin",)
# Rounding in the font atlas moves a handful of pixels by a few levels.
FEW_PIXELS = 80
SMALL_CHANGE = 40


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    reference, folder = sys.argv[1], sys.argv[2]
    names = sorted(n for n in os.listdir(folder) if n.endswith(".png"))
    same = rounding = 0
    changed = []
    missing = []
    for name in names:
        other = os.path.join(reference, name)
        if not os.path.exists(other):
            missing.append(name)
            continue
        path = os.path.join(folder, name)
        with open(path, "rb") as a, open(other, "rb") as b:
            if a.read() == b.read():
                same += 1
                continue
        one = Image.open(other).convert("RGB")
        two = Image.open(path).convert("RGB")
        if one.size != two.size:
            changed.append(f"{name}: sizes differ")
            continue
        difference = ImageChops.difference(one, two)
        box = difference.getbbox()
        if box is None:
            same += 1
            continue
        red, green, blue = difference.split()
        levels = ImageChops.lighter(ImageChops.lighter(red, green), blue).histogram()
        pixels = sum(levels[1:])
        worst = max(level for level, count in enumerate(levels) if count)
        if name.startswith(EXPECTED):
            continue
        if pixels <= FEW_PIXELS and worst <= SMALL_CHANGE:
            rounding += 1
        else:
            changed.append(f"{name}: {pixels} pixels differ, by {worst} at most, inside {box}")
    for line in changed:
        print("  CHANGED", line)
    for name in missing[:20]:
        print("  new (no reference):", name)
    print(f"{same} identical, {rounding} within rounding, {len(changed)} changed, {len(missing)} new")
    sys.exit(1 if changed else 0)


if __name__ == "__main__":
    main()
