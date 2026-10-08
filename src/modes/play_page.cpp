// ProsperoLichess - Play page: online pairing, the computer, a friend.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The page in the house look (docs/LOOK.md):
//
//   - the three ways to play run across the top, where the page's heading
//     used to be: a lit plate glides between Online, Computer and Friend;
//   - the time controls are tiles on a shared grid, each in the colour of its
//     speed, with the speed's sign, the control as a large figure and its
//     minutes on a small dial;
//   - beside them, a ticket for the next game: what was chosen, the rating it
//     counts for with its line over time (against the computer: the level as
//     rising steps and the side as kings), and the call to action, all in the
//     colour of the chosen speed;
//   - Pass & Play and the states that cannot start a game are one wide panel
//     with a board or a sign, a title and one bright button;
//   - one ring glides between everything that can have the controller.
//
// The kit's components still own the input, the focus and the sounds (TabBar,
// GridView, RadioGroup, ChoicePicker, PushButton); the page reads their state
// and draws them itself.

#include "board/mini_board.hpp"
#include "core/strings.hpp"
#include "lichess/session.hpp"
#include "modes/game_scene.hpp"
#include "modes/page.hpp"
#include "modes/play_setup.hpp"
#include "modes/scenes.hpp"
#include "ui/components/button.hpp"
#include "ui/components/choice.hpp"
#include "ui/components/choice_group.hpp"
#include "ui/components/grid.hpp"
#include "ui/components/tabs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <span>
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
using look::Section;

constexpr float kTau = 6.2831853f;

// ---- the page's grid --------------------------------------------------------

constexpr float kLeft = app::kContent;
constexpr float kWidth = app::kRight - app::kContent;
// The ways to play, across the top.
constexpr Rect kModes{kLeft, app::kTop, kWidth, 84.0f};
constexpr float kModeInset = 8.0f;
constexpr float kModeGap = 8.0f;
constexpr float kModeRadius = 18.0f;
constexpr float kModeWidth = (kWidth - 2.0f * kModeInset - 2.0f * kModeGap) / 3.0f;
constexpr float kBodyTop = kModes.y + kModes.h + app::kGap;
constexpr float kBodyHeight = app::kBottom - kBodyTop;
// The ticket for the next game, on the right ...
constexpr float kTicketWidth = 456.0f;
constexpr Rect kTicket{app::kRight - kTicketWidth, kBodyTop, kTicketWidth, kBodyHeight};
constexpr float kTicketPad = 28.0f;
constexpr float kTicketX = kTicket.x + kTicketPad;
constexpr float kTicketRoom = kTicket.w - 2.0f * kTicketPad;
constexpr float kNotch = 184.0f; // from the ticket's top to its perforation
constexpr float kBite = 13.0f;   // the radius of the bite out of each side
constexpr Rect kAction{kTicketX, kTicket.y + kTicket.h - kTicketPad - 76.0f, kTicketRoom, 76.0f};
constexpr float kActionRadius = 20.0f;
constexpr float kRowRadius = 16.0f;
// ... and the time controls in the rest: three columns, two rows.
constexpr int kColumns = 3;
constexpr int kTiles = 6;
constexpr float kTilesWidth = kWidth - kTicketWidth - app::kGap;
constexpr float kTileWidth =
    (kTilesWidth - static_cast<float>(kColumns - 1) * app::kGap) / static_cast<float>(kColumns);
constexpr float kTileHeight = (kBodyHeight - app::kGap) / 2.0f;
constexpr float kTilePad = 24.0f;
constexpr float kGridRing = 16.0f; // what the kit's grid keeps around its cells
// Online: rated or casual, two rows above the button.
constexpr float kRatedTop = 412.0f;
constexpr float kRatedHeight = 68.0f;
constexpr float kRatedPitch = 76.0f;
// Computer: the level, then the side to play.
constexpr Rect kLevel{kTicket.x + 14.0f, kTicket.y + 204.0f, kTicket.w - 28.0f, 156.0f};
constexpr float kSideTop = 416.0f;
constexpr float kSideHeight = 50.0f;
constexpr float kSidePitch = 56.0f;
// Pass & Play, and the tabs that cannot be used right now: one wide panel.
constexpr Rect kHero{kLeft, kBodyTop, kWidth, kBodyHeight};
constexpr float kHeroPad = 36.0f;
constexpr float kHeroArt = kBodyHeight - 2.0f * kHeroPad;
constexpr Rect kHeroSquares{kHero.x + kHeroPad, kHero.y + kHeroPad, kHeroArt, kHeroArt};
constexpr float kHeroText = kHeroSquares.x + kHeroArt + 56.0f;
constexpr float kHeroRoom = kHero.x + kHero.w - 48.0f - kHeroText;
constexpr Rect kHeroAction{kHeroText, kHero.y + kHero.h - kHeroPad - 76.0f, 336.0f, 76.0f};
constexpr float kHeroTitle = 76.0f;     // from the kicker's baseline to the title's
constexpr float kHeroTitleLine = 70.0f; // between the title's lines
constexpr float kHeroBody = 58.0f;      // from the title's last baseline to the body's first
constexpr float kHeroBodyLine = 34.0f;
constexpr float kHeroGap = 44.0f; // from the body's last baseline to the button
// The lines a closed tab's title and its body keep to.
constexpr int kClosedTitleLines = 2;
constexpr int kClosedBodyLines = 4;
constexpr float kSeatWidth = (kHeroRoom - 16.0f) / 2.0f;
constexpr float kSlide = 36.0f;    // how far a tab's content travels when the tab changes
constexpr float kHandover = 0.07f; // seconds the leaving tab has to itself

enum Tab : int
{
    kOnline,
    kComputer,
    kFriend,
    kTabCount,
};

// Why the Online and Computer tabs cannot be used, when they cannot.
enum class Gate
{
    open,
    offline,
    signed_out,
    signing_in,
};

constexpr Rect mode_rect(int index)
{
    return {kModes.x + kModeInset + static_cast<float>(index) * (kModeWidth + kModeGap),
            kModes.y + kModeInset, kModeWidth, kModes.h - 2.0f * kModeInset};
}

constexpr Rect tile_rect(int index)
{
    return {kLeft + static_cast<float>(index % kColumns) * (kTileWidth + app::kGap),
            kBodyTop + static_cast<float>(index / kColumns) * (kTileHeight + app::kGap), kTileWidth,
            kTileHeight};
}

constexpr Rect rated_rect(int index)
{
    return {kTicketX, kTicket.y + kRatedTop + static_cast<float>(index) * kRatedPitch, kTicketRoom,
            kRatedHeight};
}

constexpr Rect side_rect(int index)
{
    return {kTicketX, kTicket.y + kSideTop + static_cast<float>(index) * kSidePitch, kTicketRoom,
            kSideHeight};
}

std::vector<ui::CardItem> time_cells(std::span<const TimeControl> controls)
{
    std::vector<ui::CardItem> items;
    for (const TimeControl &tc : controls)
    {
        ui::CardItem item;
        item.title = control_text(tc);
        item.subtitle = increment_text(tc);
        item.badge = speed_text(tc);
        items.push_back(std::move(item));
    }
    return items;
}

look::Speed speed_of(const TimeControl &tc)
{
    return look::speed_from(perf_key(tc));
}

// The size at which text is no wider than room, never above size.
float fitted(const ui::FontRef &font, std::string_view text, float size, float room)
{
    const float wide = font.measure(text, size);
    return wide > room ? size * room / wide : size;
}

// The width ui::text_fit draws a line at: it shrinks first, then it is cut.
float fit_width(const ui::FontRef &font, std::string_view text, float size, float room)
{
    const float small = size * ui::fit_scale(font, text, size, room);
    const float wide = font.measure(text, small);
    return wide <= room + 0.5f ? wide : font.measure(font.font->fit(text, small, room), small);
}

// look::kicker for a text that has `room` and no more: a translation is
// often longer than the English words.
float kicker_fit(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
                 float baseline, float room, Color color, Align align = Align::left,
                 float size = 16.0f)
{
    return ui::text_fit(list, fonts.semibold, ui::upper(text), x, baseline, size, room, color,
                        align, 3.0f);
}

// The width look::tag draws a word at.
float tag_width(const ui::Fonts &fonts, std::string_view text, float size)
{
    return fonts.semibold.measure(ui::upper(text), size, 1.5f) + 2.0f * size * 0.62f;
}

// "Your rapid rating", by the rating's key. One text per speed: inside a
// sentence the speed's word changes form in many languages.
const char *rating_title(std::string_view perf)
{
    if (perf == "bullet")
        return tr("Your bullet rating");
    if (perf == "blitz")
        return tr("Your blitz rating");
    if (perf == "classical")
        return tr("Your classical rating");
    if (perf == "correspondence")
        return tr("Your correspondence rating");
    return tr("Your rapid rating");
}

