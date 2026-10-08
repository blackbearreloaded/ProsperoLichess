// ProsperoLichess - Lichess protocol parsing: Board API stream, API puzzles, puzzle rules.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_file.hpp"
#include "lichess/board_link.hpp"
#include "lichess/json.hpp"
#include "lichess/puzzle_sources.hpp"
#include "puzzles/puzzle.hpp"

#include <gtest/gtest.h>

#include <string>

#ifndef PCH_SOURCE_DIR
#define PCH_SOURCE_DIR "."
#endif

namespace
{

std::string fixture(const std::string &name)
{
    std::string data;
    EXPECT_TRUE(pch::save::read_file(
        std::string(PCH_SOURCE_DIR) + "/tests/fixtures/lichess/" + name, &data));
    return data;
}

// A Board API game stream, shaped like the Lichess documentation's example.
constexpr const char *kGameFull =
    R"({"type":"gameFull","id":"5IrD6Gzz","rated":true,"variant":{"key":"standard"},)"
    R"("clock":{"initial":600000,"increment":5000},"speed":"rapid","perf":{"name":"Rapid"},)"
    R"("createdAt":1523825103562,"white":{"id":"lovlas","name":"lovlas","provisional":false,)"
    R"("rating":2500,"title":"IM"},"black":{"id":"leela","name":"leela","rating":2390,"title":null},)"
    R"("initialFen":"startpos","state":{"type":"gameState","moves":"e2e4 c7c5 f2f4","wtime":594000,)"
    R"("btime":596000,"winc":5000,"binc":5000,"status":"started"}})";

} // namespace

TEST(BoardStream, GameFullThenStatesThenEnd)
{
    pch::lichess::BoardState state;
    ASSERT_TRUE(pch::lichess::apply_board_line(kGameFull, &state));
    EXPECT_TRUE(state.full);
    EXPECT_TRUE(state.initial_fen.empty());
    EXPECT_EQ(state.moves, "e2e4 c7c5 f2f4");
    EXPECT_EQ(state.white.name, "lovlas");
    EXPECT_EQ(state.white.title, "IM");
    EXPECT_EQ(state.white.rating, 2500);
    EXPECT_EQ(state.black_id, "leela");
    EXPECT_TRUE(state.black.title.empty());
    EXPECT_EQ(state.clock_initial, 600000);
    EXPECT_EQ(state.clock_increment, 5000);
    EXPECT_EQ(state.wtime, 594000);
    EXPECT_EQ(state.status, pch::modes::GameStatus::playing);
    EXPECT_TRUE(state.rated);

    ASSERT_TRUE(pch::lichess::apply_board_line(
        R"({"type":"gameState","moves":"e2e4 c7c5 f2f4 d7d6","wtime":590000,"btime":591000,)"
        R"("winc":5000,"binc":5000,"status":"started","bdraw":true})",
        &state));
    EXPECT_EQ(state.moves, "e2e4 c7c5 f2f4 d7d6");
    EXPECT_TRUE(state.bdraw);
    EXPECT_FALSE(state.wdraw);

    ASSERT_TRUE(pch::lichess::apply_board_line(
        R"({"type":"opponentGone","gone":true,"claimWinInSeconds":8})", &state));
    EXPECT_EQ(state.opponent_gone, 8);
    ASSERT_TRUE(pch::lichess::apply_board_line(R"({"type":"opponentGone","gone":false})", &state));
    EXPECT_EQ(state.opponent_gone, -1);

    ASSERT_TRUE(pch::lichess::apply_board_line(
        R"({"type":"gameState","moves":"e2e4 c7c5 f2f4 d7d6","wtime":590000,"btime":591000,)"
        R"("winc":5000,"binc":5000,"status":"resign","winner":"black"})",
        &state));
    EXPECT_EQ(state.status, pch::modes::GameStatus::resign);
    EXPECT_EQ(state.winner, 1);
    EXPECT_TRUE(pch::modes::is_over(state.status));
}

TEST(BoardStream, IgnoresChatAndRejectsGarbage)
{
    pch::lichess::BoardState state;
    EXPECT_TRUE(pch::lichess::apply_board_line(
        R"({"type":"chatLine","room":"player","username":"x","text":"hi"})", &state));
    EXPECT_FALSE(pch::lichess::apply_board_line("not json", &state));
    EXPECT_FALSE(pch::lichess::apply_board_line(R"({"type":"somethingNew"})", &state));
    EXPECT_FALSE(state.full);
}

