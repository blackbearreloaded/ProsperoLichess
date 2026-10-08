#!/usr/bin/env python3
# ProsperoLichess - Turns chess moves into the controller steps of a hardware script.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Pass & Play turns the board to face the side to move and keeps the cursor at
# the same place on screen, so the steps of a move depend on every move before
# it. This writes them out.
#
#   python3 tests/hardware/moves.py e2e4 d7d5 e4d5 ... > steps.txt
#
# A move is two squares; append "=n" (or q, r, b) for a promotion chosen in
# the picker (it needs "Always promote to queen" off), ":name" to mark the
# position after the move, and "@N" to wait N frames instead of 60.
import sys

PICKER = "qnrb"  # the picker's order, top to bottom


def screen(square, white):
    file = ord(square[0]) - ord("a")
    rank = int(square[1]) - 1
    return (file, 7 - rank) if white else (7 - file, rank)


def steps(moves, start="e2"):
    out = []
    white = True
    col, row = screen(start, True)

    def go(target):
        nonlocal col, row
        c, r = target
        out.extend(["nav right"] * (c - col) if c > col else ["nav left"] * (col - c))
        out.extend(["nav down"] * (r - row) if r > row else ["nav up"] * (row - r))
        col, row = c, r

    for word in moves:
        wait = 60
        mark = None
        if "@" in word:
            word, frames = word.split("@")
            wait = int(frames)
        if ":" in word:
            word, mark = word.split(":")
        promote = None
        if "=" in word:
            word, promote = word.split("=")
        origin, target = word[:2], word[2:4]
        out.append("# %s%s%s (%s)" % (origin, target, "=" + promote if promote else "",
                                      "White" if white else "Black"))
        go(screen(origin, white))
        out.append("press confirm")
        go(screen(target, white))
        out.append("press confirm")
        if promote:
            out.append("wait 30")
            out.append("mark promotion-picker")
            out.extend(["nav down"] * PICKER.index(promote))
            out.append("press confirm")
        out.append("wait %d" % wait)
        if mark:
            out.append("mark " + mark)
        # The board turns; the cursor keeps its place on screen.
        white = not white
    return out


if __name__ == "__main__":
    print("\n".join(steps(sys.argv[1:])))
