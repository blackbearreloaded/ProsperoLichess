// ProsperoLichess - One puzzle in play order, from the offline pack or the Lichess API.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "chess/chess.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace pch::puzzles
{

struct Puzzle
{
    std::string id;
    chess::Position before;            // position before the opponent's setup move
    chess::Move setup;                 // the opponent's move that starts the puzzle
    std::vector<chess::Move> solution; // player, reply, player, ...
    int rating = 0;
    std::vector<std::string> themes; // camelCase Lichess theme names
    std::string game_url;            // optional
    std::string source;              // "pack", "daily", "training"

    chess::Color solver() const
    {
        return chess::opposite(before.turn());
    }
    chess::Position start() const
    {
        return before.after(setup);
    }
};

// CSV / pack form: fen before the opponent's move, moves[0] is that move.
bool from_csv(std::string_view id, std::string_view fen, const std::vector<std::string> &moves,
              int rating, Puzzle *out, std::string *error);

// Lichess API form: game.pgn (ends with the opponent's move) and the solution.
bool from_api(std::string_view id, std::string_view pgn, const std::vector<std::string> &solution,
              int rating, Puzzle *out, std::string *error);

// Whether move is an acceptable answer at step (a solution index of the
// player's): the expected move, or any checkmate when the line mates next.
bool accepts(const Puzzle &puzzle, const chess::Position &position, std::size_t step,
             const chess::Move &move);

} // namespace pch::puzzles
