// ProsperoLichess - A Lichess Board API game: stream, clocks, moves and offers.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lichess/session.hpp"
#include "modes/game_link.hpp"

#include <memory>
#include <string>

namespace pch::lichess
{

// Parses one NDJSON line of /api/board/game/stream/{id} into state (exposed for tests).
struct BoardState
{
    bool full = false;
    std::string initial_fen;
    std::string moves;
    long long wtime = -1;
    long long btime = -1;
    long long winc = 0;
    long long binc = 0;
    modes::GameStatus status = modes::GameStatus::waiting;
    int winner = -1;
    bool wdraw = false;
    bool bdraw = false;
    bool wtakeback = false;
    bool btakeback = false;
    modes::PlayerInfo white;
    modes::PlayerInfo black;
    std::string white_id;
    std::string black_id;
    bool rated = false;
    std::string speed;
    long long clock_initial = -1; // ms
    long long clock_increment = 0;
    int days_per_turn = 0;
    int opponent_gone = -1;
};
modes::GameStatus parse_status(std::string_view status);
// Applies one stream line; returns false if it is not a recognised event.
bool apply_board_line(std::string_view line, BoardState *state);

std::unique_ptr<modes::GameLink> make_board_link(Session &session, std::string game_id);

} // namespace pch::lichess
