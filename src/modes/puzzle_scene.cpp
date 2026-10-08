// ProsperoLichess - Puzzle screen: classic, Streak and Storm rules over any puzzle source.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// In the house look (app/look.hpp):
//
//   - the board leads: it stands on a shadow and on light in the colour of
//     the way that is being played, and that light answers a right or a wrong
//     move;
//   - the column beside it is a stack of lit panels on shared edges. One
//     puzzle at a time: whose move it is in display type, a strip that says
//     how the last move went (its mark pops, its words give way to the next
//     ones, it takes the colour of the verdict), the facts as figures, the
//     themes as tags that arrive once they give nothing away;
//   - Storm: the clock is a ring with the time ticking inside it, the score a
//     large figure that counts, the combo a level that glows, the run a row of
//     marks. Streak: the run is a flight of rising steps toward the record;
//   - a run ends with ceremony: its score counts inside a ring that closes on
//     the record, and a new record is gold.

#include "modes/puzzle_scene.hpp"

#include "lichess/puzzle_sources.hpp"
#include "lichess/session.hpp"
#include "modes/scenes.hpp"
#include "platform/ps5/system.hpp"
#include "puzzles/pack.hpp"
#include "puzzles/pack_source.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace pch::modes
{

namespace
{

namespace look = app::look;
using gfx::Align;
using gfx::Color;
using gfx::Rect;

constexpr float kTau = 6.2831853f;
constexpr Rect kBoard = app::kBoardSquares;
// The column beside the board: panels share its left and right edges.
constexpr float kLeft = app::kBoardColumn;
constexpr float kWide = app::kRight - app::kBoardColumn;
constexpr float kFoot = 960.0f; // its lower edge
constexpr float kPad = 32.0f;   // between a panel's edge and what is on it
constexpr float kRule = 98.0f;  // from a head panel's top to the rule under its heading

// ---- one puzzle at a time: who is to play, the state, the facts, the themes ----
constexpr Rect kHead{kLeft, app::kStage, kWide, 252.0f};
constexpr Rect kStrip{kLeft, kHead.y + kHead.h + app::kGap, kWide, 88.0f};
constexpr Rect kFacts{kLeft, kStrip.y + kStrip.h + app::kGap, kWide, 132.0f};
constexpr float kThemesTop = kFacts.y + kFacts.h + app::kGap;
constexpr Rect kThemes{kLeft, kThemesTop, kWide, kFoot - kThemesTop};

// ---- a run: the figures, then Storm's combo and marks or Streak's climb ----
constexpr Rect kRunHead{kLeft, app::kStage, kWide, 330.0f};
constexpr float kRunBelow = kRunHead.y + kRunHead.h + app::kGap;
constexpr Rect kCombo{kLeft, kRunBelow, kWide, 124.0f};
constexpr float kRunTop = kCombo.y + kCombo.h + app::kGap;
constexpr Rect kRun{kLeft, kRunTop, kWide, kFoot - kRunTop};
constexpr Rect kClimb{kLeft, kRunBelow, kWide, 236.0f};
constexpr float kLastTop = kClimb.y + kClimb.h + app::kGap;
constexpr Rect kLast{kLeft, kLastTop, kWide, kFoot - kLastTop};

// ---- no puzzle: one panel across the screen ----
constexpr Rect kWhole{app::kMargin, app::kStage, app::kRight - app::kMargin, kFoot - app::kStage};

constexpr float kClock = 100.0f; // Storm's ring
constexpr float kBest = 92.0f;   // Streak's ring
constexpr float kTagHeight = 40.0f;
constexpr float kTagText = 20.0f;
constexpr float kTagPad = 16.0f;
constexpr int kMarksPerRow = 18;
constexpr int kMarkRows = 2;
constexpr int kSteps = 24;      // the steps of Streak's climb on screen
constexpr int kResultLines = 7; // the lines of the result dialog kept free for the score
constexpr std::size_t kTagsKept = 8;

// What the rows of the pause menu do.
enum MenuId
{
    kResume = 1,
    kNext,
    kFlip,
    kEnd,
    kLeave,
};

// What a puzzle is about, in two words: its first theme that is not about
// its length, the phase of the game or the size of the advantage.
std::string main_theme(const puzzles::Puzzle &puzzle)
{
    static constexpr std::string_view kBroad[] = {
        "short", "long",     "veryLong",  "oneMove",  "opening", "middlegame",     "endgame",
        "mate",  "crushing", "advantage", "equality", "master",  "masterVsMaster", "superGM"};
    for (const std::string &theme : puzzle.themes)
    {
        if (std::find(std::begin(kBroad), std::end(kBroad), theme) == std::end(kBroad))
            return puzzles::theme_label(theme);
    }
    return puzzle.themes.empty() ? std::string(tr("Puzzle"))
                                 : puzzles::theme_label(puzzle.themes[0]);
}

ui::ListItem menu_row(const char *title, int id)
{
    ui::ListItem item;
    item.title = title;
    item.tag = id;
    return item;
}

std::uint64_t seed_from(const app::Context &ctx)
{
    return static_cast<std::uint64_t>(ctx.time * 1000003.0f) + 0x9e3779b97f4a7c15ull;
}

std::string number(float value)
{
    return std::to_string(static_cast<int>(std::lround(value)));
}

// "0:07", "2:54".
std::string minutes(float seconds)
{
    const int total = std::max(0, static_cast<int>(seconds));
    char text[16];
    std::snprintf(text, sizeof(text), "%d:%02d", total / 60, total % 60);
    return text;
}

// A small label over a figure, each kept inside room (a translated label is
// often longer than the figure under it). Returns the figure's width.
float fact(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view label,
           std::string_view value, float x, float baseline, Color ink, float room,
           Align align = Align::left, float size = 34.0f)
{
    ui::text_fit(list, fonts.semibold, ui::upper(label), x, baseline, 13.0f, room,
                 look::kInk.with_alpha(look::kFaint), align, 3.0f);
    return ui::text_fit(list, fonts.semibold, value, x, baseline + 40.0f, size, room, ink, align);
}

// Small tracked capitals kept inside room: look::kicker for a text that a
// translation may make longer than its place. Returns the width.
float kicker_fit(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
                 float baseline, Color color, float room, Align align = Align::left,
                 float size = 16.0f)
{
    return ui::text_fit(list, fonts.semibold, ui::upper(text), x, baseline, size, room, color,
                        align, 3.0f);
}

float tag_width(const ui::Fonts &fonts, std::string_view text)
{
    return fonts.semibold.measure(text, kTagText) + 2.0f * kTagPad;
}

// A theme of a finished puzzle: a capsule in the accent. shown: it pops in.
void tag(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, const Rect &r,
         Color accent, float shown)
{
    if (shown <= 0.01f)
        return;
    list.push_opacity(tween::clamp01(shown));
    list.push_transform(0.8f + 0.2f * shown, r.cx(), r.cy(), 0.0f, 0.0f);
    list.rounded_rect(r, r.h * 0.5f, accent.with_alpha(0.13f));
    list.bordered_rect(r, r.h * 0.5f, look::kClear, 1.5f, accent.with_alpha(0.42f));
    ui::text(list, fonts.semibold, text, r.x + kTagPad, r.cy() + kTagText * 0.35f, kTagText,
             look::kInk);
    list.pop_transform();
    list.pop_opacity();
}

// A puzzle of a run: a check, a cross, or a quiet dot for a skipped one.
void run_mark(gfx::DrawList &list, int outcome, float cx, float cy, float radius)
{
    if (outcome == 2)
    {
        list.circle(cx, cy, radius, look::kInk.with_alpha(0.12f));
        list.circle(cx, cy, radius * 0.28f, look::kInk.with_alpha(look::kMuted));
        return;
    }
    app::draw_verdict(list, cx, cy, radius, outcome == 1, outcome == 1 ? look::kGood : look::kBad,
                      look::kNight);
}

// One line that gives way to the next: the old text leaves upward as the new
// one rises into its place. t: 0 the old one, 1 the new one. Both stay inside
// width.
void swap_text(gfx::DrawList &list, const ui::FontRef &font, const std::string &now,
               const std::string &before, float t, float x, float baseline, float size, Color color,
               float width, bool calm)
{
    t = tween::clamp01(t);
    if (calm || before.empty() || t >= 0.999f)
    {
        ui::text_fit(list, font, now, x, baseline, size, width, color);
        return;
    }
    const float gone = tween::clamp01(t * 2.2f);
    if (gone < 1.0f)
        ui::text_fit(list, font, before, x, baseline - size * 0.5f * tween::cubic_in(gone), size,
                     width, color.with_alpha(1.0f - gone));
    const float here = tween::cubic_out(tween::clamp01((t - 0.42f) / 0.58f));
    if (here > 0.0f)
        ui::text_fit(list, font, now, x, baseline + size * 0.5f * (1.0f - here), size, width,
                     color.with_alpha(here));
}

// For the modes that need lichess.org when the app has no session at all.
class NoSource final : public puzzles::Source
{
  public:
    State next(app::Context &, int, puzzles::Puzzle *, std::string *error) override
    {
        *error = tr("No connection to Lichess");
        return State::error;
    }
};

} // namespace

// ---- the ways to play: their hues and their signs -----------------------------

Color puzzle_hue(PuzzleKind kind)
{
    switch (kind)
    {
    case PuzzleKind::training:
        return Color::rgb(0xd4e86a); // a target's yellow green
    case PuzzleKind::streak:
        return Color::rgb(0xff9a5c); // embers
    case PuzzleKind::storm:
        return Color::rgb(0xc4a7ff); // lightning over the page's violet
    default:
        return look::accent(look::Section::puzzles);
    }
}

void draw_puzzle_sign(gfx::DrawList &list, PuzzleKind kind, const Rect &box, Color ink)
{
    const float s = std::min(box.w, box.h);
    const float cx = box.cx();
    const float cy = box.cy();
    const float pen = std::max(s * 0.1f, 2.2f);
    const Color clear{ink.r, ink.g, ink.b, 0.0f};
    switch (kind)
    {
    case PuzzleKind::daily:
    {
        // A calendar leaf: its rings, its header and today's dot.
        const Rect leaf{cx - s * 0.42f, cy - s * 0.34f, s * 0.84f, s * 0.8f};
        list.bordered_rect(leaf, s * 0.14f, clear, pen, ink);
        list.rounded_rect({leaf.x, leaf.y + s * 0.2f, leaf.w, pen}, 0.0f, ink);
        for (const float side : {-1.0f, 1.0f})
            list.rounded_rect({cx + side * s * 0.2f - pen * 0.5f, cy - s * 0.48f, pen, s * 0.22f},
                              pen * 0.5f, ink);
        list.circle(cx + s * 0.14f, cy + s * 0.2f, s * 0.1f, ink);
        break;
    }
    case PuzzleKind::training:
        // A target.
        list.ring(cx, cy, s * 0.46f, pen, ink);
        list.ring(cx, cy, s * 0.26f, pen, ink);
        list.circle(cx, cy, s * 0.08f, ink);
        break;
    case PuzzleKind::streak:
    {
        // Rising steps.
        const float w = s * 0.19f;
        const float gap = s * 0.07f;
        for (int i = 0; i < 4; ++i)
        {
            const float h = s * (0.28f + 0.22f * static_cast<float>(i));
            list.rounded_rect(
                {cx - s * 0.485f + static_cast<float>(i) * (w + gap), cy + s * 0.47f - h, w, h},
                std::min(3.0f, w * 0.3f), ink);
        }
        break;
    }
    case PuzzleKind::storm:
        look::speed_icon(list, box, look::Speed::blitz, ink);
        break;
    default:
    {
        // Four squares of a board: every theme at once.
        const float side = s * 0.44f;
        for (int i = 0; i < 4; ++i)
        {
            const float x = i % 2 == 0 ? cx - s * 0.5f : cx + s * 0.06f;
            const float y = i / 2 == 0 ? cy - s * 0.5f : cy + s * 0.06f;
            list.rounded_rect({x, y, side, side}, side * 0.2f,
                              ink.with_alpha(i == 0 || i == 3 ? 1.0f : 0.45f));
        }
        break;
    }
    }
}

void draw_puzzle_badge(gfx::DrawList &list, PuzzleKind kind, const Rect &tile, float lit)
{
    const Color hue = puzzle_hue(kind);
    const float radius = tile.w * 0.29f;
    lit = tween::clamp01(lit);
    if (lit > 0.01f)
        list.glow(tile, radius, tile.w * 0.22f, hue.with_alpha(0.35f * lit));
    list.gradient_rect(tile, radius,
                       gfx::mix(hue.with_alpha(0.16f), gfx::mix(hue, look::kInk, 0.2f), lit),
                       gfx::mix(hue.with_alpha(0.12f), gfx::mix(hue, look::kNight, 0.25f), lit));
    draw_puzzle_sign(list, kind, tile.inset(tile.w * 0.25f), gfx::mix(hue, look::kNight, lit));
}

// ---- lines that give way ------------------------------------------------------

bool PuzzleScene::Line::set(const std::string &text)
{
    if (text == now)
        return false;
    before = now;
    now = text;
    // The first words of a screen are simply there.
    in.snap(before.empty() ? 1.0f : 0.0f);
    in.target = 1.0f;
    return true;
}

void PuzzleScene::Line::update(float dt, bool calm)
{
    in.target = 1.0f;
    in.update(dt, calm ? 60.0f : 10.0f);
}

PuzzleScene::PuzzleScene(PuzzleSetup setup)
    : title_(std::move(setup.title)), subtitle_(std::move(setup.subtitle)), rules_(setup.rules),
      kind_(setup.rules == PuzzleRules::storm    ? PuzzleKind::storm
            : setup.rules == PuzzleRules::streak ? PuzzleKind::streak
                                                 : setup.kind),
      accent_(puzzle_hue(kind_)), source_(std::move(setup.source))
{
    strip_ink_.snap(accent_);
    const bool storm = rules_ == PuzzleRules::storm;
    const bool classic = rules_ == PuzzleRules::classic;
    const ui::Theme &theme = ui::default_theme();
    look::tint(accent_, run_bar_, play_bar_, done_bar_, busy_, bones_head_, bones_facts_,
               bones_foot_, empty_, retry_, leave_, pause_, result_);

    // ---- the shortcuts sit at the foot of the column's last panel ----
    const Rect last = classic ? kThemes : storm ? kRun : kLast;
    for (ui::QuickActionBar *bar : {&run_bar_, &play_bar_, &done_bar_})
    {
        bar->style.anchor = ui::QuickAnchor::bottom_left;
        bar->style.margin = 0.0f;
        bar->style.spacing = 16.0f;
        bar->set_bounds(last.inset(kPad));
    }
    if (storm)
        run_bar_.set_actions({{tr("Flip board"), ui::Button::square, Action::west},
                              {tr("End run"), ui::Button::triangle, Action::north}});
    else
        run_bar_.set_actions({{tr("Hint"), ui::Button::triangle, Action::north},
                              {tr("Skip"), ui::Button::square, Action::west}});
    play_bar_.set_actions({{tr("Hint"), ui::Button::triangle, Action::north},
                           {tr("View solution"), ui::Button::square, Action::west}});
    done_bar_.set_actions({{tr("Next puzzle"), ui::Button::cross, Action::confirm},
                           {tr("Retry"), ui::Button::square, Action::west}});
    done_bar_.set_visible(false, true);

    // ---- loading: bones where the board and the column's words will be ----
    const Rect head = classic ? kHead : kRunHead;
    const Rect second = classic ? kFacts : storm ? kCombo : kClimb;
    bones_head_.style.kind = ui::SkeletonKind::line;
    bones_head_.style.lines = 2;
    bones_head_.style.line_height = 18.0f;
    bones_head_.style.line_gap = 16.0f;
    bones_head_.set_bounds({head.x + kPad, head.y + kRule + 78.0f, head.w * 0.62f, 56.0f});
    bones_facts_.style.kind = ui::SkeletonKind::line;
    bones_facts_.style.lines = 2;
    bones_facts_.style.line_height = 20.0f;
    bones_facts_.style.line_gap = 22.0f;
    bones_facts_.set_bounds(
        {second.x + kPad, second.y + 34.0f, second.w - 2.0f * kPad, second.h - 64.0f});
    bones_foot_.style.kind = ui::SkeletonKind::list_row;
    bones_foot_.style.rows = 2;
    bones_foot_.style.row_height = 52.0f;
    bones_foot_.style.row_gap = 20.0f;
    bones_foot_.set_bounds({last.x + kPad, last.y + 40.0f, last.w - 2.0f * kPad, 124.0f});

    // ---- no puzzle ----
    empty_.style.title_size = 34.0f;
    empty_.style.body_size = 24.0f;
    empty_.style.icon_size = 112.0f;
    empty_.style.max_text_width = 620.0f;
    empty_.style.float_icon = false;
    empty_.action.clear();
    const Color accent = accent_;
    empty_.icon = [accent](ui::Canvas &canvas, const Rect &area)
    {
        // A board with nothing on it: there is no position to show.
        look::halo(canvas.list, area.inset(-40.0f), accent, 0.16f);
        canvas.list.circle(area.cx(), area.cy(), area.w * 0.5f, accent.with_alpha(0.12f));
        canvas.list.ring(area.cx(), area.cy(), area.w * 0.5f, 2.0f, accent.with_alpha(0.5f));
        draw_puzzle_sign(canvas.list, PuzzleKind::themes, area.inset(area.w * 0.3f),
                         look::kInk.with_alpha(look::kMuted));
    };
    empty_.set_bounds({kWhole.x, kWhole.y + 150.0f, kWhole.w, 360.0f});
    retry_.label = tr("Try again");
    retry_.glyph = ui::Button::cross;
    retry_.style.role = ui::ButtonRole::primary;
    leave_.style.role = ui::ButtonRole::secondary;

    // ---- the menu and the end of a run ----
    pause_.style.layout = ui::PauseLayout::center;
    pause_.kicker = tr("Paused");
    pause_.title = title_;
    pause_.subtitle = subtitle_;
    std::vector<ui::ListItem> rows{menu_row(tr("Resume"), kResume)};
    if (classic)
        rows.push_back(menu_row(tr("Skip this puzzle"), kNext));
    rows.push_back(menu_row(tr("Flip board"), kFlip));
    if (!classic)
        rows.push_back(menu_row(tr("End run"), kEnd));
    rows.push_back(menu_row(tr("Return to menu"), kLeave));
    pause_.set_items(std::move(rows));

    // The answers are plain plates on the frosted panel: glass buttons on a
    // glass panel would each frost the board a second time.
    if (theme.style == ui::SurfaceStyle::glass)
    {
        result_.style.theme.style = ui::SurfaceStyle::flat;
        result_.style.theme.border = 1.5f;
        result_.style.theme.shadow_blur = 40.0f;
        result_.style.theme.shadow_offset = 16.0f;
    }
    result_.style.frost = 0.78f;
    result_.style.body_lines = kResultLines + 2;
    // The run's own cue announces its end.
    result_.style.sounds.open = audio::Cue::count;
}

void PuzzleScene::enter(app::Context &ctx)
{
    // The column assembles every time the screen comes to the front.
    since_ = 0.0f;
    if (started_)
        return;
    started_ = true;
    records_ = puzzles::load_records(ctx.data_root);
    // The offline pack answers at once: the screen opens on its first puzzle.
    next_puzzle(ctx);
}

bool PuzzleScene::expected(chess::Move *move) const
{
    if (phase_ != Phase::player || step_ >= puzzle_.solution.size())
        return false;
    *move = puzzle_.solution[step_];
    return true;
}

int PuzzleScene::target_rating() const
{
    switch (rules_)
    {
    case PuzzleRules::streak:
        return streak_rating(score_);
    case PuzzleRules::storm:
        return storm_rating(score_);
    case PuzzleRules::classic:
        break;
    }
    return 0;
}

// Storm's clock runs while puzzles are on screen.
bool PuzzleScene::clock_runs() const
{
    return rules_ == PuzzleRules::storm && phase_ != Phase::run_over && phase_ != Phase::error &&
           puzzles_seen_ > 0 && !pause_.is_open();
}

void PuzzleScene::next_puzzle(app::Context &ctx)
{
    phase_ = Phase::loading;
    error_.clear();
    practice_ = false;
    input_.clear_selection();
    // Asked at once, so a source that has a puzzle ready never shows the
    // loading state for a frame between two puzzles.
    poll_source(ctx);
}

void PuzzleScene::poll_source(app::Context &ctx)
{
    std::string error;
    const puzzles::Source::State state = source_->next(ctx, target_rating(), &puzzle_, &error);
    if (state == puzzles::Source::State::ready)
    {
        begin(ctx);
    }
    else if (state == puzzles::Source::State::error || state == puzzles::Source::State::exhausted)
    {
        exhausted_ = state == puzzles::Source::State::exhausted;
        error_ = error.empty() ? tr("No more puzzles here") : error;
        phase_ = Phase::error;
        choice_ = 0;
        choice_placed_ = false;
        empty_.title = exhausted_ ? tr("Nothing more to solve") : tr("No puzzle to show");
        empty_.body = error_;
        empty_.enter();
        leave_.label = tr("Back");
        ctx.cue(exhausted_ ? audio::Cue::notify : audio::Cue::error);
    }
}

void PuzzleScene::begin(app::Context &ctx)
{
    position_ = puzzle_.before;
    last_move_ = {};
    step_ = 0;
    failed_ = false;
    hinted_ = false;
    shown_ = false;
    hint_level_ = 0;
    feedback_ = 0.0f;
    verdict_ = Verdict::none;
    puzzle_time_ = 0.0f;
    ++puzzles_seen_;
    // In a run the themes of the puzzle before stay up; a classic puzzle
    // starts without any, since they would give its answer away.
    if (rules_ == PuzzleRules::classic)
    {
        tags_.clear();
        rating_reveal_.snap(0.0f);
        puzzle_rating_.snap(0.0f);
    }
    sys::log("[PCH] puzzle %s rating=%d source=%s moves=%zu", puzzle_.id.c_str(), puzzle_.rating,
             puzzle_.source.c_str(), puzzle_.solution.size());
    view_.set_orientation(puzzle_.solver(), false);
    view_.snap(position_);
    input_.clear_selection();
    input_.place_cursor(chess::make_square(4, puzzle_.solver() == chess::Color::white ? 3 : 4));
    if (!ctx.reduced_motion() && rules_ != PuzzleRules::storm)
        view_.deal();
    phase_ = Phase::intro;
    timer_ = rules_ == PuzzleRules::storm ? 0.25f : 1.0f;
}

void PuzzleScene::play(app::Context &ctx, const chess::Move &move, bool animate)
{
    const chess::Position before = position_;
    position_ = position_.after(move);
    last_move_ = move;
    view_.play(before, move, position_, animate && !ctx.reduced_motion() ? 1.0f : 0.0f);
    if (position_.in_check())
        ctx.cue(audio::Cue::check);
    else if (before.is_capture(move))
        ctx.cue(audio::Cue::capture);
    else if (before.is_castle(move))
        ctx.cue(audio::Cue::castle);
    else if (move.promotion)
        ctx.cue(audio::Cue::promote);
    else
        ctx.cue(audio::Cue::move);
}

// Notes how the puzzle on the board ended: it is the run's newest mark, and
// the puzzle the column tells about until the next one is over.
void PuzzleScene::remember(int outcome)
{
    if (rules_ == PuzzleRules::classic)
        return;
    last_.rating = puzzle_.rating;
    last_.theme = main_theme(puzzle_);
    last_.outcome = outcome;
    last_.seconds = puzzle_time_;
    has_last_ = true;
    marks_.push_back(static_cast<std::uint8_t>(outcome));
    mark_age_ = 0.0f;
}

// The themes of the puzzle on the board become tags. Only once it is over:
// "Fork" or "Mate in 2" is half the answer.
void PuzzleScene::reveal_themes()
{
    tags_.clear();
    for (const std::string &theme : puzzle_.themes)
    {
        if (tags_.size() >= kTagsKept)
            break;
        tags_.push_back(puzzles::theme_label(theme));
    }
    tags_age_ = 0.0f;
}

void PuzzleScene::solved(app::Context &ctx)
{
    reveal_themes();
    if (!ctx.reduced_motion())
    {
        board_flash_good_ = true;
        board_flash_.trigger();
    }
    if (practice_)
    {
        // A retry of a puzzle whose result is already in.
        phase_ = Phase::solved;
        ctx.cue(audio::Cue::puzzle_solved);
        return;
    }
    const bool clean = !failed_ && !hinted_;
    source_->report(ctx, puzzle_, clean, clean);
    ++records_.solved;
    if (clean || rules_ == PuzzleRules::classic)
        ++score_;
    remember(1);
    switch (rules_)
    {
    case PuzzleRules::classic:
        phase_ = Phase::solved;
        ctx.cue(audio::Cue::puzzle_solved);
        if (clean)
            confetti_.burst(accent_, static_cast<std::uint32_t>(ctx.time * 997.0f), true);
        break;
    case PuzzleRules::streak:
        ctx.cue(audio::Cue::puzzle_solved);
        phase_ = Phase::solved;
        timer_ = 0.9f;
        break;
    case PuzzleRules::storm:
    {
        ++combo_;
        best_combo_ = std::max(best_combo_, combo_);
        ctx.cue(audio::Cue::combo);
        for (int i = 0; i < kStormComboCount; ++i)
        {
            if (combo_ == kStormComboSteps[i])
            {
                clock_ += static_cast<float>(kStormComboBonus[i]);
                bonus_seconds_ = kStormComboBonus[i];
                bonus_flash_ = 1.2f;
                if (!ctx.reduced_motion())
                    level_flash_.trigger();
                ctx.cue(audio::Cue::puzzle_solved);
            }
        }
        phase_ = Phase::solved;
        timer_ = 0.25f;
        break;
    }
    }
    puzzles::save_records(ctx.data_root, records_);
}

void PuzzleScene::end_run(app::Context &ctx)
{
    phase_ = Phase::run_over;
    input_.clear_selection();
    new_record_ = false;
    const bool storm = rules_ == PuzzleRules::storm;
    best_before_ = storm ? records_.best_storm : records_.best_streak;
    if (rules_ == PuzzleRules::streak && score_ > records_.best_streak)
    {
        records_.best_streak = score_;
        new_record_ = score_ > 0;
    }
    if (rules_ == PuzzleRules::storm && score_ > records_.best_storm)
    {
        records_.best_storm = score_;
        new_record_ = score_ > 0;
    }
    puzzles::save_records(ctx.data_root, records_);
    ctx.cue(new_record_ ? audio::Cue::new_record : audio::Cue::streak_end);
    if (new_record_)
        confetti_.burst(look::kGold, static_cast<std::uint32_t>(ctx.time * 1000.0f),
                        ctx.reduced_motion());

    ui::DialogContent content;
    std::string line;
    if (storm)
    {
        content.title = new_record_      ? tr("New record!")
                        : clock_ <= 0.0f ? tr("Time's up")
                                         : tr("Run over");
        line = fill(tr(mistakes_ == 1 ? TR("{0} mistake \xC2\xB7 best combo {1}")
                                      : TR("{0} mistakes \xC2\xB7 best combo {1}")),
                    {std::to_string(mistakes_), std::to_string(best_combo_)});
    }
    else
    {
        content.title = new_record_ ? tr("New record!") : tr("Streak over");
        line = fill(tr("Reached puzzles rated {0}"), {std::to_string(target_rating())});
    }
    // Empty lines keep room for the score, which is drawn over the dialog:
    // it has no slot for content of its own.
    content.body = line + std::string(static_cast<std::size_t>(kResultLines + 1), '\n');
    ui::DialogButton again;
    again.label = tr("Play again");
    again.kind = ui::ButtonKind::primary;
    ui::DialogButton back;
    back.label = tr("Back to puzzles");
    content.buttons = {again, back};
    result_.open(std::move(content), *ctx.feedback);
    final_shown_.snap(0.0f);
    final_share_.snap(0.0f);
    figures_.snap(0.0f);
    over_age_ = 0.0f;
}

void PuzzleScene::restart_run(app::Context &ctx)
{
    score_ = 0;
    combo_ = 0;
    best_combo_ = 0;
    skips_ = 1;
    mistakes_ = 0;
    clock_ = kStormSeconds;
    bonus_flash_ = 0.0f;
    puzzles_seen_ = 0;
    new_record_ = false;
    has_last_ = false;
    marks_.clear();
    tags_.clear();
    // The column counts and grows again from nothing.
    since_ = 0.0f;
    score_seen_ = 0;
    score_shown_.snap(0.0f);
    ring_.snap(0.0f);
    level_.snap(0.0f);
    best_share_.snap(0.0f);
    next_puzzle(ctx);
}

void PuzzleScene::mistake(app::Context &ctx, const chess::Move &move)
{
    bad_ = move.to;
    good_ = chess::kNoSquare;
    feedback_ = 1.0f;
    verdict_ = Verdict::wrong;
    view_.shake(move.from);
    if (!ctx.reduced_motion())
    {
        board_flash_good_ = false;
        board_flash_.trigger();
    }
    ctx.cue(audio::Cue::puzzle_wrong);
    ++mistakes_;
    switch (rules_)
    {
    case PuzzleRules::classic:
        if (!failed_ && !practice_)
            source_->report(ctx, puzzle_, false, !hinted_);
        failed_ = true;
        break;
    case PuzzleRules::streak:
        source_->report(ctx, puzzle_, false, true);
        reveal_themes();
        remember(0);
        end_run(ctx);
        break;
    case PuzzleRules::storm:
        source_->report(ctx, puzzle_, false, true);
        reveal_themes();
        remember(0);
        clock_ = std::max(0.0f, clock_ - static_cast<float>(kStormPenalty));
        combo_ = 0;
        bonus_seconds_ = -kStormPenalty;
        bonus_flash_ = 1.2f;
        next_puzzle(ctx);
        break;
    }
}

void PuzzleScene::on_player_move(app::Context &ctx, const chess::Move &move)
{
    if (!puzzles::accepts(puzzle_, position_, step_, move))
    {
        mistake(ctx, move);
        return;
    }
    good_ = move.to;
    bad_ = chess::kNoSquare;
    feedback_ = 1.0f;
    verdict_ = Verdict::correct;
    play(ctx, move, true);
    ++step_;
    if (step_ >= puzzle_.solution.size() || position_.is_checkmate())
    {
        solved(ctx);
        return;
    }
    if (rules_ != PuzzleRules::storm)
        ctx.cue(audio::Cue::puzzle_correct);
    phase_ = Phase::opponent;
    timer_ = rules_ == PuzzleRules::storm ? 0.18f : 0.45f;
}

// The first hint marks the piece to move, the second draws the move.
void PuzzleScene::give_hint(app::Context &)
{
    if (step_ >= puzzle_.solution.size())
        return;
    hinted_ = true;
    hint_level_ = std::min(2, hint_level_ + 1);
    input_.place_cursor(puzzle_.solution[step_].from);
}

void PuzzleScene::show_solution()
{
    failed_ = true;
    shown_ = true;
    phase_ = Phase::showing;
    timer_ = 0.2f;
    input_.clear_selection();
}

void PuzzleScene::flip(app::Context &ctx)
{
    view_.set_orientation(chess::opposite(view_.orientation()), !ctx.reduced_motion());
}

// A row of the menu was chosen: its own cue is the goodbye, so the menu
// leaves without a second sound.
void PuzzleScene::close_menu_quietly(app::Context &ctx)
{
    const audio::Cue voice = pause_.style.sounds.close;
    pause_.style.sounds.close = audio::Cue::count;
    pause_.close(*ctx.feedback);
    pause_.style.sounds.close = voice;
}

// What the strip under the heading says about a classic puzzle right now.
PuzzleScene::State PuzzleScene::strip_state() const
{
    State now;
    now.words = hint_level_ >= 2   ? tr("Hint: follow the arrow")
                : hint_level_ == 1 ? tr("Hint: move the marked piece")
                                   : tr("Your move");
    now.mark = Mark::live;
    if (phase_ == Phase::solved)
    {
        now.words = shown_               ? tr("Solution shown")
                    : failed_ || hinted_ ? tr("Solved with help")
                                         : tr("Solved!");
        now.mark = shown_ ? Mark::quiet : Mark::good;
    }
    else if (phase_ == Phase::showing)
    {
        now.words = tr("The solution");
        now.mark = Mark::quiet;
    }
    else if (phase_ == Phase::intro)
    {
        now.words = tr("The opponent moves first");
        now.mark = Mark::quiet;
    }
    else if (verdict_ == Verdict::wrong)
    {
        now.words = tr("That's not it. Try again");
        now.mark = Mark::bad;
    }
    else if (verdict_ == Verdict::correct)
    {
        now.words = tr("Best move. Keep going");
        now.mark = Mark::good;
    }
    return now;
}

// The line under "Black to play". The side is part of the sentence: its
// word takes another form in many languages.
std::string PuzzleScene::task() const
{
    if (practice_)
        return tr("Practice: this result is not counted");
    if (phase_ == Phase::showing || (phase_ == Phase::solved && shown_))
        return tr("The winning line is on the board");
    if (phase_ == Phase::solved && failed_)
        return tr("Solved after a mistake");
    if (phase_ == Phase::solved && hinted_)
        return tr("Solved with a hint");
    if (phase_ == Phase::solved)
        return fill(tr("Solved in {0} s"),
                    {std::to_string(std::max(1, static_cast<int>(std::lround(puzzle_time_))))});
    return puzzle_.solver() == chess::Color::white ? tr("Find the best move for White")
                                                   : tr("Find the best move for Black");
}

float PuzzleScene::arrive(int index) const
{
    return calm_ ? 1.0f : look::rise(since_, index, 0.07f, 0.5f);
}

// Advances everything that moves, whatever the rules are doing.
void PuzzleScene::animate(app::Context &ctx, float dt)
{
    ctx.calm(run_bar_, play_bar_, done_bar_, busy_, bones_head_, bones_facts_, bones_foot_, empty_,
             retry_, leave_, pause_, result_);
    calm_ = ctx.reduced_motion();
    const float quick = calm_ ? 60.0f : 14.0f;
    const float counting = calm_ ? 60.0f : 8.0f;
    since_ += dt;
    tags_age_ += dt;
    mark_age_ += dt;
    step_age_ += dt;
    // A puzzle that kept the screen waiting arrives the way the screen did.
    if (phase_ == Phase::loading)
    {
        waited_ += dt;
    }
    else
    {
        if (waited_ > 0.2f && phase_ != Phase::error)
            since_ = 0.0f;
        waited_ = 0.0f;
    }

    confetti_.update(dt);
    feedback_ = std::max(0.0f, feedback_ - dt * 1.4f);
    bonus_flash_ = std::max(0.0f, bonus_flash_ - dt);
    if (phase_ == Phase::player || phase_ == Phase::opponent)
        puzzle_time_ += dt;

    board::Overlay overlay;
    input_.fill_overlay(overlay);
    view_.update(dt, overlay);

    // ---- the heading and the board are lit while it is the player's move ----
    turn_.target = phase_ == Phase::player ? 1.0f : 0.0f;
    turn_.update(dt, quick);
    board_flash_.update(dt, 2.6f);

    // ---- the words: each gives way to the next ----
    if (phase_ != Phase::loading && phase_ != Phase::error)
    {
        const bool white = puzzle_.solver() == chess::Color::white;
        const bool classic = rules_ == PuzzleRules::classic;
        // Whole sentences: the side's word changes with what follows it.
        title_line_.set(white ? (classic ? tr("White to play") : tr("White to move"))
                              : (classic ? tr("Black to play") : tr("Black to move")));
        task_line_.set(task());
        const State now = strip_state();
        if (strip_line_.set(now.words))
        {
            strip_mark_before_ = strip_mark_;
            if ((now.mark == Mark::good || now.mark == Mark::bad) && !calm_)
                strip_flash_.trigger();
        }
        strip_mark_ = now.mark;
    }
    title_line_.update(dt, calm_);
    task_line_.update(dt, calm_);
    strip_line_.update(dt, calm_);
    strip_ink_.target(strip_mark_ == Mark::good  ? look::kGood
                      : strip_mark_ == Mark::bad ? look::kBad
                                                 : accent_);
    strip_ink_.update(dt, calm_ ? 60.0f : 12.0f);
    strip_flash_.update(dt, 2.6f);

    // ---- the figures count ----
    const bool over = phase_ == Phase::solved || phase_ == Phase::showing;
    if (rules_ == PuzzleRules::classic)
    {
        // The rating says how hard to look: Lichess shows it once the puzzle is over.
        rating_reveal_.target = over ? 1.0f : 0.0f;
        rating_reveal_.update(dt, quick);
        puzzle_rating_.target = over ? static_cast<float>(puzzle_.rating) : 0.0f;
        if (over)
            puzzle_rating_.update(dt, counting);
    }
    else
    {
        puzzle_rating_.target = static_cast<float>(target_rating());
        puzzle_rating_.update(dt, counting);
    }
    const int rating = source_->player_rating();
    if (rating > 0)
    {
        if (player_rating_.target <= 0.0f)
            player_rating_.snap(static_cast<float>(rating));
        player_rating_.target = static_cast<float>(rating);
    }
    player_rating_.update(dt, calm_ ? 60.0f : 6.0f);
    const int change = source_->rating_change();
    if (change != delta_seen_)
    {
        delta_seen_ = change;
        if (change != 0 && !calm_)
            delta_pop_.trigger();
    }
    delta_pop_.update(dt, 5.0f);
    solved_shown_.target = static_cast<float>(records_.solved);
    if (score_ != score_seen_)
    {
        if (score_ > score_seen_)
        {
            step_age_ = 0.0f;
            if (!calm_)
                score_pop_.trigger();
        }
        score_seen_ = score_;
    }
    score_shown_.target = static_cast<float>(score_);
    score_pop_.update(dt, 7.0f);
    // The numbers start counting once their panels have arrived.
    if (since_ > 0.35f || calm_)
    {
        solved_shown_.update(dt, counting);
        score_shown_.update(dt, calm_ ? 60.0f : 14.0f);
    }

    if (rules_ == PuzzleRules::storm)
    {
        // The ring opens from nothing with the screen, then follows the clock.
        ring_.target = std::clamp(clock_ / kStormSeconds, 0.0f, 1.0f);
        if (since_ > 0.3f || calm_)
            ring_.update(dt, calm_ ? 60.0f : since_ < 1.4f ? 5.0f : 20.0f);

        // The combo level fills towards the next bonus.
        int next = kStormComboSteps[0];
        int previous = 0;
        for (int step : kStormComboSteps)
        {
            if (combo_ < step)
            {
                next = step;
                break;
            }
            previous = step;
            next = step + 10;
        }
        level_.target =
            std::clamp(static_cast<float>(combo_ - previous) / static_cast<float>(next - previous),
                       0.0f, 1.0f);
        level_.update(dt, calm_ ? 60.0f : 12.0f);
        heat_.target =
            combo_ > 0 ? std::min(1.0f, 0.4f + static_cast<float>(combo_) / 20.0f) : 0.0f;
        heat_.update(dt, calm_ ? 60.0f : 8.0f);
        level_flash_.update(dt, 2.4f);
    }
    else if (rules_ == PuzzleRules::streak)
    {
        const int best = std::max(records_.best_streak, 1);
        best_share_.target = std::min(1.0f, static_cast<float>(score_) / static_cast<float>(best));
        if (since_ > 0.3f || calm_)
            best_share_.update(dt, calm_ ? 60.0f : 6.0f);
        run_bar_.at(1).set_enabled(skips_ > 0);
    }

    // ---- classic: which shortcuts are offered ----
    play_bar_.set_visible(phase_ == Phase::intro || phase_ == Phase::opponent ||
                          phase_ == Phase::player);
    done_bar_.set_visible(phase_ == Phase::solved);
    play_bar_.update(dt);
    done_bar_.update(dt);
    run_bar_.update(dt);

    busy_.update(dt);
    for (ui::Skeleton *bones : {&bones_head_, &bones_facts_, &bones_foot_})
        bones->update(dt);
    empty_.update(dt);
    retry_.update(dt);
    leave_.update(dt);
    // One ring glides between the answers of the error state.
    if (phase_ == Phase::error)
    {
        const float y = empty_.bounds().y + empty_.bounds().h + 16.0f;
        const Rect target = exhausted_     ? Rect{kWhole.cx() - 148.0f, y, 296.0f, 64.0f}
                            : choice_ == 0 ? Rect{kWhole.cx() - 264.0f, y, 256.0f, 64.0f}
                                           : Rect{kWhole.cx() + 8.0f, y, 256.0f, 64.0f};
        choice_ring_.target(target);
        if (!choice_placed_)
        {
            choice_placed_ = true;
            choice_ring_.snap(target);
        }
        choice_ring_.update(dt, calm_ ? 60.0f : 18.0f);
    }
    choice_nudge_.update(dt, 9.0f);

    pause_.update(dt);
    result_.update(dt);
    // The score arrives once the dialog has settled, and leaves with it.
    over_age_ += dt;
    figures_.target = result_.is_open() && over_age_ > 0.16f ? 1.0f : 0.0f;
    figures_.update(dt, calm_ ? 60.0f : 16.0f);
    if (figures_.target > 0.5f && (over_age_ > 0.4f || calm_))
    {
        final_shown_.target = static_cast<float>(score_);
        final_shown_.update(dt, calm_ ? 60.0f : 5.0f);
        // How close the run came to the record; a new record closes the ring.
        final_share_.target =
            new_record_ ? 1.0f
                        : std::min(1.0f, static_cast<float>(score_) /
                                             static_cast<float>(std::max(best_before_, 1)));
        final_share_.update(dt, calm_ ? 60.0f : 4.0f);
    }
}

app::Transition PuzzleScene::update(app::Context &ctx, const InputFrame &input, float dt)
{
    animate(ctx, dt);

    // ---- the menu takes every input while it is open ----
    if (pause_.is_open())
    {
        const ui::Event event = pause_.handle(input, *ctx.feedback);
        if (event != ui::Event::activated)
            return app::Transition::stay();
        const int id = pause_.items()[static_cast<std::size_t>(pause_.focus())].tag;
        if (id == kLeave)
            return app::Transition::pop();
        close_menu_quietly(ctx);
        if (id == kFlip)
            flip(ctx);
        else if (id == kNext && rules_ == PuzzleRules::classic)
            next_puzzle(ctx);
        else if (id == kEnd && phase_ != Phase::error)
            end_run(ctx);
        return app::Transition::stay();
    }

    // ---- so does the end of a run ----
    if (phase_ == Phase::run_over)
    {
        const ui::Event event = result_.handle(input, *ctx.feedback);
        if (event == ui::Event::cancelled ||
            (event == ui::Event::activated && result_.choice() != 0))
            return app::Transition::pop();
        if (event == ui::Event::activated)
            restart_run(ctx);
        return app::Transition::stay();
    }

    if (input.is_pressed(Action::menu) || input.is_pressed(Action::touch))
    {
        pause_.open(*ctx.feedback);
        return app::Transition::stay();
    }

    if (clock_runs())
    {
        const float before = clock_;
        clock_ -= dt;
        if (clock_ < 10.0f && std::floor(before) != std::floor(clock_) && clock_ > 0.0f)
            ctx.cue(audio::Cue::clock_tick);
        if (clock_ <= 0.0f)
        {
            clock_ = 0.0f;
            end_run(ctx);
            return app::Transition::stay();
        }
    }

    // Storm's shortcuts do not wait for the player's turn: the clock does not.
    if (rules_ == PuzzleRules::storm && phase_ != Phase::error &&
        run_bar_.handle(input, *ctx.feedback) == ui::Event::activated)
    {
        if (run_bar_.fired() == 0)
        {
            flip(ctx);
        }
        else
        {
            end_run(ctx);
            return app::Transition::stay();
        }
    }

    switch (phase_)
    {
    case Phase::loading:
        poll_source(ctx);
        break;
    case Phase::intro:
        timer_ -= dt;
        if (timer_ <= 0.0f)
        {
            play(ctx, puzzle_.setup, true);
            phase_ = Phase::player;
        }
        break;
    case Phase::opponent:
        timer_ -= dt;
        if (timer_ <= 0.0f && step_ < puzzle_.solution.size())
        {
            play(ctx, puzzle_.solution[step_], true);
            ++step_;
            phase_ = Phase::player;
        }
        break;
    case Phase::showing:
        timer_ -= dt;
        if (timer_ <= 0.0f)
        {
            if (step_ < puzzle_.solution.size())
            {
                play(ctx, puzzle_.solution[step_], true);
                ++step_;
                timer_ = 0.75f;
            }
            else
            {
                reveal_themes();
                phase_ = Phase::solved;
            }
        }
        break;
    case Phase::solved:
        if (rules_ != PuzzleRules::classic)
        {
            timer_ -= dt;
            if (timer_ <= 0.0f)
                next_puzzle(ctx);
        }
        else if (done_bar_.handle(input, *ctx.feedback) == ui::Event::activated)
        {
            if (done_bar_.fired() == 0)
            {
                next_puzzle(ctx);
            }
            else
            {
                // The same puzzle again, for practice.
                practice_ = true;
                begin(ctx);
            }
            return app::Transition::stay();
        }
        break;
    case Phase::error:
    {
        if (!exhausted_ && (input.nav == Direction::left || input.nav == Direction::right))
        {
            const int wanted = input.nav == Direction::right ? 1 : 0;
            if (wanted != choice_)
            {
                choice_ = wanted;
                ctx.cue(audio::Cue::focus);
            }
            else if (!input.nav_repeat && !ctx.reduced_motion())
            {
                // Nothing further that way: the ring shakes it off.
                choice_nudge_.trigger();
            }
        }
        ui::PushButton &button = exhausted_ || choice_ == 1 ? leave_ : retry_;
        if (button.handle(input, *ctx.feedback) == ui::Event::activated)
        {
            if (&button == &leave_)
                return app::Transition::pop();
            next_puzzle(ctx);
            return app::Transition::stay();
        }
        break;
    }
    case Phase::player:
    case Phase::run_over:
        break;
    }

    if (input.is_pressed(Action::back) && phase_ != Phase::player)
    {
        ctx.cue(audio::Cue::back);
        return app::Transition::pop();
    }
    if (phase_ != Phase::player)
        return app::Transition::stay();

    // ---- the player's turn: hints, the solution, the skip and the board ----
    if (rules_ == PuzzleRules::classic)
    {
        if (play_bar_.handle(input, *ctx.feedback) == ui::Event::activated)
        {
            if (play_bar_.fired() == 0)
            {
                give_hint(ctx);
            }
            else
            {
                show_solution();
                return app::Transition::stay();
            }
        }
    }
    else if (rules_ == PuzzleRules::streak)
    {
        if (run_bar_.handle(input, *ctx.feedback) == ui::Event::activated)
        {
            if (run_bar_.fired() == 0)
            {
                give_hint(ctx);
            }
            else if (skips_ > 0)
            {
                --skips_;
                remember(2);
                next_puzzle(ctx);
                return app::Transition::stay();
            }
        }
    }

    board::InputRules rules;
    rules.white = puzzle_.solver() == chess::Color::white;
    rules.black = !rules.white;
    rules.premoves = false;
    rules.auto_queen = ctx.settings->auto_queen;
    chess::Move move;
    const board::BoardInput::Result result =
        input_.update(input, dt, position_, view_.orientation(), rules, *ctx.feedback, &move);
    if (result == board::BoardInput::Result::move)
    {
        on_player_move(ctx, move);
    }
    else if (result == board::BoardInput::Result::back)
    {
        ctx.cue(audio::Cue::back);
        return app::Transition::pop();
    }
    return app::Transition::stay();
}

// ---- drawing ---------------------------------------------------------------

void PuzzleScene::draw_board(const app::Context &ctx, gfx::DrawList &list) const
{
    // The light under the board is the accent, steadier while the player is
    // to move, and for a moment the colour of the verdict.
    const float flash = tween::clamp01(board_flash_.value);
    const float breath = calm_ ? 0.5f : ui::breathe(ctx.time, 3.2f);
    look::frame_board(list, kBoard,
                      gfx::mix(accent_, board_flash_good_ ? look::kGood : look::kBad, flash),
                      (0.45f + turn_.value * (0.3f + 0.25f * breath) + 0.9f * flash) * arrive(0));

    board::Overlay overlay;
    if (phase_ == Phase::player)
        input_.fill_overlay(overlay);
    overlay.last_move = last_move_;
    overlay.coordinates = ctx.settings->coordinates;
    overlay.show_dests = ctx.settings->show_dests;
    overlay.good = good_;
    overlay.bad = bad_;
    overlay.feedback = feedback_;
    overlay.cursor_color = accent_;
    const bool hinting = phase_ == Phase::player && step_ < puzzle_.solution.size();
    if (hinting && hint_level_ >= 2)
        overlay.arrows.push_back({puzzle_.solution[step_].from, puzzle_.solution[step_].to,
                                  Color::rgb(0x15781b, 0.75f)});
    if (hinting && hint_level_ == 1 && overlay.selected == chess::kNoSquare)
    {
        overlay.good = puzzle_.solution[step_].from;
        overlay.feedback = 0.6f;
    }
    view_.draw(list, *ctx.fonts, *ctx.pieces, kBoard, overlay, ctx.board_theme(), ctx.time);
    input_.draw_promotion(list, *ctx.pieces, view_, kBoard, position_, accent_);

    // A mark pops on the square: a check for the right move, a cross otherwise.
    const chess::Square marked = good_ != chess::kNoSquare ? good_ : bad_;
    if (feedback_ > 0.0f && marked != chess::kNoSquare)
    {
        const bool right = good_ != chess::kNoSquare;
        const Rect square = view_.square_rect(kBoard, marked);
        const float pop = calm_ ? 1.0f : tween::back_out(tween::clamp01((1.0f - feedback_) * 5.0f));
        const float alpha = tween::clamp01(feedback_ * 3.0f);
        const float cx = square.x + square.w * 0.86f;
        const float cy = square.y + square.h * 0.14f;
        const float radius = square.w * 0.24f * pop;
        const Color fill = right ? look::kGood : look::kBad;
        list.circle(cx, cy + 2.0f, radius + 3.0f, Color::rgb(0x000000, 0.3f * alpha));
        app::draw_verdict(list, cx, cy, radius, right, fill.with_alpha(alpha),
                          look::kNight.with_alpha(alpha));
    }
}

// The top of the column's first panel: the sign of the way that is being
// played, its name and what it is, and for a run whose move it is.
void PuzzleScene::draw_heading(const app::Context &ctx, gfx::DrawList &list, const Rect &panel,
                               bool to_move) const
{
    const ui::Fonts &fonts = *ctx.fonts;
    const Rect tile{panel.x + kPad, panel.y + 26.0f, 48.0f, 48.0f};
    draw_puzzle_badge(list, kind_, tile, 1.0f);
    const float tx = tile.x + tile.w + 16.0f;
    float room = panel.x + panel.w - kPad - tx;
    if (to_move && !title_line_.now.empty())
    {
        // That side's king on a disc, lit while the player is to move.
        const float lit = turn_.value;
        const float cx = panel.x + panel.w - kPad - 24.0f;
        const float cy = tile.cy();
        list.circle(cx, cy, 24.0f,
                    gfx::mix(look::kInk.with_alpha(0.08f), accent_.with_alpha(0.24f), lit));
        list.ring(cx, cy, 24.0f, 2.0f, accent_.with_alpha(0.25f + 0.65f * lit));
        ctx.pieces->draw(list, {puzzle_.solver(), chess::Role::king},
                         {cx - 18.0f, cy - 18.0f, 36.0f, 36.0f});
        const Color ink = look::kInk.with_alpha(tween::lerp(look::kMuted, 1.0f, lit));
        // The words end at the disc; a long translation shrinks into their place.
        const float width = std::min(fonts.semibold.measure(title_line_.now, 22.0f), 240.0f);
        swap_text(list, fonts.semibold, title_line_.now, title_line_.before, title_line_.in.value,
                  cx - 38.0f - width, cy + 8.0f, 22.0f, ink, 240.0f, calm_);
        room -= width + 38.0f + 48.0f + 20.0f;
    }
    kicker_fit(list, fonts, title_, tx, panel.y + 46.0f, accent_, room);
    ui::text_fit(list, fonts.regular, subtitle_, tx, panel.y + 73.0f, 20.0f, room,
                 look::kInk.with_alpha(look::kMuted));
    look::rule(list, panel.x + kPad, panel.y + kRule, panel.w - 2.0f * kPad);
}

// Tags for the themes on record, wrapped into at most `rows` lines; they pop
// in one after another. Returns the bottom of what was drawn.
float PuzzleScene::draw_tags(gfx::DrawList &list, const ui::Fonts &fonts, float x, float y,
                             float width, int rows) const
{
    float cx = x;
    float cy = y;
    int row = 0;
    for (std::size_t i = 0; i < tags_.size(); ++i)
    {
        const float w = tag_width(fonts, tags_[i]);
        if (cx + w > x + width && cx > x)
        {
            if (++row >= rows)
                break;
            cx = x;
            cy += kTagHeight + 10.0f;
        }
        const float shown = calm_ ? 1.0f
                                  : tween::back_out(tween::clamp01(
                                        (tags_age_ - 0.06f * static_cast<float>(i)) / 0.32f));
        tag(list, fonts, tags_[i], {cx, cy, w, kTagHeight}, accent_, shown);
        cx += w + 12.0f;
    }
    return cy + kTagHeight;
}

void PuzzleScene::draw_classic(const app::Context &ctx, ui::Canvas &canvas) const
{
    gfx::DrawList &list = canvas.list;
    const ui::Fonts &fonts = *ctx.fonts;
    const Color faint = look::kInk.with_alpha(look::kFaint);
    const float turn = turn_.value;
    const auto part = [&](int index, const auto &paint)
    {
        const float in = arrive(index);
        list.push_opacity(in);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in));
        paint();
        list.pop_transform();
        list.pop_opacity();
    };

    // ---- who is to play ----
    part(1,
         [&]()
         {
             look::panel(list, kHead, 0.2f + 0.6f * turn, accent_);
             draw_heading(ctx, list, kHead, false);
             const float cx = kHead.x + kPad + 48.0f;
             const float cy = kHead.y + kRule + 76.0f;
             look::halo(list, {cx - 90.0f, cy - 90.0f, 180.0f, 180.0f}, accent_,
                        0.1f + 0.14f * turn);
             list.circle(cx, cy, 48.0f,
                         gfx::mix(look::kInk.with_alpha(0.08f), accent_.with_alpha(0.2f), turn));
             list.ring(cx, cy, 48.0f, 2.5f, accent_.with_alpha(0.3f + 0.6f * turn));
             ctx.pieces->draw(list, {puzzle_.solver(), chess::Role::king},
                              {cx - 36.0f, cy - 36.0f, 72.0f, 72.0f});
             const float tx = cx + 48.0f + 26.0f;
             const float room = kHead.x + kHead.w - kPad - tx;
             swap_text(list, fonts.display, title_line_.now, title_line_.before,
                       title_line_.in.value, tx - 3.0f, cy + 8.0f, 56.0f, look::kInk, room, calm_);
             swap_text(list, fonts.regular, task_line_.now, task_line_.before, task_line_.in.value,
                       tx, cy + 46.0f, 24.0f, look::kInk.with_alpha(look::kMuted), room, calm_);
         });

    // ---- how the last move went ----
    part(2,
         [&]()
         {
             const Color ink = strip_ink_.value();
             const float flash = tween::clamp01(strip_flash_.value);
             const bool verdict = strip_mark_ == Mark::good || strip_mark_ == Mark::bad;
             if (flash > 0.01f)
                 list.glow(kStrip, look::kRadius, 26.0f, ink.with_alpha(0.45f * flash));
             look::panel(list, kStrip, verdict ? 1.0f : 0.25f + 0.45f * turn, ink);
             if (verdict)
                 list.gradient_rect_h(kStrip.inset(1.5f), look::kRadius - 1.5f,
                                      ink.with_alpha(0.16f + 0.14f * flash), ink.with_alpha(0.02f));

             // The mark: the old one fades as the new one pops.
             const float t = tween::clamp01(strip_line_.in.value);
             const float mx = kStrip.x + kPad + 20.0f;
             const float cy = kStrip.cy();
             const auto mark = [&](Mark which, float alpha, float scale)
             {
                 if (alpha <= 0.01f)
                     return;
                 list.push_opacity(alpha);
                 switch (which)
                 {
                 case Mark::quiet:
                     list.ring(mx, cy, 16.0f * scale, 3.0f, look::kInk.with_alpha(0.45f));
                     list.circle(mx, cy, 5.0f * scale, look::kInk.with_alpha(look::kMuted));
                     break;
                 case Mark::live:
                     list.circle(mx, cy, 18.0f * scale, accent_.with_alpha(0.14f));
                     look::live_dot(list, mx, cy, 7.0f * scale, accent_, ctx.time, calm_);
                     break;
                 case Mark::good:
                 case Mark::bad:
                     app::draw_verdict(list, mx, cy, 20.0f * scale, which == Mark::good,
                                       which == Mark::good ? look::kGood : look::kBad,
                                       look::kNight);
                     break;
                 }
                 list.pop_opacity();
             };
             if (calm_ || strip_mark_ == strip_mark_before_)
             {
                 mark(strip_mark_, 1.0f, 1.0f);
             }
             else
             {
                 mark(strip_mark_before_, 1.0f - tween::clamp01(t * 3.0f), 1.0f);
                 mark(strip_mark_, tween::clamp01(t * 3.0f - 0.6f),
                      verdict ? tween::back_out(tween::clamp01(t * 1.5f - 0.2f))
                              : 0.7f + 0.3f * tween::cubic_out(t));
             }

             const float tx = mx + 20.0f + 18.0f;
             const float right = kStrip.x + kStrip.w - kPad;
             swap_text(list, fonts.semibold, strip_line_.now, strip_line_.before, t, tx,
                       cy + 28.0f * 0.35f, 28.0f,
                       verdict ? gfx::mix(ink, look::kInk, 0.25f) : look::kInk, right - 96.0f - tx,
                       calm_);
             // The time this puzzle has taken, ticking while it is being solved.
             look::ticker(list, fonts, minutes(puzzle_time_), right, cy + 22.0f * 0.35f, 22.0f,
                          look::kInk.with_alpha(tween::lerp(look::kFaint, look::kMuted, turn)),
                          Align::right);
         });

    // ---- what is known about the puzzle, as figures ----
    part(3,
         [&]()
         {
             look::panel(list, kFacts, 0.0f, accent_);
             const bool rated = source_->player_rating() > 0;
             const int cells = (puzzle_.rating > 0 ? 3 : 2) + (rated ? 1 : 0);
             const float cell = (kWide - 2.0f * kPad) / static_cast<float>(cells);
             const float top = kFacts.y + 50.0f;
             float x = kFacts.x + kPad;
             const auto next = [&]()
             {
                 x += cell;
                 list.rounded_rect({x - 20.0f, kFacts.y + 32.0f, 1.5f, kFacts.h - 64.0f}, 0.0f,
                                   look::kInk.with_alpha(0.09f));
             };
             // A fact keeps to its cell, short of the line before the next one.
             const float room = cell - 36.0f;
             fact(list, fonts, tr("Puzzle"),
                  fonts.semibold.font->fit("#" + puzzle_.id, 30.0f, room), x, top, look::kInk, room,
                  Align::left, 30.0f);
             if (puzzle_.rating > 0)
             {
                 next();
                 const float shown = tween::clamp01(rating_reveal_.value);
                 if (shown < 0.5f)
                 {
                     list.push_opacity(1.0f - shown * 2.0f);
                     fact(list, fonts, tr("Rating"), tr("Hidden"), x, top, faint, room, Align::left,
                          30.0f);
                     list.pop_opacity();
                 }
                 else
                 {
                     // It arrives with a small pop and counts to its value.
                     const float pop = calm_ ? 1.0f : tween::back_out(shown * 2.0f - 1.0f);
                     list.push_opacity(shown * 2.0f - 1.0f);
                     list.push_transform(0.8f + 0.2f * pop, x, top + 30.0f, 0.0f, 0.0f);
                     fact(list, fonts, tr("Rating"), number(puzzle_rating_.value), x, top,
                          look::kInk, room);
                     list.pop_transform();
                     list.pop_opacity();
                 }
             }
             if (rated)
             {
                 next();
                 const float width = fact(list, fonts, tr("Your rating"),
                                          number(player_rating_.value), x, top, accent_, room);
                 const float pop = 1.0f + 0.3f * delta_pop_.value;
                 list.push_transform(pop, x + width + 14.0f, top + 30.0f, 0.0f, 0.0f);
                 look::delta(list, fonts, source_->rating_change(), x + width + 14.0f, top + 38.0f,
                             20.0f);
                 list.pop_transform();
             }
             next();
             fact(list, fonts, tr("Solved here"), number(solved_shown_.value), x, top, look::kInk,
                  room);
         });

    // ---- its themes, once they give nothing away, and what Cross and Square do ----
    part(4,
         [&]()
         {
             look::panel(list, kThemes, 0.0f, accent_);
             const float label =
                 look::kicker(list, fonts, tr("Themes"), kThemes.x + kPad, kThemes.y + 48.0f,
                              tags_.empty() ? faint : accent_, Align::left, 14.0f);
             if (!tags_.empty())
             {
                 draw_tags(list, fonts, kThemes.x + kPad, kThemes.y + 68.0f, kWide - 2.0f * kPad,
                           2);
             }
             else
             {
                 // Not yet: "Fork" would be half the answer. Empty tags hold their place.
                 kicker_fit(list, fonts, tr("Shown when the puzzle is solved"),
                            kThemes.x + kThemes.w - kPad, kThemes.y + 48.0f, faint,
                            kWide - 2.0f * kPad - label - 28.0f, Align::right, 13.0f);
                 constexpr float kGhosts[] = {148.0f, 112.0f, 92.0f, 136.0f, 84.0f, 120.0f, 104.0f};
                 float x = kThemes.x + kPad;
                 float y = kThemes.y + 68.0f;
                 for (int i = 0; i < 7; ++i)
                 {
                     if (i == 5)
                     {
                         x = kThemes.x + kPad;
                         y += kTagHeight + 10.0f;
                     }
                     const Rect ghost{x, y, kGhosts[i], kTagHeight};
                     list.rounded_rect(ghost, kTagHeight * 0.5f, look::kInk.with_alpha(0.035f));
                     list.bordered_rect(ghost, kTagHeight * 0.5f, look::kClear, 1.5f,
                                        look::kInk.with_alpha(0.09f));
                     x += kGhosts[i] + 12.0f;
                 }
             }
             play_bar_.draw(canvas);
             done_bar_.draw(canvas);
         });
}

