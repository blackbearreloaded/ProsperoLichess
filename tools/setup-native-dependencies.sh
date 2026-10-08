#!/usr/bin/env bash
# ps5-native-app-boilerplate - Linux/WSL native dependency bootstrapper.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Fetches the public PS5 payload SDK, static zlib and the HarfBuzz source
# into the ignored cache.

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cache="$root/.deps/native"
sdk="$cache/ps5-payload-sdk"
zlib_directory="$cache/zlib"
zlib_root="$zlib_directory/root"
zlib_version="1.3.2"
zlib_source="$zlib_directory/zlib-$zlib_version"
zlib_archive="$zlib_directory/zlib-$zlib_version.tar.gz"
zlib_stamp="$zlib_root/.source-version"
sdk_url="https://github.com/ps5-payload-dev/sdk/releases/download/v0.42/ps5-payload-sdk.zip"
sdk_hash="8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da"
zlib_url="https://zlib.net/fossils/zlib-$zlib_version.tar.gz"
zlib_hash="bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16"
harfbuzz_version="12.3.2"
harfbuzz_directory="$cache/harfbuzz"
harfbuzz_source="$harfbuzz_directory/harfbuzz-$harfbuzz_version"
harfbuzz_archive="$harfbuzz_directory/harfbuzz-$harfbuzz_version.tar.xz"
harfbuzz_url="https://github.com/harfbuzz/harfbuzz/releases/download/$harfbuzz_version/harfbuzz-$harfbuzz_version.tar.xz"
harfbuzz_hash="6f6db164359a2da5a84ef826615b448b33e6306067ad829d85d5b0bf936f1bb8"
lapy_commit="c3bdfe3a399366d8eacfc580f20b19fd03b16ca3"
lapy_source="$root/.deps/PS5-Lapy-JB-Daemon-c3bdfe3"
lapy_sdk="$root/.deps/lapy-ps5-payload-sdk-v0.42"
lapy_sdk_archive="$root/.deps/lapy-ps5-payload-sdk-v0.42.zip"
lapy_sdk_url="https://github.com/ps5-payload-dev/sdk/releases/download/v0.42/ps5-payload-sdk.zip"
lapy_sdk_hash="8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da"
lapy_log="$root/.deps/lapy-ps5log-1ae1f918/ps5log.h"
lapy_log_url="https://raw.githubusercontent.com/mpereiraesaa/ps5-agc-gears/1ae1f9182abd2770c131b97419034fb85173c2dc/native/ps5log/ps5log.h"
lapy_log_hash="394af67d0f8b60b3335deb53396e52855ea2daa50ca914a456ea7663f48900c6"
skip_sdk=false

if [[ ${1:-} == "--skip-sdk" ]]; then
    skip_sdk=true
