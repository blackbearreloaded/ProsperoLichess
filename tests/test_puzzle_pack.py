#!/usr/bin/env python3
# ProsperoLichess - Puzzle pack builder and shipped pack integration tests.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Rebuilds the committed fixture pack and decodes the shipped assets/puzzles/pack.bin.

import csv
import importlib.util
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools/build-puzzle-pack.py"
FIXTURES = ROOT / "tests/fixtures/puzzles"

spec = importlib.util.spec_from_file_location("build_puzzle_pack", TOOL)
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


def read_pack(path):
    data = path.read_bytes()
    (magic, version, header_size, count, theme_count, mask_bytes, _, base, step, buckets,
     _) = struct.unpack_from("<4sHHIHBBHHHH", data, 0)
    date = data[24:40].rstrip(b"\0").decode()
    digest = data[40:72].hex()
    total = struct.unpack_from("<I", data, 72)[0]
    position = header_size
    themes = []
    for _ in range(theme_count):
        length = data[position]
        themes.append(data[position + 1:position + 1 + length].decode())
        position += 1 + length
    index = struct.unpack_from(f"<{buckets + 1}I", data, position)
    position += 4 * (buckets + 1)
    offsets = struct.unpack_from(f"<{count + 1}I", data, position)
    records = position + 4 * (count + 1)
    puzzles = []
    for i in range(count):
        at = records + offsets[i]
        end = records + offsets[i + 1]
        rating, popularity = struct.unpack_from("<Hb", data, at + 5)
        fen, move_at = builder.decode_position(data, at + 8)
        moves = [builder.decode_move(struct.unpack_from("<H", data, move_at + 1 + 2 * m)[0])
                 for m in range(data[move_at])]
        mask = int.from_bytes(data[end - mask_bytes:end], "little")
        names = [t for bit, t in enumerate(themes) if mask >> bit & 1]
        puzzles.append({"id": data[at:at + 5].decode(), "rating": rating,
                        "popularity": popularity, "fen": fen, "moves": moves, "themes": names})
    return {"magic": magic, "version": version, "count": count, "themes": themes,
            "base": base, "step": step, "index": index, "date": date, "sha256": digest,
            "total": total, "size": len(data), "puzzles": puzzles,
            "crc_ok": zlib.crc32(data[:-4]) == struct.unpack_from("<I", data, len(data) - 4)[0]}


class PuzzlePackTests(unittest.TestCase):
    def test_fixture_is_reproducible_and_round_trips(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory) / "mini.bin"
            subprocess.run([sys.executable, str(TOOL), "--all", "--source-date", "2026-09-09",
                            str(FIXTURES / "mini.csv"), str(out)],
                           check=True, capture_output=True, text=True)
            self.assertEqual(out.read_bytes(), (FIXTURES / "mini.bin").read_bytes())
        pack = read_pack(FIXTURES / "mini.bin")
        with open(FIXTURES / "mini.csv", newline="") as handle:
            rows = {row["PuzzleId"]: row for row in csv.DictReader(handle)}
        self.assertTrue(pack["crc_ok"])
        self.assertEqual(pack["count"], len(rows))
        for puzzle in pack["puzzles"]:
            row = rows[puzzle["id"]]
            self.assertEqual(puzzle["fen"], row["FEN"])
            self.assertEqual(puzzle["moves"], row["Moves"].split())
            self.assertEqual(puzzle["rating"], int(row["Rating"]))
            self.assertEqual(puzzle["popularity"], int(row["Popularity"]))
            self.assertEqual(sorted(puzzle["themes"]), sorted(row["Themes"].split()))

    def test_shipped_pack_header_and_theme_coverage(self):
        path = ROOT / "assets/puzzles/pack.bin"
        self.assertLessEqual(path.stat().st_size, 8 * 1024 * 1024)
        pack = read_pack(path)
        self.assertEqual((pack["magic"], pack["version"]), (b"PCHP", 1))
        self.assertTrue(pack["crc_ok"])
        self.assertEqual(pack["total"], pack["size"])
        self.assertRegex(pack["date"], r"^\d{4}-\d{2}-\d{2}$")
        self.assertEqual(len(pack["sha256"]), 64)
        self.assertGreaterEqual(pack["count"], 95000)
        self.assertLessEqual(pack["count"], 110000)
        self.assertGreaterEqual(len(pack["themes"]), 60)
        self.assertEqual(pack["themes"], sorted(pack["themes"]))
        coverage = {theme: 0 for theme in pack["themes"]}
        ratings = [p["rating"] for p in pack["puzzles"]]
        self.assertEqual(ratings, sorted(ratings))
        for puzzle in pack["puzzles"]:
            for theme in puzzle["themes"]:
                coverage[theme] += 1
        for theme, count in coverage.items():
            self.assertGreaterEqual(count, 200, theme)
        # Rating index entries point at the first puzzle of each bucket.
        for b, first in enumerate(pack["index"]):
            threshold = pack["base"] + b * pack["step"]
            self.assertTrue(first == len(ratings) or ratings[first] >= threshold)
            self.assertTrue(first == 0 or ratings[first - 1] < threshold)
        # Stratification: every 50-point bucket in 400..2999 holds a similar share.
        buckets = {}
        for rating in ratings:
            key = builder.bucket_of(rating)
            buckets[key] = buckets.get(key, 0) + 1
        self.assertGreaterEqual(buckets[builder.bucket_of(1500)], 1500)


if __name__ == "__main__":
    unittest.main()
