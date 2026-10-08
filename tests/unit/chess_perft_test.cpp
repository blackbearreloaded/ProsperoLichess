// ProsperoLichess - Chess move generator perft tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chess/chess.hpp"

#include <gtest/gtest.h>

#include <cstdint>

namespace
{

using namespace pch::chess;

std::uint64_t perft(const Position &pos, int depth)
{
    MoveList moves;
    pos.legal_moves(moves);
    if (depth <= 1)
    {
        return depth == 1 ? moves.size() : 1;
    }
    std::uint64_t nodes = 0;
    for (const Move &m : moves)
    {
        nodes += perft(pos.after(m), depth - 1);
    }
    return nodes;
}

Position load(const char *fen)
{
    Position pos;
    std::string error;
    EXPECT_TRUE(Position::from_fen(fen, &pos, &error)) << fen << ": " << error;
    return pos;
}

TEST(ChessPerft, StartPosition)
{
    const Position pos = Position::start();
    EXPECT_EQ(perft(pos, 1), 20u);
    EXPECT_EQ(perft(pos, 2), 400u);
    EXPECT_EQ(perft(pos, 3), 8902u);
    EXPECT_EQ(perft(pos, 4), 197281u);
}

TEST(ChessPerft, StartPositionDepth5)
{
    EXPECT_EQ(perft(Position::start(), 5), 4865609u);
}

TEST(ChessPerft, Kiwipete)
{
    const Position pos =
        load("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    EXPECT_EQ(perft(pos, 1), 48u);
    EXPECT_EQ(perft(pos, 2), 2039u);
    EXPECT_EQ(perft(pos, 3), 97862u);
}

TEST(ChessPerft, Position3)
{
    const Position pos = load("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    EXPECT_EQ(perft(pos, 1), 14u);
    EXPECT_EQ(perft(pos, 2), 191u);
    EXPECT_EQ(perft(pos, 3), 2812u);
    EXPECT_EQ(perft(pos, 4), 43238u);
}

TEST(ChessPerft, Position4)
{
    const Position pos = load("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
    EXPECT_EQ(perft(pos, 1), 6u);
    EXPECT_EQ(perft(pos, 2), 264u);
    EXPECT_EQ(perft(pos, 3), 9467u);
}

TEST(ChessPerft, Position5)
{
    const Position pos = load("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
    EXPECT_EQ(perft(pos, 1), 44u);
    EXPECT_EQ(perft(pos, 2), 1486u);
    EXPECT_EQ(perft(pos, 3), 62379u);
}

TEST(ChessPerft, EnPassantExposingKingOnRankIsIllegal)
{
    const Position pos = load("8/8/8/KPp4r/8/8/8/7k w - c6 0 1");
    const Move ep{make_square(1, 4), make_square(2, 5), std::nullopt};
    EXPECT_FALSE(pos.is_legal(ep));
    MoveList moves;
    pos.legal_moves(moves);
    EXPECT_FALSE(moves.contains(ep));
    EXPECT_EQ(pos.ep_square(), kNoSquare);
    EXPECT_EQ(pos.fen(), "8/8/8/KPp4r/8/8/8/7k w - - 0 1");

    // Without the pinning rook the capture is fine.
    const Position free = load("8/8/8/KPp5/8/8/8/7k w - c6 0 1");
    EXPECT_TRUE(free.is_legal(ep));
    EXPECT_TRUE(free.is_en_passant(ep));
    EXPECT_TRUE(free.is_capture(ep));
    const Position after = free.after(ep);
    EXPECT_FALSE(after.piece_at(make_square(2, 4)).has_value());
    EXPECT_EQ(after.piece_at(make_square(2, 5)), (Piece{Color::white, Role::pawn}));
}

} // namespace
