// ProsperoLichess - Puzzles from the bundled offline pack.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "puzzles/pack.hpp"
#include "puzzles/source.hpp"

#include <memory>
#include <vector>

namespace pch::puzzles
{

class PackSource : public Source
{
  public:
    // theme < 0: any theme. min/max bound the rating when no target is given.
    PackSource(const Pack *pack, int theme, int min_rating, int max_rating, std::uint64_t seed);
    State next(app::Context &ctx, int target_rating, Puzzle *out, std::string *error) override;

  private:
    const Pack *pack_;
    int theme_;
    int min_rating_;
    int max_rating_;
    std::uint64_t rng_;
    std::vector<bool> seen_;
};

} // namespace pch::puzzles