// The puzzle before the one on the board: how it went, and its themes.
void PuzzleScene::draw_last_puzzle(const app::Context &ctx, gfx::DrawList &list, const Rect &panel,
                                   float top) const
{
    const ui::Fonts &fonts = *ctx.fonts;
    const Color faint = look::kInk.with_alpha(look::kFaint);
    const float x = panel.x + kPad;
    const float width = panel.w - 2.0f * kPad;
    const float label = look::kicker(list, fonts, tr("Last puzzle"), x, top + 26.0f,
                                     has_last_ ? accent_ : faint, Align::left, 14.0f);
    if (has_last_)
    {
        // {0}: its theme, {1}: its rating, {2}: the seconds it took.
        const std::string rating = std::to_string(last_.rating);
        const std::string words =
            last_.outcome == 1
                ? fill(tr("{0} \xC2\xB7 rated {1} \xC2\xB7 solved in {2} s"),
                       {last_.theme, rating,
                        std::to_string(std::max(1, static_cast<int>(std::lround(last_.seconds))))})
                : fill(last_.outcome == 2 ? tr("{0} \xC2\xB7 rated {1} \xC2\xB7 skipped")
                                          : tr("{0} \xC2\xB7 rated {1} \xC2\xB7 wrong move"),
                       {last_.theme, rating});
        const Color ink = last_.outcome == 1   ? look::kGood
                          : last_.outcome == 2 ? look::kInk.with_alpha(look::kMuted)
                                               : look::kBad;
        kicker_fit(list, fonts, words, x + width, top + 26.0f, ink, width - label - 28.0f,
                   Align::right, 13.0f);
    }
    if (!tags_.empty())
        draw_tags(list, fonts, x, top + 42.0f, width, 1);
    else
        ui::text_fit(list, fonts.regular, tr("Its themes show here once it is over"), x,
                     top + 70.0f, 22.0f, width, faint);
}

