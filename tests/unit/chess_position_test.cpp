// ProsperoLichess - Chess position FEN, status and material tests.
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

Move mv(const char *uci, const Position &pos)
{
    Move m;
    EXPECT_TRUE(parse_uci(pos, uci, &m)) << uci;
    return m;
}

TEST(ChessSquares, NamesAndParsing)
{
    EXPECT_EQ(square_name(0), "a1");
    EXPECT_EQ(square_name(28), "e4");
    EXPECT_EQ(square_name(63), "h8");
    EXPECT_EQ(parse_square("e4"), 28);
    EXPECT_EQ(parse_square("i1"), kNoSquare);
    EXPECT_EQ(parse_square("a9"), kNoSquare);
    EXPECT_EQ(parse_square("e"), kNoSquare);
    EXPECT_EQ(make_square(4, 3), 28);
    EXPECT_EQ(make_square(8, 0), kNoSquare);
    EXPECT_EQ(file_of(28), 4);
    EXPECT_EQ(rank_of(28), 3);
    EXPECT_EQ(role_char(Role::knight), 'n');
    EXPECT_EQ(opposite(Color::white), Color::black);
}

TEST(ChessFen, RoundTrips)
{
    const char *fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3",
        "4k3/8/8/8/8/8/8/4K2R b K - 12 40",
    };
    for (const char *fen : fens)
    {
        EXPECT_EQ(load(fen).fen(), fen);
    }
    EXPECT_EQ(Position::start().fen(), fens[0]);
    EXPECT_EQ(load(fens[0]), Position::start());
}

