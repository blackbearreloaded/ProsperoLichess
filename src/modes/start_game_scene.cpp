// ProsperoLichess - Waiting for a game: an open seek, or a challenge to the computer.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The waiting screen in the house look (docs/LOOK.md): the board that is being
// set up stands in the middle of a radar (rings that leave it, a beam that
// goes round), and beside it the ticket the Play page wrote says what was
// asked for, what is happening and how long it has taken. All of it is in the
// colour of the speed that was asked for. A request that fails turns the
// screen into one lit panel that says why.

#include "app/chrome.hpp"
#include "board/mini_board.hpp"
#include "core/strings.hpp"
#include "lichess/json.hpp"
#include "lichess/session.hpp"
#include "modes/scenes.hpp"
#include "platform/ps5/system.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
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

// The ticket, on the right ...
constexpr Rect kTicket{app::kRight - 560.0f, 302.0f, 560.0f, 480.0f};
constexpr float kPad = 32.0f;
constexpr float kTicketX = kTicket.x + kPad;
constexpr float kTicketRoom = kTicket.w - 2.0f * kPad;
constexpr float kNotch = 212.0f; // from the ticket's top to its perforation
constexpr float kBite = 13.0f;
// ... and the radar in the middle of the room that is left.
constexpr float kStageX = (app::kMargin + kTicket.x) * 0.5f;
constexpr float kStageY = (app::kTop + app::kBottom) * 0.5f;
constexpr float kBoard = 304.0f; // the squares of the board in its middle
constexpr float kDisc = 238.0f;  // the plate the board stands on
constexpr float kReach = 394.0f; // how far the rings travel
constexpr float kBeam = 0.16f;   // radians per piece of the beam's trail
constexpr int kBeamPieces = 6;
constexpr int kRings = 4;
// A request that failed says so in the middle of the screen.
constexpr Rect kFailed{580.0f, 302.0f, 760.0f, 480.0f};

// "time=10&increment=5": the number after key=, or fallback.
int form_number(std::string_view form, std::string_view key, int fallback)
{
    std::size_t at = 0;
    while (at < form.size())
    {
        const std::size_t end = std::min(form.find('&', at), form.size());
        const std::string_view pair = form.substr(at, end - at);
        if (pair.size() > key.size() && pair.substr(0, key.size()) == key &&
            pair[key.size()] == '=')
            return std::atoi(std::string(pair.substr(key.size() + 1)).c_str());
        at = end + 1;
    }
    return fallback;
}

bool form_has(std::string_view form, std::string_view pair)
{
    return form.find(pair) != std::string_view::npos;
}

// Small tracked capitals, as look::kicker draws them, that stay inside room.
float kicker_fit(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
                 float baseline, float room, Color color, Align align = Align::left,
                 float size = 16.0f)
{
    return ui::text_fit(list, fonts.semibold, ui::upper(text), x, baseline, size, room, color,
                        align, 3.0f);
}

class StartGameScene final : public app::Scene
{
  public:
    enum class Kind
    {
        seek,
        ai,
    };

    StartGameScene(Kind kind, std::string form, std::string label)
        : kind_(kind), form_(std::move(form)), label_(std::move(label))
    {
        describe();
    }

    ~StartGameScene() override
    {
        // The callbacks capture this scene: none may outlive it.
        if (session_ != nullptr && request_ != 0)
            session_->cancel(request_);
    }

