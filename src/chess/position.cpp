// ProsperoLichess - Chess position: FEN, legal move generation and game-state queries.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chess/bitboard.hpp"
#include "chess/chess.hpp"

namespace pch::chess
{

namespace
{

using bb::bit;
using bb::Bitboard;

constexpr int idx(Color c)
{
    return static_cast<int>(c);
}

constexpr int idx(Role r)
{
    return static_cast<int>(r);
}

constexpr int backrank(Color c)
{
    return c == Color::white ? 0 : 7;
}

constexpr Role kRoles[6] = {Role::pawn,   Role::queen,  Role::king,
                            Role::knight, Role::bishop, Role::rook};

constexpr Role kPromotions[4] = {Role::queen, Role::rook, Role::bishop, Role::knight};

bool role_from_char(char ch, Role *role)
{
    switch (ch)
    {
    case 'p':
        *role = Role::pawn;
        return true;
    case 'n':
        *role = Role::knight;
        return true;
    case 'b':
        *role = Role::bishop;
        return true;
    case 'r':
        *role = Role::rook;
        return true;
    case 'q':
        *role = Role::queen;
        return true;
    case 'k':
        *role = Role::king;
        return true;
    default:
        return false;
    }
}

bool parse_uint(std::string_view s, int *out)
{
    if (s.empty() || s.size() > 6)
    {
        return false;
    }
    int value = 0;
    for (const char ch : s)
    {
        if (ch < '0' || ch > '9')
        {
            return false;
        }
        value = value * 10 + (ch - '0');
    }
    *out = value;
    return true;
}

void append_uint(std::string &out, int value)
{
    char digits[12];
    int n = 0;
    unsigned v = value < 0 ? 0u : static_cast<unsigned>(value);
    do
    {
        digits[n++] = static_cast<char>('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
    {
        out.push_back(digits[--n]);
    }
}

} // namespace

std::string square_name(Square s)
{
    if (s < 0 || s > 63)
    {
        return "-";
    }
    std::string out(2, ' ');
    out[0] = static_cast<char>('a' + file_of(s));
    out[1] = static_cast<char>('1' + rank_of(s));
    return out;
}

Square parse_square(std::string_view s)
{
    if (s.size() != 2 || s[0] < 'a' || s[0] > 'h' || s[1] < '1' || s[1] > '8')
    {
        return kNoSquare;
    }
    return make_square(s[0] - 'a', s[1] - '1');
}

char role_char(Role r)
{
    static constexpr char kChars[6] = {'p', 'n', 'b', 'r', 'q', 'k'};
    return kChars[idx(r)];
}

Position Position::start()
{
    Position p;
    const Role back[8] = {Role::rook, Role::knight, Role::bishop, Role::queen,
                          Role::king, Role::bishop, Role::knight, Role::rook};
    for (int f = 0; f < 8; ++f)
    {
        p.put(make_square(f, 0), Piece{Color::white, back[f]});
        p.put(make_square(f, 1), Piece{Color::white, Role::pawn});
        p.put(make_square(f, 6), Piece{Color::black, Role::pawn});
        p.put(make_square(f, 7), Piece{Color::black, back[f]});
    }
    p.castling_ = bit(0) | bit(7) | bit(56) | bit(63);
    return p;
}

void Position::put(Square s, Piece p)
{
    colors_[idx(p.color)] |= bit(s);
    roles_[idx(p.role)] |= bit(s);
}

void Position::remove(Square s)
{
    const Bitboard mask = ~bit(s);
    for (auto &b : colors_)
    {
        b &= mask;
    }
    for (auto &b : roles_)
    {
        b &= mask;
    }
}

std::optional<Role> Position::role_at(Square s) const
{
    const Bitboard m = bit(s);
    if (!(occupied() & m))
    {
        return std::nullopt;
    }
    for (const Role r : kRoles)
    {
        if (roles_[idx(r)] & m)
        {
            return r;
        }
    }
    return std::nullopt;
}

std::optional<Piece> Position::piece_at(Square s) const
{
    if (s < 0 || s > 63)
    {
        return std::nullopt;
    }
    const std::optional<Role> role = role_at(s);
    if (!role)
    {
        return std::nullopt;
    }
    const Color c = (colors_[0] & bit(s)) ? Color::white : Color::black;
    return Piece{c, *role};
}

Square Position::king_square(Color c) const
{
    const Bitboard k = pieces(c, Role::king);
    return k ? static_cast<Square>(bb::lsb(k)) : kNoSquare;
}

int Position::count(Color c, Role r) const
{
    return bb::popcount(pieces(c, r));
}

// Pieces within `by` that attack square s given occupancy occ.
std::uint64_t Position::attackers(Square s, std::uint64_t occ, std::uint64_t by) const
{
    const Bitboard diag = roles_[idx(Role::bishop)] | roles_[idx(Role::queen)];
    const Bitboard ortho = roles_[idx(Role::rook)] | roles_[idx(Role::queen)];
    const Bitboard white_pawns = by & colors_[0] & roles_[idx(Role::pawn)];
    const Bitboard black_pawns = by & colors_[1] & roles_[idx(Role::pawn)];
    return by &
           ((bb::knight_attacks(s) & roles_[idx(Role::knight)]) |
            (bb::king_attacks(s) & roles_[idx(Role::king)]) | (bb::bishop_attacks(s, occ) & diag) |
            (bb::rook_attacks(s, occ) & ortho) | (bb::pawn_attacks(Color::black, s) & white_pawns) |
            (bb::pawn_attacks(Color::white, s) & black_pawns));
}

bool Position::in_check() const
{
    const Square k = king_square(turn_);
    return k != kNoSquare && attackers(k, occupied(), colors_[idx(opposite(turn_))]) != 0;
}

bool Position::king_safe_after(Square from, Square to, Square ep_victim) const
{
    Bitboard occ = (occupied() & ~bit(from)) | bit(to);
    Bitboard them = colors_[idx(opposite(turn_))] & ~bit(to);
    if (ep_victim != kNoSquare)
    {
        occ &= ~bit(ep_victim);
        them &= ~bit(ep_victim);
    }
    const Square king = (roles_[idx(Role::king)] & bit(from)) ? to : king_square(turn_);
    if (king == kNoSquare)
    {
        return true;
    }
    return attackers(king, occ, them) == 0;
}

Square Position::castling_rook(Color c, bool king_side) const
{
    const Square k = king_square(c);
    if (k == kNoSquare || rank_of(k) != backrank(c))
    {
        return kNoSquare;
    }
    Bitboard candidates = castling_ & pieces(c, Role::rook) & bb::rank_mask(backrank(c));
    candidates &= king_side ? ~((bit(k) << 1) - 1) : (bit(k) - 1);
    if (!candidates)
    {
        return kNoSquare;
    }
    return static_cast<Square>(king_side ? bb::msb(candidates) : bb::lsb(candidates));
}

bool Position::castle_squares(Square rook, Square *king_to, Square *rook_to) const
{
    const Color c = (colors_[0] & bit(rook)) ? Color::white : Color::black;
    const Square k = king_square(c);
    if (k == kNoSquare)
    {
        return false;
    }
    const bool king_side = file_of(rook) > file_of(k);
    const int rank = backrank(c);
    *king_to = make_square(king_side ? 6 : 2, rank);
    *rook_to = make_square(king_side ? 5 : 3, rank);
    return true;
}

void Position::generate_castles(MoveList &out) const
{
    const Color us = turn_;
    const Square k = king_square(us);
    const Bitboard them = colors_[idx(opposite(us))];
    const Bitboard occ = occupied();
    for (const bool king_side : {true, false})
    {
        const Square rook = castling_rook(us, king_side);
        Square kto = kNoSquare;
        Square rto = kNoSquare;
        if (rook == kNoSquare || !castle_squares(rook, &kto, &rto))
        {
            continue;
        }
        const Bitboard path =
            (bb::rank_span(k, kto) | bb::rank_span(rook, rto)) & ~bit(k) & ~bit(rook);
        if (path & occ)
        {
            continue;
        }
        bool safe = true;
        Bitboard king_path = bb::rank_span(k, kto) & ~bit(kto);
        const Bitboard occ_no_king = occ & ~bit(k);
        while (king_path && safe)
        {
            const Square s = static_cast<Square>(bb::pop_lsb(king_path));
            safe = attackers(s, occ_no_king, them) == 0;
        }
        const Bitboard final_occ = (occ & ~bit(k) & ~bit(rook)) | bit(kto) | bit(rto);
        if (safe && attackers(kto, final_occ, them) == 0)
        {
            out.push_back(Move{k, kto, std::nullopt});
        }
    }
}

void Position::generate(std::uint64_t from_mask, MoveList &out) const
{
    const Color us = turn_;
    const Bitboard own = colors_[idx(us)];
    const Bitboard opp = colors_[idx(opposite(us))];
    const Bitboard occ = own | opp;
    const int dir = us == Color::white ? 8 : -8;
    const int start_rank = us == Color::white ? 1 : 6;
    const int last_rank = us == Color::white ? 7 : 0;

    auto add = [&](Square from, Square to, Square victim, bool pawn)
    {
        if (!king_safe_after(from, to, victim))
        {
            return;
        }
        if (pawn && rank_of(to) == last_rank)
        {
            for (const Role promo : kPromotions)
            {
                out.push_back(Move{from, to, promo});
            }
            return;
        }
        out.push_back(Move{from, to, std::nullopt});
    };
    auto add_targets = [&](Square from, Bitboard targets, bool pawn)
    {
        while (targets)
        {
            add(from, static_cast<Square>(bb::pop_lsb(targets)), kNoSquare, pawn);
        }
    };

    Bitboard sources = own & from_mask;
    while (sources)
    {
        const Square from = static_cast<Square>(bb::pop_lsb(sources));
        const Bitboard m = bit(from);
        if (roles_[idx(Role::pawn)] & m)
        {
            const Square one = static_cast<Square>(from + dir);
            if (one >= 0 && one < 64 && !(occ & bit(one)))
            {
                add(from, one, kNoSquare, true);
                const Square two = static_cast<Square>(one + dir);
                if (rank_of(from) == start_rank && !(occ & bit(two)))
                {
                    add(from, two, kNoSquare, true);
                }
            }
            const Bitboard attacks = bb::pawn_attacks(us, from);
            add_targets(from, attacks & opp, true);
            if (ep_ != kNoSquare && (attacks & bit(ep_)))
            {
                add(from, ep_, static_cast<Square>(ep_ - dir), true);
            }
        }
        else if (roles_[idx(Role::knight)] & m)
        {
            add_targets(from, bb::knight_attacks(from) & ~own, false);
        }
        else if (roles_[idx(Role::bishop)] & m)
        {
            add_targets(from, bb::bishop_attacks(from, occ) & ~own, false);
        }
        else if (roles_[idx(Role::rook)] & m)
        {
            add_targets(from, bb::rook_attacks(from, occ) & ~own, false);
        }
        else if (roles_[idx(Role::queen)] & m)
        {
            add_targets(from, (bb::rook_attacks(from, occ) | bb::bishop_attacks(from, occ)) & ~own,
                        false);
        }
        else if (roles_[idx(Role::king)] & m)
        {
            add_targets(from, bb::king_attacks(from) & ~own, false);
            if (!in_check())
            {
                generate_castles(out);
            }
        }
    }
}

void Position::legal_moves(MoveList &out) const
{
    out.clear();
    generate(~Bitboard{0}, out);
}

void Position::legal_moves_from(Square from, MoveList &out) const
{
    out.clear();
    if (from >= 0 && from < 64)
    {
        generate(bit(from), out);
    }
}

bool Position::has_legal_moves() const
{
    MoveList moves;
    legal_moves(moves);
    return !moves.empty();
}

bool Position::is_legal(const Move &m) const
{
    if (!m.valid())
    {
        return false;
    }
    MoveList moves;
    legal_moves_from(m.from, moves);
    return moves.contains(m);
}

bool Position::is_checkmate() const
{
    return in_check() && !has_legal_moves();
}

bool Position::is_stalemate() const
{
    return !in_check() && !has_legal_moves();
}

bool Position::has_insufficient_material(Color c) const
{
    const Bitboard own = colors_[idx(c)];
    const Bitboard pawns = roles_[idx(Role::pawn)];
    const Bitboard knights = roles_[idx(Role::knight)];
    const Bitboard bishops = roles_[idx(Role::bishop)];
    if (own & (pawns | roles_[idx(Role::rook)] | roles_[idx(Role::queen)]))
    {
        return false;
    }
    if (own & knights)
    {
        const Bitboard others =
            colors_[idx(opposite(c))] & ~roles_[idx(Role::king)] & ~roles_[idx(Role::queen)];
        return bb::popcount(own) <= 2 && others == 0;
    }
    if (own & bishops)
    {
        const bool same_color = !(bishops & bb::kDarkSquares) || !(bishops & ~bb::kDarkSquares);
        return same_color && pawns == 0 && knights == 0;
    }
    return true;
}

bool Position::insufficient_material() const
{
    return has_insufficient_material(Color::white) && has_insufficient_material(Color::black);
}

bool Position::is_en_passant(const Move &m) const
{
    return m.valid() && ep_ != kNoSquare && m.to == ep_ &&
           (pieces(turn_, Role::pawn) & bit(m.from)) && file_of(m.from) != file_of(m.to);
}

bool Position::is_castle(const Move &m) const
{
    if (!m.valid() || m.promotion || !(pieces(turn_, Role::king) & bit(m.from)))
    {
        return false;
    }
    if (castling_ & pieces(turn_, Role::rook) & bit(m.to))
    {
        return true;
    }
    if (rank_of(m.from) != rank_of(m.to) || rank_of(m.from) != backrank(turn_))
    {
        return false;
    }
    const int df = file_of(m.to) - file_of(m.from);
    if (df != 2 && df != -2)
    {
        return false;
    }
    const Square rook = castling_rook(turn_, df > 0);
    Square kto = kNoSquare;
    Square rto = kNoSquare;
    return rook != kNoSquare && castle_squares(rook, &kto, &rto) && kto == m.to;
}

bool Position::castle_rook_squares(const Move &m, Square *rook_from, Square *rook_to) const
{
    if (!is_castle(m))
    {
        return false;
    }
    Square rook = m.to;
    if (!(castling_ & pieces(turn_, Role::rook) & bit(m.to)))
    {
        rook = castling_rook(turn_, file_of(m.to) > file_of(m.from));
    }
    Square kto = kNoSquare;
    Square rto = kNoSquare;
    if (!castle_squares(rook, &kto, &rto))
    {
        return false;
    }
    if (rook_from)
    {
        *rook_from = rook;
    }
    if (rook_to)
    {
        *rook_to = rto;
    }
    return true;
}

bool Position::is_capture(const Move &m) const
{
    if (!m.valid() || is_castle(m))
    {
        return false;
    }
    return (colors_[idx(opposite(turn_))] & bit(m.to)) || is_en_passant(m);
}

Position Position::after(const Move &m) const
{
    Position p = *this;
    const Color us = turn_;
    p.ep_ = kNoSquare;
    Square rook_from = kNoSquare;
    Square rook_to = kNoSquare;
    if (castle_rook_squares(m, &rook_from, &rook_to))
    {
        Square kto = kNoSquare;
        Square rto = kNoSquare;
        castle_squares(rook_from, &kto, &rto);
        p.remove(m.from);
        p.remove(rook_from);
        p.put(kto, Piece{us, Role::king});
        p.put(rook_to, Piece{us, Role::rook});
        p.castling_ &= ~bb::rank_mask(backrank(us));
        ++p.halfmoves_;
    }
    else
    {
        const std::optional<Role> moving = role_at(m.from);
        const Role role = moving ? *moving : Role::pawn;
        const bool capture = (colors_[idx(opposite(us))] & bit(m.to)) != 0;
        if (is_en_passant(m))
        {
            p.remove(static_cast<Square>(m.to + (us == Color::white ? -8 : 8)));
        }
        p.halfmoves_ = (capture || role == Role::pawn) ? 0 : halfmoves_ + 1;
        p.remove(m.from);
        p.remove(m.to);
        p.put(m.to, Piece{us, m.promotion ? *m.promotion : role});
        if (role == Role::pawn && (m.to - m.from == 16 || m.from - m.to == 16))
        {
            p.ep_ = static_cast<Square>((m.from + m.to) / 2);
        }
        if (role == Role::king)
        {
            p.castling_ &= ~bb::rank_mask(backrank(us));
        }
        p.castling_ &= ~bit(m.from) & ~bit(m.to);
    }
    p.turn_ = opposite(us);
    if (us == Color::black)
    {
        ++p.fullmoves_;
    }
    return p;
}

Square Position::ep_square() const
{
    if (ep_ == kNoSquare)
    {
        return kNoSquare;
    }
    const int dir = turn_ == Color::white ? 8 : -8;
    Bitboard pawns = bb::pawn_attacks(opposite(turn_), ep_) & pieces(turn_, Role::pawn);
    while (pawns)
    {
        const Square from = static_cast<Square>(bb::pop_lsb(pawns));
        if (king_safe_after(from, ep_, static_cast<Square>(ep_ - dir)))
        {
            return ep_;
        }
    }
    return kNoSquare;
}

std::uint64_t Position::hash() const
{
    std::uint64_t h = 0;
    for (int c = 0; c < 2; ++c)
    {
        for (int r = 0; r < 6; ++r)
        {
            Bitboard b = colors_[c] & roles_[r];
            while (b)
            {
                h ^= bb::kZobrist.piece[c * 6 + r][bb::pop_lsb(b)];
            }
        }
    }
    Bitboard rooks = castling_;
    while (rooks)
    {
        h ^= bb::kZobrist.castling[bb::pop_lsb(rooks)];
    }
    const Square ep = ep_square();
    if (ep != kNoSquare)
    {
        h ^= bb::kZobrist.ep[file_of(ep)];
    }
    if (turn_ == Color::black)
    {
        h ^= bb::kZobrist.black_to_move;
    }
    return h;
}

bool Position::from_fen(std::string_view fen, Position *out, std::string *error)
{
    auto fail = [error](const char *message)
    {
        if (error)
        {
            *error = message;
        }
        return false;
    };

    std::string_view fields[6];
    int field_count = 0;
    std::size_t i = 0;
    while (i < fen.size())
    {
        while (i < fen.size() && (fen[i] == ' ' || fen[i] == '\t'))
        {
            ++i;
        }
        if (i >= fen.size())
        {
            break;
        }
        std::size_t j = i;
        while (j < fen.size() && fen[j] != ' ' && fen[j] != '\t')
        {
            ++j;
        }
        if (field_count == 6)
        {
            return fail("too many fields");
        }
        fields[field_count++] = fen.substr(i, j - i);
        i = j;
    }
    if (field_count == 0)
    {
        return fail("empty fen");
    }

    Position p;
    p.fullmoves_ = 1;

    // Board.
    int rank = 7;
    int file = 0;
    for (const char ch : fields[0])
    {
        if (ch == '/')
        {
            if (file != 8 || rank == 0)
            {
                return fail("invalid board");
            }
            --rank;
            file = 0;
        }
        else if (ch >= '1' && ch <= '8')
        {
            file += ch - '0';
            if (file > 8)
            {
                return fail("invalid board");
            }
        }
        else
        {
            const bool white = ch >= 'A' && ch <= 'Z';
            Role role = Role::pawn;
            if (!role_from_char(white ? static_cast<char>(ch - 'A' + 'a') : ch, &role) || file > 7)
            {
                return fail("invalid board");
            }
            p.put(make_square(file, rank), Piece{white ? Color::white : Color::black, role});
            ++file;
        }
    }
    if (rank != 0 || file != 8)
    {
        return fail("invalid board");
    }

    // Turn.
    if (field_count > 1)
    {
        if (fields[1] == "w")
        {
            p.turn_ = Color::white;
        }
        else if (fields[1] == "b")
        {
            p.turn_ = Color::black;
        }
        else
        {
            return fail("invalid turn");
        }
    }

    // Validate kings and pawns before interpreting castling.
    for (const Color c : {Color::white, Color::black})
    {
        if (bb::popcount(p.pieces(c, Role::king)) != 1)
        {
            return fail("each side needs exactly one king");
        }
    }
    if (p.roles_[idx(Role::pawn)] & (bb::rank_mask(0) | bb::rank_mask(7)))
    {
        return fail("pawns on backrank");
    }

    // Castling.
    if (field_count > 2 && fields[2] != "-")
    {
        for (const char ch : fields[2])
        {
            const bool white = ch >= 'A' && ch <= 'Z';
            const Color c = white ? Color::white : Color::black;
            const char lower = white ? static_cast<char>(ch - 'A' + 'a') : ch;
            const Square k = p.king_square(c);
            const int back = backrank(c);
            const Bitboard rooks = p.pieces(c, Role::rook) & bb::rank_mask(back);
            Square rook = kNoSquare;
            if (lower == 'k' || lower == 'q')
            {
                if (rank_of(k) != back)
                {
                    continue;
                }
                const Bitboard side = rooks & (lower == 'k' ? ~((bit(k) << 1) - 1) : (bit(k) - 1));
                if (side)
                {
                    rook = static_cast<Square>(lower == 'k' ? bb::msb(side) : bb::lsb(side));
                }
            }
            else if (lower >= 'a' && lower <= 'h')
            {
                const Square s = make_square(lower - 'a', back);
                if (rooks & bit(s))
                {
                    rook = s;
                }
            }
            else
            {
                return fail("invalid castling");
            }
            if (rook != kNoSquare && rank_of(k) == back)
            {
                p.castling_ |= bit(rook);
            }
        }
    }

    // En passant.
    if (field_count > 3 && fields[3] != "-")
    {
        const Square ep = parse_square(fields[3]);
        if (ep == kNoSquare)
        {
            return fail("invalid en passant square");
        }
        const bool white = p.turn_ == Color::white;
        const int dir = white ? 8 : -8;
        const Square victim = static_cast<Square>(ep - dir);
        const Square origin = static_cast<Square>(ep + dir);
        if (rank_of(ep) == (white ? 5 : 2) && !(p.occupied() & bit(ep)) &&
            !(p.occupied() & bit(origin)) &&
            (p.pieces(opposite(p.turn_), Role::pawn) & bit(victim)))
        {
            p.ep_ = ep;
        }
    }

    // Clocks.
    if (field_count > 4 && !parse_uint(fields[4], &p.halfmoves_))
    {
        return fail("invalid halfmove clock");
    }
    if (field_count > 5)
    {
        if (!parse_uint(fields[5], &p.fullmoves_))
        {
            return fail("invalid fullmove number");
        }
        if (p.fullmoves_ < 1)
        {
            p.fullmoves_ = 1;
        }
    }

    // The side not to move must not be in check.
    const Color them = opposite(p.turn_);
    if (p.attackers(p.king_square(them), p.occupied(), p.colors_[idx(p.turn_)]))
    {
        return fail("opposite side in check");
    }

    if (out)
    {
        *out = p;
    }
    return true;
}

std::string Position::fen() const
{
    std::string out;
    out.reserve(90);
    for (int rank = 7; rank >= 0; --rank)
    {
        int empty = 0;
        for (int file = 0; file < 8; ++file)
        {
            const std::optional<Piece> piece = piece_at(make_square(file, rank));
            if (!piece)
            {
                ++empty;
                continue;
            }
            if (empty)
            {
                out.push_back(static_cast<char>('0' + empty));
                empty = 0;
            }
            const char ch = role_char(piece->role);
            out.push_back(piece->color == Color::white ? static_cast<char>(ch - 'a' + 'A') : ch);
        }
        if (empty)
        {
            out.push_back(static_cast<char>('0' + empty));
        }
        if (rank)
        {
            out.push_back('/');
        }
    }
    out += turn_ == Color::white ? " w " : " b ";

    const std::size_t castling_start = out.size();
    for (const Color c : {Color::white, Color::black})
    {
        const Square k = king_square(c);
        const Bitboard rooks = castling_ & colors_[idx(c)];
        const char upper = c == Color::white ? 'A' : 'a';
        for (const bool king_side : {true, false})
        {
            Bitboard side = rooks & (king_side ? ~((bit(k) << 1) - 1) : (bit(k) - 1));
            const Square outer = castling_rook(c, king_side);
            while (side)
            {
                const Square r = static_cast<Square>(king_side ? bb::msb(side) : bb::lsb(side));
                side &= ~bit(r);
                if (r == outer)
                {
                    out.push_back(static_cast<char>(upper + (king_side ? 'K' : 'Q') - 'A'));
                }
                else
                {
                    out.push_back(static_cast<char>(upper + file_of(r)));
                }
            }
        }
    }
    if (out.size() == castling_start)
    {
        out.push_back('-');
    }

    out.push_back(' ');
    const Square ep = ep_square();
    out += ep == kNoSquare ? std::string("-") : square_name(ep);
    out.push_back(' ');
    append_uint(out, halfmoves_);
    out.push_back(' ');
    append_uint(out, fullmoves_);
    return out;
}

} // namespace pch::chess
