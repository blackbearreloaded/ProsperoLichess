// ProsperoLichess - The app's text: baked SDF faces, the console's fonts for other scripts.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "gfx/font_format.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace pch::gfx
{

class Font;
class SystemGlyphs;

// One positioned glyph quad in output pixels, with its atlas UV rectangle.
struct GlyphQuad
{
    float x0, y0, x1, y1;
    float u0, v0, u1, v1;
    // Set when the glyph is borrowed from the fallback face: that face and
    // the texture holding its atlas.
    const Font *font = nullptr;
    std::uint32_t texture = 0;
    // Set for a glyph of the console's fonts: the spread of its distance
    // field in output pixels (texture then names the atlas they share).
    float range = 0.0f;
};

enum class Align : std::uint8_t
{
    left,
    center,
    right,
};

// Text is measured and laid out a line at a time, from UTF-8.
//
// A face is baked into an atlas (tools/bake-fonts.sh): Latin with the marks of
// European languages and Vietnamese, Greek, Cyrillic. A character it lacks is
// borrowed from another baked face (set_fallback), and one no baked face has
// (Japanese, Korean, Chinese, Thai, Arabic) from the console's own fonts
// (use_system): such text is shaped (Arabic letters join, Thai marks stack)
// and ordered (Arabic runs right to left) before it is measured, so every
// function here gives the same answers for it as for Latin text.
//
// One thread uses a Font: the app's. Measuring and laying out fill the cache
// of shaped lines behind the const functions.
class Font
{
  public:
    Font();
    ~Font();
    Font(const Font &) = delete;
    Font &operator=(const Font &) = delete;
    // A face may be moved before it is put to use (a fallback points at the
    // face it was set on, so set those afterwards).
    Font(Font &&) noexcept;
    Font &operator=(Font &&) noexcept;

    // Parses a .pchfont blob; returns false (and keeps an error) if invalid.
    bool load(std::string_view data);
    const std::string &error() const
    {
        return error_;
    }

    // Width in pixels of one line of UTF-8 text at the given pixel size.
    // tracking is extra space between glyphs, in pixels.
    float measure(std::string_view text, float size, float tracking = 0.0f) const;
    // text, or its longest prefix plus an ellipsis that fits max_width.
    std::string fit(std::string_view text, float size, float max_width,
                    float tracking = 0.0f) const;
    float ascent(float size) const
    {
        return header_.ascent * size / header_.pixel_size;
    }
    float descent(float size) const
    {
        return -header_.descent * size / header_.pixel_size;
    }
    float line_height(float size) const
    {
        return (header_.ascent - header_.descent + header_.line_gap) * size / header_.pixel_size;
    }
    // Distance-field spread in output pixels at a size (for shader anti-aliasing).
    float sdf_range(float size) const
    {
        return header_.sdf_range * size / header_.pixel_size;
    }

    // Lays out one line with its baseline at y. x is the left edge, centre or
    // right edge depending on align. Appends to quads; returns the advance.
    float layout(std::string_view text, float x, float y, float size, Align align,
                 std::vector<GlyphQuad> &quads, float tracking = 0.0f) const;

    // Breaks text into lines no wider than max_width: at spaces, between the
    // characters of Japanese and Chinese text, and where a zero-width space
    // (U+200B) allows it (Thai). A word wider than the line is split between
    // characters.
    std::vector<std::string> wrap(std::string_view text, float size, float max_width) const;

    std::uint16_t atlas_width() const
    {
        return header_.atlas_width;
    }
    std::uint16_t atlas_height() const
    {
        return header_.atlas_height;
    }
    const std::vector<std::uint8_t> &atlas() const
    {
        return atlas_;
    }
    bool has_glyph(std::uint32_t codepoint) const
    {
        return find(codepoint) != nullptr;
    }

    // Characters this face lacks are drawn with another one (texture holds
    // its atlas): the display face has no Greek, the mono face little
    // Vietnamese. Measuring, fitting and layout all take it into account.
    void set_fallback(const Font *font, std::uint32_t texture)
    {
        fallback_ = font;
        fallback_texture_ = texture;
    }
    // Characters no baked face has are drawn with the console's fonts, which
    // every face shares. Null: the baked faces alone.
    void use_system(SystemGlyphs *system);
    // Whether every character of text has a glyph: here, in the fallback or
    // in the console's fonts (a catalog the fonts cannot draw in full is not used).
    bool can_draw(std::string_view text) const;

  private:
    struct Found
    {
        const font_format::Glyph *glyph = nullptr;
        const Font *font = nullptr;
    };
    struct Line;
    struct Shaping;

    const font_format::Glyph *find(std::uint32_t codepoint) const;
    // The glyph of a character: this face's, the fallback's, or this face's
    // question mark (*codepoint then says so). No-break spaces are spaces.
    Found resolve(std::uint32_t *codepoint) const;
    float kern(std::uint32_t first, std::uint32_t second) const;
    // The shaped form of text that needs the console's fonts; null for text
    // the baked faces cover.
    const Line *shaped(std::string_view text) const;

    font_format::Header header_{};
    std::vector<font_format::Glyph> glyphs_;
    std::vector<font_format::Kern> kerns_;
    std::vector<std::uint8_t> atlas_;
    std::string error_;
    const Font *fallback_ = nullptr;
    std::uint32_t fallback_texture_ = 0;
    SystemGlyphs *system_ = nullptr;
    std::unique_ptr<Shaping> shaping_;
};

// Decodes the next UTF-8 codepoint from text at *index (advancing it);
// invalid bytes decode as U+FFFD.
std::uint32_t next_codepoint(std::string_view text, std::size_t *index);

// The line breaking of Font::wrap for text measured some other way (a
// component that measures through its painter): measure gives a line's width.
std::vector<std::string> wrap_lines(std::string_view text, float max_width,
                                    const std::function<float(std::string_view)> &measure);

} // namespace pch::gfx
