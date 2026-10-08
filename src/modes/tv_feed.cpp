// ProsperoLichess - Lichess TV: its channels and the game on air, for both TV screens.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modes/tv_feed.hpp"

#include "core/strings.hpp"

#include <algorithm>

namespace pch::modes
{

namespace
{

namespace look = app::look;

// Standard chess and Chess960 only: the board does not know the other variants.
// Chess960 is a name; the other labels and the lines are text.
constexpr TvChannel kChannels[] = {
    {"", TR("Top rated"), TR("Best game right now")},
    {"blitz", TR("Blitz"), TR("3 to 8 minutes")},
    {"rapid", TR("Rapid"), TR("8 to 25 minutes")},
    {"classical", TR("Classical"), TR("25 minutes and more")},
    {"bullet", TR("Bullet"), TR("Under 3 minutes")},
    {"chess960", "Chess960", TR("Shuffled back rank")},
};

// How a channel is drawn: one of the five speeds, or a sign of its own.
enum class Sign
{
    star,
    speed,
    shuffle,
};
struct Dress
{
    Sign sign;
    look::Speed speed;
};
constexpr Dress kDress[] = {
    {Sign::star, look::Speed::rapid},   {Sign::speed, look::Speed::blitz},
    {Sign::speed, look::Speed::rapid},  {Sign::speed, look::Speed::classical},
    {Sign::speed, look::Speed::bullet}, {Sign::shuffle, look::Speed::rapid},
};
static_assert(std::size(kDress) == std::size(kChannels));

const Dress &dress(int index)
{
    return kDress[static_cast<std::size_t>(
        std::clamp(index, 0, static_cast<int>(std::size(kDress)) - 1))];
}

struct StandIn
{
    bool set = false;
    lichess::TvGame game;
};

StandIn &stand_in()
{
    static StandIn instance;
    return instance;
}

// The placement and the side to move of a FEN: what the feed is sure about
// (it leaves castling rights and move counters out, or sends them as it likes).
std::string board_part(const std::string &fen)
{
    const std::size_t first = fen.find(' ');
    if (first == std::string::npos)
        return fen;
    const std::size_t second = fen.find(' ', first + 1);
    return fen.substr(0, second);
}

} // namespace

std::span<const TvChannel> tv_channels()
{
    return kChannels;
}

int tv_channel_index(std::string_view id)
{
    for (std::size_t i = 0; i < std::size(kChannels); ++i)
    {
        if (id == kChannels[i].id)
            return static_cast<int>(i);
    }
    return 0;
}

gfx::Color tv_channel_color(int index)
{
    switch (dress(index).sign)
    {
    case Sign::star:
        return look::accent(look::Section::watch);
    case Sign::shuffle:
        return look::accent(look::Section::profile);
    case Sign::speed:
        break;
    }
    return look::speed_color(dress(index).speed);
}

void draw_tv_channel_sign(gfx::DrawList &list, int index, const gfx::Rect &box, gfx::Color ink)
{
    const float s = std::min(box.w, box.h);
    const float cx = box.cx();
    const float cy = box.cy();
    switch (dress(index).sign)
    {
    case Sign::star:
        list.star(cx, cy + s * 0.02f, s * 0.54f, ink);
        break;
    case Sign::shuffle:
    {
        // Two arrows that cross: the pieces of the back rank change places.
        const float pen = std::max(s * 0.1f, 2.4f);
        const float head = s * 0.3f;
        list.arrow(cx - s * 0.46f, cy + s * 0.27f, cx + s * 0.48f, cy - s * 0.27f, pen, head, ink);
        list.arrow(cx - s * 0.46f, cy - s * 0.27f, cx + s * 0.48f, cy + s * 0.27f, pen, head, ink);
        break;
    }
    case Sign::speed:
        look::speed_icon(list, box, dress(index).speed, ink);
        break;
    }
}

const lichess::TvGame &tv_game(const app::Context &ctx)
{
    static const lichess::TvGame kNothing;
    if (stand_in().set)
        return stand_in().game;
    return ctx.lichess != nullptr ? ctx.lichess->tv() : kNothing;
}

bool tv_on_air(const lichess::TvGame &game, std::string_view channel)
{
    return game.valid && game.channel == channel;
}

void set_tv_stand_in(const lichess::TvGame *game)
{
    stand_in().set = game != nullptr;
    stand_in().game = game != nullptr ? *game : lichess::TvGame{};
}

long long tv_clock_ms(const lichess::TvGame &game, chess::Color color, double now)
{
    const long long seconds = color == chess::Color::white ? game.white_clock : game.black_clock;
    if (seconds < 0)
        return -1;
    double left = static_cast<double>(seconds) * 1000.0;
    if (color == game.position.turn())
        left -= std::max(now - game.received, 0.0) * 1000.0;
    return static_cast<long long>(std::max(left, 0.0));
}

std::string tv_game_link(const lichess::TvGame &game)
{
    return game.id.empty() ? std::string() : "lichess.org/" + game.id;
}

std::string tv_last_san(const chess::Position &before, const lichess::TvGame &game)
{
    if (!game.last_move.valid() || !before.is_legal(game.last_move) ||
        board_part(before.after(game.last_move).fen()) != board_part(game.position.fen()))
        return {};
    return chess::to_san(before, game.last_move);
}

std::string tv_move_squares(const chess::Move &move)
{
    if (!move.valid())
        return {};
    return chess::square_name(move.from) + "-" + chess::square_name(move.to);
}

int tv_material(const chess::Position &position, chess::Color color)
{
    constexpr chess::Role kRoles[] = {chess::Role::queen, chess::Role::rook, chess::Role::bishop,
                                      chess::Role::knight, chess::Role::pawn};
    constexpr int kWorth[] = {9, 5, 3, 3, 1};
    int total = 0;
    for (std::size_t i = 0; i < 5; ++i)
        total += kWorth[i] * position.count(color, kRoles[i]);
    return total;
}

void draw_tv_offline_mark(gfx::DrawList &list, const gfx::Rect &area, gfx::Color ink,
                          gfx::Color accent)
{
    const float cx = area.cx();
    const float cy = area.cy();
    const float r = std::min(area.w, area.h) * 0.5f;
    const float pen = std::max(r * 0.07f, 3.0f);
    list.circle(cx, cy, r, ink.with_alpha(0.08f));
    list.ring(cx, cy, r, 2.0f, ink.with_alpha(0.22f));
    list.circle(cx, cy, r * 0.11f, ink.with_alpha(0.9f));
    list.ring(cx, cy, r * 0.34f, pen, ink.with_alpha(0.6f));
    list.ring(cx, cy, r * 0.58f, pen, ink.with_alpha(0.32f));
    list.line(cx - r * 0.5f, cy + r * 0.5f, cx + r * 0.5f, cy - r * 0.5f, pen * 1.4f, accent);
}

} // namespace pch::modes
