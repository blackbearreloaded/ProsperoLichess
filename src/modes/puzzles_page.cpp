// ProsperoLichess - Puzzles page: the four ways to play and the themes of the offline pack.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// In the house look (app/look.hpp):
//
//   - the four ways to play are a row of lit tiles on the page's four columns,
//     each with a sign and a hue of its own; one ring glides between them and
//     the focused tile floats;
//   - under them the focused way is told in full: its board, a display title,
//     its facts as figures and a chart of its own (today's puzzle against the
//     player's rating, the rating's line over time, the climb of the best
//     streak, the combos of a storm). It gives way to the next one when the
//     focus moves;
//   - the themes of the pack are a catalogue: a sign per theme in the colour
//     of its family, its share of the pack as a level, and a segmented control
//     for how hard the puzzles are;
//   - the page assembles a part at a time, its numbers count and its charts
//     grow.
//
// The two grids and the picker of the kit still own the focus, the sounds and
// the refusals; the tiles, the ring and the segments are drawn here.

#include "board/mini_board.hpp"
#include "core/strings.hpp"
#include "lichess/json.hpp"
#include "lichess/puzzle_sources.hpp"
#include "lichess/session.hpp"
#include "modes/page.hpp"
#include "modes/puzzle_scene.hpp"
#include "modes/scenes.hpp"
#include "puzzles/pack.hpp"
#include "puzzles/puzzle.hpp"
#include "puzzles/records.hpp"
#include "ui/components/choice.hpp"
#include "ui/components/grid.hpp"
#include "ui/components/stat.hpp"
#include "ui/components/tabs.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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

constexpr float kWidth = app::kRight - app::kContent;
constexpr float kHead = 56.0f; // the row of the tabs
constexpr Rect kTabs{app::kContent, app::kTop, 360.0f, kHead};
constexpr float kHeadLine = app::kTop + kHead * 0.5f;   // its centre line
constexpr float kBelow = app::kTop + kHead + app::kGap; // where a section starts
constexpr float kSlide = 22.0f;                         // how far a section travels as it gives way

// ---- the ways to play: four tiles, and the focused one in full under them ----
constexpr float kTileHeight = 196.0f;
constexpr float kTilePad = 22.0f;
constexpr float kHeroTop = kBelow + kTileHeight + app::kGap;
constexpr Rect kHero{app::kContent, kHeroTop, kWidth, app::kBottom - kHeroTop};
constexpr float kHeroPad = 24.0f;
constexpr float kHeroBoard = kHero.h - 2.0f * kHeroPad;
constexpr float kHeroText = kHero.x + kHeroPad + kHeroBoard + 44.0f;
constexpr float kHeroRight = kHero.x + kHero.w - 36.0f;
constexpr float kBand = kHero.y + 366.0f; // the figure and the chart at the hero's foot
constexpr Rect kChart{kHeroText + 300.0f, kBand + 40.0f, kHeroRight - kHeroText - 300.0f, 84.0f};

// ---- the themes: a catalogue on the same four columns ----
constexpr float kCardHeight = 116.0f;
constexpr float kCardGap = 20.0f;
constexpr float kBleed = 28.0f;    // room around the grid for the focused card's light
constexpr float kSegment = 128.0f; // one difficulty of the segmented control
constexpr float kGlyph = 36.0f;    // the L2 and R2 beside it

enum Tab
{
    kTabModes,
    kTabThemes,
};

enum Mode
{
    kDaily,
    kTraining,
    kStreak,
    kStorm,
    kModeCount,
};

// What the page says about a mode whatever the state of the app: English
// here, translated where it is drawn.
struct ModeText
{
    PuzzleKind kind;
    const char *title;
    const char *kicker;
    const char *description;
    const char *figure; // what the number on its tile is
    bool needs_network;
    int preview_rating; // a pack position of this rating stands in for a board
    std::uint64_t preview_salt;
};
constexpr ModeText kModes[kModeCount] = {
    {PuzzleKind::daily, TR("Daily Puzzle"), TR("Lichess \xC2\xB7 every day"),
     TR("One new puzzle every day, shared by every player on Lichess. Find the best move to win "
        "material or deliver mate."),
     TR("Today's rating"), true, 0, 0},
    {PuzzleKind::training, TR("Puzzle Training"), TR("Lichess \xC2\xB7 rated"),
     TR("Endless puzzles picked for your level. Signed in, every result counts towards your "
        "Lichess puzzle rating."),
     TR("Your rating"), true, 1500, 0x1234},
    {PuzzleKind::streak, TR("Puzzle Streak"), TR("Offline \xC2\xB7 endless"),
     TR("Solve puzzles of rising difficulty. One mistake ends the run, and you get a single skip. "
        "How far can you go?"),
     TR("Best streak"), false, 1200, 0x5eed},
    {PuzzleKind::storm, TR("Puzzle Storm"), TR("Offline \xC2\xB7 3 minutes"),
     TR("Race the clock through as many puzzles as you can. Combos add time; mistakes cost ten "
        "seconds."),
     TR("Best score"), false, 900, 0x5707},
};

// Themes that make good practice sessions come first; the rest follow A-Z.
constexpr const char *kFeatured[] = {"mateIn1",   "mateIn2",    "mateIn3",      "fork",
                                     "pin",       "skewer",     "hangingPiece", "discoveredAttack",
                                     "sacrifice", "deflection", "endgame",      "rookEndgame"};

// Themes that say something about the game or the puzzle, not about a tactic.
constexpr std::string_view kAbout[] = {"opening",  "middlegame", "short",          "long",
                                       "veryLong", "oneMove",    "crushing",       "advantage",
                                       "equality", "master",     "masterVsMaster", "superGM"};

std::string number(float value)
{
    return std::to_string(static_cast<int>(std::lround(value)));
}

ui::TabItem tab(const char *label)
{
    ui::TabItem item;
    item.label = label;
    return item;
}

// Small tracked capitals kept inside a width: look::kicker for a text that a
// translation may make longer than its place. Returns the width.
float kicker_fit(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
                 float baseline, Color color, float size, float width, Align align = Align::left)
{
    return ui::text_fit(list, fonts.semibold, ui::upper(text), x, baseline, size, width, color,
                        align, 3.0f);
}

// The first letter of a name, whole: outside ASCII a letter takes several bytes.
std::string_view first_letter(std::string_view text)
{
    std::size_t length = text.empty() ? 0 : 1;
    while (length < text.size() && (static_cast<unsigned char>(text[length]) & 0xC0) == 0x80)
        ++length;
    return text.substr(0, length);
}

// A label and its value on one line, centred on cx and kept inside [left, right].
void pair(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view label,
          std::string_view value, float cx, float baseline, Color ink, float left, float right)
{
    const std::string caps = ui::upper(label);
    const float label_width = fonts.semibold.measure(caps, 13.0f, 3.0f);
    const float width = label_width + 8.0f + fonts.semibold.measure(value, 22.0f);
    const float x = std::clamp(cx - width * 0.5f, left, std::max(left, right - width));
    ui::text(list, fonts.semibold, caps, x, baseline - 1.0f, 13.0f, ink.with_alpha(look::kMuted),
             Align::left, 3.0f);
    look::figure(list, fonts, value, x + label_width + 8.0f, baseline, 22.0f, ink);
}

// A board to show for a mode.
struct Preview
{
    chess::Position position = chess::Position::start();
    chess::Move last_move;
    chess::Color orientation = chess::Color::white;
};

// A position from the offline pack (the same one every time for a salt).
void pack_preview(const app::Context &ctx, Preview *preview, int rating, std::uint64_t salt)
{
    if (ctx.pack == nullptr || ctx.pack->size() == 0)
        return;
    std::uint64_t rng = salt | 1u;
    std::size_t index = 0;
    if (!ctx.pack->pick(
            rating - 100, rating + 100, -1, &rng, [](std::size_t) { return false; }, &index))
        return;
    puzzles::PackPuzzle raw;
    puzzles::Puzzle puzzle;
    std::string error;
    if (!ctx.pack->get(index, &raw) ||
        !puzzles::from_csv(raw.id, raw.fen, raw.moves, raw.rating, &puzzle, &error))
        return;
    preview->position = puzzle.start();
    preview->last_move = puzzle.setup;
    preview->orientation = puzzle.solver();
}

// A theme of the pack as the catalogue shows it.
struct ThemeEntry
{
    int theme = -1; // -1: every theme
    std::string label;
    std::size_t count = 0;
    std::string count_text;                // "12,164 puzzles"
    float share = 0.0f;                    // its puzzles against the largest theme's
    PuzzleKind family = PuzzleKind::daily; // whose hue it wears
    std::string mark;                      // the letters on its tile ("#2", "F")
    bool piece = false;                    // ... or a chess piece (the endgames)
    chess::Role role = chess::Role::king;
};