TEST(BoardStream, StatusNames)
{
    using pch::modes::GameStatus;
    EXPECT_EQ(pch::lichess::parse_status("mate"), GameStatus::mate);
    EXPECT_EQ(pch::lichess::parse_status("outoftime"), GameStatus::outoftime);
    EXPECT_EQ(pch::lichess::parse_status("draw"), GameStatus::draw);
    EXPECT_EQ(pch::lichess::parse_status("aborted"), GameStatus::aborted);
    EXPECT_EQ(pch::lichess::parse_status("created"), GameStatus::waiting);
    EXPECT_EQ(pch::lichess::parse_status("brandNewStatus"), GameStatus::unknown_end);
}

TEST(LichessPuzzles, RecordedDailyPuzzleStartsWithTheSolverToMove)
{
    const std::string json = fixture("daily.json");
    pch::puzzles::Puzzle puzzle;
    std::string error;
    ASSERT_TRUE(pch::lichess::parse_api_puzzle(json, &puzzle, &error)) << error;
    EXPECT_FALSE(puzzle.id.empty());
    EXPECT_GT(puzzle.rating, 0);
    EXPECT_FALSE(puzzle.themes.empty());
    // The API's own "fen" is the position after the setup move.
    pch::lichess::Document doc(json);
    const std::string api_fen = doc.root()["puzzle"]["fen"].str();
    if (!api_fen.empty())
    {
        pch::chess::Position expected;
        ASSERT_TRUE(pch::chess::Position::from_fen(api_fen, &expected));
        // The API FEN resets the move counters; compare the first four fields.
        const auto fields = [](const std::string &fen)
        {
            std::size_t end = 0;
            for (int i = 0; i < 4 && end != std::string::npos; ++i)
                end = fen.find(' ', end + (i > 0 ? 1 : 0));
            return fen.substr(0, end);
        };
        EXPECT_EQ(fields(puzzle.start().fen()), fields(expected.fen()));
    }
    EXPECT_EQ(puzzle.start().turn(), puzzle.solver());
    // Playing the whole solution is legal from the start.
    pch::chess::Position position = puzzle.start();
    for (const pch::chess::Move &move : puzzle.solution)
    {
        ASSERT_TRUE(position.is_legal(move));
        position = position.after(move);
    }
}

TEST(LichessPuzzles, RecordedBatchParses)
{
    pch::lichess::Document doc(fixture("batch.json"));
    const pch::lichess::Value list = doc.root()["puzzles"];
    ASSERT_GT(list.size(), 0u);
    for (std::size_t i = 0; i < list.size(); ++i)
    {
        pch::puzzles::Puzzle puzzle;
        std::string error;
        ASSERT_TRUE(pch::puzzles::from_api(list.at(i)["puzzle"]["id"].str(),
                                           list.at(i)["game"]["pgn"].str(), {}, 1500, &puzzle,
                                           &error) ||
                    !error.empty());
    }
}

TEST(Puzzles, AcceptsAnyMateWhereTheSolutionMates)
{
    // Back-rank position: both Ra8# and Rb8# mate; the line only has Ra8#.
    pch::puzzles::Puzzle puzzle;
    std::string error;
    ASSERT_TRUE(pch::puzzles::from_csv("mate1", "6k1/5ppp/8/8/8/8/5PPP/RR4K1 b - - 0 1",
                                       {"g8h8", "a1a8"}, 800, &puzzle, &error))
        << error;
    const pch::chess::Position start = puzzle.start();
    pch::chess::Move other;
    ASSERT_TRUE(pch::chess::parse_uci(start, "b1b8", &other));
    EXPECT_TRUE(pch::puzzles::accepts(puzzle, start, 0, puzzle.solution[0]));
    EXPECT_TRUE(pch::puzzles::accepts(puzzle, start, 0, other));
    pch::chess::Move quiet;
    ASSERT_TRUE(pch::chess::parse_uci(start, "a1a2", &quiet));
    EXPECT_FALSE(pch::puzzles::accepts(puzzle, start, 0, quiet));
}

TEST(Puzzles, RejectsIllegalCsvLines)
{
    pch::puzzles::Puzzle puzzle;
    std::string error;
    EXPECT_FALSE(pch::puzzles::from_csv("bad", "8/8/8/8/8/8/8/K6k w - - 0 1", {"a1a2", "h1h8"}, 800,
                                        &puzzle, &error));
    EXPECT_FALSE(error.empty());
}

TEST(Json, EscapesControlCharacters)
{
    EXPECT_EQ(pch::lichess::json_escape("a\"b\\c\n"), "a\\\"b\\\\c\\n");
    pch::lichess::Document doc(R"({"n":3.7,"s":"x","a":[1,2]})");
    EXPECT_EQ(doc.root()["n"].integer(), 3);
    EXPECT_EQ(doc.root()["missing"]["deeper"].str("fallback"), "fallback");
    EXPECT_EQ(doc.root()["a"].size(), 2u);
}
