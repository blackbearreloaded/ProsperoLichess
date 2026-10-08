// ProsperoLichess - The authority behind a game screen (local or a Lichess game).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/scene.hpp"
#include "chess/chess.hpp"

#include <cstdint>
#include <string>

namespace pch::modes
{

struct PlayerInfo
{
    std::string name;
    std::string title; // "GM", "BOT", ...
    int rating = 0;
    bool provisional = false;
    int ai_level = 0; // Lichess AI (Stockfish) level, 0 for people
};

enum class GameStatus : std::uint8_t
{
    waiting, // not started (loading, created)
    playing,
    aborted,
    mate,
    resign,
    stalemate,
    timeout, // opponent left
    draw,
    outoftime,
    cheat,
    no_start,
    insufficient,
    variant_end,
    unknown_end,
};

inline bool is_over(GameStatus status)
{
    return status != GameStatus::waiting && status != GameStatus::playing;
}

// A remote game: Lichess Board API. The scene applies moves() whenever
// version() changes and routes the player's actions here.
class GameLink
{
  public:
    virtual ~GameLink() = default;
    virtual void pump(app::Context &ctx, float dt) = 0;
    virtual bool ready() const = 0;        // first full state received
    virtual std::string error() const = 0; // fatal problem ("" when fine)
    virtual bool connected() const = 0;    // the live stream is up
    virtual std::uint64_t version() const = 0;
    virtual const std::string &initial_fen() const = 0; // "" = standard start
    virtual const std::string &moves() const = 0;       // space-separated UCI
    virtual chess::Color my_color() const = 0;
    virtual PlayerInfo player(chess::Color color) const = 0;
    virtual bool has_clock() const = 0;
    // Remaining time now, extrapolated for the side to move.
    virtual long long clock_ms(chess::Color color) const = 0;
    virtual GameStatus status() const = 0;
    virtual int winner() const = 0; // -1 none, 0 white, 1 black
    virtual bool rated() const = 0;
    virtual std::string speed_label() const = 0; // "Rapid 10+5"
    virtual bool draw_offered_by(chess::Color color) const = 0;
    virtual bool takeback_offered_by(chess::Color color) const = 0;
    virtual int opponent_gone_seconds() const = 0; // -1 when present

    virtual void send_move(const chess::Move &move) = 0;
    virtual void resign() = 0;
    virtual void abort() = 0;
    virtual void draw(bool yes) = 0;     // offer / accept (yes) or decline (no)
    virtual void takeback(bool yes) = 0; // propose / accept (yes) or decline (no)
    virtual void claim_victory() = 0;
    // A move of ours the server refused since the last call (to revert).
    virtual bool take_refusal(std::string *message) = 0;
    virtual std::string game_id() const = 0;
};

} // namespace pch::modes
