// ProsperoLichess - Game history with SAN, outcomes and Lichess move-list syncing.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chess/chess.hpp"

#include <utility>

namespace pch::chess
{

Game::Game()
{
    reset();
}

void Game::reset(const Position &initial)
{
    positions_.clear();
    moves_.clear();
    sans_.clear();
    hashes_.clear();
    positions_.push_back(initial);
    hashes_.push_back(initial.hash());
}

bool Game::play(const Move &m)
{
    const Position &current = position();
    if (!current.is_legal(m))
    {
        return false;
    }
    std::string san = to_san(current, m);
    Position next = current.after(m);
    moves_.push_back(m);
    sans_.push_back(std::move(san));
    hashes_.push_back(next.hash());
    positions_.push_back(next);
    return true;
}

bool Game::undo()
{
    if (moves_.empty())
    {
        return false;
    }
    moves_.pop_back();
    sans_.pop_back();
    hashes_.pop_back();
    positions_.pop_back();
    return true;
}

Outcome Game::outcome() const
{
    const Position &pos = position();
    const bool can_move = pos.has_legal_moves();
    if (!can_move)
    {
        return pos.in_check() ? Outcome::checkmate : Outcome::stalemate;
    }
    if (pos.insufficient_material())
    {
        return Outcome::insufficient_material;
    }
    if (pos.halfmove_clock() >= 100)
    {
        return Outcome::fifty_moves;
    }
    // Repetitions can only occur since the last irreversible move.
    const std::uint64_t current = hashes_.back();
    const std::size_t last = hashes_.size() - 1;
    const std::size_t window = static_cast<std::size_t>(pos.halfmove_clock());
    int repetitions = 1;
    for (std::size_t back = 2; back <= window && back <= last; back += 2)
    {
        if (hashes_[last - back] == current && ++repetitions >= 3)
        {
            return Outcome::threefold;
        }
    }
    return Outcome::ongoing;
}

bool Game::apply_uci_moves(std::string_view moves)
{
    // Tokenise and check whether the list extends our current history.
    std::size_t token_count = 0;
    bool extends = true;
    std::size_t i = 0;
    std::size_t append_from = moves.size();
    while (i < moves.size())
    {
        while (i < moves.size() && moves[i] == ' ')
        {
            ++i;
        }
        if (i >= moves.size())
        {
            break;
        }
        std::size_t j = i;
        while (j < moves.size() && moves[j] != ' ')
        {
            ++j;
        }
        const std::string_view token = moves.substr(i, j - i);
        if (token_count < ply_count())
        {
            Move m;
            if (!parse_uci(positions_[token_count], token, &m) || !(m == moves_[token_count]))
            {
                extends = false;
                break;
            }
        }
        else if (token_count == ply_count())
        {
            append_from = i;
        }
        ++token_count;
        i = j;
    }
    if (extends && token_count < ply_count())
    {
        extends = false;
    }

    Game next;
    std::string_view rest = moves;
    if (extends)
    {
        if (append_from >= moves.size())
        {
            return true; // nothing new
        }
        next = *this;
        rest = moves.substr(append_from);
    }
    else
    {
        next.reset(initial());
    }

    i = 0;
    while (i < rest.size())
    {
        while (i < rest.size() && rest[i] == ' ')
        {
            ++i;
        }
        if (i >= rest.size())
        {
            break;
        }
        std::size_t j = i;
        while (j < rest.size() && rest[j] != ' ')
        {
            ++j;
        }
        Move m;
        if (!parse_uci(next.position(), rest.substr(i, j - i), &m) || !next.play(m))
        {
            return false;
        }
        i = j;
    }
    *this = std::move(next);
    return true;
}

} // namespace pch::chess
