// ProsperoLichess - Where a puzzle screen gets its next puzzle from.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/scene.hpp"
#include "puzzles/puzzle.hpp"

#include <string>

namespace pch::puzzles
{

class Source
{
  public:
    enum class State
    {
        loading,
        ready,
        error,
        exhausted,
    };
    virtual ~Source() = default;
    // Called every frame until it returns ready (then *out holds the puzzle).
    // target_rating steers difficulty for sources that support it (0 = any).
    virtual State next(app::Context &ctx, int target_rating, Puzzle *out, std::string *error) = 0;
    // The player's result; rated is false after a hint or a retry.
    virtual void report(app::Context &, const Puzzle &, bool win, bool rated)
    {
        (void)win;
        (void)rated;
    }
    // Rating shown in the side panel (0 hides it) and its latest change.
    virtual int player_rating() const
    {
        return 0;
    }
    virtual int rating_change() const
    {
        return 0;
    }
};

} // namespace pch::puzzles
