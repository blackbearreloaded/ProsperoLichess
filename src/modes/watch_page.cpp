// ProsperoLichess - Watch page: the game on air and the Lichess TV channels.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The page in the house look (app/look.hpp):
//
//   - the game on air is the hero: the board stands between the two players'
//     name plates the way a broadcast shows it, on a rose light that breathes
//     while the game is live, and the plate of the side to move is lit;
//   - beside the board: which channel this is (its sign and its name), the
//     last move, the players' average rating counting to its value, who holds
//     more material as a bar, and what each side has taken;
//   - the channels are six tiles in their own colours with their own signs;
//     the one with the controller floats and one ring glides between them;
//   - a change of channel slides the station's name and cross-fades the game
//     with the bones of the one that is coming.

#include "board/mini_board.hpp"
#include "core/strings.hpp"
#include "lichess/session.hpp"
#include "modes/game_widgets.hpp"
#include "modes/page.hpp"
#include "modes/scenes.hpp"
#include "modes/tv_feed.hpp"
#include "ui/components/list.hpp"
#include "ui/components/skeleton.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <span>
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

// The home page's four columns: the game on air spans three of them and the
// channels are the fourth, so both pages share their vertical edges.
constexpr float kColumnWidth = (app::kRight - app::kContent - 3.0f * app::kGap) / 4.0f;
constexpr float kColumnPitch = kColumnWidth + app::kGap;
constexpr Rect kHero{app::kContent, app::kTop, 3.0f * kColumnPitch - app::kGap,
                     app::kBottom - app::kTop};
constexpr Rect kTiles{app::kContent + 3.0f * kColumnPitch, app::kTop, kColumnWidth,
                      app::kBottom - app::kTop};
constexpr int kChannelCount = 6;

// Inside the hero the board stands between the two name plates and shares
// their left and right edges.
constexpr float kPad = 24.0f;
constexpr float kPlate = 84.0f;
constexpr float kPlateGap = 14.0f;
constexpr float kPlateRadius = 18.0f;
constexpr float kFrame = 12.0f; // the board's wooden frame, inside kBoard
constexpr float kStack = kHero.h - 2.0f * kPad - 2.0f * kPlate - 2.0f * kPlateGap;
constexpr Rect kTopPlate{kHero.x + kPad, kHero.y + kPad, kStack, kPlate};
constexpr Rect kBoard{kHero.x + kPad, kTopPlate.y + kPlate + kPlateGap, kStack, kStack};
constexpr Rect kBottomPlate{kHero.x + kPad, kBoard.y + kStack + kPlateGap, kStack, kPlate};
// Beside them: the station and what is known about the game.
constexpr float kSide = kBoard.x + kStack + 36.0f;
constexpr float kSideWidth = kHero.x + kHero.w - 28.0f - kSide;
constexpr float kDisc = 104.0f; // the station's sign
constexpr Rect kAction{kSide, kBottomPlate.y, kSideWidth, kPlate};
// Holding a direction walks the channels; the stream follows once it rests.
constexpr float kSwitchDelay = 0.35f;
constexpr long long kLowClockMs = 20000;

// A channel's tile. The six share the column's height exactly; their edges
// sit on whole pixels.
Rect tile_rect(int index)
{
    const float pitch = (kTiles.h + app::kGap) / static_cast<float>(kChannelCount);
    const float top = std::floor(kTiles.y + static_cast<float>(index) * pitch + 0.5f);
    const float next = std::floor(kTiles.y + static_cast<float>(index + 1) * pitch + 0.5f);
    return {kTiles.x, top, kTiles.w, next - app::kGap - top};
}

// One side of the game on air, as its name plate shows it.
struct Side
{
    std::string name;
    std::string title;
    int rating = 0;
    chess::Color color = chess::Color::white;
    long long clock_ms = -1;
    long long peak_ms = 0; // the most this clock has shown: the level's full mark
    bool to_move = false;
    std::vector<chess::Piece> taken; // what this side captured
    tween::Spring lit;               // 0..1: this side is to move
    tween::Spring shown;             // the rating on screen, counting to its value
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

class WatchPage final : public Page
{
  public:
    explicit WatchPage(app::Context &ctx)
    {
        // The list keeps the channels' behaviour and voice (focus, sounds, the
        // refusal at both ends); the tiles are drawn by this page.
        std::vector<ui::ListItem> items;
        for (const TvChannel &channel : tv_channels())
        {
            ui::ListItem item;
            item.title = channel.label;
            items.push_back(std::move(item));
        }
        channels_.set_items(std::move(items));
        channels_.set_bounds(kTiles);
        channels_.set_focus(0);

        // Bones in the shape of the board; the game fades in over them.
        board_.style.kind = ui::SkeletonKind::block;
        board_.style.radius = 10.0f;
        board_.set_bounds(kBoard);
        board_.content = [this, &ctx](ui::Canvas &canvas, const Rect &area, float)
        {
            // Another game of the same channel fades in over the panel.
            const float swap = swap_.running && !calm_ ? tween::cubic_out(swap_.progress()) : 1.0f;
            canvas.list.push_opacity(swap);
            board::draw_mini_board(canvas.list, *ctx.pieces, ctx.board_theme(), area.inset(kFrame),
                                   game_.position, game_.orientation, game_.last_move, kFrame);
            canvas.list.pop_opacity();
        };
        online_ = ctx.lichess != nullptr && ctx.lichess->online();
        slide_.snap(1.0f);
    }

