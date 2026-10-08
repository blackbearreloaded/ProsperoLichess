// ProsperoLichess - The app's text: baked SDF faces, the console's fonts for other scripts.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gfx/font.hpp"

#include "gfx/bidi.hpp"
#include "gfx/system_glyphs.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <utility>

namespace pch::gfx
{

namespace ff = font_format;

namespace
{

// Shaped lines kept; the app redraws the same text every frame.
constexpr std::size_t kLineLimit = 4096;

// Characters that are part of a text and take no room in a line: the
// zero-width space (where a line may break), the joiners, the direction marks
// and controls, variation selectors, the soft hyphen and the byte order mark.
bool takes_no_room(std::uint32_t c)
{
    return (c >= 0x200b && c <= 0x200f) || (c >= 0x202a && c <= 0x202e) ||
           (c >= 0x2060 && c <= 0x2069) || (c >= 0xfe00 && c <= 0xfe0f) || c == 0xfeff ||
           c == 0x00ad || c == 0x061c;
}

// The no-break spaces are drawn as the space they are.
std::uint32_t drawn_as(std::uint32_t c)
{
    return c == 0x00a0 || c == 0x202f ? ' ' : c;
}

// Arabic letters join: no letter-spacing between them.
bool cursive(char32_t c)
{
    return (c >= 0x0600 && c <= 0x06ff) || (c >= 0x0750 && c <= 0x077f) ||
           (c >= 0xfb50 && c <= 0xfdff) || (c >= 0xfe70 && c <= 0xfeff);
}

// Japanese and Chinese text is written without spaces: a line may break
// between its characters. (Korean separates its words with spaces and is
// broken there.)
bool ideographic(char32_t c)
{
    return (c >= 0x2e80 && c <= 0x30ff) || (c >= 0x31f0 && c <= 0x31ff) ||
           (c >= 0x3400 && c <= 0x4dbf) || (c >= 0x4e00 && c <= 0x9fff) ||
           (c >= 0xf900 && c <= 0xfaff) || (c >= 0xfe30 && c <= 0xfe4f) ||
           (c >= 0xff00 && c <= 0xff60) || (c >= 0xffe0 && c <= 0xffe6) ||
           (c >= 0x20000 && c <= 0x2fa1f);
}

// What may not start a line: closing punctuation, the long vowel mark,
// iteration marks, small kana.
constexpr char32_t kNoStart[] = {
    ')',    ']',    '}',    ',',    '.',    '!',    '?',    ':',    ';',    '%',    0x00bb,
    0x2019, 0x201d, 0x2025, 0x2026, 0x3001, 0x3002, 0x3005, 0x3009, 0x300b, 0x300d, 0x300f,
    0x3011, 0x3015, 0x3017, 0x3019, 0x301f, 0x3041, 0x3043, 0x3045, 0x3047, 0x3049, 0x3063,
    0x3083, 0x3085, 0x3087, 0x308e, 0x309d, 0x309e, 0x30a1, 0x30a3, 0x30a5, 0x30a7, 0x30a9,
    0x30c3, 0x30e3, 0x30e5, 0x30e7, 0x30ee, 0x30f5, 0x30f6, 0x30fb, 0x30fc, 0x30fd, 0x30fe,
    0xff01, 0xff05, 0xff09, 0xff0c, 0xff0e, 0xff1a, 0xff1b, 0xff1f, 0xff3d, 0xff5d,
};
// What may not end a line: opening brackets and quotes.
constexpr char32_t kNoEnd[] = {
    '(',    '[',    '{',    0x00ab, 0x2018, 0x201c, 0x3008, 0x300a, 0x300c,
    0x300e, 0x3010, 0x3014, 0x3016, 0x3018, 0x301d, 0xff08, 0xff3b, 0xff5b,
};

template <std::size_t N> bool among(const char32_t (&set)[N], char32_t c)
{
    return std::find(std::begin(set), std::end(set), c) != std::end(set);
}

bool breaks_between(char32_t before, char32_t after)
{
    if (!ideographic(before) && !ideographic(after))
        return false;
    return !among(kNoStart, after) && !among(kNoEnd, before) && !bidi::attaches(after) &&
           after != ' ';
}

struct TextHash
{
    using is_transparent = void;
    std::size_t operator()(std::string_view text) const
    {
        return std::hash<std::string_view>{}(text);
    }
};

// Which font draws a character of a shaped line.
constexpr std::int16_t kOwn = -1;      // this face
constexpr std::int16_t kNone = -2;     // nothing: it takes no room
constexpr std::int16_t kBorrowed = -3; // the fallback face
// 0 and up: one of the console's fonts.

} // namespace

std::uint32_t next_codepoint(std::string_view text, std::size_t *index)
{
    const auto byte = [&](std::size_t at) { return static_cast<unsigned char>(text[at]); };
    const std::size_t i = *index;
    const unsigned char lead = byte(i);
    int length = 1;
    std::uint32_t value = lead;
    if (lead >= 0xf0 && lead < 0xf8)
    {
        length = 4;
        value = lead & 0x07u;
    }
    else if (lead >= 0xe0)
    {
        length = 3;
        value = lead & 0x0fu;
    }
    else if (lead >= 0xc0)
    {
        length = 2;
        value = lead & 0x1fu;
    }
    else if (lead >= 0x80)
    {
        *index = i + 1;
        return 0xfffd;
    }
    if (i + static_cast<std::size_t>(length) > text.size())
    {
        *index = text.size();
        return 0xfffd;
    }
    for (int k = 1; k < length; ++k)
    {
        const unsigned char continuation = byte(i + static_cast<std::size_t>(k));
        if ((continuation & 0xc0u) != 0x80u)
        {
            *index = i + 1;
            return 0xfffd;
        }
        value = (value << 6) | (continuation & 0x3fu);
    }
    *index = i + static_cast<std::size_t>(length);
    return value;
}

// A shaped line: its glyphs in drawing order, positions in em from the line's
// left edge.
struct Font::Line
{
    struct Glyph
    {
        std::int16_t
            face; // kOwn or kBorrowed: id is a code point; else a system font, id is its glyph
        std::uint16_t gaps; // letter-spacing gaps to the left of this glyph
        std::uint32_t id;
        float x; // the pen
        float y; // below the baseline
    };
    std::vector<Glyph> glyphs;
    float advance = 0.0f;
    std::uint16_t gaps = 0;
};

struct Font::Shaping
{
    std::unordered_map<std::string, Line, TextHash, std::equal_to<>> lines;
    unsigned generation = 0; // of the system fonts the lines were shaped with
    std::vector<RunGlyph> run;
};

Font::Font() = default;
Font::~Font() = default;
Font::Font(Font &&) noexcept = default;
Font &Font::operator=(Font &&) noexcept = default;

bool Font::load(std::string_view data)
{
    error_.clear();
    if (data.size() < sizeof(ff::Header))
    {
        error_ = "font too small";
        return false;
    }
    std::memcpy(&header_, data.data(), sizeof(header_));
    if (header_.magic != ff::kMagic || header_.version != ff::kVersion ||
        header_.pixel_size <= 0.0f)
    {
        error_ = "not a pchfont v1";
        return false;
    }
    const std::size_t glyph_bytes =
        static_cast<std::size_t>(header_.glyph_count) * sizeof(ff::Glyph);
    const std::size_t kern_bytes = static_cast<std::size_t>(header_.kern_count) * sizeof(ff::Kern);
    const std::size_t atlas_bytes = static_cast<std::size_t>(header_.atlas_width) *
                                    static_cast<std::size_t>(header_.atlas_height);
    if (data.size() != sizeof(ff::Header) + glyph_bytes + kern_bytes + atlas_bytes)
    {
        error_ = "font size mismatch";
        return false;
    }
    const char *cursor = data.data() + sizeof(ff::Header);
    // A font may have no glyphs or (monospaced faces) no kerning pairs.
    glyphs_.resize(header_.glyph_count);
    if (glyph_bytes != 0)
        std::memcpy(glyphs_.data(), cursor, glyph_bytes);
    cursor += glyph_bytes;
    kerns_.resize(header_.kern_count);
    if (kern_bytes != 0)
        std::memcpy(kerns_.data(), cursor, kern_bytes);
    cursor += kern_bytes;
    atlas_.assign(reinterpret_cast<const std::uint8_t *>(cursor),
                  reinterpret_cast<const std::uint8_t *>(cursor) + atlas_bytes);
    for (const ff::Glyph &glyph : glyphs_)
    {
        if (glyph.x + glyph.w > header_.atlas_width || glyph.y + glyph.h > header_.atlas_height)
        {
            error_ = "glyph outside atlas";
            return false;
        }
    }
    return true;
}

void Font::use_system(SystemGlyphs *system)
{
    system_ = system;
    if (system != nullptr && !shaping_)
        shaping_ = std::make_unique<Shaping>();
    if (shaping_)
        shaping_->lines.clear();
}

const ff::Glyph *Font::find(std::uint32_t codepoint) const
{
    const auto it =
        std::lower_bound(glyphs_.begin(), glyphs_.end(), codepoint,
                         [](const ff::Glyph &g, std::uint32_t c) { return g.codepoint < c; });
    return it != glyphs_.end() && it->codepoint == codepoint ? &*it : nullptr;
}

Font::Found Font::resolve(std::uint32_t *codepoint) const
{
    *codepoint = drawn_as(*codepoint);
    if (const ff::Glyph *glyph = find(*codepoint))
        return {glyph, this};
    if (fallback_ != nullptr)
    {
        if (const ff::Glyph *glyph = fallback_->find(*codepoint))
            return {glyph, fallback_};
    }
    *codepoint = '?';
    return {find('?'), this};
}

bool Font::can_draw(std::string_view text) const
{
    for (std::size_t index = 0; index < text.size();)
    {
        const std::uint32_t read = next_codepoint(text, &index);
        if (read == '\n' || takes_no_room(read))
            continue;
        const std::uint32_t c = drawn_as(read);
        if (find(c) != nullptr || (fallback_ != nullptr && fallback_->find(c) != nullptr))
            continue;
        if (system_ == nullptr || system_->fonts().face_for(c) < 0)
            return false;
    }
    return true;
}

float Font::kern(std::uint32_t first, std::uint32_t second) const
{
    const auto it = std::lower_bound(
        kerns_.begin(), kerns_.end(), std::make_pair(first, second),
        [](const ff::Kern &k, const std::pair<std::uint32_t, std::uint32_t> &key)
        { return k.first != key.first ? k.first < key.first : k.second < key.second; });
    return it != kerns_.end() && it->first == first && it->second == second ? it->amount : 0.0f;
}

const Font::Line *Font::shaped(std::string_view text) const
{
    if (system_ == nullptr || system_->empty() || !shaping_)
        return nullptr;
    // Text the baked faces cover keeps the plain path of measure() and
    // layout(): all of it, when it is ASCII.
    bool ascii = true;
    for (const char c : text)
        ascii = ascii && static_cast<unsigned char>(c) < 0x80;
    if (ascii)
        return nullptr;
    bool covered = true;
    for (std::size_t index = 0; covered && index < text.size();)
    {
        const std::uint32_t read = next_codepoint(text, &index);
        if (takes_no_room(read))
            continue;
        const std::uint32_t c = drawn_as(read);
        covered = find(c) != nullptr || (fallback_ != nullptr && fallback_->find(c) != nullptr);
    }
    if (covered)
        return nullptr;
    Shaping &s = *shaping_;
    if (s.generation != system_->generation())
    {
        s.lines.clear();
        s.generation = system_->generation();
    }
    if (const auto found = s.lines.find(text); found != s.lines.end())
        return &found->second;
    if (s.lines.size() >= kLineLimit)
        s.lines.clear();

    SystemFonts &fonts = system_->fonts();
    std::vector<char32_t> characters;
    for (std::size_t index = 0; index < text.size();)
        characters.push_back(drawn_as(next_codepoint(text, &index)));
    const std::size_t count = characters.size();
    // Which font draws each character: this face when it can, then the
    // fallback face, else the first system font that has it; a mark stays in
    // the font of the letter it sits on.
    std::vector<std::int16_t> face(count, kNone);
    for (std::size_t i = 0; i < count; ++i)
    {
        const char32_t c = characters[i];
        const std::int16_t before = i > 0 ? face[i - 1] : kNone;
        if (takes_no_room(c))
        {
            face[i] = before >= 0 ? before : kNone;
            continue;
        }
        if (find(c) != nullptr)
        {
            face[i] = kOwn;
            continue;
        }
        if (fallback_ != nullptr && fallback_->find(c) != nullptr)
        {
            face[i] = kBorrowed;
            continue;
        }
        int chosen = fonts.face_for(c);
        if (chosen >= 0 && before >= 0 && before != chosen && bidi::attaches(c) &&
            fonts.face_has(before, c))
            chosen = before;
        if (chosen < 0)
        {
            characters[i] = '?';
            face[i] = find('?') != nullptr ? kOwn : kNone;
        }
        else
        {
            face[i] = static_cast<std::int16_t>(chosen);
        }
    }
    std::vector<std::uint8_t> levels;
    bidi::resolve(characters, &levels);
    const std::vector<int> order = bidi::visual_order(levels);

    // Runs: neighbours drawn by one font in one direction. They are laid out
    // in drawing order.
    struct Run
    {
        std::size_t first;
        std::size_t last; // one past
        std::int16_t face;
        std::uint8_t level;
        int rank; // where the run starts on the line
    };
    std::vector<int> rank(count);
    for (std::size_t position = 0; position < count; ++position)
        rank[static_cast<std::size_t>(order[position])] = static_cast<int>(position);
    std::vector<Run> runs;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (face[i] == kNone)
            continue;
        if (!runs.empty() && runs.back().face == face[i] && runs.back().level == levels[i])
        {
            // Only invisible characters lie between the run's end and i.
            bool adjacent = true;
            for (std::size_t k = runs.back().last; k < i; ++k)
                adjacent = adjacent && face[k] == kNone;
            if (adjacent)
            {
                runs.back().last = i + 1;
                runs.back().rank = std::min(runs.back().rank, rank[i]);
                continue;
            }
        }
        runs.push_back({i, i + 1, face[i], levels[i], rank[i]});
    }
    std::sort(runs.begin(), runs.end(), [](const Run &a, const Run &b) { return a.rank < b.rank; });

