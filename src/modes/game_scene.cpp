// ProsperoLichess - The game screen: board, players, clocks, moves, menus and results.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The screen in the house look (docs/LOOK.md):
//
//   - the board leads: it stands on its own light, which takes the game's
//     colour and breathes while it is the player's move;
//   - the game's colour is its speed's (a rapid game is green, a classical one
//     blue, as on the Play page and the waiting screen); Pass & Play keeps the
//     colour of the game screens;
//   - between the two players, a band says what game this is and what is
//     happening in it, over the moves;
//   - what can be asked for is a row of lit plates with signs, and one ring
//     that glides between them; resigning fills its plate while Square is held;
//   - a draw offer or a takeback request arrives as a lit card of frosted
//     glass over the moves, with its two answers;
//   - the result arrives with ceremony: the mark pops, the verdict follows in
//     its colour, then the two players, then the answers.
//
// The kit's components still own the input, the focus and the sounds
// (ButtonGroup, HoldButton, Dialog, PauseMenu); the screen reads their state
// and draws the row, the question and the result itself.

#include "modes/game_scene.hpp"

#include "app/chrome.hpp"
#include "board/board_input.hpp"
#include "board/board_view.hpp"
#include "board/mini_board.hpp"
#include "core/save_file.hpp"
#include "core/strings.hpp"
#include "lichess/board_link.hpp"
#include "lichess/session.hpp"
#include "modes/game_widgets.hpp"
#include "modes/scenes.hpp"
#include "ui/components/button_group.hpp"
#include "ui/components/dialog.hpp"
#include "ui/components/hold_button.hpp"
#include "ui/components/pause_menu.hpp"
#include "ui/confetti.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace pch::modes
{

namespace
{

namespace look = app::look;
using gfx::Align;
using gfx::Color;
using gfx::Rect;

constexpr float kTau = 6.2831853f;

// The column beside the board, top to bottom.
constexpr float kColumn = app::kBoardColumn;
constexpr float kColumnWidth = app::kRight - kColumn;
constexpr Rect kTopPanel{kColumn, app::kStage, kColumnWidth, 132.0f};
// What game this is and what is happening in it, over its moves.
constexpr Rect kBand{kColumn, app::kStage + 156.0f, kColumnWidth, 44.0f};
// Six whole rows of moves: the header, the rows and the table's own padding.
constexpr Rect kTable{kColumn, kBand.y + kBand.h + 10.0f, kColumnWidth,
                      46.0f + 6.0f * 52.0f + 24.0f};
constexpr Rect kRow{kColumn, 740.0f, kColumnWidth, 64.0f};
constexpr float kGroupWidth = 528.0f;
constexpr float kRowGap = 16.0f;
constexpr float kChipRadius = 18.0f;
constexpr Rect kBottomPanel{kColumn, 828.0f, kColumnWidth, 132.0f};
// A question from the opponent covers the band and the moves: the board and
// the clocks stay in plain sight while the player decides.
constexpr Rect kOfferCard{kColumn, kBand.y, kColumnWidth, kTable.y + kTable.h - kBand.y};
// The result, in the middle of the screen.
constexpr Rect kResultCard{520.0f, 262.0f, 880.0f, 556.0f};
constexpr float kCardRadius = 28.0f;
constexpr float kCardPad = 48.0f;
constexpr float kAnswerHeight = 68.0f;
constexpr float kAnswerRadius = 18.0f;

// The game menu: this wide, and wider when a language's words need it.
constexpr float kMenuWidth = 600.0f;
constexpr float kMenuWidest = 880.0f;

// A game that cannot be opened says so in the middle of the screen, where
// the waiting screen that led here had its panel.
constexpr Rect kFailedPanel{580.0f, 302.0f, 760.0f, 480.0f};

constexpr long long kLowTimeMs = 20000;   // the clock turns red and the warning sounds
constexpr long long kCountdownMs = 10000; // every second ticks from here
constexpr float kFlipDelay = 0.55f;       // the move lands before the board turns
constexpr float kResultDelay = 0.9f;      // the last move is seen before the result covers it

const char *const kSaveName = "/passplay.sav";

// What a row of the game menu, a button of the action row or of the result does.
enum Command
{
    kResume = 1,
    kUndo,
    kFlip,
    kAutoFlip,
    kNewGame,
    kOfferDraw,
    kTakeback,
    kAbort,
    kClaimVictory,
    kLeave,
    kReview,
    kResult,
};

std::string join_uci(const chess::Game &game)
{
    std::string out;
    for (std::size_t i = 0; i < game.ply_count(); ++i)
    {
        if (!out.empty())
            out += ' ';
        out += chess::to_uci(game.move_at(i));
    }
    return out;
}

// "Rapid 10+0  ·  Rated" -> "Rapid 10+0 · Rated".
std::string single_spaced(const std::string &text)
{
    std::string out;
    for (const char c : text)
    {
        if (c != ' ' || out.empty() || out.back() != ' ')
            out += c;
    }
    return out;
}

// Whether text begins with name as a word (or words) of its own, whatever
// the case of the plain letters: "Rapid 10+0 · Rated" begins with "rapid".
bool starts_with_word(std::string_view text, std::string_view name)
{
    const auto lower = [](char c)
    { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
    if (name.empty() || text.size() < name.size())
        return false;
    for (std::size_t i = 0; i < name.size(); ++i)
    {
        if (lower(text[i]) != lower(name[i]))
            return false;
    }
    return text.size() == name.size() || text[name.size()] == ' ';
}

// The side, as a player of Pass & Play is called and as a player's note says it.
const char *side_name(chess::Color color)
{
    return color == chess::Color::white ? trc("side", "White") : trc("side", "Black");
}

ui::GroupItem group_item(const char *label, int tag)
{
    ui::GroupItem item;
    item.label = label;
    item.tag = tag;
    return item;
}

// The size at which text is no wider than room, never above size.
float fitted(const ui::FontRef &font, std::string_view text, float size, float room)
{
    const float wide = font.measure(text, size);
    return wide > room ? size * room / wide : size;
}

// Small tracked capitals, as look::kicker draws them, that stay inside room.
float kicker_fit(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
                 float baseline, float room, gfx::Color color, gfx::Align align, float size)
{
    return ui::text_fit(list, fonts.semibold, ui::upper(text), x, baseline, size, room, color,
                        align, 3.0f);
}

// The words of a plate of the row: on one line, or, when they are too long
// for the room even shrunk, on two smaller lines instead of ending in an
// ellipsis.
struct PlateWords
{
    std::string first;
    std::string second; // empty: one line
    float size = 0.0f;  // of a line, before ui::text_fit shrinks it
    float room = 0.0f;
    float width = 0.0f; // of the wider line, as it is drawn
};

PlateWords plate_words(const ui::FontRef &font, std::string_view text, float size, float room)
{
    // As wide as ui::text_fit draws a line.
    const auto drawn = [&](std::string_view line, float at)
    { return std::min(font.measure(line, at * ui::fit_scale(font, line, at, room)), room); };
    PlateWords words;
    words.room = room;
    std::vector<std::string> lines;
    if (font.measure(text, size * ui::fit_scale(font, text, size, room)) > room + 0.5f)
        lines = font.font->wrap(text, size * 0.74f, room);
    if (lines.size() < 2)
    {
        words.first = text;
        words.size = size;
        words.width = drawn(text, size);
        return words;
    }
    // What a third line would hold stays on the second, which is then cut.
    words.first = lines[0];
    words.second = lines[1];
    for (std::size_t i = 2; i < lines.size(); ++i)
        words.second += " " + lines[i];
    words.size = size * 0.74f;
    words.width = std::max(drawn(words.first, words.size), drawn(words.second, words.size));
    return words;
}

// The words from x, centred on cy.
void draw_plate_words(gfx::DrawList &list, const ui::FontRef &font, const PlateWords &words,
                      float x, float cy, gfx::Color ink)
{
    const float middle = cy + words.size * 0.35f;
    if (words.second.empty())
    {
        ui::text_fit(list, font, words.first, x, middle, words.size, words.room, ink);
        return;
    }
    const float pitch = words.size * 1.2f;
    ui::text_fit(list, font, words.first, x, middle - pitch * 0.5f, words.size, words.room, ink);
    ui::text_fit(list, font, words.second, x, middle + pitch * 0.5f, words.size, words.room, ink);
}

// The row of hints under the column. It ends at the screen's right edge and
// grows to the left, where the board's frame stands at the same height: in a
// language with longer words it is drawn smaller, so that it stays clear of
// the frame instead of running over it.
void draw_column_hints(const app::Context &ctx, gfx::DrawList &list, const ui::Hint *hints,
                       int count)
{
    const ui::Fonts &fonts = *ctx.fonts;
    const Rect &squares = app::kBoardSquares;
    const float frame = squares.w / 8.0f * 0.16f; // as the board draws it
    const float room = app::kRight - (squares.x + squares.w + frame) - 10.0f;
    float wide = ui::measure_hints(fonts, hints, count);
    if (wide <= room)
    {
        app::draw_hints(ctx, list, hints, count);
        return;
    }
    // The glyphs, the words and the gaps between hints shrink together. The
    // gap inside a hint does not, so one pass may leave the row a little wide.
    ui::HintLayout layout;
    for (int pass = 0; pass < 4 && wide > room; ++pass)
    {
        const float scale = room / wide;
        layout.size *= scale;
        layout.text_size *= scale;
        layout.item_gap *= scale;
        wide = ui::measure_hints(fonts, hints, count, layout);
    }
    // In the colours app::draw_hints gives a row on the page.
    const ui::Theme &theme = ctx.theme();
    ui::GlyphStyle glyphs = theme.dark ? ui::GlyphStyle::dark() : ui::GlyphStyle::light();
    glyphs.label = theme.page_text.a > 0.0f ? theme.page_text : theme.text.with_alpha(0.86f);
    ui::draw_hints(list, fonts, glyphs, hints, count, app::kRight, true, layout);
}

// ---- signs ------------------------------------------------------------------

enum class Sign
{
    none,
    equal, // a draw
    back,  // a move taken back
    flip,  // the board turned
    star,  // the result
    flag,  // giving up
    cross, // a loss, an abort
    check, // a win
    pawns, // two players on one controller
};

// A sign in one colour, inside box.
void draw_sign(gfx::DrawList &list, Sign sign, const Rect &box, Color ink)
{
    const float s = std::min(box.w, box.h);
    const float cx = box.cx();
    const float cy = box.cy();
    const float pen = std::max(s * 0.12f, 2.4f);
    switch (sign)
    {
    case Sign::equal:
        list.rounded_rect({cx - s * 0.34f, cy - s * 0.2f - pen * 0.5f, s * 0.68f, pen}, pen * 0.5f,
                          ink);
        list.rounded_rect({cx - s * 0.34f, cy + s * 0.2f - pen * 0.5f, s * 0.68f, pen}, pen * 0.5f,
                          ink);
        break;
    case Sign::back:
    {
        // Three quarters of a circle, with a head where it started.
        const float radius = s * 0.36f;
        list.arc(cx, cy + s * 0.04f, radius + pen * 0.5f, pen, kTau * 0.875f, kTau * 0.72f, ink);
        const float hx = cx - 0.707f * radius;
        const float hy = cy + s * 0.04f - 0.707f * radius;
        const float head = s * 0.2f;
        list.triangle({hx - head, hy - head * 0.9f, 2.0f * head, 2.0f * head}, ink, 0.0f,
                      kTau * 0.625f);
        break;
    }
    case Sign::flip:
        for (const float side : {-1.0f, 1.0f})
        {
            // One arrow up, one down.
            const float x = cx + side * s * 0.2f;
            const float head = s * 0.17f;
            list.line(x, cy - s * 0.26f, x, cy + s * 0.26f, pen, ink);
            if (side < 0.0f)
                list.triangle({x - head, cy - s * 0.42f, 2.0f * head, 1.6f * head}, ink);
            else
                list.triangle({x - head, cy + s * 0.42f - 1.6f * head, 2.0f * head, 1.6f * head},
                              ink, 0.0f, kTau * 0.5f);
        }
        break;
    case Sign::star:
        list.star(cx, cy, s * 0.46f, ink);
        break;
    case Sign::flag:
        list.line(cx - s * 0.26f, cy - s * 0.38f, cx - s * 0.26f, cy + s * 0.4f, pen, ink);
        list.rounded_rect({cx - s * 0.26f, cy - s * 0.38f, s * 0.6f, s * 0.38f}, s * 0.06f, ink);
        break;
    case Sign::cross:
        list.line(cx - s * 0.28f, cy - s * 0.28f, cx + s * 0.28f, cy + s * 0.28f, pen, ink);
        list.line(cx - s * 0.28f, cy + s * 0.28f, cx + s * 0.28f, cy - s * 0.28f, pen, ink);
        break;
    case Sign::check:
        list.line(cx - s * 0.32f, cy + s * 0.02f, cx - s * 0.08f, cy + s * 0.26f, pen, ink);
        list.line(cx - s * 0.08f, cy + s * 0.26f, cx + s * 0.34f, cy - s * 0.24f, pen, ink);
        break;
    case Sign::pawns:
        for (const float side : {-1.0f, 1.0f})
        {
            const float px = cx + side * s * 0.24f;
            list.circle(px, cy - s * 0.24f, s * 0.15f, ink);
            list.triangle({px - s * 0.17f, cy - s * 0.2f, s * 0.34f, s * 0.46f}, ink);
            list.rounded_rect({px - s * 0.21f, cy + s * 0.26f, s * 0.42f, s * 0.14f}, s * 0.05f,
                              ink);
        }
        break;
    case Sign::none:
        break;
    }
}

Sign sign_of(int command)
{
    switch (command)
    {
    case kOfferDraw:
        return Sign::equal;
    case kTakeback:
    case kUndo:
        return Sign::back;
    case kFlip:
        return Sign::flip;
    case kResult:
        return Sign::star;
    default:
        return Sign::none;
    }
}

// ---- surfaces ---------------------------------------------------------------

// Light around the board's frame. It goes over the board (whose own shadow
// would swallow a glow drawn under it); its inside stays clear.
void draw_board_light(gfx::DrawList &list, const Rect &squares, Color c, float amount)
{
    if (amount <= 0.01f)
        return;
    const float frame = squares.w / 8.0f * 0.16f; // as the board draws it
    const Rect outer = squares.inset(-frame);
    const float radius = frame * 0.9f;
    // Thin bands, each fainter than the one inside it: the steps are too
    // small to be seen as steps.
    constexpr int kSteps = 12;
    constexpr float kWidth = 3.0f;
    for (int i = 0; i < kSteps; ++i)
    {
        const float out = kWidth * static_cast<float>(i + 1);
        const float fall = 1.0f - static_cast<float>(i) / static_cast<float>(kSteps);
        list.bordered_rect(outer.inset(-out), radius + out, look::kClear, kWidth + 0.75f,
                           c.with_alpha(0.3f * amount * fall * fall * fall));
    }
    list.bordered_rect(outer, radius, look::kClear, 2.0f, c.with_alpha(0.45f * amount));
}

// A card that floats over the screen: the blurred screen behind it (glass is
// the frame's blurred copy, 0 when there is none), tinted with the colour and
// lit at its edge.
void draw_glass_card(gfx::DrawList &list, std::uint32_t glass, const Rect &r, Color c, float lit)
{
    list.shadow({r.x, r.y + 18.0f, r.w, r.h}, kCardRadius, 46.0f, look::kNight.with_alpha(0.6f));
    list.glow(r, kCardRadius, 30.0f, c.with_alpha(0.2f * lit));
    const Color top = gfx::mix(gfx::mix(look::kNight, look::kInk, 0.12f), c, 0.14f);
    const Color bottom = gfx::mix(look::kNight, c, 0.05f);
    if (glass != 0)
    {
        list.glass(glass, r, kCardRadius, Color{1.0f, 1.0f, 1.0f, 1.0f});
        list.gradient_rect(r, kCardRadius, top.with_alpha(0.74f), bottom.with_alpha(0.84f));
    }
    else
    {
        list.gradient_rect(r, kCardRadius, top, bottom);
    }
    list.bordered_rect(r, kCardRadius, look::kClear, 1.5f,
                       gfx::mix(look::kInk.with_alpha(0.16f), c.with_alpha(0.75f), lit));
}

// One answer of a card. The bright one is in the colour; the others are
// quiet plates.
void draw_answer(gfx::DrawList &list, const ui::Fonts &fonts, const Rect &r, std::string_view label,
                 bool bright, Color c, float press)
{
    list.push_transform(1.0f - 0.035f * press, r.cx(), r.cy(), 0.0f, 0.0f);
    if (bright)
    {
        list.glow(r, kAnswerRadius, 18.0f, c.with_alpha(0.22f));
        list.gradient_rect(r, kAnswerRadius, gfx::mix(c, look::kInk, 0.3f),
                           gfx::mix(c, look::kNight, 0.12f));
        list.gradient_rect({r.x + 2.0f, r.y + 2.0f, r.w - 4.0f, r.h * 0.5f}, kAnswerRadius - 2.0f,
                           look::kInk.with_alpha(0.2f), look::kInk.with_alpha(0.0f));
    }
    else
    {
        list.rounded_rect(r, kAnswerRadius, look::kInk.with_alpha(0.08f));
        list.bordered_rect(r, kAnswerRadius, look::kClear, 1.5f, look::kInk.with_alpha(0.18f));
    }
    ui::text_fit(list, fonts.semibold, label, r.cx(), r.cy() + 25.0f * 0.35f, 25.0f, r.w - 32.0f,
                 bright ? look::kNight : look::kInk, Align::center);
    list.pop_transform();
}

// A king on a square of the board: white on a dark one, black on a light one.
void draw_king(const app::Context &ctx, gfx::DrawList &list, chess::Color color, const Rect &r)
{
    const board::BoardTheme &squares = ctx.board_theme();
    const bool white = color == chess::Color::white;
    list.rounded_rect(r, r.w * 0.22f, Color::rgb(white ? squares.dark : squares.light));
    ctx.pieces->draw(list, {color, chess::Role::king}, r);
}

// The way out of a screen that has nothing else to offer, as the hint row says it.
void draw_back_pill(gfx::DrawList &list, const ui::Fonts &fonts, float cx, float cy)
{
    constexpr float kGlyph = 34.0f;
    const float glyph = ui::button_width(ui::Button::circle, kGlyph);
    const char *back = tr("Back");
    const float label = fonts.semibold.measure(back, 24.0f);
    const float x = cx - (glyph + 12.0f + label) * 0.5f;
    const Rect pill{x - 24.0f, cy - 28.0f, glyph + 12.0f + label + 48.0f, 56.0f};
    list.rounded_rect(pill, 28.0f, look::kInk.with_alpha(0.07f));
    list.bordered_rect(pill, 28.0f, look::kClear, 1.5f, look::kInk.with_alpha(0.14f));
    ui::draw_button(list, fonts, ui::GlyphStyle::dark(), ui::Button::circle, x, cy, kGlyph);
    ui::text(list, fonts.semibold, back, x + glyph + 12.0f, cy + 24.0f * 0.35f, 24.0f, look::kInk);
}

class GameScene final : public app::Scene
{
  public:
    // Pass & Play on this controller; resumes the saved game when there is one.
    explicit GameScene(app::Context &ctx)
    {
        build();
        std::string data;
        std::string saved;
        if (save::read_file(ctx.data_root + kSaveName, &data))
        {
            const save::Decoded decoded = save::decode(save::Kind::game, data);
            if (decoded.ok)
                saved = decoded.payload;
        }
        start_local(ctx, saved);
    }

    // A game whose moves, clocks and result come from link.
    explicit GameScene(std::unique_ptr<GameLink> link)
        : online_(true), link_(std::move(link)), auto_flip_(false)
    {
        build();
    }

    void enter(app::Context &) override
    {
        // The screen assembles again.
        since_ = 0.0f;
    }

    app::Transition update(app::Context &ctx, const InputFrame &input, float dt) override
    {
        confetti_.update(dt);
        if (announce_start_)
        {
            announce_start_ = false;
            ctx.cue(audio::Cue::game_start);
        }
        tick(ctx, dt);
        app::Transition transition = step(ctx, input, dt);
        animate(ctx, dt);
        return transition;
    }

    void tick(app::Context &ctx, float dt) override
    {
        if (!link_)
            return;
        link_->pump(ctx, dt);
        if (link_->version() != link_version_)
            sync_from_link(ctx);
    }

    void draw(app::Context &ctx, app::Frame &frame) const override
    {
        gfx::DrawList &list = frame.scene;
        if (failed())
        {
            draw_failed(ctx, list);
            const ui::Hint hints[] = {{ui::Button::circle, TR("Back")}};
            app::draw_hints(ctx, list, hints, 1);
            return;
        }
        if (loading())
        {
            draw_loading(ctx, list);
            return;
        }

        // A game that has just loaded takes the waiting board's place gently.
        const float arrived = tween::clamp01(arrive_.value);
        const bool arriving = arrived < 0.995f;
        if (arriving)
            list.push_opacity(arrived);

        // ---- the board, on its own light
        part(list, 0, 22.0f,
             [&]()
             {
                 const float breath = calm_ ? 0.5f : ui::breathe(ctx.time, 2.6f);
                 look::frame_board(list, app::kBoardSquares, glow_ink_.value(), glow_.value);
                 const board::Overlay overlay = board_overlay(ctx);
                 view_.draw(list, *ctx.fonts, *ctx.pieces, app::kBoardSquares, overlay,
                            ctx.board_theme(), ctx.time);
                 draw_board_light(list, app::kBoardSquares, glow_ink_.value(),
                                  glow_.value * (0.7f + 0.3f * breath));
             });

        // ---- the column: a player, the band over the moves, the row, a player
        part(list, 1, 18.0f, [&]() { top_.draw(ctx, list); });
        part(list, 2, 18.0f, [&]() { draw_band(ctx, list); });
        part(list, 3, 18.0f,
             [&]()
             {
                 moves_.draw(ctx, list);
                 if (game_.ply_count() == 0)
                 {
                     // An empty table says who starts instead of showing bare headers.
                     const float cy = kTable.y + 46.0f + (kTable.h - 46.0f) * 0.5f;
                     ui::text_fit(list, ctx.fonts->regular,
                                  online_ ? tr("Waiting for the first move")
                                          : tr("White moves first"),
                                  kTable.cx(), cy + 24.0f * 0.35f, 24.0f, kTable.w - 48.0f,
                                  look::kInk.with_alpha(look::kMuted), Align::center);
                 }
             });
        part(list, 4, 18.0f, [&]() { draw_row(ctx, list); });
        part(list, 5, 18.0f, [&]() { bottom_.draw(ctx, list); });
        if (arriving)
            list.pop_opacity();

        // What floats above the frosted board: the promotion's choice, a
        // question, the menu, the result.
        const bool result = result_in_.value > 0.01f;
        const bool offer = offer_in_.value > 0.01f;
        const bool promotion = promotion_in_.value > 0.01f || input_.promotion_open();
        const bool modal = pause_.visible() || result;
        frame.glass = modal || offer || promotion;
        const std::uint32_t glass = frame.glass ? frame.glass_texture : 0;
        if (promotion)
            draw_promotion(ctx, frame.overlay, glass);
        if (offer)
            draw_offer(ctx, frame.overlay, glass);
        ui::Canvas over = app::canvas_for(ctx, frame.overlay, glass);
        pause_.draw(over);
        if (result)
            draw_result(ctx, frame.overlay, glass);
        confetti_.draw(frame.overlay);
        draw_hints(ctx, frame, modal);
    }

    // The sky of the game screens, and the game's own colour.
    app::look::Mood mood() const override
    {
        look::Mood mood = look::mood(look::Section::game);
        mood.accent = accent_;
        return mood;
    }

    const char *name() const override
    {
        return online_ ? "online-game" : "pass-and-play";
    }

  private:
    enum class Focus
    {
        board,
        actions,
    };
    // Which buttons the action row carries.
    enum class Row
    {
        none,
        local,
        online,
        finished,
    };
    enum class Offer
    {
        none,
        draw,
        takeback,
    };
    // How the game ended, for the player (or for the winner at this console).
    enum class Verdict
    {
        none, // aborted: nobody won or lost
        win,
        loss,
        draw,
    };

    // ---- construction ------------------------------------------------------

    void build()
    {
        top_.set_bounds(kTopPanel);
        bottom_.set_bounds(kBottomPanel);
        moves_.set_bounds(kTable);

        actions_.style.mode = ui::GroupMode::actions;
        actions_.style.gap = kRowGap;
        actions_.set_bounds({kColumn, kRow.y, kGroupWidth, kRow.h});
        actions_.set_active(false);
        resign_.hint = tr("Keep holding");
        resign_.style.role = ui::ButtonRole::danger;
        resign_.style.glyph = ui::Button::square;
        resign_.style.action = Action::west;
        resign_.set_bounds({kColumn + kGroupWidth + kRowGap, kRow.y,
                            kColumnWidth - kGroupWidth - kRowGap, kRow.h});
        resign_.set_active(false);

        pause_.kicker = tr("Game menu");
        pause_.style.layout = ui::PauseLayout::center;
        pause_.style.width = kMenuWidth;

        // The two dialogs answer the controller and play the cues; the screen
        // draws their cards. They are told where those are, so their sounds
        // come from the right place.
        result_.style.width = kResultCard.w;
        // The result's own fanfare is its entrance: no dialog chime on top.
        result_.style.sounds.open = audio::Cue::count;
        offer_dialog_.set_bounds(kOfferCard);
        // The offer has already announced itself with the challenge cue.
        offer_dialog_.style.sounds.open = audio::Cue::count;

        set_accent(look::accent(look::Section::game));
        glow_ink_.snap(accent_);
        review_amount_.snap(0.0f);
    }

    // The game's colour reaches everything that is lit in it.
    void set_accent(Color accent)
    {
        accent_ = accent;
        top_.set_accent(accent);
        bottom_.set_accent(accent);
        moves_.set_accent(accent);
        look::tint(accent, pause_);
    }

    // ---- who and what ------------------------------------------------------

    // The link has not delivered the game yet.
    bool loading() const
    {
        return online_ && link_ && !link_->ready() && link_->error().empty();
    }
    // ... and never will: only the way back is left.
    bool failed() const
    {
        return online_ && (!link_ || (!link_->ready() && !link_->error().empty()));
    }
    bool playing() const
    {
        if (over_)
            return false;
        return !online_ || link_->status() == GameStatus::playing;
    }
    // Lichess aborts a game that has barely begun instead of scoring it.
    bool early() const
    {
        return game_.ply_count() < 2;
    }
    bool human_controls(chess::Color color) const
    {
        if (!online_)
            return true;
        return link_->ready() && color == link_->my_color();
    }
    PlayerInfo player(chess::Color color) const
    {
        if (online_)
            return link_->player(color);
        PlayerInfo info;
        // Pass & Play has no names: the two players are their sides.
        info.name = side_name(color);
        return info;
    }
    // -1 nobody, 0 White, 1 Black.
    int winner() const
    {
        return online_ ? link_->winner() : local_winner_;
    }
    const chess::Position &shown_position() const
    {
        if (history_ >= 0)
            return game_.position_at(static_cast<std::size_t>(history_));
        return game_.position();
    }
    chess::Move shown_last_move() const
    {
        const std::size_t ply =
            history_ >= 0 ? static_cast<std::size_t>(history_) : game_.ply_count();
        return ply > 0 ? game_.move_at(ply - 1) : chess::Move{};
    }

    // ---- the game ----------------------------------------------------------

    void start_local(app::Context &ctx, const std::string &saved_moves)
    {
        game_.reset();
        if (!saved_moves.empty() && !game_.apply_uci_moves(saved_moves))
            game_.reset();
        over_ = false;
        result_shown_ = false;
        local_winner_ = -1;
        end_reason_.clear();
        history_ = -1;
        flip_delay_ = -1.0f;
        focus_ = Focus::board;
        view_.set_orientation(auto_flip_ ? game_.position().turn() : chess::Color::white, false);
        view_.snap(game_.position());
        input_.clear_selection();
        input_.clear_premove();
        input_.place_cursor(
            chess::make_square(4, game_.position().turn() == chess::Color::white ? 1 : 6));
        // Heard on the next frame: a screen is built before it is on the
        // stack, and a cue asked for then would be lost.
        announce_start_ = true;
        if (!ctx.reduced_motion())
            view_.deal();
        refresh_table();
        finish_if_over();
    }

    void play_move_cue(app::Context &ctx, const chess::Position &before, const chess::Move &move,
                       const chess::Position &after)
    {
        if (after.is_checkmate() || after.in_check())
            ctx.cue(audio::Cue::check);
        else if (move.promotion)
            ctx.cue(audio::Cue::promote);
        else if (before.is_castle(move))
            ctx.cue(audio::Cue::castle);
        else if (before.is_capture(move))
            ctx.cue(audio::Cue::capture);
        else
            ctx.cue(audio::Cue::move);
    }

    void apply_move(app::Context &ctx, const chess::Move &move, bool animate)
    {
        const chess::Position before = game_.position();
        if (!game_.play(move))
            return;
        const chess::Position &after = game_.position();
        view_.play(before, move, after, animate && !ctx.reduced_motion() ? 1.0f : 0.0f);
        play_move_cue(ctx, before, move, after);
        history_ = -1;
        refresh_table();
        if (!online_)
        {
            save_local(ctx);
            if (auto_flip_)
                flip_delay_ = kFlipDelay;
        }
        finish_if_over();
        if (!online_ && over_)
            save_local(ctx); // a finished game is not resumed
    }

    void sync_from_link(app::Context &ctx)
    {
        if (!link_->ready())
            return;
        const std::size_t before_count = game_.ply_count();
        const chess::Position before = game_.position();
        const std::string before_moves = join_uci(game_);
        const bool first = link_version_ == 0;
        if (first)
        {
            chess::Position initial = chess::Position::start();
            if (!link_->initial_fen().empty())
                chess::Position::from_fen(link_->initial_fen(), &initial);
            game_.reset(initial);
            view_.set_orientation(link_->my_color(), false);
            input_.place_cursor(
                chess::make_square(4, link_->my_color() == chess::Color::white ? 1 : 6));
            ctx.cue(audio::Cue::game_start);
            describe_game();
        }
        link_version_ = link_->version();
        const std::string &moves = link_->moves();
        if (moves == before_moves && !first)
        {
            finish_if_over();
            return;
        }
        if (!game_.apply_uci_moves(moves))
        {
            ctx.notify(tr("Could not follow the game's moves"), app::Note::warning);
            return;
        }
        const std::size_t count = game_.ply_count();
        if (!first && count == before_count + 1 &&
            moves.compare(0, before_moves.size(), before_moves) == 0)
        {
            const chess::Move &last = game_.move_at(count - 1);
            view_.play(before, last, game_.position(), ctx.reduced_motion() ? 0.0f : 1.0f);
            // Our own optimistic move was already heard; only the opponent's sounds.
            play_move_cue(ctx, before, last, game_.position());
        }
        else
        {
            view_.snap(game_.position());
        }
        history_ = -1;
        refresh_table();
        // Fire a queued premove as soon as it is our turn.
        if (link_->status() == GameStatus::playing && game_.position().turn() == link_->my_color())
        {
            chess::Move premove;
            if (input_.take_premove(game_.position(), &premove))
            {
                apply_move(ctx, premove, true);
                link_->send_move(premove);
            }
        }
        finish_if_over();
    }

    // The game arrived: what the band says it is, and the colour of its speed.
    void describe_game()
    {
        band_label_ = single_spaced(link_->speed_label());
        // The label starts with the speed's name, in the player's language
        // (or in English, from a link that does not translate it).
        struct Named
        {
            const char *name;
            look::Speed speed;
        };
        static constexpr Named kSpeeds[] = {{TR("UltraBullet"), look::Speed::bullet},
                                            {TR("Bullet"), look::Speed::bullet},
                                            {TR("Blitz"), look::Speed::blitz},
                                            {TR("Rapid"), look::Speed::rapid},
                                            {TR("Classical"), look::Speed::classical},
                                            {TR("Correspondence"), look::Speed::correspondence}};
        const Named *found =
            std::find_if(std::begin(kSpeeds), std::end(kSpeeds),
                         [&](const Named &named)
                         {
                             return starts_with_word(band_label_, tr(named.name)) ||
                                    starts_with_word(band_label_, named.name);
                         });
        has_speed_ = found != std::end(kSpeeds);
        if (!has_speed_)
            return;
        speed_ = found->speed;
        set_accent(look::speed_color(speed_));
        glow_ink_.snap(accent_);
    }

    void finish_if_over()
    {
        if (over_)
            return;
        if (online_)
        {
            if (!is_over(link_->status()))
                return;
        }
        else
        {
            switch (game_.outcome())
            {
            case chess::Outcome::ongoing:
                return;
            case chess::Outcome::checkmate:
                end_reason_ = tr("Checkmate");
                local_winner_ = game_.position().turn() == chess::Color::white ? 1 : 0;
                break;
            case chess::Outcome::stalemate:
                end_reason_ = tr("Stalemate");
                break;
            case chess::Outcome::insufficient_material:
                end_reason_ = tr("Insufficient material");
                break;
            case chess::Outcome::fifty_moves:
                end_reason_ = tr("Fifty-move rule");
                break;
            case chess::Outcome::threefold:
                end_reason_ = tr("Threefold repetition");
                break;
            }
        }
        finish(kResultDelay);
    }

    void finish(float delay)
    {
        over_ = true;
        result_shown_ = false;
        result_delay_ = delay;
        input_.clear_selection();
        input_.clear_premove();
        // The question is moot once the game has ended.
        offer_dialog_.dismiss();
        offer_ = Offer::none;
    }

    // Hold Square: resign, or abort a game that has barely begun. In Pass &
    // Play the side to move gives up.
    void give_up(app::Context &ctx)
    {
        if (online_)
        {
            if (early())
                link_->abort();
            else
                link_->resign();
            return;
        }
        const chess::Color loser = game_.position().turn();
        local_winner_ = loser == chess::Color::white ? 1 : 0;
        end_reason_ = loser == chess::Color::white ? tr("White resigned") : tr("Black resigned");
        finish(0.5f);
        save_local(ctx);
    }

    void undo(app::Context &ctx)
    {
        if (online_ || !game_.undo())
            return;
        // With the board turning, the player holding the controller takes
        // back their own last move: the reply goes with it.
        if (game_.ply_count() > 0 && auto_flip_)
            game_.undo();
        view_.snap(game_.position());
        if (auto_flip_)
            view_.set_orientation(game_.position().turn(), false);
        over_ = false;
        result_shown_ = false;
        local_winner_ = -1;
        end_reason_.clear();
        history_ = -1;
        flip_delay_ = -1.0f;
        input_.clear_selection();
        refresh_table();
        save_local(ctx);
        ctx.cue(audio::Cue::back);
    }

    void flip(app::Context &ctx)
    {
        view_.set_orientation(chess::opposite(view_.orientation()), !ctx.reduced_motion());
    }

    void new_game(app::Context &ctx)
    {
        if (online_)
            return;
        over_ = false;
        save::write_atomic(ctx.data_root + kSaveName, save::encode(save::Kind::game, 1, ""));
        start_local(ctx, "");
    }

    void save_local(app::Context &ctx) const
    {
        if (online_)
            return;
        const std::string payload = over_ ? std::string() : join_uci(game_);
        save::write_atomic(ctx.data_root + kSaveName, save::encode(save::Kind::game, 1, payload));
    }

    // The move table and the review strip follow the game and the shown ply.
    void refresh_table()
    {
        sans_.clear();
        for (std::size_t i = 0; i < game_.ply_count(); ++i)
            sans_.push_back(game_.san_at(i));
        const int plies = static_cast<int>(sans_.size());
        const chess::Position &initial = game_.initial();
        const bool black_first = initial.turn() == chess::Color::black;
        moves_.set_moves(sans_, history_ >= 0 ? history_ : plies, initial.fullmove_number(),
                         black_first);
        if (history_ < 0)
            return;
        if (history_ == 0)
        {
            review_text_ = tr("Starting position");
        }
        else
        {
            // The move as a game's record writes it: "12. Nxd4", "12… Nf6".
            const int slot = history_ - 1 + (black_first ? 1 : 0);
            const std::string move = std::to_string(initial.fullmove_number() + slot / 2) +
                                     (slot % 2 == 0 ? ". " : "\xE2\x80\xA6 ") +
                                     sans_[static_cast<std::size_t>(history_ - 1)];
            review_text_ = fill(tr("After {0}"), {move});
        }
        review_count_ = std::to_string(history_) + " / " + std::to_string(plies);
    }

    // ---- results -----------------------------------------------------------

    std::string result_title() const
    {
        const int who = winner();
        if (online_)
        {
            if (link_->status() == GameStatus::aborted)
                return tr("Game aborted");
            if (who < 0)
                return trc("result", "Draw");
            const bool mine = (who == 0) == (link_->my_color() == chess::Color::white);
            return mine ? tr("Victory") : tr("Defeat");
        }
        if (who < 0)
            return trc("result", "Draw");
        return who == 0 ? tr("White wins") : tr("Black wins");
    }

    std::string result_reason() const
    {
        if (!online_)
            return end_reason_;
        switch (link_->status())
        {
        case GameStatus::mate:
            return tr("Checkmate");
        case GameStatus::resign:
            return link_->winner() >= 0
                       ? (link_->winner() == 0 ? tr("Black resigned") : tr("White resigned"))
                       : tr("Resignation");
        case GameStatus::stalemate:
            return tr("Stalemate");
        case GameStatus::timeout:
            return tr("The opponent left the game");
        case GameStatus::draw:
            return tr("Draw agreed or claimed");
        case GameStatus::outoftime:
            return tr("Time out");
        case GameStatus::insufficient:
            return tr("Insufficient material");
        case GameStatus::aborted:
            return tr("The game was aborted");
        case GameStatus::no_start:
            return tr("The game did not start in time");
        case GameStatus::cheat:
            return tr("Cheat detected");
        default:
            return {};
        }
    }

    void open_result(app::Context &ctx)
    {
        result_shown_ = true;
        const int who = winner();
        bool won = !online_ && who >= 0;
        bool lost = false;
        if (online_ && who >= 0)
        {
            won = (who == 0) == (link_->my_color() == chess::Color::white);
            lost = !won;
        }
        ctx.cue(won ? audio::Cue::victory : lost ? audio::Cue::defeat : audio::Cue::draw);
        if (won)
            confetti_.burst(accent_, static_cast<std::uint32_t>(ctx.time * 1000.0f),
                            ctx.reduced_motion());

        ui::DialogContent content;
        content.icon = won    ? ui::StatusKind::success
                       : lost ? ui::StatusKind::danger
                              : ui::StatusKind::info;
        content.title = result_title();
        content.body = result_reason();
        const int plies = static_cast<int>(game_.ply_count());
        if (plies > 0)
        {
            const int count = (plies + 1) / 2;
            if (!content.body.empty())
                content.body += " \xC2\xB7 ";
            content.body += plural(TR("{0} move"), TR("{0} moves"), count);
        }
        // The ceremony draws these itself: the verdict, why, and what game it was.
        verdict_ = won                                                 ? Verdict::win
                   : lost                                              ? Verdict::loss
                   : online_ && link_->status() == GameStatus::aborted ? Verdict::none
                                                                       : Verdict::draw;
        result_word_ = content.title;
        result_reason_ = content.body;
        result_speed_ = online_ ? single_spaced(link_->speed_label()) : std::string();
        result_age_ = 0.0f;
        if (online_)
            content.body += "\n" + single_spaced(link_->speed_label());

        result_commands_.clear();
        const auto add = [&](const char *label, int command)
        {
            ui::DialogButton button;
            button.label = label;
            if (result_commands_.empty())
                button.kind = ui::ButtonKind::primary;
            content.buttons.push_back(std::move(button));
            result_commands_.push_back(command);
        };
        if (!online_)
            add(tr("New game"), kNewGame);
        if (plies > 0)
            add(tr("Review game"), kReview);
        add(trc("button", "Menu"), kLeave);
        result_.open(std::move(content), *ctx.feedback);
        save_local(ctx);
    }

    // ---- the game menu -----------------------------------------------------

    void open_menu(app::Context &ctx)
    {
        std::vector<ui::ListItem> items;
        const auto add = [&](const char *title, int command, bool disabled = false)
        {
            ui::ListItem item;
            item.title = title;
            item.tag = command;
            item.disabled = disabled;
            items.push_back(std::move(item));
        };
        add(tr("Resume"), kResume);
        if (online_)
        {
            const bool live = playing();
            if (live && early())
                add(tr("Abort game"), kAbort);
            if (live && !early())
            {
                add(tr("Offer draw"), kOfferDraw);
                add(tr("Propose takeback"), kTakeback);
            }
            if (live && link_->opponent_gone_seconds() == 0)
                add(tr("Claim victory"), kClaimVictory);
            add(tr("Flip board"), kFlip);
            add(tr("Return to menu"), kLeave);
            const chess::Color them = chess::opposite(link_->my_color());
            pause_.title = fill(tr("vs {0}"), {player(them).name});
            if (over_)
                pause_.subtitle = tr("The game is over");
            else if (live && link_->has_clock())
                pause_.subtitle = tr("Your clock keeps running while you are here");
            else
                pause_.subtitle = single_spaced(link_->speed_label());
        }
        else
        {
            add(tr("Take back move"), kUndo, game_.ply_count() == 0);
            add(tr("Flip board"), kFlip);
            add(tr("Auto-flip"), kAutoFlip);
            items.back().value = auto_flip_ ? tr("On") : tr("Off");
            add(tr("New game"), kNewGame);
            add(tr("Return to menu"), kLeave);
            pause_.title = tr("Pass & Play");
            pause_.subtitle = tr("Two players, one controller");
        }
        // The menu is as wide as its subtitle needs in the player's language,
        // and never narrower than it is in English.
        const float line =
            ctx.fonts->regular.measure(pause_.subtitle, pause_.style.subtitle_size) + 1.0f;
        pause_.style.width =
            std::clamp(line + 2.0f * pause_.style.padding, kMenuWidth, kMenuWidest);
        pause_.set_items(std::move(items));
        pause_.open(*ctx.feedback);
    }

    // Closes the menu after a choice: the choice's own cue was the goodbye.
    void close_menu(app::Context &ctx)
    {
        const audio::Cue voice = pause_.style.sounds.close;
        pause_.style.sounds.close = audio::Cue::count;
        pause_.close(*ctx.feedback);
        pause_.style.sounds.close = voice;
    }

    app::Transition step_menu(app::Context &ctx, const InputFrame &input)
    {
        if (pause_.handle(input, *ctx.feedback) != ui::Event::activated)
            return app::Transition::stay();
        const int focus = pause_.focus();
        if (focus < 0 || focus >= static_cast<int>(pause_.items().size()))
            return app::Transition::stay();
        switch (pause_.items()[static_cast<std::size_t>(focus)].tag)
        {
        case kAutoFlip:
            // A switch: the menu stays open and shows the new state.
            auto_flip_ = !auto_flip_;
            pause_.item(focus).value = auto_flip_ ? tr("On") : tr("Off");
            if (auto_flip_)
                view_.set_orientation(game_.position().turn(), !ctx.reduced_motion());
            return app::Transition::stay();
        case kLeave:
            save_local(ctx);
            pause_.dismiss();
            return app::Transition::pop();
        case kUndo:
            undo(ctx);
            break;
        case kFlip:
            flip(ctx);
            break;
        case kNewGame:
            new_game(ctx);
            break;
        case kOfferDraw:
            offer_draw(ctx);
            break;
        case kTakeback:
            propose_takeback(ctx);
            break;
        case kAbort:
            link_->abort();
            break;
        case kClaimVictory:
            link_->claim_victory();
            break;
        default:
            break;
        }
        close_menu(ctx);
        return app::Transition::stay();
    }

    void offer_draw(app::Context &ctx)
    {
        link_->draw(true);
        ctx.notify(tr("Draw offer sent"), app::Note::info);
    }

    void propose_takeback(app::Context &ctx)
    {
        link_->takeback(true);
        ctx.notify(tr("Takeback proposal sent"), app::Note::info);
    }

    // ---- one frame of input ------------------------------------------------

    app::Transition step(app::Context &ctx, const InputFrame &input, float dt)
    {
        ui::Feedback &feedback = *ctx.feedback;
        if (failed() || loading())
        {
            // Nothing to play yet: the only way is back.
            if (input.is_pressed(Action::back) || (failed() && input.is_pressed(Action::confirm)))
            {
                ctx.cue(audio::Cue::back);
                return app::Transition::pop();
            }
            return app::Transition::stay();
        }
        if (online_)
        {
            std::string message;
            if (link_->take_refusal(&message))
            {
                // Our optimistic move was refused: follow the server again.
                game_.apply_uci_moves(link_->moves());
                history_ = -1;
                input_.clear_selection();
                view_.snap(game_.position());
                refresh_table();
                ctx.cue(audio::Cue::illegal);
                ctx.notify(message.empty() ? tr("Move refused") : message, app::Note::warning);
            }
            watch_clock(ctx);
            watch_opponent(ctx);
        }

        if (pause_.is_open())
            return step_menu(ctx, input);

        if (over_ && !result_shown_)
        {
            result_delay_ -= dt;
            if (result_delay_ <= 0.0f)
                open_result(ctx);
        }
        if (result_.is_open())
            return step_result(ctx, input);

        if (step_offer(ctx, input))
            return app::Transition::stay();

        if (input.is_pressed(Action::menu) || input.is_pressed(Action::touch))
        {
            open_menu(ctx);
            return app::Transition::stay();
        }
        if (step_history(ctx, input))
            return app::Transition::stay();
        if (step_resign(ctx, input))
            return app::Transition::stay();

        if (focus_ == Focus::actions)
        {
            if (input.is_pressed(Action::back) || input.is_pressed(Action::north))
            {
                focus_ = Focus::board;
                ctx.cue(audio::Cue::back);
            }
            else
            {
                const ui::Event event = actions_.handle(input, feedback);
                if (event == ui::Event::activated)
                {
                    press_.trigger();
                    run_action(ctx, actions_.item(actions_.focus()).tag);
                }
                else if (event == ui::Event::refused && !input.nav_repeat)
                {
                    // The end of the row, or a plate that cannot be used now.
                    refusal_.trigger();
                }
            }
            return app::Transition::stay();
        }
        if (input.is_pressed(Action::north) && !input_.promotion_open())
        {
            focus_ = Focus::actions;
            ctx.cue(audio::Cue::focus);
            return app::Transition::stay();
        }

        board::InputRules rules;
        rules.white = human_controls(chess::Color::white);
        rules.black = human_controls(chess::Color::black);
        rules.premoves = online_ && ctx.settings->premoves;
        rules.auto_queen = ctx.settings->auto_queen;
        rules.locked = !playing();
        chess::Move move;
        const board::BoardInput::Result result =
            input_.update(input, dt, game_.position(), view_.orientation(), rules, feedback, &move);
        if (result == board::BoardInput::Result::move)
        {
            apply_move(ctx, move, true);
            if (online_)
                link_->send_move(move);
        }
        else if (result == board::BoardInput::Result::back)
        {
            open_menu(ctx);
        }
        return app::Transition::stay();
    }

    app::Transition step_result(app::Context &ctx, const InputFrame &input)
    {
        if (result_.handle(input, *ctx.feedback) != ui::Event::activated)
            return app::Transition::stay();
        answer_press_.trigger();
        const int choice = result_.choice();
        if (choice < 0 || choice >= static_cast<int>(result_commands_.size()))
            return app::Transition::stay();
        switch (result_commands_[static_cast<std::size_t>(choice)])
        {
        case kNewGame:
            new_game(ctx);
            break;
        case kReview:
            if (game_.ply_count() > 0)
            {
                // Step through the finished game from the first move with L2 / R2.
                history_ = 1;
                view_.snap(shown_position());
                refresh_table();
            }
            break;
        case kLeave:
            return app::Transition::pop();
        default:
            break;
        }
        return app::Transition::stay();
    }

    // A draw or a takeback the opponent asks for. True while the question
    // has the controller.
    bool step_offer(app::Context &ctx, const InputFrame &input)
    {
        Offer offered = Offer::none;
        if (online_ && playing())
        {
            const chess::Color them = chess::opposite(link_->my_color());
            if (link_->draw_offered_by(them))
                offered = Offer::draw;
            else if (link_->takeback_offered_by(them))
                offered = Offer::takeback;
        }
        if (offered != offer_)
        {
            offer_ = offered;
            if (offered == Offer::none)
            {
                // Withdrawn, or settled by the next move.
                offer_dialog_.close(*ctx.feedback);
            }
            else
            {
                ui::DialogContent content;
                content.icon = ui::StatusKind::question;
                content.title = offered == Offer::draw ? tr("Your opponent offers a draw")
                                                       : tr("Your opponent asks for a takeback");
                content.body = offered == Offer::draw
                                   ? tr("Accept and the game ends in a draw.")
                                   : tr("Accept and their last move is taken back.");
                ui::DialogButton decline;
                decline.label = tr("Decline");
                ui::DialogButton accept;
                accept.label = tr("Accept");
                accept.kind = ui::ButtonKind::primary;
                content.buttons = {decline, accept};
                // A stray press while placing a piece must not end the game.
                content.default_button = 0;
                ctx.cue(audio::Cue::challenge);
                offer_dialog_.open(std::move(content), *ctx.feedback);
                // The card draws these itself, and keeps them while it leaves.
                offer_shown_ = offered;
                offer_age_ = 0.0f;
            }
        }
        if (!offer_dialog_.is_open())
            return false;
        const ui::Event event = offer_dialog_.handle(input, *ctx.feedback);
        if (event == ui::Event::activated || event == ui::Event::cancelled)
        {
            // Circle declines: a question left open would keep the board locked.
            const bool yes = event == ui::Event::activated && offer_dialog_.choice() == 1;
            answer_press_.trigger();
            if (offer_ == Offer::draw)
                link_->draw(yes);
            else
                link_->takeback(yes);
        }
        return true;
    }

    // L2 / R2 step through the moves; while an earlier position shows, the
    // game waits behind it.
    bool step_history(app::Context &ctx, const InputFrame &input)
    {
        const int plies = static_cast<int>(game_.ply_count());
        const bool earlier = input.is_pressed(Action::jump_prev);
        if (earlier || input.is_pressed(Action::jump_next))
        {
            const int from = history_ >= 0 ? history_ : plies;
            const int ply = std::clamp(from + (earlier ? -1 : 1), 0, plies);
            if (ply == from)
            {
                // The first and the last position answer softly.
                ctx.cue(audio::Cue::error, 1.0f, 0.0f, 0.5f);
                refusal_.trigger();
                return true;
            }
            history_ = ply == plies ? -1 : ply;
            view_.snap(shown_position());
            refresh_table();
            ctx.cue(audio::Cue::cursor);
            return true;
        }
        if (history_ < 0)
            return false;
        if (input.is_pressed(Action::confirm) || input.is_pressed(Action::back))
        {
            history_ = -1;
            view_.snap(game_.position());
            refresh_table();
            ctx.cue(audio::Cue::back);
        }
        return true;
    }

    bool step_resign(app::Context &ctx, const InputFrame &input)
    {
        if (!ctx.settings->confirm_resign)
        {
            // The player turned the safeguard off: one press is enough.
            if (!playing() || !input.is_pressed(Action::west))
                return false;
            ctx.cue(audio::Cue::select);
            give_up(ctx);
            return true;
        }
        if (resign_.handle(input, *ctx.feedback) != ui::Event::activated)
            return false;
        give_up(ctx);
        return true;
    }

    void run_action(app::Context &ctx, int command)
    {
        switch (command)
        {
        case kUndo:
            undo(ctx);
            break;
        case kFlip:
            flip(ctx);
            break;
        case kOfferDraw:
            offer_draw(ctx);
            break;
        case kTakeback:
            propose_takeback(ctx);
            break;
        case kResult:
            if (result_shown_)
            {
                ctx.cue(audio::Cue::modal_open);
                result_.open(*ctx.feedback);
                result_age_ = 0.0f;
            }
            break;
        default:
            break;
        }
    }

    // Low-time warning and the last-seconds countdown on our own clock.
    void watch_clock(app::Context &ctx)
    {
        if (!link_->has_clock() || link_->status() != GameStatus::playing ||
            game_.position().turn() != link_->my_color())
            return;
        const long long ms = link_->clock_ms(link_->my_color());
        const bool low = ms < kLowTimeMs;
        if (low && !last_low_)
            ctx.cue(audio::Cue::low_time);
        last_low_ = low;
        const long long second = ms / 1000;
        if (ms < kCountdownMs && second != last_tick_second_)
            ctx.cue(audio::Cue::clock_tick);
        last_tick_second_ = second;
    }

    // The moment the win can be claimed is said once; the panel keeps saying it.
    void watch_opponent(app::Context &ctx)
    {
        const int gone = playing() ? link_->opponent_gone_seconds() : -1;
        if (gone == 0 && !gone_told_)
            ctx.notify(tr("Your opponent left. Claim victory from the menu."), app::Note::info);
        gone_told_ = gone == 0;
    }

    // ---- what moves by itself ----------------------------------------------

    void animate(app::Context &ctx, float dt)
    {
        ctx.calm(actions_, resign_, pause_, result_, offer_dialog_);
        calm_ = ctx.reduced_motion();
        const float quick = calm_ ? 60.0f : 18.0f;
        if (failed())
        {
            failed_body_ = link_ ? link_->error() : std::string(tr("The game is not available."));
            if (!failure_shown_)
            {
                failure_shown_ = true;
                failed_age_ = 0.0f;
                ctx.cue(audio::Cue::error);
            }
            failed_age_ += dt;
            return;
        }
        failure_shown_ = false;
        if (loading())
        {
            loading_age_ += dt;
            arrive_.snap(0.0f);
            since_ = 0.0f;
            return;
        }
        since_ += dt;
        arrive_.target = 1.0f;
        arrive_.update(dt, calm_ ? 60.0f : 14.0f);

        view_.update(dt, board_overlay(ctx));
        if (flip_delay_ >= 0.0f)
        {
            flip_delay_ -= dt;
            if (flip_delay_ < 0.0f && !over_ && view_.orientation() != game_.position().turn())
            {
                view_.set_orientation(game_.position().turn(), !ctx.reduced_motion());
                // Keep the cursor at the same spot on screen for the next player.
                input_.place_cursor(static_cast<chess::Square>(63 - input_.cursor()));
            }
        }

        sync_row(ctx);
        // The panel under the board belongs to whoever sits at its near side.
        const chess::Color lower = view_.orientation();
        fill_panel(bottom_, lower);
        fill_panel(top_, chess::opposite(lower));

        const bool modal = pause_.is_open() || result_.is_open() || offer_dialog_.is_open();
        const bool row = focus_ == Focus::actions && history_ < 0 && !modal;
        actions_.set_active(row);
        top_.update(ctx, dt);
        bottom_.update(ctx, dt);
        moves_.update(ctx, dt);
        actions_.update(dt);
        resign_.update(dt);
        pause_.update(dt);
        result_.update(dt);
        offer_dialog_.update(dt);
        review_amount_.target = history_ >= 0 ? 1.0f : 0.0f;
        review_amount_.update(dt, quick);
        const int plies = static_cast<int>(game_.ply_count());
        if (history_ >= 0 && plies > 0)
            review_share_.target = static_cast<float>(history_) / static_cast<float>(plies);
        review_share_.update(dt, quick);

        // ---- the board's light: brightest while the player is to move
        const bool live = playing();
        const bool mine = live && human_controls(game_.position().turn());
        const bool check = mine && game_.position().in_check();
        glow_.target = !live ? 0.25f : mine ? 1.0f : 0.4f;
        glow_.update(dt, calm_ ? 60.0f : 6.0f);
        glow_ink_.target(check ? look::kBad : accent_);
        glow_ink_.update(dt, calm_ ? 60.0f : 8.0f);

        // ---- the band: what is happening, in a line that changes without a jump
        describe_state();
        band_swap_.target = 1.0f;
        band_swap_.update(dt, calm_ ? 60.0f : 13.0f);

        // ---- the row: one ring between its plates
        const int count = actions_.count();
        if (count > 0)
            row_ring_.target(actions_.item_rect(std::clamp(actions_.focus(), 0, count - 1)));
        row_ring_alpha_.target = row ? 1.0f : 0.0f;
        if (row_ring_alpha_.value < 0.02f && count > 0)
            row_ring_.snap(actions_.item_rect(std::clamp(actions_.focus(), 0, count - 1)));
        row_ring_.update(dt, quick);
        row_ring_alpha_.update(dt, quick);
        for (int i = 0; i < 2; ++i)
        {
            tween::Spring &lit = chip_lit_[static_cast<std::size_t>(i)];
            lit.target = row && actions_.focus() == i ? 1.0f : 0.0f;
            lit.update(dt, quick);
        }
        if (calm_)
        {
            press_.value = 0.0f;
            refusal_.value = 0.0f;
            answer_press_.value = 0.0f;
        }
        press_.update(dt, 10.0f);
        refusal_.update(dt, 9.0f);
        // Letting go of Square, or finishing the hold: the plate answers.
        if (resign_.holding())
            held_ = true;
        else if (held_)
        {
            held_ = false;
            if (!calm_)
                resign_pulse_.trigger(0.6f);
        }
        resign_pulse_.update(dt, 7.0f);

        // ---- what floats: the promotion's plate, the question, the result
        promotion_in_.target = input_.promotion_open() ? 1.0f : 0.0f;
        promotion_in_.update(dt, calm_ ? 60.0f : 16.0f);
        offer_in_.target = offer_dialog_.is_open() ? 1.0f : 0.0f;
        offer_in_.update(dt, calm_ ? 60.0f : 13.0f);
        offer_age_ += dt;
        result_in_.target = result_.is_open() ? 1.0f : 0.0f;
        result_in_.update(dt, calm_ ? 60.0f : (result_.is_open() ? 11.0f : 20.0f));
        if (result_.is_open())
            result_age_ += dt;
        // One ring glides between the answers of whichever card has the controller.
        const bool answering = result_.is_open() || offer_dialog_.is_open();
        if (answering)
        {
            const Rect at = result_.is_open() ? result_answer(result_.focus())
                                              : offer_answer(offer_dialog_.focus());
            answer_ring_.target(at);
            if (answer_alpha_.value < 0.02f)
                answer_ring_.snap(at);
        }
        answer_alpha_.target = answering ? 1.0f : 0.0f;
        answer_alpha_.update(dt, quick);
        answer_ring_.update(dt, quick);
        answer_press_.update(dt, 9.0f);
    }

    // What the band says is happening right now.
    void describe_state()
    {
        std::string text;
        Color tone = look::kInk.with_alpha(look::kMuted);
        bool live = false;
        const chess::Color turn = game_.position().turn();
        const bool check = game_.position().in_check();
        if (over_)
        {
            text = result_title();
        }
        else if (history_ >= 0)
        {
            text = tr("Looking back");
        }
        else if (!online_)
        {
            if (turn == chess::Color::white)
                text = check ? tr("White is in check") : tr("White to move");
            else
                text = check ? tr("Black is in check") : tr("Black to move");
            tone = check ? look::kBad : accent_;
            live = true;
        }
        else if (!link_->connected())
        {
            text = tr("Reconnecting\xE2\x80\xA6");
            tone = look::kGold;
            live = true;
        }
        else if (link_->status() != GameStatus::playing)
        {
            text = tr("Not started");
        }
        else if (turn == link_->my_color())
        {
            text = check ? tr("You are in check") : tr("Your move");
            tone = check ? look::kBad : accent_;
            live = true;
        }
        else
        {
            text = tr("Their move");
        }
        if (text == band_state_)
        {
            band_tone_ = tone;
            band_live_ = live;
            return;
        }
        band_old_ = std::move(band_state_);
        band_old_tone_ = band_tone_;
        band_state_ = std::move(text);
        band_tone_ = tone;
        band_live_ = live;
        band_swap_.snap(calm_ || band_old_.empty() ? 1.0f : 0.0f);
    }

    // The action row follows the game: what can be asked for while it runs,
    // what is left to do once it is over.
    void sync_row(const app::Context &ctx)
    {
        const Row wanted = over_ ? Row::finished : online_ ? Row::online : Row::local;
        if (wanted != row_)
        {
            row_ = wanted;
            switch (wanted)
            {
            case Row::local:
                actions_.set_items(
                    {group_item(tr("Undo"), kUndo), group_item(tr("Flip board"), kFlip)});
                break;
            case Row::online:
                actions_.set_items({group_item(tr("Offer draw"), kOfferDraw),
                                    group_item(tr("Takeback"), kTakeback)});
                break;
            default:
                actions_.set_items(
                    {group_item(tr("Result"), kResult), group_item(tr("Flip board"), kFlip)});
                break;
            }
            actions_.set_focus(0);
        }
        const bool live = playing();
        waiting_ = {false, false};
        if (row_ == Row::local)
        {
            actions_.item(0).disabled = game_.ply_count() == 0;
        }
        else if (row_ == Row::online)
        {
            // Our own pending offer shows on its button until it is answered.
            const chess::Color me = link_->my_color();
            const bool draw_sent = link_->draw_offered_by(me);
            const bool takeback_sent = link_->takeback_offered_by(me);
            actions_.item(0).label = draw_sent ? tr("Draw offered") : tr("Offer draw");
            actions_.item(0).disabled = !live || early() || draw_sent;
            actions_.item(1).label = takeback_sent ? tr("Takeback asked") : tr("Takeback");
            actions_.item(1).disabled = !live || early() || takeback_sent;
            waiting_ = {draw_sent, takeback_sent};
        }
        else
        {
            actions_.item(0).disabled = !result_shown_;
        }

        // Without the safeguard one press is enough, and the button says so.
        const bool instant = !ctx.settings->confirm_resign;
        const bool aborts = online_ && early();
        if (!live)
            resign_.label = over_ ? tr("Game over") : tr("Not started");
        else if (instant)
            resign_.label = aborts ? tr("Abort") : tr("Resign");
        else
            resign_.label = aborts ? tr("Hold to abort") : tr("Hold to resign");
        resign_.set_disabled(!live);
        // Once the game is over the button is a quiet plate, not a red one
        // that can no longer be pressed.
        resign_.style.role = live ? ui::ButtonRole::danger : ui::ButtonRole::secondary;
        resign_.style.glyph = live ? ui::Button::square : ui::Button::none;
    }

    void fill_panel(PlayerPanel &panel, chess::Color color)
    {
        const PlayerInfo info = player(color);
        const bool ai = info.ai_level > 0;
        panel.set_player(info.name, ai ? std::string() : rating_text(info.rating, info.provisional),
                         info.title, color);
        panel.set_captured(captured_by(shown_position(), color));

        const bool live = playing();
        const bool to_move = live && game_.position().turn() == color;
        const bool check = to_move && game_.position().in_check();
        ui::Presence presence = ui::Presence::none;
        if (!online_)
        {
            // The names already say White and Black: the note says what is up.
            const int who = winner();
            const char *note = trc("player", "Waiting");
            if (over_ && who >= 0)
                note = (who == 0) == (color == chess::Color::white) ? tr("Won") : tr("Lost");
            else if (over_)
                note = trc("player", "Draw");
            else if (to_move)
                note = check ? tr("In check") : tr("To move");
            panel.set_note(note);
            panel.set_presence(presence);
            // No clock, but the panel of the side to move is still the lit one.
            panel.set_clock(-1, false, to_move);
            return;
        }

        // Whose pieces these are ("White", "Black · level 3" for the computer),
        // then what is up with this player: one pattern for each state, with
        // the side where it says {0}.
        std::string note = side_name(color);
        if (ai)
            note = fill(tr("{0} \xC2\xB7 level {1}"), {note, std::to_string(info.ai_level)});
        const char *state = nullptr;
        std::string seconds;
        if (over_)
        {
            const int who = winner();
            if (who >= 0 && (who == 0) == (color == chess::Color::white))
                state = tr("{0} \xC2\xB7 won");
            else if (who < 0 && link_->status() != GameStatus::aborted)
                state = tr("{0} \xC2\xB7 draw");
        }
        else if (color == link_->my_color())
        {
            presence = link_->connected() ? ui::Presence::online : ui::Presence::offline;
            if (!link_->connected())
                state = tr("{0} \xC2\xB7 reconnecting\xE2\x80\xA6");
            else if (to_move)
                state = check ? tr("{0} \xC2\xB7 in check") : tr("{0} \xC2\xB7 your move");
        }
        else
        {
            const int gone = live ? link_->opponent_gone_seconds() : -1;
            presence = gone >= 0 ? ui::Presence::away : ui::Presence::online;
            seconds = std::to_string(gone);
            if (gone > 0)
                state = tr("{0} \xC2\xB7 left, claim the win in {1} s");
            else if (gone == 0)
                state = tr("{0} \xC2\xB7 left, claim the win in the menu");
            else if (to_move)
                state = check ? tr("{0} \xC2\xB7 in check") : tr("{0} \xC2\xB7 thinking");
        }
        if (state != nullptr)
            note = fill(state, {note, seconds});
        panel.set_note(note);
        panel.set_presence(presence);
        if (link_->has_clock())
            panel.set_clock(link_->clock_ms(color), true, to_move, kLowTimeMs);
        else
            panel.set_clock(-1, false, to_move);
    }

    // ---- drawing -----------------------------------------------------------

    // What the board shows besides the pieces. The cursor is there only while
    // the board has the controller: one focus on screen at a time.
    board::Overlay board_overlay(const app::Context &ctx) const
    {
        board::Overlay overlay;
        if (history_ < 0 && !over_)
            input_.fill_overlay(overlay);
        if (focus_ != Focus::board)
            overlay.cursor = chess::kNoSquare;
        overlay.last_move = shown_last_move();
        overlay.coordinates = ctx.settings->coordinates;
        overlay.show_dests = ctx.settings->show_dests;
        overlay.cursor_color = accent_;
        return overlay;
    }

    // One part of the screen inside its arrival: it fades in and settles upward.
    template <typename Draw>
    void part(gfx::DrawList &list, int index, float distance, const Draw &draw) const
    {
        const float in = calm_ ? 1.0f : look::rise(since_, index, 0.06f, 0.45f);
        if (in >= 0.999f)
        {
            draw();
            return;
        }
        list.push_opacity(in);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in, distance));
        draw();
        list.pop_transform();
        list.pop_opacity();
    }

    // What game this is, and what is happening in it.
    void draw_band(const app::Context &ctx, gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Rect b = kBand;
        look::panel(list, b, 0.3f, accent_, 14.0f);
        const Rect tile{b.x + 8.0f, b.cy() - 15.0f, 30.0f, 30.0f};
        list.rounded_rect(tile, 9.0f, accent_.with_alpha(0.18f));
        if (has_speed_)
            look::speed_icon(list, tile.inset(6.5f), speed_, accent_);
        else
            draw_sign(list, Sign::pawns, tile.inset(6.5f), accent_);

        // The state, at the right: the old words leave upward as the new ones
        // settle. It may take the larger half of the band; the label has the rest.
        const float x = tile.x + tile.w + 14.0f;
        const float right = b.x + b.w - 18.0f;
        const float baseline = b.cy() + 22.0f * 0.35f;
        const float swap = tween::clamp01(band_swap_.value);
        float state_width = 0.0f;
        const auto words = [&](const std::string &text, Color tone, float alpha, float dy)
        {
            if (text.empty() || alpha <= 0.01f)
                return;
            list.push_opacity(alpha);
            const float width = ui::text_fit(list, fonts.semibold, text, right, baseline + dy,
                                             22.0f, (right - x) * 0.6f, tone, Align::right);
            list.pop_opacity();
            state_width = std::max(state_width, width);
        };
        words(band_old_, band_old_tone_, 1.0f - swap, -7.0f * swap);
        words(band_state_, band_tone_, swap, 9.0f * (1.0f - swap));
        if (band_live_)
            look::live_dot(list, right - state_width - 18.0f, b.cy(), 5.0f, band_tone_, ctx.time,
                           calm_);

        const float room = right - state_width - 44.0f - x;
        kicker_fit(list, fonts, online_ ? std::string_view(band_label_) : tr("Pass & Play"), x,
                   b.cy() + 15.0f * 0.36f, room, accent_, Align::left, 15.0f);
    }

    // The action row, or, while an earlier position shows, what is on the board.
    void draw_row(const app::Context &ctx, gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const float review = tween::clamp01(review_amount_.value);
        if (review < 0.99f)
        {
            list.push_opacity(1.0f - review);
            const int count = std::min(actions_.count(), 2);
            for (int i = 0; i < count; ++i)
                draw_chip(ctx, list, i);
            draw_resign(ctx, list);
            if (row_ring_alpha_.value > 0.01f)
            {
                Rect ring = row_ring_.value();
                ring.x += ui::shake(refusal_.value, ctx.time, 8.0f);
                look::ring(list, ring.inset(2.5f * press_.value), accent_, row_ring_alpha_.value,
                           kChipRadius);
            }
            list.pop_opacity();
        }
        if (review <= 0.01f)
            return;

        // The strip that says which position is on the board.
        list.push_opacity(review);
        list.push_transform(1.0f, 0.0f, 0.0f, ui::shake(refusal_.value, ctx.time, 8.0f),
                            calm_ ? 0.0f : 8.0f * (1.0f - review));
        look::panel(list, kRow, 0.6f, accent_, kChipRadius);
        const float left = kRow.x + 22.0f;
        const float right = kRow.x + kRow.w - 22.0f;
        // The label takes a third of the strip at most; what is on the board
        // has the room between it and the count.
        const float label =
            kicker_fit(list, fonts, tr("Review"), left, kRow.cy() + 15.0f * 0.36f - 3.0f,
                       (right - left) / 3.0f, accent_, Align::left, 15.0f);
        const float count =
            look::ticker(list, fonts, review_count_, right, kRow.cy() + 22.0f * 0.35f - 3.0f, 22.0f,
                         look::kInk.with_alpha(look::kMuted), Align::right);
        const float x = left + label + 22.0f;
        ui::text_fit(list, fonts.semibold, review_text_, x, kRow.cy() + 24.0f * 0.35f - 3.0f, 24.0f,
                     right - count - 22.0f - x, look::kInk);
        // How far through the game the shown position is.
        look::level(list, {left, kRow.y + kRow.h - 12.0f, right - left, 4.0f},
                    tween::clamp01(review_share_.value), accent_);
        list.pop_transform();
        list.pop_opacity();
    }

    // A plate of the row: a sign on a tile, and what it asks for.
    void draw_chip(const app::Context &ctx, gfx::DrawList &list, int index) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const ui::GroupItem &item = actions_.items()[static_cast<std::size_t>(index)];
        const Rect r = actions_.item_rect(index);
        const float lit = chip_lit_[static_cast<std::size_t>(index)].value;
        // An offer of ours that waits for its answer keeps its plate lit.
        const float waiting = waiting_[static_cast<std::size_t>(index)] ? 1.0f : 0.0f;
        const float dim = item.disabled && waiting < 0.5f ? 0.42f : 1.0f;
        look::lift(list, r, lit, accent_, ctx.time, calm_, kChipRadius);
        look::panel(list, r, std::max(lit, 0.5f * waiting), accent_, kChipRadius);
        list.push_opacity(dim);
        const Rect tile{r.x + 12.0f, r.cy() - 20.0f, 40.0f, 40.0f};
        list.rounded_rect(tile, 12.0f, gfx::mix(accent_.with_alpha(0.16f), accent_, lit));
        draw_sign(list, sign_of(item.tag), tile.inset(9.0f), gfx::mix(accent_, look::kNight, lit));
        const float x = tile.x + tile.w + 14.0f;
        const float room = r.x + r.w - 16.0f - x - (waiting > 0.5f ? 22.0f : 0.0f);
        draw_plate_words(list, fonts.semibold, plate_words(fonts.semibold, item.label, 23.0f, room),
                         x, r.cy(), look::kInk);
        list.pop_opacity();
        if (waiting > 0.5f)
            look::live_dot(list, r.x + r.w - 22.0f, r.cy(), 4.5f, accent_, ctx.time, calm_);
    }

    // Giving up: a plate that fills while Square is held.
    void draw_resign(const app::Context &ctx, gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Rect r = resign_.bounds();
        const bool live = !resign_.disabled();
        const Color c = look::kBad;
        const float p = tween::clamp01(resign_.progress());
        const float held = std::max(p, resign_pulse_.value);
        if (!live)
        {
            look::panel(list, r, 0.0f, look::kInk, kChipRadius);
            ui::text_fit(list, fonts.semibold, resign_.label, r.cx(), r.cy() + 23.0f * 0.35f, 23.0f,
                         r.w - 32.0f, look::kInk.with_alpha(look::kFaint), Align::center);
            return;
        }
        list.push_transform(1.0f - 0.03f * p, r.cx(), r.cy(), 0.0f, 0.0f);
        if (held > 0.01f)
            list.glow(r, kChipRadius, 18.0f, c.with_alpha(0.35f * held));
        look::panel(list, r, 0.75f + 0.25f * held, c, kChipRadius);
        if (p > 0.004f)
        {
            // The fill keeps the plate's corners: it starts as wide as they need.
            const float least = 2.0f * kChipRadius;
            const Rect sweep{r.x, r.y, tween::lerp(least, r.w, p), r.h};
            list.gradient_rect_h(sweep, kChipRadius, c.with_alpha(0.75f * std::min(p * 8.0f, 1.0f)),
                                 c.with_alpha(0.5f * std::min(p * 8.0f, 1.0f)));
        }
        // The words: the label, or for a moment after a tap, what to do. The
        // glyph and the words stand together in the middle of the plate.
        constexpr float kGlyph = 30.0f;
        constexpr ui::Button glyph = ui::Button::square;
        const float glyph_width = ui::button_width(glyph, kGlyph);
        const PlateWords words =
            plate_words(fonts.semibold, resign_.hinting() ? resign_.hint : resign_.label, 23.0f,
                        r.w - 44.0f - glyph_width);
        const float width = glyph_width + 10.0f + words.width;
        const float x = r.cx() - width * 0.5f;
        ui::draw_button(list, fonts, ui::GlyphStyle::dark(), glyph, x, r.cy(), kGlyph);
        draw_plate_words(list, fonts.semibold, words, x + glyph_width + 10.0f, r.cy(), look::kInk);
        list.pop_transform();
    }

    // ---- what floats ---------------------------------------------------------

    // The column of pieces a pawn can become stands on a plate of frosted
    // glass in the game's colour.
    void draw_promotion(const app::Context &ctx, gfx::DrawList &list, std::uint32_t glass) const
    {
        const float in = tween::clamp01(promotion_in_.value);
        const Rect area = app::kBoardSquares;
        const Rect target = view_.square_rect(area, input_.cursor());
        // As the picker does: the column grows away from the edge the pawn
        // promotes on.
        const bool downward = target.y < area.y + area.h * 0.5f;
        const float height = target.h * (1.0f + 3.0f * tween::cubic_out(in));
        const Rect plate{target.x - 8.0f,
                         (downward ? target.y : target.y + target.h - height) - 8.0f,
                         target.w + 16.0f, height + 16.0f};
        const float radius = plate.w * 0.5f;
        if (in > 0.01f)
        {
            list.push_opacity(in);
            if (glass != 0)
                list.glass(glass, plate, radius, Color{1.0f, 1.0f, 1.0f, 1.0f});
            list.gradient_rect(plate, radius, gfx::mix(look::kInk, accent_, 0.5f).with_alpha(0.42f),
                               gfx::mix(look::kNight, accent_, 0.3f).with_alpha(0.5f));
            list.pop_opacity();
        }
        // The picker dims the board under its pieces; the plate's edge goes
        // over that veil so it stays in the game's colour.
        input_.draw_promotion(list, *ctx.pieces, view_, area, game_.position(), accent_);
        if (in > 0.01f)
            list.bordered_rect(plate, radius, look::kClear, 2.5f, accent_.with_alpha(0.9f * in));
    }

    Rect offer_answer(int index) const
    {
        const float width = (kOfferCard.w - 2.0f * kCardPad - 16.0f) * 0.5f;
        return {
            kOfferCard.x + kCardPad + static_cast<float>(std::clamp(index, 0, 1)) * (width + 16.0f),
            kOfferCard.y + kOfferCard.h - kCardPad - kAnswerHeight - 8.0f, width, kAnswerHeight};
    }

    // What the opponent asks for, as a lit card over the moves, with its two answers.
    void draw_offer(const app::Context &ctx, gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const float in = tween::clamp01(offer_in_.value);
        const Rect card = kOfferCard;
        const float cx = card.cx();
        const bool draw = offer_shown_ == Offer::draw;
        list.push_opacity(in);
        list.push_transform(calm_ ? 1.0f : tween::lerp(0.95f, 1.0f, tween::back_out(in)), cx,
                            card.cy(), 0.0f, calm_ ? 0.0f : -18.0f * (1.0f - in));
        draw_glass_card(list, glass, card, accent_, 1.0f);

        // The sign of what is asked, in a ring that keeps calling.
        const float sy = card.y + 86.0f;
        if (!calm_)
        {
            const float along = std::fmod(offer_age_, 1.6f) / 1.6f;
            list.ring(cx, sy, 40.0f * (1.0f + 0.9f * along), 2.5f,
                      accent_.with_alpha(0.55f * (1.0f - along)));
        }
        list.circle(cx, sy, 40.0f, accent_.with_alpha(0.16f));
        list.ring(cx, sy, 40.0f, 3.0f, accent_);
        draw_sign(list, draw ? Sign::equal : Sign::back, {cx - 22.0f, sy - 22.0f, 44.0f, 44.0f},
                  accent_);

        const float room = card.w - 2.0f * kCardPad;
        const chess::Color them =
            online_ && link_ ? chess::opposite(link_->my_color()) : chess::Color::black;
        look::kicker(list, fonts,
                     fonts.semibold.font->fit(ui::upper(player(them).name), 16.0f, room, 3.0f), cx,
                     card.y + 166.0f, accent_, Align::center);
        const char *title =
            draw ? tr("Your opponent offers a draw") : tr("Your opponent asks for a takeback");
        ui::text(list, fonts.display, title, cx, card.y + 214.0f,
                 fitted(fonts.display, title, 36.0f, room), look::kInk, Align::center);
        ui::text_fit(list, fonts.regular,
                     draw ? tr("Accept and the game ends in a draw.")
                          : tr("Accept and their last move is taken back."),
                     cx, card.y + 256.0f, 24.0f, room, look::kInk.with_alpha(look::kMuted),
                     Align::center);

        const int focus = offer_dialog_.focus();
        const int chosen = offer_dialog_.choice();
        draw_answer(list, fonts, offer_answer(0), tr("Decline"), false, accent_,
                    chosen != 1 ? answer_press_.value : 0.0f);
        draw_answer(list, fonts, offer_answer(1), tr("Accept"), true, accent_,
                    chosen == 1 ? answer_press_.value : 0.0f);
        if (answer_alpha_.value > 0.01f && offer_dialog_.is_open())
            look::ring(list, answer_ring_.value(), focus == 1 ? accent_ : look::kInk,
                       answer_alpha_.value, kAnswerRadius);
        list.pop_transform();
        list.pop_opacity();
    }

    Rect result_answer(int index) const
    {
        const int count = std::max(1, static_cast<int>(result_commands_.size()));
        const float width =
            (kResultCard.w - 2.0f * kCardPad - static_cast<float>(count - 1) * 16.0f) /
            static_cast<float>(count);
        return {kResultCard.x + kCardPad +
                    static_cast<float>(std::clamp(index, 0, count - 1)) * (width + 16.0f),
                kResultCard.y + kResultCard.h - kCardPad - kAnswerHeight, width, kAnswerHeight};
    }

    // 0..1 for a step of the result's ceremony that starts `at` seconds in.
    float ceremony(float at, float duration = 0.45f) const
    {
        return calm_ ? 1.0f : tween::clamp01((result_age_ - at) / duration);
    }

    // The result: the mark pops, the verdict follows in its colour, then why,
    // then the two players, then the answers.
    void draw_result(const app::Context &ctx, gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const float in = tween::clamp01(result_in_.value);
        const Rect card = kResultCard;
        const float cx = card.cx();
        const Color tone = verdict_ == Verdict::win    ? look::kGood
                           : verdict_ == Verdict::loss ? look::kBad
                                                       : accent_;
        const Color ink = verdict_ == Verdict::win    ? look::kGood
                          : verdict_ == Verdict::loss ? look::kBad
                                                      : look::kInk;
        // The screen behind steps back.
        list.rounded_rect({0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0.0f,
                          look::kNight.with_alpha(0.5f * in));
        list.push_opacity(in);
        list.push_transform(calm_ ? 1.0f : tween::lerp(0.92f, 1.0f, tween::back_out(in)), cx,
                            card.cy(), 0.0f, calm_ ? 0.0f : 22.0f * (1.0f - in));
        draw_glass_card(list, glass, card, tone, 1.0f);
        list.push_clip(card.inset(4.0f));
        look::halo(list, {cx - 300.0f, card.y - 190.0f, 600.0f, 600.0f}, tone, 0.2f);
        list.pop_clip();

        // ---- the mark
        const float my = card.y + 96.0f;
        const float mark = calm_ ? 1.0f : tween::back_out(ceremony(0.1f, 0.5f));
        if (!calm_)
        {
            // Two rings leave it once, as it lands.
            for (int i = 0; i < 2; ++i)
            {
                const float along = ceremony(0.2f + 0.14f * static_cast<float>(i), 0.9f);
                if (along > 0.0f && along < 1.0f)
                    list.ring(cx, my, 48.0f + 86.0f * tween::cubic_out(along), 3.0f,
                              tone.with_alpha(0.5f * (1.0f - along)));
            }
        }
        if (mark > 0.01f)
        {
            const float radius = 48.0f * mark;
            list.glow({cx - radius, my - radius, 2.0f * radius, 2.0f * radius}, radius, 22.0f,
                      tone.with_alpha(0.4f));
            list.circle(cx, my, radius, tone);
            const Sign sign = verdict_ == Verdict::win    ? Sign::check
                              : verdict_ == Verdict::loss ? Sign::cross
                              : verdict_ == Verdict::draw ? Sign::equal
                                                          : Sign::flag;
            draw_sign(list, sign,
                      {cx - radius * 0.6f, my - radius * 0.6f, radius * 1.2f, radius * 1.2f},
                      look::kNight);
        }

        // ---- the verdict, and why
        const float room = card.w - 2.0f * kCardPad;
        const auto step = [&](float at, const auto &draw)
        {
            const float t = tween::cubic_out(ceremony(at));
            list.push_opacity(t);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, calm_ ? 0.0f : 16.0f * (1.0f - t));
            draw();
            list.pop_transform();
            list.pop_opacity();
        };
        step(0.24f,
             [&]()
             {
                 ui::text(list, fonts.display, result_word_, cx, card.y + 228.0f,
                          fitted(fonts.display, result_word_, 72.0f, room), ink, Align::center);
             });
        step(0.36f,
             [&]()
             {
                 ui::text_fit(list, fonts.regular, result_reason_, cx, card.y + 274.0f, 26.0f, room,
                              look::kInk.with_alpha(0.86f), Align::center);
                 if (!result_speed_.empty())
                     kicker_fit(list, fonts, result_speed_, cx, card.y + 308.0f, room,
                                look::kInk.with_alpha(look::kFaint + 0.12f), Align::center, 15.0f);
             });

        // ---- the two players: the winner's plate is lit
        step(0.48f,
             [&]()
             {
                 const int who = winner();
                 const float width = (room - 16.0f) * 0.5f;
                 for (int i = 0; i < 2; ++i)
                 {
                     const chess::Color color = i == 0 ? chess::Color::white : chess::Color::black;
                     const PlayerInfo info = player(color);
                     const bool won = who == i;
                     const bool lost = who >= 0 && !won;
                     const Rect r{card.x + kCardPad + static_cast<float>(i) * (width + 16.0f),
                                  card.y + 334.0f, width, 72.0f};
                     list.rounded_rect(r, 18.0f, look::kInk.with_alpha(0.05f));
                     if (won)
                         list.gradient_rect_h(r, 18.0f, look::kGood.with_alpha(0.24f),
                                              look::kGood.with_alpha(0.05f));
                     list.bordered_rect(r, 18.0f, look::kClear, 1.5f,
                                        won ? look::kGood.with_alpha(0.6f)
                                            : look::kInk.with_alpha(0.1f));
                     draw_king(ctx, list, color, {r.x + 12.0f, r.y + 12.0f, 48.0f, 48.0f});
                     const char *word = verdict_ == Verdict::none ? ""
                                        : won                     ? tr("Won")
                                        : lost                    ? tr("Lost")
                                                                  : trc("player", "Draw");
                     // The word may take half of the plate beside the king;
                     // the name and the rating have what it leaves.
                     const float end = r.x + r.w - 18.0f;
                     const float x = r.x + 74.0f;
                     const float tag = ui::text_fit(
                         list, fonts.semibold, word, end, r.cy() + 21.0f * 0.35f, 21.0f,
                         (end - x) * 0.5f, won ? look::kGood : look::kInk.with_alpha(look::kMuted),
                         Align::right);
                     const std::string rating =
                         info.ai_level > 0 ? fill(tr("level {0}"), {std::to_string(info.ai_level)})
                                           : rating_text(info.rating, info.provisional);
                     const float name_room = end - tag - 16.0f - x;
                     ui::text(list, fonts.semibold,
                              fonts.semibold.font->fit(info.name, 23.0f, name_room), x,
                              rating.empty() ? r.cy() + 23.0f * 0.35f : r.y + 32.0f, 23.0f,
                              look::kInk.with_alpha(lost ? look::kMuted : 1.0f));
                     if (!rating.empty())
                         ui::text_fit(list, fonts.regular, rating, x, r.y + 56.0f, 19.0f, name_room,
                                      look::kInk.with_alpha(look::kFaint + 0.14f));
                 }
             });

        // ---- the answers, last
        step(0.62f,
             [&]()
             {
                 const std::vector<ui::DialogButton> &buttons = result_.content().buttons;
                 const int chosen = result_.choice();
                 for (int i = 0; i < static_cast<int>(buttons.size()); ++i)
                     draw_answer(list, fonts, result_answer(i),
                                 buttons[static_cast<std::size_t>(i)].label, i == 0, accent_,
                                 i == chosen ? answer_press_.value : 0.0f);
                 if (answer_alpha_.value > 0.01f && result_.is_open())
                     look::ring(list, answer_ring_.value(), look::kInk, answer_alpha_.value,
                                kAnswerRadius);
             });
        list.pop_transform();
        list.pop_opacity();
    }

    // The game is on its way: an empty board waits on its frame, rings leave
    // its middle, and the column is drawn in outline.
    void draw_loading(const app::Context &ctx, gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Rect squares = app::kBoardSquares;
        const float age = loading_age_;
        const float in = calm_ ? 1.0f : look::rise(age, 0, 0.06f, 0.5f);
        const float breath = calm_ ? 0.5f : ui::breathe(age, 2.2f);
        list.push_opacity(in);
        look::frame_board(list, squares, accent_, 1.2f + 0.8f * breath);
        board::draw_mini_board(list, *ctx.pieces, ctx.board_theme(), squares, chess::Position{},
                               chess::Color::white, {}, squares.w / 8.0f * 0.16f);
        list.rounded_rect(squares, 0.0f, look::kNight.with_alpha(0.5f));
        const float cx = squares.cx();
        const float cy = squares.cy() - 24.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float offset = static_cast<float>(i) / 3.0f;
            const float along = calm_ ? offset + 0.15f : std::fmod(age * 0.35f + offset, 1.0f);
            list.ring(cx, cy, 64.0f + 190.0f * along, 3.0f,
                      accent_.with_alpha(0.6f * std::min(along * 6.0f, 1.0f) * (1.0f - along)));
        }
        list.circle(cx, cy, 64.0f, look::kNight.with_alpha(0.7f));
        list.ring(cx, cy, 64.0f, 3.0f, accent_);
        const float turn = calm_ ? 0.6f : age * 3.4f;
        list.arc(cx, cy, 50.0f, 6.0f, std::fmod(turn, kTau), 1.6f, accent_);
        list.arc(cx, cy, 50.0f, 6.0f, std::fmod(turn + kTau * 0.5f, kTau), 1.6f,
                 accent_.with_alpha(0.45f));
        // The plate is as wide as its words, in whatever language they are.
        const char *words = tr("Connecting to the game");
        const float wide = fonts.semibold.measure(words, 26.0f);
        const Rect plate{cx - wide * 0.5f - 28.0f, cy + 292.0f, wide + 56.0f, 60.0f};
        list.rounded_rect(plate, 30.0f, look::kNight.with_alpha(0.72f));
        list.bordered_rect(plate, 30.0f, look::kClear, 1.5f, accent_.with_alpha(0.45f));
        ui::text(list, fonts.semibold, words, cx, plate.cy() + 26.0f * 0.35f, 26.0f, look::kInk,
                 Align::center);
        list.pop_opacity();

        // The column in outline: where the players, the moves and the row will be.
        const Rect outlines[] = {kTopPanel, kBand, kTable, kRow, kBottomPanel};
        for (int i = 0; i < 5; ++i)
        {
            const float part_in = calm_ ? 1.0f : look::rise(age, 1 + i, 0.07f, 0.5f);
            const Rect &r = outlines[i];
            // A slow wave runs down the column while it waits.
            const float wave =
                calm_ ? 0.5f : ui::breathe(age - 0.18f * static_cast<float>(i), 1.8f);
            list.push_opacity(part_in);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(part_in, 18.0f));
            look::panel(list, r, 0.15f + 0.25f * wave, accent_,
                        std::min(look::kRadius, r.h * 0.32f));
            const Color bar = look::kInk.with_alpha(0.06f + 0.05f * wave);
            if (r.h > 120.0f && r.h < 140.0f)
            {
                // A player: where the portrait, the name and the clock go.
                list.circle(r.x + 24.0f + 42.0f, r.cy(), 42.0f, bar);
                list.rounded_rect({r.x + 132.0f, r.cy() - 26.0f, 280.0f, 20.0f}, 10.0f, bar);
                list.rounded_rect({r.x + 132.0f, r.cy() + 10.0f, 180.0f, 16.0f}, 8.0f, bar);
                list.rounded_rect({r.x + r.w - 24.0f - 170.0f, r.cy() - 22.0f, 170.0f, 44.0f},
                                  12.0f, bar);
            }
            else if (r.h > 140.0f)
            {
                for (int line = 0; line < 5; ++line)
                    list.rounded_rect({r.x + 28.0f, r.y + 40.0f + static_cast<float>(line) * 60.0f,
                                       (line % 2 == 0 ? 0.82f : 0.6f) * (r.w - 56.0f), 18.0f},
                                      9.0f, bar);
            }
            else
            {
                list.rounded_rect({r.x + 20.0f, r.cy() - 9.0f, r.w * 0.34f, 18.0f}, 9.0f, bar);
            }
            list.pop_transform();
            list.pop_opacity();
        }
        const ui::Hint hints[] = {{ui::Button::circle, TR("Back")}};
        app::draw_hints(ctx, list, hints, 1);
    }

    // The game cannot be opened: one lit panel says why, in the words it had.
    void draw_failed(const app::Context &ctx, gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Color c = look::kGold;
        const Rect p = kFailedPanel;
        const float cx = p.cx();
        const float in = calm_ ? 1.0f : tween::clamp01(failed_age_ / 0.4f);
        list.push_opacity(tween::cubic_out(in));
        list.push_transform(calm_ ? 1.0f : tween::lerp(0.94f, 1.0f, tween::back_out(in)), cx,
                            p.cy(), 0.0f, 0.0f);
        look::lift(list, p, 0.7f, c, ctx.time, calm_);
        look::panel(list, p, 0.7f, c);
        const float sy = p.y + 104.0f;
        list.push_clip(p.inset(6.0f));
        look::halo(list, {cx - 200.0f, sy - 200.0f, 400.0f, 400.0f}, c, 0.2f);
        list.pop_clip();
        list.circle(cx, sy, 52.0f, c.with_alpha(0.14f));
        list.ring(cx, sy, 52.0f, 3.0f, c);
        list.rounded_rect({cx - 5.0f, sy - 27.0f, 10.0f, 34.0f}, 5.0f, c);
        list.circle(cx, sy + 21.0f, 6.5f, c);
        kicker_fit(list, fonts, tr("Lichess game"), cx, p.y + 204.0f, p.w - 96.0f, c, Align::center,
                   16.0f);
        const char *title = tr("Could not open the game");
        ui::text(list, fonts.display, title, cx, p.y + 262.0f,
                 fitted(fonts.display, title, 40.0f, p.w - 96.0f), look::kInk, Align::center);
        ui::paragraph(list, fonts.regular, failed_body_, cx, p.y + 314.0f, 24.0f, p.w - 120.0f,
                      34.0f, look::kInk.with_alpha(look::kMuted), 3, Align::center);
        draw_back_pill(list, fonts, cx, p.y + p.h - 58.0f);
        list.pop_transform();
        list.pop_opacity();
    }

    void draw_hints(app::Context &ctx, app::Frame &frame, bool modal) const
    {
        if (modal)
        {
            if (result_in_.value > 0.01f)
            {
                const ui::Hint hints[] = {{ui::Button::cross, TR("Select")},
                                          {ui::Button::dpad, TR("Choose")}};
                app::draw_hints(ctx, frame.overlay, hints, 2, true);
            }
            else
            {
                const ui::Hint hints[] = {{ui::Button::cross, TR("Select")},
                                          {ui::Button::circle, TR("Resume")}};
                app::draw_hints(ctx, frame.overlay, hints, 2, true);
            }
            return;
        }
        gfx::DrawList &list = frame.scene;
        if (offer_dialog_.is_open())
        {
            const ui::Hint hints[] = {{ui::Button::cross, TR("Select")},
                                      {ui::Button::dpad, TR("Choose")},
                                      {ui::Button::circle, TR("Decline")}};
            draw_column_hints(ctx, list, hints, 3);
        }
        else if (history_ >= 0)
        {
            const ui::Hint hints[] = {{ui::Button::l2, TR("Step"), ui::Button::r2},
                                      {ui::Button::cross, TR("Back to game")},
                                      {ui::Button::options, TR("Menu")}};
            draw_column_hints(ctx, list, hints, 3);
        }
        else if (focus_ == Focus::actions)
        {
            const ui::Hint hints[] = {{ui::Button::dpad, TR("Choose")},
                                      {ui::Button::cross, TR("Select")},
                                      {ui::Button::circle, TR("Board")},
                                      {ui::Button::options, TR("Menu")}};
            draw_column_hints(ctx, list, hints, 4);
        }
        else if (!playing())
        {
            const ui::Hint hints[] = {{ui::Button::l2, TR("History"), ui::Button::r2},
                                      {ui::Button::triangle, TR("Actions")},
                                      {ui::Button::options, TR("Menu")}};
            draw_column_hints(ctx, list, hints, 3);
        }
        else
        {
            const ui::Hint hints[] = {{ui::Button::dpad, TR("Move")},
                                      {ui::Button::cross, TR("Place")},
                                      {ui::Button::circle, TR("Cancel")},
                                      {ui::Button::triangle, TR("Actions")},
                                      {ui::Button::options, TR("Menu")}};
            draw_column_hints(ctx, list, hints, 5);
        }
    }

    // ---- state -------------------------------------------------------------

    bool online_ = false;
    std::unique_ptr<GameLink> link_;
    std::uint64_t link_version_ = 0;
    chess::Game game_;
    board::BoardView view_;
    board::BoardInput input_;

    PlayerPanel top_;
    PlayerPanel bottom_;
    MoveTable moves_;
    ui::ButtonGroup actions_;
    ui::HoldButton resign_;
    ui::PauseMenu pause_;
    ui::Dialog result_;
    ui::Dialog offer_dialog_;
    ui::Confetti confetti_;

    bool auto_flip_ = true;
    bool announce_start_ = false;
    bool over_ = false;
    bool result_shown_ = false;
    float result_delay_ = 0.0f;
    std::string end_reason_; // local games
    int local_winner_ = -1;
    int history_ = -1; // ply being reviewed, -1 = live
    float flip_delay_ = -1.0f;
    Focus focus_ = Focus::board;
    Row row_ = Row::none;
    Offer offer_ = Offer::none; // what the opponent is asking for
    std::vector<int> result_commands_;
    std::vector<std::string> sans_;
    std::string review_text_;
    std::string review_count_;
    tween::Spring review_amount_;
    tween::Spring review_share_;             // how far through the game the shown position is
    tween::Spring arrive_{1.0f, 0.0f, 1.0f}; // 0 -> 1 as a loaded game replaces the waiting board
    bool failure_shown_ = false;
    bool last_low_ = false;
    long long last_tick_second_ = -1;
    bool gone_told_ = false;

    // ---- the look ----
    bool calm_ = false;                                // reduced motion
    Color accent_ = look::accent(look::Section::game); // the game's colour: its speed's
    bool has_speed_ = false;
    look::Speed speed_ = look::Speed::rapid;
    float since_ = 0.0f;       // seconds since the game appeared: its parts arrive by it
    float loading_age_ = 0.0f; // seconds spent waiting for the game
    float failed_age_ = 0.0f;  // seconds since the failure was shown
    std::string failed_body_;
    tween::Spring glow_;       // the light under the board
    ui::SpringColor glow_ink_; // ... and its colour (the game's; a warning in check)
    // The band.
    std::string band_label_; // "Rapid 10+5 · Rated"
    std::string band_state_; // "Your move"
    std::string band_old_;   // ... and what it said before, while that leaves
    Color band_tone_ = look::kInk;
    Color band_old_tone_ = look::kInk;
    bool band_live_ = false;
    tween::Spring band_swap_{1.0f, 0.0f, 1.0f};
    // The row.
    ui::SpringRect row_ring_;
    tween::Spring row_ring_alpha_;
    std::array<tween::Spring, 2> chip_lit_;
    std::array<bool, 2> waiting_{}; // an offer of ours waits for its answer
    ui::Pulse press_;               // a plate was pressed
    ui::Pulse refusal_;             // an edge, or a plate that cannot be used
    ui::Pulse resign_pulse_;        // Square was let go
    bool held_ = false;
    // What floats.
    tween::Spring promotion_in_;
    tween::Spring offer_in_;
    Offer offer_shown_ = Offer::none; // what the card says, kept while it leaves
    float offer_age_ = 0.0f;
    tween::Spring result_in_;
    float result_age_ = 0.0f; // seconds since the result opened: its ceremony runs by it
    Verdict verdict_ = Verdict::draw;
    std::string result_word_;
    std::string result_reason_;
    std::string result_speed_;
    ui::SpringRect answer_ring_;
    tween::Spring answer_alpha_;
    ui::Pulse answer_press_;
};

} // namespace

std::unique_ptr<app::Scene> make_local_game(app::Context &ctx)
{
    return std::make_unique<GameScene>(ctx);
}

bool saved_local_game(const app::Context &ctx, std::string *moves)
{
    std::string data;
    if (!save::read_file(ctx.data_root + kSaveName, &data))
        return false;
    save::Decoded decoded = save::decode(save::Kind::game, data);
    if (!decoded.ok || decoded.payload.empty())
        return false;
    *moves = std::move(decoded.payload);
    return true;
}

bool has_saved_local_game(const app::Context &ctx)
{
    std::string moves;
    return saved_local_game(ctx, &moves);
}

std::unique_ptr<app::Scene> make_online_game(app::Context &ctx, const std::string &game_id)
{
    return make_linked_game(lichess::make_board_link(*ctx.lichess, game_id));
}

std::unique_ptr<app::Scene> make_linked_game(std::unique_ptr<GameLink> link)
{
    return std::make_unique<GameScene>(std::move(link));
}

} // namespace pch::modes
