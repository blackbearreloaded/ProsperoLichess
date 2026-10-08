// ProsperoLichess - Lichess TV: the featured game of a channel, live.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The screen in the house look (app/look.hpp):
//
//   - the board leads, on a rose light that breathes while the game is live;
//   - the column beside it is the game screen's: the two players level with
//     the board's top and bottom edges, the one to move lit;
//   - between them is the broadcast: the station's name with its on-air mark,
//     the channels as tabs (each with its sign, the plate in its colour), the
//     last move as a large figure, White's share of the material as a bar,
//     and the moves played since tuning in as a row of marks that flows to the
//     left with the newest one lit;
//   - a change of channel cross-fades the game with its bones; without a
//     connection the same places stay, still, and the panel says why.

#include "app/chrome.hpp"
#include "board/board_view.hpp"
#include "core/strings.hpp"
#include "lichess/session.hpp"
#include "modes/game_widgets.hpp"
#include "modes/scenes.hpp"
#include "modes/tv_feed.hpp"
#include "ui/components/progress.hpp"
#include "ui/components/skeleton.hpp"
#include "ui/components/tabs.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace pch::modes
{

namespace
{

namespace look = app::look;
using gfx::Align;
using gfx::Color;
using gfx::Rect;

// The board and the column beside it, as on the game screen.
constexpr Rect kSquares = app::kBoardSquares;
constexpr float kColumn = app::kBoardColumn;
constexpr float kColumnWidth = app::kRight - kColumn;
constexpr Rect kTopPlayer{kColumn, app::kStage, kColumnWidth, 132.0f};
constexpr Rect kBottomPlayer{kColumn, 828.0f, kColumnWidth, 132.0f};
// Between the players: the station, its channels and what the feed tells.
constexpr float kTitleLine = 320.0f; // the baseline of "Lichess TV"
constexpr Rect kChannels{kColumn, 344.0f, kColumnWidth, 56.0f};
constexpr Rect kInfo{kColumn, 424.0f, kColumnWidth, 380.0f};
constexpr float kInfoPad = 32.0f;
constexpr float kMarkHeight = 48.0f; // a move of the flowing row
constexpr float kMarkSize = 24.0f;
constexpr float kMarkGap = 10.0f;
constexpr float kTabText = 19.0f;   // a channel's name on its tab
constexpr float kTabPadding = 5.0f; // beside it, inside the tab
// Holding a direction walks the channels; the stream follows once it rests.
constexpr float kSwitchDelay = 0.3f;
constexpr std::size_t kMovesKept = 24;

// A move seen while this screen watched.
struct Mark
{
    std::string san;
    bool white = true;
    float width = 0.0f; // of the whole mark
};

// A spring's step toward its target; under reduced motion the value snaps.
void ease(tween::Spring &spring, float dt, float omega, bool calm)
{
    if (calm)
        spring.snap(spring.target);
    else
        spring.update(dt, omega);
}

// look::kicker inside a width: a translated label shrinks, then ends in an
// ellipsis, instead of running into what stands beside it.
float kicker_fit(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
                 float baseline, float max_width, Color color, Align align = Align::left,
                 float size = 16.0f)
{
    return ui::text_fit(list, fonts.semibold, ui::upper(text), x, baseline, size, max_width, color,
                        align, 3.0f);
}

class TvScene final : public app::Scene
{
  public:
    explicit TvScene(const std::string &channel) : channel_(tv_channel_index(channel))
    {
        std::vector<ui::TabItem> tabs;
        for (const TvChannel &entry : tv_channels())
            tabs.push_back({tr(entry.label)});
        channels_.set_tabs(std::move(tabs));
        channels_.style.kind = ui::TabKind::pill;
        channels_.style.width = ui::TabWidth::fill;
        channels_.style.padding = kTabPadding;
        channels_.style.gap = 2.0f;
        channels_.style.text_size = kTabText;
        channels_.style.glyph_width = 18.0f;
        channels_.style.glyph_gap = 6.0f;
        channels_.style.wrap = true; // past the last channel comes the first
        channels_.set_bounds(kChannels);
        channels_.set_active(channel_, true);
        // The shoulder buttons turn it from anywhere: it never "has" the focus.
        channels_.set_focused(false);
        // Every channel wears its sign, in the colour of its label.
        channels_.glyph = [](ui::Canvas &canvas, const Rect &box, const ui::TabItem &, int index,
                             float, Color ink) {
            draw_tv_channel_sign(canvas.list, index, {box.x, box.cy() - box.w * 0.5f, box.w, box.w},
                                 ink);
        };
        plate_.snap(tv_channel_color(channel_));
        look::tint(tv_channel_color(channel_), channels_);

        const Color rose = look::accent(look::Section::tv);
        reconnecting_.style.kind = ui::SpinnerKind::arc;
        reconnecting_.style.status = ui::Status::neutral;
        top_.set_bounds(kTopPlayer);
        top_.set_accent(rose);
        bottom_.set_bounds(kBottomPlayer);
        bottom_.set_accent(rose);

        board_bones_.style.kind = ui::SkeletonKind::block;
        board_bones_.set_bounds(kSquares);
        player_bones_.style.kind = ui::SkeletonKind::list_row;
        player_bones_.style.rows = 1;
        player_bones_.style.row_height = 84.0f;
        info_bones_.style.kind = ui::SkeletonKind::line;
        info_bones_.style.lines = 5;
        info_bones_.style.line_height = 22.0f;
        info_bones_.style.line_gap = 36.0f;
        info_bones_.set_bounds(kInfo.inset(44.0f));
        arrive_.snap(0.0f);
        last_in_.snap(1.0f);
        share_.snap(0.5f);
    }

    void enter(app::Context &ctx) override
    {
        switch_in_ = 0.0f;
        since_ = 0.0f;
        fit_tabs(ctx);
        tune(ctx);
    }

    app::Transition update(app::Context &ctx, const InputFrame &input, float dt) override
    {
        ctx.calm(channels_, reconnecting_, board_bones_, player_bones_, info_bones_);
        calm_ = ctx.reduced_motion();
        since_ += dt;
        if (switch_in_ > 0.0f)
        {
            switch_in_ -= dt;
            if (switch_in_ <= 0.0f)
                tune(ctx);
        }
        follow(ctx);

        board::Overlay overlay;
        view_.update(dt, overlay);
        swap_.update(dt);

        if (input.is_pressed(Action::back))
        {
            // The stream is left as it is: the screen underneath decides what
            // it wants to watch when it comes back to the front.
            ctx.cue(audio::Cue::back);
            return app::Transition::pop();
        }
        const int turn = input.is_pressed(Action::page_next)   ? 1
                         : input.is_pressed(Action::page_prev) ? -1
                                                               : 0;
        const ui::Event event = turn != 0 ? channels_.step(turn, input, *ctx.feedback)
                                          : channels_.handle(input, *ctx.feedback);
        if (event == ui::Event::changed)
        {
            channel_ = channels_.active();
            switch_in_ = kSwitchDelay;
        }
        if (input.is_pressed(Action::west))
        {
            view_.set_orientation(chess::opposite(view_.orientation()), !ctx.reduced_motion());
            ctx.cue(audio::Cue::toggle);
        }

        describe(ctx);
        stale_ = ctx.lichess != nullptr && ctx.lichess->has_issue("tv");
        offline_now_ = ctx.lichess == nullptr || !ctx.lichess->online();

        // The tabs' plate takes the colour of the channel it glides to.
        plate_.target(tv_channel_color(channel_));
        plate_.update(dt, calm_ ? 60.0f : 10.0f);
        look::tint(plate_.value(), channels_);

        channels_.update(dt);
        reconnecting_.update(dt);
        top_.update(ctx, dt);
        bottom_.update(ctx, dt);
        board_bones_.update(dt);
        player_bones_.update(dt);
        info_bones_.update(dt);
        arrive_.target = on_air_ ? 1.0f : 0.0f;
        ease(arrive_, dt, 12.0f, calm_);
        last_in_.target = 1.0f;
        ease(last_in_, dt, 13.0f, calm_);
        ease(share_, dt, 8.0f, calm_);
        ease(scroll_, dt, 12.0f, calm_);
        mark_in_.update(dt);
        return app::Transition::stay();
    }

    void draw(app::Context &ctx, app::Frame &frame) const override
    {
        gfx::DrawList &list = frame.scene;
        const ui::Theme &theme = ctx.theme();
        const ui::Fonts &fonts = *ctx.fonts;
        ui::Canvas canvas = app::canvas_for(ctx, list);
        const Color rose = look::accent(look::Section::tv);
        const float shown = std::clamp(arrive_.value, 0.0f, 1.0f);
        const bool live = game_.valid && on_air_ && !stale_;

        const auto part = [&](int index, const auto &draw)
        {
            const float in = rise(index);
            list.push_opacity(in);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in, 18.0f));
            draw();
            list.pop_transform();
            list.pop_opacity();
        };

        // ---- the board ----
        part(0,
             [&]()
             {
                 if (shown < 0.99f)
                 {
                     list.push_opacity(1.0f - shown);
                     if (offline_now_)
                         list.bordered_rect(kSquares, 10.0f, look::kInk.with_alpha(0.05f), 1.5f,
                                            look::kInk.with_alpha(0.1f));
                     else
                         board_bones_.draw(canvas);
                     list.pop_opacity();
                 }
                 if (shown <= 0.01f)
                     return;
                 // A new game fades in; the moves of a game animate.
                 const float swap =
                     swap_.running && !calm_ ? tween::cubic_out(swap_.progress()) : 1.0f;
                 list.push_opacity(shown * swap);
                 const float breath = calm_ || !live ? 0.5f : ui::breathe(ctx.time, 3.2f);
                 look::frame_board(list, kSquares.inset(-14.0f), rose,
                                   live ? 0.75f + 0.5f * breath : 0.35f);
                 board::Overlay overlay;
                 overlay.last_move = game_.last_move;
                 overlay.coordinates = ctx.settings->coordinates;
                 overlay.cursor_color = theme.primary;
                 view_.draw(list, fonts, *ctx.pieces, kSquares, overlay, ctx.board_theme(),
                            ctx.time);
                 list.pop_opacity();
             });

        // ---- the players: bones until the first game of the channel arrives ----
        const auto player = [&](const PlayerPanel &panel, const Rect &strip)
        {
            if (shown < 0.99f)
            {
                list.push_opacity(1.0f - shown);
                look::panel(list, strip);
                ui::Skeleton bones = player_bones_;
                bones.style.shimmer = !offline_now_;
                bones.set_bounds(strip.inset(24.0f));
                bones.draw(canvas);
                list.pop_opacity();
            }
            if (shown > 0.01f)
            {
                list.push_opacity(shown);
                panel.draw(ctx, list);
                list.pop_opacity();
            }
        };
        part(1, [&]() { player(top_, kTopPlayer); });

        // ---- the station ----
        part(2,
             [&]()
             {
                 const float cy = kTitleLine - 14.0f;
                 if (live)
                     look::live_dot(list, kColumn + 10.0f, cy, 7.0f, rose, ctx.time, calm_);
                 else
                     list.ring(kColumn + 10.0f, cy, 8.0f, 2.5f,
                               look::kInk.with_alpha(look::kFaint));
                 float x = kColumn + 34.0f;
                 x += ui::text(list, fonts.display, tr("Lichess TV"), x, kTitleLine, 40.0f,
                               look::kInk) +
                      20.0f;
                 if (game_.valid && on_air_)
                 {
                     if (stale_)
                     {
                         x += look::tag(list, fonts, tr("Reconnecting"), x, cy,
                                        look::kInk.with_alpha(0.16f), look::kInk, Align::left,
                                        16.0f) +
                              14.0f;
                         ui::Spinner spinner = reconnecting_;
                         spinner.set_bounds({x, cy - 14.0f, 28.0f, 28.0f});
                         spinner.draw(canvas);
                         x += 28.0f;
                     }
                     else
                     {
                         x += look::tag(list, fonts, tr("Live"), x, cy, rose, look::kNight,
                                        Align::left, 16.0f);
                     }
                     // The game's address has what the words before it leave.
                     list.push_opacity(shown);
                     ui::text_fit(list, fonts.regular, tv_game_link(game_), app::kRight, kTitleLine,
                                  20.0f, std::max(app::kRight - x - 16.0f, 0.0f),
                                  look::kInk.with_alpha(look::kMuted), Align::right);
                     list.pop_opacity();
                 }
                 else if (!offline_now_)
                 {
                     // A turning arc: the stream is being opened.
                     list.arc(x + 9.0f, cy, 9.0f, 2.5f, calm_ ? 0.0f : ctx.time * 5.0f, 2.2f,
                              look::kInk.with_alpha(look::kMuted));
                     kicker_fit(list, fonts, tr("Tuning in"), x + 30.0f, cy + 6.0f,
                                app::kRight - x - 30.0f, look::kInk.with_alpha(look::kMuted));
                 }
             });
        part(3, [&]() { channels_.draw(canvas); });

        // ---- what the feed tells ----
        part(4,
             [&]()
             {
                 look::panel(list, kInfo, 0.0f, rose);
                 if (offline_now_ && shown < 0.99f)
                 {
                     list.push_opacity(1.0f - shown);
                     draw_offline(ctx, list);
                     list.pop_opacity();
                 }
                 else if (shown < 0.99f)
                 {
                     list.push_opacity(1.0f - shown);
                     info_bones_.draw(canvas);
                     list.pop_opacity();
                 }
                 if (shown > 0.01f)
                 {
                     list.push_opacity(shown);
                     draw_info(ctx, list);
                     list.pop_opacity();
                 }
             });
        part(5, [&]() { player(bottom_, kBottomPlayer); });

        const ui::Hint hints[] = {{ui::Button::l1, TR("Channel"), ui::Button::r1},
                                  {ui::Button::square, TR("Flip board")},
                                  {ui::Button::circle, TR("Back")}};
        app::draw_hints(ctx, list, hints, 3);
    }

    look::Mood mood() const override
    {
        return look::mood(look::Section::tv);
    }

    const char *name() const override
    {
        return "tv";
    }

  private:
    const TvChannel &channel() const
    {
        return tv_channels()[static_cast<std::size_t>(channel_)];
    }

    void tune(app::Context &ctx) const
    {
        if (ctx.lichess != nullptr)
            ctx.lichess->watch_tv(channel().id);
    }

    // The channels share the column's width in equal parts, which leaves a
    // tab little room for its name, and the tab bar ends a name that is too
    // long in dots. Where a language's names do not fit that way, every tab is
    // as wide as its own name and they share what is left over; names too long
    // even for that are set smaller, all alike, down to what still reads.
    void fit_tabs(const app::Context &ctx)
    {
        gfx::DrawList scratch; // a Painter needs a list, even to measure
        const ui::Painter paint(scratch, *ctx.fonts, channels_.style.theme);
        ui::TabBarStyle &style = channels_.style;
        const float count = static_cast<float>(channels_.tabs().size());
        float widest = 0.0f;
        float all = 0.0f;
        for (const ui::TabItem &tab : channels_.tabs())
        {
            const float width = paint.label_width(tab.label, kTabText);
            widest = std::max(widest, width);
            all += width;
        }
        // What the names have together: the row without the gaps, the signs
        // and the least padding of every tab.
        const float names = kChannels.w - style.gap * (count - 1.0f) -
                            count * (style.glyph_width + style.glyph_gap + 2.0f * kTabPadding);
        style.width = ui::TabWidth::fill;
        style.padding = kTabPadding;
        style.text_size = kTabText;
        if (widest <= names / count)
            return;
        const float scale = std::clamp(names / std::max(all, 1.0f), 0.78f, 1.0f);
        style.width = ui::TabWidth::fit;
        style.text_size = kTabText * scale;
        style.padding = kTabPadding + std::max(names - all * scale - 1.0f, 0.0f) / (2.0f * count);
    }

    // 0..1 for the index-th part of the screen since it was opened.
    float rise(int index) const
    {
        return calm_ ? 1.0f : look::rise(since_, index, 0.07f, 0.5f);
    }

    // Takes what the stream has: a move of the game on screen is played on
    // the board with its sound, anything else is shown at once.
    void follow(app::Context &ctx)
    {
        const lichess::TvGame &tv = tv_game(ctx);
        on_air_ = switch_in_ <= 0.0f && tv_on_air(tv, channel().id);
        if (!on_air_ || (tv.version == game_.version && tv.id == game_.id && game_.valid &&
                         tv.channel == game_.channel))
            return;
        const bool same_game = game_.valid && tv.id == game_.id && tv.channel == game_.channel;
        const std::string san = same_game ? tv_last_san(shown_, tv) : std::string();
        if (!san.empty())
        {
            add_mark(ctx, san, shown_.turn() == chess::Color::white);
            view_.play(shown_, tv.last_move, tv.position, ctx.reduced_motion() ? 0.0f : 1.0f);
            ctx.cue(shown_.is_capture(tv.last_move) ? audio::Cue::capture : audio::Cue::move);
        }
        else
        {
            // Another game, or moves were missed: what was seen no longer
            // leads to this position.
            marks_.clear();
            seen_ = 0;
            scroll_.snap(0.0f);
            view_.snap(tv.position);
            if (!same_game)
            {
                view_.set_orientation(tv.orientation, false);
                swap_.start(0.5f);
            }
        }
        shown_ = tv.position;
        game_ = tv;
    }

    // A move joins the row; the row slides so the newest stays in view.
    void add_mark(const app::Context &ctx, const std::string &san, bool white)
    {
        Mark mark;
        mark.san = san;
        mark.white = white;
        mark.width = 16.0f + 12.0f + 10.0f + ctx.fonts->semibold.measure(san, kMarkSize) + 16.0f;
        marks_.push_back(std::move(mark));
        ++seen_;
        float removed = 0.0f;
        if (marks_.size() > kMovesKept)
        {
            removed = marks_.front().width + kMarkGap;
            marks_.erase(marks_.begin());
        }
        float total = -kMarkGap;
        for (const Mark &each : marks_)
            total += each.width + kMarkGap;
        // Dropping the oldest moves everything left by its width: the scroll
        // gives that back so nothing jumps.
        scroll_.value = std::max(scroll_.value - removed, 0.0f);
        scroll_.target = std::max(total - (kInfo.w - 2.0f * kInfoPad), 0.0f);
        if (calm_)
            scroll_.snap(scroll_.target);
        mark_in_.start(calm_ ? 0.0f : 0.4f);
    }

    // The two players, as the board is turned: who they are, what they took,
    // and their clocks (the side to move counts down from the last update).
    void describe(app::Context &ctx)
    {
        if (!game_.valid)
            return;
        const double now = ctx.lichess != nullptr ? ctx.lichess->now() : 0.0;
        const chess::Color turn = game_.position.turn();
        const chess::Color below = view_.orientation();
        for (const chess::Color color : {chess::opposite(below), below})
        {
            const bool white = color == chess::Color::white;
            PlayerPanel &panel = color == below ? bottom_ : top_;
            panel.set_player(white ? game_.white : game_.black,
                             rating_text(white ? game_.white_rating : game_.black_rating),
                             white ? game_.white_title : game_.black_title, color);
            // Whose pieces these are, and whether that side is to move. Each is
            // a whole text: another language words "to move" by the side.
            panel.set_note(color != turn ? (white ? tr("White") : tr("Black"))
                           : white       ? tr("White \xC2\xB7 to move")
                                         : tr("Black \xC2\xB7 to move"));
            panel.set_captured(captured_by(game_.position, color));
            const long long clock = tv_clock_ms(game_, color, now);
            panel.set_clock(clock, clock >= 0, color == turn);
        }

        // The last move: its notation when it was seen being played, its
        // squares otherwise. A new one rises in over the one before.
        std::string last = !marks_.empty() ? marks_.back().san : tv_move_squares(game_.last_move);
        if (last != last_)
        {
            before_ = std::move(last_);
            last_ = std::move(last);
            if (calm_)
            {
                last_in_.snap(1.0f);
            }
            else
            {
                last_in_.value = 0.0f;
                last_in_.velocity = 0.0f;
            }
        }
        white_points_ = tv_material(game_.position, chess::Color::white);
        black_points_ = tv_material(game_.position, chess::Color::black);
        const int total = white_points_ + black_points_;
        share_.target =
            total > 0 ? static_cast<float>(white_points_) / static_cast<float>(total) : 0.5f;
    }

    // Between the players: what the stream tells about the game. It carries
    // the position and the last move, not the game's moves, so the row is
    // what was played while this screen watched.
    void draw_info(const app::Context &ctx, gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Color rose = look::accent(look::Section::tv);
        const float x = kInfo.x + kInfoPad;
        const float right = kInfo.x + kInfo.w - kInfoPad;
        const float half = kInfo.x + kInfo.w * 0.5f + 16.0f;
        const float y = kInfo.y;

        // ---- the last move, large
        const float room = half - 48.0f - x;
        // The label, then who played the move: together they stay in the column.
        const char *mover =
            game_.position.turn() == chess::Color::white ? tr("Black") : tr("White");
        const float mover_width =
            game_.last_move.valid() ? fonts.semibold.measure(ui::upper(mover), 16.0f, 3.0f) + 16.0f
                                    : 0.0f;
        const float kick = kicker_fit(list, fonts, tr("Last move"), x, y + 52.0f,
                                      std::max(room - mover_width, room * 0.5f), rose);
        if (game_.last_move.valid())
            kicker_fit(list, fonts, mover, x + kick + 16.0f, y + 52.0f, room - kick - 16.0f,
                       look::kInk.with_alpha(look::kFaint));
        const float t = tween::clamp01(last_in_.value);
        const auto figure = [&](const std::string &text, float alpha, float dy)
        {
            if (alpha <= 0.01f)
                return;
            list.push_opacity(alpha);
            if (text.empty())
                ui::text_fit(list, fonts.regular, tr("None yet"), x, y + 124.0f + dy, 28.0f, room,
                             look::kInk.with_alpha(look::kMuted));
            else
                ui::text(list, fonts.display, fonts.display.font->fit(text, 76.0f, room), x - 3.0f,
                         y + 138.0f + dy, 76.0f, look::kInk);
            list.pop_opacity();
        };
        // The move before is gone before the new one is all there.
        figure(before_, 1.0f - tween::smoothstep(t / 0.45f), -26.0f * t);
        figure(last_, tween::smoothstep((t - 0.3f) / 0.7f), 26.0f * (1.0f - t));

        // ---- the material: who holds how much of what is left
        kicker_fit(list, fonts, tr("Material"), half, y + 52.0f, right - half, rose);
        const int balance = white_points_ - black_points_;
        if (balance == 0)
        {
            ui::text_fit(list, fonts.display, tr("Even"), half - 2.0f, y + 112.0f, 44.0f,
                         right - half, look::kInk);
        }
        else
        {
            ctx.pieces->draw(
                list, {balance > 0 ? chess::Color::white : chess::Color::black, chess::Role::pawn},
                {half - 10.0f, y + 68.0f, 52.0f, 52.0f});
            char text[24];
            std::snprintf(text, sizeof(text), "+%d", balance > 0 ? balance : -balance);
            ui::text(list, fonts.display, text, half + 40.0f, y + 112.0f, 44.0f, look::kInk);
        }
        const Rect bar{half, y + 132.0f, right - half, 12.0f};
        list.rounded_rect(bar, 6.0f, look::kNight.with_alpha(0.7f));
        list.rounded_rect(
            {bar.x, bar.y, std::max(bar.w * tween::clamp01(share_.value), bar.h), bar.h}, 6.0f,
            look::kInk.with_alpha(0.92f));
        list.rounded_rect({bar.cx() - 1.0f, bar.y - 4.0f, 2.0f, bar.h + 8.0f}, 1.0f,
                          rose.with_alpha(0.9f));
        // Each side's count has its half of the bar.
        const float count_room = bar.w * 0.5f - 8.0f;
        ui::text_fit(list, fonts.regular, fill(tr("White {0}"), {std::to_string(white_points_)}),
                     bar.x, y + 174.0f, 19.0f, count_room, look::kInk.with_alpha(look::kMuted));
        ui::text_fit(list, fonts.regular, fill(tr("Black {0}"), {std::to_string(black_points_)}),
                     right, y + 174.0f, 19.0f, count_room, look::kInk.with_alpha(look::kMuted),
                     Align::right);

        // ---- the moves since tuning in
        look::rule(list, x, y + 200.0f, right - x);
        // The label stops before the count at the right of its line.
        const std::string seen =
            marks_.empty() ? std::string() : plural(TR("{0} move"), TR("{0} moves"), seen_);
        kicker_fit(list, fonts, tr("Since you tuned in"), x, y + 244.0f,
                   right - x - (seen.empty() ? 0.0f : fonts.regular.measure(seen, 19.0f) + 16.0f),
                   rose);
        if (marks_.empty())
        {
            ui::text_fit(list, fonts.regular, tr("Moves appear here as they are played."), x,
                         y + 306.0f, 24.0f, right - x, look::kInk.with_alpha(look::kMuted));
            return;
        }
        ui::text(list, fonts.regular, seen, right, y + 244.0f, 19.0f,
                 look::kInk.with_alpha(look::kFaint), Align::right);

        // Oldest first; the row has scrolled so the newest is in view, and
        // what leaves on the left fades as it goes.
        const float cy = y + 298.0f;
        const float width = right - x;
        list.push_clip({x - 6.0f, cy - kMarkHeight, width + 12.0f, 2.0f * kMarkHeight});
        float mx = x - scroll_.value;
        const float arriving = mark_in_.running ? tween::clamp01(mark_in_.progress()) : 1.0f;
        for (std::size_t i = 0; i < marks_.size(); ++i)
        {
            const Mark &mark = marks_[i];
            const Rect r{mx, cy - kMarkHeight * 0.5f, mark.width, kMarkHeight};
            mx += mark.width + kMarkGap;
            const float inside = tween::clamp01((r.x + r.w - x) / r.w);
            if (inside <= 0.0f)
                continue;
            const bool newest = i + 1 == marks_.size();
            // The further back a move is, the quieter it is drawn.
            const float age = static_cast<float>(marks_.size() - 1 - i);
            const float alpha = tween::smoothstep(inside) *
                                (newest ? arriving : std::max(1.0f - 0.07f * age, 0.45f));
            list.push_opacity(alpha);
            if (newest)
            {
                // The newest lands with a pop, lit.
                const float pop = tween::lerp(0.7f, 1.0f, tween::back_out(arriving));
                list.push_transform(pop, r.cx(), r.cy(), 0.0f, 0.0f);
                list.glow(r, kMarkHeight * 0.5f, 14.0f, rose.with_alpha(0.3f));
                list.rounded_rect(r, kMarkHeight * 0.5f, rose);
            }
            else
            {
                list.bordered_rect(r, kMarkHeight * 0.5f, look::kInk.with_alpha(0.06f), 1.5f,
                                   look::kInk.with_alpha(0.12f));
            }
            // Whose move it was: a light or a dark stone.
            const float sx = r.x + 16.0f + 6.0f;
            if (mark.white)
            {
                list.circle(sx, r.cy(), 6.0f, look::kInk);
            }
            else
            {
                list.circle(sx, r.cy(), 6.0f, look::kNight);
                list.ring(sx, r.cy(), 6.0f, 1.5f,
                          newest ? look::kNight : look::kInk.with_alpha(0.55f));
            }
            ui::text(list, fonts.semibold, mark.san, r.x + 16.0f + 12.0f + 10.0f,
                     r.cy() + kMarkSize * 0.35f, kMarkSize, newest ? look::kNight : look::kInk);
            if (newest)
                list.pop_transform();
            list.pop_opacity();
        }
        list.pop_clip();
    }

    // Without a connection the panel says so; the board and the players keep
    // their places, still, for when the game is back.
    void draw_offline(const app::Context &ctx, gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Color rose = look::accent(look::Section::tv);
        const Rect mark{kInfo.x + 56.0f, kInfo.cy() - 70.0f, 140.0f, 140.0f};
        draw_tv_offline_mark(list, mark, look::kInk, rose);
        const float x = mark.x + mark.w + 44.0f;
        const float room = kInfo.x + kInfo.w - kInfoPad - x;
        kicker_fit(list, fonts, tr("No connection"), x, kInfo.cy() - 76.0f, room, rose);
        ui::text_fit(list, fonts.display, tr("Lichess TV is out of reach"), x - 2.0f,
                     kInfo.cy() - 22.0f, 38.0f, room, look::kInk);
        ui::paragraph(list, fonts.regular,
                      tr("Watching games needs the internet connection. The game appears here as "
                         "soon as it is back."),
                      x, kInfo.cy() + 26.0f, 23.0f, room, 33.0f,
                      look::kInk.with_alpha(look::kMuted), 4);
    }

    ui::TabBar channels_;
    board::BoardView view_;
    PlayerPanel top_;
    PlayerPanel bottom_;
    ui::Spinner reconnecting_;
    ui::Skeleton board_bones_;
    ui::Skeleton player_bones_;
    ui::Skeleton info_bones_;

    int channel_ = 0;
    float switch_in_ = 0.0f; // seconds until the stream follows the tabs
    bool on_air_ = false;    // the chosen channel's game is the one shown
    bool stale_ = false;     // the feed dropped and is being reconnected
    bool offline_now_ = false;
    lichess::TvGame game_; // the game on the board
    chess::Position shown_ = chess::Position::start();
    std::vector<Mark> marks_; // played while this screen watched, in order
    int seen_ = 0;            // how many of them there were (the row keeps the newest)
    std::string last_;        // the last move as the panel shows it
    std::string before_;      // ... and the one it replaced, on its way out
    int white_points_ = 0;    // the material on the board
    int black_points_ = 0;

    // ---- motion ----
    float since_ = 0.0f; // seconds since the screen was opened: parts arrive by it
    bool calm_ = false;  // reduced motion
    tween::Timer swap_;
    tween::Spring arrive_;  // 0 bones, 1 the game
    tween::Spring last_in_; // 0 -> 1 as a new last move rises in
    tween::Spring share_;   // White's share of the material
    tween::Spring scroll_;  // how far the row of moves has flowed to the left
    tween::Timer mark_in_;  // the newest move landing
    ui::SpringColor plate_; // the tabs' plate, in the colour of its channel
};

} // namespace

std::unique_ptr<app::Scene> make_tv(const std::string &channel)
{
    return std::make_unique<TvScene>(channel);
}

} // namespace pch::modes