void PuzzleScene::draw_storm(const app::Context &ctx, ui::Canvas &canvas) const
{
    gfx::DrawList &list = canvas.list;
    const ui::Fonts &fonts = *ctx.fonts;
    const Color faint = look::kInk.with_alpha(look::kFaint);
    const float turn = turn_.value;
    const auto part = [&](int index, const auto &paint)
    {
        const float in = arrive(index);
        list.push_opacity(in);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in));
        paint();
        list.pop_transform();
        list.pop_opacity();
    };

    // ---- the clock and the score ----
    part(1,
         [&]()
         {
             const bool low = clock_ < 10.0f;
             look::panel(list, kRunHead, 0.2f + 0.5f * turn, low ? look::kBad : accent_);
             draw_heading(ctx, list, kRunHead, true);
             const float top = kRunHead.y + kRule;

             // The clock: a ring that drains, with the time ticking inside it.
             const float cx = kRunHead.x + kPad + kClock;
             const float cy = top + 16.0f + kClock;
             const float flash = tween::clamp01(bonus_flash_);
             const Color ring = low ? look::kBad : accent_;
             const Color bonus = bonus_seconds_ > 0 ? look::kGood : look::kBad;
             const float breath = calm_ ? 0.5f : ui::breathe(ctx.time, low ? 1.0f : 2.4f);
             list.glow({cx - kClock, cy - kClock, 2.0f * kClock, 2.0f * kClock}, kClock, 22.0f,
                       gfx::mix(ring, bonus, flash)
                           .with_alpha((low ? 0.22f + 0.2f * breath : 0.08f + 0.06f * breath) +
                                       0.35f * flash));
             look::gauge(list, cx, cy, kClock, 14.0f, ring_.value, gfx::mix(ring, bonus, flash));
             const std::string time = app::format_clock(static_cast<long long>(clock_ * 1000.0f));
             look::ticker(list, fonts, time, cx, cy + (low ? 14.0f : 18.0f), low ? 40.0f : 54.0f,
                          low ? look::kBad : look::kInk, Align::center);
             // Inside the ring, under the time.
             kicker_fit(list, fonts, trc("time", "Left"), cx, cy + 52.0f, faint, 124.0f,
                        Align::center, 13.0f);
             if (bonus_flash_ > 0.0f)
             {
                 // The seconds won or lost pop over the time, rise and fade.
                 const std::string text =
                     bonus_seconds_ > 0
                         ? fill(tr("+{0} s"), {std::to_string(bonus_seconds_)})
                         : fill(tr("\xE2\x80\x93{0} s"), {std::to_string(-bonus_seconds_)});
                 const float age = 1.2f - bonus_flash_;
                 const float pop = calm_ ? 1.0f : tween::back_out(tween::clamp01(age * 5.0f));
                 const float y = cy - 44.0f - (calm_ ? 0.0f : 18.0f * age);
                 list.push_opacity(std::min(1.0f, bonus_flash_ * 2.0f));
                 list.push_transform(0.6f + 0.4f * pop, cx, y - 8.0f, 0.0f, 0.0f);
                 look::figure(list, fonts, text, cx, y, 26.0f, bonus, Align::center);
                 list.pop_transform();
                 list.pop_opacity();
             }

             // The score: a large figure that counts, and gives a little as it grows.
             const float sx = cx + kClock + 44.0f;
             // The column at the right edge keeps this much; the score has the rest.
             constexpr float kSide = 220.0f;
             const float rx = kRunHead.x + kRunHead.w - kPad;
             kicker_fit(list, fonts, tr("Solved"), sx, top + 44.0f, accent_,
                        rx - kSide - 24.0f - sx, Align::left, 14.0f);
             list.push_transform(1.0f + 0.1f * score_pop_.value, sx, top + 120.0f, 0.0f, 0.0f);
             ui::text(list, fonts.display, number(score_shown_.value), sx - 6.0f, top + 158.0f,
                      124.0f, look::kInk);
             list.pop_transform();

             fact(list, fonts, tr("Best"), std::to_string(records_.best_storm), rx, top + 44.0f,
                  look::kInk, kSide, Align::right);
             fact(list, fonts, mistakes_ == 1 ? tr("Mistake") : tr("Mistakes"),
                  std::to_string(mistakes_), rx, top + 134.0f,
                  mistakes_ > 0 ? look::kBad : look::kInk, kSide, Align::right);
         });

    // ---- the combo: a level toward the next seconds, glowing while it runs ----
    part(2,
         [&]()
         {
             const float heat = heat_.value;
             const float flash = tween::clamp01(level_flash_.value);
             if (flash > 0.01f)
                 list.glow(kCombo, look::kRadius, 24.0f, look::kGood.with_alpha(0.35f * flash));
             look::panel(list, kCombo, std::max(0.75f * heat, flash),
                         gfx::mix(accent_, look::kGood, flash));
             const float baseline = kCombo.y + 54.0f;
             const float label = look::kicker(list, fonts, tr("Combo"), kCombo.x + kPad,
                                              baseline - 3.0f, accent_, Align::left, 14.0f);
             const float count =
                 look::figure(list, fonts, std::to_string(combo_), kCombo.x + kPad + label + 14.0f,
                              baseline + 3.0f, 34.0f, look::kInk);
             // What the combo is filling towards: the seconds, and the combo that wins them.
             for (int i = 0; i < kStormComboCount; ++i)
             {
                 if (combo_ < kStormComboSteps[i])
                 {
                     ui::text_fit(list, fonts.regular,
                                  fill(tr("+{0} s at {1}"), {std::to_string(kStormComboBonus[i]),
                                                             std::to_string(kStormComboSteps[i])}),
                                  kCombo.x + kCombo.w - kPad, baseline, 21.0f,
                                  kCombo.w - 2.0f * kPad - label - 14.0f - count - 24.0f,
                                  look::kInk.with_alpha(look::kMuted), Align::right);
                     break;
                 }
             }
             const Rect bar{kCombo.x + kPad, kCombo.y + 78.0f, kCombo.w - 2.0f * kPad, 14.0f};
             const float share = tween::clamp01(level_.value);
             const float breath = calm_ ? 0.5f : ui::breathe(ctx.time, 1.6f);
             if (heat > 0.01f && share > 0.0f)
                 list.glow({bar.x, bar.y, std::max(bar.w * share, bar.h), bar.h}, 7.0f,
                           12.0f + 10.0f * flash,
                           accent_.with_alpha(heat * (0.3f + 0.2f * breath) + 0.4f * flash));
             look::level(list, bar, share, gfx::mix(accent_, look::kInk, 0.45f * flash));
         });

    // ---- the run: every puzzle a mark, and the last one in words ----
    part(3,
         [&]()
         {
             look::panel(list, kRun, 0.0f, accent_);
             const float x = kRun.x + kPad;
             const float label = look::kicker(list, fonts, tr("This run"), x, kRun.y + 46.0f,
                                              marks_.empty() ? faint : accent_, Align::left, 14.0f);
             if (marks_.empty())
             {
                 ui::text_fit(list, fonts.regular,
                              tr("Solve as many as you can before the clock runs out"), x,
                              kRun.y + 88.0f, 22.0f, kRun.w - 2.0f * kPad, faint);
             }
             else
             {
                 int solved = 0;
                 for (const std::uint8_t outcome : marks_)
                     solved += outcome == 1 ? 1 : 0;
                 kicker_fit(list, fonts,
                            fill(tr("{0} solved \xC2\xB7 {1} missed"),
                                 {std::to_string(solved),
                                  std::to_string(static_cast<int>(marks_.size()) - solved)}),
                            kRun.x + kRun.w - kPad, kRun.y + 46.0f, faint,
                            kRun.w - 2.0f * kPad - label - 28.0f, Align::right, 13.0f);
                 // The newest marks, as many as two rows hold.
                 constexpr float kSize = 28.0f;
                 const float pitch =
                     (kRun.w - 2.0f * kPad - kSize) / static_cast<float>(kMarksPerRow - 1);
                 const int total = static_cast<int>(marks_.size());
                 const int shown = std::min(total, kMarksPerRow * kMarkRows);
                 for (int i = 0; i < shown; ++i)
                 {
                     const int at = total - shown + i;
                     const float cx =
                         x + kSize * 0.5f + static_cast<float>(i % kMarksPerRow) * pitch;
                     const float cy = kRun.y + 60.0f + kSize * 0.5f +
                                      static_cast<float>(i / kMarksPerRow) * 36.0f;
                     const float pop = at == total - 1 && !calm_
                                           ? tween::back_out(tween::clamp01(mark_age_ / 0.3f))
                                           : 1.0f;
                     if (pop > 0.01f)
                         run_mark(list, marks_[static_cast<std::size_t>(at)], cx, cy,
                                  kSize * 0.5f * pop);
                 }
             }
             draw_last_puzzle(ctx, list, kRun, kRun.y + 136.0f);
             run_bar_.draw(canvas);
         });
}

