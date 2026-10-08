// ProsperoLichess - Offline Lichess puzzle pack reader.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace pch::puzzles
{

// Pack format, version 1 (written by tools/build-puzzle-pack.py, which holds
// the authoritative description). All integers are little-endian.
//
//   header, 76 bytes:
//      0 "PCHP" | 4 u16 version | 6 u16 header size | 8 u32 puzzle count N |
//     12 u16 theme count T (<= 128) | 14 u8 theme mask bytes M = ceil(T/8) |
//     15 u8 0 | 16 u16 rating index base | 18 u16 rating step | 20 u16 bucket
//     count B | 22 u16 0 | 24 char[16] source date "YYYY-MM-DD" | 40 u8[32]
//     source SHA-256 | 72 u32 total file size
//   theme table   T x (u8 length, camelCase name); theme i is mask bit i
//   rating index  (B + 1) x u32: first puzzle with rating >= base + b * step
//   offsets       (N + 1) x u32 into the record blob; [N] = blob size
//   records       sorted by rating:
//                 char[5] id | u16 rating | i8 popularity | u8 flags (bit0
//                 black to move, bits1-4 KQkq) | u8 en-passant square or 0xFF
//                 | u8 halfmove | u16 fullmove | u64 occupancy (a1 = bit 0) |
//                 ceil(popcount/2) bytes of piece nibbles in square order, low
//                 nibble first (1..6 white PNBRQK, 9..14 black) | u8 move
//                 count | u16 moves (from | to << 6 | promo << 12, promo
//                 0 none 1 n 2 b 3 r 4 q) | u8[M] theme mask
//   u32 CRC-32 (IEEE) of every preceding byte

inline constexpr int kMaxThemes = 128;

// Fixed-size theme bitmask (the Lichess data has more than 64 themes).
struct ThemeSet
{
    std::uint64_t words[2] = {0, 0};

    bool has(int theme) const
    {
        return theme >= 0 && theme < kMaxThemes && ((words[theme >> 6] >> (theme & 63)) & 1u);
    }
    void set(int theme)
    {
        if (theme >= 0 && theme < kMaxThemes)
            words[theme >> 6] |= std::uint64_t{1} << (theme & 63);
    }
    bool empty() const
    {
        return words[0] == 0 && words[1] == 0;
    }
};

struct PackPuzzle
{
    std::string id;                 // Lichess puzzle id, e.g. "00sHx"
    std::string fen;                // position BEFORE the opponent's first move (CSV semantics)
    std::vector<std::string> moves; // UCI; moves[0] = opponent's setup move, then the solution
    int rating = 0;
    int popularity = 0;
    ThemeSet themes; // bits over theme indices
};

class Pack
{
  public:
    // Takes ownership of the whole file. Validates the header, CRC, tables and
    // every record; on failure the pack is left empty and *error is set.
    bool load(std::string data, std::string *error);

    std::size_t size() const;
    bool get(std::size_t index, PackPuzzle *out) const;
    int rating(std::size_t index) const; // 0 when out of range

    int theme_count() const;
    const std::string &theme_name(int index) const; // "" when out of range
    int theme_index(std::string_view name) const;   // -1 if unknown
    std::size_t theme_puzzle_count(int theme) const;

    // Picks a puzzle with rating in [min_rating, max_rating] and (theme < 0 or
    // having that theme), skipping indices for which seen(index) returns true
    // (seen may be empty). *rng is xorshift64 state, advanced in place (0 is
    // replaced by a fixed seed). Returns false if no puzzle qualifies.
    bool pick(int min_rating, int max_rating, int theme, std::uint64_t *rng,
              const std::function<bool(std::size_t)> &seen, std::size_t *index) const;

    std::string source_date() const;
    std::string source_sha256() const; // lowercase hex

  private:
    bool has_theme(std::size_t index, int theme) const;
    std::size_t lower_bound(int rating) const;
    std::size_t record(std::size_t index) const;

    std::string data_;
    std::size_t count_ = 0;
    std::size_t mask_bytes_ = 0;
    int rating_base_ = 0;
    int rating_step_ = 1;
    std::size_t rating_buckets_ = 0;
    std::size_t rating_index_ = 0; // byte offsets into data_
    std::size_t offsets_ = 0;
    std::size_t records_ = 0;
    std::vector<std::string> themes_;
    std::vector<std::size_t> theme_counts_;
};

// Human-readable label for a theme name ("mateIn2" -> "Mate in 2",
// "advancedPawn" -> "Advanced pawn", "kingsideAttack" -> "Kingside attack"),
// in the player's language for the themes Lichess has today.
std::string theme_label(std::string_view name);

} // namespace pch::puzzles
