// ProsperoLichess - Chess rules core: positions, legal moves, notation and games.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Standard chess rules matching Lichess semantics (see niklasf/chessops). The
// core is exception-free and the move generator is allocation-free.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pch::chess
{

enum class Color : std::uint8_t
{
    white,
    black
};

constexpr Color opposite(Color c)
{
    return c == Color::white ? Color::black : Color::white;
}

enum class Role : std::uint8_t
{
    pawn,
    knight,
    bishop,
    rook,
    queen,
    king
};

struct Piece
{
    Color color;
    Role role;
    bool operator==(const Piece &) const = default;
};

using Square = std::int8_t; // 0 = a1, 1 = b1, ... 63 = h8
constexpr Square kNoSquare = -1;

constexpr int file_of(Square s)
{
    return s & 7;
}

constexpr int rank_of(Square s)
{
    return s >> 3;
}

// Returns kNoSquare when file or rank is outside 0..7.
constexpr Square make_square(int file, int rank)
{
    if (file < 0 || file > 7 || rank < 0 || rank > 7)
    {
        return kNoSquare;
    }
    return static_cast<Square>(rank * 8 + file);
}

std::string square_name(Square s);       // "e4"; "-" for kNoSquare
Square parse_square(std::string_view s); // kNoSquare on error
char role_char(Role r);                  // 'p','n','b','r','q','k'

struct Move
{
    Square from = kNoSquare;
    Square to = kNoSquare;
    std::optional<Role> promotion; // set only for promotions

    bool valid() const
    {
        return from >= 0 && from < 64 && to >= 0 && to < 64;
    }
    bool operator==(const Move &) const = default;
};

// Fixed-capacity move buffer (no heap). 218 is the known maximum of legal moves.
class MoveList
{
  public:
    static constexpr std::size_t kCapacity = 256;

    std::size_t size() const
    {
        return size_;
    }
    bool empty() const
    {
        return size_ == 0;
    }
    const Move &operator[](std::size_t i) const
    {
        return moves_[i];
    }
    const Move *begin() const
    {
        return moves_.data();
    }
    const Move *end() const
    {
        return moves_.data() + size_;
    }
    void push_back(const Move &m)
    {
        if (size_ < kCapacity)
        {
            moves_[size_++] = m;
        }
    }
    void clear()
    {
        size_ = 0;
    }
    bool contains(const Move &m) const
    {
        for (const Move &x : *this)
        {
            if (x == m)
            {
                return true;
            }
        }
        return false;
    }

  private:
    std::array<Move, kCapacity> moves_;
    std::size_t size_ = 0;
};

class Position
{
  public:
    static Position start();
    static bool from_fen(std::string_view fen, Position *out, std::string *error = nullptr);
    std::string fen() const;

    std::optional<Piece> piece_at(Square s) const;
    Color turn() const
    {
        return turn_;
    }

    // Castling moves are generated as the king moving two squares (e1g1).
    void legal_moves(MoveList &out) const;
    void legal_moves_from(Square from, MoveList &out) const;
    bool has_legal_moves() const;
    bool is_legal(const Move &m) const;
    Position after(const Move &m) const; // precondition: legal

    bool in_check() const;
    Square king_square(Color c) const;
    bool is_checkmate() const;
    bool is_stalemate() const;
    // Lichess rules: neither side can possibly mate (K vs K, K+minor vs K,
    // bishops all on one colour, ...).
    bool insufficient_material() const;
    bool has_insufficient_material(Color c) const;

    int halfmove_clock() const
    {
        return halfmoves_;
    }
    int fullmove_number() const
    {
        return fullmoves_;
    }
    // Zobrist hash over pieces, turn, castling rights and the en passant file
    // (only when an en passant capture is actually legal).
    std::uint64_t hash() const;
    // En passant target square if an en passant capture is legal, else kNoSquare.
    Square ep_square() const;

    bool is_capture(const Move &m) const; // includes en passant
    bool is_en_passant(const Move &m) const;
    bool is_castle(const Move &m) const;
    // For animating castling: the rook's from/to squares for a castle move.
    bool castle_rook_squares(const Move &m, Square *rook_from, Square *rook_to) const;
    // Material helpers for the UI: count of a role for a colour on the board.
    int count(Color c, Role r) const;

    bool operator==(const Position &) const = default;

  private:
    std::uint64_t occupied() const
    {
        return colors_[0] | colors_[1];
    }
    std::uint64_t pieces(Color c, Role r) const
    {
        return colors_[static_cast<int>(c)] & roles_[static_cast<int>(r)];
    }
    std::optional<Role> role_at(Square s) const;
    std::uint64_t attackers(Square s, std::uint64_t occ, std::uint64_t by) const;
    bool king_safe_after(Square from, Square to, Square ep_victim) const;
    void generate(std::uint64_t from_mask, MoveList &out) const;
    void generate_castles(MoveList &out) const;
    Square castling_rook(Color c, bool king_side) const;
    bool castle_squares(Square rook, Square *king_to, Square *rook_to) const;
    bool pseudo_ep_capturable() const;
    void put(Square s, Piece p);
    void remove(Square s);

    std::array<std::uint64_t, 2> colors_{};
    std::array<std::uint64_t, 6> roles_{};
    std::uint64_t castling_ = 0; // squares of rooks that still have castling rights
    Square ep_ = kNoSquare;      // square skipped by the last double pawn push
    Color turn_ = Color::white;
    int halfmoves_ = 0;
    int fullmoves_ = 1;
};

// UCI: emits standard castling as king two squares (e1g1). Parsing accepts e1g1
// and king-takes-own-rook (e1h1/e1a1), and promotion suffixes q/r/b/n.
std::string to_uci(const Move &m);
bool parse_uci(const Position &pos, std::string_view uci, Move *out); // false if bad/illegal
std::string to_san(const Position &pos, const Move &m); // "Nbd7", "exd6", "O-O", "e8=Q+"
// Tolerant of +/#/!/? suffixes, 0-0, e8Q and lowercase promotions.
bool parse_san(const Position &pos, std::string_view san, Move *out);

enum class Outcome : std::uint8_t
{
    ongoing,
    checkmate,
    stalemate,
    insufficient_material,
    fifty_moves,
    threefold
};

class Game
{
  public:
    Game();
    void reset(const Position &initial = Position::start());
    bool play(const Move &m); // false if illegal
    bool undo();              // pops last move
    std::size_t ply_count() const
    {
        return moves_.size();
    }
    const Position &initial() const
    {
        return positions_.front();
    }
    const Position &position() const
    {
        return positions_.back();
    }
    const Position &position_at(std::size_t ply) const
    {
        return positions_[ply];
    }
    const Move &move_at(std::size_t index) const
    {
        return moves_[index];
    }
    const std::string &san_at(std::size_t index) const
    {
        return sans_[index];
    }
    // Fifty-move and threefold are reported as soon as they are claimable.
    Outcome outcome() const;
    // Lichess Board API "moves" field (space-separated UCI). Appends when it
    // extends the current moves, otherwise rebuilds from initial(). Returns
    // false on any illegal or malformed move, leaving the state unchanged.
    bool apply_uci_moves(std::string_view moves);

  private:
    std::vector<Position> positions_;
    std::vector<Move> moves_;
    std::vector<std::string> sans_;
    std::vector<std::uint64_t> hashes_;
};

// Parses a movetext mainline (SAN tokens; skips move numbers, {comments},
// ;comments, (variations), NAGs, results and [headers]) from start, appending
// moves to out. Returns false on any illegal token.
bool parse_pgn_moves(std::string_view pgn, const Position &start, std::vector<Move> *out);

} // namespace pch::chess