void PuzzleScene::draw_streak(const app::Context &ctx, ui::Canvas &canvas) const
{
    gfx::DrawList &list = canvas.list;
    const ui::Fonts &fonts = *ctx.fonts;
    const Color faint = look::kInk.with_alpha(look::kFaint);
    const float turn = turn_.value;
    const int best = records_.best_streak;
    // Past the record the run is gold.
    const bool beyond = best > 0 && score_ > best;
    const auto part = [&](int index, const auto &paint)
    {
        const float in = arrive(index);
        list.push_opacity(in);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in));
        paint();
        list.pop_transform();
        list.pop_opacity();
    };

    // ---- the run, the puzzle it has reached and the record ----
    part(1,
         [&]()
         {
             look::panel(list, kRunHead, 0.2f + 0.5f * turn, accent_);
             draw_heading(ctx, list, kRunHead, true);
             const float top = kRunHead.y + kRule;
             const float x = kRunHead.x + kPad;
             const float fx = x + 280.0f;
             kicker_fit(list, fonts, tr("In a row"), x, top + 44.0f, accent_, fx - 24.0f - x,
                        Align::left, 14.0f);
             list.push_transform(1.0f + 0.1f * score_pop_.value, x, top + 120.0f, 0.0f, 0.0f);
             ui::text(list, fonts.display, number(score_shown_.value), x - 6.0f, top + 158.0f,
                      124.0f, beyond ? look::kGold : look::kInk);
             list.pop_transform();

             // The facts stand between the score and the record's ring, clear of both.
             const float cx = kRunHead.x + kRunHead.w - kPad - kBest;
             const float cy = top + 16.0f + kBest;
             const float room = cx - kBest - 24.0f - fx;
             fact(list, fonts, tr("Puzzle rating"), number(puzzle_rating_.value), fx, top + 44.0f,
                  look::kInk, room);
             fact(list, fonts, trc("skips left", "Skip"), skips_ > 0 ? tr("1 left") : tr("Used"),
                  fx, top + 134.0f, skips_ > 0 ? look::kInk : faint, room);

             // The record: a ring the run closes, with the number to beat inside it.
             const Color ring = beyond ? look::kGold : accent_;
             if (beyond)
                 list.glow({cx - kBest, cy - kBest, 2.0f * kBest, 2.0f * kBest}, kBest, 22.0f,
                           look::kGold.with_alpha(0.16f +
                                                  (calm_ ? 0.06f : 0.12f * ui::breathe(ctx.time))));
             look::gauge(list, cx, cy, kBest, 12.0f, best_share_.value, ring);
             kicker_fit(list, fonts, tr("Best"), cx, cy - 30.0f, faint, 120.0f, Align::center,
                        13.0f);
             ui::text(list, fonts.display, std::to_string(std::max(best, score_)), cx, cy + 32.0f,
                      60.0f, beyond ? look::kGold : look::kInk, Align::center);
         });

    // ---- the climb: every solved puzzle is a step, each one higher ----
    part(2,
         [&]()
         {
             look::panel(list, kClimb, 0.0f, accent_);
             const float label = look::kicker(list, fonts, tr("The climb"), kClimb.x + kPad,
                                              kClimb.y + 46.0f, accent_, Align::left, 14.0f);
             kicker_fit(list, fonts, tr("One mistake ends the run"), kClimb.x + kClimb.w - kPad,
                        kClimb.y + 46.0f, faint, kClimb.w - 2.0f * kPad - label - 28.0f,
                        Align::right, 13.0f);
             const Rect chart{kClimb.x + kPad, kClimb.y + 90.0f, kClimb.w - 2.0f * kPad, 114.0f};
             constexpr float kGap = 6.0f;
             const float width =
                 (chart.w - kGap * static_cast<float>(kSteps - 1)) / static_cast<float>(kSteps);
             // A long run slides: the step being climbed stays in sight.
             const int first = std::max(0, score_ - (kSteps - 6));
             for (int k = 0; k < kSteps; ++k)
             {
                 const int step = first + k;
                 const float grown = calm_ ? 1.0f : look::rise(since_, 6 + k, 0.02f, 0.5f);
                 float h =
                     chart.h *
                     (0.14f + 0.86f * static_cast<float>(k + 1) / static_cast<float>(kSteps)) *
                     grown;
                 if (step == score_ - 1 && !calm_)
                     h *= 0.4f + 0.6f * tween::back_out(tween::clamp01(step_age_ / 0.35f));
                 const Rect r{chart.x + static_cast<float>(k) * (width + kGap),
                              chart.y + chart.h - h, width, std::max(h, 2.0f)};
                 // Steps beyond the record are gold.
                 const Color ink = best > 0 && step >= best ? look::kGold : accent_;
                 if (step < score_)
                 {
                     if (step == score_ - 1)
                         list.glow(r, 5.0f, 12.0f, ink.with_alpha(0.45f));
                     list.rounded_rect(r, 5.0f, ink.with_alpha(step == score_ - 1 ? 1.0f : 0.6f));
                 }
                 else if (step == score_)
                 {
                     // The puzzle on the board: the step still to be taken.
                     const float breath = calm_ ? 0.5f : ui::breathe(ctx.time, 1.8f);
                     list.bordered_rect(r, 5.0f, ink.with_alpha(0.1f + 0.1f * breath), 2.0f,
                                        ink.with_alpha(0.55f + 0.4f * breath));
                 }
                 else
                 {
                     list.rounded_rect(r, 5.0f, look::kInk.with_alpha(0.08f));
                 }
                 if (best > 0 && step == best - 1)
                 {
                     // Where the record stands.
                     list.rounded_rect({r.cx() - 1.0f, r.y - 16.0f, 2.0f, 12.0f}, 1.0f,
                                       look::kGold.with_alpha(0.8f * grown));
                     list.circle(r.cx(), r.y - 18.0f, 4.0f, look::kGold.with_alpha(grown));
                 }
             }
         });

    // ---- the puzzle before, and the shortcuts ----
    part(3,
         [&]()
         {
             look::panel(list, kLast, 0.0f, accent_);
             draw_last_puzzle(ctx, list, kLast, kLast.y + 20.0f);
             run_bar_.draw(canvas);
         });
}