    void enter(app::Context &ctx) override
    {
        if (started_)
            return;
        started_ = true;
        session_ = ctx.lichess;
        if (session_ == nullptr)
        {
            fail(tr("No connection to Lichess"));
            return;
        }
        if (!session_->signed_in() && !session_->signing_in())
        {
            // Lichess would refuse the request; say why instead of asking.
            fail(tr("Sign in to Lichess first: games are played on your account."));
            return;
        }
        asked_ = true;
        since_ = session_->event_sequence();
        session_->refresh_ongoing();
        if (kind_ == Kind::seek)
        {
            sys::log("[PCH] seek %s", form_.c_str());
            // A correspondence seek is posted and waits in the lobby; a
            // real-time one lives exactly as long as its stream stays open.
            if (correspondence())
                request_ = session_->post("/api/board/seek", form_,
                                          [this](const lichess::HttpResult &r) { seek_done(r); });
            else
                request_ = session_->stream(
                    "/api/board/seek", [](std::string_view) {},
                    [this](const lichess::HttpResult &r) { seek_done(r); }, form_);
        }
        else
        {
            sys::log("[PCH] challenge ai %s", form_.c_str());
            request_ = session_->post("/api/challenge/ai", form_,
                                      [this](const lichess::HttpResult &r)
                                      {
                                          request_ = 0;
                                          if (!r.ok())
                                          {
                                              fail(message(r));
                                              return;
                                          }
                                          lichess::Document doc(r.body);
                                          game_id_ = doc.root()["id"].str();
                                          if (game_id_.empty())
                                              fail(tr("Lichess did not create the game"));
                                      });
        }
    }

    app::Transition update(app::Context &ctx, const InputFrame &input, float dt) override
    {
        const bool calm = ctx.reduced_motion();
        elapsed_ += dt;
        // What the ticket says is happening changes without a jump.
        const char *now = title();
        if (now != title_now_)
        {
            title_old_ = title_now_;
            title_now_ = now;
            title_swap_.snap(calm || title_old_ == nullptr ? 1.0f : 0.0f);
        }
        title_swap_.target = 1.0f;
        title_swap_.update(dt, calm ? 60.0f : 12.0f);
        failed_in_.target = error_.empty() ? 0.0f : 1.0f;
        failed_in_.update(dt, calm ? 60.0f : 13.0f);
        if (announce_failure_)
        {
            // The request failed inside a callback, where there is no context.
            announce_failure_ = false;
            ctx.cue(audio::Cue::error);
        }
        if (game_id_.empty() && kind_ == Kind::seek)
        {
            for (const lichess::Event &event : ctx.lichess->events())
            {
                if (event.sequence > since_ && event.kind == lichess::Event::Kind::game_start)
                {
                    game_id_ = event.id;
                    break;
                }
            }
        }
        if (!game_id_.empty())
        {
            sys::log("[PCH] game start %s", game_id_.c_str());
            if (request_ != 0)
            {
                // The seek stream ends by itself when paired; stop listening.
                ctx.lichess->cancel(request_);
                request_ = 0;
            }
            return app::Transition::replace(make_online_game(ctx, game_id_));
        }
        if (input.is_pressed(Action::back) ||
            (!error_.empty() && input.is_pressed(Action::confirm)))
        {
            ctx.cue(audio::Cue::back);
            if (request_ != 0)
            {
                ctx.lichess->cancel(request_); // closing the seek cancels it
                request_ = 0;
            }
            if (kind_ == Kind::seek && asked_)
                ctx.lichess->del("/api/board/seek", [](const lichess::HttpResult &) {});
            return app::Transition::pop();
        }
        return app::Transition::stay();
    }

    void draw(app::Context &ctx, app::Frame &frame) const override
    {
        gfx::DrawList &list = frame.scene;
        const bool calm = ctx.reduced_motion();
        // A failure takes the waiting screen's place.
        const float failed = tween::clamp01(failed_in_.value);
        if (failed < 0.995f)
        {
            list.push_opacity(1.0f - failed);
            draw_radar(ctx, list, calm);
            draw_ticket(ctx, list, calm);
            list.pop_opacity();
        }
        if (failed > 0.005f)
            draw_failed(ctx, list, failed, calm);

        const ui::Hint hints[] = {{ui::Button::circle, error_.empty() ? TR("Cancel") : TR("Back")}};
        app::draw_hints(ctx, list, hints, 1);
    }

    app::look::Mood mood() const override
    {
        look::Mood mood = look::mood(look::Section::game);
        mood.accent = error_.empty() ? look::speed_color(speed_) : look::kGold;
        return mood;
    }

    const char *name() const override
    {
        return kind_ == Kind::seek ? "seek" : "ai-start";
    }

  private:
    bool correspondence() const
    {
        return form_.find("days=") != std::string::npos;
    }

    const char *title() const
    {
        if (kind_ == Kind::ai)
            return tr("Setting up the board");
        // A posted correspondence seek stays in the lobby after this screen.
        return waiting_id_.empty() ? tr("Finding an opponent") : tr("Waiting in the lobby");
    }

