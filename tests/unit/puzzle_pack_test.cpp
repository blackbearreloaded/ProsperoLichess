// ProsperoLichess - Offline puzzle pack reader tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_file.hpp"
#include "puzzles/pack.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#ifndef PCH_SOURCE_DIR
#define PCH_SOURCE_DIR "."
#endif

namespace
{

using pch::puzzles::Pack;
using pch::puzzles::PackPuzzle;

struct CsvRow
{
    std::string id, fen;
    std::vector<std::string> moves, themes;
    int rating = 0, popularity = 0;
};

std::vector<std::string> split(const std::string &text, char separator)
{
    std::vector<std::string> parts;
    std::string current;
    for (char c : text)
    {
        if (c == separator)
        {
            parts.push_back(current);
            current.clear();
        }
        else
            current.push_back(c);
    }
    parts.push_back(current);
    return parts;
}

std::string fixture(const char *name)
{
    std::string data;
    EXPECT_TRUE(pch::save::read_file(std::string(PCH_SOURCE_DIR) + "/" + name, &data, 16u << 20))
        << name;
    return data;
}

std::vector<CsvRow> mini_rows()
{
    std::vector<CsvRow> rows;
    const std::vector<std::string> lines = split(fixture("tests/fixtures/puzzles/mini.csv"), '\n');
    for (std::size_t i = 1; i < lines.size(); ++i)
    {
        if (lines[i].empty())
            continue;
        const std::vector<std::string> fields = split(lines[i], ',');
        EXPECT_GE(fields.size(), 8u);
        CsvRow row;
        row.id = fields[0];
        row.fen = fields[1];
        row.moves = split(fields[2], ' ');
        row.rating = std::stoi(fields[3]);
        row.popularity = std::stoi(fields[5]);
        row.themes = split(fields[7], ' ');
        rows.push_back(row);
    }
    return rows;
}

Pack load_mini()
{
    Pack pack;
    std::string error;
    EXPECT_TRUE(pack.load(fixture("tests/fixtures/puzzles/mini.bin"), &error)) << error;
    return pack;
}

std::uint32_t le32(const std::string &data, std::size_t at)
{
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(data[at + i])) << (8 * i);
    return value;
}

void put32(std::string *data, std::size_t at, std::uint32_t value)
{
    for (std::size_t i = 0; i < 4; ++i)
        (*data)[at + i] = static_cast<char>((value >> (8 * i)) & 0xff);
}

// Recomputes the trailing CRC so a structural corruption reaches the validator.
void reseal(std::string *data)
{
    put32(data, data->size() - 4,
          pch::save::crc32(std::string_view(*data).substr(0, data->size() - 4)));
}

// Byte offset of the first record in a pack.
std::size_t first_record(const std::string &data)
{
    const std::size_t count = le32(data, 8);
    const std::size_t themes = static_cast<unsigned char>(data[12]) |
                               static_cast<std::size_t>(static_cast<unsigned char>(data[13])) << 8;
    const std::size_t buckets = static_cast<unsigned char>(data[20]) |
                                static_cast<std::size_t>(static_cast<unsigned char>(data[21])) << 8;
    std::size_t at = 76;
    for (std::size_t t = 0; t < themes; ++t)
        at += 1 + static_cast<unsigned char>(data[at]);
    return at + 4 * (buckets + 1) + 4 * (count + 1);
}

TEST(PuzzlePack, RoundTripsEveryFixtureField)
{
    const Pack pack = load_mini();
    const std::vector<CsvRow> rows = mini_rows();
    ASSERT_EQ(rows.size(), 18u);
    ASSERT_EQ(pack.size(), rows.size());
    EXPECT_EQ(pack.source_date(), "2026-09-09");
    EXPECT_EQ(pack.source_sha256().size(), 64u);

    std::map<std::string, PackPuzzle> by_id;
    int previous = 0;
    for (std::size_t i = 0; i < pack.size(); ++i)
    {
        PackPuzzle puzzle;
        ASSERT_TRUE(pack.get(i, &puzzle));
        EXPECT_GE(puzzle.rating, previous) << "sorted by rating";
        EXPECT_EQ(pack.rating(i), puzzle.rating);
        previous = puzzle.rating;
        by_id[puzzle.id] = puzzle;
    }
    PackPuzzle unused;
    EXPECT_FALSE(pack.get(pack.size(), &unused));

    for (const CsvRow &row : rows)
    {
        ASSERT_EQ(by_id.count(row.id), 1u) << row.id;
        const PackPuzzle &puzzle = by_id[row.id];
        EXPECT_EQ(puzzle.fen, row.fen) << row.id;
        EXPECT_EQ(puzzle.moves, row.moves) << row.id;
        EXPECT_EQ(puzzle.rating, row.rating) << row.id;
        EXPECT_EQ(puzzle.popularity, row.popularity) << row.id;
        std::set<std::string> expected(row.themes.begin(), row.themes.end());
        std::set<std::string> actual;
        for (int t = 0; t < pack.theme_count(); ++t)
            if (puzzle.themes.has(t))
                actual.insert(pack.theme_name(t));
        EXPECT_EQ(actual, expected) << row.id;
    }
}

