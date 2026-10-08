// ProsperoLichess - Text in scripts the baked faces lack: direction, shaping, the shared atlas.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_file.hpp"
#include "gfx/bidi.hpp"
#include "gfx/font.hpp"
#include "gfx/system_fonts.hpp"
#include "gfx/system_glyphs.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#ifndef PCH_SOURCE_DIR
#define PCH_SOURCE_DIR "."
#endif

namespace
{

using namespace pch;

// The console's fonts are not in the repository. DejaVu Sans Mono, which is,
// has Arabic letters and their joining forms: it stands in for them here.
const std::string kStandIn = std::string(PCH_SOURCE_DIR) + "/third_party/fonts/DejaVuSansMono.ttf";
// "salam": seen, lam, alef, meem.
constexpr std::string_view kSalam = "\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85";

bool load(const char *name, gfx::Font *font)
{
    std::string data;
    return save::read_file(std::string(PCH_SOURCE_DIR) + "/assets/fonts/" + name, &data) &&
           font->load(data);
}

TEST(Bidi, LettersGetTheirDirection)
{
    std::vector<char32_t> text = {'a', 'b', ' ', 0x0633, 0x0644, ' ', 'c'};
    std::vector<std::uint8_t> levels;
    EXPECT_EQ(gfx::bidi::resolve(text, &levels), 0);
    EXPECT_EQ(levels, (std::vector<std::uint8_t>{0, 0, 0, 1, 1, 0, 0}));
    EXPECT_EQ(gfx::bidi::visual_order(levels), (std::vector<int>{0, 1, 2, 4, 3, 5, 6}));

    // A number inside right-to-left text keeps its digits in order.
    text = {0x0633, ' ', '1', '2'};
    EXPECT_EQ(gfx::bidi::resolve(text, &levels), 1);
    EXPECT_EQ(levels, (std::vector<std::uint8_t>{1, 1, 2, 2}));
    EXPECT_EQ(gfx::bidi::visual_order(levels), (std::vector<int>{2, 3, 1, 0}));

    EXPECT_EQ(gfx::bidi::paragraph_level({0x0633, 'a'}), 1);
    EXPECT_EQ(gfx::bidi::paragraph_level({'a', 0x0633}), 0);
    EXPECT_EQ(gfx::bidi::mirror('('), U')');
    EXPECT_EQ(gfx::bidi::mirror('a'), U'a');
    EXPECT_TRUE(gfx::bidi::attaches(0x0e31)); // a Thai vowel above
    EXPECT_TRUE(gfx::bidi::attaches(0x064e)); // an Arabic vowel sign
    EXPECT_FALSE(gfx::bidi::attaches('a'));
}

TEST(SystemText, ArabicLettersJoin)
{
    gfx::SystemFonts fonts;
    fonts.set({kStandIn}, "ar");
    ASSERT_EQ(fonts.face_for(0x0644), 0);
    EXPECT_LT(fonts.face_for(0x3042), 0); // no kana in it

    const char32_t word[] = {0x0633, 0x0644, 0x0627, 0x0645};
    std::vector<gfx::RunGlyph> joined;
    fonts.shape(0, word, 4, true, &joined);
    ASSERT_GE(joined.size(), 3u); // lam and alef may be one glyph
    ASSERT_LE(joined.size(), 4u);
    const char32_t lam[] = {0x0644};
    std::vector<gfx::RunGlyph> alone;
    fonts.shape(0, lam, 1, true, &alone);
    ASSERT_EQ(alone.size(), 1u);
    float advance = 0.0f;
    for (const gfx::RunGlyph &glyph : joined)
    {
        // Inside the word the lam takes a joined form, never the one it has alone.
        EXPECT_NE(glyph.id, alone[0].id);
        advance += glyph.advance;
    }
    EXPECT_GT(advance, 1.0f); // in em: more than one letter wide

    gfx::GlyphField field;
    ASSERT_TRUE(fonts.field(0, joined[0].id, 48.0f, 6.0f, &field));
    EXPECT_GT(field.w, 0);
    EXPECT_EQ(field.pixels.size(), static_cast<std::size_t>(field.w) * field.h);
    // Inside the letter the field is above the edge value, far outside it is 0.
    EXPECT_GT(*std::max_element(field.pixels.begin(), field.pixels.end()), 128);
    EXPECT_EQ(field.pixels.front(), 0);
}

TEST(SystemText, AFaceDrawsOtherScriptsWithTheSharedAtlas)
{
    gfx::Font regular;
    ASSERT_TRUE(load("inter-regular.pchfont", &regular));
    EXPECT_FALSE(regular.can_draw(kSalam));
    const float unknown = regular.measure(kSalam, 40.0f);

    gfx::SystemGlyphs system;
    EXPECT_TRUE(system.empty());
    system.use({kStandIn}, "ar");
    system.set_texture(9);
    ASSERT_FALSE(system.empty());
    regular.use_system(&system);
    EXPECT_TRUE(regular.can_draw(kSalam));
    EXPECT_FALSE(regular.can_draw("\xE6\x97\xA5")); // still no font for Japanese

    std::vector<gfx::GlyphQuad> quads;
    const float width = regular.layout(kSalam, 100.0f, 50.0f, 40.0f, gfx::Align::left, quads);
    EXPECT_GT(width, 0.0f);
    EXPECT_NE(width, unknown);
    EXPECT_FLOAT_EQ(regular.measure(kSalam, 40.0f), width);
    ASSERT_GE(quads.size(), 3u);
    for (const gfx::GlyphQuad &quad : quads)
    {
        EXPECT_EQ(quad.font, nullptr);
        EXPECT_EQ(quad.texture, 9u);
        EXPECT_FLOAT_EQ(quad.range, 40.0f * gfx::SystemGlyphs::kSpread);
        EXPECT_GT(quad.x1, quad.x0);
        EXPECT_GE(quad.x0, 100.0f - 12.0f);
        EXPECT_LE(quad.x1, 100.0f + width + 12.0f);
        EXPECT_GT(quad.u1, quad.u0);
        EXPECT_LE(quad.v1, 1.0f);
    }
    // The glyphs were drawn into the atlas, and that is reported once.
    int first = 0;
    int last = 0;
    ASSERT_TRUE(system.take_changed_rows(&first, &last));
    EXPECT_LT(first, last);
    EXPECT_FALSE(system.take_changed_rows(&first, &last));
    // The same text again draws nothing new.
    quads.clear();
    regular.layout(kSalam, 0.0f, 0.0f, 40.0f, gfx::Align::left, quads);
    EXPECT_FALSE(system.take_changed_rows(&first, &last));

    // Text the face has stays the face's own.
    quads.clear();
    regular.layout("Play", 0.0f, 0.0f, 40.0f, gfx::Align::left, quads);
    ASSERT_EQ(quads.size(), 4u);
    EXPECT_EQ(quads[0].range, 0.0f);
    EXPECT_EQ(quads[0].texture, 0u);
}

TEST(SystemText, ALatinWordKeepsItsPlaceInARightToLeftLine)
{
    gfx::Font regular;
    ASSERT_TRUE(load("inter-regular.pchfont", &regular));
    gfx::SystemGlyphs system;
    system.use({kStandIn}, "ar");
    regular.use_system(&system);

    // Written: the Arabic word, then "Lichess". Drawn: "Lichess" at the left.
    const std::string text = std::string(kSalam) + " Lichess";
    std::vector<gfx::GlyphQuad> quads;
    const float width = regular.layout(text, 0.0f, 0.0f, 40.0f, gfx::Align::left, quads);
    float latin_right = 0.0f;
    float arabic_left = width;
    int latin = 0;
    for (const gfx::GlyphQuad &quad : quads)
    {
        if (quad.range > 0.0f)
        {
            arabic_left = std::min(arabic_left, quad.x0);
        }
        else
        {
            latin_right = std::max(latin_right, quad.x1);
            ++latin;
        }
    }
    EXPECT_EQ(latin, 7);
    EXPECT_LT(latin_right, arabic_left + 8.0f);
    EXPECT_FLOAT_EQ(regular.measure(text, 40.0f), width);
    // Letter-spacing goes between letters, but not inside the joined word.
    const float spaced = regular.measure(text, 40.0f, 10.0f);
    EXPECT_GT(spaced, width + 10.0f * 7);
    EXPECT_LT(spaced, width + 10.0f * 10);
}

TEST(SystemText, LinesBreakWhereAZeroWidthSpaceAllows)
{
    gfx::Font regular;
    ASSERT_TRUE(load("inter-regular.pchfont", &regular));
    const std::string text = "alpha\xE2\x80\x8B"
                             "beta gamma";
    EXPECT_FLOAT_EQ(regular.measure("alpha\xE2\x80\x8B"
                                    "beta",
                                    40.0f),
                    regular.measure("alphabeta", 40.0f));
    const float narrow = regular.measure("alpha", 40.0f) + 2.0f;
    EXPECT_EQ(regular.wrap(text, 40.0f, narrow),
              (std::vector<std::string>{"alpha", "beta", "gamma"}));
    EXPECT_EQ(regular.wrap(text, 40.0f, 2000.0f), (std::vector<std::string>{"alphabeta gamma"}));
    // A Latin word wider than the line keeps its letters together.
    EXPECT_EQ(regular.wrap("extraordinary", 40.0f, 30.0f),
              (std::vector<std::string>{"extraordinary"}));
    // A mark is never cut from its letter (e + combining acute): the line ends
    // before the pair or after it, not between the two.
    const std::string marked = "cafe\xCC\x81 au lait";
    const std::string cut = regular.fit(marked, 40.0f, regular.measure("cafe", 40.0f) + 20.0f);
    EXPECT_TRUE(cut == "caf\xE2\x80\xA6" || cut.find("e\xCC\x81") != std::string::npos) << cut;
}

} // namespace