elif [[ $# -ne 0 ]]; then
    echo "usage: tools/setup-native-dependencies.sh [--skip-sdk]" >&2
    exit 2
fi

for command in wget unzip sha256sum tar make; do
    command -v "$command" >/dev/null || {
        echo "missing required command: $command" >&2
        exit 2
    }
done
compiler=$(command -v clang-18 || command -v clang || true)
archiver=$(command -v llvm-ar-18 || command -v llvm-ar || command -v ar || true)
ranlib=$(command -v llvm-ranlib-18 || command -v llvm-ranlib || command -v ranlib || true)
[[ -n $compiler && -n $archiver && -n $ranlib ]] || {
    echo "clang, an archive tool, and ranlib are required to build zlib" >&2
    exit 2
}

mkdir -p "$cache"
if ! $skip_sdk && [[ ! -x "$sdk/bin/prospero-lld" ]]; then
    archive="$cache/ps5-payload-sdk.zip"
    temporary="$archive.download"
    wget -q "$sdk_url" -O "$temporary"
    printf '%s  %s\n' "$sdk_hash" "$temporary" | sha256sum --check --strict
    mv "$temporary" "$archive"
    unzip -q -o "$archive" -d "$cache"
fi
if ! $skip_sdk && [[ ! -x "$sdk/bin/prospero-lld" || ! -d "$sdk/target/include" ]]; then
    echo "the pinned PS5 payload SDK is incomplete" >&2
    exit 2
fi

zlib_library=$(find "$zlib_root" -type f -name libz.a -print -quit 2>/dev/null || true)
if [[ -z $zlib_library || ! -f $zlib_root/usr/include/zlib.h ||
    ! -f $zlib_stamp || $(<"$zlib_stamp") != "$zlib_version" ]]; then
    echo "==> [deps] Building pinned zlib $zlib_version from source" >&2
    mkdir -p "$zlib_directory"
    if [[ -f $zlib_archive ]] &&
        ! printf '%s  %s\n' "$zlib_hash" "$zlib_archive" |
            sha256sum --check --strict >/dev/null 2>&1; then
        rm -f -- "$zlib_archive"
    fi
    if [[ ! -f $zlib_archive ]]; then
        wget -q "$zlib_url" -O "$zlib_archive.download"
        mv "$zlib_archive.download" "$zlib_archive"
    fi
    printf '%s  %s\n' "$zlib_hash" "$zlib_archive" | sha256sum --check --strict >/dev/null
    rm -rf -- "$zlib_source" "$zlib_root"
    tar -xzf "$zlib_archive" -C "$zlib_directory"
    mkdir -p "$zlib_root"
    jobs=${BUILD_JOBS:-$(nproc 2>/dev/null || printf '2')}
    (
        cd "$zlib_source"
        CC="$compiler" AR="$archiver" RANLIB="$ranlib" ./configure --static --prefix=/usr
        make -j "$jobs" CC="$compiler" AR="$archiver" RANLIB="$ranlib"
        make DESTDIR="$zlib_root" install
    ) >"$zlib_directory/build.log"
    printf '%s\n' "$zlib_version" >"$zlib_stamp"
    zlib_library=$(find "$zlib_root" -type f -name libz.a -print -quit 2>/dev/null || true)
fi
if [[ -z $zlib_library ]]; then
    echo "the pinned native zlib archive was not found after compilation" >&2
    exit 2
fi

# HarfBuzz is compiled into the app from its single-file source
# (src/third_party/harfbuzz): only src/ and the licence are kept.
if [[ ! -f $harfbuzz_source/src/harfbuzz.cc || ! -f $harfbuzz_source/COPYING ]]; then
    echo "==> [deps] Fetching pinned HarfBuzz $harfbuzz_version source" >&2
    mkdir -p "$harfbuzz_directory"
    if [[ -f $harfbuzz_archive ]] &&
        ! printf '%s  %s\n' "$harfbuzz_hash" "$harfbuzz_archive" |
            sha256sum --check --strict >/dev/null 2>&1; then
        rm -f -- "$harfbuzz_archive"
    fi
    if [[ ! -f $harfbuzz_archive ]]; then
        wget -q "$harfbuzz_url" -O "$harfbuzz_archive.download"
        mv "$harfbuzz_archive.download" "$harfbuzz_archive"
    fi
    printf '%s  %s\n' "$harfbuzz_hash" "$harfbuzz_archive" | sha256sum --check --strict >/dev/null
    rm -rf -- "$harfbuzz_source"
    mkdir -p "$harfbuzz_source"
    tar -xJf "$harfbuzz_archive" -C "$harfbuzz_source" --strip-components=1 \
        "harfbuzz-$harfbuzz_version/src" "harfbuzz-$harfbuzz_version/COPYING"
fi
if [[ ! -f $harfbuzz_source/src/harfbuzz.cc ]]; then
    echo "the pinned HarfBuzz source was not found after extraction" >&2
    exit 2
fi

if ! $skip_sdk && [[ ! -f $lapy_source/source/lapy_elevation_protocol.h ]]; then
    git clone -q https://github.com/blackbearreloaded/PS5-Lapy-JB-Daemon.git "$lapy_source"
    git -C "$lapy_source" fetch -q origin "$lapy_commit"
    git -C "$lapy_source" checkout -q --detach "$lapy_commit"
fi
if ! $skip_sdk; then
[[ $(git -C "$lapy_source" rev-parse HEAD) == "$lapy_commit" ]] || {
    echo "the pinned Lapy checkout has the wrong commit" >&2; exit 2;
}
if [[ ! -x $lapy_sdk/bin/prospero-clang ]]; then
    wget -q "$lapy_sdk_url" -O "$lapy_sdk_archive.download"
    printf '%s  %s\n' "$lapy_sdk_hash" "$lapy_sdk_archive.download" | sha256sum --check --strict
    rm -rf -- "$lapy_sdk"
    mkdir -p "$lapy_sdk"
    unzip -q "$lapy_sdk_archive.download" -d "$lapy_sdk"
    if [[ -d $lapy_sdk/ps5-payload-sdk ]]; then
        cp -a "$lapy_sdk/ps5-payload-sdk/." "$lapy_sdk/"
        rm -rf -- "$lapy_sdk/ps5-payload-sdk"
    fi
    mv "$lapy_sdk_archive.download" "$lapy_sdk_archive"
fi
if [[ ! -f $lapy_log ]]; then
    mkdir -p "$(dirname "$lapy_log")"
    wget -q "$lapy_log_url" -O "$lapy_log.download"
    printf '%s  %s\n' "$lapy_log_hash" "$lapy_log.download" | sha256sum --check --strict
    mv "$lapy_log.download" "$lapy_log"
fi
fi

printf 'SDK_ROOT=%s\nZLIB_INCLUDE=%s\nZLIB_ARCHIVE=%s\n' \
    "$sdk" "$zlib_root/usr/include" "$zlib_library"