// One part of the page inside its arrival: it fades in and settles upward.
template <typename Draw>
void arrive(gfx::DrawList &list, float in, float distance, const Draw &draw)
{
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

// ---- signs ------------------------------------------------------------------

// The three ways to play: a globe, a chip, two pawns.
void draw_mode_sign(gfx::DrawList &list, int mode, const Rect &box, Color ink)
{
    const float s = std::min(box.w, box.h);
    const float cx = box.cx();
    const float cy = box.cy();
    const float pen = std::max(s * 0.1f, 2.2f);
    switch (mode)
    {
    case kOnline:
        list.ring(cx, cy, s * 0.48f, pen, ink);
        list.bordered_rect({cx - s * 0.21f, cy - s * 0.48f, s * 0.42f, s * 0.96f}, s * 0.21f,
                           look::kClear, pen, ink);
        list.rounded_rect({cx - s * 0.46f, cy - pen * 0.5f, s * 0.92f, pen}, 0.0f, ink);
        break;
    case kComputer:
    {
        const float half = s * 0.3f;
        list.bordered_rect({cx - half, cy - half, 2.0f * half, 2.0f * half}, s * 0.1f, look::kClear,
                           pen, ink);
        list.rounded_rect({cx - s * 0.11f, cy - s * 0.11f, s * 0.22f, s * 0.22f}, s * 0.04f, ink);
        for (const float offset : {-s * 0.14f, s * 0.14f})
        {
            list.line(cx + offset, cy - half, cx + offset, cy - s * 0.48f, pen, ink);
            list.line(cx + offset, cy + half, cx + offset, cy + s * 0.48f, pen, ink);
            list.line(cx - half, cy + offset, cx - s * 0.48f, cy + offset, pen, ink);
            list.line(cx + half, cy + offset, cx + s * 0.48f, cy + offset, pen, ink);
        }
        break;
    }
    default:
        for (const float side : {-1.0f, 1.0f})
        {
            const float px = cx + side * s * 0.24f;
            list.circle(px, cy - s * 0.24f, s * 0.15f, ink);
            list.triangle({px - s * 0.17f, cy - s * 0.2f, s * 0.34f, s * 0.46f}, ink);
            list.rounded_rect({px - s * 0.21f, cy + s * 0.26f, s * 0.42f, s * 0.14f}, s * 0.05f,
                              ink);
        }
        break;
    }
}

// "<" (direction -1) or ">" (direction 1), centred on (cx, cy).
void draw_chevron(gfx::DrawList &list, float cx, float cy, float reach, int direction, Color ink)
{
    const float d = static_cast<float>(direction) * reach * 0.5f;
    list.line(cx - d, cy - reach, cx + d, cy, 3.0f, ink);
    list.line(cx + d, cy, cx - d, cy + reach, 3.0f, ink);
}

// A time control drawn, at the right end of a tile's lower row: its minutes
// on a dial, its days as pips, no clock as a loop without an end.
void draw_time_sign(gfx::DrawList &list, const TimeControl &tc, float right, float cy, Color c,
                    float grow)
{
    if (tc.days > 0)
    {
        const int days = std::min(tc.days, 7);
        for (int i = 0; i < days; ++i)
        {
            const float shown =
                tween::clamp01(grow * static_cast<float>(days) - static_cast<float>(days - 1 - i));
            list.rounded_rect(
                {right - 14.0f - static_cast<float>(i) * 20.0f, cy - 7.0f, 14.0f, 14.0f}, 4.0f,
                gfx::mix(look::kInk.with_alpha(0.12f), c, shown));
        }
        return;
    }
    if (tc.minutes == 0)
    {
        list.ring(right - 31.0f, cy, 12.0f, 4.0f, c.with_alpha(grow));
        list.ring(right - 12.0f, cy, 12.0f, 4.0f, c.with_alpha(grow));
        return;
    }
    const float radius = 22.0f;
    const float cx = right - radius;
    const float share = std::min(static_cast<float>(tc.minutes) / 60.0f, 1.0f);
    look::gauge(list, cx, cy, radius, 5.0f, share, c, grow);
    const float angle = kTau * share * tween::clamp01(grow);
    list.line(cx, cy, cx + std::sin(angle) * 11.0f, cy - std::cos(angle) * 11.0f, 3.0f,
              look::kInk.with_alpha(0.75f));
    list.circle(cx, cy, 3.0f, look::kInk);
}

// One row of a choice inside the ticket: lit in the colour while it is the
// chosen one.
void draw_choice_plate(gfx::DrawList &list, const Rect &r, float on, Color c)
{
    list.rounded_rect(r, kRowRadius, look::kInk.with_alpha(0.035f));
    if (on > 0.01f)
        list.gradient_rect_h(r, kRowRadius, c.with_alpha(0.24f * on), c.with_alpha(0.05f * on));
    list.bordered_rect(r, kRowRadius, look::kClear, 1.5f,
                       gfx::mix(look::kInk.with_alpha(0.09f), c.with_alpha(0.6f), on));
}

// The button that starts a game: the brightest thing on the page.
void draw_action(gfx::DrawList &list, const ui::Fonts &fonts, const Rect &r, std::string_view label,
                 ui::Button glyph, Color c, float focus, float press, float time, bool calm)
{
    const float breath = calm ? 0.5f : ui::breathe(time, 2.8f);
    list.push_transform(1.0f - 0.035f * press, r.cx(), r.cy(), 0.0f, 0.0f);
    list.shadow({r.x, r.y + 10.0f, r.w, r.h}, kActionRadius, 26.0f,
                look::kNight.with_alpha(0.4f + 0.2f * focus));
    list.glow(r, kActionRadius, 20.0f + 10.0f * focus,
              c.with_alpha(0.14f + 0.1f * breath + 0.14f * focus));
    list.gradient_rect(r, kActionRadius, gfx::mix(c, look::kInk, 0.3f),
                       gfx::mix(c, look::kNight, 0.12f));
    list.gradient_rect({r.x + 2.0f, r.y + 2.0f, r.w - 4.0f, r.h * 0.5f}, kActionRadius - 2.0f,
                       look::kInk.with_alpha(0.22f), look::kInk.with_alpha(0.0f));
    constexpr float kSize = 28.0f;
    constexpr float kGlyph = 38.0f;
    const float gap = glyph == ui::Button::none ? 0.0f : 14.0f;
    const float glyph_width = glyph == ui::Button::none ? 0.0f : ui::button_width(glyph, kGlyph);
    const float room = r.w - 48.0f - gap - glyph_width;
    const float width = fit_width(fonts.semibold, label, kSize, room) + gap + glyph_width;
    const float x = r.cx() - width * 0.5f;
    ui::text_fit(list, fonts.semibold, label, x, r.cy() + kSize * 0.35f, kSize, room, look::kNight);
    if (glyph != ui::Button::none)
        ui::draw_button(list, fonts, ui::GlyphStyle::dark(), glyph, x + width - glyph_width, r.cy(),
                        kGlyph);
    list.pop_transform();
}

// What the ticket's stub says is about to be played.
struct Stub
{
    std::string kicker; // "Rapid", "Stockfish level 3"
    std::string title;  // "10+5"
    std::string body;   // "10 minutes each, +5 s per move"
    look::Speed speed = look::Speed::rapid;

    bool operator==(const Stub &) const = default;
};

// A fact of the wide panel: a small label over a figure.
struct Fact
{
    const char *label = "";
    std::string value;
};

// The room a fact takes in its row: the wider of its two lines.
float fact_width(const ui::Fonts &fonts, const Fact &fact)
{
    return std::max({fonts.semibold.measure(fact.value, 30.0f),
                     fonts.semibold.measure(ui::upper(fact.label), 14.0f, 3.0f), 96.0f});
}

// The size of the wide panel's title: the display size, or the largest under
// it at which a longer title (a translation) keeps to `lines` lines.
float hero_title_size(const ui::Fonts &fonts, std::string_view title, int lines)
{
    float size = 60.0f;
    while (size > 40.0f &&
           static_cast<int>(fonts.display.font->wrap(title, size, kHeroRoom).size()) > lines)
        size -= 4.0f;
    return size;
}

// How the wide panel's running text is set: at its size or, when a longer
// text (a translation) needs more than `lines` lines, smaller and with one
// line more in about the same height.
struct Running
{
    float size;
    float line; // from one baseline to the next
    int lines;
};
Running hero_body(const ui::Fonts &fonts, std::string_view body, int lines)
{
    if (static_cast<int>(fonts.regular.font->wrap(body, 24.0f, kHeroRoom).size()) <= lines)
        return {24.0f, kHeroBodyLine, lines};
    return {21.0f, 29.0f, lines + 1};
}

class PlayPage final : public Page
{
  public:
    explicit PlayPage(app::Context &ctx)
    {
        tabs_.set_tabs({{trc("way to play", "Online")}, {tr("Computer")}, {tr("Friend")}});
        tabs_.set_bounds(kModes);
        tabs_.set_active(kOnline, true);

        for (ui::GridView *grid : {&pools_, &clocks_})
        {
            // The grid's cells are the page's tiles (tile_rect).
            grid->style.columns = kColumns;
            grid->style.gap_x = app::kGap;
            grid->style.gap_y = app::kGap;
            grid->style.cell_height = kTileHeight;
            grid->style.padding = kGridRing;
            grid->style.entrance_step = 0.0f;
            grid->style.exits.up = true;    // to the modes
            grid->style.exits.left = true;  // to the rail
            grid->style.exits.right = true; // to the ticket
            grid->set_bounds({kLeft - kGridRing, kBodyTop - kGridRing,
                              kTilesWidth + 2.0f * kGridRing, kBodyHeight + 2.0f * kGridRing});
        }
        pools_.set_items(time_cells(pairing_pools()));
        pools_.set_focus(1); // 10+5, as the old screen opened
        clocks_.set_items(time_cells(ai_clocks()));
        clocks_.set_focus(2); // 10+5 here too

        rated_.set_items({{tr("Rated"), tr("The result changes your rating")},
                          {tr("Casual"), tr("Play without rating points")}});
        rated_.set_selected(0);
        rated_.style.exits.up = true;
        rated_.style.exits.down = true;
        rated_.style.exits.left = true;
        rated_.set_bounds({kTicketX, rated_rect(0).y, kTicketRoom,
                           rated_rect(1).y + kRatedHeight - rated_rect(0).y});

        std::vector<std::string> levels;
        for (int level = 1; level <= kAiLevels; ++level)
            levels.push_back(fill(tr("Level {0}"), {std::to_string(level)}));
        level_.set_options(std::move(levels));
        level_.set_index(2); // level 3, as the old screen opened
        level_.style.wrap = false;
        level_.style.confirm_cycles = false;
        level_.set_bounds(kLevel);

        color_.set_items({{tr("Random"), tr("Lichess picks your side")},
                          {tr("White"), tr("You move first")},
                          {tr("Black"), tr("Stockfish moves first")}});
        color_.set_selected(0);
        color_.style.exits.up = true;
        color_.style.exits.down = true;
        color_.style.exits.left = true;
        color_.set_bounds(
            {kTicketX, side_rect(0).y, kTicketRoom, side_rect(2).y + kSideHeight - side_rect(0).y});

        for (ui::PushButton *button : {&find_, &play_, &friend_})
        {
            // Starting a game is the big moment of this page.
            button->style.sounds.activate = audio::Cue::launch;
            button->set_bounds(kAction);
        }
        find_.label = tr("Find opponent");
        play_.label = tr("Play");
        friend_.set_bounds(kHeroAction);
        way_out_.set_bounds(kHeroAction);

        refresh(ctx, true);
    }

    void enter(app::Context &ctx) override
    {
        refresh(ctx, false);
        // The page assembles again, and its figures count again.
        since_ = 0.0f;
        tab_since_ = 0.0f;
        chart_since_ = 0.0f;
        rating_shown_.snap(0.0f);
        tab_swap_.snap(1.0f);
    }

    PageResult update(app::Context &ctx, const InputFrame &input, float dt, bool focused) override
    {
        ctx.calm(tabs_, pools_, clocks_, rated_, level_, color_, find_, play_, friend_, way_out_);
        calm_ = ctx.reduced_motion();
        since_ += dt;
        tab_since_ += dt;
        chart_since_ += dt;
        // The account, the connection and the saved game are looked at twice
        // a second; what follows from the player's own choices, every frame.
        refresh_in_ -= dt;
        if (refresh_in_ <= 0.0f)
            refresh(ctx, false);

        PageResult result;
        if (focused)
            handle(ctx, input, &result);
        describe(ctx, false);

        const bool setup = tab() != kFriend && gate_ == Gate::open;
        tabs_.set_focused(focused && zone_ == Zone::tabs);
        pools_.set_active(focused && zone_ == Zone::grid);
        clocks_.set_active(focused && zone_ == Zone::grid);
        rated_.set_active(focused && setup && zone_ == Zone::options);
        level_.set_active(focused && setup && zone_ == Zone::options);
        color_.set_active(focused && setup && zone_ == Zone::color);
        find_.set_active(focused && setup && zone_ == Zone::action);
        play_.set_active(focused && setup && zone_ == Zone::action);
        friend_.set_active(focused && zone_ == Zone::action);
        way_out_.set_active(focused && zone_ == Zone::action);

        tabs_.update(dt);
        pools_.update(dt);
        clocks_.update(dt);
        rated_.update(dt);
        level_.update(dt);
        color_.update(dt);
        find_.update(dt);
        play_.update(dt);
        friend_.update(dt);
        way_out_.update(dt);
        animate(dt, focused);
        return result;
    }

    void draw(app::Context &ctx, app::Frame &frame, bool) const override
    {
        gfx::DrawList &list = frame.scene;
        draw_modes(ctx, list);

        // The tab that was showing leaves the way the new one came.
        const float swap = tween::clamp01(tab_swap_.value);
        const float way = tab() > from_tab_ ? 1.0f : -1.0f;
        if (swap < 0.995f && from_tab_ != tab())
        {
            list.push_opacity((1.0f - swap) * (1.0f - swap));
            list.push_transform(1.0f, 0.0f, 0.0f, -way * kSlide * swap, 0.0f);
            draw_tab(ctx, list, from_tab_, true);
            list.pop_transform();
            list.pop_opacity();
        }
        list.push_transform(1.0f, 0.0f, 0.0f, way * kSlide * (1.0f - swap), 0.0f);
        draw_tab(ctx, list, tab(), false);
        list.pop_transform();

        // One ring for the whole page, on whatever has the controller.
        if (ring_alpha_.value > 0.01f)
        {
            Rect ring = ring_.value();
            const float nudge = ui::shake(refusal_.value, ctx.time, 8.0f);
            (refused_vertical_ ? ring.y : ring.x) += nudge;
            look::ring(list, ring.inset(3.0f * press_.value), ring_ink_.value(), ring_alpha_.value,
                       ring_radius_.value);
        }
    }

    std::span<const ui::Hint> hints() const override
    {
        static constexpr ui::Hint kOnlineHints[] = {{ui::Button::cross, TR("Choose")},
                                                    {ui::Button::triangle, TR("Find opponent")},
                                                    {ui::Button::l1, TR("Mode"), ui::Button::r1},
                                                    {ui::Button::circle, TR("Back")}};
        static constexpr ui::Hint kComputerHints[] = {{ui::Button::cross, TR("Choose")},
                                                      {ui::Button::triangle, TR("Play")},
                                                      {ui::Button::l1, TR("Mode"), ui::Button::r1},
                                                      {ui::Button::circle, TR("Back")}};
        static constexpr ui::Hint kOtherHints[] = {{ui::Button::cross, TR("Select")},
                                                   {ui::Button::l1, TR("Mode"), ui::Button::r1},
                                                   {ui::Button::circle, TR("Back")}};
        if (tab() == kFriend || gate_ != Gate::open)
            return kOtherHints;
        if (tab() == kOnline)
            return kOnlineHints;
        return kComputerHints;
    }

    Section section() const override
    {
        return Section::play;
    }

    // The sky leans toward the colour of what is about to be played.
    look::Mood mood() const override
    {
        look::Mood mood = look::mood(Section::play);
        mood.sky[2] =
            gfx::mix(mood.sky[2], gfx::mix(look::kNight, tint(tab(), gate_), 0.5f), 0.45f);
        return mood;
    }

    const char *title() const override
    {
        return tr("Play");
    }

    bool tabbed() const override
    {
        return true;
    }

    const char *name() const override
    {
        return "play";
    }

  private:
    // What has the controller inside the page.
    enum class Zone
    {
        tabs,
        grid,    // the time controls
        options, // Online: rated or casual; Computer: the level
        color,   // Computer: the side to play
        action,  // the button that starts the game (or leads out of a closed tab)
    };

    int tab() const
    {
        return tabs_.active();
    }

    bool allows(Zone zone) const
    {
        if (zone == Zone::tabs)
            return true;
        if (tab() == kFriend)
            return zone == Zone::action;
        if (gate_ != Gate::open)
            return zone == Zone::action && gate_ != Gate::signing_in;
        return zone != Zone::color || tab() == kComputer;
    }

    // Where the controller lands when it comes down from the tabs.
    Zone first_content() const
    {
        if (tab() != kFriend && gate_ == Gate::open)
            return Zone::grid;
        return allows(Zone::action) ? Zone::action : Zone::tabs;
    }

    static Gate gate_for(const app::Context &ctx, int tab)
    {
        if (tab == kFriend)
            return Gate::open;
        const lichess::Session *session = ctx.lichess;
        if (session == nullptr || !session->online())
            return Gate::offline;
        if (session->signing_in())
            return Gate::signing_in;
        return session->signed_in() ? Gate::open : Gate::signed_out;
    }

    ui::GridView &grid()
    {
        return tab() == kComputer ? clocks_ : pools_;
    }
    // The time control chosen on a tab (Friend has none: Online's).
    const TimeControl &chosen(int which) const
    {
        const bool computer = which == kComputer;
        const std::span<const TimeControl> controls = computer ? ai_clocks() : pairing_pools();
        const int index = (computer ? clocks_ : pools_).focus();
        return controls[static_cast<std::size_t>(
            std::clamp(index, 0, static_cast<int>(controls.size()) - 1))];
    }
    const TimeControl &chosen() const
    {
        return chosen(tab());
    }
    bool rated() const
    {
        return rated_.selected() != 1;
    }
    int level() const
    {
        return std::clamp(level_.index() + 1, 1, kAiLevels);
    }
    AiColor color() const
    {
        return color_.selected() == 1   ? AiColor::white
               : color_.selected() == 2 ? AiColor::black
                                        : AiColor::random;
    }

    // The colour a tab is in: the chosen speed's, the page's for Pass & Play,
    // a warning's while there is no connection.
    Color tint(int which, Gate gate) const
    {
        if (which == kFriend || gate == Gate::signed_out || gate == Gate::signing_in)
            return look::accent(Section::play);
        if (gate == Gate::offline)
            return look::kGold;
        return look::speed_color(speed_of(chosen(which)));
    }

    void focus_to(app::Context &ctx, Zone zone)
    {
        if (!allows(zone) || zone == zone_)
            return;
        zone_ = zone;
        ctx.cue(audio::Cue::focus);
    }

    // The tab changed: its content arrives, and the controller stays where it
    // was (on the tab row, or in the content).
    void tab_changed(app::Context &ctx, int from, bool into_content)
    {
        refresh(ctx, false);
        from_tab_ = from;
        tab_swap_.snap(ctx.reduced_motion() ? 1.0f : 0.0f);
        // The old content leaves first; the new one's parts start a moment later.
        tab_since_ = -kHandover;
        chart_since_ = -kHandover;
        zone_ = into_content ? first_content() : Zone::tabs;
    }

    // An edge, or something that cannot be done: the ring answers with a nudge.
    void nudge(const InputFrame &input)
    {
        if (calm_ || input.nav_repeat)
            return;
        refusal_.trigger();
        refused_vertical_ = input.nav == Direction::up || input.nav == Direction::down;
    }

    void handle(app::Context &ctx, const InputFrame &input, PageResult *result)
    {
        ui::Feedback &feedback = *ctx.feedback;
        if (input.is_pressed(Action::back))
        {
            ctx.cue(audio::Cue::back);
            result->to_rail = true;
            return;
        }
        const int turn = input.is_pressed(Action::page_next)   ? 1
                         : input.is_pressed(Action::page_prev) ? -1
                                                               : 0;
        if (turn != 0)
        {
            const int from = tab();
            if (tabs_.step(turn, input, feedback) == ui::Event::changed)
                tab_changed(ctx, from, zone_ != Zone::tabs);
            else if (!calm_)
                mode_refusal_.trigger();
            return;
        }
        const bool setup = tab() != kFriend && gate_ == Gate::open;
        if (setup && input.is_pressed(Action::north))
        {
            // Triangle starts the game from anywhere in the tab.
            (tab() == kOnline ? find_ : play_).press();
            action_press_.trigger();
            ctx.cue(audio::Cue::launch);
            start(result);
            return;
        }

        switch (zone_)
        {
        case Zone::tabs:
            if (input.nav == Direction::left && tab() == 0)
            {
                result->to_rail = true;
            }
            else if (input.nav == Direction::down || input.is_pressed(Action::confirm))
            {
                focus_to(ctx, first_content());
            }
            else
            {
                const int from = tab();
                const ui::Event event = tabs_.handle(input, feedback);
                if (event == ui::Event::changed)
                    tab_changed(ctx, from, false);
                else if (event == ui::Event::refused)
                    nudge(input);
            }
            break;
        case Zone::grid:
        {
            const ui::Event event = grid().handle(input, feedback);
            if (event == ui::Event::activated)
            {
                zone_ = Zone::action; // the choice is made: next, start
                if (!calm_)
                    press_.trigger();
            }
            else if (event == ui::Event::refused)
                nudge(input);
            else if (grid().exit() == Direction::up)
                focus_to(ctx, Zone::tabs);
            else if (grid().exit() == Direction::left)
                result->to_rail = true;
            else if (grid().exit() == Direction::right)
                focus_to(ctx, grid().focus() < grid().columns() ? Zone::options : Zone::action);
            break;
        }
        case Zone::options:
            if (tab() == kOnline)
            {
                const ui::Event event = rated_.handle(input, feedback);
                if (event == ui::Event::refused)
                    nudge(input);
                else if (event == ui::Event::changed && !calm_)
                    press_.trigger();
                leave_group(ctx, rated_, Zone::tabs, Zone::action);
            }
            else if (input.nav == Direction::up)
            {
                focus_to(ctx, Zone::tabs);
            }
            else if (input.nav == Direction::down)
            {
                focus_to(ctx, Zone::color);
            }
            else
            {
                const ui::Event event = level_.handle(input, feedback);
                if (event == ui::Event::activated)
                    zone_ = Zone::action;
                else if (event == ui::Event::refused)
                    nudge(input);
                else if (event == ui::Event::changed && !calm_)
                    (input.nav == Direction::left ? step_down_ : step_up_).trigger();
            }
            break;
        case Zone::color:
        {
            const ui::Event event = color_.handle(input, feedback);
            if (event == ui::Event::refused)
                nudge(input);
            else if (event == ui::Event::changed && !calm_)
                press_.trigger();
            leave_group(ctx, color_, Zone::options, Zone::action);
            break;
        }
        case Zone::action:
            handle_action(ctx, input, result);
            break;
        }
    }

    // A group of choices hands the controller on at its edges.
    void leave_group(app::Context &ctx, const ui::RadioGroup &group, Zone above, Zone below)
    {
        if (group.exit() == Direction::up)
            focus_to(ctx, above);
        else if (group.exit() == Direction::down)
            focus_to(ctx, below);
        else if (group.exit() == Direction::left)
            focus_to(ctx, Zone::grid);
    }

    void handle_action(app::Context &ctx, const InputFrame &input, PageResult *result)
    {
        ui::Feedback &feedback = *ctx.feedback;
        if (tab() == kFriend || gate_ != Gate::open)
        {
            if (input.nav == Direction::up)
                focus_to(ctx, Zone::tabs);
            else if (input.nav == Direction::left)
                result->to_rail = true;
            else if (tab() == kFriend)
            {
                if (friend_.handle(input, feedback) == ui::Event::activated)
                {
                    action_press_.trigger();
                    result->transition = app::Transition::push(make_local_game(ctx));
                }
            }
            else if (way_out_.handle(input, feedback) == ui::Event::activated)
            {
                action_press_.trigger();
                if (gate_ == Gate::signed_out)
                {
                    result->transition = app::Transition::push(make_account());
                }
                else
                {
                    // Offline: the one way to play is the Friend tab.
                    const int from = tab();
                    tabs_.set_active(kFriend);
                    tab_changed(ctx, from, true);
                }
            }
            return;
        }
        if (input.nav == Direction::up)
        {
            ui::RadioGroup &above = tab() == kOnline ? rated_ : color_;
            above.set_focus(static_cast<int>(above.items().size()) - 1, false);
            focus_to(ctx, tab() == kOnline ? Zone::options : Zone::color);
        }
        else if (input.nav == Direction::left)
            focus_to(ctx, Zone::grid);
        else if ((tab() == kOnline ? find_ : play_).handle(input, feedback) == ui::Event::activated)
        {
            action_press_.trigger();
            start(result);
        }
    }

    // Opens the waiting screen for what the tab is set to.
    void start(PageResult *result) const
    {
        if (tab() == kOnline)
        {
            GameRequest request = seek_request(chosen(), rated());
            result->transition =
                app::Transition::push(make_seek(std::move(request.form), std::move(request.label)));
        }
        else
        {
            GameRequest request = ai_request(chosen(), level(), color());
            result->transition = app::Transition::push(
                make_ai_game(std::move(request.form), std::move(request.label)));
        }
    }

    // Reads the account, the connection and the saved game. first: settle
    // without animation.
    void refresh(app::Context &ctx, bool first)
    {
        refresh_in_ = 0.5f;
        const Gate gate = gate_for(ctx, tab());
        if (gate != gate_ || first)
        {
            gate_ = gate;
            tab_since_ = 0.0f;
            // What had the controller may be gone: it goes to what replaced it.
            if (zone_ != Zone::tabs)
                zone_ = first_content();
        }
        way_out_.label = gate_action(gate_);
        if (gate_ != Gate::open)
        {
            closed_ = closed_layout(*ctx.fonts, tab(), gate_);
            way_out_.set_bounds(closed_.action);
        }

        std::string moves;
        saved_game_ = saved_local_game(ctx, &moves);
        friend_.label = saved_game_ ? tr("Resume game") : tr("New game");
        if (moves != saved_moves_ || first)
        {
            // The wide panel shows the game as it was left.
            saved_moves_ = moves;
            chess::Game game;
            if (!saved_game_ || !game.apply_uci_moves(moves))
                game.reset();
            const std::size_t plies = game.ply_count();
            saved_position_ = game.position();
            saved_last_ = plies > 0 ? game.move_at(plies - 1) : chess::Move{};
            if (saved_game_)
                facts_ = {Fact{trc("move number", "Move"),
                               std::to_string(saved_position_.fullmove_number())},
                          Fact{tr("Players"), "2"}, Fact{tr("Internet"), tr("Not needed")}};
            else
                facts_ = {Fact{tr("Players"), "2"}, Fact{tr("Controllers"), "1"},
                          Fact{tr("Internet"), tr("Not needed")}};
        }
        describe(ctx, first);
    }

    // The words and figures that follow from what is chosen.
    void describe(app::Context &ctx, bool snap)
    {
        for (const int which : {kOnline, kComputer})
        {
            const TimeControl &tc = chosen(which);
            Stub next;
            next.speed = speed_of(tc);
            next.title = control_text(tc);
            if (which == kComputer)
            {
                next.kicker = fill(tr("Stockfish level {0}"), {std::to_string(level())});
                // Two facts with a dot between them: the speed, and the side.
                next.body = std::string(tc.minutes > 0 ? speed_text(tc) : tr("No clock")) +
                            " \xC2\xB7 " +
                            (color() == AiColor::random  ? tr("a random side")
                             : color() == AiColor::white ? tr("you play White")
                                                         : tr("you play Black"));
            }
            else
            {
                next.kicker = speed_text(tc);
                next.body = clock_sentence(tc);
            }
            const std::size_t g = static_cast<std::size_t>(which);
            if (next == stub_[g])
                continue;
            // The words that were there leave as the new ones arrive.
            stub_old_[g] = std::move(stub_[g]);
            stub_[g] = std::move(next);
            stub_swap_[g].snap(snap || calm_ ? 1.0f : 0.0f);
        }

        if (ctx.lichess == nullptr)
            return;
        const std::string key = perf_key(chosen(kOnline));
        const lichess::Perf *perf = ctx.lichess->perf(key);
        const int rating = perf != nullptr ? perf->rating : 0;
        if (key != rating_perf_)
        {
            // Another speed: its line is drawn again from the left.
            rating_perf_ = key;
            rating_label_ = rating_title(key);
            chart_since_ = 0.0f;
        }
        has_rating_ = rating > 0;
        provisional_ = perf != nullptr && perf->provisional;
        rating_change_ = perf != nullptr ? perf->progress : 0;
        const int games = perf != nullptr ? perf->games : 0;
        if (games != rating_games_ || rating_note_.empty())
        {
            rating_games_ = games;
            rating_note_ = games > 0 ? plural(TR("{0} game"), TR("{0} games"), games)
                                     : std::string(tr("No games yet"));
        }
        if (perf != nullptr && perf->history != rating_history_)
            rating_history_ = perf->history;
        else if (perf == nullptr)
            rating_history_.clear();
        rating_shown_.target = static_cast<float>(rating);
        if (snap)
            rating_shown_.snap(rating_shown_.target);
    }

    // ---- what moves by itself ------------------------------------------------

    Rect focus_rect() const
    {
        switch (zone_)
        {
        case Zone::tabs:
            return mode_rect(tab());
        case Zone::grid:
            return tile_rect(
                std::clamp((tab() == kComputer ? clocks_ : pools_).focus(), 0, kTiles - 1));
        case Zone::options:
            return tab() == kOnline ? rated_rect(std::clamp(rated_.focus(), 0, 1)) : kLevel;
        case Zone::color:
            return side_rect(std::clamp(color_.focus(), 0, 2));
        case Zone::action:
            break;
        }
        if (tab() == kFriend)
            return kHeroAction;
        return gate_ != Gate::open ? closed_.action : kAction;
    }

    float focus_radius() const
    {
        switch (zone_)
        {
        case Zone::tabs:
            return kModeRadius;
        case Zone::grid:
            return look::kRadius;
        case Zone::action:
            return kActionRadius;
        default:
            return kRowRadius;
        }
    }

    void animate(float dt, bool focused)
    {
        const float quick = calm_ ? 60.0f : 18.0f;
        const bool setup = tab() != kFriend && gate_ == Gate::open;
        const Color ink = tint(tab(), gate_);

        plate_.target(mode_rect(tab()));
        ring_.target(focus_rect());
        ring_radius_.target = focus_radius();
        ring_ink_.target(zone_ == Zone::tabs ? look::accent(Section::play) : ink);
        ring_alpha_.target = focused ? 1.0f : 0.0f;
        ink_.target(ink);
        // A ring that is not showing appears where the focus is, without a glide.
        if (!placed_ || ring_alpha_.value < 0.02f)
        {
            ring_.snap(focus_rect());
            ring_radius_.snap(ring_radius_.target);
            ring_ink_.snap(zone_ == Zone::tabs ? look::accent(Section::play) : ink);
        }
        if (!placed_)
        {
            placed_ = true;
            plate_.snap(mode_rect(tab()));
            ink_.snap(ink);
        }
        plate_.update(dt, calm_ ? 60.0f : 16.0f);
        ring_.update(dt, quick);
        ring_radius_.update(dt, quick);
        ring_ink_.update(dt, calm_ ? 60.0f : 12.0f);
        ring_alpha_.update(dt, quick);
        ink_.update(dt, calm_ ? 60.0f : 9.0f);

        for (int i = 0; i < kTabCount; ++i)
        {
            tween::Spring &lit = mode_lit_[static_cast<std::size_t>(i)];
            lit.target = i == tab() ? 1.0f : 0.0f;
            lit.update(dt, quick);
        }
        for (std::size_t g = 0; g < 2; ++g)
        {
            const ui::GridView &view = g == 0 ? pools_ : clocks_;
            const bool showing = setup && tab() == static_cast<int>(g);
            for (int i = 0; i < kTiles; ++i)
            {
                const std::size_t at = static_cast<std::size_t>(i);
                chosen_[g][at].target = view.focus() == i ? 1.0f : 0.0f;
                chosen_[g][at].update(dt, quick);
                hover_[g][at].target =
                    focused && showing && zone_ == Zone::grid && view.focus() == i ? 1.0f : 0.0f;
                hover_[g][at].update(dt, quick);
            }
            stub_swap_[g].target = 1.0f;
            stub_swap_[g].update(dt, calm_ ? 60.0f : 13.0f);
        }
        for (int i = 0; i < 2; ++i)
        {
            tween::Spring &on = rated_on_[static_cast<std::size_t>(i)];
            on.target = rated_.selected() == i ? 1.0f : 0.0f;
            on.update(dt, quick);
        }
        for (int i = 0; i < 3; ++i)
        {
            tween::Spring &on = side_on_[static_cast<std::size_t>(i)];
            on.target = color_.selected() == i ? 1.0f : 0.0f;
            on.update(dt, quick);
        }
        for (int i = 0; i < kAiLevels; ++i)
        {
            // The steps fill one after another.
            tween::Spring &step = steps_[static_cast<std::size_t>(i)];
            step.target = i < level() ? 1.0f : 0.0f;
            step.update(dt, calm_ ? 60.0f : 24.0f - static_cast<float>(i));
        }
        level_lit_.target =
            focused && setup && tab() == kComputer && zone_ == Zone::options ? 1.0f : 0.0f;
        level_lit_.update(dt, quick);
        action_lit_.target = focused && zone_ == Zone::action ? 1.0f : 0.0f;
        action_lit_.update(dt, quick);

        // The rating starts counting once its figure has arrived.
        if (calm_)
            rating_shown_.snap(rating_shown_.target);
        else if (tab_since_ > 0.3f)
            rating_shown_.update(dt, 10.0f);
        tab_swap_.target = 1.0f;
        tab_swap_.update(dt, calm_ ? 60.0f : 22.0f);
        if (calm_)
            action_press_.value = 0.0f;
        press_.update(dt, 10.0f);
        action_press_.update(dt, 9.0f);
        refusal_.update(dt, 9.0f);
        mode_refusal_.update(dt, 9.0f);
        step_down_.update(dt, 10.0f);
        step_up_.update(dt, 10.0f);
    }

    // 0..1 for the index-th part of a tab's content since the tab was shown.
    float part(int index, bool outgoing) const
    {
        return calm_ || outgoing ? 1.0f : look::rise(tab_since_, index, 0.05f, 0.45f);
    }

    // ---- drawing ---------------------------------------------------------------

    void draw_modes(const app::Context &ctx, gfx::DrawList &list) const
    {
        static constexpr const char *kLines[] = {TR("Rapid, classical, correspondence"),
                                                 TR("Stockfish plays on lichess.org"),
                                                 TR("Two players, no account needed")};
        const ui::Fonts &fonts = *ctx.fonts;
        const Color c = look::accent(Section::play);
        arrive(list, calm_ ? 1.0f : look::rise(since_, 0, 0.06f, 0.5f), 18.0f,
               [&]()
               {
                   look::panel(list, kModes, 0.0f, c, kModeRadius + kModeInset);
                   // The lit plate under the way to play that is showing.
                   Rect plate = plate_.value();
                   plate.x += ui::shake(mode_refusal_.value, ctx.time, 8.0f);
                   list.glow(plate, kModeRadius, 16.0f, c.with_alpha(0.1f));
                   list.gradient_rect_h(plate, kModeRadius, c.with_alpha(0.3f),
                                        c.with_alpha(0.06f));
                   list.bordered_rect(plate, kModeRadius, look::kClear, 1.5f, c.with_alpha(0.5f));
               });
        for (int i = 0; i < kTabCount; ++i)
        {
            arrive(list, calm_ ? 1.0f : look::rise(since_, 1 + i, 0.06f, 0.45f), 14.0f,
                   [&]()
                   {
                       const Rect r = mode_rect(i);
                       const float lit = mode_lit_[static_cast<std::size_t>(i)].value;
                       const Rect tile{r.x + 12.0f, r.cy() - 22.0f, 44.0f, 44.0f};
                       if (lit > 0.01f)
                           list.glow(tile, 13.0f, 10.0f, c.with_alpha(0.35f * lit));
                       list.rounded_rect(tile, 13.0f, gfx::mix(c.with_alpha(0.15f), c, lit));
                       draw_mode_sign(list, i, tile.inset(10.0f), gfx::mix(c, look::kNight, lit));
                       const float x = tile.x + tile.w + 16.0f;
                       const float room = r.x + r.w - 12.0f - x;
                       ui::text_fit(list, fonts.semibold,
                                    tabs_.tabs()[static_cast<std::size_t>(i)].label, x, r.y + 31.0f,
                                    24.0f, room,
                                    look::kInk.with_alpha(tween::lerp(look::kMuted, 1.0f, lit)));
                       ui::text_fit(
                           list, fonts.regular, tr(kLines[i]), x, r.y + 56.0f, 19.0f, room,
                           look::kInk.with_alpha(tween::lerp(look::kFaint, look::kMuted, lit)));
                   });
        }
    }

    // outgoing: the tab is on its way out (nothing of it arrives or has the focus).
    void draw_tab(app::Context &ctx, gfx::DrawList &list, int which, bool outgoing) const
    {
        const Gate gate = which == tab() ? gate_ : gate_for(ctx, which);
        if (which == kFriend)
            draw_friend(ctx, list, outgoing);
        else if (gate != Gate::open)
            draw_closed(ctx, list, which, gate, outgoing);
        else
            draw_setup(ctx, list, which, outgoing);
    }

    void draw_setup(const app::Context &ctx, gfx::DrawList &list, int which, bool outgoing) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const bool computer = which == kComputer;
        const std::size_t g = computer ? 1 : 0;
        const std::span<const TimeControl> controls = computer ? ai_clocks() : pairing_pools();
        const int count = std::min(kTiles, static_cast<int>(controls.size()));

        // ---- the time controls: the light under the focused one first
        for (int i = 0; i < count; ++i)
        {
            const float hover = outgoing ? 0.0f : hover_[g][static_cast<std::size_t>(i)].value;
            if (hover <= 0.01f)
                continue;
            arrive(list, part(i, outgoing), 22.0f,
                   [&]()
                   {
                       look::lift(
                           list, tile_rect(i), hover,
                           look::speed_color(speed_of(controls[static_cast<std::size_t>(i)])),
                           ctx.time, calm_);
                   });
        }
        for (int i = 0; i < count; ++i)
        {
            const std::size_t at = static_cast<std::size_t>(i);
            arrive(list, part(i, outgoing), 22.0f,
                   [&]()
                   {
                       const float grow =
                           calm_ || outgoing ? 1.0f : look::rise(tab_since_, 4 + i, 0.06f, 0.8f);
                       draw_tile(fonts, list, controls[at], tile_rect(i), chosen_[g][at].value,
                                 outgoing ? 0.0f : hover_[g][at].value, grow);
                   });
        }

        // ---- the ticket
        const Color c = which == tab() ? ink_.value() : tint(which, Gate::open);
        const float lit = outgoing ? 0.0f : action_lit_.value;
        arrive(list, part(2, outgoing), 22.0f, [&]() { draw_ticket(list, c); });
        arrive(list, part(3, outgoing), 16.0f, [&]() { draw_stub(fonts, list, which, c); });
        if (computer)
        {
            arrive(list, part(4, outgoing), 16.0f, [&]() { draw_level(ctx, list, c, outgoing); });
            arrive(list, part(5, outgoing), 16.0f, [&]() { draw_sides(ctx, list, c); });
        }
        else
        {
            arrive(list, part(4, outgoing), 16.0f,
                   [&]() { draw_rating(fonts, list, c, outgoing); });
            arrive(list, part(5, outgoing), 16.0f, [&]() { draw_rated(fonts, list, c); });
        }
        arrive(list, part(6, outgoing), 16.0f,
               [&]()
               {
                   draw_action(list, fonts, kAction, computer ? play_.label : find_.label,
                               ui::Button::triangle, c, lit, action_press_.value, ctx.time, calm_);
               });
    }

    void draw_tile(const ui::Fonts &fonts, gfx::DrawList &list, const TimeControl &tc,
                   const Rect &r, float chosen, float hover, float grow) const
    {
        const look::Speed speed = speed_of(tc);
        const Color c = look::speed_color(speed);
        // The chosen time control stays marked while the controller is elsewhere.
        const float lit = std::max(chosen * 0.7f, hover);
        look::panel(list, r, lit, c);
        list.push_clip(r.inset(6.0f));
        look::halo(list, {r.x - 70.0f, r.y - 70.0f, 250.0f, 250.0f}, c, 0.1f + 0.16f * lit);
        list.pop_clip();

        const float x = r.x + kTilePad;
        const float room = r.w - 2.0f * kTilePad;
        const Rect sign{x, r.y + kTilePad, 56.0f, 56.0f};
        if (chosen > 0.01f)
            list.glow(sign, 16.0f, 12.0f, c.with_alpha(0.35f * chosen));
        list.rounded_rect(sign, 16.0f, gfx::mix(c.with_alpha(0.16f), c, chosen));
        look::speed_icon(list, sign.inset(14.0f), speed, gfx::mix(c, look::kNight, chosen));
        if (chosen > 0.01f)
        {
            // A check where the tile's corner is: this is the one.
            const float pop = calm_ ? chosen : tween::back_out(chosen);
            app::draw_verdict(list, r.x + r.w - kTilePad - 15.0f, sign.cy(), 15.0f * pop, true,
                              c.with_alpha(chosen), look::kNight.with_alpha(chosen));
        }

        kicker_fit(list, fonts, speed_text(tc), x, r.y + 130.0f, room, c);
        // The figure is as large as its tile allows ("Unlimited" is a long one).
        const std::string control = control_text(tc);
        const float size = fitted(fonts.semibold, control, 80.0f, room);
        look::figure(list, fonts, control, x - 3.0f, r.y + 212.0f, size, look::kInk);
        look::rule(list, x, r.y + 250.0f, room, 0.1f + 0.1f * lit);
        const float cy = (r.y + 250.0f + r.y + r.h) * 0.5f;
        ui::text_fit(list, fonts.regular, increment_text(tc), x, cy + 21.0f * 0.35f, 21.0f,
                     room - 64.0f, look::kInk.with_alpha(tween::lerp(look::kMuted, 0.9f, lit)));
        draw_time_sign(list, tc, x + room, cy, c, grow);
    }

    // The ticket's paper: a panel in the colour, a tinted stub, and a
    // perforation with a bite out of each side.
    void draw_ticket(gfx::DrawList &list, Color c) const
    {
        const Rect &t = kTicket;
        look::panel(list, t, 0.35f, c);
        list.push_clip({t.x, t.y, t.w, kNotch});
        list.gradient_rect({t.x + 1.5f, t.y + 1.5f, t.w - 3.0f, kNotch + 80.0f},
                           look::kRadius - 1.5f, c.with_alpha(0.17f), c.with_alpha(0.0f));
        list.pop_clip();
        const float y = t.y + kNotch;
        const look::Mood &sky = look::mood(Section::play);
        const Color hole = gfx::mix(sky.sky[0], sky.sky[1], 0.6f);
        const Color edge = gfx::mix(look::kInk.with_alpha(0.1f), c.with_alpha(0.62f), 0.35f);
        list.push_clip(t);
        for (const float x : {t.x, t.x + t.w})
        {
            list.circle(x, y, kBite, hole);
            list.ring(x, y, kBite, 1.5f, edge);
        }
        list.pop_clip();
        for (float x = t.x + kBite + 11.0f; x + 8.0f <= t.x + t.w - kBite - 8.0f; x += 16.0f)
            list.rounded_rect({x, y - 1.0f, 8.0f, 2.0f}, 1.0f, look::kInk.with_alpha(0.2f));
    }

    void draw_stub(const ui::Fonts &fonts, gfx::DrawList &list, int which, Color c) const
    {
        const std::size_t g = static_cast<std::size_t>(which);
        const Rect &t = kTicket;
        // Online, a tag in the corner says whether the game counts: the kicker
        // has the room beside the wider of the two.
        const char *rated = tr("Rated");
        const char *casual = tr("Casual");
        const float tags =
            which == kOnline
                ? std::max(tag_width(fonts, rated, 14.0f), tag_width(fonts, casual, 14.0f)) + 16.0f
                : 0.0f;
        kicker_fit(list, fonts, tr("Next game"), kTicketX, t.y + 46.0f, kTicketRoom - tags,
                   look::kInk.with_alpha(look::kMuted), Align::left, 14.0f);
        if (which == kOnline)
        {
            const float on = rated_on_[0].value;
            list.push_opacity(on);
            look::tag(list, fonts, rated, kTicketX + kTicketRoom, t.y + 41.0f, c, look::kNight,
                      Align::right, 14.0f);
            list.pop_opacity();
            list.push_opacity(1.0f - on);
            look::tag(list, fonts, casual, kTicketX + kTicketRoom, t.y + 41.0f,
                      look::kInk.with_alpha(0.16f), look::kInk, Align::right, 14.0f);
            list.pop_opacity();
        }

        // What was chosen: the words that were there leave upward, the new
        // ones settle from below.
        const float swap = tween::clamp01(stub_swap_[g].value);
        const auto words = [&](const Stub &stub, float alpha, float dy)
        {
            if (alpha <= 0.01f || stub.title.empty())
                return;
            const Color own = look::speed_color(stub.speed);
            list.push_opacity(alpha);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, dy);
            const Rect sign{kTicketX, t.y + 66.0f, 64.0f, 64.0f};
            list.glow(sign, 18.0f, 14.0f, own.with_alpha(0.3f));
            list.rounded_rect(sign, 18.0f, own);
            look::speed_icon(list, sign.inset(16.0f), stub.speed, look::kNight);
            const float x = sign.x + sign.w + 18.0f;
            const float room = kTicketX + kTicketRoom - x;
            kicker_fit(list, fonts, stub.kicker, x, t.y + 90.0f, room, own);
            ui::text(list, fonts.display, stub.title, x - 2.0f, t.y + 128.0f,
                     fitted(fonts.display, stub.title, 42.0f, room), look::kInk);
            ui::text_fit(list, fonts.regular, stub.body, kTicketX, t.y + 164.0f, 21.0f, kTicketRoom,
                         look::kInk.with_alpha(look::kMuted));
            list.pop_transform();
            list.pop_opacity();
        };
        words(stub_old_[g], 1.0f - swap, -8.0f * swap);
        words(stub_[g], swap, 10.0f * (1.0f - swap));
    }

    // Online: the rating the game counts for, how it moved and its line over time.
    void draw_rating(const ui::Fonts &fonts, gfx::DrawList &list, Color c, bool outgoing) const
    {
        const Rect &t = kTicket;
        const float right = kTicketX + kTicketRoom;
        kicker_fit(list, fonts, rating_label_, kTicketX, t.y + 226.0f, kTicketRoom, c);
        const float grow = calm_ || outgoing ? 1.0f : look::rise(chart_since_, 2, 0.08f, 0.9f);
        const Rect chart{kTicketX, t.y + 310.0f, kTicketRoom, 66.0f};
        if (has_rating_)
        {
            char text[16];
            std::snprintf(text, sizeof(text), "%d%s", static_cast<int>(rating_shown_.value + 0.5f),
                          provisional_ ? "?" : "");
            look::figure(list, fonts, text, kTicketX - 3.0f, t.y + 292.0f, 64.0f, look::kInk);
            look::delta(list, fonts, rating_change_, right, t.y + 262.0f, 22.0f, Align::right);
            // The games are counted beside the rating, in the room the whole
            // rating leaves (not the number that is still counting).
            std::snprintf(text, sizeof(text), "%d%s", static_cast<int>(rating_shown_.target + 0.5f),
                          provisional_ ? "?" : "");
            ui::text_fit(list, fonts.regular, rating_note_, right, t.y + 292.0f, 20.0f,
                         kTicketRoom - fonts.semibold.measure(text, 64.0f) - 16.0f,
                         look::kInk.with_alpha(look::kMuted), Align::right);
            look::sparkline(list, chart, rating_history_, c, grow);
        }
        else
        {
            // No games at this speed yet: say so instead of showing a zero.
            ui::text_fit(list, fonts.display, tr("No rating yet"), kTicketX - 2.0f, t.y + 276.0f,
                         36.0f, kTicketRoom, look::kInk);
            ui::text_fit(list, fonts.regular, tr("Your first games at this speed set it."),
                         kTicketX, t.y + 310.0f, 20.0f, kTicketRoom,
                         look::kInk.with_alpha(look::kMuted));
            look::sparkline(list, chart, {}, c, grow);
        }
        look::rule(list, kTicketX, t.y + 394.0f, kTicketRoom);
    }

    // Online: does the game count? The chosen answer is lit.
    void draw_rated(const ui::Fonts &fonts, gfx::DrawList &list, Color c) const
    {
        for (int i = 0; i < 2; ++i)
        {
            const Rect r = rated_rect(i);
            const float on = rated_on_[static_cast<std::size_t>(i)].value;
            const ui::ChoiceItem &item = rated_.items()[static_cast<std::size_t>(i)];
            draw_choice_plate(list, r, on, c);
            const float cx = r.x + 30.0f;
            list.ring(cx, r.cy(), 12.0f, 2.5f, gfx::mix(look::kInk.with_alpha(0.4f), c, on));
            if (on > 0.01f)
                list.circle(cx, r.cy(), 6.0f * (calm_ ? on : tween::back_out(on)), c);
            const float x = r.x + 60.0f;
            const float room = r.x + r.w - 16.0f - x;
            ui::text_fit(list, fonts.semibold, item.label, x, r.y + 31.0f, 24.0f, room,
                         look::kInk.with_alpha(tween::lerp(look::kMuted, 1.0f, on)));
            ui::text_fit(list, fonts.regular, item.description, x, r.y + 55.0f, 19.0f, room,
                         look::kInk.with_alpha(tween::lerp(look::kFaint, look::kMuted, on)));
        }
    }

    // Computer: the level as eight rising steps that fill as it rises.
    void draw_level(const app::Context &ctx, gfx::DrawList &list, Color c, bool outgoing) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Rect &t = kTicket;
        const float right = kTicketX + kTicketRoom;
        const float focus = outgoing ? 0.0f : level_lit_.value;
        if (focus > 0.01f)
            list.gradient_rect(kLevel, kRowRadius, c.with_alpha(0.13f * focus),
                               c.with_alpha(0.03f * focus));
        kicker_fit(list, fonts, tr("Strength"), kTicketX, t.y + 233.0f, kTicketRoom - 72.0f, c);
        // Left and right change it: the arrows say so while it has the controller.
        const float arrows = tween::lerp(0.25f, 0.95f, focus);
        draw_chevron(list, right - 44.0f - 5.0f * step_down_.value, t.y + 227.0f, 8.0f, -1,
                     look::kInk.with_alpha(arrows * (level() > 1 ? 1.0f : 0.3f)));
        draw_chevron(list, right - 10.0f + 5.0f * step_up_.value, t.y + 227.0f, 8.0f, 1,
                     look::kInk.with_alpha(arrows * (level() < kAiLevels ? 1.0f : 0.3f)));

        char digit[8];
        std::snprintf(digit, sizeof(digit), "%d", level());
        const float pop = 1.0f + 0.1f * std::max(step_down_.value, step_up_.value);
        list.push_transform(pop, kTicketX + 24.0f, t.y + 284.0f, 0.0f, 0.0f);
        const float width =
            look::figure(list, fonts, digit, kTicketX - 3.0f, t.y + 310.0f, 76.0f, look::kInk);
        list.pop_transform();
        // "3 of 8": the words after the figure end before the steps begin.
        const float left = kTicketX + 124.0f;
        ui::text_fit(list, fonts.regular, fill(tr("of {0}"), {std::to_string(kAiLevels)}),
                     kTicketX + width + 6.0f, t.y + 310.0f, 20.0f,
                     left - 10.0f - (kTicketX + width + 6.0f), look::kInk.with_alpha(look::kMuted));

        const float grow = calm_ || outgoing ? 1.0f : look::rise(tab_since_, 5, 0.06f, 0.7f);
        constexpr float kBar = 26.0f;
        const float pitch = (right - left - kBar) / static_cast<float>(kAiLevels - 1);
        const float bottom = t.y + 310.0f;
        for (int i = 0; i < kAiLevels; ++i)
        {
            const float h = tween::lerp(16.0f, 72.0f,
                                        static_cast<float>(i) / static_cast<float>(kAiLevels - 1));
            const Rect bar{left + static_cast<float>(i) * pitch, bottom - h, kBar, h};
            list.rounded_rect(bar, 7.0f, look::kInk.with_alpha(0.1f));
            const float filled = steps_[static_cast<std::size_t>(i)].value * grow;
            if (filled <= 0.01f)
                continue;
            const float fh = std::max(h * tween::clamp01(filled), 14.0f);
            const Rect fill{bar.x, bottom - fh, kBar, fh};
            const bool top = i == level() - 1;
            if (top)
                list.glow(fill, 7.0f, 12.0f, c.with_alpha(0.45f * tween::clamp01(filled)));
            list.rounded_rect(fill, 7.0f,
                              c.with_alpha((top ? 1.0f : 0.55f) * tween::clamp01(filled)));
        }
        ui::text_fit(list, fonts.regular, tr("From 1, the gentlest, to 8, the strongest"), kTicketX,
                     t.y + 344.0f, 19.0f, kTicketRoom, look::kInk.with_alpha(look::kMuted));
        look::rule(list, kTicketX, t.y + 376.0f, kTicketRoom);
    }

    // Computer: the side to play, as kings on squares of the board.
    void draw_sides(const app::Context &ctx, gfx::DrawList &list, Color c) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const board::BoardTheme &squares = ctx.board_theme();
        const Color light = Color::rgb(squares.light);
        const Color dark = Color::rgb(squares.dark);
        kicker_fit(list, fonts, tr("Your colour"), kTicketX, kTicket.y + 404.0f, kTicketRoom, c);
        // A label and its description stand side by side when every row has
        // the room. Longer words (a translation) stand on two lines, a little
        // smaller, in all three rows alike.
        const float room = kTicketRoom - 60.0f - 16.0f;
        bool stacked = false;
        for (const ui::ChoiceItem &item : color_.items())
            stacked = stacked || fonts.semibold.measure(item.label, 23.0f) + 16.0f +
                                         fonts.regular.measure(item.description, 19.0f) >
                                     room;
        for (int i = 0; i < 3; ++i)
        {
            const Rect r = side_rect(i);
            const float on = side_on_[static_cast<std::size_t>(i)].value;
            const ui::ChoiceItem &item = color_.items()[static_cast<std::size_t>(i)];
            draw_choice_plate(list, r, on, c);
            const Rect square{r.x + 6.0f, r.y + 5.0f, 40.0f, 40.0f};
            const Rect left{square.x, square.y, square.w * 0.5f, square.h};
            const Rect right{square.cx(), square.y, square.w * 0.5f, square.h};
            // The white king stands on a dark square, the black one on a
            // light one; a random side is half of each.
            if (i != 2)
            {
                list.push_clip(i == 0 ? left : square);
                list.rounded_rect(square, 10.0f, dark);
                ctx.pieces->draw(list, {chess::Color::white, chess::Role::king}, square);
                list.pop_clip();
            }
            if (i != 1)
            {
                list.push_clip(i == 0 ? right : square);
                list.rounded_rect(square, 10.0f, light);
                ctx.pieces->draw(list, {chess::Color::black, chess::Role::king}, square);
                list.pop_clip();
            }
            const float x = square.x + square.w + 14.0f;
            const Color ink = look::kInk.with_alpha(tween::lerp(look::kMuted, 1.0f, on));
            const Color quiet = look::kInk.with_alpha(tween::lerp(look::kFaint, look::kMuted, on));
            if (stacked)
            {
                ui::text_fit(list, fonts.semibold, item.label, x, r.y + 22.0f, 20.0f, room, ink);
                ui::text_fit(list, fonts.regular, item.description, x, r.y + 41.0f, 16.0f, room,
                             quiet);
                continue;
            }
            ui::text(list, fonts.semibold, item.label, x, r.cy() + 23.0f * 0.35f, 23.0f, ink);
            ui::text(list, fonts.regular, item.description, r.x + r.w - 16.0f,
                     r.cy() + 19.0f * 0.35f, 19.0f, quiet, Align::right);
        }
    }

    // The wide panel's words under a kicker whose baseline is `top`: a title
    // of up to title_lines lines, facts, a paragraph of up to body_lines
    // (longer words are set smaller: hero_title_size, hero_body). Returns the
    // baseline after the paragraph.
    float draw_hero_words(const ui::Fonts &fonts, gfx::DrawList &list, bool outgoing, Color c,
                          float top, std::string_view kicker, std::string_view title,
                          int title_lines, std::span<const Fact> facts, std::string_view body,
                          int body_lines) const
    {
        const float x = kHeroText;
        arrive(list, part(1, outgoing), 16.0f,
               [&]() { kicker_fit(list, fonts, kicker, x, top, kHeroRoom, c); });
        float y = top + kHeroTitle;
        arrive(list, part(2, outgoing), 16.0f,
               [&]()
               {
                   y = ui::paragraph(list, fonts.display, title, x - 3.0f, y,
                                     hero_title_size(fonts, title, title_lines), kHeroRoom,
                                     kHeroTitleLine, look::kInk, title_lines);
               });
        // ui::paragraph returns the next baseline: the facts start under the title.
        y -= kHeroTitleLine;
        if (!facts.empty())
        {
            arrive(list, part(3, outgoing), 16.0f,
                   [&]()
                   {
                       // A row too long for the panel (a translation) gives
                       // way evenly: each fact shrinks into its share.
                       float total = -44.0f;
                       for (const Fact &fact : facts)
                           total += fact_width(fonts, fact) + 44.0f;
                       const float squeeze = std::min(1.0f, kHeroRoom / total);
                       float fx = x;
                       for (const Fact &fact : facts)
                       {
                           const float room = fact_width(fonts, fact) * squeeze;
                           const float label =
                               kicker_fit(list, fonts, fact.label, fx, y + 58.0f, room,
                                          look::kInk.with_alpha(look::kFaint), Align::left, 14.0f);
                           const float width = ui::text_fit(list, fonts.semibold, fact.value, fx,
                                                            y + 94.0f, 30.0f, room, look::kInk);
                           fx += std::max({width, label, 96.0f * squeeze}) + 44.0f * squeeze;
                       }
                       look::rule(list, x, y + 122.0f, kHeroRoom);
                   });
            y += 122.0f - (kHeroBody - 46.0f);
        }
        y += kHeroBody;
        arrive(list, part(4, outgoing), 16.0f,
               [&]()
               {
                   const Running text = hero_body(fonts, body, body_lines);
                   y = ui::paragraph(list, fonts.regular, body, x, y, text.size, kHeroRoom,
                                     text.line, look::kInk.with_alpha(look::kMuted), text.lines);
               });
        return y;
    }

    void draw_friend(app::Context &ctx, gfx::DrawList &list, bool outgoing) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Color c = look::accent(Section::play);
        const float lit = outgoing ? 0.0f : action_lit_.value;
        const chess::Color turn = saved_position_.turn();
        arrive(list, part(0, outgoing), 22.0f,
               [&]()
               {
                   look::lift(list, kHero, lit, c, ctx.time, calm_);
                   look::panel(list, kHero, 0.2f + 0.8f * lit, c);
                   // The board as it was left, facing whoever is to move.
                   look::frame_board(list, kHeroSquares, c, 0.5f + 0.5f * lit);
                   board::draw_mini_board(list, *ctx.pieces, ctx.board_theme(), kHeroSquares,
                                          saved_position_, turn, saved_last_, 12.0f);
               });
        // With a game in progress the seats stand higher: a line less for the words.
        draw_hero_words(fonts, list, outgoing, c, kHero.y + 82.0f, tr("On this console"),
                        tr("Pass & Play"), 1, facts_,
                        tr("Two players share one controller and take turns. The board turns to "
                           "face whoever is to move, and the game is kept when you leave."),
                        saved_game_ ? 3 : 4);
        // The two seats: the side to move is the lit one.
        arrive(list, part(5, outgoing), 16.0f,
               [&]()
               {
                   const board::BoardTheme &squares = ctx.board_theme();
                   for (int i = 0; i < 2; ++i)
                   {
                       const bool white = i == 0;
                       const bool moves = white == (turn == chess::Color::white);
                       const Rect r{kHeroText + static_cast<float>(i) * (kSeatWidth + 16.0f),
                                    kHeroAction.y - (saved_game_ ? 172.0f : 124.0f), kSeatWidth,
                                    76.0f};
                       draw_choice_plate(list, r, moves ? 1.0f : 0.0f, c);
                       const Rect square{r.x + 12.0f, r.y + 12.0f, 52.0f, 52.0f};
                       list.rounded_rect(square, 12.0f,
                                         Color::rgb(white ? squares.dark : squares.light));
                       ctx.pieces->draw(
                           list,
                           {white ? chess::Color::white : chess::Color::black, chess::Role::king},
                           square);
                       const float x = square.x + square.w + 16.0f;
                       // The words end before the dot of the side to move.
                       const bool live = moves && saved_game_;
                       const float room = r.x + r.w - (live ? 44.0f : 16.0f) - x;
                       ui::text_fit(list, fonts.semibold, white ? tr("White") : tr("Black"), x,
                                    r.y + 33.0f, 24.0f, room,
                                    look::kInk.with_alpha(moves ? 1.0f : look::kMuted));
                       const char *line = saved_game_ ? (moves ? tr("To move") : tr("Waiting"))
                                                      : (white ? tr("Moves first") : tr("Replies"));
                       ui::text_fit(list, fonts.regular, line, x, r.y + 59.0f, 20.0f, room,
                                    moves ? c : look::kInk.with_alpha(look::kFaint + 0.1f));
                       if (live)
                           look::live_dot(list, r.x + r.w - 26.0f, r.cy(), 5.0f, c, ctx.time,
                                          calm_);
                   }
               });
        arrive(list, part(6, outgoing), 16.0f,
               [&]()
               {
                   if (saved_game_)
                       ui::text_fit(list, fonts.regular,
                                    tr("A game is in progress. Its menu starts a new one."),
                                    kHeroText, kHeroAction.y - 36.0f, 22.0f, kHeroRoom,
                                    look::kInk.with_alpha(look::kMuted));
                   draw_action(list, fonts, kHeroAction, friend_.label, ui::Button::cross, c, lit,
                               action_press_.value, ctx.time, calm_);
               });
    }

    // Where a closed tab's words start and where its button is: the group is
    // centred in the panel's height, whatever its title and body wrap to.
    struct Closed
    {
        float top = 0.0f; // the kicker's baseline
        Rect action{};
    };
    static Closed closed_layout(const ui::Fonts &fonts, int which, Gate gate)
    {
        // As draw_hero_words sets them.
        const char *heading = gate_title(which, gate);
        const float title = static_cast<float>(std::clamp(
            static_cast<int>(
                fonts.display.font
                    ->wrap(heading, hero_title_size(fonts, heading, kClosedTitleLines), kHeroRoom)
                    .size()),
            1, kClosedTitleLines));
        const char *words_text = gate_body(which, gate);
        const Running text = hero_body(fonts, words_text, kClosedBodyLines);
        const float body = static_cast<float>(std::clamp(
            static_cast<int>(fonts.regular.font->wrap(words_text, text.size, kHeroRoom).size()), 1,
            text.lines));
        // From the kicker's capitals to the last line of the body ...
        float height = 12.0f + kHeroTitle + (title - 1.0f) * kHeroTitleLine + kHeroBody +
                       (body - 1.0f) * text.line;
        const float words = height;
        // ... and the button under it, unless there is nothing to press.
        if (gate != Gate::signing_in)
            height += kHeroGap + kHeroAction.h;
        Closed at;
        at.top = kHero.y + (kHero.h - height) * 0.5f + 12.0f;
        at.action = {kHeroText, at.top - 12.0f + words + kHeroGap, kHeroAction.w, kHeroAction.h};
        return at;
    }

    static const char *gate_title(int which, Gate gate)
    {
        switch (gate)
        {
        case Gate::offline:
            return tr("You are offline");
        case Gate::signed_out:
            return which == kComputer ? tr("Sign in to play the computer")
                                      : tr("Sign in to play online");
        default:
            return tr("Signing in");
        }
    }

    static const char *gate_body(int which, Gate gate)
    {
        switch (gate)
        {
        case Gate::offline:
            return which == kComputer
                       ? tr("Stockfish plays on lichess.org, which needs the internet "
                            "connection. Pass & Play works without it.")
                       : tr("Finding an opponent needs the internet connection. "
                            "Pass & Play works without it.");
        case Gate::signed_out:
            return tr("Games are played with your Lichess account. Sign in once with your phone or "
                      "with a personal token.");
        default:
            return tr("Checking your Lichess account.");
        }
    }

    static const char *gate_action(Gate gate)
    {
        return gate == Gate::offline ? tr("Pass & Play") : tr("Sign in");
    }

    // A tab that cannot start a game says why, and what to do about it.
    void draw_closed(app::Context &ctx, gfx::DrawList &list, int which, Gate gate,
                     bool outgoing) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Color c = tint(which, gate);
        const float lit = outgoing ? 0.0f : action_lit_.value;
        arrive(list, part(0, outgoing), 22.0f,
               [&]()
               {
                   look::lift(list, kHero, lit, c, ctx.time, calm_);
                   look::panel(list, kHero, 0.2f + 0.8f * lit, c);
                   draw_gate_sign(ctx, list, gate, c);
               });
        // The tab that is showing has its layout at hand; one on its way out
        // works it out for these few frames.
        const Closed at =
            which == tab() && gate == gate_ ? closed_ : closed_layout(fonts, which, gate);
        draw_hero_words(fonts, list, outgoing, c, at.top,
                        gate == Gate::offline ? tr("No connection") : tr("Lichess account"),
                        gate_title(which, gate), kClosedTitleLines, {}, gate_body(which, gate),
                        kClosedBodyLines);
        if (gate == Gate::signing_in)
            return;
        arrive(list, part(5, outgoing), 16.0f,
               [&]()
               {
                   draw_action(list, fonts, at.action, gate_action(gate), ui::Button::cross, c, lit,
                               action_press_.value, ctx.time, calm_);
               });
    }

    // The sign of a closed tab, on a dark plate where the board would be:
    // a signal that does not get through, or a pawn waiting to be let in
    // (with an arc that turns while the account is checked).
    void draw_gate_sign(const app::Context &ctx, gfx::DrawList &list, Gate gate, Color c) const
    {
        const Rect &a = kHeroSquares;
        list.gradient_rect(a, 18.0f, look::kNight.with_alpha(0.55f), look::kNight.with_alpha(0.3f));
        list.bordered_rect(a, 18.0f, look::kClear, 1.5f, look::kInk.with_alpha(0.08f));
        const float cx = a.cx();
        const float cy = a.cy();
        constexpr float kDisc = 150.0f;
        list.push_clip(a.inset(2.0f));
        look::halo(list, a.inset(40.0f), c, 0.22f);
        // Rings that leave the sign, slowly: the page is waiting, not broken.
        for (int i = 0; i < 2; ++i)
        {
            const float offset = static_cast<float>(i) * 0.5f;
            const float along = calm_ ? offset + 0.25f : std::fmod(ctx.time * 0.22f + offset, 1.0f);
            list.ring(cx, cy, kDisc + 130.0f * along, 2.5f, c.with_alpha(0.4f * (1.0f - along)));
        }
        list.pop_clip();
        list.circle(cx, cy, kDisc, c.with_alpha(0.13f));
        list.ring(cx, cy, kDisc, 3.0f, c.with_alpha(0.85f));

        if (gate == Gate::offline)
        {
            // Three arcs fanning out of a dot, and a stroke across them.
            const float base = cy + 62.0f;
            const Color ink = look::kInk.with_alpha(0.85f);
            list.circle(cx, base, 13.0f, ink);
            for (int i = 0; i < 3; ++i)
                list.arc(cx, base, 52.0f + static_cast<float>(i) * 40.0f, 13.0f, kTau - 0.72f,
                         1.44f, ink.with_alpha(0.85f - 0.2f * static_cast<float>(i)));
            list.line(cx - 92.0f, cy + 84.0f, cx + 92.0f, cy - 84.0f, 26.0f, look::kNight);
            list.line(cx - 92.0f, cy + 84.0f, cx + 92.0f, cy - 84.0f, 12.0f, c);
            return;
        }
        ctx.pieces->draw(list, {chess::Color::white, chess::Role::pawn},
                         {cx - 120.0f, cy - 128.0f, 240.0f, 240.0f});
        if (gate == Gate::signing_in)
        {
            const float turn = calm_ ? 0.6f : ctx.time * 3.2f;
            list.arc(cx, cy, kDisc + 22.0f, 7.0f, turn, 1.5f, c);
            list.arc(cx, cy, kDisc + 22.0f, 7.0f, turn + kTau * 0.5f, 1.5f, c.with_alpha(0.5f));
            return;
        }
        // A plus on the disc's edge: there is room for you.
        const float bx = cx + 106.0f;
        const float by = cy + 106.0f;
        list.circle(bx, by, 40.0f, look::kNight);
        list.circle(bx, by, 34.0f, c);
        list.rounded_rect({bx - 17.0f, by - 3.5f, 34.0f, 7.0f}, 3.5f, look::kNight);
        list.rounded_rect({bx - 3.5f, by - 17.0f, 7.0f, 34.0f}, 3.5f, look::kNight);
    }

    ui::TabBar tabs_;
    ui::GridView pools_;  // Online: what can be paired
    ui::GridView clocks_; // Computer: the clocks Stockfish plays at
    ui::RadioGroup rated_;
    ui::ChoicePicker level_;
    ui::RadioGroup color_;
    ui::PushButton find_;
    ui::PushButton play_;
    ui::PushButton friend_;
    ui::PushButton way_out_; // out of a tab that cannot be used

    Zone zone_ = Zone::grid;
    Gate gate_ = Gate::open;
    float refresh_in_ = 0.0f;
    bool calm_ = false; // reduced motion

    // The saved Pass & Play game, as the wide panel shows it.
    bool saved_game_ = false;
    std::string saved_moves_;
    chess::Position saved_position_ = chess::Position::start();
    chess::Move saved_last_;
    std::array<Fact, 3> facts_;
    Closed closed_; // a tab that cannot be used: where its words and its button are

    // The ticket: what was chosen on each tab, and Online's rating.
    std::array<Stub, 2> stub_;
    std::array<Stub, 2> stub_old_;
    std::array<tween::Spring, 2> stub_swap_; // 0 -> 1 as new words take the old ones' place
    std::string rating_perf_;                // the speed the rating is of ("rapid")
    std::string rating_label_;
    std::string rating_note_;
    std::vector<float> rating_history_;
    int rating_games_ = -1;
    int rating_change_ = 0;
    bool has_rating_ = false;
    bool provisional_ = false;
    tween::Spring rating_shown_; // the number on screen, counting toward the rating

    // Arrivals and the things that glide.
    float since_ = 0.0f;                       // seconds since the page was entered
    float tab_since_ = 0.0f;                   // ... since the tab's content was shown
    float chart_since_ = 0.0f;                 // ... since the rating's line started growing
    int from_tab_ = kOnline;                   // the tab that is leaving
    tween::Spring tab_swap_{1.0f, 0.0f, 1.0f}; // 0 -> 1 as it gives way to the new one
    bool placed_ = false;
    ui::SpringRect plate_;      // the lit plate under the way to play
    ui::SpringRect ring_;       // the focus ring
    tween::Spring ring_radius_; // ... its corners
    ui::SpringColor ring_ink_;  // ... its colour
    tween::Spring ring_alpha_;  // ... and whether the page has the controller
    ui::SpringColor ink_;       // the colour of the ticket: the chosen speed's
    std::array<tween::Spring, kTabCount> mode_lit_;
    std::array<std::array<tween::Spring, kTiles>, 2> chosen_; // per grid, per tile
    std::array<std::array<tween::Spring, kTiles>, 2> hover_;
    std::array<tween::Spring, 2> rated_on_;
    std::array<tween::Spring, 3> side_on_;
    std::array<tween::Spring, kAiLevels> steps_;
    tween::Spring level_lit_;
    tween::Spring action_lit_;
    ui::Pulse press_;        // a choice was made: the ring squeezes
    ui::Pulse action_press_; // the button that starts a game went down
    ui::Pulse refusal_;      // an edge was pushed: the ring shakes
    ui::Pulse mode_refusal_; // ... or the plate, for a shoulder button at the last tab
    ui::Pulse step_down_;    // the level went down, or up: its arrow jumps
    ui::Pulse step_up_;
    bool refused_vertical_ = false;
};

} // namespace

std::unique_ptr<Page> make_play_page(app::Context &ctx)
{
    return std::make_unique<PlayPage>(ctx);
}

} // namespace pch::modes
