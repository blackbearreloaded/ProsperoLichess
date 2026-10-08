// ProsperoLichess - The console's fonts drawn into one atlas that every face shares.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gfx/system_glyphs.hpp"

#include <algorithm>
#include <cstring>

namespace pch::gfx
{

void SystemGlyphs::changed(int first, int last)
{
    changed_first_ = std::min(changed_first_, first);
    changed_last_ = std::max(changed_last_, last);
}

void SystemGlyphs::use(std::vector<std::string> files, std::string_view language)
{
    fonts_.set(std::move(files), language);
    placed_.clear();
    ++generation_;
    pen_x_ = 1;
    pen_y_ = 1;
    shelf_ = 0;
    full_ = false;
    emptying_ = false;
    if (fonts_.empty())
    {
        atlas_.clear();
        return;
    }
    atlas_.assign(static_cast<std::size_t>(kWidth) * kHeight, std::uint8_t{0});
    changed(0, kHeight);
}

const SystemGlyphs::Placed *SystemGlyphs::place(int face, std::uint32_t glyph)
{
    if (atlas_.empty())
        return nullptr;
    // A full atlas was noticed last frame: start this one with empty rows.
    if (emptying_)
    {
        emptying_ = false;
        full_ = false;
        placed_.clear();
        std::fill(atlas_.begin(), atlas_.end(), std::uint8_t{0});
        pen_x_ = 1;
        pen_y_ = 1;
        shelf_ = 0;
        changed(0, kHeight);
    }
    const std::uint64_t key =
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(face)) << 32) | glyph;
    auto found = placed_.find(key);
    if (found != placed_.end())
        return &found->second;

    GlyphField field;
    Placed placed;
    if (fonts_.field(face, glyph, kPixelSize, kSpread * kPixelSize, &field) && field.w > 0)
    {
        if (pen_x_ + field.w + 1 > kWidth)
        {
            pen_x_ = 1;
            pen_y_ += shelf_ + 1;
            shelf_ = 0;
        }
        if (field.w + 2 > kWidth || pen_y_ + field.h + 1 > kHeight)
        {
            full_ = true; // not remembered: it is drawn once the rows are emptied
            return nullptr;
        }
        for (int row = 0; row < field.h; ++row)
        {
            std::memcpy(&atlas_[static_cast<std::size_t>(pen_y_ + row) * kWidth +
                                static_cast<std::size_t>(pen_x_)],
                        &field.pixels[static_cast<std::size_t>(row) * field.w],
                        static_cast<std::size_t>(field.w));
        }
        placed.x = static_cast<std::uint16_t>(pen_x_);
        placed.y = static_cast<std::uint16_t>(pen_y_);
        placed.w = static_cast<std::uint16_t>(field.w);
        placed.h = static_cast<std::uint16_t>(field.h);
        placed.offset_x = field.offset_x;
        placed.offset_y = field.offset_y;
        changed(pen_y_, pen_y_ + field.h);
        pen_x_ += field.w + 1;
        shelf_ = std::max(shelf_, field.h);
    }
    return &placed_.emplace(key, placed).first->second;
}

bool SystemGlyphs::take_changed_rows(int *first, int *last)
{
    // Text that found no room this frame is drawn after the rows are emptied, next frame.
    if (full_)
        emptying_ = true;
    if (changed_first_ >= changed_last_)
        return false;
    *first = changed_first_;
    *last = changed_last_;
    changed_first_ = kHeight;
    changed_last_ = 0;
    return true;
}

} // namespace pch::gfx