TEST(ChessFen, NormalisesIrrelevantFields)
{
    // En passant square without a capturing pawn is dropped (Lichess style).
    EXPECT_EQ(load("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1").fen(),
              "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    // Castling rights without a rook are dropped.
    EXPECT_EQ(load("4k3/8/8/8/8/8/8/4K3 w KQkq - 0 1").fen(), "4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    // Missing clocks default to 0 1.
    EXPECT_EQ(load("4k3/8/8/8/8/8/8/4K3 w -").fen(), "4k3/8/8/8/8/8/8/4K3 w - - 0 1");
}

TEST(ChessFen, RejectsBadFens)
{
    const char *bad[] = {
        "",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP w KQkq - 0 1",            // 7 ranks
        "rnbqkbnr/pppppppp/9/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",   // 9 files
        "rnbqkbnr/pppppppp/7/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",   // 7 files
        "rnbqkbnr/ppppxppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",   // bad piece
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1",   // bad turn
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkz - 0 1",   // bad castling
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq e9 0 1",  // bad ep
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - x 1",   // bad clock
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 7", // extra field
        "rnbq1bnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQ - 0 1",     // no black king
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBKKBNR w kq - 0 1",     // two white kings
        "4k3/8/8/8/8/8/8/P3K3 w - - 0 1",                             // pawn on rank 1
        "4k3/4R3/8/8/8/8/8/4K3 w - - 0 1",                            // side not to move in check
    };
    for (const char *fen : bad)
    {
        Position pos;
        std::string error;
        EXPECT_FALSE(Position::from_fen(fen, &pos, &error)) << fen;
        EXPECT_FALSE(error.empty()) << fen;
    }
}

TEST(ChessPosition, PiecesAndCounts)
{
    const Position pos = Position::start();
    EXPECT_EQ(pos.piece_at(parse_square("e1")), (Piece{Color::white, Role::king}));
    EXPECT_EQ(pos.piece_at(parse_square("d8")), (Piece{Color::black, Role::queen}));
    EXPECT_FALSE(pos.piece_at(parse_square("e4")).has_value());
    EXPECT_FALSE(pos.piece_at(kNoSquare).has_value());
    EXPECT_EQ(pos.king_square(Color::white), parse_square("e1"));
    EXPECT_EQ(pos.king_square(Color::black), parse_square("e8"));
    EXPECT_EQ(pos.count(Color::white, Role::pawn), 8);
    EXPECT_EQ(pos.count(Color::black, Role::knight), 2);
    EXPECT_EQ(pos.count(Color::black, Role::king), 1);
    EXPECT_EQ(pos.turn(), Color::white);
    EXPECT_EQ(pos.halfmove_clock(), 0);
    EXPECT_EQ(pos.fullmove_number(), 1);
}

TEST(ChessPosition, LegalMovesFromAndClocks)
{
    Position pos = Position::start();
    MoveList moves;
    pos.legal_moves_from(parse_square("g1"), moves);
    EXPECT_EQ(moves.size(), 2u);
    pos.legal_moves_from(parse_square("e1"), moves);
    EXPECT_TRUE(moves.empty());
    pos.legal_moves_from(parse_square("e7"), moves); // not our piece
    EXPECT_TRUE(moves.empty());

    pos = pos.after(mv("g1f3", pos));
    EXPECT_EQ(pos.halfmove_clock(), 1);
    EXPECT_EQ(pos.fullmove_number(), 1);
    EXPECT_EQ(pos.turn(), Color::black);
    pos = pos.after(mv("e7e5", pos));
    EXPECT_EQ(pos.halfmove_clock(), 0);
    EXPECT_EQ(pos.fullmove_number(), 2);
    EXPECT_EQ(pos.fen(), "rnbqkbnr/pppp1ppp/8/4p3/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 0 2");
}

TEST(ChessPosition, CastlingDetailsAndRights)
{
    Position pos = load("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    const Move short_castle{parse_square("e1"), parse_square("g1"), std::nullopt};
    const Move long_castle{parse_square("e1"), parse_square("c1"), std::nullopt};
    EXPECT_TRUE(pos.is_legal(short_castle));
    EXPECT_TRUE(pos.is_legal(long_castle));
    EXPECT_TRUE(pos.is_castle(short_castle));
    EXPECT_FALSE(pos.is_capture(short_castle));
    Square rook_from = kNoSquare;
    Square rook_to = kNoSquare;
    ASSERT_TRUE(pos.castle_rook_squares(long_castle, &rook_from, &rook_to));
    EXPECT_EQ(rook_from, parse_square("a1"));
    EXPECT_EQ(rook_to, parse_square("d1"));
    EXPECT_FALSE(pos.castle_rook_squares(Move{parse_square("e1"), parse_square("f1"), {}},
                                         &rook_from, &rook_to));

    const Position after = pos.after(short_castle);
    EXPECT_EQ(after.fen(), "r3k2r/8/8/8/8/8/8/R4RK1 b kq - 1 1");

    // Rook move drops only that side; capturing a rook drops the victim's right.
    pos = pos.after(mv("a1a8", pos));
    EXPECT_EQ(pos.fen(), "R3k2r/8/8/8/8/8/8/4K2R b Kk - 0 1");

    // Cannot castle through or out of check.
    const Position through = load("r3k2r/8/8/8/8/8/5r2/R3K2R w KQkq - 0 1");
    EXPECT_FALSE(through.is_legal(short_castle));
    EXPECT_TRUE(through.is_legal(long_castle));
    const Position checked = load("r3k2r/8/8/8/8/8/4r3/R3K2R w KQkq - 0 1");
    EXPECT_FALSE(checked.is_legal(short_castle));
    EXPECT_FALSE(checked.is_legal(long_castle));
    // b1 attacked does not prevent long castling; b1 occupied does.
    EXPECT_TRUE(load("r3k2r/8/8/8/8/8/1r6/R3K2R w KQkq - 0 1").is_legal(long_castle));
    EXPECT_FALSE(load("r3k2r/8/8/8/8/8/8/RN2K2R w KQkq - 0 1").is_legal(long_castle));
}

TEST(ChessPosition, Promotions)
{
    const Position pos = load("1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    MoveList moves;
    pos.legal_moves_from(parse_square("a7"), moves);
    EXPECT_EQ(moves.size(), 8u); // a8 and axb8, four promotions each
    const Move promo{parse_square("a7"), parse_square("b8"), Role::knight};
    EXPECT_TRUE(pos.is_legal(promo));
    EXPECT_TRUE(pos.is_capture(promo));
    EXPECT_FALSE(pos.is_legal(Move{parse_square("a7"), parse_square("a8"), std::nullopt}));
    EXPECT_EQ(pos.after(promo).piece_at(parse_square("b8")), (Piece{Color::white, Role::knight}));
}

TEST(ChessPosition, CheckmateAndStalemate)
{
    Position pos = Position::start();
    for (const char *uci : {"f2f3", "e7e5", "g2g4", "d8h4"})
    {
        pos = pos.after(mv(uci, pos));
    }
    EXPECT_TRUE(pos.in_check());
    EXPECT_TRUE(pos.is_checkmate());
    EXPECT_FALSE(pos.is_stalemate());

    const Position stalemate = load("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    EXPECT_FALSE(stalemate.in_check());
    EXPECT_TRUE(stalemate.is_stalemate());
    EXPECT_FALSE(stalemate.is_checkmate());
    EXPECT_FALSE(Position::start().is_checkmate());
    EXPECT_FALSE(Position::start().is_stalemate());
}

TEST(ChessPosition, InsufficientMaterial)
{
    EXPECT_TRUE(load("4k3/8/8/8/8/8/8/4K3 w - - 0 1").insufficient_material());
    EXPECT_TRUE(load("4k3/8/8/8/8/8/8/4KN2 w - - 0 1").insufficient_material());
    EXPECT_TRUE(load("4k3/8/8/8/8/8/8/4KB2 w - - 0 1").insufficient_material());
    EXPECT_TRUE(load("2b1k3/8/8/8/8/8/8/4KB2 w - - 0 1").insufficient_material()); // same colour
    EXPECT_FALSE(load("4kb2/8/8/8/8/8/8/4KB2 w - - 0 1").insufficient_material());
    EXPECT_TRUE(load("4k3/8/8/8/8/8/8/B1B1K3 w - - 0 1").insufficient_material());
    EXPECT_FALSE(load("4k3/8/8/8/8/8/8/4KBB1 w - - 0 1").insufficient_material());
    EXPECT_FALSE(load("4kb2/8/8/8/8/8/8/3BK3 w - - 0 1").insufficient_material()); // opposite
    EXPECT_FALSE(load("4kn2/8/8/8/8/8/8/4KN2 w - - 0 1").insufficient_material());
    EXPECT_FALSE(load("4k3/8/8/8/8/8/8/4KNN1 w - - 0 1").insufficient_material());
    EXPECT_FALSE(load("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1").insufficient_material());
    EXPECT_FALSE(load("4k3/8/8/8/8/8/8/4K2R w - - 0 1").insufficient_material());
    EXPECT_FALSE(Position::start().insufficient_material());
    // K+N vs K+Q: the knight side could still be mated, so the queen side can win.
    EXPECT_FALSE(load("4kq2/8/8/8/8/8/8/4KN2 w - - 0 1").insufficient_material());
    EXPECT_TRUE(load("4kq2/8/8/8/8/8/8/4KN2 w - - 0 1").has_insufficient_material(Color::white));
}

TEST(ChessPosition, HashTracksStateAndTransposes)
{
    const Position start = Position::start();
    Position a = start;
    for (const char *uci : {"g1f3", "g8f6", "b1c3"})
    {
        a = a.after(mv(uci, a));
    }
    Position b = start;
    for (const char *uci : {"b1c3", "g8f6", "g1f3"})
    {
        b = b.after(mv(uci, b));
    }
    EXPECT_EQ(a.hash(), b.hash());
    EXPECT_NE(a.hash(), start.hash());
    EXPECT_NE(load("4k3/8/8/8/8/8/8/4K2R w K - 0 1").hash(),
              load("4k3/8/8/8/8/8/8/4K2R w - - 0 1").hash());
    EXPECT_NE(load("4k3/8/8/8/8/8/8/4K2R w - - 0 1").hash(),
              load("4k3/8/8/8/8/8/8/4K2R b - - 0 1").hash());
    // A legal en passant square affects the hash, an irrelevant one does not.
    EXPECT_NE(load("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1").hash(),
              load("4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1").hash());
    EXPECT_EQ(load("4k3/8/8/3p4/4P3/8/8/4K3 w - d6 0 1").hash(),
              load("4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1").hash());
}

} // namespace
