// ProsperoLichess - Chess game history, outcomes and Lichess move-list sync tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chess/chess.hpp"

#include <gtest/gtest.h>

namespace
{

using namespace pch::chess;

Position load(const char *fen)
{
    Position pos;
    std::string error;
    EXPECT_TRUE(Position::from_fen(fen, &pos, &error)) << fen << ": " << error;
    return pos;
}

bool play_uci(Game &game, const char *text)
{
    Move m;
    return parse_uci(game.position(), text, &m) && game.play(m);
}

TEST(ChessGame, PlayUndoAndHistory)
{
    Game game;
    EXPECT_EQ(game.ply_count(), 0u);
    EXPECT_EQ(game.position(), Position::start());
    EXPECT_TRUE(play_uci(game, "e2e4"));
    EXPECT_TRUE(play_uci(game, "e7e5"));
    EXPECT_FALSE(game.play(Move{parse_square("e4"), parse_square("e6"), std::nullopt}));
    EXPECT_EQ(game.ply_count(), 2u);
    EXPECT_EQ(game.san_at(0), "e4");
    EXPECT_EQ(game.san_at(1), "e5");
    EXPECT_EQ(to_uci(game.move_at(1)), "e7e5");
    EXPECT_EQ(game.position_at(0), game.initial());
    EXPECT_EQ(game.position_at(2), game.position());
    EXPECT_EQ(game.position_at(1).fen(),
              "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    EXPECT_TRUE(game.undo());
    EXPECT_EQ(game.ply_count(), 1u);
    EXPECT_TRUE(game.undo());
    EXPECT_FALSE(game.undo());
    EXPECT_EQ(game.position(), Position::start());
    EXPECT_EQ(game.outcome(), Outcome::ongoing);

    game.reset(load("4k3/8/8/8/8/8/8/R3K3 w Q - 0 1"));
    EXPECT_TRUE(play_uci(game, "e1c1"));
    EXPECT_EQ(game.san_at(0), "O-O-O");
}

TEST(ChessGame, FoolsMateAndStalemate)
{
    Game game;
    for (const char *m : {"f2f3", "e7e5", "g2g4", "d8h4"})
    {
        ASSERT_TRUE(play_uci(game, m)) << m;
    }
    EXPECT_EQ(game.san_at(3), "Qh4#");
    EXPECT_EQ(game.outcome(), Outcome::checkmate);

    game.reset(load("7k/8/4Q3/6K1/8/8/8/8 w - - 0 1"));
    EXPECT_EQ(game.outcome(), Outcome::ongoing);
    ASSERT_TRUE(play_uci(game, "e6f7"));
    EXPECT_EQ(game.outcome(), Outcome::stalemate);
}

TEST(ChessGame, InsufficientMaterialAfterCapture)
{
    Game game;
    game.reset(load("4k3/8/8/8/8/8/3r4/4K3 w - - 0 1"));
    EXPECT_EQ(game.outcome(), Outcome::ongoing);
    ASSERT_TRUE(play_uci(game, "e1d2"));
    EXPECT_EQ(game.san_at(0), "Kxd2");
    EXPECT_EQ(game.outcome(), Outcome::insufficient_material);
}

TEST(ChessGame, ThreefoldByKnightShuffles)
{
    Game game;
    const char *shuffle[] = {"g1f3", "g8f6", "f3g1", "f6g8"};
    for (int round = 0; round < 2; ++round)
    {
        for (const char *m : shuffle)
        {
            EXPECT_EQ(game.outcome(), Outcome::ongoing);
            ASSERT_TRUE(play_uci(game, m)) << m;
        }
    }
    EXPECT_EQ(game.ply_count(), 8u);
    EXPECT_EQ(game.outcome(), Outcome::threefold);
    EXPECT_TRUE(game.undo());
    EXPECT_EQ(game.outcome(), Outcome::ongoing);
}

TEST(ChessGame, FiftyMoveRule)
{
    Game game;
    game.reset(load("4k3/8/8/8/8/8/8/R3K3 w - - 99 80"));
    EXPECT_EQ(game.outcome(), Outcome::ongoing);
    ASSERT_TRUE(play_uci(game, "a1a2"));
    EXPECT_EQ(game.position().halfmove_clock(), 100);
    EXPECT_EQ(game.outcome(), Outcome::fifty_moves);

    // Checkmate on the hundredth half-move wins.
    game.reset(load("k7/8/1K6/8/8/8/8/7R w - - 99 80"));
    ASSERT_TRUE(play_uci(game, "h1h8"));
    EXPECT_EQ(game.outcome(), Outcome::checkmate);
}

TEST(ChessGame, ApplyUciMovesAppendsAndRebuilds)
{
    Game game;
    ASSERT_TRUE(game.apply_uci_moves("e2e4 e7e5"));
    EXPECT_EQ(game.ply_count(), 2u);
    ASSERT_TRUE(game.apply_uci_moves("e2e4 e7e5 g1f3"));
    EXPECT_EQ(game.ply_count(), 3u);
    EXPECT_EQ(game.san_at(2), "Nf3");
    ASSERT_TRUE(game.apply_uci_moves("e2e4 e7e5 g1f3")); // unchanged
    EXPECT_EQ(game.ply_count(), 3u);

    // Takeback: shorter list rebuilds.
    ASSERT_TRUE(game.apply_uci_moves("e2e4 e7e5"));
    EXPECT_EQ(game.ply_count(), 2u);
    EXPECT_EQ(game.position().fen(),
              "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");

    // Divergent list rebuilds from the initial position.
    ASSERT_TRUE(game.apply_uci_moves("d2d4 d7d5"));
    EXPECT_EQ(game.ply_count(), 2u);
    EXPECT_EQ(game.san_at(0), "d4");

    // Empty list resets to the initial position.
    ASSERT_TRUE(game.apply_uci_moves(""));
    EXPECT_EQ(game.ply_count(), 0u);

    // Castling in king-takes-rook form is accepted and matches e1g1 history.
    game.reset(load("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
    ASSERT_TRUE(game.apply_uci_moves("e1h1"));
    ASSERT_TRUE(game.apply_uci_moves("e1g1 e8c8"));
    EXPECT_EQ(game.ply_count(), 2u);
    EXPECT_EQ(game.san_at(1), "O-O-O");
    EXPECT_EQ(game.initial().fen(), "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
}

TEST(ChessGame, ApplyUciMovesRejectsWithoutChangingState)
{
    Game game;
    ASSERT_TRUE(game.apply_uci_moves("e2e4 e7e5 g1f3"));
    const std::string fen = game.position().fen();
    EXPECT_FALSE(game.apply_uci_moves("e2e4 e7e5 g1f3 e8e1"));   // illegal append
    EXPECT_FALSE(game.apply_uci_moves("d2d4 d7d5 zz"));          // malformed rebuild
    EXPECT_FALSE(game.apply_uci_moves("e2e4 e7e5 g1f3 b8c6 x")); // malformed append
    EXPECT_FALSE(game.apply_uci_moves("e2e5"));                  // illegal rebuild
    EXPECT_EQ(game.ply_count(), 3u);
    EXPECT_EQ(game.position().fen(), fen);
    EXPECT_EQ(game.san_at(2), "Nf3");
}

} // namespace
