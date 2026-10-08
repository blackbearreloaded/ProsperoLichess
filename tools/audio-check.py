#!/usr/bin/env python3
# ProsperoLichess - Validates delivered sound effects and music (PLAN.md Appendix A).
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks assets/audio/sfx/<set>/*.wav and assets/audio/music/*.ogg.

Errors (the file would be rejected or misplayed) make the exit status 1.
Warnings point at spec targets such as length, loudness and fades; they are
advice for the mix, not failures. Music loudness needs ffmpeg.
"""

import array
import json
import math
import importlib.util
import re
import shutil
import subprocess
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SFX = ROOT / "assets/audio/sfx"
MUSIC = ROOT / "assets/audio/music"

# Cue -> (min seconds, max seconds): the maximum lengths come from the cue
# table in tools/process-sfx.py, which trims every generated sound.
def load_cues():
    spec = importlib.util.spec_from_file_location("process_sfx", ROOT / "tools/process-sfx.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return {cue: (0.02, limit) for cue, (limit, _level, _stereo) in module.CUES.items()}


CUES = load_cues()
# Every .ogg in the music folder joins the shuffled playlist; keep names simple.
MUSIC_NAMES = re.compile(r"^[A-Za-z0-9][A-Za-z0-9 _.-]*$")
SFX_NAME = re.compile(r"^(?P<cue>[a-z_]+?)(?:_(?P<n>\d\d))?$")
# One folder per sound set (audio::SoundSet in src/audio/cues.hpp): the app's
# own recordings, and the UI kit's set that fills the interface cues ours lack.
SETS = ("chess", "glass")


def dbfs(value):
    return 20 * math.log10(value) if value > 0 else -math.inf


class Report:
    def __init__(self):
        self.errors = 0
        self.warnings = 0

    def error(self, path, text):
        self.errors += 1
        print(f"ERROR {path.name}: {text}")

    def warn(self, path, text):
        self.warnings += 1
        print(f"warn  {path.name}: {text}")


def read_wav(path):
    with wave.open(str(path), "rb") as w:
        rate, channels, width, frames = (w.getframerate(), w.getnchannels(), w.getsampwidth(),
                                         w.getnframes())
        raw = w.readframes(frames)
    if width == 2:
        samples = array.array("h", raw)
        scale = 32768.0
    elif width == 3:
        samples = array.array("i", (int.from_bytes(raw[i:i + 3], "little", signed=True)
                                    for i in range(0, len(raw), 3)))
        scale = 8388608.0
    else:
        samples = array.array("h")
        scale = 1.0
    return rate, channels, width, frames, samples, scale


def check_sfx(report):
    files = []
    for name in SETS:
        files += sorted((SFX / name).glob("*.wav"))
    for stray in sorted(SFX.glob("*.wav")) if SFX.is_dir() else []:
        report.error(stray, "sound effects belong in a set folder (sfx/chess, sfx/glass)")
    for path in files:
        match = SFX_NAME.match(path.stem)
        if not match or match.group("cue") not in CUES:
            report.error(path, "unknown cue name (see tools/process-sfx.py)")
            continue
        try:
            rate, channels, width, frames, samples, scale = read_wav(path)
        except (wave.Error, EOFError) as exc:
            report.error(path, f"not a PCM WAV file ({exc})")
            continue
        if rate != 48000:
            report.error(path, f"{rate} Hz (need 48000)")
        if channels not in (1, 2):
            report.error(path, f"{channels} channels (need 1 or 2)")
        if width not in (2, 3):
            report.error(path, f"{width * 8}-bit (need 16 or 24)")
            continue
        if frames == 0:
            report.error(path, "no audio")
            continue
        seconds = frames / rate
        low, high = CUES[match.group("cue")]
        # The kit's set was mastered to its own targets: only ours is held to the table.
        mine = path.parent.name == "chess"
        if mine and (seconds < low * 0.5 or seconds > high * 1.5):
            report.warn(path, f"{seconds:.3f} s (target {low}-{high} s)")
        peak = max(abs(s) for s in samples) / scale
        if dbfs(peak) > -1.0:
            report.warn(path, f"peak {dbfs(peak):.1f} dBFS (keep at or below -1)")
        threshold = scale * 10 ** (-60 / 20)
        lead = next((i for i, s in enumerate(samples) if abs(s) > threshold), len(samples))
        lead_ms = lead / channels / rate * 1000
        if lead_ms > 5:
            report.warn(path, f"{lead_ms:.1f} ms of leading silence (keep under 5 ms)")
        tail = samples[-max(1, int(rate * 0.005) * channels):]
        if dbfs(max(abs(s) for s in tail) / scale) > -40:
            report.warn(path, "tail is not faded to silence")
    return len(files)


def probe(path):
    out = subprocess.run(["ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json",
                          str(path)], capture_output=True, text=True, check=False)
    return json.loads(out.stdout or "{}")


def loudness(path):
    out = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", str(path), "-filter:a",
                          "ebur128=peak=true", "-f", "null", "-"], capture_output=True, text=True,
                         check=False).stderr
    integrated = re.findall(r"I:\s+(-?[\d.]+) LUFS", out)
    peak = re.findall(r"Peak:\s+(-?[\d.]+) dBFS", out)
    return (float(integrated[-1]) if integrated else None, float(peak[-1]) if peak else None)


def check_music(report):
    files = sorted(MUSIC.glob("*.ogg")) if MUSIC.is_dir() else []
    have_ffmpeg = shutil.which("ffprobe") and shutil.which("ffmpeg")
    if files and not have_ffmpeg:
        print("note  ffmpeg/ffprobe not found: music is checked by name only")
    for path in files:
        if not MUSIC_NAMES.match(path.stem):
            report.error(path, "use letters, digits, spaces, '.', '_' or '-' in song names")
        if not have_ffmpeg:
            continue
        info = probe(path)
        streams = [s for s in info.get("streams", []) if s.get("codec_type") == "audio"]
        if not streams or streams[0].get("codec_name") != "vorbis":
            report.error(path, "not an OGG Vorbis stream")
            continue
        stream = streams[0]
        if int(stream.get("sample_rate", 0)) != 48000:
            report.error(path, f"{stream.get('sample_rate')} Hz (need 48000)")
        if int(stream.get("channels", 0)) not in (1, 2):
            report.error(path, f"{stream.get('channels')} channels (need 1 or 2)")
        elif int(stream.get("channels", 0)) == 1:
            report.warn(path, "mono (stereo is expected for music)")
        seconds = float(info.get("format", {}).get("duration", 0))
        if not 60 <= seconds <= 480:
            report.warn(path, f"{seconds:.0f} s long (songs are usually 1-8 minutes)")
        tags = {k.upper(): v for k, v in info.get("format", {}).get("tags", {}).items()}
        tags.update({k.upper(): v for k, v in stream.get("tags", {}).items()})
        if "LOOPLENGTH" in tags and "LOOPSTART" not in tags:
            report.error(path, "LOOPLENGTH without LOOPSTART")
        integrated, peak = loudness(path)
        if integrated is not None and abs(integrated + 18) > 2:
            report.warn(path, f"{integrated:.1f} LUFS integrated (target -18)")
        if peak is not None and peak > -1.0:
            report.warn(path, f"true peak {peak:.1f} dBTP (keep at or below -1)")
    return len(files)


def main():
    report = Report()
    sfx = check_sfx(report)
    music = check_music(report)
    print(f"audio-check: {sfx} sound effect(s), {music} music track(s), "
          f"{report.errors} error(s), {report.warnings} warning(s)")
    return 1 if report.errors else 0


if __name__ == "__main__":
    sys.exit(main())
