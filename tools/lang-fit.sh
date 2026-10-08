#!/usr/bin/env bash
# ProsperoLichess - List the text of a catalog that does not fit its place on screen.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# usage: tools/lang-fit.sh <tag> [scenario filter]
#
# Renders the PC scenarios in the language of assets/lang/<tag>.po and lists
# every line that had to be cut (it ended in dots because it is wider than its
# place even when shrunk), with the scenarios that show it. Shorten those
# translations. The pictures are left in build/lang-fit/<tag>.
#
# The pictures tool is run as it was last built (tools/host-snapshots.sh builds
# it), so several languages can be rendered at the same time. For Japanese,
# Korean, Chinese, Thai and Arabic set PCH_SYSTEM_FONTS to a folder with copies
# of the console's fonts.

set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -ge 1 ]] || { sed -n '6,17p' "${BASH_SOURCE[0]}" >&2; exit 2; }
tag=$1
filter=${2:-}
tool="$root/build/host-snapshots/pch_snapshots"
[[ -x $tool ]] || { echo "build the pictures tool first: tools/host-snapshots.sh <folder>" >&2; exit 2; }
out="$root/build/lang-fit/$tag"
rm -rf "$out"
mkdir -p "$out"
PCH_ONLY="$filter" PCH_LANG="$tag" EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 \
    GALLIUM_DRIVER=llvmpipe "$tool" "$root/assets" "$out" > "$out/log.txt" 2>&1 ||
    { tail -5 "$out/log.txt"; exit 1; }
grep '^language' "$out/log.txt"
echo "$(grep -c '^wrote' "$out/log.txt") pictures in build/lang-fit/$tag"
python3 - "$out/log.txt" <<'PY'
import sys
found = {}
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    if line.startswith("cut "):
        scenario, _, text = line[4:].rstrip("\n").partition(": ")
        found.setdefault(text, []).append(scenario)
for text in sorted(found):
    scenarios = sorted(set(found[text]))
    print(f"cut: {text}   ({', '.join(scenarios[:3])}{'...' if len(scenarios) > 3 else ''})")
print(f"{len(found)} lines cut")
PY