    void enter(app::Context &ctx) override
    {
        online_ = ctx.lichess != nullptr && ctx.lichess->online();
        if (in_tv_)
        {
            // Back from the full screen: it may have changed channel, and its
            // stream keeps running, so the preview picks up where it left.
            in_tv_ = false;
            choose(tv_channel_index(tv_game(ctx).channel), true);
        }
        else
        {
            // Shown from the rail: the featured game, the stream the Home page
            // keeps open anyway. Other channels open once the controller is here.
            choose(0, true);
        }
        switch_in_ = 0.0f;
        // The page assembles again, and its numbers count from nothing.
        since_ = 0.0f;
        placed_ = false;
        average_.snap(0.0f);
        for (Side &side : sides_)
            side.shown.snap(0.0f);
        tune(ctx);
        read(ctx, true);
    }

    PageResult update(app::Context &ctx, const InputFrame &input, float dt, bool focused) override
    {
        ctx.calm(channels_, board_);
        calm_ = ctx.reduced_motion();
        since_ += dt;
        const bool online = ctx.lichess != nullptr && ctx.lichess->online();
        if (online != online_)
        {
            online_ = online;
            since_ = 0.0f;
            tune(ctx);
        }

        PageResult result;
        if (focused)
        {
            handle(ctx, input, &result);
        }
        else if (had_controller_)
        {
            // The controller went back to the rail, from where another page
            // may be chosen: return to the featured game, the one stream that
            // is kept open (the Home page asks for the same), so no channel
            // keeps streaming behind a page that does not show it.
            choose(0, false);
            switch_in_ = 0.0f;
            tune(ctx);
        }
        had_controller_ = focused;

        if (switch_in_ > 0.0f)
        {
            switch_in_ -= dt;
            if (switch_in_ <= 0.0f)
                tune(ctx);
        }
        read(ctx, false);

        channels_.set_active(focused && online_);
        animate(dt, focused && online_);
        board_.update(dt);
        return result;
    }

    void draw(app::Context &ctx, app::Frame &frame, bool) const override
    {
        gfx::DrawList &list = frame.scene;
        if (!online_)
        {
            draw_offline(ctx, list);
            return;
        }
        draw_hero(ctx, list);
        for (int i = 0; i < kChannelCount; ++i)
            draw_tile(ctx, list, i);

        // The ring, while the page has the controller.
        if (ring_alpha_.value > 0.01f)
        {
            Rect ring = ring_.value();
            ring.x += nudge_x();
            ring.y += nudge_y();
            look::ring(list, ring.inset(2.0f * press_.value), ring_ink_.value(), ring_alpha_.value);
        }
    }

    std::span<const ui::Hint> hints() const override
    {
        static constexpr ui::Hint kHints[] = {{ui::Button::cross, TR("Watch")},
                                              {ui::Button::dpad, TR("Channel")},
                                              {ui::Button::circle, TR("Back")}};
        static constexpr ui::Hint kOfflineHints[] = {{ui::Button::circle, TR("Back")}};
        if (!online_)
            return kOfflineHints;
        return kHints;
    }

    look::Section section() const override
    {
        return look::Section::watch;
    }

    const char *title() const override
    {
        return tr("Watch");
    }

    const char *name() const override
    {
        return "watch";
    }

  private:
    const TvChannel &channel() const
    {
        return tv_channels()[static_cast<std::size_t>(chosen_)];
    }

    // snap: without the slide of the station's name (the page is arriving).
    void choose(int index, bool snap)
    {
        const int before = chosen_;
        chosen_ = std::clamp(index, 0, static_cast<int>(tv_channels().size()) - 1);
        channels_.set_focus(chosen_, snap);
        station(before, snap);
    }

    // The station's name slides out toward where the focus came from.
    void station(int before, bool snap)
    {
        if (snap || calm_)
        {
            slide_.snap(1.0f);
            return;
        }
        if (before == chosen_)
            return;
        from_ = before;
        slide_.value = 0.0f;
        slide_.velocity = 0.0f;
        slide_.target = 1.0f;
    }

    // Asks the session for the chosen channel (it keeps a stream it already has).
    void tune(app::Context &ctx) const
    {
        if (online_ && ctx.lichess != nullptr)
            ctx.lichess->watch_tv(channel().id);
    }

    void handle(app::Context &ctx, const InputFrame &input, PageResult *result)
    {
        if (input.is_pressed(Action::back))
        {
            ctx.cue(audio::Cue::back);
            result->to_rail = true;
            return;
        }
        if (input.nav == Direction::left)
        {
            result->to_rail = true;
            return;
        }
        if (!online_)
            return;
        // Nothing lies to the right of the channels: the tile answers.
        if (input.nav == Direction::right && !input.nav_repeat)
            wall_.trigger();
        const ui::Event event = channels_.handle(input, *ctx.feedback);
        if (event == ui::Event::moved)
        {
            const int before = chosen_;
            chosen_ = channels_.focus();
            switch_in_ = kSwitchDelay;
            station(before, false);
        }
        else if (event == ui::Event::activated)
        {
            // The full screen takes over the stream from here.
            press_.trigger();
            switch_in_ = 0.0f;
            in_tv_ = true;
            result->transition = app::Transition::push(make_tv(channel().id));
        }
        else if (event == ui::Event::refused && !input.nav_repeat)
        {
            // The end of the channels answers with a nudge.
            refusal_.trigger();
        }
    }

