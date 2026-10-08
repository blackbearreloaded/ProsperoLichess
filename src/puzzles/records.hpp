// ProsperoLichess - The player's own puzzle records, kept on the console.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

namespace pch::puzzles
{

struct Records
{
    int best_streak = 0; // longest Puzzle Streak run
    int best_storm = 0;  // best Puzzle Storm score
    int solved = 0;      // puzzles solved in every mode
};

// A missing or damaged file reads as no records.
Records load_records(const std::string &data_root);
void save_records(const std::string &data_root, const Records &records);

} // namespace pch::puzzles
