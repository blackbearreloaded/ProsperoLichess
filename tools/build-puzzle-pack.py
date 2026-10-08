#!/usr/bin/env python3
# ProsperoLichess - Builds the offline Lichess puzzle pack (assets/puzzles/pack.bin).
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# usage: tools/build-puzzle-pack.py [options] <lichess_db_puzzle.csv[.zst]> <out.bin>
#
# Input: the CC0 Lichess puzzle database (https://database.lichess.org/),
# columns PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,...
# FEN is the position BEFORE the opponent's move; Moves[0] is that move and the
# player's solution starts at Moves[1].
#
# Selection (default mode, fully deterministic; order keys are the first 8
# bytes of SHA-256(PuzzleId), never a time-seeded RNG):
#   1. keep rows with Popularity >= 80, NbPlays >= 300, RatingDeviation <= 90;
#   2. stratify into 50-point rating buckets 400..2999 (ratings outside are
#      clamped into the first/last bucket) and take the lowest-hash rows of each
#      bucket with water-filled quotas so buckets are as equal as possible and
#      the total is --target (default 100000);
#   3. for every theme present in the source (alphabetical order), top up with
#      the lowest-hash unselected filtered rows carrying it until the pack holds
#      >= --theme-min (200) of them; if the filtered pool runs out, use relaxed
#      rows (Popularity >= 50, NbPlays >= 100, RatingDeviation <= 120).
# --all skips selection and packs every row of the input (test fixtures).
#
# Output format, version 1, all integers little-endian:
#   header (76 bytes)
#     0  char[4]  magic "PCHP"
#     4  u16      format version (1)
#     6  u16      header size (76)
#     8  u32      puzzle count N
#    12  u16      theme count T (<= 128)
#    14  u8       theme mask bytes M = ceil(T / 8)
#    15  u8       reserved (0)
#    16  u16      rating index base (multiple of the step)
#    18  u16      rating index step (50)
#    20  u16      rating index bucket count B
#    22  u16      reserved (0)
#    24  char[16] source date "YYYY-MM-DD", NUL padded
#    40  u8[32]   SHA-256 of the source file
#    72  u32      total file size in bytes
#   theme table: T x (u8 length, camelCase name bytes); theme i is bit i
#   rating index: (B + 1) x u32; entry b = first puzzle index whose rating is
#     >= base + b * step (entry B = N); puzzles are sorted by rating
#   record offsets: (N + 1) x u32 relative to the first record; [N] = blob size
#   records, each:
#     char[5]  PuzzleId
#     u16      rating
#     i8       popularity (-100..100)
#     u8       flags: bit0 black to move, bit1 K, bit2 Q, bit3 k, bit4 q castling
#     u8       en-passant square (a1=0 .. h8=63) or 0xFF
#     u8       halfmove clock
#     u16      fullmove number
#     u64      occupancy, bit s set when square s (a1=0, b1=1, .., h8=63) holds a piece
#     nibbles  one per occupied square in ascending square order, low nibble
#              first, padded with 0 to a whole byte: 1..6 = white PNBRQK,
#              9..14 = black pnbrqk
#     u8       move count (>= 2)
#     u16[]    UCI moves: from | to << 6 | promotion << 12 (0 none, 1 n, 2 b, 3 r, 4 q)
#     u8[M]    theme bitmask
#   u32 CRC-32 (IEEE, zlib) of every preceding byte

import argparse
import csv
import datetime
import hashlib
import io
import os
from pathlib import Path
import struct
import sys
import zlib

MAGIC = b"PCHP"
VERSION = 1
HEADER_SIZE = 76
MAX_THEMES = 128
RATING_STEP = 50
BUCKET_LOW, BUCKET_HIGH = 400, 3000
PIECES = "PNBRQK"
PROMOTIONS = " nbrq"
CASTLING = "KQkq"


class PackError(Exception):
    pass


def open_rows(path):
    raw = open(path, "rb")
    if str(path).endswith(".zst"):
        import zstandard

        stream = zstandard.ZstdDecompressor().stream_reader(raw)
    else:
        stream = raw
    reader = csv.reader(io.TextIOWrapper(stream, encoding="utf-8", newline=""))
    header = next(reader)
    if header[:8] != ["PuzzleId", "FEN", "Moves", "Rating", "RatingDeviation", "Popularity",
                      "NbPlays", "Themes"]:
        raise PackError(f"unexpected CSV header: {header}")
    return raw, reader