// Gives a theme its family and its sign: mates are "#" as a score sheet
// writes them, endgames show the piece they are about, everything else its
// initial; themes about the game rather than a tactic form a family too.
void dress(std::string_view name, ThemeEntry *entry)
{
    const auto ends_with = [&](std::string_view tail)
    { return name.size() >= tail.size() && name.substr(name.size() - tail.size()) == tail; };
    entry->mark = entry->label.empty() ? std::string("?") : ui::upper(first_letter(entry->label));
    if (name.empty())
    {
        entry->family = PuzzleKind::themes;
        entry->mark.clear();
    }
    else if (name.substr(0, 6) == "mateIn")
    {
        entry->family = PuzzleKind::storm;
        entry->mark = "#" + std::string(name.substr(6, 1));
    }
    else if (name == "mate" || ends_with("Mate"))
    {
        entry->family = PuzzleKind::storm;
        entry->mark = "#";
    }
    else if (name == "endgame" || ends_with("Endgame"))
    {
        entry->family = PuzzleKind::training;
        entry->piece = true;
        entry->role = name.substr(0, 6) == "bishop"   ? chess::Role::bishop
                      : name.substr(0, 6) == "knight" ? chess::Role::knight
                      : name.substr(0, 4) == "pawn"   ? chess::Role::pawn
                      : name.substr(0, 5) == "queen"  ? chess::Role::queen
                      : name.substr(0, 4) == "rook"   ? chess::Role::rook
                                                      : chess::Role::king;
    }
    else if (std::find(std::begin(kAbout), std::end(kAbout), name) != std::end(kAbout))
    {
        entry->family = PuzzleKind::streak;
    }
    else
    {
        entry->family = PuzzleKind::daily;
    }
}

// What the hero says a mode's state is: a line and the dot before it.
struct Standing
{
    std::string words;
    Color dot = look::kGood;
    bool live = true; // the dot pulses: the mode can be played now
};

// A fact of the hero: a small label over a figure, both in the player's language.
struct Fact
{
    const char *label;
    std::string value;
};

class PuzzlesPage final : public Page
{
  public:
    explicit PuzzlesPage(app::Context &ctx)
    {
        tabs_.set_tabs({tab(tr("Modes")), tab(tr("Themes"))});
        tabs_.style.kind = ui::TabKind::pill;
        tabs_.style.width = ui::TabWidth::equal;
        tabs_.set_bounds(kTabs);
        tabs_.set_active(kTabModes, true);
        tabs_.set_focused(false);

        // ---- the four modes: the grid keeps the focus and the sounds, the
        // tiles are drawn by the page ----
        modes_.style.columns = kModeCount;
        modes_.style.gap_x = app::kGap;
        modes_.style.padding = 0.0f;
        modes_.style.cell_height = kTileHeight;
        modes_.style.exits.left = true; // back to the rail
        // Starting a mode is the big step of this page.
        modes_.style.sounds.activate = audio::Cue::launch;
        modes_.set_bounds({app::kContent, kBelow, kWidth, kTileHeight});
        for (int mode = 0; mode < kModeCount; ++mode)
        {
            if (kModes[mode].preview_rating > 0)
                pack_preview(ctx, &previews_[static_cast<std::size_t>(mode)],
                             kModes[mode].preview_rating, kModes[mode].preview_salt);
        }

        // ---- the themes of the pack ----
        themes_.style.columns = 4;
        themes_.style.gap_x = app::kGap;
        themes_.style.gap_y = kCardGap;
        themes_.style.cell_height = kCardHeight;
        themes_.style.padding = kBleed;
        themes_.style.entrance_step = 0.03f;
        themes_.style.scroll_thumb = false; // the place in the catalogue is a figure above it
        themes_.style.exits.left = true;
        themes_.style.card.art_aspect = 0.0f;
        themes_.style.card.text = ui::CardText::none;
        // A card keeps its place on the grid; the page lights it and rings it.
        themes_.style.card.focus_scale = 1.0f;
        themes_.style.card.lift = 0.0f;
        themes_.style.card.ring = false;
        themes_.style.card.shadow = false;
        themes_.style.sounds.activate = audio::Cue::launch;
        themes_.content = [this, &ctx](ui::Canvas &canvas, const Rect &cell, const ui::CardItem &,
                                       int index, float focus)
        { draw_card(ctx, canvas, cell, index, focus); };
        themes_.set_bounds({app::kContent - kBleed, kBelow - kBleed, kWidth + 2.0f * kBleed,
                            app::kBottom - kBelow + kBleed});
        english_ = catalog().size() == 0;
        load_themes(ctx);

        std::vector<std::string> bands;
        for (const PuzzleBand &band : kPuzzleBands)
            bands.emplace_back(puzzle_band_label(band));
        difficulty_.set_options(std::move(bands));
        difficulty_.set_index(1);
        difficulty_.style.wrap = false;
        difficulty_.set_bounds(track());
        difficulty_.set_active(false);

        missing_.title = tr("No offline puzzles");
        missing_.body = tr("The puzzle pack that ships with the app could not be read. Install the "
                           "app again to restore it.");
        missing_.action.clear();
        missing_.set_bounds({app::kContent, kBelow, kWidth, app::kBottom - kBelow});

        look::tint(look::accent(look::Section::puzzles), tabs_, modes_, themes_, difficulty_,
                   missing_);
        refresh(ctx);
    }

    void enter(app::Context &ctx) override
    {
        records_ = puzzles::load_records(ctx.data_root);
        refresh(ctx);
        // The page assembles again, and its numbers count again.
        since_ = 0.0f;
        detail_since_ = -0.45f;
        themes_since_ = 0.0f;
        detail_before_ = -1;
        detail_.snap(1.0f);
        for (tween::Spring &figure : figures_)
            figure.snap(0.0f);
        band_figure_.snap(0.0f);
        solved_.snap(0.0f);
        puzzles_.snap(0.0f);
        themes_.enter();
    }

    PageResult update(app::Context &ctx, const InputFrame &input, float dt, bool focused) override
    {
        fit_tabs(ctx);
        ctx.calm(tabs_, modes_, themes_, difficulty_, missing_);
        calm_ = ctx.reduced_motion();
        since_ += dt;
        age_ += dt;
        detail_since_ += dt;
        themes_since_ += dt;
        // Whether Lichess can be reached, and today's puzzle, may change under the page.
        refresh_in_ -= dt;
        if (refresh_in_ <= 0.0f)
            refresh(ctx);

        PageResult result;
        if (focused)
        {
            const bool previous = input.is_pressed(Action::page_prev);
            if (previous || input.is_pressed(Action::page_next))
            {
                if (tabs_.step(previous ? -1 : 1, input, *ctx.feedback) == ui::Event::changed)
                {
                    // The section that arrives draws its charts again.
                    detail_since_ = 0.0f;
                    themes_since_ = 0.0f;
                    band_figure_.snap(0.0f);
                    if (tabs_.active() == kTabThemes)
                        themes_.enter();
                }
            }
            else if (tabs_.active() == kTabModes)
            {
                handle_modes(ctx, input, &result);
            }
            else
            {
                handle_themes(ctx, input, &result);
            }
        }
        modes_.set_active(focused && tabs_.active() == kTabModes);
        themes_.set_active(focused && tabs_.active() == kTabThemes);

        tabs_.update(dt);
        modes_.update(dt);
        themes_.update(dt);
        difficulty_.update(dt);
        missing_.update(dt);
        animate(dt, focused);
        return result;
    }

