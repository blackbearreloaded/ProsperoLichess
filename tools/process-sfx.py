#!/usr/bin/env python3
# ProsperoLichess - Turns raw generated sound effects into game-ready cues.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
"""process-sfx.py <raw dir> <out dir>

Each raw file is named <cue>_NN.<ext> (any format ffmpeg reads). The output is
a 48 kHz 16-bit WAV per file that meets PLAN.md Appendix A: leading silence
removed, cut to the cue's length, a clean fade to silence, and a peak level by
category (quiet ticks sit well below stings). Needs ffmpeg and numpy.
"""

import subprocess
import sys
import wave
from pathlib import Path

import numpy as np

RATE = 48000

# cue -> (max seconds, loudness category, stereo). Categories are RMS targets
# in dBFS (quiet ticks sit well below stings, per Appendix A); the peak never
# goes above -1 dBFS (-8 for the quiet ticks).
QUIET, UI, PLAY, STING = -33.0, -27.0, -24.0, -19.0
CUES = {
    # Board and clock.
    "move": (0.2, PLAY, False), "capture": (0.2, PLAY, False),
    "pickup": (0.2, PLAY, False), "check": (0.35, PLAY, False),
    "castle": (0.35, PLAY, False), "illegal": (0.35, PLAY, False),
    "promote": (0.6, PLAY, False), "cursor": (0.05, QUIET, False),
    "premove": (0.2, UI, False), "clock_tick": (0.06, QUIET, False),
    "low_time": (0.6, PLAY, False),
    # Puzzles.
    "puzzle_correct": (0.6, PLAY, False), "puzzle_wrong": (0.6, PLAY, False),
    "combo": (0.6, PLAY, False), "puzzle_solved": (1.5, STING, True),
    "streak_end": (3.0, STING, True), "new_record": (3.0, STING, True),
    # Game results.
    "game_start": (1.0, STING, True), "victory": (3.0, STING, True),
    "defeat": (3.0, STING, True), "draw": (3.0, STING, True),
    # Interface (the UI kit's cue names; see src/audio/cues.hpp).
    "focus": (0.05, QUIET, False), "select": (0.35, UI, False),
    "back": (0.35, UI, False), "tab": (0.35, UI, False),
    "launch": (1.0, UI, True), "modal_open": (0.6, UI, False),
    "modal_close": (0.6, UI, False), "toggle": (0.2, UI, False),
    "slider": (0.06, QUIET, False), "error": (0.35, UI, False),
    "notify": (1.0, UI, True), "challenge": (1.0, UI, True),
    # Interface cues the chess set does not record: the kit's own recordings
    # (assets/audio/sfx/glass and two from its paper set) fill them.
    "open": (0.6, UI, False), "resume": (0.6, UI, False),
    "saved": (1.0, UI, True), "welcome": (2.0, UI, True),
    "tick": (0.06, QUIET, False), "type": (0.2, UI, False),
}


def decode(path, channels, highpass):
    # The high-pass removes DC drift and sub-bass rumble that generated clips
    # often start with; it would otherwise fool the trim and the level.
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", str(path), "-af",
                          f"highpass=f={highpass},highpass=f={highpass}", "-ac", str(channels),
                          "-ar", str(RATE), "-f", "f32le", "-"],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.float32).reshape(-1, channels).copy()


def db(x):
    return 20 * np.log10(max(x, 1e-9))


def process(path, out_dir):
    stem = path.stem
    cue = stem.rsplit("_", 1)[0]
    if cue not in CUES:
        print(f"skip {path.name}: unknown cue")
        return
    max_s, rms_db, stereo = CUES[cue]
    # Ticks live in the mids and highs; a thump below 250 Hz is lost on a TV.
    ticks = {"focus", "slider", "toggle", "cursor", "tick", "clock_tick", "premove"}
    highpass = 35 if rms_db == STING else 250 if cue in ticks else 90
    audio = decode(path, 2 if stereo else 1, highpass)
    level = np.abs(audio).max(axis=1)
    peak = level.max()
    if peak <= 0:
        print(f"skip {path.name}: silent")
        return
    # Start at the attack: the first 1 ms window whose energy reaches a quarter
    # (-12 dB) of the loudest window, so noise ahead of the hit is dropped.
    hop = RATE // 1000
    frames = len(level) // hop
    rms = np.sqrt((level[:frames * hop].reshape(frames, hop) ** 2).mean(axis=1))
    onset = int(np.nonzero(rms >= rms.max() * 10 ** (-12 / 20))[0][0]) * hop
    start = max(0, onset - int(0.003 * RATE))
    # End after the last audible sound, capped at the cue's length.
    audible = np.nonzero(level > peak * 10 ** (-50 / 20))[0]
    end = min(audible[-1] + int(0.02 * RATE), start + int(max_s * RATE), len(audio))
    audio = audio[start:end]
    n = len(audio)
    # A short fade in (no click) and a fade out that reaches digital silence.
    fade_in = min(int(0.002 * RATE), n // 4)
    fade_out = min(n // 2 if n < 0.3 * RATE else int(0.25 * n), int(0.4 * RATE))
    env = np.ones(n, dtype=np.float32)
    if fade_in > 0:
        env[:fade_in] = np.linspace(0, 1, fade_in, dtype=np.float32)
    if fade_out > 0:
        env[n - fade_out:] = np.linspace(1, 0, fade_out, dtype=np.float32) ** 2
    audio *= env[:, None]
    # Loudness by category, measured over the audible part so a long fade does
    # not count as quiet; the peak cap wins when the two disagree.
    mono = np.abs(audio).max(axis=1)
    active = audio[mono > mono.max() * 10 ** (-30 / 20)]
    rms = float(np.sqrt((active ** 2).mean()))
    peak_cap = 10 ** ((-8.0 if rms_db == QUIET else -1.0) / 20)
    gain = min(10 ** (rms_db / 20) / max(rms, 1e-9), peak_cap / max(float(np.abs(audio).max()), 1e-9))
    audio *= gain
    pcm = np.clip(np.round(audio * 32767), -32768, 32767).astype("<i2")
    out = out_dir / f"{stem}.wav"
    with wave.open(str(out), "wb") as w:
        w.setnchannels(pcm.shape[1])
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    print(f"{stem}: {n / RATE * 1000:.0f} ms, peak {db(np.abs(audio).max()):.1f} dBFS, "
          f"{'stereo' if stereo else 'mono'}")


def main():
    raw_dir, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)
    for path in sorted(raw_dir.iterdir()):
        if path.is_file():
            process(path, out_dir)


if __name__ == "__main__":
    main()
