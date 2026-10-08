// ProsperoLichess - Move notation: UCI, SAN and PGN movetext.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chess/chess.hpp"

namespace pch::chess
{

namespace
{

bool promotion_from_char(char ch, Role *out)
{
    switch (ch)
    {
    case 'q':
    case 'Q':
        *out = Role::queen;
        return true;
    case 'r':
    case 'R':
        *out = Role::rook;
        return true;
    case 'b':
    case 'B':
        *out = Role::bishop;
        return true;
    case 'n':
    case 'N':
        *out = Role::knight;
        return true;
    default:
        return false;
    }
}

bool piece_from_san_char(char ch, Role *out)
{
    switch (ch)
    {
    case 'N':
        *out = Role::knight;
        return true;
    case 'B':
        *out = Role::bishop;
        return true;
    case 'R':
        *out = Role::rook;
        return true;
    case 'Q':
        *out = Role::queen;
        return true;
    case 'K':
        *out = Role::king;
        return true;
    default:
        return false;
    }
}

char upper(char ch)
{
    return (ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - 'a' + 'A') : ch;
}

bool is_space(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

} // namespace

std::string to_uci(const Move &m)
{
    if (!m.valid())
    {
        return "0000";
    }
    std::string out = square_name(m.from) + square_name(m.to);
    if (m.promotion)
    {
        out.push_back(role_char(*m.promotion));
    }
    return out;
}

bool parse_uci(const Position &pos, std::string_view uci, Move *out)
{
    if (uci.size() != 4 && uci.size() != 5)
    {
        return false;
    }
    Move m;
    m.from = parse_square(uci.substr(0, 2));
    m.to = parse_square(uci.substr(2, 2));
    if (m.from == kNoSquare || m.to == kNoSquare)
    {
        return false;
    }
    if (uci.size() == 5)
    {
        Role promo = Role::queen;
        if (!promotion_from_char(uci[4], &promo))
        {
            return false;
        }
        m.promotion = promo;
    }
    // King-takes-own-rook castling notation: normalise to the king's destination.
    const std::optional<Piece> mover = pos.piece_at(m.from);
    const std::optional<Piece> target = pos.piece_at(m.to);
    if (!m.promotion && mover && target && mover->role == Role::king &&
        target->role == Role::rook && mover->color == pos.turn() && target->color == pos.turn())
    {
        Square rook_from = kNoSquare;
        Square rook_to = kNoSquare;
        if (!pos.castle_rook_squares(m, &rook_from, &rook_to))
        {
            return false;
        }
        m.to = make_square(file_of(m.to) > file_of(m.from) ? 6 : 2, rank_of(m.from));
    }
    if (!pos.is_legal(m))
    {
        return false;
    }
    if (out)
    {
        *out = m;
    }
    return true;
}

std::string to_san(const Position &pos, const Move &m)
{
    std::string san;
    const std::optional<Piece> mover = pos.piece_at(m.from);
    if (!mover || !m.valid())
    {
        return "--";
    }
    if (pos.is_castle(m))
    {
        san = file_of(m.to) > file_of(m.from) ? "O-O" : "O-O-O";
    }
    else
    {
        const bool capture = pos.is_capture(m);
        if (mover->role == Role::pawn)
        {
            if (capture)
            {
                san.push_back(static_cast<char>('a' + file_of(m.from)));
                san.push_back('x');
            }
            san += square_name(m.to);
            if (m.promotion)
            {
                san.push_back('=');
                san.push_back(upper(role_char(*m.promotion)));
            }
        }
        else
        {
            san.push_back(upper(role_char(mover->role)));
            if (mover->role != Role::king)
            {
                MoveList moves;
                pos.legal_moves(moves);
                bool others = false;
                bool same_file = false;
                bool same_rank = false;
                for (const Move &other : moves)
                {
                    if (other.to != m.to || other.from == m.from)
                    {
                        continue;
                    }
                    const std::optional<Piece> p = pos.piece_at(other.from);
                    if (!p || p->role != mover->role)
                    {
                        continue;
                    }
                    others = true;
                    same_file = same_file || file_of(other.from) == file_of(m.from);
                    same_rank = same_rank || rank_of(other.from) == rank_of(m.from);
                }
                if (others)
                {
                    // chessops: file when it disambiguates, rank when the file
                    // is shared, both when file and rank are each shared.
                    const bool use_rank = same_file;
                    const bool use_file = !same_file || same_rank;
                    if (use_file)
                    {
                        san.push_back(static_cast<char>('a' + file_of(m.from)));
                    }
                    if (use_rank)
                    {
                        san.push_back(static_cast<char>('1' + rank_of(m.from)));
                    }
                }
            }
            if (capture)
            {
                san.push_back('x');
            }
            san += square_name(m.to);
        }
    }
    const Position next = pos.after(m);
    if (next.in_check())
    {
        san.push_back(next.has_legal_moves() ? '+' : '#');
    }
    return san;
}

bool parse_san(const Position &pos, std::string_view san, Move *out)
{
    // Trim whitespace and annotation suffixes.
    while (!san.empty() && is_space(san.front()))
    {
        san.remove_prefix(1);
    }
    while (!san.empty())
    {
        const char ch = san.back();
        if (ch == '+' || ch == '#' || ch == '!' || ch == '?' || is_space(ch))
        {
            san.remove_suffix(1);
        }
        else
        {
            break;
        }
    }
    if (san.empty())
    {
        return false;
    }

    MoveList moves;
    pos.legal_moves(moves);

    if (san == "O-O" || san == "0-0" || san == "O-O-O" || san == "0-0-0")
    {
        const bool king_side = san.size() == 3;
        for (const Move &m : moves)
        {
            if (pos.is_castle(m) && (file_of(m.to) > file_of(m.from)) == king_side)
            {
                if (out)
                {
                    *out = m;
                }
                return true;
            }
        }
        return false;
    }

    Role role = Role::pawn;
    if (piece_from_san_char(san.front(), &role))
    {
        san.remove_prefix(1);
    }

    std::optional<Role> promotion;
    if (role == Role::pawn && san.size() >= 3)
    {
        Role promo = Role::queen;
        const char last = san.back();
        const char before = san[san.size() - 2];
        if (promotion_from_char(last, &promo) &&
            ((before >= '1' && before <= '8') || before == '='))
        {
            promotion = promo;
            san.remove_suffix(1);
            if (!san.empty() && san.back() == '=')
            {
                san.remove_suffix(1);
            }
        }
    }

    if (san.size() < 2)
    {
        return false;
    }
    const Square to = parse_square(san.substr(san.size() - 2));
    if (to == kNoSquare)
    {
        return false;
    }
    san.remove_suffix(2);
    if (!san.empty() && (san.back() == 'x' || san.back() == ':'))
    {
        san.remove_suffix(1);
    }
    int from_file = -1;
    int from_rank = -1;
    for (const char ch : san)
    {
        if (ch >= 'a' && ch <= 'h' && from_file < 0 && from_rank < 0)
        {
            from_file = ch - 'a';
        }
        else if (ch >= '1' && ch <= '8' && from_rank < 0)
        {
            from_rank = ch - '1';
        }
        else if (ch != '-')
        {
            return false;
        }
    }

    const Move *found = nullptr;
    for (const Move &m : moves)
    {
        if (m.to != to || m.promotion != promotion || pos.is_castle(m))
        {
            continue;
        }
        const std::optional<Piece> p = pos.piece_at(m.from);
        if (!p || p->role != role)
        {
            continue;
        }
        if ((from_file >= 0 && file_of(m.from) != from_file) ||
            (from_rank >= 0 && rank_of(m.from) != from_rank))
        {
            continue;
        }
        if (found)
        {
            return false; // ambiguous
        }
        found = &m;
    }
    if (!found)
    {
        return false;
    }
    if (out)
    {
        *out = *found;
    }
    return true;
}

bool parse_pgn_moves(std::string_view pgn, const Position &start, std::vector<Move> *out)
{
    Position pos = start;
    std::size_t i = 0;
    int variation_depth = 0;
    while (i < pgn.size())
    {
        const char ch = pgn[i];
        if (is_space(ch))
        {
            ++i;
            continue;
        }
        if (ch == '{')
        {
            const std::size_t end = pgn.find('}', i);
            i = end == std::string_view::npos ? pgn.size() : end + 1;
            continue;
        }
        if (ch == ';' || (ch == '%' && (i == 0 || pgn[i - 1] == '\n')))
        {
            const std::size_t end = pgn.find('\n', i);
            i = end == std::string_view::npos ? pgn.size() : end + 1;
            continue;
        }
        if (ch == '[' && variation_depth == 0)
        {
            // Header tag: skip to the closing bracket, honouring quoted strings.
            bool quoted = false;
            ++i;
            while (i < pgn.size() && (quoted || pgn[i] != ']'))
            {
                if (pgn[i] == '\\' && quoted && i + 1 < pgn.size())
                {
                    ++i;
                }
                else if (pgn[i] == '"')
                {
                    quoted = !quoted;
                }
                ++i;
            }
            ++i;
            continue;
        }
        if (ch == '(')
        {
            ++variation_depth;
            ++i;
            continue;
        }
        if (ch == ')')
        {
            if (variation_depth > 0)
            {
                --variation_depth;
            }
            ++i;
            continue;
        }
        // Read a token up to whitespace or a structural character.
        std::size_t j = i;
        while (j < pgn.size() && !is_space(pgn[j]) && pgn[j] != '{' && pgn[j] != '(' &&
               pgn[j] != ')' && pgn[j] != ';')
        {
            ++j;
        }
        std::string_view token = pgn.substr(i, j - i);
        i = j;
        if (variation_depth > 0)
        {
            continue;
        }
        if (token.front() == '$')
        {
            continue; // NAG
        }
        if (token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*")
        {
            continue;
        }
        // Move number prefix ("1.", "12...", "1.e4").
        std::size_t k = 0;
        while (k < token.size() && token[k] >= '0' && token[k] <= '9')
        {
            ++k;
        }
        if (k > 0 && k < token.size() && token[k] == '.')
        {
            while (k < token.size() && token[k] == '.')
            {
                ++k;
            }
            token.remove_prefix(k);
        }
        else if (k == token.size())
        {
            continue; // bare number
        }
        while (!token.empty() && token.front() == '.')
        {
            token.remove_prefix(1);
        }
        if (token.empty())
        {
            continue;
        }
        Move m;
        if (!parse_san(pos, token, &m))
        {
            return false;
        }
        if (out)
        {
            out->push_back(m);
        }
        pos = pos.after(m);
    }
    return true;
}

} // namespace pch::chess
