// ProsperoLichess - Profile page: the account, its ratings and its games in progress.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The page in the house look (app/look.hpp), on the home page's four columns:
//
//   - the hero is the account: the avatar in a ring of light, the name in the
//     display face with the title as a gold tag, the games played counting to
//     their number, the time spent playing, and how the games ended as a donut
//     that grows, with its legend;
//   - the six ratings are tiles on a grid of their own, each in the colour and
//     with the sign of its way to play: the rating counts up, its line over
//     time grows, its recent change is marked, a provisional one says so, and
//     one the account does not have shows an honest empty tile;
//   - the games in progress are the home page's shelf: lit cards in the play
//     colour with one ring that glides;
//   - signed out, the hero invites: what an account brings as three tiles and
//     one clear button.

#include "board/mini_board.hpp"
#include "core/strings.hpp"
#include "lichess/session.hpp"
#include "modes/page.hpp"
#include "modes/play_setup.hpp"
#include "modes/scenes.hpp"
#include "ui/components/badge.hpp"
#include "ui/components/button.hpp"
#include "ui/components/carousel.hpp"
#include "ui/components/hold_button.hpp"
#include "ui/components/progress.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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
using look::Section;

constexpr float kWidth = app::kRight - app::kContent;
// The home page's four columns: the account spans two, the ratings are a
// grid in the other two, and the shelf below shows one card per column.
constexpr float kColumnWidth = (kWidth - 3.0f * app::kGap) / 4.0f;
constexpr float kColumnPitch = kColumnWidth + app::kGap;
constexpr Rect kHero{app::kContent, app::kTop, 2.0f * kColumnPitch - app::kGap, 476.0f};
constexpr float kPad = 28.0f;
constexpr float kAvatar = 128.0f;
constexpr float kFacts = 212.0f;     // from the hero's top to the rule over its figures
constexpr float kFactWidth = 152.0f; // the room of one of the two figures under it
// How the games ended, at the hero's right: a donut and its legend.
constexpr float kDonut = 72.0f;   // its radius
constexpr float kLegend = 138.0f; // the legend's width
constexpr float kResultsLeft = kHero.x + kHero.w - kPad - kLegend - 24.0f - 2.0f * kDonut;
constexpr float kTilesX = app::kContent + 2.0f * kColumnPitch;
constexpr float kShelfY = kHero.y + kHero.h + app::kGap;
constexpr float kShelfTitle = 40.0f; // the room the shelf's kicker takes
constexpr Rect kShelf{app::kContent, kShelfY + kShelfTitle, kWidth,
                      app::kBottom - kShelfY - kShelfTitle};
// Signed out, the hero spans the page and three tiles stand under it.
constexpr Rect kWelcome{app::kContent, app::kTop, kWidth, kShelf.y - app::kGap - app::kTop};
constexpr float kWelcomeBoard = 388.0f;
constexpr float kRefreshSeconds = 10.0f; // how often the games are asked for again

// What stands for a rating: one of the five speeds, or the puzzle piece.
enum class Sign
{
    speed,
    puzzle,
};

// The ratings shown, as Lichess orders its speeds, then the puzzles.
struct PerfLabel
{
    const char *key;
    const char *label;
    Sign sign; // puzzles are counted as solved, the speeds in games
    look::Speed speed;
};
constexpr PerfLabel kPerfs[] = {
    {"bullet", TR("Bullet"), Sign::speed, look::Speed::bullet},
    {"blitz", TR("Blitz"), Sign::speed, look::Speed::blitz},
    {"rapid", TR("Rapid"), Sign::speed, look::Speed::rapid},
    {"classical", TR("Classical"), Sign::speed, look::Speed::classical},
    {"correspondence", TR("Correspondence"), Sign::speed, look::Speed::correspondence},
    {"puzzle", TR("Puzzles"), Sign::puzzle, look::Speed::rapid},
};
constexpr std::size_t kPerfCount = sizeof(kPerfs) / sizeof(kPerfs[0]);

// What an account brings, for the page of someone who has none here yet.
struct Benefit
{
    const char *title;
    const char *body;
    Section section;
    app::NavIcon icon;
};
constexpr Benefit kBenefits[] = {
    {TR("Rated puzzles"),
     TR("Solve the daily puzzle and train with puzzles that count for your rating."),
     Section::puzzles, app::NavIcon::puzzles},
    {TR("Online games"), TR("Play rated and casual games against players from all over the world."),
     Section::play, app::NavIcon::play},
    {TR("Games in progress"),
     TR("Pick up the games you have going, on the console and everywhere else."), Section::profile,
     app::NavIcon::home},
};
constexpr int kBenefitCount = 3;

Color perf_color(const PerfLabel &perf)
{
    return perf.sign == Sign::puzzle ? look::accent(Section::puzzles)
                                     : look::speed_color(perf.speed);
}

// A rating's tile: two columns, three rows that share the hero's height
// exactly, with their edges on whole pixels.
Rect tile_rect(int index)
{
    const int column = index % 2;
    const int row = index / 2;
    const float pitch = (kHero.h + app::kGap) / 3.0f;
    const float top = std::floor(kHero.y + static_cast<float>(row) * pitch + 0.5f);
    const float next = std::floor(kHero.y + static_cast<float>(row + 1) * pitch + 0.5f);
    return {kTilesX + static_cast<float>(column) * kColumnPitch, top, kColumnWidth,
            next - app::kGap - top};
}

// One of the three tiles under the signed-out hero: they fill the width.
Rect benefit_rect(int index)
{
    const float pitch = (kWidth + app::kGap) / static_cast<float>(kBenefitCount);
    const float left = std::floor(app::kContent + static_cast<float>(index) * pitch + 0.5f);
    const float next = std::floor(app::kContent + static_cast<float>(index + 1) * pitch + 0.5f);
    return {left, kShelf.y, next - app::kGap - left, kShelf.h};
}

