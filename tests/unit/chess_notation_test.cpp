// ProsperoLichess - Chess UCI, SAN and PGN movetext tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chess/chess.hpp"

#include <gtest/gtest.h>

#include <vector>

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

Move uci(const Position &pos, const char *text)
{
    Move m;
    EXPECT_TRUE(parse_uci(pos, text, &m)) << text;
    return m;
}

std::string san(const Position &pos, const char *text)
{
    return to_san(pos, uci(pos, text));
}

TEST(ChessUci, FormatsAndParses)
{
    const Position start = Position::start();
    EXPECT_EQ(to_uci(uci(start, "e2e4")), "e2e4");
    EXPECT_EQ(to_uci(Move{}), "0000");
    Move m;
    EXPECT_FALSE(parse_uci(start, "e2e5", &m)); // illegal
    EXPECT_FALSE(parse_uci(start, "e2e", &m));
    EXPECT_FALSE(parse_uci(start, "e2e9", &m));
    EXPECT_FALSE(parse_uci(start, "e2e4x", &m));
    EXPECT_FALSE(parse_uci(start, "", &m));
    EXPECT_FALSE(parse_uci(start, "e7e5", &m)); // wrong side

    const Position promo = load("1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    ASSERT_TRUE(parse_uci(promo, "a7b8n", &m));
    EXPECT_EQ(m, (Move{parse_square("a7"), parse_square("b8"), Role::knight}));
    EXPECT_EQ(to_uci(m), "a7b8n");
    EXPECT_TRUE(parse_uci(promo, "a7a8q", &m));
    EXPECT_EQ(m.promotion, Role::queen);
    EXPECT_FALSE(parse_uci(promo, "a7a8", &m)); // promotion piece required
    EXPECT_FALSE(parse_uci(promo, "a7a8k", &m));
}

TEST(ChessUci, CastlingBothNotations)
{
    const Position pos = load("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    Move king_two = uci(pos, "e1g1");
    Move takes_rook = uci(pos, "e1h1");
    EXPECT_EQ(king_two, takes_rook);
    EXPECT_EQ(to_uci(takes_rook), "e1g1");
    EXPECT_EQ(uci(pos, "e1a1"), uci(pos, "e1c1"));
    EXPECT_TRUE(pos.is_castle(uci(pos, "e1c1")));

    const Position black = load("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
    EXPECT_EQ(to_uci(uci(black, "e8h8")), "e8g8");
    EXPECT_EQ(to_uci(uci(black, "e8a8")), "e8c8");

    // No rights: neither notation is legal.
    const Position none = load("r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1");
    Move m;
    EXPECT_FALSE(parse_uci(none, "e1g1", &m));
    EXPECT_FALSE(parse_uci(none, "e1h1", &m));
}

TEST(ChessSan, Basics)
{
    const Position start = Position::start();
    EXPECT_EQ(san(start, "e2e4"), "e4");
    EXPECT_EQ(san(start, "g1f3"), "Nf3");
    EXPECT_EQ(san(load("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1"), "e5d6"), "exd6");
    EXPECT_EQ(san(load("4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1"), "e4d5"), "exd5");
    EXPECT_EQ(san(load("4k3/8/8/3p4/8/8/8/3QK3 w - - 0 1"), "d1d5"), "Qxd5");
    EXPECT_EQ(san(load("4k3/8/8/8/8/8/8/4K3 w - - 0 1"), "e1e2"), "Ke2");
}

TEST(ChessSan, Disambiguation)
{
    const Position files = load("4k3/8/8/8/8/8/8/1N2KN2 w - - 0 1");
    EXPECT_EQ(san(files, "b1d2"), "Nbd2");
    EXPECT_EQ(san(files, "f1d2"), "Nfd2");
    EXPECT_EQ(san(files, "b1c3"), "Nc3");

    const Position ranks = load("4k3/8/8/R7/8/8/8/R3K3 w - - 0 1");
    EXPECT_EQ(san(ranks, "a1a3"), "R1a3");
    EXPECT_EQ(san(ranks, "a5a3"), "R5a3");
    EXPECT_EQ(san(ranks, "a1b1"), "Rb1");

    const Position both = load("8/7k/8/8/8/Q7/8/Q1Q4K w - - 0 1");
    EXPECT_EQ(san(both, "a1b2"), "Qa1b2");
    EXPECT_EQ(san(both, "c1b2"), "Qcb2");
    EXPECT_EQ(san(both, "a3b2"), "Q3b2");

    // A pinned piece does not count for disambiguation.
    const Position pinned = load("4k3/4r3/8/8/8/8/4N3/1N2K3 w - - 0 1");
    EXPECT_EQ(san(pinned, "b1c3"), "Nc3");
}

TEST(ChessSan, PromotionsCastlingAndMate)
{
    const Position promo = load("1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    EXPECT_EQ(san(promo, "a7b8q"), "axb8=Q+");
    EXPECT_EQ(san(promo, "a7b8n"), "axb8=N");
    EXPECT_EQ(san(promo, "a7a8r"), "a8=R"); // b8 knight blocks

    EXPECT_EQ(san(load("5k2/8/8/8/8/8/8/4K2R w K - 0 1"), "e1g1"), "O-O+");
    EXPECT_EQ(san(load("r3k3/8/8/8/8/8/8/4K3 b q - 0 1"), "e8c8"), "O-O-O");

    Position pos = Position::start();
    for (const char *m : {"f2f3", "e7e5", "g2g4"})
    {
        pos = pos.after(uci(pos, m));
    }
    EXPECT_EQ(san(pos, "d8h4"), "Qh4#");
}

TEST(ChessSan, ParseTolerance)
{
    const Position start = Position::start();
    Move m;
    ASSERT_TRUE(parse_san(start, "Nf3!?", &m));
    EXPECT_EQ(to_uci(m), "g1f3");
    ASSERT_TRUE(parse_san(start, "e4+", &m));
    EXPECT_EQ(to_uci(m), "e2e4");
    EXPECT_FALSE(parse_san(start, "e5", &m));
    EXPECT_FALSE(parse_san(start, "Nf4", &m));
    EXPECT_FALSE(parse_san(start, "", &m));
    EXPECT_FALSE(parse_san(start, "O-O", &m));
    EXPECT_FALSE(parse_san(start, "Zz9", &m));

    const Position castle = load("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    ASSERT_TRUE(parse_san(castle, "0-0", &m));
    EXPECT_EQ(to_uci(m), "e1g1");
    ASSERT_TRUE(parse_san(castle, "O-O-O+", &m));
    EXPECT_EQ(to_uci(m), "e1c1");
    EXPECT_FALSE(parse_san(castle, "Kg1", &m)); // castling needs castling notation

    const Position promo = load("1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    for (const char *text : {"a8=Q", "a8Q", "a8=q", "a8q"})
    {
        ASSERT_TRUE(parse_san(promo, text, &m)) << text;
        EXPECT_EQ(to_uci(m), "a7a8q") << text;
    }
    ASSERT_TRUE(parse_san(promo, "axb8=N", &m));
    EXPECT_EQ(to_uci(m), "a7b8n");
    ASSERT_TRUE(parse_san(promo, "axb8b", &m));
    EXPECT_EQ(to_uci(m), "a7b8b");
    EXPECT_FALSE(parse_san(promo, "a8", &m)); // missing promotion piece

    const Position files = load("4k3/8/8/8/8/8/8/1N2KN2 w - - 0 1");
    EXPECT_FALSE(parse_san(files, "Nd2", &m)); // ambiguous
    ASSERT_TRUE(parse_san(files, "Nfd2", &m));
    EXPECT_EQ(to_uci(m), "f1d2");
    ASSERT_TRUE(parse_san(files, "Nb1d2", &m));
    EXPECT_EQ(to_uci(m), "b1d2");

    const Position both = load("8/7k/8/8/8/Q7/8/Q1Q4K w - - 0 1");
    ASSERT_TRUE(parse_san(both, "Qa1xb2", &m));
    EXPECT_EQ(to_uci(m), "a1b2");
    EXPECT_FALSE(parse_san(both, "Qab2", &m)); // a1 and a3
}

TEST(ChessSan, RoundTripsAllMovesOfPerftPositions)
{
    const char *fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    };
    std::size_t checked = 0;
    for (const char *fen : fens)
    {
        const Position root = load(fen);
        MoveList first;
        root.legal_moves(first);
        for (const Move &m1 : first)
        {
            // Depth 2: every reply of every move.
            const Position child = root.after(m1);
            for (const Position *pos : {&root, &child})
            {
                MoveList moves;
                pos->legal_moves(moves);
                for (const Move &m : moves)
                {
                    const std::string text = to_san(*pos, m);
                    Move parsed;
                    ASSERT_TRUE(parse_san(*pos, text, &parsed)) << pos->fen() << " " << text;
                    ASSERT_EQ(parsed, m) << pos->fen() << " " << text;
                    ++checked;
                }
            }
        }
    }
    EXPECT_GT(checked, 4000u);
}

TEST(ChessPgn, ParsesPuzzleMainline)
{
    std::vector<Move> moves;
    ASSERT_TRUE(parse_pgn_moves("e4 e5 Nf3 Nc6 Bc4 Nf6 Ng5 d5 exd5 Nxd5 Nxf7 Kxf7 Qf3+ Ke6 Nc3",
                                Position::start(), &moves));
    ASSERT_EQ(moves.size(), 15u);
    EXPECT_EQ(to_uci(moves[0]), "e2e4");
    EXPECT_EQ(to_uci(moves[10]), "g5f7");
    EXPECT_EQ(to_uci(moves[14]), "b1c3");
}

TEST(ChessPgn, SkipsNumbersCommentsNagsHeadersAndResults)
{
    const char *pgn = "[Event \"Rated Blitz game\"]\n"
                      "[Site \"https://lichess.org/abc]def\"]\n"
                      "\n"
                      "1. e4 {[%clk 0:03:00]} 1... e5 $1 2.Nf3 Nc6!? ; line comment\n"
                      "3. Bb5 (3. Bc4 Bc5 (3... Nf6)) 3... a6 4. Ba4 1/2-1/2";
    std::vector<Move> moves;
    ASSERT_TRUE(parse_pgn_moves(pgn, Position::start(), &moves));
    ASSERT_EQ(moves.size(), 7u);
    EXPECT_EQ(to_uci(moves[4]), "f1b5");
    EXPECT_EQ(to_uci(moves[6]), "b5a4");

    moves.clear();
    EXPECT_TRUE(
        parse_pgn_moves("1. e4 e5 2. Qh5 Nc6 3. Bc4 Nf6 4. Qxf7# 1-0", Position::start(), &moves));
    EXPECT_EQ(moves.size(), 7u);

    moves.clear();
    EXPECT_TRUE(parse_pgn_moves("*", Position::start(), &moves));
    EXPECT_TRUE(moves.empty());
    EXPECT_FALSE(parse_pgn_moves("1. e4 e4", Position::start(), &moves));

    // From a non-initial position (puzzle-style FEN start).
    moves.clear();
    const Position black = load("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
    ASSERT_TRUE(parse_pgn_moves("1... O-O 2. O-O-O", black, &moves));
    ASSERT_EQ(moves.size(), 2u);
    EXPECT_EQ(to_uci(moves[0]), "e8g8");
    EXPECT_EQ(to_uci(moves[1]), "e1c1");
}

} // namespace