    void draw(app::Context &ctx, app::Frame &frame, bool) const override
    {
        gfx::DrawList &list = frame.scene;
        ui::Canvas canvas = app::canvas_for(ctx, list);

        {
            const float in = arrive(0);
            list.push_opacity(in);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in, 14.0f));
            tabs_.draw(canvas);
            list.pop_transform();
            list.pop_opacity();
        }

        // One section gives way to the other: it leaves to the side it came
        // from and the next one arrives a beat later.
        const float at =
            calm_ ? static_cast<float>(tabs_.active()) : tween::clamp01(tabs_.active_value());
        const float leaving = 1.0f - tween::smoothstep(at * 2.2f);
        const float arriving = tween::smoothstep(at * 2.2f - 1.2f);
        if (leaving > 0.004f)
        {
            list.push_opacity(leaving);
            list.push_transform(1.0f, 0.0f, 0.0f, calm_ ? 0.0f : -kSlide * at, 0.0f);
            draw_modes(ctx, list);
            list.pop_transform();
            list.pop_opacity();
        }
        if (arriving > 0.004f)
        {
            list.push_opacity(arriving);
            list.push_transform(1.0f, 0.0f, 0.0f, calm_ ? 0.0f : kSlide * (1.0f - at), 0.0f);
            draw_themes(ctx, canvas);
            list.pop_transform();
            list.pop_opacity();
        }
    }

    std::span<const ui::Hint> hints() const override
    {
        static constexpr ui::Hint kModeHints[] = {{ui::Button::cross, TR("Play")},
                                                  {ui::Button::dpad, TR("Choose")},
                                                  {ui::Button::l1, TR("Section"), ui::Button::r1},
                                                  {ui::Button::options, TR("Menu")}};
        static constexpr ui::Hint kThemeHints[] = {
            {ui::Button::cross, TR("Start")},
            {ui::Button::l1, TR("Section"), ui::Button::r1},
            {ui::Button::l2, TR("Difficulty"), ui::Button::r2},
            {ui::Button::options, TR("Menu")}};
        if (tabs_.active() == kTabModes)
            return kModeHints;
        return kThemeHints;
    }

    app::look::Section section() const override
    {
        return app::look::Section::puzzles;
    }

    // The sky leans toward the hue of the way to play that has the focus.
    app::look::Mood mood() const override
    {
        app::look::Mood mood = app::look::mood(section());
        if (tabs_.active() == kTabModes)
            mood.sky[2] = gfx::mix(
                mood.sky[2], gfx::mix(puzzle_hue(kModes[focused_mode()].kind), look::kNight, 0.5f),
                0.55f);
        return mood;
    }

    const char *title() const override
    {
        return tr("Puzzles");
    }

    bool tabbed() const override
    {
        return true;
    }

    const char *name() const override
    {
        return "puzzles";
    }

  private:
    // ---- input ---------------------------------------------------------------

    void handle_modes(app::Context &ctx, const InputFrame &input, PageResult *result)
    {
        const int before = modes_.focus();
        const ui::Event event = modes_.handle(input, *ctx.feedback);
        if (modes_.focus() != before)
        {
            // The mode that had the focus leaves the hero; the new one follows it in.
            detail_before_ = before;
            detail_side_ = modes_.focus() > before ? 1.0f : -1.0f;
            detail_.snap(0.0f);
            detail_since_ = 0.0f;
            band_figure_.snap(0.0f);
        }
        if (event == ui::Event::cancelled ||
            (event == ui::Event::none && modes_.exit() == Direction::left))
        {
            result->to_rail = true;
            return;
        }
        // A mode that cannot run refuses by itself, and so does the last
        // tile's far edge: the tile and the ring answer with a nudge.
        if (event == ui::Event::refused && !input.nav_repeat && !calm_)
            refusal_.trigger();
        if (event != ui::Event::activated)
            return;
        if (!calm_)
            press_.trigger();
        switch (modes_.focus())
        {
        case kDaily:
            result->transition = app::Transition::push(make_daily_puzzle(ctx));
            break;
        case kTraining:
            result->transition = app::Transition::push(make_puzzle_training(ctx));
            break;
        case kStreak:
            result->transition = app::Transition::push(make_puzzle_streak(ctx));
            break;
        default:
            result->transition = app::Transition::push(make_puzzle_storm(ctx));
            break;
        }
    }

    void handle_themes(app::Context &ctx, const InputFrame &input, PageResult *result)
    {
        // L2 and R2 pick how hard the puzzles are, wherever the focus is.
        const bool easier = input.is_pressed(Action::jump_prev);
        if (easier || input.is_pressed(Action::jump_next))
        {
            InputFrame step;
            step.nav = easier ? Direction::left : Direction::right;
            const ui::Event event = difficulty_.handle(step, *ctx.feedback);
            plate_side_ = easier ? -1.0f : 1.0f;
            if (event == ui::Event::changed && !calm_)
                plate_press_.trigger();
            else if (event == ui::Event::refused && !calm_)
                plate_refusal_.trigger();
            return;
        }
        if (entries_.empty())
        {
            if (input.is_pressed(Action::back))
                ctx.cue(audio::Cue::back);
            if (input.is_pressed(Action::back) || input.nav == Direction::left)
                result->to_rail = true;
            return;
        }
        const ui::Event event = themes_.handle(input, *ctx.feedback);
        if (event == ui::Event::cancelled ||
            (event == ui::Event::none && themes_.exit() == Direction::left))
        {
            result->to_rail = true;
            return;
        }
        if (event == ui::Event::refused && !input.nav_repeat && !calm_)
        {
            theme_refusal_.trigger();
            theme_refused_ = input.nav;
        }
        if (event != ui::Event::activated)
            return;
        if (!calm_)
            theme_press_.trigger();
        const ThemeEntry &entry = focused_entry();
        const PuzzleBand &band =
            kPuzzleBands[std::clamp(difficulty_.index(), 0, kPuzzleBandCount - 1)];
        result->transition =
            app::Transition::push(make_puzzle_theme(ctx, entry.theme, (band.min + band.max) / 2));
    }

    // ---- what the page shows ---------------------------------------------------

    // The row of tabs is as wide as its labels make it. The English ones fit
    // the room they were given; a translation may need more, and a row wider
    // than its bounds would scroll and be cut. Once, when the fonts can measure.
    void fit_tabs(const app::Context &ctx)
    {
        if (tabs_fitted_ || ctx.fonts == nullptr || ctx.fonts->semibold.font == nullptr)
            return;
        tabs_fitted_ = true;
        const Rect first = tabs_.tab_rect(*ctx.fonts, kTabModes);
        const Rect last = tabs_.tab_rect(*ctx.fonts, kTabThemes);
        const float row = last.x + last.w - first.x;
        if (row <= kTabs.w)
            return;
        tabs_width_ = row + 1.0f;
        tabs_.set_bounds({kTabs.x, kTabs.y, tabs_width_, kTabs.h});
    }

    // The pack's themes in browsing order, once: the pack never changes.
    void load_themes(app::Context &ctx)
    {
        if (ctx.pack == nullptr || ctx.pack->size() == 0)
            return;
        const auto add =
            [this](int theme, std::string_view name, std::string label, std::size_t count)
        {
            ThemeEntry entry;
            entry.theme = theme;
            entry.label = std::move(label);
            entry.count = count;
            entry.count_text =
                plural(TR("{0} puzzle"), TR("{0} puzzles"), static_cast<long long>(count));
            dress(name, &entry);
            entries_.push_back(std::move(entry));
        };
        add(-1, "", tr("Mixed themes"), ctx.pack->size());
        for (const char *name : kFeatured)
        {
            const int theme = ctx.pack->theme_index(name);
            if (theme >= 0)
                add(theme, name, puzzles::theme_label(name), ctx.pack->theme_puzzle_count(theme));
        }
        const std::size_t featured = entries_.size();
        for (int theme = 0; theme < ctx.pack->theme_count(); ++theme)
        {
            const std::string &name = ctx.pack->theme_name(theme);
            const std::size_t count = ctx.pack->theme_puzzle_count(theme);
            // A theme with a handful of puzzles is not a practice session.
            if (count < 30 ||
                std::find_if(std::begin(kFeatured), std::end(kFeatured), [&](const char *known)
                             { return name == known; }) != std::end(kFeatured))
                continue;
            add(theme, name, puzzles::theme_label(name), count);
        }
        std::sort(entries_.begin() + static_cast<std::ptrdiff_t>(featured), entries_.end(),
                  [](const ThemeEntry &a, const ThemeEntry &b) { return a.label < b.label; });

        // A theme's level is its size against the largest single theme.
        std::size_t largest = 1;
        for (const ThemeEntry &entry : entries_)
        {
            if (entry.theme >= 0)
                largest = std::max(largest, entry.count);
        }
        std::vector<ui::CardItem> items;
        for (ThemeEntry &entry : entries_)
        {
            entry.share =
                std::min(1.0f, static_cast<float>(entry.count) / static_cast<float>(largest));
            ui::CardItem item;
            item.title = entry.label;
            item.subtitle = entry.count_text;
            item.tag = entry.theme;
            items.push_back(std::move(item));
        }
        themes_.set_items(std::move(items));
        themes_.set_focus(0);
    }

    // Reads what the modes show from the session, the pack and the records.
    void refresh(app::Context &ctx)
    {
        refresh_in_ = 0.5f;
        lichess::Session *session = ctx.lichess;
        online_ = session != nullptr && session->online();
        signed_in_ = session != nullptr && session->signed_in();
        const lichess::Perf *perf = signed_in_ ? session->perf("puzzle") : nullptr;
        rating_ = perf != nullptr ? perf->rating : 0;
        rating_change_ = perf != nullptr ? perf->progress : 0;
        rating_games_ = perf != nullptr ? perf->games : 0;
        rating_provisional_ = perf != nullptr && perf->provisional;
        if (perf != nullptr)
            rating_history_ = perf->history;
        else
            rating_history_.clear();
        pack_size_ = ctx.pack != nullptr ? ctx.pack->size() : 0;
        const bool pack = pack_size_ > 0;

        // Today's puzzle is the Daily mode's board.
        const std::string none;
        const std::string &json = session != nullptr ? session->daily_json() : none;
        if (json != daily_json_)
        {
            daily_json_ = json;
            daily_rating_ = 0;
            daily_plays_ = 0;
            previews_[kDaily] = {};
            puzzles::Puzzle puzzle;
            std::string error;
            if (!json.empty() && lichess::parse_api_puzzle(json, &puzzle, &error))
            {
                previews_[kDaily].position = puzzle.start();
                previews_[kDaily].last_move = puzzle.setup;
                previews_[kDaily].orientation = puzzle.solver();
                daily_rating_ = puzzle.rating;
                daily_white_ = puzzle.solver() == chess::Color::white;
                const lichess::Document doc(json);
                daily_plays_ = doc.root()["puzzle"]["plays"].integer();
            }
        }

        std::string signature;
        for (int mode = 0; mode < kModeCount; ++mode)
        {
            std::string &reason = reasons_[static_cast<std::size_t>(mode)];
            reason = kModes[mode].needs_network
                         ? (online_ ? "" : tr("Needs a connection to Lichess"))
                         : (pack ? "" : tr("The offline puzzle pack is missing"));
            signature += reason + "|";
        }
        // The grid is rebuilt only when a mode opens or closes, so the focus
        // stays where the player left it.
        if (signature == signature_)
            return;
        signature_ = signature;
        std::vector<ui::CardItem> items;
        for (int mode = 0; mode < kModeCount; ++mode)
        {
            ui::CardItem item;
            item.title = tr(kModes[mode].title);
            item.disabled = !reasons_[static_cast<std::size_t>(mode)].empty();
            item.tag = mode;
            items.push_back(std::move(item));
        }
        const int focus = modes_.focus();
        modes_.set_items(std::move(items));
        modes_.set_focus(std::clamp(focus, 0, kModeCount - 1));
    }

    int focused_mode() const
    {
        return std::clamp(modes_.focus(), 0, kModeCount - 1);
    }

    const ThemeEntry &focused_entry() const
    {
        return entries_[static_cast<std::size_t>(
            std::clamp(themes_.focus(), 0, static_cast<int>(entries_.size()) - 1))];
    }

    bool blocked(int mode) const
    {
        return !reasons_[static_cast<std::size_t>(mode)].empty();
    }

    // The number on a mode's tile; known says whether there is one.
    int tile_figure(int mode, bool *known) const
    {
        switch (mode)
        {
        case kDaily:
            *known = daily_rating_ > 0;
            return daily_rating_;
        case kTraining:
            *known = rating_ > 0;
            return rating_;
        case kStreak:
            *known = true;
            return records_.best_streak;
        default:
            *known = true;
            return records_.best_storm;
        }
    }

    // The large number at the foot of a mode's hero.
    int band_figure(int mode, bool *known) const
    {
        if (mode != kDaily)
            return tile_figure(mode, known);
        // Today's puzzle against the player's own rating.
        *known = daily_rating_ > 0 && rating_ > 0;
        return *known ? daily_rating_ - rating_ : 0;
    }

    // Where the segmented control sits: between its L2 and its R2.
    static Rect track()
    {
        const float right = app::kRight - ui::button_width(ui::Button::r2, kGlyph) - 14.0f;
        return {right - 4.0f * kSegment, app::kTop, 4.0f * kSegment, kHead};
    }

    static Rect segment(int index)
    {
        const Rect all = track();
        return {all.x + static_cast<float>(index) * kSegment, all.y, kSegment, all.h};
    }

    // Where a theme's card is inside the catalogue, whatever it has scrolled to.
    Rect card_in_catalogue(int index) const
    {
        const Rect origin = themes_.cell_rect(0);
        const Rect cell = themes_.cell_rect(index);
        return {cell.x - origin.x, cell.y - origin.y, cell.w, cell.h};
    }

    // ---- motion -----------------------------------------------------------------

    // 0..1 for the index-th part of the page since it was entered.
    float arrive(int index) const
    {
        return calm_ ? 1.0f : look::rise(since_, index, 0.06f, 0.5f);
    }

    // 0..1 for the index-th part of a chart since its mode took the hero.
    float grow(int index = 0, float step = 0.03f, float duration = 0.7f) const
    {
        return calm_ ? 1.0f : look::rise(std::max(detail_since_, 0.0f), index, step, duration);
    }

    void animate(float dt, bool focused)
    {
        const float quick = calm_ ? 60.0f : 18.0f;
        const bool on_modes = tabs_.active() == kTabModes;
        const int mode = focused_mode();
        const Color hue = puzzle_hue(kModes[mode].kind);

        // ---- the tiles, their ring and the hero ----
        for (int i = 0; i < kModeCount; ++i)
        {
            const std::size_t at = static_cast<std::size_t>(i);
            // The tile whose mode fills the hero stays lit while the rail has
            // the controller, only less.
            lit_[at].target = i == mode ? (focused && on_modes ? 1.0f : 0.45f) : 0.0f;
            bool known = false;
            figures_[at].target = static_cast<float>(tile_figure(i, &known));
        }
        ring_.target(modes_.cell_rect(mode));
        ring_ink_.target(hue);
        hero_ink_.target(hue);
        ring_alpha_.target = focused && on_modes ? 1.0f : 0.0f;
        bool known = false;
        band_figure_.target = static_cast<float>(band_figure(mode, &known));
        solved_.target = static_cast<float>(records_.solved);
        puzzles_.target = static_cast<float>(pack_size_);

        // ---- the catalogue: its ring, and the plate of the segmented control ----
        const PuzzleBand &band =
            kPuzzleBands[std::clamp(difficulty_.index(), 0, kPuzzleBandCount - 1)];
        plate_.target(segment(difficulty_.index()).inset(5.0f));
        band_low_.target = static_cast<float>(band.min);
        band_high_.target = static_cast<float>(band.max);
        if (!entries_.empty())
        {
            theme_ring_.target(card_in_catalogue(themes_.focus()));
            theme_ink_.target(puzzle_hue(focused_entry().family));
            place_.target = static_cast<float>(themes_.focus() + 1);
        }
        theme_alpha_.target = focused && !on_modes ? 1.0f : 0.0f;

        if (!placed_)
        {
            // The first frame: everything is simply where it belongs.
            placed_ = true;
            for (tween::Spring &lit : lit_)
                lit.snap(lit.target);
            ring_.snap(modes_.cell_rect(mode));
            ring_ink_.snap(hue);
            hero_ink_.snap(hue);
            ring_alpha_.snap(ring_alpha_.target);
            plate_.snap(segment(difficulty_.index()).inset(5.0f));
            band_low_.snap(band_low_.target);
            band_high_.snap(band_high_.target);
            place_.snap(place_.target);
            theme_alpha_.snap(theme_alpha_.target);
            detail_.snap(1.0f);
            if (!entries_.empty())
            {
                theme_ring_.snap(card_in_catalogue(themes_.focus()));
                theme_ink_.snap(puzzle_hue(focused_entry().family));
            }
        }

        for (tween::Spring &lit : lit_)
            lit.update(dt, quick);
        ring_.update(dt, quick);
        ring_alpha_.update(dt, quick);
        ring_ink_.update(dt, calm_ ? 60.0f : 10.0f);
        hero_ink_.update(dt, calm_ ? 60.0f : 8.0f);
        press_.update(dt, 10.0f);
        refusal_.update(dt, 9.0f);
        detail_.target = 1.0f;
        detail_.update(dt, calm_ ? 60.0f : 8.0f);
        if (detail_.value > 0.995f)
            detail_before_ = -1;
        // The numbers start counting once their part of the page has arrived.
        const float counting = calm_ ? 60.0f : 7.0f;
        if (since_ > 0.4f || calm_)
        {
            for (tween::Spring &figure : figures_)
                figure.update(dt, counting);
            solved_.update(dt, counting);
            puzzles_.update(dt, counting);
        }
        if (detail_since_ > 0.12f || calm_)
            band_figure_.update(dt, counting);

        theme_ring_.update(dt, quick);
        theme_alpha_.update(dt, quick);
        theme_ink_.update(dt, calm_ ? 60.0f : 10.0f);
        theme_press_.update(dt, 10.0f);
        theme_refusal_.update(dt, 9.0f);
        plate_.update(dt, calm_ ? 60.0f : 16.0f);
        plate_press_.update(dt, 8.0f);
        plate_refusal_.update(dt, 9.0f);
        band_low_.update(dt, calm_ ? 60.0f : 10.0f);
        band_high_.update(dt, calm_ ? 60.0f : 10.0f);
        place_.update(dt, calm_ ? 60.0f : 14.0f);
    }

    // ---- drawing: the ways to play ----------------------------------------------

    void draw_modes(app::Context &ctx, gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const int mode = focused_mode();

        // ---- what the console holds, beside the tabs ----
        {
            const float in = arrive(0);
            list.push_opacity(in);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in, 14.0f));
            const float baseline = kHeadLine + 10.0f;
            float x = app::kRight;
            x -= look::figure(list, fonts, number(solved_.value), x, baseline, 28.0f, look::kInk,
                              Align::right);
            // The two labels share what the tabs leave of the row.
            const float left = kTabs.x + tabs_width_ + 24.0f;
            x -= 12.0f + kicker_fit(list, fonts, tr("Solved here"), x - 12.0f, baseline - 2.0f,
                                    look::kInk.with_alpha(look::kFaint), 13.0f,
                                    (x - 12.0f - left) * 0.4f, Align::right);
            if (pack_size_ > 0)
            {
                x -= 40.0f;
                x -= look::figure(list, fonts,
                                  grouped(static_cast<long long>(std::lround(puzzles_.value))), x,
                                  baseline, 28.0f, look::kInk, Align::right);
                kicker_fit(list, fonts, tr("Puzzles on this console"), x - 12.0f, baseline - 2.0f,
                           look::kInk.with_alpha(look::kFaint), 13.0f, x - 12.0f - left,
                           Align::right);
            }
            list.pop_transform();
            list.pop_opacity();
        }

        // ---- the four tiles: the light under them first, then the tiles ----
        for (int i = 0; i < kModeCount; ++i)
        {
            const float in = arrive(1 + i);
            list.push_opacity(in);
            tile_transform(list, i, in);
            look::lift(list, modes_.cell_rect(i), lit_[static_cast<std::size_t>(i)].value,
                       puzzle_hue(kModes[i].kind), ctx.time, calm_);
            list.pop_transform();
            list.pop_opacity();
        }
        for (int i = 0; i < kModeCount; ++i)
        {
            const float in = arrive(1 + i);
            list.push_opacity(in);
            tile_transform(list, i, in);
            draw_tile(ctx, list, i);
            list.pop_transform();
            list.pop_opacity();
        }
        if (ring_alpha_.value > 0.01f)
        {
            Rect ring = ring_.value();
            ring.x += ui::shake(refusal_.value, age_, 9.0f);
            ring.y -= calm_ ? 0.0f : 4.0f;
            look::ring(list, ring.inset(2.0f * press_.value), ring_ink_.value(),
                       ring_alpha_.value * arrive(1 + mode));
        }

        // ---- the focused one in full ----
        {
            const float in = arrive(5);
            list.push_opacity(in);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in));
            look::panel(list, kHero, 0.35f, hero_ink_.value());
            list.pop_transform();
            list.pop_opacity();
        }
        const float t = tween::clamp01(detail_.value);
        if (detail_before_ >= 0 && !calm_)
        {
            // The mode that had the focus leaves toward where the focus went.
            const float gone = tween::smoothstep(t * 2.4f);
            if (gone < 0.996f)
                draw_hero(ctx, list, detail_before_, 1.0f - gone, -detail_side_ * 26.0f * gone,
                          false);
            const float here = tween::smoothstep(t * 1.8f - 0.5f);
            draw_hero(ctx, list, mode, here, detail_side_ * 26.0f * (1.0f - here), true);
        }
        else
        {
            draw_hero(ctx, list, mode, 1.0f, 0.0f, true);
        }
    }

    // A tile's place: it settles as it arrives, floats while it has the
    // focus, gives a little under a press and shakes off a refusal.
    void tile_transform(gfx::DrawList &list, int index, float in) const
    {
        const Rect r = modes_.cell_rect(index);
        const bool focus = index == focused_mode();
        const float lit = lit_[static_cast<std::size_t>(index)].value;
        list.push_transform(1.0f - (focus ? 0.03f * press_.value : 0.0f), r.cx(), r.cy(),
                            focus ? ui::shake(refusal_.value, age_, 9.0f) : 0.0f,
                            look::settle(in) - (calm_ ? 0.0f : 4.0f * lit));
    }

    void draw_tile(app::Context &ctx, gfx::DrawList &list, int index) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const ModeText &mode = kModes[index];
        const std::size_t at = static_cast<std::size_t>(index);
        const Rect r = modes_.cell_rect(index);
        const float lit = lit_[at].value;
        const Color hue = puzzle_hue(mode.kind);
        const bool closed = blocked(index);
        const float dim = closed ? 0.5f : 1.0f;

        look::panel(list, r, lit, hue);
        const Rect badge{r.x + kTilePad, r.y + kTilePad, 56.0f, 56.0f};
        const float tx = badge.x + badge.w + 16.0f;
        const float room = r.x + r.w - kTilePad - tx;
        list.push_opacity(dim);
        draw_puzzle_badge(list, mode.kind, badge, lit);
        list.pop_opacity();
        // Why a mode cannot be played takes the place of its kicker.
        if (closed)
            kicker_fit(list, fonts,
                       mode.needs_network ? tr("Needs a connection") : tr("No puzzle pack"), tx,
                       r.y + 44.0f, ctx.theme().warning, 13.0f, room);
        else
            kicker_fit(list, fonts, tr(mode.kicker), tx, r.y + 44.0f, hue, 13.0f, room);
        list.push_opacity(dim);
        ui::text_fit(list, fonts.semibold, tr(mode.title), tx, r.y + 72.0f, 24.0f, room,
                     look::kInk);
        look::rule(list, r.x + kTilePad, r.y + 100.0f, r.w - 2.0f * kTilePad, 0.08f);

        // ---- its figure, counting; the label stops short of the drawing
        // beside it (the change of the rating, the top of the climb, the ring) ----
        kicker_fit(list, fonts, tr(mode.figure), r.x + kTilePad, r.y + 130.0f,
                   look::kInk.with_alpha(look::kFaint), 13.0f, r.w - 2.0f * kTilePad - 68.0f);
        bool known = false;
        tile_figure(index, &known);
        if (known)
            look::figure(list, fonts, number(figures_[at].value), r.x + kTilePad - 2.0f,
                         r.y + 174.0f, 40.0f, look::kInk);
        else
            look::figure(list, fonts, "\xE2\x80\x94", r.x + kTilePad - 2.0f, r.y + 174.0f, 40.0f,
                         look::kInk.with_alpha(look::kFaint));

        // ---- and a small drawing of it, where a line would be ----
        const Rect art{r.x + r.w - kTilePad - 128.0f, r.y + 122.0f, 128.0f, 52.0f};
        const float drawn = calm_ ? 1.0f : look::rise(since_, 5 + index, 0.08f, 0.9f);
        switch (index)
        {
        case kDaily:
        {
            // Whose move today's puzzle is: that side's king on a lit disc.
            const float cx = art.x + art.w - 28.0f;
            list.circle(cx, art.cy(), 28.0f, hue.with_alpha(0.14f * drawn));
            list.ring(cx, art.cy(), 28.0f, 2.0f, hue.with_alpha(0.7f * drawn));
            if (daily_rating_ > 0)
                ctx.pieces->draw(
                    list,
                    {daily_white_ ? chess::Color::white : chess::Color::black, chess::Role::king},
                    {cx - 21.0f, art.cy() - 21.0f, 42.0f, 42.0f}, {1.0f, 1.0f, 1.0f, drawn});
            break;
        }
        case kTraining:
            look::delta(list, fonts, rating_change_, r.x + r.w - kTilePad, r.y + 131.0f, 19.0f,
                        Align::right);
            look::sparkline(list, {art.x, art.y + 12.0f, art.w, art.h - 12.0f}, rating_history_,
                            hue, drawn);
            break;
        case kStreak:
            draw_climb(list, art, std::min(records_.best_streak, 8), 8, hue, drawn);
            break;
        default:
        {
            // Three minutes: a full ring and the time it starts from.
            const float cx = art.x + art.w - 26.0f;
            look::gauge(list, cx, art.cy(), 26.0f, 5.0f, 1.0f, hue, drawn);
            look::ticker(list, fonts, "3:00", cx - 40.0f, art.cy() + 8.0f, 22.0f,
                         look::kInk.with_alpha(look::kMuted), Align::right);
            break;
        }
        }
        list.pop_opacity();
    }

    // Rising steps: the first `filled` in the colour, the rest still ahead.
    void draw_climb(gfx::DrawList &list, const Rect &r, int filled, int count, Color hue,
                    float drawn) const
    {
        const float gap = std::min(6.0f, r.w / static_cast<float>(count) * 0.3f);
        const float width = (r.w - gap * static_cast<float>(count - 1)) / static_cast<float>(count);
        for (int i = 0; i < count; ++i)
        {
            const float rise = calm_ ? 1.0f
                                     : tween::clamp01(drawn * 1.6f - 0.6f * static_cast<float>(i) /
                                                                         static_cast<float>(count));
            const float share =
                0.16f + 0.84f * static_cast<float>(i + 1) / static_cast<float>(count);
            const float h = std::max(r.h * share * tween::cubic_out(rise), 2.0f);
            const Rect step{r.x + static_cast<float>(i) * (width + gap), r.y + r.h - h, width, h};
            const float radius = std::min(width * 0.5f, 5.0f);
            if (i < filled)
            {
                const bool top = i == filled - 1;
                if (top)
                    list.glow(step, radius, 10.0f, hue.with_alpha(0.4f * drawn));
                list.rounded_rect(step, radius, hue.with_alpha(top ? 1.0f : 0.5f));
            }
            else
            {
                list.rounded_rect(step, radius, look::kInk.with_alpha(0.09f));
            }
        }
    }

    // ---- drawing: the hero -------------------------------------------------------

    Standing standing(int mode, Color warning) const
    {
        if (blocked(mode))
            return {reasons_[static_cast<std::size_t>(mode)], warning, false};
        switch (mode)
        {
        case kDaily:
            return {tr("Shared by every player")};
        case kTraining:
            if (signed_in_)
                return {tr("Results count on Lichess")};
            return {tr("Sign in to earn a rating"), look::kInk.with_alpha(look::kFaint), false};
        default:
            return {tr("No connection needed")};
        }
    }

    void facts(int mode, std::array<Fact, 4> *out) const
    {
        std::array<Fact, 4> &list = *out;
        list = {};
        switch (mode)
        {
        case kDaily:
            if (daily_rating_ <= 0)
                break;
            // The side, named on its own under "To play".
            list[0] = {tr("To play"),
                       daily_white_ ? trc("to play", "White") : trc("to play", "Black")};
            list[1] = {tr("Rating"), std::to_string(daily_rating_)};
            if (daily_plays_ > 0)
                list[2] = {tr("Played"), grouped(daily_plays_)};
            break;
        case kTraining:
            list[0] = {tr("Results"), signed_in_ ? trc("puzzle results", "Rated")
                                                 : trc("puzzle results", "Not rated")};
            if (rating_games_ > 0)
                list[1] = {tr("Played on Lichess"), grouped(rating_games_)};
            if (rating_ > 0)
                list[2] = {tr("Rating"),
                           rating_provisional_ ? tr("Provisional") : tr("Established")};
            break;
        case kStreak:
            list[0] = {tr("Starts at"), std::to_string(streak_rating(0))};
            list[1] = {tr("Each solve"), "+" + std::to_string(streak_rating(1) - streak_rating(0))};
            list[2] = {tr("Skips"), "1"};
            list[3] = {tr("Mistakes allowed"), "0"};
            break;
        default:
            list[0] = {tr("Clock"), "3:00"};
            list[1] = {tr("A mistake"),
                       fill(tr("\xE2\x80\x93{0} s"), {std::to_string(kStormPenalty)})};
            list[2] = {tr("Starts at"), std::to_string(storm_rating(0))};
            list[3] = {tr("Each solve"), "+" + std::to_string(storm_rating(1) - storm_rating(0))};
            break;
        }
    }

    // One mode in the hero. alpha and shift: how far it has arrived (the mode
    // before it leaves the same way). live: it is the one that has the focus,
    // so its numbers count and its charts grow.
    void draw_hero(app::Context &ctx, gfx::DrawList &list, int mode, float alpha, float shift,
                   bool live) const
    {
        if (alpha <= 0.004f)
            return;
        const ui::Fonts &fonts = *ctx.fonts;
        const ModeText &text = kModes[mode];
        const Color hue = puzzle_hue(text.kind);
        const float room = kHeroRight - kHeroText;
        const auto part = [&](int index, float distance, const auto &paint)
        {
            const float in = arrive(index);
            list.push_opacity(in * alpha);
            list.push_transform(1.0f, 0.0f, 0.0f, shift, look::settle(in, distance));
            paint();
            list.pop_transform();
            list.pop_opacity();
        };

        // ---- its board, standing off the panel ----
        part(5, 22.0f,
             [&]()
             {
                 const Preview &preview = previews_[static_cast<std::size_t>(mode)];
                 const Rect squares{kHero.x + kHeroPad, kHero.y + kHeroPad, kHeroBoard, kHeroBoard};
                 look::frame_board(list, squares, hue, blocked(mode) ? 0.25f : 0.75f);
                 board::draw_mini_board(list, *ctx.pieces, ctx.board_theme(), squares,
                                        preview.position, preview.orientation, preview.last_move,
                                        10.0f);
             });

        // ---- its sign and kicker, and whether it can be played now ----
        part(6, 16.0f,
             [&]()
             {
                 const float baseline = kHero.y + 62.0f;
                 draw_puzzle_sign(list, text.kind, {kHeroText, baseline - 21.0f, 26.0f, 26.0f},
                                  hue);
                 const Standing state = standing(mode, ctx.theme().warning);
                 const float width =
                     ui::text_fit(list, fonts.regular, state.words, kHeroRight, baseline, 20.0f,
                                  room * 0.55f, look::kInk.with_alpha(look::kMuted), Align::right);
                 const float cx = kHeroRight - width - 16.0f;
                 if (state.live)
                     look::live_dot(list, cx, baseline - 7.0f, 5.0f, state.dot, ctx.time, calm_);
                 else
                     list.circle(cx, baseline - 7.0f, 5.0f, state.dot);
                 // The kicker has the line up to the dot of the standing.
                 kicker_fit(list, fonts, tr(text.kicker), kHeroText + 38.0f, baseline, hue, 16.0f,
                            cx - 29.0f - (kHeroText + 38.0f));
             });
        part(7, 16.0f,
             [&]()
             {
                 ui::text_fit(list, fonts.display, tr(text.title), kHeroText - 3.0f,
                              kHero.y + 134.0f, 64.0f, room, look::kInk);
             });

        // ---- its facts, as figures ----
        part(8, 16.0f,
             [&]()
             {
                 std::array<Fact, 4> all;
                 facts(mode, &all);
                 // Side by side, each as wide as its label or its figure. When
                 // longer labels make the row wider than the hero, the gaps
                 // close first and then each fact keeps to its share.
                 constexpr float kNarrowest = 96.0f;
                 const auto wide = [&](const Fact &fact)
                 {
                     return std::max({fonts.semibold.measure(fact.value, 30.0f),
                                      fonts.semibold.measure(ui::upper(fact.label), 14.0f, 3.0f),
                                      kNarrowest});
                 };
                 float natural = 0.0f;
                 int count = 0;
                 for (const Fact &fact : all)
                 {
                     if (fact.value.empty())
                         continue;
                     natural += wide(fact);
                     ++count;
                 }
                 float gap = 44.0f;
                 float share = 1.0f;
                 const float gaps = static_cast<float>(std::max(count - 1, 0));
                 if (count > 0 && natural + gap * gaps > room)
                 {
                     gap = std::max(20.0f, (room - natural) / std::max(gaps, 1.0f));
                     share = std::min(1.0f, (room - gap * gaps) / natural);
                 }
                 float fx = kHeroText;
                 for (const Fact &fact : all)
                 {
                     if (fact.value.empty())
                         continue;
                     const float column = share * wide(fact);
                     const float label =
                         kicker_fit(list, fonts, fact.label, fx, kHero.y + 190.0f,
                                    look::kInk.with_alpha(look::kFaint), 14.0f, column);
                     const float width = ui::text_fit(list, fonts.semibold, fact.value, fx,
                                                      kHero.y + 228.0f, 30.0f, column, look::kInk);
                     fx += std::max({width, label, kNarrowest * share}) + gap;
                 }
                 if (count == 0)
                     ui::text_fit(list, fonts.regular,
                                  tr("Today's puzzle shows here once it is loaded"), kHeroText,
                                  kHero.y + 218.0f, 22.0f, room,
                                  look::kInk.with_alpha(look::kFaint));
                 look::rule(list, kHeroText, kHero.y + 254.0f, room);
             });
        part(9, 16.0f,
             [&]()
             {
                 // Two lines in English; a longer translation takes three
                 // smaller ones in the same room.
                 const char *words = tr(text.description);
                 const Color ink = look::kInk.with_alpha(look::kMuted);
                 if (fonts.regular.font->wrap(words, 24.0f, room).size() <= 2)
                     ui::paragraph(list, fonts.regular, words, kHeroText, kHero.y + 296.0f, 24.0f,
                                   room, 34.0f, ink, 2);
                 else
                     ui::paragraph(list, fonts.regular, words, kHeroText, kHero.y + 286.0f, 20.0f,
                                   room, 28.0f, ink, 3);
                 look::rule(list, kHeroText, kBand - 10.0f, room);
             });

        // ---- its figure and its chart ----
        part(10, 16.0f, [&]() { draw_band(ctx, list, mode, hue, live); });
    }

    // The foot of the hero: a large figure, and the chart that belongs to the mode.
    void draw_band(app::Context &ctx, gfx::DrawList &list, int mode, Color hue, bool live) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Color faint = look::kInk.with_alpha(look::kFaint);
        const float drawn = live ? grow() : 1.0f;
        bool known = false;
        const int value = band_figure(mode, &known);
        const float shown = live ? band_figure_.value : static_cast<float>(value);

        const char *label = tr("Best streak");
        const char *caption = tr("The climb \xC2\xB7 +60 rating a puzzle");
        std::string note;
        std::string figure = number(shown);
        switch (mode)
        {
        case kDaily:
            label = tr("Against your rating");
            caption = tr("Today's puzzle and you");
            if (known)
            {
                if (value > 0)
                    figure = "+" + figure;
                note = value > 25    ? tr("Harder than your puzzle rating")
                       : value < -25 ? tr("Easier than your puzzle rating")
                                     : tr("Right at your puzzle rating");
            }
            else
            {
                note = daily_rating_ <= 0 ? tr("Known once it is loaded")
                       : signed_in_       ? tr("Your rating is not known yet")
                                          : tr("Sign in to compare");
            }
            break;
        case kTraining:
            label = tr("Your puzzle rating");
            caption = tr("Your rating over time");
            note = known ? (rating_provisional_ ? tr("Provisional rating") : tr("Lichess rating"))
                   : signed_in_ ? tr("No rating yet")
                                : tr("Sign in to earn a rating");
            break;
        case kStreak:
            note = value > 0 ? fill(tr("Reached puzzles rated {0}"),
                                    {std::to_string(streak_rating(value))})
                             : tr("No run yet on this console");
            break;
        default:
            label = tr("Best score");
            caption = tr("Puzzles in a row add seconds");
            note = value > 0 ? tr("Solved in three minutes") : tr("No run yet on this console");
            break;
        }

        // The figure's column ends where the chart's begins.
        constexpr float kColumn = 284.0f;
        kicker_fit(list, fonts, label, kHeroText, kBand + 22.0f, hue, 14.0f, kColumn);
        float width = 0.0f;
        if (known)
            width = look::figure(list, fonts, figure, kHeroText - 2.0f, kBand + 84.0f, 52.0f,
                                 look::kInk);
        else
            width = look::figure(list, fonts, "\xE2\x80\x94", kHeroText - 2.0f, kBand + 84.0f,
                                 52.0f, faint);
        if (mode == kTraining && known)
            look::delta(list, fonts, rating_change_, kHeroText + width + 16.0f, kBand + 82.0f,
                        22.0f);
        // One line under the figure; a translation too long for it, even a
        // little smaller, takes two.
        constexpr float kLeast = 0.86f;
        if (fonts.regular.measure(note, 19.0f) * kLeast <= kColumn)
            ui::text_fit(list, fonts.regular, note, kHeroText, kBand + 116.0f, 19.0f, kColumn,
                         look::kInk.with_alpha(look::kMuted), Align::left, 0.0f, kLeast);
        else
            ui::paragraph(list, fonts.regular, note, kHeroText, kBand + 108.0f, 17.0f, kColumn,
                          20.0f, look::kInk.with_alpha(look::kMuted), 2);

        kicker_fit(list, fonts, caption, kChart.x, kBand + 22.0f, faint, 13.0f, kChart.w);
        switch (mode)
        {
        case kDaily:
            draw_scale(list, fonts, hue, drawn);
            break;
        case kTraining:
            draw_line(list, fonts, hue, drawn);
            break;
        case kStreak:
        {
            const int steps = std::clamp(records_.best_streak + 3, 16, 32);
            draw_climb(list, kChart, std::min(records_.best_streak, steps), steps, hue, drawn);
            break;
        }
        default:
            draw_combos(list, fonts, hue, drawn);
            break;
        }
    }

    // Today's puzzle and the player on one scale of ratings.
    void draw_scale(gfx::DrawList &list, const ui::Fonts &fonts, Color hue, float drawn) const
    {
        const float y = kChart.y + kChart.h * 0.5f + 2.0f;
        const Rect line{kChart.x + 62.0f, y - 4.0f, kChart.w - 124.0f, 8.0f};
        list.rounded_rect(line, 4.0f, look::kInk.with_alpha(0.1f));
        if (daily_rating_ <= 0)
            return; // an empty scale: the puzzle is not here yet
        const bool you = rating_ > 0;
        int low = daily_rating_ - 300;
        int high = daily_rating_ + 300;
        if (you)
        {
            low = (std::min(daily_rating_, rating_) - 150) / 100 * 100;
            high = (std::max(daily_rating_, rating_) + 249) / 100 * 100;
        }
        const auto at = [&](int rating) {
            return line.x +
                   line.w * static_cast<float>(rating - low) / static_cast<float>(high - low);
        };
        const Color faint = look::kInk.with_alpha(look::kFaint);
        look::ticker(list, fonts, std::to_string(low), line.x - 12.0f, y + 7.0f, 19.0f, faint,
                     Align::right);
        look::ticker(list, fonts, std::to_string(high), line.x + line.w + 12.0f, y + 7.0f, 19.0f,
                     faint);
        const float today = at(daily_rating_);
        if (you)
        {
            // The stretch between the two grows from the player to the puzzle.
            const float mine = at(rating_);
            const float end = tween::lerp(mine, today, drawn);
            list.rounded_rect({std::min(mine, end), y - 4.0f, std::fabs(end - mine), 8.0f}, 4.0f,
                              hue.with_alpha(0.55f));
            list.circle(mine, y, 10.0f, look::kInk);
            list.circle(mine, y, 5.0f, look::kNight);
            pair(list, fonts, tr("You"), std::to_string(rating_), mine, y - 20.0f, look::kInk,
                 kChart.x, kChart.x + kChart.w);
        }
        list.glow({today - 9.0f, y - 9.0f, 18.0f, 18.0f}, 9.0f, 14.0f,
                  hue.with_alpha(0.5f * drawn));
        list.rotated_rect({today - 9.0f, y - 9.0f, 18.0f, 18.0f}, 3.0f, 0.7854f, hue);
        pair(list, fonts, tr("Today"), std::to_string(daily_rating_), today, y + 38.0f, hue,
             kChart.x, kChart.x + kChart.w);
    }

    // The player's puzzle rating over time, between its lowest and its highest.
    void draw_line(gfx::DrawList &list, const ui::Fonts &fonts, Color hue, float drawn) const
    {
        const Rect line{kChart.x + 62.0f, kChart.y, kChart.w - 62.0f, kChart.h};
        look::sparkline(list, line, rating_history_, hue, drawn);
        const Color faint = look::kInk.with_alpha(look::kFaint);
        if (rating_history_.size() < 2)
        {
            ui::text_fit(list, fonts.regular,
                         signed_in_ ? tr("Its line shows here once your results are known")
                                    : tr("Its line shows here when you are signed in"),
                         line.x, line.y + line.h - 18.0f, 19.0f, line.w, faint);
            return;
        }
        const auto [low, high] =
            std::minmax_element(rating_history_.begin(), rating_history_.end());
        look::ticker(list, fonts, number(*high), line.x - 14.0f, line.y + 19.0f, 19.0f, faint,
                     Align::right);
        look::ticker(list, fonts, number(*low), line.x - 14.0f, line.y + line.h, 19.0f, faint,
                     Align::right);
    }

    // Storm's combos: how many in a row add how many seconds.
    void draw_combos(gfx::DrawList &list, const ui::Fonts &fonts, Color hue, float drawn) const
    {
        const float y = kChart.y + 42.0f;
        const Rect line{kChart.x + 8.0f, y - 3.0f, kChart.w - 28.0f, 6.0f};
        const float last = static_cast<float>(kStormComboSteps[kStormComboCount - 1]);
        list.rounded_rect(line, 3.0f, look::kInk.with_alpha(0.1f));
        list.rounded_rect({line.x, line.y, std::max(line.w * drawn, 6.0f), line.h}, 3.0f,
                          hue.with_alpha(0.55f));
        for (int i = 0; i < kStormComboCount; ++i)
        {
            const float share = static_cast<float>(kStormComboSteps[i]) / last;
            const float x = line.x + line.w * share;
            // A milestone pops as the line reaches it.
            const float reached =
                calm_ ? 1.0f : tween::back_out(tween::clamp01((drawn - share * 0.8f) * 5.0f));
            if (reached <= 0.01f)
                continue;
            list.push_opacity(tween::clamp01(reached));
            list.circle(x, y, 11.0f * reached, look::kNight);
            list.ring(x, y, 11.0f * reached, 3.0f, hue);
            // The seconds a combo adds, over the combo that adds them. The last
            // one stands at the chart's end: a longer text keeps to the hero.
            const std::string bonus = fill(tr("+{0} s"), {std::to_string(kStormComboBonus[i])});
            const float half = fonts.semibold.measure(bonus, 20.0f) * 0.5f;
            look::figure(list, fonts, bonus, std::min(x, kHeroRight + 8.0f - half), y - 22.0f,
                         20.0f, look::kInk, Align::center);
            look::ticker(list, fonts, std::to_string(kStormComboSteps[i]), x, y + 38.0f, 19.0f,
                         look::kInk.with_alpha(look::kMuted), Align::center);
            list.pop_opacity();
        }
    }

    // ---- drawing: the themes ------------------------------------------------------

    void draw_themes(app::Context &ctx, ui::Canvas &canvas) const
    {
        gfx::DrawList &list = canvas.list;
        const ui::Fonts &fonts = *ctx.fonts;
        if (entries_.empty())
        {
            missing_.draw(canvas);
            return;
        }
        const Color accent = look::accent(look::Section::puzzles);
        const Color faint = look::kInk.with_alpha(look::kFaint);

        // ---- beside the tabs: what the choice means, and where the focus is.
        // The two columns end before the L2 of the segmented control; the
        // second one moves right when the first one's label is longer than
        // the English one ----
        const Rect all = track();
        const float left = ui::button_width(ui::Button::l2, kGlyph);
        const float x = kTabs.x + tabs_width_ + 36.0f;
        const float end = all.x - 14.0f - left - 24.0f;
        const float rated = fonts.semibold.measure(ui::upper(tr("Puzzles rated")), 13.0f, 3.0f);
        const float second = std::min(x + std::max(196.0f, rated + 28.0f), end - 140.0f);
        kicker_fit(list, fonts, tr("Puzzles rated"), x, kHeadLine - 8.0f, faint, 13.0f,
                   second - 24.0f - x);
        look::figure(list, fonts,
                     number(band_low_.value) + " \xE2\x80\x93 " + number(band_high_.value), x,
                     kHeadLine + 22.0f, 24.0f, look::kInk);
        kicker_fit(list, fonts, tr("Theme"), second, kHeadLine - 8.0f, faint, 13.0f, end - second);
        // Which theme has the focus: its place and how many there are.
        ui::text_fit(
            list, fonts.semibold,
            fill(tr("{0} of {1}"), {number(place_.value), std::to_string(entries_.size())}), second,
            kHeadLine + 22.0f, 24.0f, end - second, look::kInk);

        // ---- how hard: four segments and a plate that glides between them ----
        const ui::GlyphStyle glyphs = ui::GlyphStyle::dark();
        const float nudge = ui::shake(plate_refusal_.value, age_, 7.0f);
        // A trigger that leads nowhere fades; the one just used lights up.
        const int chosen = difficulty_.index();
        const auto trigger = [&](ui::Button button, float x, bool leads, float side)
        {
            const float used =
                plate_side_ * side > 0.0f ? tween::clamp01(plate_press_.value) : 0.0f;
            list.push_opacity(leads ? 0.78f + 0.22f * used : 0.32f);
            ui::draw_button(list, fonts, glyphs, button, x, kHeadLine, kGlyph);
            list.pop_opacity();
        };
        trigger(ui::Button::l2, all.x - 14.0f - left, chosen > 0, -1.0f);
        trigger(ui::Button::r2, all.x + all.w + 14.0f, chosen < kPuzzleBandCount - 1, 1.0f);
        look::panel(list, all, 0.0f, accent, all.h * 0.5f);
        Rect plate = plate_.value();
        plate.x += nudge;
        plate = plate.inset(2.0f * plate_press_.value);
        list.glow(plate, plate.h * 0.5f, 14.0f, accent.with_alpha(0.3f));
        list.gradient_rect(plate, plate.h * 0.5f, gfx::mix(accent, look::kInk, 0.15f),
                           gfx::mix(accent, look::kNight, 0.22f));
        for (int i = 0; i < kPuzzleBandCount; ++i)
        {
            const Rect cell = segment(i);
            // A label turns dark as the plate comes under it.
            const float under =
                tween::clamp01(1.0f - std::fabs(plate.cx() - cell.cx()) / (kSegment * 0.6f));
            ui::text_fit(list, fonts.semibold, puzzle_band_label(kPuzzleBands[i]), cell.cx(),
                         cell.cy() + 22.0f * 0.35f, 22.0f, kSegment - 24.0f,
                         gfx::mix(look::kInk.with_alpha(look::kMuted), look::kNight, under),
                         Align::center);
        }

        // ---- the catalogue, and the one ring that glides over it ----
        themes_.draw(canvas);
        if (theme_alpha_.value > 0.01f)
        {
            const Rect origin = themes_.cell_rect(0);
            const Rect area = themes_.bounds();
            Rect ring = theme_ring_.value();
            ring.x += origin.x;
            ring.y += origin.y;
            const float shake = ui::shake(theme_refusal_.value, canvas.time, 9.0f);
            if (theme_refused_ == Direction::up || theme_refused_ == Direction::down)
                ring.y += shake;
            else
                ring.x += shake;
            list.push_clip({area.x - kBleed, area.y, area.w + 2.0f * kBleed, area.h});
            look::ring(list, ring.inset(2.0f * theme_press_.value), theme_ink_.value(),
                       theme_alpha_.value);
            list.pop_clip();
        }
    }

    // A theme's name on its card, from the card's top. The English names keep
    // the one line they were approved in (the two longest end in an ellipsis).
    // A translation that is too long for it shrinks a little, and takes two
    // smaller lines when that is not enough.
    void draw_name(gfx::DrawList &list, const ui::Fonts &fonts, const std::string &name, float x,
                   float top, float room) const
    {
        const ui::FontRef &face = fonts.semibold;
        const float width = face.measure(name, 24.0f);
        if (english_ || width <= room)
        {
            ui::text(list, face, face.font->fit(name, 24.0f, room), x, top + 48.0f, 24.0f,
                     look::kInk);
            return;
        }
        constexpr float kLeast = 0.86f;
        std::vector<std::string> lines;
        if (width * kLeast > room)
            lines = face.font->wrap(name, 19.0f, room);
        if (lines.size() < 2)
        {
            // Nearly fits, or one long word: one line, as small as it has to be.
            ui::text_fit(list, face, name, x, top + 48.0f, 24.0f, room, look::kInk, Align::left,
                         0.0f, lines.empty() ? kLeast : 0.78f);
            return;
        }
        // Whatever a second line cannot hold stays on it, shrunk.
        for (std::size_t i = 2; i < lines.size(); ++i)
            lines[1] += " " + lines[i];
        ui::text_fit(list, face, lines[0], x, top + 33.0f, 19.0f, room, look::kInk);
        ui::text_fit(list, face, lines[1], x, top + 54.0f, 19.0f, room, look::kInk);
    }

    // One theme of the catalogue: its sign on a tile, its name, how many
    // puzzles it has and that number as a level.
    void draw_card(app::Context &ctx, ui::Canvas &canvas, const Rect &r, int index,
                   float focus) const
    {
        if (index < 0 || index >= static_cast<int>(entries_.size()))
            return;
        gfx::DrawList &list = canvas.list;
        const ui::Fonts &fonts = canvas.fonts;
        const ThemeEntry &entry = entries_[static_cast<std::size_t>(index)];
        const Color hue = puzzle_hue(entry.family);
        look::lift(list, r, focus, hue, canvas.time, calm_);
        look::panel(list, r, focus, hue);

        const Rect tile{r.x + 20.0f, r.y + 24.0f, 68.0f, 68.0f};
        const Color ink = gfx::mix(hue, look::kNight, focus);
        if (focus > 0.01f)
            list.glow(tile, 18.0f, 12.0f, hue.with_alpha(0.35f * focus));
        list.rounded_rect(tile, 18.0f, gfx::mix(hue.with_alpha(0.14f), hue, focus));
        if (entry.piece)
            ctx.pieces->draw(list, {chess::Color::white, entry.role}, tile.inset(7.0f));
        else if (entry.mark.empty())
            draw_puzzle_sign(list, PuzzleKind::themes, tile.inset(17.0f), ink);
        else
            ui::text(list, fonts.display, entry.mark, tile.cx(), tile.cy() + 30.0f * 0.36f, 30.0f,
                     ink, Align::center);

        const float tx = tile.x + tile.w + 16.0f;
        const float room = r.x + r.w - 20.0f - tx;
        draw_name(list, fonts, entry.label, tx, r.y, room);
        ui::text_fit(list, fonts.regular, entry.count_text, tx, r.y + 77.0f, 19.0f, room,
                     gfx::mix(look::kInk.with_alpha(look::kMuted), hue, focus));
        const float drawn = calm_ ? 1.0f : look::rise(themes_since_, 0, 0.0f, 0.8f);
        look::level(list, {tx, r.y + 90.0f, room, 6.0f}, entry.share * drawn,
                    hue.with_alpha(0.55f + 0.45f * focus));
    }

    ui::TabBar tabs_;
    ui::GridView modes_;          // the four tiles: their focus, sounds and refusals
    ui::GridView themes_;         // the catalogue
    ui::ChoicePicker difficulty_; // which band, with its sounds (drawn here as segments)
    ui::EmptyState missing_;

    // ---- motion ----
    bool calm_ = false;   // reduced motion
    bool placed_ = false; // the rings and the plate have been put somewhere
    float since_ = 0.0f;  // seconds since the page was entered: parts arrive by it
    float age_ = 0.0f;    // seconds the page has been updated: shakes run on it
    std::array<tween::Spring, kModeCount> lit_;     // 0..1: a tile has the focus
    std::array<tween::Spring, kModeCount> figures_; // the tiles' numbers, counting
    ui::SpringRect ring_;                           // the one ring over the tiles
    ui::SpringColor ring_ink_;                      // ... in the focused tile's hue
    tween::Spring ring_alpha_;
    ui::Pulse press_;           // Cross on a tile
    ui::Pulse refusal_;         // an edge, or a mode that cannot run
    ui::SpringColor hero_ink_;  // the hero's edge follows the focused mode
    int detail_before_ = -1;    // the mode that is leaving the hero, or -1
    float detail_side_ = 1.0f;  // the way the focus went: +1 right, -1 left
    tween::Spring detail_;      // 0 -> 1 as the focused mode takes the hero
    float detail_since_ = 0.0f; // seconds since it did: its charts grow by it
    tween::Spring band_figure_; // the hero's large figure, counting
    tween::Spring solved_;      // beside the tabs
    tween::Spring puzzles_;
    ui::SpringRect theme_ring_; // the one ring over the catalogue (scrolls with it)
    ui::SpringColor theme_ink_;
    tween::Spring theme_alpha_;
    ui::Pulse theme_press_;
    ui::Pulse theme_refusal_;
    Direction theme_refused_ = Direction::none;
    float themes_since_ = 0.0f; // seconds since the catalogue was shown: its levels grow
    ui::SpringRect plate_;      // the lit plate of the segmented control
    ui::Pulse plate_press_;
    ui::Pulse plate_refusal_;
    float plate_side_ = 1.0f; // which trigger was used last
    tween::Spring band_low_;  // the chosen band's ratings, counting
    tween::Spring band_high_;
    tween::Spring place_; // the focused theme's number in the catalogue

    // ---- what it shows ----
    bool english_ = true;        // no catalog is loaded: the themes wear their English names
    bool tabs_fitted_ = false;   // the tab row has been measured
    float tabs_width_ = kTabs.w; // ... and is this wide
    float refresh_in_ = 0.0f;
    puzzles::Records records_;
    std::size_t pack_size_ = 0;
    std::array<Preview, kModeCount> previews_;
    std::array<std::string, kModeCount> reasons_; // why a mode cannot be opened, or empty
    std::string signature_;
    std::string daily_json_;
    int daily_rating_ = 0;
    long long daily_plays_ = 0;
    bool daily_white_ = true;
    bool online_ = false;
    bool signed_in_ = false;
    int rating_ = 0; // the player's puzzle rating (0: not known)
    int rating_change_ = 0;
    int rating_games_ = 0;
    bool rating_provisional_ = false;
    std::vector<float> rating_history_;
    std::vector<ThemeEntry> entries_;
};

} // namespace

std::unique_ptr<Page> make_puzzles_page(app::Context &ctx)
{
    return std::make_unique<PuzzlesPage>(ctx);
}

} // namespace pch::modes