    Line line;
    float pen = 0.0f;
    bool any = false;    // a glyph with a width has been placed
    bool joined = false; // the last such glyph was part of joined (Arabic) writing
    std::uint16_t gaps = 0;
    std::vector<char32_t> piece;
    for (const Run &run : runs)
    {
        const bool right_to_left = (run.level & 1) != 0;
        if (run.face == kOwn || run.face == kBorrowed)
        {
            const Font &source = run.face == kOwn ? *this : *fallback_;
            const float unit = 1.0f / source.header_.pixel_size;
            std::uint32_t previous = 0;
            for (std::size_t k = 0; k < run.last - run.first; ++k)
            {
                const std::size_t i = right_to_left ? run.last - 1 - k : run.first + k;
                if (face[i] == kNone)
                    continue;
                std::uint32_t c = characters[i];
                if (right_to_left && source.find(bidi::mirror(c)) != nullptr)
                    c = bidi::mirror(c);
                const ff::Glyph *glyph = source.find(c);
                if (glyph == nullptr)
                    continue;
                if (previous != 0)
                    pen += source.kern(previous, c) * unit;
                if (any)
                    ++gaps;
                line.glyphs.push_back({run.face, gaps, c, pen, 0.0f});
                pen += glyph->advance * unit;
                previous = c;
                any = true;
                joined = false;
            }
            continue;
        }
        piece.clear();
        bool joins = false;
        for (std::size_t i = run.first; i < run.last; ++i)
        {
            if (face[i] == kNone)
                continue;
            piece.push_back(characters[i]);
            joins = joins || cursive(characters[i]);
        }
        fonts.shape(run.face, piece.data(), static_cast<int>(piece.size()), right_to_left, &s.run);
        bool first_of_run = true;
        for (const RunGlyph &glyph : s.run)
        {
            // Letter-spacing goes between glyphs that take room, but never
            // inside joined writing.
            if (glyph.advance > 0.0f)
            {
                if (any && !(joins && joined && !first_of_run))
                    ++gaps;
                any = true;
                joined = joins;
                first_of_run = false;
            }
            line.glyphs.push_back(
                {run.face, gaps, glyph.id, pen + glyph.x_offset, -glyph.y_offset});
            pen += glyph.advance;
        }
    }
    line.advance = pen;
    line.gaps = gaps;
    return &s.lines.emplace(std::string(text), std::move(line)).first->second;
}

