#!/usr/bin/env bash
# ProsperoLichess - Check on the PC that screens are ready for other languages.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# usage: tools/lang-check.sh [scenario filter]
#
# Renders the PC scenarios whose name contains the filter (all of them without
# one) twice:
#   1. in English, and compares the pictures with those in $PCH_BASELINE when
#      that names a folder of reference pictures: marking text for translation
#      must not change an English screen;
#   2. in the made-up test language (tools/strings.py pseudo), then lists the
#      text that reached the screen unmarked and the lines that had to be cut.
# The pictures are left in build/lang-check/en and build/lang-check/xx.

set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
filter=${1:-}
out="$root/build/lang-check"
rm -rf "$out"
mkdir -p "$out/en" "$out/xx"

echo "== English"
PCH_ONLY="$filter" bash "$root/tools/host-snapshots.sh" "$out/en" > "$out/en.log" 2>&1 ||
    { grep -E 'error' -A8 "$out/en.log" | head -60; exit 1; }
echo "$(grep -c '^wrote' "$out/en.log") pictures"
if [[ -n ${PCH_BASELINE:-} ]]; then
    python3 "$root/tools/compare-snapshots.py" "$PCH_BASELINE" "$out/en" || true
fi

echo "== test language"
python3 "$root/tools/strings.py" pseudo "$out/pseudo.po"
PCH_ONLY="$filter" PCH_LANG=xx PCH_LANG_FILE="$out/pseudo.po" PCH_TEXT_LOG="$out/texts.tsv" \
    bash "$root/tools/host-snapshots.sh" "$out/xx" > "$out/xx.log" 2>&1 ||
    { grep -E 'error' -A8 "$out/xx.log" | head -60; exit 1; }
grep '^language' "$out/xx.log" || true
python3 "$root/tools/strings.py" unmarked "$out/texts.tsv" || true
echo "== lines that had to be cut (they do not fit even when shrunk)"
grep '^cut ' "$out/xx.log" | sort -u | head -80 || true
echo "$(grep -c '^cut ' "$out/xx.log" || true) cut"