TEST(PuzzlePack, ThemeTableAndCounts)
{
    const Pack pack = load_mini();
    const std::vector<CsvRow> rows = mini_rows();
    std::map<std::string, std::size_t> counts;
    for (const CsvRow &row : rows)
        for (const std::string &theme : row.themes)
            ++counts[theme];
    ASSERT_EQ(pack.theme_count(), static_cast<int>(counts.size()));
    for (const auto &[name, count] : counts)
    {
        const int index = pack.theme_index(name);
        ASSERT_GE(index, 0) << name;
        EXPECT_EQ(pack.theme_name(index), name);
        EXPECT_EQ(pack.theme_puzzle_count(index), count) << name;
    }
    EXPECT_EQ(pack.theme_index("noSuchTheme"), -1);
    EXPECT_EQ(pack.theme_name(-1), "");
    EXPECT_EQ(pack.theme_name(pack.theme_count()), "");
    EXPECT_EQ(pack.theme_puzzle_count(pack.theme_count()), 0u);
}

TEST(PuzzlePack, PickRespectsRatingThemeAndSeen)
{
    const Pack pack = load_mini();
    std::uint64_t rng = 12345;
    const std::function<bool(std::size_t)> none;
    std::size_t index = 0;

    for (int i = 0; i < 200; ++i)
    {
        ASSERT_TRUE(pack.pick(1000, 1600, -1, &rng, none, &index));
        EXPECT_GE(pack.rating(index), 1000);
        EXPECT_LE(pack.rating(index), 1600);
    }
    // Exact bounds are inclusive: 1495 is held by two puzzles, 499 by one.
    for (int i = 0; i < 50; ++i)
    {
        ASSERT_TRUE(pack.pick(1495, 1495, -1, &rng, none, &index));
        EXPECT_EQ(pack.rating(index), 1495);
    }
    ASSERT_TRUE(pack.pick(0, 499, -1, &rng, none, &index));
    EXPECT_EQ(pack.rating(index), 499);
    EXPECT_FALSE(pack.pick(0, 498, -1, &rng, none, &index));
    EXPECT_FALSE(pack.pick(2870, 5000, -1, &rng, none, &index));
    EXPECT_FALSE(pack.pick(1600, 1500, -1, &rng, none, &index));
    EXPECT_FALSE(pack.pick(0, 3000, pack.theme_count(), &rng, none, &index));

    const int mate = pack.theme_index("mateIn1");
    ASSERT_GE(mate, 0);
    std::set<std::size_t> found;
    for (int i = 0; i < 300; ++i)
    {
        ASSERT_TRUE(pack.pick(0, 3000, mate, &rng, none, &index));
        PackPuzzle puzzle;
        ASSERT_TRUE(pack.get(index, &puzzle));
        EXPECT_TRUE(puzzle.themes.has(mate));
        found.insert(index);
    }
    EXPECT_EQ(found.size(), pack.theme_puzzle_count(mate));

    // Seen puzzles are skipped until the range is exhausted.
    std::set<std::size_t> seen;
    const std::function<bool(std::size_t)> is_seen = [&](std::size_t i)
    { return seen.count(i) > 0; };
    while (pack.pick(0, 3000, -1, &rng, is_seen, &index))
    {
        EXPECT_EQ(seen.count(index), 0u);
        seen.insert(index);
    }
    EXPECT_EQ(seen.size(), pack.size());

    std::uint64_t a = 99, b = 99;
    std::size_t first = 0, second = 0;
    for (int i = 0; i < 20; ++i)
    {
        ASSERT_TRUE(pack.pick(0, 3000, -1, &a, none, &first));
        ASSERT_TRUE(pack.pick(0, 3000, -1, &b, none, &second));
        EXPECT_EQ(first, second);
    }
    std::uint64_t zero = 0;
    EXPECT_TRUE(pack.pick(0, 3000, -1, &zero, none, &index));
    EXPECT_NE(zero, 0u);
}