std::string Font::fit(std::string_view text, float size, float max_width, float tracking) const
{
    if (measure(text, size, tracking) <= max_width)
        return std::string(text);
    constexpr std::string_view kEllipsis = "\xE2\x80\xA6";
    const std::string_view mark = has_glyph(0x2026) ? kEllipsis : std::string_view("...");
    std::string best;
    for (std::size_t index = 0; index < text.size();)
    {
        const std::size_t start = index;
        next_codepoint(text, &index);
        // A mark stays with the character it sits on: never cut before one.
        for (std::size_t next = index; next < text.size();)
        {
            if (!bidi::attaches(next_codepoint(text, &next)))
                break;
            index = next;
        }
        std::string candidate(text.substr(0, start));
        while (!candidate.empty() && candidate.back() == ' ')
            candidate.pop_back();
        candidate.append(mark);
        if (measure(candidate, size, tracking) > max_width)
            break;
        best = std::move(candidate);
    }
    return best;
}

float Font::measure(std::string_view text, float size, float tracking) const
{
    if (const Line *line = shaped(text))
        return line->advance * size + static_cast<float>(line->gaps) * tracking;
    float width = 0.0f;
    std::uint32_t previous = 0;
    const Font *previous_font = nullptr;
    int glyphs = 0;
    for (std::size_t index = 0; index < text.size();)
    {
        std::uint32_t codepoint = next_codepoint(text, &index);
        if (takes_no_room(codepoint))
            continue;
        const Found found = resolve(&codepoint);
        if (found.glyph == nullptr)
            continue;
        // A borrowed glyph is measured in its own face; kerning is known
        // only between two glyphs of one face.
        const float scale = size / found.font->header_.pixel_size;
        if (previous != 0 && found.font == previous_font)
            width += found.font->kern(previous, codepoint) * scale;
        width += found.glyph->advance * scale;
        previous = codepoint;
        previous_font = found.font;
        ++glyphs;
    }
    return glyphs > 1 ? width + tracking * static_cast<float>(glyphs - 1) : width;
}