    // Takes what the stream has for the chosen channel. A game of another
    // channel (the one being left) is not shown: the bones are, until the
    // first game of the new one arrives.
    void read(app::Context &ctx, bool snap)
    {
        const lichess::TvGame &tv = tv_game(ctx);
        on_air_ = online_ && switch_in_ <= 0.0f && tv_on_air(tv, channel().id);
        if (on_air_ && (tv.version != game_.version || tv.id != game_.id ||
                        tv.channel != game_.channel || !game_.valid))
        {
            const bool same_game = game_.valid && tv.id == game_.id && tv.channel == game_.channel;
            if (!same_game)
            {
                // Another game arrives as a whole: it fades in, and its
                // clocks have new full marks.
                if (!snap)
                    swap_.start(0.5f);
                for (Side &side : sides_)
                    side.peak_ms = 0;
                last_move_ = tv_move_squares(tv.last_move);
            }
            else if (tv.position.fen() != game_.position.fen())
            {
                // A move of the game on screen: its notation when the position
                // shown leads to the new one, its squares otherwise.
                const std::string san = tv_last_san(game_.position, tv);
                last_move_ = san.empty() ? tv_move_squares(tv.last_move) : san;
                move_pop_.trigger();
            }
            game_ = tv;
        }
        board_.set_loaded(on_air_, snap);
        // The feed dropped: the game stays, the page stops claiming it is live.
        stale_ = ctx.lichess != nullptr && ctx.lichess->has_issue("tv");
        if (!game_.valid)
            return;

        // The player at the top of the board first.
        const double now = ctx.lichess != nullptr ? ctx.lichess->now() : 0.0;
        const chess::Color colors[] = {chess::opposite(game_.orientation), game_.orientation};
        for (std::size_t i = 0; i < 2; ++i)
        {
            const bool white = colors[i] == chess::Color::white;
            Side &side = sides_[i];
            side.name = white ? game_.white : game_.black;
            side.title = white ? game_.white_title : game_.black_title;
            side.rating = white ? game_.white_rating : game_.black_rating;
            side.color = colors[i];
            side.clock_ms = tv_clock_ms(game_, colors[i], now);
            side.peak_ms = std::max(side.peak_ms, side.clock_ms);
            side.to_move = game_.position.turn() == colors[i];
            side.taken = captured_by(game_.position, colors[i]);
        }
        const int white = tv_material(game_.position, chess::Color::white);
        const int black = tv_material(game_.position, chess::Color::black);
        white_points_ = white;
        black_points_ = black;
        share_.target = white + black > 0
                            ? static_cast<float>(white) / static_cast<float>(white + black)
                            : 0.5f;
        const int rated = (game_.white_rating > 0 ? 1 : 0) + (game_.black_rating > 0 ? 1 : 0);
        average_.target = rated > 0 ? static_cast<float>(std::max(game_.white_rating, 0) +
                                                         std::max(game_.black_rating, 0)) /
                                          static_cast<float>(rated)
                                    : 0.0f;
    }

    // Everything that moves by itself. active: the page has the controller
    // and channels to move over.
    void animate(float dt, bool active)
    {
        const float quick = calm_ ? 60.0f : 18.0f;
        for (int i = 0; i < kChannelCount; ++i)
        {
            const std::size_t at = static_cast<std::size_t>(i);
            focus_[at].target = active && i == chosen_ ? 1.0f : 0.0f;
            tuned_[at].target = i == chosen_ ? 1.0f : 0.0f;
        }
        ring_.target(tile_rect(chosen_));
        ring_ink_.target(tv_channel_color(chosen_));
        ring_alpha_.target = active ? 1.0f : 0.0f;
        for (Side &side : sides_)
        {
            side.lit.target = on_air_ && side.to_move ? 1.0f : 0.0f;
            side.shown.target = static_cast<float>(side.rating);
        }
        if (!placed_)
        {
            // The page was just entered: nothing glides in from where it was.
            placed_ = true;
            for (std::size_t i = 0; i < focus_.size(); ++i)
            {
                focus_[i].snap(focus_[i].target);
                tuned_[i].snap(tuned_[i].target);
            }
            ring_.snap(tile_rect(chosen_));
            ring_ink_.snap(tv_channel_color(chosen_));
            ring_alpha_.snap(ring_alpha_.target);
            share_.snap(share_.target);
            for (Side &side : sides_)
                side.lit.snap(side.lit.target);
        }
        for (std::size_t i = 0; i < focus_.size(); ++i)
        {
            focus_[i].update(dt, quick);
            tuned_[i].update(dt, quick);
        }
        ring_.update(dt, quick);
        ring_ink_.update(dt, calm_ ? 60.0f : 12.0f);
        ring_alpha_.update(dt, quick);
        ease(slide_, dt, 11.0f, calm_);
        ease(share_, dt, 8.0f, calm_);
        // The numbers start counting once their panel has arrived.
        const bool counting = calm_ || since_ > 0.35f;
        for (Side &side : sides_)
        {
            ease(side.lit, dt, 14.0f, calm_);
            if (counting)
                ease(side.shown, dt, 10.0f, calm_);
        }
        if (counting)
            ease(average_, dt, 10.0f, calm_);
        press_.update(dt, 10.0f);
        refusal_.update(dt, 9.0f);
        wall_.update(dt, 9.0f);
        move_pop_.update(dt, 7.0f);
        swap_.update(dt);
    }