TEST(PuzzlePack, RejectsCorruptAndTruncatedData)
{
    const std::string good = fixture("tests/fixtures/puzzles/mini.bin");
    std::string error;
    Pack pack;
    for (std::size_t length = 0; length < good.size(); length += 7)
    {
        EXPECT_FALSE(pack.load(good.substr(0, length), &error)) << length;
        EXPECT_FALSE(error.empty());
        EXPECT_EQ(pack.size(), 0u);
    }
    EXPECT_FALSE(pack.load(good + std::string(1, '\0'), &error));
    for (std::size_t at = 0; at < good.size(); at += 5)
    {
        std::string flipped = good;
        flipped[at] = static_cast<char>(flipped[at] ^ 0x21);
        EXPECT_FALSE(pack.load(flipped, &error)) << at;
    }

    std::string magic = good;
    magic[0] = 'X';
    reseal(&magic);
    EXPECT_FALSE(pack.load(magic, &error));
    EXPECT_EQ(error, "puzzle pack: bad magic");

    std::string version = good;
    version[4] = 2;
    reseal(&version);
    EXPECT_FALSE(pack.load(version, &error));
    EXPECT_EQ(error, "puzzle pack: unsupported version");

    const std::size_t record = first_record(good);
    std::string flags = good;
    flags[record + 8] = static_cast<char>(0x80);
    reseal(&flags);
    EXPECT_FALSE(pack.load(flags, &error));
    EXPECT_EQ(error, "puzzle pack: corrupt record");

    std::string piece = good;
    piece[record + 21] = static_cast<char>(0x77); // nibble 7 is not a piece
    reseal(&piece);
    EXPECT_FALSE(pack.load(piece, &error));
    EXPECT_EQ(error, "puzzle pack: corrupt record");

    std::string rating = good;
    rating[record + 5] = static_cast<char>(0xff);
    rating[record + 6] = static_cast<char>(0x7f);
    reseal(&rating);
    EXPECT_FALSE(pack.load(rating, &error));

    ASSERT_TRUE(pack.load(good, &error)) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(pack.size(), 18u);
}

TEST(PuzzlePack, ThemeLabels)
{
    using pch::puzzles::theme_label;
    EXPECT_EQ(theme_label("mateIn2"), "Mate in 2");
    EXPECT_EQ(theme_label("advancedPawn"), "Advanced pawn");
    EXPECT_EQ(theme_label("kingsideAttack"), "Kingside attack");
    EXPECT_EQ(theme_label("fork"), "Fork");
    EXPECT_EQ(theme_label("queenRookEndgame"), "Queen rook endgame");
    EXPECT_EQ(theme_label("superGM"), "Super GM");
    EXPECT_EQ(theme_label("masterVsMaster"), "Master vs master");
    EXPECT_EQ(theme_label("xRayAttack"), "X-ray attack");
    EXPECT_EQ(theme_label("anastasiaMate"), "Anastasia's mate");
    EXPECT_EQ(theme_label("attackingF2F7"), "Attacking f2 or f7");
    EXPECT_EQ(theme_label(""), "");
}

TEST(PuzzlePack, ShippedPackLoadsAndCoversEveryTheme)
{
    Pack pack;
    std::string error;
    ASSERT_TRUE(pack.load(fixture("assets/puzzles/pack.bin"), &error)) << error;
    EXPECT_GE(pack.size(), 95000u);
    EXPECT_GE(pack.theme_count(), 60);
    for (int t = 0; t < pack.theme_count(); ++t)
    {
        EXPECT_GE(pack.theme_puzzle_count(t), 200u) << pack.theme_name(t);
        EXPECT_FALSE(pch::puzzles::theme_label(pack.theme_name(t)).empty());
    }
    for (std::size_t i = 0; i < pack.size(); i += 997)
    {
        PackPuzzle puzzle;
        ASSERT_TRUE(pack.get(i, &puzzle));
        EXPECT_EQ(puzzle.id.size(), 5u);
        EXPECT_EQ(split(puzzle.fen, ' ').size(), 6u);
        EXPECT_GE(puzzle.moves.size(), 2u);
        EXPECT_FALSE(puzzle.themes.empty());
    }
}

} // namespace
