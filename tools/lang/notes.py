# ProsperoLichess - What a translator cannot tell from a text alone.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# English text -> a note. tools/strings.py writes each note above its text in
# the template and in every catalog. Say where the text stands, what a {0}
# holds, and which of two meanings is meant.

NOTES = {
    "Select": "Button hint: choose the highlighted item.",
    "Back": "Button hint: go back one screen.",
    "Menu": "Button hint: on the home screen, hand the controller to the menu at the left; "
            "in a game or a puzzle, open its menu.",
    "thousands separator\x04,": "What stands between the thousands of a count (42,318): a comma, "
                               "a point or a no-break space. One character.",
    "{0}%": "A share of a hundred; {0} is the number. Put the sign and any space where the "
            "language has them (49%, 49 %, %49).",
    "of one game\x04{0} won": "The profile's legend when the count is exactly 1. The same as "
                             "the text without this context if the language needs no other form.",
    "of one game\x04{0} drawn": "The profile's legend when the count is exactly 1.",
    "of one game\x04{0} lost": "The profile's legend when the count is exactly 1.",
}