    // The answer to a direction that leads nowhere: a short shake of the
    // tile and its ring, up and down at the ends of the channels, sideways
    // at their right.
    float nudge_x() const
    {
        return calm_ ? 0.0f : ui::shake(wall_.value, since_, 7.0f);
    }
    float nudge_y() const
    {
        return calm_ ? 0.0f : ui::shake(refusal_.value, since_, 8.0f);
    }

    // 0..1 for the index-th part of the page since it was entered.
    float arrive(int index) const
    {
        return calm_ ? 1.0f : look::rise(since_, index, 0.06f, 0.5f);
    }

    // ---- the hero ------------------------------------------------------------

    void draw_hero(const app::Context &ctx, gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Color rose = look::accent(look::Section::watch);
        ui::Canvas canvas = app::canvas_for(ctx, list);
        // How much of the game is there (the rest is its bones).
        const float shown = board_.content_alpha();
        const bool live = on_air_ && !stale_;

        const auto part = [&](int index, const auto &draw)
        {
            const float in = arrive(index);
            list.push_opacity(in);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in, 16.0f));
            draw();
            list.pop_transform();
            list.pop_opacity();
        };

        // ---- the panel, the board and the two plates
        part(0,
             [&]()
             {
                 look::panel(list, kHero, 0.3f * shown, rose);
                 // The station's light, in the colour of its channel.
                 list.push_clip(kHero.inset(2.0f));
                 look::halo(list, {kSide - 150.0f, kHero.y - 150.0f, 500.0f, 500.0f},
                            ring_ink_.value(), 0.12f);
                 list.pop_clip();
             });
        part(1,
             [&]()
             {
                 const float breath = calm_ || !live ? 0.5f : ui::breathe(ctx.time, 3.2f);
                 list.push_opacity(shown);
                 look::frame_board(list, kBoard, rose, live ? 0.7f + 0.5f * breath : 0.3f);
                 list.pop_opacity();
                 board_.draw(canvas);
             });
        part(1, [&]() { draw_plate(ctx, list, kTopPlate, sides_[0], shown); });
        part(2, [&]() { draw_plate(ctx, list, kBottomPlate, sides_[1], shown); });

        // ---- on air
        part(2,
             [&]()
             {
                 const float cy = kHero.y + 48.0f;
                 const float x = kSide + 24.0f;
                 // The word and the game's address share the line. Where a
                 // translated word leaves the address too little room, both
                 // shrink alike.
                 const char *word = live      ? tr("On air")
                                    : on_air_ ? tr("Reconnecting")
                                              : tr("Tuning in");
                 const std::string link = tv_game_link(game_);
                 const float word_width = fonts.semibold.measure(ui::upper(word), 16.0f, 3.0f);
                 const float link_width = fonts.regular.measure(link, 19.0f);
                 const float both = kSide + kSideWidth - x - 16.0f;
                 const bool tight = word_width + link_width > both;
                 const float share = tight ? both / (word_width + link_width) : 1.0f;
                 const float word_room = tight ? word_width * share : both;
                 const float link_room = tight ? link_width * share : kSideWidth - 200.0f;
                 if (live)
                 {
                     look::live_dot(list, kSide + 8.0f, cy, 6.0f, rose, ctx.time, calm_);
                     kicker_fit(list, fonts, word, x, cy + 6.0f, word_room, rose);
                 }
                 else if (on_air_)
                 {
                     list.circle(kSide + 8.0f, cy, 6.0f, look::kInk.with_alpha(look::kFaint));
                     kicker_fit(list, fonts, word, x, cy + 6.0f, word_room,
                                look::kInk.with_alpha(look::kMuted));
                 }
                 else
                 {
                     // A turning arc: the stream is being opened.
                     list.ring(kSide + 8.0f, cy, 8.0f, 2.5f, look::kInk.with_alpha(0.14f));
                     list.arc(kSide + 8.0f, cy, 8.0f, 2.5f, calm_ ? 0.0f : ctx.time * 5.0f, 2.2f,
                              look::kInk.with_alpha(look::kMuted));
                     kicker_fit(list, fonts, word, x, cy + 6.0f, word_room,
                                look::kInk.with_alpha(look::kMuted));
                 }
                 // Where the game can be found.
                 list.push_opacity(shown);
                 ui::text_fit(list, fonts.regular, link, kSide + kSideWidth, cy + 6.0f, 19.0f,
                              link_room, look::kInk.with_alpha(look::kFaint), Align::right);
                 list.pop_opacity();
             });

        // ---- the station: the chosen channel, sliding in over the one left
        part(3,
             [&]()
             {
                 const float t = tween::clamp01(slide_.value);
                 const float way = chosen_ >= from_ ? 1.0f : -1.0f;
                 // The one being left is gone before the new one is all there.
                 draw_station(ctx, list, from_, 1.0f - tween::smoothstep(t / 0.45f),
                              -way * 26.0f * t);
                 draw_station(ctx, list, chosen_, tween::smoothstep((t - 0.3f) / 0.7f),
                              way * 26.0f * (1.0f - t));
             });

