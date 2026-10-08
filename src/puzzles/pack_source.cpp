// ProsperoLichess - Puzzles from the bundled offline pack.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "puzzles/pack_source.hpp"

#include "core/strings.hpp"

namespace pch::puzzles
{

PackSource::PackSource(const Pack *pack, int theme, int min_rating, int max_rating,
                       std::uint64_t seed)
    : pack_(pack), theme_(theme), min_rating_(min_rating), max_rating_(max_rating), rng_(seed | 1u)
{
    if (pack_ != nullptr)
        seen_.assign(pack_->size(), false);
}

Source::State PackSource::next(app::Context &, int target_rating, Puzzle *out, std::string *error)
{
    if (pack_ == nullptr || pack_->size() == 0)
    {
        *error = tr("The offline puzzle pack is missing");
        return State::error;
    }
    const auto seen = [this](std::size_t index) { return seen_[index]; };
    // Widen the window until something fits.
    for (int widen = 0; widen < 8; ++widen)
    {
        int low = min_rating_;
        int high = max_rating_;
        if (target_rating > 0)
        {
            low = target_rating - 75 - widen * 100;
            high = target_rating + 75 + widen * 100;
        }
        else if (widen > 0)
        {
            low -= widen * 150;
            high += widen * 150;
        }
        std::size_t index = 0;
        if (!pack_->pick(low, high, theme_, &rng_, seen, &index))
            continue;
        seen_[index] = true;
        PackPuzzle raw;
        if (!pack_->get(index, &raw))
            continue;
        if (!from_csv(raw.id, raw.fen, raw.moves, raw.rating, out, error))
            continue;
        for (int t = 0; t < pack_->theme_count(); ++t)
        {
            if (raw.themes.has(t))
                out->themes.push_back(pack_->theme_name(t));
        }
        out->source = "pack";
        return State::ready;
    }
    *error = tr("You have solved every puzzle here");
    return State::exhausted;
}

} // namespace pch::puzzles