void PuzzleScene::draw_loading(const app::Context &ctx, ui::Canvas &canvas) const
{
    gfx::DrawList &list = canvas.list;
    const ui::Fonts &fonts = *ctx.fonts;
    const bool classic = rules_ == PuzzleRules::classic;
    const bool storm = rules_ == PuzzleRules::storm;
    const Rect head = classic ? kHead : kRunHead;
    const auto part = [&](int index, const auto &paint)
    {
        const float in = arrive(index);
        list.push_opacity(in);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in));
        paint();
        list.pop_transform();
        list.pop_opacity();
    };
    part(1,
         [&]()
         {
             look::panel(list, head, 0.2f, accent_);
             draw_heading(ctx, list, head, false);
             // "Loading", with the spinner before it, where the side to move goes.
             ui::Spinner busy = busy_;
             busy.set_bounds({head.x + kPad, head.y + kRule + 26.0f, 34.0f, 34.0f});
             busy.draw(canvas);
             ui::text_fit(list, fonts.semibold, tr("Loading"), head.x + kPad + 50.0f,
                          head.y + kRule + 52.0f, 26.0f, head.w - 2.0f * kPad - 50.0f,
                          look::kInk.with_alpha(look::kMuted));
             bones_head_.draw(canvas);
         });
    if (classic)
        part(2, [&]() { look::panel(list, kStrip, 0.0f, accent_); });
    part(classic ? 3 : 2,
         [&]()
         {
             look::panel(list, classic ? kFacts : storm ? kCombo : kClimb, 0.0f, accent_);
             bones_facts_.draw(canvas);
         });
    part(classic ? 4 : 3,
         [&]()
         {
             look::panel(list, classic ? kThemes : storm ? kRun : kLast, 0.0f, accent_);
             bones_foot_.draw(canvas);
         });
}

