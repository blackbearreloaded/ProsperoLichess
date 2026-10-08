// ProsperoLichess - Internal bitboard attack tables and Zobrist keys.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "chess/chess.hpp"

#include <array>
#include <bit>
#include <cstdint>

namespace pch::chess::bb
{

using Bitboard = std::uint64_t;

constexpr Bitboard bit(int s)
{
    // Masking keeps the shift defined for every input (callers pass 0..63).
    return Bitboard{1} << (static_cast<unsigned>(s) & 63u);
}

constexpr Bitboard kRank1 = 0xffull;
constexpr Bitboard kFileA = 0x0101010101010101ull;
constexpr Bitboard kDarkSquares = 0xaa55aa55aa55aa55ull;

constexpr Bitboard rank_mask(int r)
{
    return kRank1 << (8 * r);
}

constexpr Bitboard file_mask(int f)
{
    return kFileA << f;
}

inline int lsb(Bitboard b)
{
    return std::countr_zero(b);
}

inline int msb(Bitboard b)
{
    return 63 - std::countl_zero(b);
}

inline int pop_lsb(Bitboard &b)
{
    const int s = std::countr_zero(b);
    b &= b - 1;
    return s;
}

inline int popcount(Bitboard b)
{
    return std::popcount(b);
}

constexpr Bitboard step_mask(int s, const int (*deltas)[2], int n)
{
    Bitboard out = 0;
    const int f = s & 7;
    const int r = s >> 3;
    for (int i = 0; i < n; ++i)
    {
        const int nf = f + deltas[i][0];
        const int nr = r + deltas[i][1];
        if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8)
        {
            out |= bit(nr * 8 + nf);
        }
    }
    return out;
}

constexpr int kKnightDeltas[8][2] = {{1, 2},   {2, 1},   {2, -1}, {1, -2},
                                     {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
constexpr int kKingDeltas[8][2] = {{1, 0},  {1, 1},   {0, 1},  {-1, 1},
                                   {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
constexpr int kWhitePawnDeltas[2][2] = {{-1, 1}, {1, 1}};
constexpr int kBlackPawnDeltas[2][2] = {{-1, -1}, {1, -1}};

// Ray directions: the first four increase the square index, the last four decrease it.
constexpr int kRayDeltas[8][2] = {{0, 1},  {1, 1},   {1, 0},  {-1, 1},
                                  {0, -1}, {-1, -1}, {-1, 0}, {1, -1}};

struct Tables
{
    std::array<Bitboard, 64> knight{};
    std::array<Bitboard, 64> king{};
    std::array<std::array<Bitboard, 64>, 2> pawn{};
    std::array<std::array<Bitboard, 64>, 8> ray{};
};

constexpr Tables make_tables()
{
    Tables t;
    for (int s = 0; s < 64; ++s)
    {
        t.knight[s] = step_mask(s, kKnightDeltas, 8);
        t.king[s] = step_mask(s, kKingDeltas, 8);
        t.pawn[0][s] = step_mask(s, kWhitePawnDeltas, 2);
        t.pawn[1][s] = step_mask(s, kBlackPawnDeltas, 2);
        for (int d = 0; d < 8; ++d)
        {
            Bitboard ray = 0;
            int f = (s & 7) + kRayDeltas[d][0];
            int r = (s >> 3) + kRayDeltas[d][1];
            while (f >= 0 && f < 8 && r >= 0 && r < 8)
            {
                ray |= bit(r * 8 + f);
                f += kRayDeltas[d][0];
                r += kRayDeltas[d][1];
            }
            t.ray[d][s] = ray;
        }
    }
    return t;
}

inline constexpr Tables kTables = make_tables();

inline Bitboard ray_attacks(int dir, int s, Bitboard occ)
{
    Bitboard a = kTables.ray[dir][s];
    const Bitboard blockers = a & occ;
    if (blockers)
    {
        const int b = dir < 4 ? lsb(blockers) : msb(blockers);
        a ^= kTables.ray[dir][b];
    }
    return a;
}

inline Bitboard rook_attacks(int s, Bitboard occ)
{
    return ray_attacks(0, s, occ) | ray_attacks(2, s, occ) | ray_attacks(4, s, occ) |
           ray_attacks(6, s, occ);
}

inline Bitboard bishop_attacks(int s, Bitboard occ)
{
    return ray_attacks(1, s, occ) | ray_attacks(3, s, occ) | ray_attacks(5, s, occ) |
           ray_attacks(7, s, occ);
}

inline Bitboard knight_attacks(int s)
{
    return kTables.knight[s];
}

inline Bitboard king_attacks(int s)
{
    return kTables.king[s];
}

// Squares attacked by a pawn of colour c standing on s.
inline Bitboard pawn_attacks(Color c, int s)
{
    return kTables.pawn[static_cast<int>(c)][s];
}

// Squares strictly between a and b on the same rank (empty otherwise).
inline Bitboard rank_between(int a, int b)
{
    if ((a >> 3) != (b >> 3) || a == b)
    {
        return 0;
    }
    const int lo = a < b ? a : b;
    const int hi = a < b ? b : a;
    return (bit(hi) - 1) & ~(bit(lo + 1) - 1);
}

// Squares from a to b inclusive on the same rank.
inline Bitboard rank_span(int a, int b)
{
    return rank_between(a, b) | bit(a) | bit(b);
}

// Zobrist keys: 12 x 64 piece keys, 64 castling-rook keys, 8 ep-file keys, turn.
struct Zobrist
{
    std::array<std::array<std::uint64_t, 64>, 12> piece{};
    std::array<std::uint64_t, 64> castling{};
    std::array<std::uint64_t, 8> ep{};
    std::uint64_t black_to_move = 0;
};

constexpr std::uint64_t splitmix64(std::uint64_t &state)
{
    std::uint64_t z = (state += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

constexpr Zobrist make_zobrist()
{
    Zobrist z;
    std::uint64_t state = 0x50524f535045524full; // "PROSPERO"
    for (auto &table : z.piece)
    {
        for (auto &key : table)
        {
            key = splitmix64(state);
        }
    }
    for (auto &key : z.castling)
    {
        key = splitmix64(state);
    }
    for (auto &key : z.ep)
    {
        key = splitmix64(state);
    }
    z.black_to_move = splitmix64(state);
    return z;
}

inline constexpr Zobrist kZobrist = make_zobrist();

} // namespace pch::chess::bb