float Font::layout(std::string_view text, float x, float y, float size, Align align,
                   std::vector<GlyphQuad> &quads, float tracking) const
{
    const float width = measure(text, size, tracking);
    float pen = x;
    if (align == Align::center)
        pen -= width * 0.5f;
    else if (align == Align::right)
        pen -= width;

    if (const Line *line = shaped(text))
    {
        const float system_scale = size / SystemGlyphs::kPixelSize;
        const float inverse_w = 1.0f / static_cast<float>(SystemGlyphs::kWidth);
        const float inverse_h = 1.0f / static_cast<float>(SystemGlyphs::kHeight);
        for (const Line::Glyph &glyph : line->glyphs)
        {
            const float at_x = pen + glyph.x * size + static_cast<float>(glyph.gaps) * tracking;
            const float at_y = y + glyph.y * size;
            GlyphQuad quad;
            if (glyph.face < 0)
            {
                const Font &source = glyph.face == kOwn ? *this : *fallback_;
                const ff::Glyph *baked = source.find(glyph.id);
                if (baked == nullptr || baked->w == 0 || baked->h == 0)
                    continue;
                const float scale = size / source.header_.pixel_size;
                quad.x0 = at_x + baked->offset_x * scale;
                quad.y0 = at_y + baked->offset_y * scale;
                quad.x1 = quad.x0 + static_cast<float>(baked->w) * scale;
                quad.y1 = quad.y0 + static_cast<float>(baked->h) * scale;
                quad.u0 =
                    static_cast<float>(baked->x) / static_cast<float>(source.header_.atlas_width);
                quad.v0 =
                    static_cast<float>(baked->y) / static_cast<float>(source.header_.atlas_height);
                quad.u1 = static_cast<float>(baked->x + baked->w) /
                          static_cast<float>(source.header_.atlas_width);
                quad.v1 = static_cast<float>(baked->y + baked->h) /
                          static_cast<float>(source.header_.atlas_height);
                if (&source != this)
                {
                    quad.font = &source;
                    quad.texture = fallback_texture_;
                }
                quads.push_back(quad);
                continue;
            }
            // Drawn into the shared atlas the first time it is needed. A glyph
            // that found no room there is drawn next frame, once it is emptied.
            const SystemGlyphs::Placed *placed = system_->place(glyph.face, glyph.id);
            if (placed == nullptr || placed->w == 0)
                continue;
            quad.x0 = at_x + placed->offset_x * system_scale;
            quad.y0 = at_y + placed->offset_y * system_scale;
            quad.x1 = quad.x0 + static_cast<float>(placed->w) * system_scale;
            quad.y1 = quad.y0 + static_cast<float>(placed->h) * system_scale;
            quad.u0 = static_cast<float>(placed->x) * inverse_w;
            quad.v0 = static_cast<float>(placed->y) * inverse_h;
            quad.u1 = static_cast<float>(placed->x + placed->w) * inverse_w;
            quad.v1 = static_cast<float>(placed->y + placed->h) * inverse_h;
            quad.texture = system_->texture();
            quad.range = size * SystemGlyphs::kSpread;
            quads.push_back(quad);
        }
        return width;
    }

    std::uint32_t previous = 0;
    const Font *previous_font = nullptr;
    for (std::size_t index = 0; index < text.size();)
    {
        std::uint32_t codepoint = next_codepoint(text, &index);
        if (takes_no_room(codepoint))
            continue;
        const Found found = resolve(&codepoint);
        const ff::Glyph *glyph = found.glyph;
        if (glyph == nullptr)
            continue;
        const ff::Header &face = found.font->header_;
        const float scale = size / face.pixel_size;
        const float inverse_w = 1.0f / static_cast<float>(face.atlas_width);
        const float inverse_h = 1.0f / static_cast<float>(face.atlas_height);
        if (previous != 0 && found.font == previous_font)
            pen += found.font->kern(previous, codepoint) * scale;
        if (glyph->w > 0 && glyph->h > 0)
        {
            GlyphQuad quad;
            if (found.font != this)
            {
                quad.font = found.font;
                quad.texture = fallback_texture_;
            }
            quad.x0 = pen + glyph->offset_x * scale;
            quad.y0 = y + glyph->offset_y * scale;
            quad.x1 = quad.x0 + static_cast<float>(glyph->w) * scale;
            quad.y1 = quad.y0 + static_cast<float>(glyph->h) * scale;
            quad.u0 = static_cast<float>(glyph->x) * inverse_w;
            quad.v0 = static_cast<float>(glyph->y) * inverse_h;
            quad.u1 = static_cast<float>(glyph->x + glyph->w) * inverse_w;
            quad.v1 = static_cast<float>(glyph->y + glyph->h) * inverse_h;
            quads.push_back(quad);
        }
        pen += glyph->advance * scale + tracking;
        previous = codepoint;
        previous_font = found.font;
    }
    return width;
}