def order_key(puzzle_id):
    return int.from_bytes(hashlib.sha256(puzzle_id.encode()).digest()[:8], "big")


def square(name):
    if len(name) != 2 or name[0] not in "abcdefgh" or name[1] not in "12345678":
        raise PackError(f"bad square {name!r}")
    return (ord(name[0]) - 97) + 8 * (ord(name[1]) - 49)


def square_name(index):
    return "abcdefgh"[index % 8] + "12345678"[index // 8]


def encode_move(uci):
    if len(uci) not in (4, 5):
        raise PackError(f"bad move {uci!r}")
    promotion = 0
    if len(uci) == 5:
        promotion = PROMOTIONS.find(uci[4])
        if promotion <= 0:
            raise PackError(f"bad promotion in {uci!r}")
    return square(uci[:2]) | square(uci[2:4]) << 6 | promotion << 12


def decode_move(value):
    text = square_name(value & 63) + square_name(value >> 6 & 63)
    return text + (PROMOTIONS[value >> 12] if value >> 12 else "")


def encode_position(fen):
    fields = fen.split(" ")
    if len(fields) != 6:
        raise PackError(f"bad FEN {fen!r}")
    placement, side, castling, ep, halfmove, fullmove = fields
    board = [0] * 64
    ranks = placement.split("/")
    if len(ranks) != 8:
        raise PackError(f"bad FEN board {fen!r}")
    for row, text in enumerate(ranks):
        file = 0
        for char in text:
            if char.isdigit():
                file += int(char)
            else:
                code = PIECES.find(char.upper())
                if code < 0 or file > 7:
                    raise PackError(f"bad FEN board {fen!r}")
                board[(7 - row) * 8 + file] = code + 1 | (8 if char.islower() else 0)
                file += 1
        if file != 8:
            raise PackError(f"bad FEN board {fen!r}")
    if side not in ("w", "b"):
        raise PackError(f"bad side {fen!r}")
    flags = 1 if side == "b" else 0
    if castling != "-":
        if "".join(c for c in CASTLING if c in castling) != castling:
            raise PackError(f"bad castling {fen!r}")
        for bit, char in enumerate(CASTLING):
            if char in castling:
                flags |= 2 << bit
    ep_square = 0xFF if ep == "-" else square(ep)
    half, full = int(halfmove), int(fullmove)
    if not (0 <= half <= 255 and 0 <= full <= 65535):
        raise PackError(f"move counters out of range {fen!r}")
    occupancy = 0
    nibbles = []
    for index, piece in enumerate(board):
        if piece:
            occupancy |= 1 << index
            nibbles.append(piece)
    if len(nibbles) % 2:
        nibbles.append(0)
    packed = bytes(nibbles[i] | nibbles[i + 1] << 4 for i in range(0, len(nibbles), 2))
    return struct.pack("<BBBHQ", flags, ep_square, half, full, occupancy) + packed


def decode_position(data, offset):
    flags, ep_square, half, full, occupancy = struct.unpack_from("<BBBHQ", data, offset)
    offset += 13
    count = bin(occupancy).count("1")
    nibbles = []
    for byte in data[offset:offset + (count + 1) // 2]:
        nibbles += [byte & 15, byte >> 4]
    offset += (count + 1) // 2
    board = [0] * 64
    it = iter(nibbles)
    for index in range(64):
        if occupancy >> index & 1:
            board[index] = next(it)
    rows = []
    for rank in range(7, -1, -1):
        text, empty = "", 0
        for file in range(8):
            piece = board[rank * 8 + file]
            if not piece:
                empty += 1
                continue
            if empty:
                text += str(empty)
                empty = 0
            char = PIECES[(piece & 7) - 1]
            text += char.lower() if piece & 8 else char
        rows.append(text + (str(empty) if empty else ""))
    castling = "".join(c for bit, c in enumerate(CASTLING) if flags & 2 << bit) or "-"
    ep = "-" if ep_square == 0xFF else square_name(ep_square)
    fen = f"{'/'.join(rows)} {'b' if flags & 1 else 'w'} {castling} {ep} {half} {full}"
    return fen, offset


def encode_record(row, theme_index, mask_bytes):
    puzzle_id, fen, moves, rating, popularity, themes = row
    if len(puzzle_id) != 5 or not puzzle_id.isascii() or not puzzle_id.isalnum():
        raise PackError(f"bad puzzle id {puzzle_id!r}")
    move_list = moves.split(" ")
    if not 2 <= len(move_list) <= 255:
        raise PackError(f"{puzzle_id}: bad move count")
    if not (0 <= rating <= 65535 and -100 <= popularity <= 100):
        raise PackError(f"{puzzle_id}: rating/popularity out of range")
    mask = 0
    for name in themes.split():
        mask |= 1 << theme_index[name]
    out = puzzle_id.encode() + struct.pack("<Hb", rating, popularity) + encode_position(fen)
    out += struct.pack("<B", len(move_list))
    out += b"".join(struct.pack("<H", encode_move(m)) for m in move_list)
    out += mask.to_bytes(mask_bytes, "little")
    # Round-trip check: the reader must reproduce the CSV text exactly.
    decoded_fen, offset = decode_position(out, 8)
    count = out[offset]
    decoded_moves = [decode_move(struct.unpack_from("<H", out, offset + 1 + 2 * i)[0])
                     for i in range(count)]
    if decoded_fen != fen or decoded_moves != move_list:
        raise PackError(f"{puzzle_id}: does not round-trip")
    return out


def parse_int(text):
    return int(text) if text else 0


def bucket_of(rating):
    return (min(max(rating, BUCKET_LOW), BUCKET_HIGH - 1) - BUCKET_LOW) // RATING_STEP


def scan(path, keep_all):
    """Pass 1: theme universe and the lightweight candidate list."""
    themes = set()
    candidates = []  # (key, row number, rating, tier, theme names)
    combos = {}  # shares one names tuple per distinct Themes string
    raw, reader = open_rows(path)
    with raw:
        for number, row in enumerate(reader):
            names = combos.get(row[7])
            if names is None:
                names = combos[row[7]] = tuple(sorted(set(row[7].split())))
                themes.update(names)
            rating, deviation = parse_int(row[3]), parse_int(row[4])
            popularity, plays = parse_int(row[5]), parse_int(row[6])
            if keep_all or (popularity >= 80 and plays >= 300 and deviation <= 90):
                tier = 0
            elif popularity >= 50 and plays >= 100 and deviation <= 120:
                tier = 1
            else:
                continue
            candidates.append((order_key(row[0]), number, rating, tier, tuple(names)))
    return sorted(themes), candidates


def select(candidates, themes, target, theme_min):
    tier0 = [c for c in candidates if c[3] == 0]
    buckets = {}
    for c in sorted(tier0):
        buckets.setdefault(bucket_of(c[2]), []).append(c)
    sizes = [len(buckets.get(b, [])) for b in range((BUCKET_HIGH - BUCKET_LOW) // RATING_STEP)]
    target = min(target, sum(sizes))
    low, high = 0, max(sizes, default=0)
    while low < high:  # largest equal quota q with sum(min(size, q)) <= target
        mid = (low + high + 1) // 2
        if sum(min(s, mid) for s in sizes) <= target:
            low = mid
        else:
            high = mid - 1
    quotas = [min(s, low) for s in sizes]
    spare = target - sum(quotas)
    for b, s in enumerate(sizes):
        if spare and s > quotas[b]:
            quotas[b] += 1
            spare -= 1
    selected = set()
    for b, quota in enumerate(quotas):
        selected.update(c[1] for c in buckets.get(b, [])[:quota])
    stratified = len(selected)

    have = {name: 0 for name in themes}
    for c in candidates:
        if c[1] in selected:
            for name in c[4]:
                have[name] += 1
    by_key = sorted(candidates, key=lambda c: (c[3], c[0]))
    topped = {}
    for theme in themes:
        for c in by_key:
            if have[theme] >= theme_min:
                break
            if theme in c[4] and c[1] not in selected:
                selected.add(c[1])
                for name in c[4]:
                    have[name] += 1
                topped[theme] = topped.get(theme, 0) + 1
    return selected, stratified, topped


def build(args):
    source = Path(args.source)
    digest = hashlib.sha256(source.read_bytes()).digest()
    date = args.source_date or datetime.datetime.fromtimestamp(
        source.stat().st_mtime, datetime.timezone.utc).strftime("%Y-%m-%d")
    if len(date) != 10:
        raise PackError("--source-date must be YYYY-MM-DD")

    themes, candidates = scan(source, args.all)
    if len(themes) > MAX_THEMES:
        raise PackError(f"{len(themes)} themes exceed the format limit {MAX_THEMES}")
    if args.all:
        selected, stratified, topped = {c[1] for c in candidates}, len(candidates), {}
    else:
        selected, stratified, topped = select(candidates, themes, args.target, args.theme_min)

    theme_index = {name: i for i, name in enumerate(themes)}
    mask_bytes = (len(themes) + 7) // 8
    rows = []
    raw, reader = open_rows(source)
    with raw:
        for number, row in enumerate(reader):
            if number in selected:
                rows.append((row[0], row[1], row[2], parse_int(row[3]), parse_int(row[5]),
                             row[7]))
    rows.sort(key=lambda r: (r[3], order_key(r[0])))
    if not rows:
        raise PackError("no puzzles selected")

    records = [encode_record(r, theme_index, mask_bytes) for r in rows]
    base = rows[0][3] // RATING_STEP * RATING_STEP
    bucket_count = (rows[-1][3] - base) // RATING_STEP + 1
    rating_index = []
    position = 0
    for b in range(bucket_count + 1):
        while position < len(rows) and rows[position][3] < base + b * RATING_STEP:
            position += 1
        rating_index.append(position)

    theme_table = b"".join(struct.pack("<B", len(n)) + n.encode() for n in themes)
    offsets, total = [], 0
    for record in records:
        offsets.append(total)
        total += len(record)
    offsets.append(total)
    body = (theme_table + struct.pack(f"<{len(rating_index)}I", *rating_index)
            + struct.pack(f"<{len(offsets)}I", *offsets) + b"".join(records))
    size = HEADER_SIZE + len(body) + 4
    header = (MAGIC + struct.pack("<HHIHBBHHHH", VERSION, HEADER_SIZE, len(rows), len(themes),
                                  mask_bytes, 0, base, RATING_STEP, bucket_count, 0)
              + date.encode().ljust(16, b"\0") + digest + struct.pack("<I", size))
    assert len(header) == HEADER_SIZE
    data = header + body
    data += struct.pack("<I", zlib.crc32(data))

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    temporary = out.with_name(out.name + ".tmp")
    temporary.write_bytes(data)
    os.replace(temporary, out)

    counts = {name: 0 for name in themes}
    for r in rows:
        for name in r[5].split():
            counts[name] += 1
    print(f"source      {source.name} ({date}, sha256 {digest.hex()})")
    print(f"puzzles     {len(rows)} (stratified {stratified}, theme top-up "
          f"{len(rows) - stratified})")
    print(f"themes      {len(themes)} (min {min(counts.values())} "
          f"{min(counts, key=counts.get)})")
    for name, added in topped.items():
        print(f"  top-up    {name}: +{added} -> {counts[name]}")
    print(f"ratings     {rows[0][3]}..{rows[-1][3]}")
    print(f"size        {len(data)} bytes -> {out}")


def main():
    parser = argparse.ArgumentParser(description="Build the ProsperoLichess puzzle pack.")
    parser.add_argument("source", help="lichess_db_puzzle.csv.zst (or an uncompressed .csv)")
    parser.add_argument("output", help="pack path, e.g. assets/puzzles/pack.bin")
    parser.add_argument("--source-date", help="YYYY-MM-DD (default: source mtime, UTC)")
    parser.add_argument("--target", type=int, default=100000)
    parser.add_argument("--theme-min", type=int, default=200)
    parser.add_argument("--all", action="store_true", help="pack every row, no selection")
    args = parser.parse_args()
    try:
        build(args)
    except (PackError, OSError) as error:
        print(f"build-puzzle-pack: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