    // What the ticket's stub shows, read from the request itself.
    void describe()
    {
        int seconds = 0;
        int increment = 0;
        int days = 0;
        if (kind_ == Kind::seek)
        {
            days = form_number(form_, "days", 0);
            seconds = form_number(form_, "time", 0) * 60;
            increment = form_number(form_, "increment", 0);
            rated_ = form_has(form_, "rated=true");
        }
        else
        {
            seconds = form_number(form_, "clock.limit", 0);
            increment = form_number(form_, "clock.increment", 0);
            black_ = form_has(form_, "color=black");
        }
        if (days > 0)
        {
            speed_ = look::Speed::correspondence;
            control_ = plural(TR("{0} day"), TR("{0} days"), days);
        }
        else if (seconds <= 0)
        {
            speed_ = look::Speed::correspondence;
            control_ = tr("Unlimited");
        }
        else
        {
            // As Lichess sorts games: by how long forty moves take.
            const int estimate = seconds + 40 * increment;
            speed_ = estimate < 180    ? look::Speed::bullet
                     : estimate < 480  ? look::Speed::blitz
                     : estimate < 1500 ? look::Speed::rapid
                                       : look::Speed::classical;
            control_ = std::to_string(seconds / 60) + "+" + std::to_string(increment);
        }
        if (kind_ == Kind::ai)
        {
            kicker_ = fill(tr("Level {0}"), {std::to_string(form_number(form_, "level", 1))});
            return;
        }
        static constexpr const char *kNames[] = {TR("Bullet"), TR("Blitz"), TR("Rapid"),
                                                 TR("Classical"), TR("Correspondence")};
        kicker_ = tr(kNames[static_cast<int>(speed_)]);
    }

    void fail(std::string text)
    {
        error_ = std::move(text);
        announce_failure_ = true;
        // A request that could not even be made: the screen opens on the failure.
        if (elapsed_ <= 0.0f)
            failed_in_.snap(1.0f);
    }

    static std::string message(const lichess::HttpResult &r)
    {
        if (r.status == 0)
            return tr("No connection to Lichess");
        if (r.status == 429)
            return tr("Lichess asked us to slow down. Try again in a minute.");
        lichess::Document doc(r.body);
        const lichess::Value error = doc.root()["error"];
        if (error.is_object())
        {
            // Form errors: {"error":{"field":["message"]}}
            for (const char *key : {"global", "time", "increment", "days", "rated", "color"})
                if (error[key].size() > 0)
                    return error[key].at(0).str();
        }
        // What Lichess answers is in its own words.
        const std::string text = error.str();
        return text.empty() ? tr("Lichess refused the request") : text;
    }

    void seek_done(const lichess::HttpResult &r)
    {
        request_ = 0;
        if (!r.ok() && game_id_.empty())
            fail(message(r));
        else if (r.ok() && kind_ == Kind::seek && correspondence())
        {
            lichess::Document doc(r.body);
            // Correspondence seeks wait in the lobby; the game appears later.
            waiting_id_ = doc.root()["id"].str();
        }
    }

    // ---- drawing -----------------------------------------------------------

    // 0..1 for the index-th part of the screen since it opened.
    float arrive(int index, bool calm) const
    {
        return calm ? 1.0f : look::rise(elapsed_, index, 0.08f, 0.5f);
    }