// What a game's clock still holds: "2d 4h left", "5h 12m left", "12:05 left".
std::string time_left(long long seconds)
{
    if (seconds < 0)
        return tr("No clock");
    if (seconds >= 86400)
        return fill(tr("{0}d {1}h left"),
                    {std::to_string(seconds / 86400), std::to_string((seconds % 86400) / 3600)});
    if (seconds >= 3600)
        return fill(tr("{0}h {1}m left"),
                    {std::to_string(seconds / 3600), std::to_string((seconds % 3600) / 60)});
    char clock[32];
    std::snprintf(clock, sizeof(clock), "%lld:%02lld", seconds / 60, seconds % 60);
    return fill(tr("{0} left"), {clock});
}

// Time spent playing: "17d 4h", "5h 12m", "12m".
std::string time_played(long long seconds)
{
    if (seconds >= 86400)
        return fill(tr("{0}d {1}h"),
                    {std::to_string(seconds / 86400), std::to_string((seconds % 86400) / 3600)});
    if (seconds >= 3600)
        return fill(tr("{0}h {1}m"),
                    {std::to_string(seconds / 3600), std::to_string((seconds % 3600) / 60)});
    return fill(tr("{0}m"), {std::to_string(seconds / 60)});
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

// Where a kicker too long for one line of `room` is broken to stand on two:
// the space nearest its middle. npos when it fits, or has no space.
std::size_t kicker_break(const ui::Fonts &fonts, std::string_view caps, float size, float room)
{
    std::size_t best = std::string_view::npos;
    if (fonts.semibold.measure(caps, size, 3.0f) <= room)
        return best;
    const std::size_t middle = caps.size() / 2;
    for (std::size_t at = caps.find(' '); at != std::string_view::npos; at = caps.find(' ', at + 1))
    {
        const std::size_t off = at > middle ? at - middle : middle - at;
        if (best == std::string_view::npos || off < (best > middle ? best - middle : middle - best))
            best = at;
    }
    return best;
}

// A kicker in a narrow place: one line on `baseline` when it fits `room`,
// else two (kicker_break), the upper one `pitch` above `second`.
void kicker_stack(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
                  float baseline, float second, float pitch, float room, Color color, Align align,
                  float size)
{
    const std::string caps = ui::upper(text);
    const std::size_t cut = kicker_break(fonts, caps, size, room);
    if (cut == std::string_view::npos)
    {
        ui::text_fit(list, fonts.semibold, caps, x, baseline, size, room, color, align, 3.0f);
        return;
    }
    const std::string_view lines(caps);
    ui::text_fit(list, fonts.semibold, lines.substr(0, cut), x, second - pitch, size, room, color,
                 align, 3.0f);
    ui::text_fit(list, fonts.semibold, lines.substr(cut + 1), x, second, size, room, color, align,
                 3.0f);
}

// A hold button as wide as its words need: `width` when the label and the
// hint fit it, wider up to `most` when they are longer (a translation), and
// with smaller letters when that is not enough.
void size_hold_button(const ui::Fonts &fonts, ui::HoldButton &button, float x, float y, float width,
                      float most)
{
    const ui::ButtonMetrics m = ui::button_metrics(button.style.size);
    const float around = 2.0f * m.padding +
                         ui::button_width(button.style.glyph, m.text_size * 1.3f) +
                         button.style.gap;
    const float words = std::max(fonts.semibold.measure(button.label, m.text_size),
                                 fonts.semibold.measure(button.hint, m.text_size));
    if (around + words > most)
        button.style.text_size = std::max(18.0f, m.text_size * (most - around - 12.0f) / words);
    // A button that has to grow takes a little more than the words measure:
    // it measures them again, inside its own border.
    const float needed = around + words;
    button.set_bounds({x, y, needed <= width ? width : std::min(needed + 12.0f, most), m.height});
}

// One game in progress, ready to draw.
struct Entry
{
    std::string game_id;
    chess::Position position = chess::Position::start();
    chess::Move last_move;
    chess::Color orientation = chess::Color::white;
    bool my_turn = false;
    std::string clock;  // "2d 4h left"
    std::string rating; // the opponent's: "Rating 1871"
};

// One rating of the account, as its tile shows it.
struct Rating
{
    bool known = false; // the account has this rating
    int rating = 0;
    bool provisional = false;
    int games = 0;
    int progress = 0;           // how it moved over the last games
    std::vector<float> history; // its line over time (empty: not fetched)
    tween::Spring shown;        // the number on screen, counting to the rating
};

// A spring's step toward its target; under reduced motion the value snaps.
void ease(tween::Spring &spring, float dt, float omega, bool calm)
{
    if (calm)
        spring.snap(spring.target);
    else
        spring.update(dt, omega);
}

class ProfilePage final : public Page
{
  public:
    explicit ProfilePage(app::Context &ctx)
    {
        const Color violet = look::accent(Section::profile);
        const Color green = look::accent(Section::play);

        // ---- signed out, signing in ----
        // Buttons are as wide as they were drawn for, or as a longer label
        // (a translation) needs.
        sign_in_.label = tr("Sign in");
        sign_in_.glyph = ui::Button::cross;
        sign_in_.style.role = ui::ButtonRole::primary;
        sign_in_.set_bounds({kWelcome.x + 56.0f, kWelcome.y + 384.0f,
                             std::clamp(sign_in_.preferred_width(*ctx.fonts), 296.0f, 560.0f),
                             64.0f});
        busy_.style.kind = ui::SpinnerKind::arc;
        busy_.style.color = violet;
        busy_.set_bounds({kWelcome.x + 56.0f, kWelcome.y + 384.0f, 64.0f, 64.0f});

        // ---- signed in ----
        avatar_.set_bounds({kHero.x + 44.0f, kHero.y + 40.0f, kAvatar, kAvatar});
        sign_out_.label = tr("Sign out");
        sign_out_.hint = tr("Hold to sign out");
        sign_out_.style.role = ui::ButtonRole::secondary;
        // Leaving is quiet: no launch fanfare for it.
        sign_out_.style.complete_cue = audio::Cue::back;
        // It may grow as far as the results' donut.
        size_hold_button(*ctx.fonts, sign_out_, kHero.x + kPad, kHero.y + kHero.h - kPad - 64.0f,
                         280.0f, kResultsLeft - 16.0f - (kHero.x + kPad));
        look::tint(violet, sign_in_, sign_out_);

        games_.style.item_width = kColumnWidth;
        games_.style.gap = app::kGap;
        games_.style.counter = false;
        games_.style.entrance_step = 0.0f;
        games_.style.peek = 0.0f;
        games_.style.exits.left = true; // back to the rail
        games_.style.card.art_aspect = 16.0f / 9.0f;
        games_.style.card.title_size = 24.0f;
        games_.style.card.subtitle_size = 20.0f;
        // The cards keep their places on the grid; the focused one is lit.
        games_.style.card.focus_scale = 1.0f;
        games_.style.card.lift = 0.0f;
        games_.style.card.glow = true;
        games_.style.theme.focus = green;
        games_.art = [this, &ctx](ui::Canvas &canvas, const Rect &art, float radius,
                                  const ui::CardItem &item, float)
        { draw_game_art(ctx, canvas, art, radius, item); };
        games_.set_bounds(kShelf);

        play_.label = tr("Play");
        play_.glyph = ui::Button::cross;
        play_.style.role = ui::ButtonRole::primary;
        const float play_width = std::clamp(play_.preferred_width(*ctx.fonts), 296.0f, 440.0f);
        play_.set_bounds(
            {kShelf.x + kShelf.w - 56.0f - play_width, kShelf.cy() - 32.0f, play_width, 64.0f});
        look::tint(green, play_);

        refresh(ctx, true);
    }

    void enter(app::Context &ctx) override
    {
        // The list is asked for when the page shows and every ten seconds after.
        if (ctx.lichess != nullptr)
            ctx.lichess->refresh_ongoing();
        ask_in_ = kRefreshSeconds;
        sign_out_.reset();
        refresh(ctx, false);
        assemble();
    }

    PageResult update(app::Context &ctx, const InputFrame &input, float dt, bool focused) override
    {
        ctx.calm(sign_in_, busy_, avatar_, sign_out_, games_, play_);
        calm_ = ctx.reduced_motion();
        since_ += dt;

        ask_in_ -= dt;
        if (ask_in_ <= 0.0f)
        {
            ask_in_ = kRefreshSeconds;
            if (ctx.lichess != nullptr && ctx.lichess->signed_in())
                ctx.lichess->refresh_ongoing();
        }
        // What the session knows is read twice a second.
        refresh_in_ -= dt;
        if (refresh_in_ <= 0.0f)
            refresh(ctx, false);

        PageResult result;
        // A direction that leads nowhere is answered with a nudge.
        if (focused && !input.nav_repeat && walled(input.nav))
            nudge_.trigger();
        nudge_.update(dt, 9.0f);
        if (focused)
        {
            if (input.is_pressed(Action::back))
            {
                ctx.cue(audio::Cue::back);
                result.to_rail = true;
            }
            else
            {
                switch (state_)
                {
                case State::signed_out:
                    if (input.nav == Direction::left)
                        result.to_rail = true;
                    else if (sign_in_.handle(input, *ctx.feedback) == ui::Event::activated)
                        result.transition = app::Transition::push(make_account());
                    break;
                case State::signing_in:
                    if (input.nav == Direction::left)
                        result.to_rail = true;
                    break;
                case State::signed_in:
                    handle_account(ctx, input, &result);
                    break;
                }
            }
        }
        else
        {
            sign_out_.reset();
        }

        const bool account = focused && state_ == State::signed_in;
        sign_in_.set_active(focused && state_ == State::signed_out);
        sign_out_.set_active(account && zone_ == Zone::sign_out);
        games_.set_active(account && zone_ == Zone::games && !entries_.empty());
        play_.set_active(account && zone_ == Zone::games && entries_.empty());

        // The hero is lit while the controller is in it; the numbers start
        // counting once their panels have arrived.
        hero_lit_.target = account && zone_ == Zone::sign_out ? 1.0f : 0.0f;
        ease(hero_lit_, dt, 14.0f, calm_);
        invite_lit_.target = focused && state_ == State::signed_out ? 1.0f : 0.0f;
        ease(invite_lit_, dt, 14.0f, calm_);
        if (calm_ || since_ > 0.35f)
        {
            ease(games_shown_, dt, 10.0f, calm_);
            for (Rating &rating : ratings_)
                ease(rating.shown, dt, 10.0f, calm_);
        }

        sign_in_.update(dt);
        busy_.update(dt);
        avatar_.update(dt);
        sign_out_.update(dt);
        games_.update(dt);
        play_.update(dt);
        return result;
    }

    void draw(app::Context &ctx, app::Frame &frame, bool) const override
    {
        gfx::DrawList &list = frame.scene;
        ui::Canvas canvas = app::canvas_for(ctx, list);
        if (state_ == State::signed_in)
            draw_account(ctx, canvas);
        else
            draw_welcome(ctx, canvas);
    }

    std::span<const ui::Hint> hints() const override
    {
        static constexpr ui::Hint kSignedOut[] = {{ui::Button::cross, TR("Sign in")},
                                                  {ui::Button::options, TR("Menu")}};
        static constexpr ui::Hint kWaiting[] = {{ui::Button::options, TR("Menu")}};
        static constexpr ui::Hint kGamesHints[] = {{ui::Button::cross, TR("Resume")},
                                                   {ui::Button::dpad, TR("Move")},
                                                   {ui::Button::options, TR("Menu")}};
        static constexpr ui::Hint kNoGames[] = {{ui::Button::cross, TR("Play")},
                                                {ui::Button::dpad, TR("Move")},
                                                {ui::Button::options, TR("Menu")}};
        static constexpr ui::Hint kSignOut[] = {{ui::Button::cross, TR("Hold to sign out")},
                                                {ui::Button::dpad, TR("Move")},
                                                {ui::Button::options, TR("Menu")}};
        switch (state_)
        {
        case State::signed_out:
            return kSignedOut;
        case State::signing_in:
            return kWaiting;
        case State::signed_in:
            break;
        }
        if (zone_ == Zone::sign_out)
            return kSignOut;
        if (entries_.empty())
            return kNoGames;
        return kGamesHints;
    }

    look::Section section() const override
    {
        return Section::profile;
    }

    const char *title() const override
    {
        return tr("Profile");
    }

    const char *name() const override
    {
        return "profile";
    }

  private:
    enum class State
    {
        signed_out,
        signing_in,
        signed_in,
    };
    enum class Zone
    {
        sign_out,
        games,
    };

    void handle_account(app::Context &ctx, const InputFrame &input, PageResult *result)
    {
        if (zone_ == Zone::sign_out)
        {
            if (input.nav == Direction::down)
            {
                zone_ = Zone::games;
                sign_out_.reset();
                ctx.cue(audio::Cue::focus);
                return;
            }
            if (input.nav == Direction::left)
            {
                sign_out_.reset();
                result->to_rail = true;
                return;
            }
            // Held to the end: the account leaves this console.
            if (sign_out_.handle(input, *ctx.feedback) == ui::Event::activated)
            {
                if (ctx.lichess != nullptr)
                    ctx.lichess->sign_out();
                sign_out_.reset();
                zone_ = Zone::games;
                if (ctx.notify)
                    ctx.notify(tr("Signed out"), app::Note::info);
                refresh(ctx, false);
            }
            return;
        }

        if (input.nav == Direction::up)
        {
            zone_ = Zone::sign_out;
            ctx.cue(audio::Cue::focus);
            return;
        }
        if (entries_.empty())
        {
            if (input.nav == Direction::left)
                result->to_rail = true;
            else if (play_.handle(input, *ctx.feedback) == ui::Event::activated)
                result->go_to = kPagePlay;
            return;
        }
        const ui::Event event = games_.handle(input, *ctx.feedback);
        if (event == ui::Event::none && games_.exit() == Direction::left)
        {
            result->to_rail = true;
            return;
        }
        if (event != ui::Event::activated)
            return;
        const Entry &entry = entries_[static_cast<std::size_t>(
            std::clamp(games_.focus(), 0, static_cast<int>(entries_.size()) - 1))];
        ctx.cue(audio::Cue::launch);
        result->transition = app::Transition::push(make_online_game(ctx, entry.game_id));
    }

    // True when nothing lies that way from where the controller is.
    bool walled(Direction nav) const
    {
        const bool up = nav == Direction::up;
        const bool down = nav == Direction::down;
        const bool right = nav == Direction::right;
        switch (state_)
        {
        case State::signed_out:
            return up || down || right;
        case State::signing_in:
            return false;
        case State::signed_in:
            break;
        }
        if (zone_ == Zone::sign_out)
            return up || right;
        // The shelf answers its own right end; the Play button has none.
        return down || (right && entries_.empty());
    }

    // Draws what has the controller, shaken while a nudge lasts.
    template <typename Draw> void nudged(gfx::DrawList &list, bool mine, const Draw &draw) const
    {
        const float dx = mine && !calm_ ? ui::shake(nudge_.value, since_, 7.0f) : 0.0f;
        list.push_transform(1.0f, 0.0f, 0.0f, dx, 0.0f);
        draw();
        list.pop_transform();
    }

    // The page assembles again: its parts arrive a moment apart and its
    // numbers count from nothing.
    void assemble()
    {
        since_ = 0.0f;
        games_shown_.value = 0.0f;
        games_shown_.velocity = 0.0f;
        for (Rating &rating : ratings_)
        {
            rating.shown.value = 0.0f;
            rating.shown.velocity = 0.0f;
        }
    }

    // Reads the account and its games from the session. first: settle
    // without animation.
    void refresh(app::Context &ctx, bool first)
    {
        refresh_in_ = 0.5f;
        const lichess::Session *session = ctx.lichess;
        const State before = state_;
        state_ = session == nullptr      ? State::signed_out
                 : session->signed_in()  ? State::signed_in
                 : session->signing_in() ? State::signing_in
                                         : State::signed_out;
        if (state_ != before && !first)
        {
            // The state's own entrance, and the focus back where it starts.
            assemble();
            zone_ = Zone::games;
        }
        if (state_ != State::signed_in)
        {
            entries_.clear();
            signature_.clear();
            return;
        }

        const lichess::Account &account = session->account();
        if (avatar_.name() != account.username)
            avatar_.set_name(account.username);
        name_ = account.username;
        title_ = account.title;
        games_played_ = account.games;
        wins_ = account.wins;
        draws_ = account.draws;
        losses_ = account.losses;
        play_seconds_ = account.play_seconds;
        games_shown_.target = static_cast<float>(games_played_);
        if (first)
            games_shown_.snap(games_shown_.target);

        // ---- ratings ----
        for (std::size_t i = 0; i < kPerfCount; ++i)
        {
            Rating &rating = ratings_[i];
            const lichess::Perf *perf = session->perf(kPerfs[i].key);
            rating.known = perf != nullptr;
            rating.rating = perf != nullptr ? perf->rating : 0;
            rating.provisional = perf != nullptr && perf->provisional;
            rating.games = perf != nullptr ? perf->games : 0;
            rating.progress = perf != nullptr ? perf->progress : 0;
            if (perf != nullptr)
                rating.history = perf->history;
            else
                rating.history.clear();
            rating.shown.target = static_cast<float>(rating.rating);
            if (first)
                rating.shown.snap(rating.shown.target);
        }

        // ---- games: the ones waiting for the player come first ----
        std::vector<Entry> entries;
        std::vector<ui::CardItem> items;
        for (const bool mine : {true, false})
        {
            for (const lichess::OngoingGame &game : session->ongoing())
            {
                if (game.my_turn != mine)
                    continue;
                Entry entry;
                entry.game_id = game.game_id;
                if (!chess::Position::from_fen(game.fen, &entry.position))
                    entry.position = chess::Position::start();
                if (game.last_move.size() >= 4)
                {
                    entry.last_move.from = chess::parse_square(game.last_move.substr(0, 2));
                    entry.last_move.to = chess::parse_square(game.last_move.substr(2, 2));
                }
                entry.orientation = game.color;
                entry.my_turn = game.my_turn;
                entry.clock = time_left(game.seconds_left);
                entry.rating = game.ai_level > 0 || game.opponent_rating <= 0
                                   ? std::string()
                                   : fill(tr("Rating {0}"), {std::to_string(game.opponent_rating)});
                ui::CardItem item;
                item.title =
                    fill(tr("vs {0}"), {game.ai_level > 0 ? fill(tr("Stockfish level {0}"),
                                                                 {std::to_string(game.ai_level)})
                                                          : game.opponent});
                // Two facts with a dot between them: the speed, and whether it counts.
                item.subtitle = speed_name(game.speed);
                if (!item.subtitle.empty())
                    item.subtitle += " \xC2\xB7 ";
                item.subtitle += game.rated ? tr("Rated") : tr("Casual");
                item.tag = static_cast<int>(entries.size());
                item.accent = look::accent(Section::play);
                items.push_back(std::move(item));
                entries.push_back(std::move(entry));
            }
        }
        // The shelf is rebuilt only when what it lists changed, so the focus
        // and the scroll stay where the player left them.
        std::string signature = account.id + "|";
        for (std::size_t i = 0; i < entries.size(); ++i)
            signature += entries[i].game_id + items[i].title + items[i].subtitle + "|";
        entries_ = std::move(entries);
        if (signature != signature_)
        {
            const int focus = games_.focus();
            signature_ = signature;
            const int count = static_cast<int>(items.size());
            games_.set_items(std::move(items));
            games_.set_focus(std::clamp(focus, 0, std::max(0, count - 1)));
        }
    }

    // 0..1 for the index-th part of the page since it assembled.
    float arrive(int index) const
    {
        return calm_ ? 1.0f : look::rise(since_, index, 0.06f, 0.5f);
    }

    // Draws one part inside its arrival: it fades in and settles upward.
    template <typename Draw> void part(gfx::DrawList &list, int index, const Draw &draw) const
    {
        const float in = arrive(index);
        list.push_opacity(in);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in, 18.0f));
        draw();
        list.pop_transform();
        list.pop_opacity();
    }

    // ---- signed in -------------------------------------------------------------

    void draw_account(const app::Context &ctx, ui::Canvas &canvas) const
    {
        gfx::DrawList &list = canvas.list;
        const ui::Fonts &fonts = *ctx.fonts;
        const Color violet = look::accent(Section::profile);
        const Color label = look::kInk.with_alpha(look::kFaint);
        const float left = kHero.x + kPad;
        const float right = kHero.x + kHero.w - kPad;

        // ---- who: the avatar in its ring, the name, where the account lives
        part(list, 0,
             [&]()
             {
                 look::lift(list, kHero, hero_lit_.value, violet, ctx.time, calm_);
                 look::panel(list, kHero, hero_lit_.value, violet);
                 list.push_clip(kHero.inset(2.0f));
                 look::halo(list, {kHero.x - 120.0f, kHero.y - 150.0f, 520.0f, 520.0f}, violet,
                            0.16f);
                 list.pop_clip();
             });
        part(list, 1,
             [&]()
             {
                 const Rect a = avatar_.bounds();
                 const float breath = calm_ ? 0.5f : ui::breathe(ctx.time, 3.0f);
                 list.glow(a, kAvatar * 0.5f, 24.0f, violet.with_alpha(0.2f + 0.14f * breath));
                 avatar_.draw(canvas);
                 list.ring(a.cx(), a.cy(), kAvatar * 0.5f + 8.0f, 3.0f, violet);
                 list.ring(a.cx(), a.cy(), kAvatar * 0.5f + 17.0f, 1.5f, violet.with_alpha(0.25f));
             });
        const float x = avatar_.bounds().x + kAvatar + 40.0f;
        part(list, 2,
             [&]()
             {
                 kicker_fit(list, fonts, tr("Lichess account"), x, kHero.y + 74.0f, right - x,
                            violet);
                 float name_x = x;
                 const float baseline = kHero.y + 134.0f;
                 if (!title_.empty())
                     name_x += look::tag(list, fonts, title_, x, baseline - 19.0f, look::kGold,
                                         look::kNight, Align::left, 19.0f) +
                               14.0f;
                 // A long name is set smaller before it is cut.
                 float size = 56.0f;
                 while (size > 32.0f && fonts.display.measure(name_, size) > right - name_x)
                     size -= 2.0f;
                 ui::text(list, fonts.display, fonts.display.font->fit(name_, size, right - name_x),
                          name_x - 2.0f, baseline, size, look::kInk);
                 ui::text(list, fonts.regular,
                          fonts.regular.font->fit("lichess.org/@/" + name_, 22.0f, right - x), x,
                          kHero.y + 172.0f, 22.0f, look::kInk.with_alpha(look::kMuted));
             });

        // ---- how much: games, time, and how the games ended
        part(list, 3,
             [&]()
             {
                 const float y = kHero.y + kFacts;
                 look::rule(list, left, y, right - left);
                 const float second = left + kFactWidth + 24.0f;
                 // Each heading ends before what stands to its right.
                 kicker_fit(list, fonts, tr("Games played"), left, y + 44.0f, second - 12.0f - left,
                            label, Align::left, 14.0f);
                 kicker_fit(list, fonts, tr("Time played"), second, y + 44.0f,
                            kResultsLeft - 12.0f - second, label, Align::left, 14.0f);
                 // The two figures share one size: the largest at which the
                 // longer of them still fits its column.
                 const std::string games = grouped(games_played_);
                 const std::string time = time_played(play_seconds_);
                 float size = 42.0f;
                 while (size > 28.0f && std::max(fonts.semibold.measure(games, size),
                                                 fonts.semibold.measure(time, size)) > kFactWidth)
                     size -= 2.0f;
                 if (games_played_ > 0)
                     look::figure(list, fonts,
                                  grouped(static_cast<long long>(games_shown_.value + 0.5f)),
                                  left - 1.0f, y + 94.0f, size, look::kInk);
                 else
                     look::figure(list, fonts, "\xE2\x80\x94", left, y + 94.0f, size, label);
                 if (play_seconds_ > 0)
                     ui::text_fit(list, fonts.semibold, time, second - 1.0f, y + 94.0f, size,
                                  kFactWidth, look::kInk);
                 else
                     look::figure(list, fonts, "\xE2\x80\x94", second, y + 94.0f, size, label);
             });
        part(list, 4, [&]() { draw_results(ctx, list); });
        part(list, 5,
             [&]() { nudged(list, zone_ == Zone::sign_out, [&]() { sign_out_.draw(canvas); }); });

        // ---- how strong
        for (int i = 0; i < static_cast<int>(kPerfCount); ++i)
            draw_rating(ctx, list, i);

        // ---- what is waiting
        part(list, 8,
             [&]()
             {
                 kicker_fit(list, fonts, tr("Games in progress"), app::kContent, kShelfY + 22.0f,
                            kWidth, look::kInk.with_alpha(look::kMuted));
                 if (!entries_.empty())
                 {
                     nudged(list, zone_ == Zone::games, [&]() { games_.draw(canvas); });
                     return;
                 }
                 draw_no_games(ctx, canvas);
             });
    }

    // Won, drawn, lost: a donut that grows, the share won in its middle, and
    // the three counts beside it.
    void draw_results(const app::Context &ctx, gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const float right = kHero.x + kHero.w - kPad;
        // Centred in the room under the rule.
        const float cy = kHero.y + (kFacts + kHero.h) * 0.5f;
        const float cx = right - kLegend - 24.0f - kDonut;
        const Color even = look::kInk.with_alpha(0.5f);
        const float shares[] = {static_cast<float>(wins_), static_cast<float>(draws_),
                                static_cast<float>(losses_)};
        const Color colors[] = {look::kGood, even, look::kBad};
        const float grow = calm_ ? 1.0f : look::rise(since_, 5, 0.08f, 0.9f);
        look::donut(list, cx, cy, kDonut, 14.0f, shares, colors, grow);
        // What is written inside the donut stays clear of its ring.
        const float hole = 2.0f * (kDonut - 14.0f) - 12.0f;
        const int decided = wins_ + draws_ + losses_;
        if (decided > 0)
        {
            const std::string text = percent(static_cast<int>(
                100.0f * static_cast<float>(wins_) / static_cast<float>(decided) * grow + 0.5f));
            look::figure(list, fonts, text, cx, cy + 5.0f, 30.0f, look::kInk, Align::center);
            kicker_fit(list, fonts, trc("share of games", "Won"), cx, cy + 27.0f, hole - 16.0f,
                       look::kInk.with_alpha(look::kFaint), Align::center, 13.0f);
        }
        else
        {
            kicker_stack(list, fonts, tr("No games"), cx, cy + 5.0f, cy + 13.0f, 16.0f, hole,
                         look::kInk.with_alpha(look::kFaint), Align::center, 13.0f);
        }
        // The legend: "1,301 won". The count is a figure, the words around it
        // are quieter; without games the words stand alone.
        static constexpr const char *kOutcomes[] = {TR("{0} won"), TR("{0} drawn"), TR("{0} lost")};
        // Exactly one game may need another word ("1 vit\xC3\xB3ria").
        static constexpr const char *kOutcome[] = {TRC("of one game", "{0} won"),
                                                   TRC("of one game", "{0} drawn"),
                                                   TRC("of one game", "{0} lost")};
        const int counts[] = {wins_, draws_, losses_};
        const Color quiet = look::kInk.with_alpha(look::kMuted);
        const float lx = right - kLegend;
        // A long line may run into the panel's padding, not over its edge.
        const float end = kHero.x + kHero.w - 8.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float line = cy - 40.0f + static_cast<float>(i) * 40.0f;
            list.circle(lx + 6.0f, line, 6.0f, colors[i]);
            const std::string_view pattern =
                decided > 0 && counts[i] == 1 ? trc("of one game", kOutcome[i]) : tr(kOutcomes[i]);
            const std::size_t at = pattern.find("{0}");
            std::string_view before = pattern.substr(0, std::min(at, pattern.size()));
            std::string_view after = at == std::string_view::npos ? "" : pattern.substr(at + 3);
            while (!before.empty() && before.back() == ' ')
                before.remove_suffix(1);
            while (!after.empty() && after.front() == ' ')
                after.remove_prefix(1);
            const std::string count = decided > 0 ? grouped(counts[i]) : std::string();
            float tx = lx + 22.0f;
            // Words before the count leave it its room.
            if (!before.empty())
                tx += ui::text_fit(
                          list, fonts.regular, before, tx, line + 7.0f, 19.0f,
                          end - tx -
                              (decided > 0 ? fonts.semibold.measure(count, 22.0f) + 8.0f : 0.0f),
                          quiet) +
                      8.0f;
            if (decided > 0)
                tx += look::figure(list, fonts, count, tx, line + 8.0f, 22.0f, look::kInk) + 8.0f;
            if (!after.empty())
                ui::text_fit(list, fonts.regular, after, tx, line + 7.0f, 19.0f, end - tx, quiet);
        }
    }

    // A rating: the sign and the name of its way to play in its colour, the
    // number counting up, how it moved lately, its line over time.
    void draw_rating(const app::Context &ctx, gfx::DrawList &list, int index) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const PerfLabel &perf = kPerfs[static_cast<std::size_t>(index)];
        const Rating &rating = ratings_[static_cast<std::size_t>(index)];
        const Rect r = tile_rect(index);
        const Color c = perf_color(perf);
        const float in = arrive(2 + index);
        list.push_opacity(in);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in));
        look::panel(list, r, 0.0f, c);

        const Rect plate{r.x + 18.0f, r.y + 18.0f, 46.0f, 46.0f};
        list.rounded_rect(plate, 14.0f, c.with_alpha(rating.known ? 0.16f : 0.08f));
        const Color ink = c.with_alpha(rating.known ? 1.0f : 0.5f);
        if (perf.sign == Sign::puzzle)
            app::draw_nav_icon(list, app::NavIcon::puzzles, plate.inset(11.0f), ink);
        else
            look::speed_icon(list, plate.inset(11.0f), perf.speed, ink);

        // The name is fitted: "Correspondence" is long for a tile.
        const float name_x = plate.x + plate.w + 14.0f;
        const float name_room = r.x + r.w - 20.0f - (rating.progress != 0 ? 64.0f : 0.0f) - name_x;
        const std::string name = ui::upper(tr(perf.label));
        float size = 16.0f;
        while (size > 13.0f && fonts.semibold.measure(name, size, 3.0f) > name_room)
            size -= 1.0f;
        kicker_fit(list, fonts, name, name_x, r.y + 47.0f, name_room, ink, Align::left, size);

        const Rect chart{r.x + r.w - 20.0f - 150.0f, r.y + 62.0f, 150.0f, 48.0f};
        if (!rating.known)
        {
            // The account has no such rating: an honest empty tile.
            look::figure(list, fonts, "\xE2\x80\x94", r.x + 20.0f, r.y + 104.0f, 44.0f,
                         look::kInk.with_alpha(look::kFaint));
            ui::text_fit(list, fonts.regular, tr("No rating yet"), r.x + 20.0f, r.y + r.h - 12.0f,
                         19.0f, r.w - 40.0f, look::kInk.with_alpha(look::kFaint));
            look::sparkline(list, chart, {}, c);
            list.pop_transform();
            list.pop_opacity();
            return;
        }
        look::delta(list, fonts, rating.progress, r.x + r.w - 20.0f, r.y + 47.0f, 19.0f,
                    Align::right);
        char text[16];
        std::snprintf(text, sizeof(text), "%d", static_cast<int>(rating.shown.value + 0.5f));
        const float width =
            look::figure(list, fonts, text, r.x + 18.0f, r.y + 104.0f, 44.0f, look::kInk);
        // A provisional rating carries Lichess' question mark.
        if (rating.provisional)
            look::figure(list, fonts, "?", r.x + 18.0f + width + 5.0f, r.y + 104.0f, 30.0f, c);
        // How much stands behind it: games played, or puzzles solved (plural()
        // would write the number without its comma). Then, after a dot, that
        // it is provisional.
        const std::string count = grouped(rating.games);
        std::string note =
            perf.sign == Sign::puzzle
                ? fill(tr("{0} solved"), {count})
                : fill(rating.games == 1 ? tr("{0} game") : tr("{0} games"), {count});
        if (rating.provisional)
            note += std::string(" \xC2\xB7 ") + tr("provisional");
        ui::text_fit(list, fonts.regular, note, r.x + 20.0f, r.y + r.h - 12.0f, 19.0f, r.w - 40.0f,
                     look::kInk.with_alpha(look::kFaint));
        const float grow = calm_ ? 1.0f : look::rise(since_, 5 + index, 0.08f, 0.9f);
        look::sparkline(list, chart, rating.history, c, grow);
        list.pop_transform();
        list.pop_opacity();
    }

    // No game is going: the shelf's place says so and offers one.
    void draw_no_games(const app::Context &ctx, ui::Canvas &canvas) const
    {
        gfx::DrawList &list = canvas.list;
        const ui::Fonts &fonts = *ctx.fonts;
        const Color green = look::accent(Section::play);
        look::panel(list, kShelf, 0.0f, green);
        const float cy = kShelf.cy();
        const float cx = kShelf.x + 56.0f + 64.0f;
        list.circle(cx, cy, 64.0f, green.with_alpha(0.12f));
        list.ring(cx, cy, 64.0f, 2.5f, green.with_alpha(0.7f));
        ctx.pieces->draw(list, {chess::Color::white, chess::Role::pawn},
                         {cx - 46.0f, cy - 48.0f, 92.0f, 92.0f});
        const float x = cx + 64.0f + 40.0f;
        const float room = play_.bounds().x - 48.0f - x;
        ui::text_fit(list, fonts.display, tr("No games in progress"), x - 2.0f, cy - 22.0f, 40.0f,
                     room, look::kInk);
        ui::paragraph(list, fonts.regular,
                      tr("Start a game and it waits for you here, on the console and on every "
                         "other device you play on."),
                      x, cy + 22.0f, 23.0f, room, 33.0f, look::kInk.with_alpha(look::kMuted), 3);
        nudged(list, zone_ == Zone::games, [&]() { play_.draw(canvas); });
    }

    void draw_game_art(const app::Context &ctx, ui::Canvas &canvas, const Rect &art, float radius,
                       const ui::CardItem &item) const
    {
        if (item.tag < 0 || item.tag >= static_cast<int>(entries_.size()))
            return;
        const Entry &entry = entries_[static_cast<std::size_t>(item.tag)];
        gfx::DrawList &list = canvas.list;
        const ui::Theme &theme = ctx.theme();
        const ui::Fonts &fonts = canvas.fonts;
        const Color green = look::accent(Section::play);
        list.gradient_rect(art, radius, gfx::mix(theme.page, green, 0.2f).with_alpha(0.92f),
                           gfx::mix(theme.page, green, 0.05f).with_alpha(0.92f));
        list.bordered_rect(art, radius, look::kClear, 1.5f, look::kInk.with_alpha(0.1f));
        const float side = art.h - 32.0f;
        const Rect squares{art.x + 16.0f, art.y + 16.0f, side, side};
        board::draw_mini_board(list, *ctx.pieces, ctx.board_theme(), squares, entry.position,
                               entry.orientation, entry.last_move, 5.0f);
        const float tx = squares.x + side + 22.0f;
        const float room = art.x + art.w - tx - 14.0f;

        // Whose turn it is: a game that waits for the player is live.
        // The column is narrow: words too long for a line of it stand on two.
        const float top = art.cy() - 34.0f;
        if (entry.my_turn)
        {
            // The dot stands beside the first line.
            const char *words = tr("Your move");
            const bool two = kicker_break(fonts, ui::upper(words), 13.0f, room - 18.0f) !=
                             std::string_view::npos;
            look::live_dot(list, tx + 5.0f, top - (two ? 16.0f : 0.0f) - 5.0f, 4.5f, green,
                           canvas.time, calm_);
            kicker_stack(list, fonts, words, tx + 18.0f, top, top, 16.0f, room - 18.0f, green,
                         Align::left, 13.0f);
        }
        else
        {
            kicker_stack(list, fonts, tr("Their move"), tx, top, top, 16.0f, room,
                         look::kInk.with_alpha(look::kMuted), Align::left, 13.0f);
        }
        // The clock is what the card is read for: with a language's longer
        // words it is set smaller than other lines before it is cut.
        ui::text_fit(list, fonts.semibold, entry.clock, tx, art.cy() + 2.0f, 24.0f, room,
                     look::kInk, Align::left, 0.0f, 0.6f);
        if (!entry.rating.empty())
            ui::text_fit(list, fonts.regular, entry.rating, tx, art.cy() + 32.0f, 19.0f, room,
                         look::kInk.with_alpha(look::kMuted));
    }

    // ---- signed out, signing in ------------------------------------------------

    void draw_welcome(const app::Context &ctx, ui::Canvas &canvas) const
    {
        gfx::DrawList &list = canvas.list;
        const ui::Fonts &fonts = *ctx.fonts;
        const Color violet = look::accent(Section::profile);
        const bool busy = state_ == State::signing_in;
        const float x = kWelcome.x + 56.0f;
        const Rect squares{kWelcome.x + kWelcome.w - 60.0f - kWelcomeBoard,
                           kWelcome.cy() - kWelcomeBoard * 0.5f, kWelcomeBoard, kWelcomeBoard};
        const float room = squares.x - 72.0f - x;

        // ---- the invitation
        part(list, 0,
             [&]()
             {
                 look::lift(list, kWelcome, invite_lit_.value, violet, ctx.time, calm_);
                 look::panel(list, kWelcome, invite_lit_.value, violet);
                 list.push_clip(kWelcome.inset(2.0f));
                 look::halo(list, squares.inset(-220.0f), violet, 0.2f);
                 list.pop_clip();
                 // A board set up and waiting, on the light of the page.
                 look::frame_board(list, squares.inset(-10.0f), violet,
                                   0.6f + 0.4f * invite_lit_.value);
                 board::draw_mini_board(list, *ctx.pieces, ctx.board_theme(), squares,
                                        chess::Position::start(), chess::Color::white, {}, 10.0f);
             });
        part(list, 1,
             [&]()
             {
                 const Rect tile{x, kWelcome.y + 74.0f, 44.0f, 44.0f};
                 list.rounded_rect(tile, 13.0f, violet.with_alpha(0.16f));
                 app::draw_nav_icon(list, app::NavIcon::profile, tile.inset(10.0f), violet);
                 kicker_fit(list, fonts, tr("Lichess account"), tile.x + tile.w + 16.0f,
                            tile.cy() + 6.0f, room - tile.w - 16.0f, violet);
             });
        part(list, 2,
             [&]()
             {
                 ui::text_fit(list, fonts.display,
                              busy ? tr("Signing in") : tr("Sign in to Lichess"), x - 4.0f,
                              kWelcome.y + 212.0f, 70.0f, room, look::kInk);
             });
        part(list, 3,
             [&]()
             {
                 // Three lines stand over the button; the English words take two.
                 ui::paragraph(list, fonts.regular,
                               busy ? tr("Fetching your account from Lichess")
                                    : tr("Play rated puzzles and online games, and pick up your "
                                         "games in progress on this console."),
                               x, kWelcome.y + 270.0f, 25.0f, std::min(room, 680.0f), 36.0f,
                               look::kInk.with_alpha(look::kMuted), 3);
             });
        part(list, 4,
             [&]()
             {
                 if (busy)
                     busy_.draw(canvas);
                 else
                     nudged(list, true, [&]() { sign_in_.draw(canvas); });
             });

        // ---- what an account brings
        for (int i = 0; i < kBenefitCount; ++i)
        {
            part(list, 4 + i,
                 [&]()
                 {
                     const Benefit &benefit = kBenefits[i];
                     const Color c = look::accent(benefit.section);
                     const Rect r = benefit_rect(i);
                     look::panel(list, r, 0.0f, c);
                     const Rect plate{r.x + kPad, r.y + kPad, 64.0f, 64.0f};
                     list.rounded_rect(plate, 18.0f, c.with_alpha(0.16f));
                     if (i == 2)
                     {
                         // Games that wait: a clock with its hands.
                         const float cx = plate.cx();
                         const float cy = plate.cy();
                         list.ring(cx, cy, 16.0f, 3.0f, c);
                         list.line(cx, cy, cx, cy - 9.0f, 3.0f, c);
                         list.line(cx, cy, cx + 7.0f, cy + 4.0f, 3.0f, c);
                     }
                     else
                     {
                         app::draw_nav_icon(list, benefit.icon, plate.inset(18.0f), c);
                     }
                     ui::text_fit(list, fonts.semibold, tr(benefit.title), r.x + kPad, r.y + 140.0f,
                                  30.0f, r.w - 2.0f * kPad, look::kInk);
                     ui::paragraph(list, fonts.regular, tr(benefit.body), r.x + kPad, r.y + 182.0f,
                                   22.0f, r.w - 2.0f * kPad, 32.0f,
                                   look::kInk.with_alpha(look::kMuted), 3);
                 });
        }
    }

    // Signed out and signing in.
    ui::PushButton sign_in_;
    ui::Spinner busy_;
    // Signed in.
    ui::Avatar avatar_;
    ui::HoldButton sign_out_;
    ui::Carousel games_;
    ui::PushButton play_;

    State state_ = State::signed_out;
    Zone zone_ = Zone::games;
    float ask_in_ = kRefreshSeconds;
    float refresh_in_ = 0.0f;
    std::string name_;
    std::string title_; // "GM", when the account has a title
    int games_played_ = 0;
    int wins_ = 0;
    int draws_ = 0;
    int losses_ = 0;
    long long play_seconds_ = 0;
    std::array<Rating, kPerfCount> ratings_;
    std::vector<Entry> entries_;
    std::string signature_;

    // ---- motion ----
    float since_ = 0.0f;        // seconds since the page assembled: parts arrive by it
    bool calm_ = false;         // reduced motion
    tween::Spring hero_lit_;    // 0..1: the controller is on Sign out
    tween::Spring invite_lit_;  // 0..1: the controller is on Sign in
    tween::Spring games_shown_; // the games played on screen, counting
    ui::Pulse nudge_;           // a direction that leads nowhere
};

} // namespace

std::unique_ptr<Page> make_profile_page(app::Context &ctx)
{
    return std::make_unique<ProfilePage>(ctx);
}

} // namespace pch::modes