        // ---- what is known about the game
        const Color label = look::kInk.with_alpha(look::kFaint);
        const Color unknown = look::kInk.with_alpha(look::kFaint);
        part(4,
             [&]()
             {
                 const float y = kHero.y + 216.0f;
                 look::rule(list, kSide, y, kSideWidth);
                 // Two columns: each label stays inside its own.
                 const float half = kSide + kSideWidth * 0.5f;
                 kicker_fit(list, fonts, tr("Last move"), kSide, y + 40.0f, half - 12.0f - kSide,
                            label, Align::left, 14.0f);
                 kicker_fit(list, fonts, tr("Average rating"), half, y + 40.0f,
                            kSide + kSideWidth - half, label, Align::left, 14.0f);
                 if (shown <= 0.01f || !game_.valid)
                 {
                     look::figure(list, fonts, "\xE2\x80\x94", kSide, y + 90.0f, 40.0f, unknown);
                     look::figure(list, fonts, "\xE2\x80\x94", half, y + 90.0f, 40.0f, unknown);
                     return;
                 }
                 list.push_opacity(shown);
                 // A new move lands with a small pop.
                 const float pop = calm_ ? 0.0f : move_pop_.value;
                 list.push_transform(1.0f + 0.1f * pop, kSide, y + 76.0f, 0.0f, 0.0f);
                 look::figure(list, fonts, last_move_.empty() ? "\xE2\x80\x94" : last_move_, kSide,
                              y + 90.0f, 40.0f,
                              last_move_.empty() ? unknown : gfx::mix(look::kInk, rose, pop));
                 list.pop_transform();
                 if (average_.target > 0.0f)
                 {
                     char text[16];
                     std::snprintf(text, sizeof(text), "%d",
                                   static_cast<int>(average_.value + 0.5f));
                     look::figure(list, fonts, text, half, y + 90.0f, 40.0f, look::kInk);
                 }
                 else
                 {
                     look::figure(list, fonts, "\xE2\x80\x94", half, y + 90.0f, 40.0f, unknown);
                 }
                 list.pop_opacity();
             });
        part(5,
             [&]()
             {
                 const float y = kHero.y + 346.0f;
                 look::rule(list, kSide, y, kSideWidth);
                 // The label has the left half of the line, who is ahead the right;
                 // under the bar each side's count has its half too.
                 const float half = kSideWidth * 0.5f - 8.0f;
                 kicker_fit(list, fonts, tr("Material"), kSide, y + 40.0f, half, label, Align::left,
                            14.0f);
                 const Rect bar{kSide, y + 60.0f, kSideWidth, 14.0f};
                 list.rounded_rect(bar, 7.0f, look::kInk.with_alpha(0.1f));
                 if (shown <= 0.01f || !game_.valid)
                     return;
                 list.push_opacity(shown);
                 // White's share of the pieces on the board, from the left.
                 const float grow = calm_ ? 1.0f : look::rise(since_, 6, 0.08f, 0.8f);
                 const float white = bar.w * tween::clamp01(share_.value) * grow;
                 list.rounded_rect(bar, 7.0f, look::kNight.with_alpha(0.7f * grow));
                 list.rounded_rect({bar.x, bar.y, std::max(white, bar.h), bar.h}, 7.0f,
                                   look::kInk.with_alpha(0.92f));
                 list.rounded_rect({bar.cx() - 1.0f, bar.y - 4.0f, 2.0f, bar.h + 8.0f}, 1.0f,
                                   rose.with_alpha(0.9f));
                 ui::text_fit(list, fonts.regular,
                              fill(tr("White {0}"), {std::to_string(white_points_)}), kSide,
                              y + 106.0f, 19.0f, half, look::kInk.with_alpha(look::kMuted));
                 ui::text_fit(list, fonts.regular,
                              fill(tr("Black {0}"), {std::to_string(black_points_)}),
                              kSide + kSideWidth, y + 106.0f, 19.0f, half,
                              look::kInk.with_alpha(look::kMuted), Align::right);
                 // Who is ahead, and by how much.
                 const int balance = white_points_ - black_points_;
                 if (balance == 0)
                 {
                     ui::text_fit(list, fonts.semibold, tr("Even"), kSide + kSideWidth, y + 42.0f,
                                  22.0f, half, look::kInk.with_alpha(look::kMuted), Align::right);
                 }
                 else
                 {
                     char text[24];
                     std::snprintf(text, sizeof(text), "+%d", balance > 0 ? balance : -balance);
                     const float width = look::figure(list, fonts, text, kSide + kSideWidth,
                                                      y + 42.0f, 22.0f, look::kInk, Align::right);
                     ctx.pieces->draw(
                         list,
                         {balance > 0 ? chess::Color::white : chess::Color::black,
                          chess::Role::pawn},
                         {kSide + kSideWidth - width - 34.0f, y + 16.0f, 32.0f, 32.0f});
                 }
                 list.pop_opacity();
             });
        part(6,
             [&]()
             {
                 const float y = kHero.y + 492.0f;
                 look::rule(list, kSide, y, kSideWidth);
                 kicker_fit(list, fonts, tr("Captured"), kSide, y + 40.0f, kSideWidth, label,
                            Align::left, 14.0f);
                 for (std::size_t i = 0; i < 2; ++i)
                 {
                     const float cy = y + 82.0f + static_cast<float>(i) * 48.0f;
                     if (shown <= 0.01f || !game_.valid)
                     {
                         list.rounded_rect({kSide, cy - 7.0f, 150.0f, 14.0f}, 7.0f,
                                           look::kInk.with_alpha(0.07f));
                         continue;
                     }
                     const Side &side = sides_[i];
                     list.push_opacity(shown);
                     // Whose captures these are, then the pieces.
                     list.circle(kSide + 17.0f, cy, 17.0f, look::kInk.with_alpha(0.08f));
                     ctx.pieces->draw(list, {side.color, chess::Role::king},
                                      {kSide + 1.0f, cy - 16.0f, 32.0f, 32.0f});
                     float x = kSide + 48.0f;
                     if (side.taken.empty())
                         ui::text_fit(list, fonts.regular, tr("Nothing yet"), x, cy + 7.0f, 19.0f,
                                      kSide + kSideWidth - x, look::kInk.with_alpha(look::kFaint));
                     for (const chess::Piece &piece : side.taken)
                     {
                         if (x + 34.0f > kSide + kSideWidth)
                             break;
                         ctx.pieces->draw(list, piece, {x, cy - 17.0f, 34.0f, 34.0f});
                         x += 22.0f;
                     }
                     list.pop_opacity();
                 }
             });