    // The board being set up, in the middle of a radar.
    void draw_radar(const app::Context &ctx, gfx::DrawList &list, bool calm) const
    {
        const Color c = look::speed_color(speed_);
        const float cx = kStageX;
        const float cy = kStageY;
        const float breath = calm ? 0.5f : ui::breathe(elapsed_, 2.6f);

        list.push_opacity(arrive(0, calm));
        look::halo(list, {cx - kReach, cy - kReach, 2.0f * kReach, 2.0f * kReach}, c,
                   0.14f + 0.06f * breath);
        for (const float radius : {kDisc + 52.0f, kDisc + 104.0f, kReach})
            list.ring(cx, cy, radius, 1.5f, look::kInk.with_alpha(0.07f));
        // Rings that leave the board, one after another ...
        for (int i = 0; i < kRings; ++i)
        {
            const float offset = static_cast<float>(i) / static_cast<float>(kRings);
            const float along = calm ? offset + 0.12f : std::fmod(elapsed_ * 0.28f + offset, 1.0f);
            const float fade = std::min(along * 8.0f, 1.0f) * (1.0f - along);
            list.ring(cx, cy, tween::lerp(kDisc, kReach, along), 3.0f, c.with_alpha(0.6f * fade));
        }
        // ... and a beam that goes round, with its trail behind it.
        if (!calm)
        {
            const float angle = std::fmod(elapsed_ * 1.15f, kTau);
            // Sectors that all end at the beam, each longer than the last:
            // where they lie over one another the trail is brighter.
            for (int i = 0; i < kBeamPieces; ++i)
            {
                const float sweep = static_cast<float>(i + 1) * kBeam;
                list.arc(cx, cy, kReach, kReach - kDisc, std::fmod(angle - sweep + kTau, kTau),
                         sweep, c.with_alpha(0.035f), false);
            }
            list.line(cx + std::sin(angle) * kDisc, cy - std::cos(angle) * kDisc,
                      cx + std::sin(angle) * kReach, cy - std::cos(angle) * kReach, 2.5f,
                      c.with_alpha(0.7f));
        }
        list.pop_opacity();

        // The board on its plate arrives with a small pop.
        const float in = arrive(1, calm);
        const float pop = calm ? 1.0f : tween::lerp(0.9f, 1.0f, tween::back_out(in));
        list.push_opacity(in);
        list.push_transform(pop, cx, cy, 0.0f, 0.0f);
        list.circle(cx, cy, kDisc, look::kNight.with_alpha(0.62f));
        list.ring(cx, cy, kDisc, 2.5f, c.with_alpha(0.55f + 0.3f * breath));
        const Rect squares{cx - kBoard * 0.5f, cy - kBoard * 0.5f, kBoard, kBoard};
        look::frame_board(list, squares, c, 0.6f + 0.4f * breath);
        board::draw_mini_board(list, *ctx.pieces, ctx.board_theme(), squares,
                               chess::Position::start(),
                               black_ ? chess::Color::black : chess::Color::white, {}, 10.0f);
        list.pop_transform();
        list.pop_opacity();
    }

