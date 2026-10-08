// ProsperoLichess - Lichess TV: its channels and the game on air, for both TV screens.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/scene.hpp"
#include "lichess/session.hpp"

#include <span>
#include <string>
#include <string_view>

namespace pch::modes
{

// The label and the line are English, marked TR("..."): the screens translate
// them where they draw them.
struct TvChannel
{
    const char *id;    // what Session::watch_tv takes; "" is the featured game
    const char *label; // "Blitz"
    const char *about; // one quiet line on what plays there
};

// The channels on offer, in the order they are listed.
std::span<const TvChannel> tv_channels();
// The index of a channel id; 0 (the featured game) when it is not one of ours.
int tv_channel_index(std::string_view id);
// The colour a channel is drawn in: the featured game has the Watch page's
// rose, the four speeds have their own, Chess960 is violet.
gfx::Color tv_channel_color(int index);
// A channel's sign in one colour: a star for the featured game, the speed's
// own sign, two crossing arrows for the shuffled start of Chess960.
void draw_tv_channel_sign(gfx::DrawList &list, int index, const gfx::Rect &box, gfx::Color ink);

// The game Lichess TV shows right now: the session's (invalid until the first
// one arrives, or without a session), or the stand-in when one is set.
const lichess::TvGame &tv_game(const app::Context &ctx);
// True when that game is the one `channel` shows.
bool tv_on_air(const lichess::TvGame &game, std::string_view channel);

// Host pictures and tests only: a game to show instead of the session's, since
// the TV stream cannot be reached offline. nullptr returns to the session.
void set_tv_stand_in(const lichess::TvGame *game);

// A player's clock in milliseconds at session time `now`: the feed gives whole
// seconds with each move, and the side to move counts down from there. -1
// when the game has no clock.
long long tv_clock_ms(const lichess::TvGame &game, chess::Color color, double now);

// "lichess.org/U5MtVuM2": where the game can be found, empty without an id.
std::string tv_game_link(const lichess::TvGame &game);

// The feed carries a position and the move that led to it, not the game's
// moves. When `before` (the position this screen showed) leads to the game's
// position by that move, this is the move in standard notation ("Nxd4");
// empty when it does not (another game, or moves were missed).
std::string tv_last_san(const chess::Position &before, const lichess::TvGame &game);
// "e2-e4": the squares of a move, all there is to say about one seen without
// the position before it. Empty for no move.
std::string tv_move_squares(const chess::Move &move);
// What one side's pieces are worth, in pawns (the king counts for nothing).
int tv_material(const chess::Position &position, chess::Color color);

// A signal that does not get through, on a quiet disc: the offline mark.
// ink draws the signal, accent the stroke that cuts it.
void draw_tv_offline_mark(gfx::DrawList &list, const gfx::Rect &area, gfx::Color ink,
                          gfx::Color accent);

} // namespace pch::modes
