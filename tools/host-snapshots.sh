#!/usr/bin/env bash
# ProsperoLichess - Render app screens on the host (Mesa surfaceless EGL) to PNG.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/tools/ninja-build.sh"
cxx=$(command -v "${HOST_CXX:-clang++}")
cc=$(command -v "${HOST_CC:-clang}")
build="$root/build/host-snapshots"
bash "$root/tools/setup-native-dependencies.sh" --skip-sdk >/dev/null
harfbuzz="$root/.deps/native/harfbuzz/harfbuzz-12.3.2/src"
ninja_begin "$build/build.ninja"

# Every host source: the snapshot driver, its scenarios and the platform stand-ins.
sources=("$root"/host/*.cpp)
# Every platform-neutral application source (the PS5 entry point, platform
# backends and runtime are replaced by the host files above).
while IFS= read -r -d '' source; do
    case $source in
    "$root/src/main.cpp" | "$root"/src/platform/ps5/* | "$root"/src/runtime/* | \
        "$root"/src/net/self_update/*.c) continue ;;
    esac
    sources+=("$source")
done < <(find "$root/src" \( -name '*.cpp' -o -name '*.c' \) -print0 | sort -z)

objects=()
for source in "${sources[@]}"; do
    relative=${source#"$root/"}
    object="$build/obj/${relative//\//_}.o"
    if [[ $source == *.c ]]; then
        ninja_inputs=("$source" "$cc")
        ninja_edge CC "$object" "${compiler_cache[@]}" "$cc" -std=c11 -O2 -w -I"$root/src" \
            -MD -MF "$object.d" -c "$source" -o "$object"
    else
        ninja_inputs=("$source" "$cxx")
        ninja_edge CXX "$object" "${compiler_cache[@]}" "$cxx" -std=c++20 -O2 -Wall -Wextra \
            -DGL_GLEXT_PROTOTYPES=1 -DPCH_HOST=1 -I"$root/src" -I"$root/host" -isystem "$harfbuzz" \
            -MD -MF "$object.d" -c "$source" -o "$object"
    fi
    objects+=("$object")
done
ninja_inputs=("${objects[@]}")
ninja_edge LINK "$build/pch_snapshots" "$cxx" "${objects[@]}" -lEGL -lGL -lcurl -lpthread -lm \
    -o "$build/pch_snapshots"
if ! ninja_run >"$build/build.log" 2>&1; then
    grep -E 'error|FAILED|warning' -A6 "$build/build.log" | head -${PCH_ERROR_LINES:-80} >&2
    exit 1
fi
grep -E 'warning' -A4 "$build/build.log" | head -40 >&2 || true

output=${1:-"$root/build/snapshots"}
mkdir -p "$output"
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    "$build/pch_snapshots" "$root/assets" "$output" "${@:2}"