    template <typename Draw>
    void part(gfx::DrawList &list, int index, bool calm, const Draw &draw) const
    {
        const float in = arrive(index, calm);
        list.push_opacity(in);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in, 18.0f));
        draw();
        list.pop_transform();
        list.pop_opacity();
    }

    // What was asked for, what is happening, and for how long.
    void draw_ticket(const app::Context &ctx, gfx::DrawList &list, bool calm) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Color c = look::speed_color(speed_);
        const Rect t = kTicket;
        const float right = kTicketX + kTicketRoom;

        // ---- the paper: a tinted stub, a perforation with a bite out of each side
        part(list, 2, calm,
             [&]()
             {
                 look::panel(list, t, 0.4f, c);
                 list.push_clip({t.x, t.y, t.w, kNotch});
                 list.gradient_rect({t.x + 1.5f, t.y + 1.5f, t.w - 3.0f, kNotch + 80.0f},
                                    look::kRadius - 1.5f, c.with_alpha(0.17f), c.with_alpha(0.0f));
                 list.pop_clip();
                 const float y = t.y + kNotch;
                 const look::Mood &sky = look::mood(look::Section::game);
                 list.push_clip(t);
                 for (const float x : {t.x, t.x + t.w})
                 {
                     list.circle(x, y, kBite, gfx::mix(sky.sky[0], sky.sky[1], 0.6f));
                     list.ring(x, y, kBite, 1.5f, c.with_alpha(0.3f));
                 }
                 list.pop_clip();
                 for (float x = t.x + kBite + 11.0f; x + 8.0f <= t.x + t.w - kBite - 8.0f;
                      x += 16.0f)
                     list.rounded_rect({x, y - 1.0f, 8.0f, 2.0f}, 1.0f,
                                       look::kInk.with_alpha(0.2f));
             });

        // ---- the stub: who arranges the game, and what was asked for
        part(list, 3, calm,
             [&]()
             {
                 look::live_dot(list, kTicketX + 7.0f, t.y + 46.0f, 5.0f, c, elapsed_, calm);
                 // The tag keeps its place at the right; who arranges the
                 // game has the room that is left of it.
                 float tag = 0.0f;
                 if (kind_ == Kind::seek)
                     tag = look::tag(list, fonts, rated_ ? tr("Rated") : tr("Casual"), right,
                                     t.y + 46.0f, rated_ ? c : look::kInk.with_alpha(0.16f),
                                     rated_ ? look::kNight : look::kInk, Align::right, 14.0f) +
                           16.0f;
                 kicker_fit(list, fonts, kind_ == Kind::seek ? tr("Lichess pairing") : "Stockfish",
                            kTicketX + 26.0f, t.y + 52.0f, right - tag - (kTicketX + 26.0f), c);
                 const Rect sign{kTicketX, t.y + 78.0f, 72.0f, 72.0f};
                 list.glow(sign, 20.0f, 14.0f, c.with_alpha(0.3f));
                 list.rounded_rect(sign, 20.0f, c);
                 look::speed_icon(list, sign.inset(18.0f), speed_, look::kNight);
                 const float x = sign.x + sign.w + 20.0f;
                 kicker_fit(list, fonts, kicker_, x, t.y + 104.0f, right - x,
                            look::kInk.with_alpha(look::kMuted));
                 const float wide = fonts.display.measure(control_, 46.0f);
                 ui::text(list, fonts.display, control_, x - 2.0f, t.y + 147.0f,
                          wide > right - x ? 46.0f * (right - x) / wide : 46.0f, look::kInk);
                 ui::text_fit(list, fonts.regular, label_, kTicketX, t.y + 188.0f, 22.0f,
                              kTicketRoom, look::kInk.with_alpha(look::kMuted));
             });

        // ---- what is happening: the words change without a jump
        part(list, 4, calm,
             [&]()
             {
                 const float swap = tween::clamp01(title_swap_.value);
                 const auto words = [&](const char *text, float alpha, float dy)
                 {
                     if (text == nullptr || alpha <= 0.01f)
                         return;
                     const float wide = fonts.display.measure(text, 38.0f);
                     list.push_opacity(alpha);
                     ui::text(list, fonts.display, text, kTicketX - 2.0f, t.y + 274.0f + dy,
                              wide > kTicketRoom ? 38.0f * kTicketRoom / wide : 38.0f, look::kInk);
                     list.pop_opacity();
                 };
                 words(title_old_, 1.0f - swap, -8.0f * swap);
                 words(title_now_, swap, 10.0f * (1.0f - swap));
             });

        // ---- how long it has taken, and a light that keeps looking
        part(list, 5, calm,
             [&]()
             {
                 kicker_fit(list, fonts, trc("timer", "Waiting"), kTicketX, t.y + 322.0f,
                            kTicketRoom, look::kInk.with_alpha(look::kFaint + 0.1f), Align::left,
                            14.0f);
                 char clock[24];
                 const int seconds = static_cast<int>(elapsed_);
                 std::snprintf(clock, sizeof(clock), "%d:%02d", seconds / 60, seconds % 60);
                 look::ticker(list, fonts, clock, kTicketX - 3.0f, t.y + 398.0f, 76.0f, look::kInk);
                 const Rect track{kTicketX, t.y + 428.0f, kTicketRoom, 6.0f};
                 list.rounded_rect(track, 3.0f, look::kInk.with_alpha(0.1f));
                 constexpr float kLight = 132.0f;
                 const float along = calm ? 0.5f : 0.5f - 0.5f * std::cos(elapsed_ * 1.7f);
                 const Rect light{track.x + (track.w - kLight) * along, track.y, kLight, track.h};
                 list.glow(light, 3.0f, 10.0f, c.with_alpha(0.45f));
                 list.rounded_rect(light, 3.0f, c);
             });
    }

    // The request failed: one lit panel says why, in the words it had.
    void draw_failed(const app::Context &ctx, gfx::DrawList &list, float in, bool calm) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Color c = look::kGold;
        const Rect p = kFailed;
        const float cx = p.cx();
        list.push_opacity(in);
        list.push_transform(calm ? 1.0f : tween::lerp(0.94f, 1.0f, tween::back_out(in)), cx, p.cy(),
                            0.0f, 0.0f);
        look::lift(list, p, 0.7f, c, ctx.time, calm);
        look::panel(list, p, 0.7f, c);

        // A warning sign in a ring.
        const float sy = p.y + 104.0f;
        list.push_clip(p.inset(6.0f));
        look::halo(list, {cx - 200.0f, sy - 200.0f, 400.0f, 400.0f}, c, 0.2f);
        list.pop_clip();
        list.circle(cx, sy, 52.0f, c.with_alpha(0.14f));
        list.ring(cx, sy, 52.0f, 3.0f, c);
        list.rounded_rect({cx - 5.0f, sy - 27.0f, 10.0f, 34.0f}, 5.0f, c);
        list.circle(cx, sy + 21.0f, 6.5f, c);

        const float room = p.w - 96.0f;
        kicker_fit(list, fonts, kind_ == Kind::seek ? tr("Lichess pairing") : "Stockfish", cx,
                   p.y + 204.0f, room, c, Align::center);
        const char *title = tr("Could not start the game");
        const float wide = fonts.display.measure(title, 40.0f);
        ui::text(list, fonts.display, title, cx, p.y + 262.0f,
                 wide > room ? 40.0f * room / wide : 40.0f, look::kInk, Align::center);
        ui::paragraph(list, fonts.regular, error_, cx, p.y + 314.0f, 24.0f, p.w - 120.0f, 34.0f,
                      look::kInk.with_alpha(look::kMuted), 3, Align::center);

        // The way out, as the hint row says it.
        constexpr float kGlyph = 34.0f;
        const float glyph = ui::button_width(ui::Button::circle, kGlyph);
        const char *back = tr("Back");
        const float label = fonts.semibold.measure(back, 24.0f);
        const float x = cx - (glyph + 12.0f + label) * 0.5f;
        const float y = p.y + p.h - 58.0f;
        const Rect pill{x - 24.0f, y - 28.0f, glyph + 12.0f + label + 48.0f, 56.0f};
        list.rounded_rect(pill, 28.0f, look::kInk.with_alpha(0.07f));
        list.bordered_rect(pill, 28.0f, look::kClear, 1.5f, look::kInk.with_alpha(0.14f));
        ui::draw_button(list, fonts, ui::GlyphStyle::dark(), ui::Button::circle, x, y, kGlyph);
        ui::text(list, fonts.semibold, back, x + glyph + 12.0f, y + 24.0f * 0.35f, 24.0f,
                 look::kInk);
        list.pop_transform();
        list.pop_opacity();
    }

    Kind kind_;
    std::string form_;
    std::string label_;
    lichess::Session *session_ = nullptr;
    std::uint64_t request_ = 0;
    std::uint64_t since_ = 0;
    bool started_ = false;
    bool asked_ = false; // a request went to Lichess (there may be a seek to withdraw)
    float elapsed_ = 0.0f;
    std::string game_id_;
    std::string waiting_id_;
    std::string error_;
    bool announce_failure_ = false;

    // What the ticket shows, read from the request.
    look::Speed speed_ = look::Speed::rapid;
    std::string kicker_;  // "Rapid", "Level 3"
    std::string control_; // "10+5", "3 days", "Unlimited"
    bool rated_ = false;
    bool black_ = false; // the player asked for Black: the board is set up that way

    const char *title_now_ = nullptr; // what is happening ...
    const char *title_old_ = nullptr; // ... and what was, while it leaves
    tween::Spring title_swap_{1.0f, 0.0f, 1.0f};
    tween::Spring failed_in_; // 0 -> 1 as a failure takes the screen
};

} // namespace

std::unique_ptr<app::Scene> make_seek(std::string form, std::string label)
{
    return std::make_unique<StartGameScene>(StartGameScene::Kind::seek, std::move(form),
                                            std::move(label));
}

std::unique_ptr<app::Scene> make_ai_game(std::string form, std::string label)
{
    return std::make_unique<StartGameScene>(StartGameScene::Kind::ai, std::move(form),
                                            std::move(label));
}

} // namespace pch::modes
