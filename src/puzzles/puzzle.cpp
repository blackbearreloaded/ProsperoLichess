// ProsperoLichess - One puzzle in play order, from the offline pack or the Lichess API.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "puzzles/puzzle.hpp"

#include "core/strings.hpp"

namespace pch::puzzles
{

namespace
{

// Checks that every solution move is legal in order. What is wrong with a
// puzzle Lichess sent is told to the player, so it is in their language.
bool validate(Puzzle *out, const std::vector<std::string> &solution, std::string *error)
{
    chess::Position position = out->start();
    out->solution.clear();
    for (const std::string &uci : solution)
    {
        chess::Move move;
        if (!chess::parse_uci(position, uci, &move))
        {
            *error = fill(tr("illegal solution move {0}"), {uci});
            return false;
        }
        out->solution.push_back(move);
        position = position.after(move);
    }
    if (out->solution.empty())
    {
        *error = tr("empty solution");
        return false;
    }
    return true;
}

} // namespace

bool from_csv(std::string_view id, std::string_view fen, const std::vector<std::string> &moves,
              int rating, Puzzle *out, std::string *error)
{
    Puzzle puzzle;
    puzzle.id = std::string(id);
    puzzle.rating = rating;
    puzzle.source = "pack";
    // A puzzle of the pack that cannot be used is passed over: these two
    // reasons are never shown.
    if (moves.size() < 2 || !chess::Position::from_fen(fen, &puzzle.before, error))
    {
        if (error->empty())
            *error = "too few moves";
        return false;
    }
    if (!chess::parse_uci(puzzle.before, moves[0], &puzzle.setup))
    {
        *error = "illegal setup move";
        return false;
    }
    const std::vector<std::string> solution(moves.begin() + 1, moves.end());
    if (!validate(&puzzle, solution, error))
        return false;
    *out = std::move(puzzle);
    return true;
}

bool from_api(std::string_view id, std::string_view pgn, const std::vector<std::string> &solution,
              int rating, Puzzle *out, std::string *error)
{
    Puzzle puzzle;
    puzzle.id = std::string(id);
    puzzle.rating = rating;
    std::vector<chess::Move> moves;
    if (!chess::parse_pgn_moves(pgn, chess::Position::start(), &moves) || moves.empty())
    {
        *error = tr("unreadable puzzle game");
        return false;
    }
    chess::Position position = chess::Position::start();
    for (std::size_t i = 0; i + 1 < moves.size(); ++i)
        position = position.after(moves[i]);
    puzzle.before = position;
    puzzle.setup = moves.back();
    if (!validate(&puzzle, solution, error))
        return false;
    *out = std::move(puzzle);
    return true;
}

bool accepts(const Puzzle &puzzle, const chess::Position &position, std::size_t step,
             const chess::Move &move)
{
    if (step >= puzzle.solution.size())
        return false;
    const chess::Move &expected = puzzle.solution[step];
    if (move.from == expected.from && move.to == expected.to &&
        move.promotion.value_or(chess::Role::queen) ==
            expected.promotion.value_or(chess::Role::queen))
        return true;
    // Lichess accepts any mate where the solution mates.
    if (!position.is_legal(move))
        return false;
    const chess::Position expected_after = position.after(expected);
    return expected_after.is_checkmate() && position.after(move).is_checkmate();
}

} // namespace pch::puzzles