        // ---- what Cross does, level with the lower plate
        part(7,
             [&]()
             {
                 const float press = press_.value;
                 const Rect r = kAction.inset(2.0f * press);
                 look::panel(list, r, 0.55f + 0.45f * press, rose, kPlateRadius);
                 const ui::GlyphStyle glyphs = ui::GlyphStyle::dark();
                 constexpr float kGlyph = 36.0f;
                 constexpr float kSize = 24.0f;
                 // The button and the words are centred as one; words too long
                 // for the plate shrink before they are cut.
                 const char *words = tr("Watch full screen");
                 const float room = kAction.w - 2.0f * look::kPad -
                                    ui::button_width(ui::Button::cross, kGlyph) - 14.0f;
                 const float size = kSize * ui::fit_scale(fonts.semibold, words, kSize, room);
                 const float width = ui::button_width(ui::Button::cross, kGlyph) + 14.0f +
                                     std::min(fonts.semibold.measure(words, size), room);
                 const float x = r.cx() - width * 0.5f;
                 ui::draw_button(list, fonts, glyphs, ui::Button::cross, x, r.cy(), kGlyph);
                 ui::text_fit(list, fonts.semibold, words,
                              x + ui::button_width(ui::Button::cross, kGlyph) + 14.0f,
                              r.cy() + kSize * 0.35f, kSize, room, look::kInk);
             });
    }

    // A channel as the station on air: its sign in a ring, its name in the
    // display face and one quiet line.
    void draw_station(const app::Context &ctx, gfx::DrawList &list, int index, float alpha,
                      float dy) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = *ctx.fonts;
        const TvChannel &entry = tv_channels()[static_cast<std::size_t>(index)];
        const Color c = tv_channel_color(index);
        const float y = kHero.y + 84.0f;
        const float cx = kSide + kDisc * 0.5f;
        const float cy = y + kDisc * 0.5f;
        const float x = kSide + kDisc + 22.0f;
        const float room = kSide + kSideWidth - x;
        list.push_opacity(alpha);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, dy);
        list.glow({cx - kDisc * 0.5f, cy - kDisc * 0.5f, kDisc, kDisc}, kDisc * 0.5f, 22.0f,
                  c.with_alpha(0.18f));
        list.circle(cx, cy, kDisc * 0.5f, c.with_alpha(0.14f));
        list.ring(cx, cy, kDisc * 0.5f, 2.5f, c.with_alpha(0.85f));
        draw_tv_channel_sign(list, index, {cx - 26.0f, cy - 26.0f, 52.0f, 52.0f}, c);
        // The name is large: a long one may shrink further than other text.
        ui::text_fit(list, fonts.display, tr(entry.label), x - 2.0f, cy + 6.0f, 50.0f, room,
                     look::kInk, Align::left, 0.0f, 0.6f);
        ui::text_fit(list, fonts.regular, tr(entry.about), x, cy + 42.0f, 22.0f, room,
                     look::kInk.with_alpha(look::kMuted));
        list.pop_transform();
        list.pop_opacity();
    }

    // A player's name plate: whose pieces, the title as a gold tag, the name,
    // the rating as a figure and the clock as a ticker over its level. The
    // plate of the side to move is lit. shown: how much of the game is there.
    void draw_plate(const app::Context &ctx, gfx::DrawList &list, const Rect &r, const Side &side,
                    float shown) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Color rose = look::accent(look::Section::watch);
        const bool low = side.to_move && side.clock_ms >= 0 && side.clock_ms < kLowClockMs;
        const Color tint = low ? look::kBad : rose;
        const float lit = side.lit.value * shown;
        look::lift(list, r, lit * 0.6f, tint, ctx.time, calm_, kPlateRadius);
        look::panel(list, r, lit, tint, kPlateRadius);
        const Rect seat{r.x + 16.0f, r.cy() - 26.0f, 52.0f, 52.0f};
        const float left = seat.x + seat.w + 16.0f;
        if (shown < 0.99f)
        {
            // The bones of a plate.
            const Color bone = look::kInk.with_alpha(0.08f * (1.0f - shown));
            list.rounded_rect(seat, 14.0f, bone);
            list.rounded_rect({left, r.y + 22.0f, 190.0f, 16.0f}, 8.0f, bone);
            list.rounded_rect({left, r.y + 52.0f, 110.0f, 12.0f}, 6.0f, bone);
            list.rounded_rect({r.x + r.w - 20.0f - 120.0f, r.cy() - 14.0f, 120.0f, 28.0f}, 10.0f,
                              bone);
        }
        if (shown <= 0.01f || !game_.valid)
            return;
        list.push_opacity(shown);
        list.rounded_rect(seat, 14.0f, look::kInk.with_alpha(0.07f + 0.06f * lit));
        ctx.pieces->draw(list, {side.color, chess::Role::king}, seat.inset(3.0f));

        constexpr float kClockWidth = 150.0f;
        const bool clock = side.clock_ms >= 0;
        const float right = r.x + r.w - 20.0f - (clock ? kClockWidth + 16.0f : 0.0f);
        float x = left;
        const float first = r.y + 38.0f;
        if (!side.title.empty())
            x += look::tag(list, fonts, side.title, x, first - 9.0f, look::kGold, look::kNight,
                           Align::left, 15.0f) +
                 10.0f;
        ui::text(list, fonts.semibold, fonts.semibold.font->fit(side.name, 26.0f, right - x), x,
                 first, 26.0f, look::kInk);
        const float second = r.y + 66.0f;
        float sx = left;
        // The side's name leaves the rating its room before the clock.
        char rating[16] = "";
        if (side.rating > 0)
            std::snprintf(rating, sizeof(rating), "%d", static_cast<int>(side.shown.value + 0.5f));
        sx += kicker_fit(
                  list, fonts, side.color == chess::Color::white ? tr("White") : tr("Black"), sx,
                  second - 1.0f, right - sx - 12.0f - fonts.semibold.measure(rating, 20.0f),
                  gfx::mix(look::kInk.with_alpha(look::kFaint), tint, lit), Align::left, 13.0f) +
              12.0f;
        if (side.rating > 0)
            look::figure(list, fonts, rating, sx, second, 20.0f,
                         look::kInk.with_alpha(look::kMuted));
        if (clock)
        {
            const Color ink = gfx::mix(look::kInk.with_alpha(look::kFaint + 0.14f),
                                       low ? look::kBad : look::kInk, side.lit.value);
            look::ticker(list, fonts, app::format_clock(side.clock_ms), r.x + r.w - 20.0f,
                         r.y + 48.0f, 36.0f, ink, Align::right);
            const float share = side.peak_ms > 0 ? static_cast<float>(side.clock_ms) /
                                                       static_cast<float>(side.peak_ms)
                                                 : 0.0f;
            look::level(list,
                        {r.x + r.w - 20.0f - kClockWidth, r.y + r.h - 20.0f, kClockWidth, 5.0f},
                        share, gfx::mix(look::kInk.with_alpha(0.3f), rose, side.lit.value),
                        low && side.lit.value > 0.5f);
        }
        list.pop_opacity();
    }

    // ---- the channels --------------------------------------------------------

    void draw_tile(const app::Context &ctx, gfx::DrawList &list, int index) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const TvChannel &entry = tv_channels()[static_cast<std::size_t>(index)];
        const Color c = tv_channel_color(index);
        const Rect r = tile_rect(index);
        const float focus = focus_[static_cast<std::size_t>(index)].value;
        const float tuned = tuned_[static_cast<std::size_t>(index)].value;
        const float lit = std::max(focus, tuned);
        const bool mine = index == chosen_;
        // The tiles arrive from the right, one after the other.
        const float in = calm_ ? 1.0f : look::rise(since_, 2 + index, 0.05f, 0.5f);
        list.push_opacity(in);
        list.push_transform(1.0f - (mine ? 0.025f * press_.value : 0.0f), r.cx(), r.cy(),
                            look::settle(in, 30.0f) + (mine ? nudge_x() : 0.0f),
                            mine ? nudge_y() : 0.0f);
        look::lift(list, r, focus, c, ctx.time, calm_);
        look::panel(list, r, std::max(focus, tuned * 0.5f), c);

        const Rect plate{r.x + 20.0f, r.cy() - 32.0f, 64.0f, 64.0f};
        if (lit > 0.01f)
            list.glow(plate, 18.0f, 12.0f, c.with_alpha(0.32f * lit));
        list.rounded_rect(plate, 18.0f, gfx::mix(c.with_alpha(0.15f), c, lit));
        draw_tv_channel_sign(list, index, plate.inset(16.0f), gfx::mix(c, look::kNight, lit));

        // The words lean toward the hero while the tile has the controller.
        const float x = plate.x + plate.w + 18.0f + (calm_ ? 0.0f : 5.0f * focus);
        const float room = r.x + r.w - 46.0f - x;
        ui::text_fit(list, fonts.semibold, tr(entry.label), x, r.cy() - 1.0f, 26.0f, room,
                     look::kInk.with_alpha(tween::lerp(look::kMuted, 1.0f, lit)));
        // The tile is narrow: its quiet line may shrink a little more than other text.
        ui::text_fit(list, fonts.regular, tr(entry.about), x, r.cy() + 27.0f, 19.0f, room,
                     look::kInk.with_alpha(tween::lerp(look::kFaint, look::kMuted, lit)),
                     Align::left, 0.0f, 0.7f);

        // The channel the hero shows carries the mark of what is on air.
        if (tuned > 0.01f)
        {
            const float cx = r.x + r.w - 28.0f;
            list.push_opacity(tuned);
            if (on_air_ && !stale_)
                look::live_dot(list, cx, r.cy(), 5.0f, look::accent(look::Section::watch), ctx.time,
                               calm_);
            else
                list.ring(cx, r.cy(), 6.0f, 2.0f, look::kInk.with_alpha(look::kFaint));
            list.pop_opacity();
        }
        list.pop_transform();
        list.pop_opacity();
    }

    // ---- without a connection ------------------------------------------------

    void draw_offline(const app::Context &ctx, gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Color rose = look::accent(look::Section::watch);
        const Rect wide{app::kContent, app::kTop, app::kRight - app::kContent,
                        app::kBottom - app::kTop};
        const auto part = [&](int index, const auto &draw)
        {
            const float in = arrive(index);
            list.push_opacity(in);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in, 16.0f));
            draw();
            list.pop_transform();
            list.pop_opacity();
        };
        const float cx = wide.cx();
        part(0, [&]() { look::panel(list, wide, 0.0f, rose); });
        part(1,
             [&]()
             {
                 const Rect mark{cx - 80.0f, wide.y + 96.0f, 160.0f, 160.0f};
                 look::halo(list, mark.inset(-90.0f), rose, 0.16f);
                 draw_tv_offline_mark(list, mark, look::kInk, rose);
             });
        part(2,
             [&]()
             {
                 look::kicker(list, fonts, tr("Lichess TV"), cx, wide.y + 318.0f, rose,
                              Align::center);
                 ui::text_fit(list, fonts.display, tr("Lichess TV is out of reach"), cx,
                              wide.y + 384.0f, 52.0f, wide.w - 96.0f, look::kInk, Align::center);
             });
        part(3,
             [&]()
             {
                 ui::paragraph(list, fonts.regular,
                               tr("The best games being played on Lichess, move by move. "
                                  "Watching them needs the internet connection."),
                               cx, wide.y + 438.0f, 24.0f, 720.0f, 34.0f,
                               look::kInk.with_alpha(look::kMuted), 3, Align::center);
             });
        // What plays here once the connection is back: the channels, unlit.
        constexpr float kChip = 72.0f;
        const float pitch = (wide.w - 2.0f * 96.0f) / static_cast<float>(kChannelCount);
        look::rule(list, wide.x + 48.0f, wide.y + 566.0f, wide.w - 96.0f, 0.1f * arrive(4));
        for (int i = 0; i < kChannelCount; ++i)
        {
            part(4 + i,
                 [&]()
                 {
                     const Color c = tv_channel_color(i);
                     const float x = wide.x + 96.0f + (static_cast<float>(i) + 0.5f) * pitch;
                     const Rect chip{x - kChip * 0.5f, wide.y + 620.0f, kChip, kChip};
                     list.rounded_rect(chip, 20.0f, c.with_alpha(0.12f));
                     draw_tv_channel_sign(list, i, chip.inset(19.0f), c.with_alpha(0.75f));
                     ui::text_fit(list, fonts.semibold,
                                  tr(tv_channels()[static_cast<std::size_t>(i)].label), x,
                                  chip.y + kChip + 36.0f, 22.0f, pitch - 16.0f,
                                  look::kInk.with_alpha(look::kMuted), Align::center);
                 });
        }
    }

    ui::ListView channels_; // the channels' focus, sounds and refusals; never drawn
    ui::Skeleton board_;    // the board's bones until a game arrives

    int chosen_ = 0;         // the channel the controller is on
    float switch_in_ = 0.0f; // seconds until the stream follows it
    bool online_ = false;
    bool on_air_ = false; // the chosen channel's game is the one shown
    bool stale_ = false;  // the feed dropped and is being reconnected
    bool had_controller_ = false;
    bool in_tv_ = false;    // the full screen is open on top of the page
    lichess::TvGame game_;  // the last game shown (it fades out under the bones)
    Side sides_[2];         // top, bottom
    std::string last_move_; // "Nxd4", or its squares when the notation is unknown
    int white_points_ = 0;  // the material on the board
    int black_points_ = 0;

    // ---- motion ----
    float since_ = 0.0f;  // seconds since the page was entered: parts arrive by it
    bool calm_ = false;   // reduced motion
    bool placed_ = false; // springs were snapped for this visit
    std::array<tween::Spring, kChannelCount> focus_; // the tile has the controller
    std::array<tween::Spring, kChannelCount> tuned_; // the tile is the channel shown
    ui::SpringRect ring_;                            // glides between the tiles
    ui::SpringColor ring_ink_;
    tween::Spring ring_alpha_;
    ui::Pulse press_;       // Cross: the tile and the action squeeze
    ui::Pulse refusal_;     // a direction past the end of the channels
    ui::Pulse wall_;        // a direction with nothing beside the channels
    int from_ = 0;          // the station being left
    tween::Spring slide_;   // 0 -> 1 as the chosen station slides in
    tween::Timer swap_;     // another game of the same channel fades in
    ui::Pulse move_pop_;    // a move was played: the last move pops
    tween::Spring average_; // the players' average rating, counting
    tween::Spring share_;   // White's share of the material
};

} // namespace

std::unique_ptr<Page> make_watch_page(app::Context &ctx)
{
    return std::make_unique<WatchPage>(ctx);
}

} // namespace pch::modes
