// ProsperoLichess - Animated chess board renderer (chessground-style).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "board/pieces.hpp"
#include "chess/chess.hpp"
#include "core/strings.hpp"
#include "core/tween.hpp"
#include "gfx/draw_list.hpp"
#include "ui/fonts.hpp"

#include <array>
#include <vector>

namespace pch::board
{

struct BoardTheme
{
    const char *label; // English, marked TR("..."): the settings translate it
    std::uint32_t light;
    std::uint32_t dark;
    gfx::BoardStyle style;
    std::uint32_t frame_top;
    std::uint32_t frame_bottom;
};

inline constexpr BoardTheme kBoardThemes[] = {
    {TR("Walnut"), 0xf0d9b5, 0xb58863, gfx::BoardStyle::wood, 0x5a3a22, 0x3a2414},
    {TR("Maple"), 0xf3dfb8, 0xc99a63, gfx::BoardStyle::wood, 0x7a5230, 0x4f331c},
    {TR("Marble"), 0xe9e6df, 0x9d9a94, gfx::BoardStyle::marble, 0x4a4d57, 0x2b2d35},
    {TR("Classic"), 0xf0d9b5, 0xb58863, gfx::BoardStyle::flat, 0x4a3524, 0x2c1f15},
    {TR("Ocean"), 0xdee3e6, 0x8ca2ad, gfx::BoardStyle::flat, 0x34414c, 0x1f2830},
    {TR("Meadow"), 0xffffdd, 0x86a666, gfx::BoardStyle::flat, 0x3d4d2d, 0x26321b},
    {TR("Twilight"), 0xb9b2d0, 0x6f64a0, gfx::BoardStyle::marble, 0x3a3358, 0x221d38},
};
inline constexpr int kBoardThemeCount =
    static_cast<int>(sizeof(kBoardThemes) / sizeof(kBoardThemes[0]));

struct Arrow
{
    chess::Square from = chess::kNoSquare;
    chess::Square to = chess::kNoSquare;
    gfx::Color color = gfx::Color::rgb(0x15781b, 0.8f);
};

// Everything besides the pieces that the board shows this frame.
struct Overlay
{
    chess::Move last_move;
    chess::Square selected = chess::kNoSquare;
    std::vector<chess::Square> dests; // legal targets of the selected piece
    chess::Move premove;
    chess::Square cursor = chess::kNoSquare;
    std::vector<Arrow> arrows;
    chess::Square good = chess::kNoSquare; // puzzle feedback flashes
    chess::Square bad = chess::kNoSquare;
    float feedback = 0.0f; // 1 fresh .. 0 gone
    bool coordinates = true;
    bool show_dests = true;
    bool blindfold = false;
    // The controller cursor and the promotion picker's focus (the theme's primary).
    gfx::Color cursor_color = gfx::Color::rgb(0x76d6ff);
};

class BoardView
{
  public:
    BoardView();

    // Shows a position at once (no animation).
    void snap(const chess::Position &position);
    // Animates move from before to after (castling moves both pieces,
    // captures fade out). speed 1 = normal; 0 snaps.
    void play(const chess::Position &before, const chess::Move &move, const chess::Position &after,
              float speed = 1.0f);
    const chess::Position &position() const
    {
        return position_;
    }

    void set_orientation(chess::Color side, bool animate);
    chess::Color orientation() const
    {
        return orientation_;
    }
    // Nudges the piece on a square side to side (a refused move).
    void shake(chess::Square square);
    // Deals the pieces onto the board one after another (a new game or puzzle).
    void deal();
    bool animating() const
    {
        return move_timer_.running;
    }

    void update(float dt, const Overlay &overlay);
    void draw(gfx::DrawList &list, const ui::Fonts &fonts, const PieceAtlas &atlas,
              const gfx::Rect &area, const Overlay &overlay, const BoardTheme &theme,
              float time) const;

    // Screen rectangle of a square inside the board area (orientation aware).
    gfx::Rect square_rect(const gfx::Rect &area, chess::Square square) const;

  private:
    struct Sprite
    {
        chess::Piece piece{};
        chess::Square from = chess::kNoSquare;
        chess::Square to = chess::kNoSquare;
    };

    chess::Position position_;
    chess::Color orientation_ = chess::Color::white;
    std::array<Sprite, 2> moving_{}; // the piece (and a castling rook)
    int moving_count_ = 0;
    Sprite captured_{};
    bool has_captured_ = false;
    tween::Timer move_timer_;
    tween::Timer flip_timer_;
    tween::Timer shake_timer_;
    chess::Square shake_square_ = chess::kNoSquare;
    tween::Spring cursor_x_;
    tween::Spring cursor_y_;
    tween::Spring lift_;
    tween::Spring cursor_alpha_;
    chess::Square last_selected_ = chess::kNoSquare;
    bool cursor_placed_ = false;

    // Short-lived effects, in board units (squares, origin at the top-left).
    struct Particle
    {
        float x, y, vx, vy;
        float age, life, size;
        gfx::Color color;
    };
    struct Ripple
    {
        float x, y;
        float age, life, reach; // reach in squares
        gfx::Color color;
    };
    void burst(chess::Square square, gfx::Color color, int count, float speed);
    void ripple(chess::Square square, gfx::Color color, float reach, float life);
    float random01();

    std::vector<Particle> particles_;
    std::vector<Ripple> ripples_;
    tween::Timer deal_timer_;
    std::uint32_t rng_ = 0x9e3779b9u;
};

} // namespace pch::board