std::vector<std::string> Font::wrap(std::string_view text, float size, float max_width) const
{
    return wrap_lines(text, max_width, [&](std::string_view line) { return measure(line, size); });
}

std::vector<std::string> wrap_lines(std::string_view text, float max_width,
                                    const std::function<float(std::string_view)> &measure)
{
    std::vector<std::string> lines;
    std::string line;
    // Appends a word wider than a whole line, split between characters (never
    // before a mark that sits on the character before it).
    const auto split_word = [&](std::string_view word)
    {
        std::string part;
        for (std::size_t index = 0; index < word.size();)
        {
            const std::size_t start = index;
            next_codepoint(word, &index);
            for (std::size_t next = index; next < word.size();)
            {
                if (!bidi::attaches(next_codepoint(word, &next)))
                    break;
                index = next;
            }
            const std::string_view character = word.substr(start, index - start);
            if (!part.empty() && measure(part + std::string(character)) > max_width)
            {
                lines.push_back(part);
                part.clear();
            }
            part.append(character);
        }
        line = part;
    };
    // The parts of a word a line may break between: one, unless it holds
    // Japanese or Chinese text or a zero-width space.
    std::vector<std::string_view> pieces;
    const auto split_pieces = [&pieces](std::string_view word)
    {
        pieces.clear();
        std::size_t start = 0;
        char32_t before = 0;
        for (std::size_t index = 0; index < word.size();)
        {
            const std::size_t at = index;
            const char32_t c = next_codepoint(word, &index);
            if (c == 0x200b)
            {
                if (at > start)
                    pieces.push_back(word.substr(start, at - start));
                start = index;
                before = 0;
                continue;
            }
            if (before != 0 && at > start && breaks_between(before, c))
            {
                pieces.push_back(word.substr(start, at - start));
                start = at;
            }
            before = c;
        }
        if (start < word.size() || pieces.empty())
            pieces.push_back(word.substr(start));
    };
    std::size_t index = 0;
    while (index <= text.size())
    {
        const std::size_t newline = text.find('\n', index);
        const std::string_view paragraph = text.substr(
            index, newline == std::string_view::npos ? std::string_view::npos : newline - index);
        std::size_t word_start = 0;
        line.clear();
        const std::size_t first_line = lines.size();
        while (word_start <= paragraph.size())
        {
            std::size_t word_end = paragraph.find(' ', word_start);
            if (word_end == std::string_view::npos)
                word_end = paragraph.size();
            split_pieces(paragraph.substr(word_start, word_end - word_start));
            for (std::size_t p = 0; p < pieces.size(); ++p)
            {
                const std::string_view piece = pieces[p];
                // A space before the word's first piece; the others follow without one.
                const std::string candidate = line.empty()
                                                  ? std::string(piece)
                                                  : line + (p == 0 ? " " : "") + std::string(piece);
                if (line.empty() || measure(candidate) <= max_width)
                {
                    // A first word wider than the line is split only when it
                    // is of a script written without spaces: a Latin word
                    // keeps its letters together.
                    if (line.empty() && pieces.size() > 1 && measure(piece) > max_width)
                        split_word(piece);
                    else
                        line = candidate;
                }
                else
                {
                    lines.push_back(line);
                    if (pieces.size() > 1 && measure(piece) > max_width)
                        split_word(piece);
                    else
                        line.assign(piece);
                }
            }
            word_start = word_end + 1;
        }
        lines.push_back(line);
        // Every line of a right-to-left paragraph runs right to left, also one
        // that happens to start with a Latin word: a right-to-left mark
        // (U+200F, invisible) leads each of them.
        if (lines.size() - first_line > 1)
        {
            std::vector<char32_t> characters;
            for (std::size_t at = 0; at < paragraph.size();)
                characters.push_back(next_codepoint(paragraph, &at));
            if (bidi::paragraph_level(characters) == 1)
            {
                for (std::size_t k = first_line; k < lines.size(); ++k)
                    lines[k].insert(0, "\xE2\x80\x8F");
            }
        }
        if (newline == std::string_view::npos)
            break;
        index = newline + 1;
    }
    return lines;
}

} // namespace pch::gfx