void PuzzleScene::draw_error(const app::Context &ctx, ui::Canvas &canvas) const
{
    gfx::DrawList &list = canvas.list;
    const ui::Fonts &fonts = *ctx.fonts;
    const float in = arrive(0);
    list.push_opacity(in);
    list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in));
    look::panel(list, kWhole, 0.25f, accent_);
    const Rect tile{kWhole.x + kPad, kWhole.y + 26.0f, 48.0f, 48.0f};
    draw_puzzle_badge(list, kind_, tile, 1.0f);
    kicker_fit(list, fonts, title_, tile.x + tile.w + 16.0f, tile.cy() + 6.0f, accent_,
               kWhole.x + kWhole.w - kPad - (tile.x + tile.w + 16.0f));
    empty_.draw(canvas);
    // The answers sit under the words, centred as a pair.
    const float y = empty_.bounds().y + empty_.bounds().h + 16.0f;
    if (exhausted_)
    {
        ui::PushButton back = leave_;
        back.set_bounds({kWhole.cx() - 148.0f, y, 296.0f, 64.0f});
        back.draw(canvas);
    }
    else
    {
        ui::PushButton again = retry_;
        again.set_bounds({kWhole.cx() - 256.0f - 8.0f, y, 256.0f, 64.0f});
        again.draw(canvas);
        ui::PushButton back = leave_;
        back.set_bounds({kWhole.cx() + 8.0f, y, 256.0f, 64.0f});
        back.draw(canvas);
    }
    if (choice_placed_)
    {
        Rect ring = choice_ring_.value();
        ring.x += ui::shake(choice_nudge_.value, ctx.time, 8.0f);
        look::ring(list, ring, look::kInk, 1.0f, 14.0f);
    }
    list.pop_transform();
    list.pop_opacity();
}

