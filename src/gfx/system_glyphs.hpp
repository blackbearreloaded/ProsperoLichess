// ProsperoLichess - The console's fonts drawn into one atlas that every face shares.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "gfx/system_fonts.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pch::gfx
{

// The baked faces hold Latin, Greek and Cyrillic. A character none of them has
// (Japanese, Korean, Chinese, Thai, Arabic) is drawn with the font files the
// console carries (gfx/system_fonts.hpp). Its glyph is turned into a distance
// field the first time a text needs it and kept in the atlas here, which the
// app's four faces share (gfx::Font::use_system): the console has one weight
// of those fonts, so one copy of a glyph serves them all.
//
// One thread uses it: the app's.
class SystemGlyphs
{
  public:
    // A glyph is drawn with the em at this many pixels...
    static constexpr float kPixelSize = 48.0f;
    // ... and its distance field spreads this share of the em, as the baked
    // faces' do (8 pixels at 56), so one shader serves both.
    static constexpr float kSpread = 8.0f / 56.0f;
    // About a thousand Japanese glyphs fit.
    static constexpr int kWidth = 2048;
    static constexpr int kHeight = 2048;

    // Where a glyph sits in the atlas; w 0: it draws nothing (a space).
    struct Placed
    {
        std::uint16_t x = 0;
        std::uint16_t y = 0;
        std::uint16_t w = 0;
        std::uint16_t h = 0;
        float offset_x = 0.0f; // top-left corner from the pen on the baseline, at kPixelSize
        float offset_y = 0.0f;
    };

    // The font files to draw from, tried in order and read when first needed;
    // language is the app's (a tag such as "ja-JP"). An empty list: none.
    void use(std::vector<std::string> files, std::string_view language);
    bool empty() const
    {
        return fonts_.empty();
    }
    SystemFonts &fonts()
    {
        return fonts_;
    }
    // Changes whenever use() names fonts: lines shaped with the old ones are stale.
    unsigned generation() const
    {
        return generation_;
    }

    // The glyph's place in the atlas, drawing it there when it is first
    // needed. Null when the atlas has no room left: the rows are emptied at
    // the start of the next frame's text and the glyph is drawn then.
    const Placed *place(int face, std::uint32_t glyph);

    // The texture that holds the atlas: a font handle of the renderer.
    void set_texture(std::uint32_t texture)
    {
        texture_ = texture;
    }
    std::uint32_t texture() const
    {
        return texture_;
    }
    const std::vector<std::uint8_t> &atlas() const
    {
        return atlas_;
    }
    // The rows [*first, *last) of the atlas that changed since the last call
    // (glyphs drawn for new text); false when none did. Call it once a frame,
    // after the frame's text is laid out, and upload those rows before drawing.
    bool take_changed_rows(int *first, int *last);

    // The fonts read and the glyphs drawn so far, for the log.
    std::string loaded() const
    {
        return fonts_.loaded();
    }

  private:
    void changed(int first, int last);

    SystemFonts fonts_;
    std::unordered_map<std::uint64_t, Placed> placed_;
    std::vector<std::uint8_t> atlas_;
    // Shelf packing.
    int pen_x_ = 1;
    int pen_y_ = 1;
    int shelf_ = 0;
    int changed_first_ = kHeight;
    int changed_last_ = 0;
    bool full_ = false;     // a glyph of this frame's text found no room
    bool emptying_ = false; // the next frame starts with empty rows
    unsigned generation_ = 0;
    std::uint32_t texture_ = 0;
};

} // namespace pch::gfx