// The end of a run, in the room the dialog's empty body lines keep free: the
// score counts inside a ring that closes on the record. A new record is gold:
// the ring, its rays, the star on it and the light around the panel.
void PuzzleScene::draw_result(const app::Context &ctx, ui::Canvas &canvas) const
{
    if (figures_.value <= 0.01f)
        return;
    gfx::DrawList &list = canvas.list;
    const ui::Fonts &fonts = *ctx.fonts;
    const Rect panel = result_.panel_rect(canvas);
    const float bottom = panel.y + panel.h - result_.style.padding - result_.style.button_height -
                         result_.style.padding * 0.8f;
    const float top = bottom - static_cast<float>(kResultLines) * result_.style.body_size *
                                   result_.style.body_line;
    const float shown = tween::clamp01(figures_.value);
    const bool storm = rules_ == PuzzleRules::storm;
    const Color ink = new_record_ ? look::kGold : accent_;
    const float cx = panel.cx();
    const float cy = top + 118.0f;
    constexpr float kRing = 80.0f;

    if (new_record_)
    {
        const float breath = calm_ ? 0.5f : ui::breathe(ctx.time);
        list.glow(panel, look::kRadius, 36.0f,
                  look::kGold.with_alpha((0.14f + 0.1f * breath) * shown));
        look::ring(list, panel.inset(5.0f), look::kGold, 0.7f * shown, look::kRadius - 5.0f);
    }

    list.push_opacity(shown);
    list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, calm_ ? 0.0f : 12.0f * (1.0f - shown));
    const float closed = tween::clamp01(final_share_.value);
    if (new_record_)
    {
        // Rays around the ring, growing as it closes and turning slowly.
        constexpr int kRays = 28;
        const float turned = calm_ ? 0.0f : over_age_ * 0.12f;
        for (int i = 0; i < kRays; ++i)
        {
            const float angle = turned + kTau * static_cast<float>(i) / static_cast<float>(kRays);
            const float reach = (i % 2 == 0 ? 16.0f : 9.0f) * closed;
            const float sx = std::sin(angle);
            const float sy = -std::cos(angle);
            list.line(cx + sx * (kRing + 12.0f), cy + sy * (kRing + 12.0f),
                      cx + sx * (kRing + 12.0f + reach), cy + sy * (kRing + 12.0f + reach), 2.5f,
                      look::kGold.with_alpha(0.75f * closed));
        }
        look::halo(list, {cx - 150.0f, cy - 150.0f, 300.0f, 300.0f}, look::kGold, 0.16f * closed);
    }
    look::gauge(list, cx, cy, kRing, 10.0f, closed, ink);
    const std::string score = number(final_shown_.value);
    ui::text(list, fonts.display, score, cx, cy + 22.0f, score.size() > 2 ? 60.0f : 84.0f,
             new_record_ ? look::kGold : look::kInk, Align::center);
    // Inside the ring, under the score.
    kicker_fit(list, fonts, storm ? tr("Solved") : tr("In a row"), cx, cy + 50.0f,
               look::kInk.with_alpha(look::kMuted), 94.0f, Align::center, 13.0f);
    if (new_record_)
    {
        // The star lands on the ring once it has closed.
        const float pop = calm_ ? 1.0f : tween::back_out(tween::clamp01((closed - 0.8f) * 5.0f));
        if (pop > 0.01f)
        {
            list.circle(cx, cy - kRing + 5.0f, 17.0f * pop, look::kNight);
            list.star(cx, cy - kRing + 5.0f, 15.0f * pop, look::kGold);
        }
        const float tag_pop =
            calm_ ? 1.0f : tween::back_out(tween::clamp01((over_age_ - 0.9f) * 3.0f));
        if (tag_pop > 0.01f)
        {
            list.push_transform(0.6f + 0.4f * tag_pop, cx, top + 234.0f, 0.0f, 0.0f);
            look::tag(list, fonts, tr("New record"), cx, top + 234.0f, look::kGold, look::kNight,
                      Align::center, 16.0f);
            list.pop_transform();
        }
    }
    else
    {
        const int best = storm ? records_.best_storm : records_.best_streak;
        ui::text_fit(list, fonts.regular, fill(tr("Best {0}"), {std::to_string(best)}), cx,
                     top + 242.0f, 24.0f, panel.w - 2.0f * result_.style.padding,
                     look::kInk.with_alpha(look::kMuted), Align::center);
    }
    list.pop_transform();
    list.pop_opacity();
}

void PuzzleScene::draw(app::Context &ctx, app::Frame &frame) const
{
    gfx::DrawList &list = frame.scene;
    ui::Canvas canvas = app::canvas_for(ctx, list);
    const bool modal = pause_.is_open() || phase_ == Phase::run_over;

    if (phase_ == Phase::error)
    {
        draw_error(ctx, canvas);
    }
    else if (phase_ == Phase::loading)
    {
        // A board in waiting: its squares, dim, until the position arrives.
        const board::BoardTheme &wood = ctx.board_theme();
        const Rect frame = kBoard.inset(-16.0f);
        look::frame_board(list, kBoard, accent_, 0.3f * arrive(0));
        list.push_opacity(0.34f + (calm_ ? 0.0f : 0.08f * ui::breathe(ctx.time, 1.8f)));
        list.gradient_rect(frame, 14.0f, Color::rgb(wood.frame_top), Color::rgb(wood.frame_bottom));
        list.board(kBoard, Color::rgb(wood.light), Color::rgb(wood.dark), wood.style,
                   static_cast<float>(wood.light % 97));
        list.pop_opacity();
        draw_loading(ctx, canvas);
    }
    else
    {
        draw_board(ctx, list);
        if (rules_ == PuzzleRules::classic)
            draw_classic(ctx, canvas);
        else if (rules_ == PuzzleRules::storm)
            draw_storm(ctx, canvas);
        else
            draw_streak(ctx, canvas);
    }

    if (!modal)
    {
        if (phase_ == Phase::player)
        {
            // Four fit beside the board; L1 and R1 (next piece) are in the manual.
            const ui::Hint hints[] = {{ui::Button::dpad, TR("Move")},
                                      {ui::Button::cross, TR("Place")},
                                      {ui::Button::circle, TR("Back")},
                                      {ui::Button::options, TR("Menu")}};
            app::draw_hints(ctx, list, hints, 4);
        }
        else if (phase_ == Phase::error)
        {
            const ui::Hint hints[] = {{ui::Button::cross, TR("Select")},
                                      {ui::Button::circle, TR("Back")}};
            app::draw_hints(ctx, list, hints, 2);
        }
        else
        {
            const ui::Hint hints[] = {{ui::Button::circle, TR("Back")},
                                      {ui::Button::options, TR("Menu")}};
            app::draw_hints(ctx, list, hints, 2);
        }
    }

    // ---- above the frosted copy: the end of a run, the menu ----
    frame.glass = pause_.visible() || result_.visible();
    ui::Canvas over = app::canvas_for(ctx, frame.overlay, frame.glass ? frame.glass_texture : 0);
    result_.draw(over);
    if (result_.visible())
        draw_result(ctx, over);
    confetti_.draw(frame.overlay);
    pause_.draw(over);
    if (pause_.is_open())
    {
        const ui::Hint hints[] = {{ui::Button::cross, TR("Select")},
                                  {ui::Button::circle, TR("Resume")}};
        app::draw_hints(ctx, frame.overlay, hints, 2, true);
    }
    else if (phase_ == Phase::run_over)
    {
        const ui::Hint hints[] = {{ui::Button::cross, TR("Select")},
                                  {ui::Button::dpad, TR("Choose")}};
        app::draw_hints(ctx, frame.overlay, hints, 2, true);
    }
}

// ---- how each mode opens ---------------------------------------------------

std::unique_ptr<app::Scene> make_daily_puzzle(app::Context &ctx)
{
    PuzzleSetup setup;
    setup.title = tr("Daily Puzzle");
    setup.subtitle = tr("Today's puzzle on Lichess");
    setup.rules = PuzzleRules::classic;
    setup.kind = PuzzleKind::daily;
    if (ctx.lichess != nullptr)
        setup.source = lichess::make_daily_source(*ctx.lichess);
    else
        setup.source = std::make_unique<NoSource>();
    return std::make_unique<PuzzleScene>(std::move(setup));
}

std::unique_ptr<app::Scene> make_puzzle_training(app::Context &ctx)
{
    PuzzleSetup setup;
    setup.title = tr("Puzzle Training");
    setup.rules = PuzzleRules::classic;
    setup.kind = PuzzleKind::training;
    if (ctx.lichess != nullptr)
    {
        setup.subtitle = ctx.lichess->signed_in() ? tr("Rated on your Lichess account")
                                                  : tr("Sign in to earn a rating");
        setup.source = lichess::make_training_source(*ctx.lichess, "mix");
    }
    else
    {
        setup.source = std::make_unique<NoSource>();
    }
    return std::make_unique<PuzzleScene>(std::move(setup));
}

std::unique_ptr<app::Scene> make_puzzle_streak(app::Context &ctx)
{
    PuzzleSetup setup;
    setup.title = tr("Puzzle Streak");
    setup.subtitle = tr("One mistake ends the run");
    setup.rules = PuzzleRules::streak;
    setup.kind = PuzzleKind::streak;
    setup.source = std::make_unique<puzzles::PackSource>(ctx.pack, -1, 900, 1100, seed_from(ctx));
    return std::make_unique<PuzzleScene>(std::move(setup));
}

std::unique_ptr<app::Scene> make_puzzle_storm(app::Context &ctx)
{
    PuzzleSetup setup;
    setup.title = tr("Puzzle Storm");
    setup.subtitle = tr("Three minutes on the clock");
    setup.rules = PuzzleRules::storm;
    setup.kind = PuzzleKind::storm;
    setup.source = std::make_unique<puzzles::PackSource>(ctx.pack, -1, 700, 900, seed_from(ctx));
    return std::make_unique<PuzzleScene>(std::move(setup));
}

std::unique_ptr<app::Scene> make_puzzle_theme(app::Context &ctx, int theme, int rating)
{
    // The rating picks the band it falls in.
    const PuzzleBand *band = &kPuzzleBands[0];
    for (const PuzzleBand &candidate : kPuzzleBands)
    {
        if (rating >= candidate.min)
            band = &candidate;
    }
    PuzzleSetup setup;
    setup.title = tr("Puzzle Themes");
    const std::string label = ctx.pack != nullptr && theme >= 0 && theme < ctx.pack->theme_count()
                                  ? puzzles::theme_label(ctx.pack->theme_name(theme))
                                  : std::string(tr("Mixed themes"));
    // The theme, then how hard, with a dot between them.
    setup.subtitle = fill(tr("{0} \xC2\xB7 {1}"), {label, puzzle_band_label(*band)});
    setup.rules = PuzzleRules::classic;
    setup.kind = PuzzleKind::themes;
    setup.source =
        std::make_unique<puzzles::PackSource>(ctx.pack, theme, band->min, band->max,
                                              static_cast<std::uint64_t>(ctx.time * 7919.0f) + 17u);
    return std::make_unique<PuzzleScene>(std::move(setup));
}

} // namespace pch::modes
