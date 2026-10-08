// ProsperoLichess - Controller input on the board: cursor, selection, premoves, promotion.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/cues.hpp"
#include "ui/feedback.hpp"
#include "board/board_view.hpp"
#include "chess/chess.hpp"
#include "core/input.hpp"
#include "core/tween.hpp"

#include <vector>

namespace pch::board
{

// Who may move pieces this frame.
struct InputRules
{
    bool white = true;    // the player controls White
    bool black = true;    // the player controls Black
    bool premoves = true; // allow queuing a move on the opponent's turn
    bool auto_queen = true;
    bool locked = false; // no moves at all (animations, finished games)
};

class BoardInput
{
  public:
    enum class Result
    {
        none,
        move,    // *move holds a complete move (promotion chosen)
        premove, // a premove was queued (read premove())
        back,    // Circle with nothing to cancel: the scene handles it
    };

    // Puts the cursor on a square (for example the side to move's king).
    void place_cursor(chess::Square square);
    chess::Square cursor() const
    {
        return cursor_;
    }
    void clear_selection();
    // Drops the queued premove (the opponent's reply made it illegal, or Circle).
    void clear_premove()
    {
        premove_ = {};
    }
    const chess::Move &premove() const
    {
        return premove_;
    }
    // Returns the queued premove if it is legal now, clearing it either way.
    bool take_premove(const chess::Position &position, chess::Move *move);

    Result update(const InputFrame &input, float dt, const chess::Position &position,
                  chess::Color orientation, const InputRules &rules, ui::Feedback &feedback,
                  chess::Move *move);

    // Selection, destinations, premove and cursor for the renderer.
    void fill_overlay(Overlay &overlay) const;

    bool promotion_open() const
    {
        return promotion_.valid();
    }
    // The promotion picker drawn over the board (Lichess style column).
    void draw_promotion(gfx::DrawList &list, const PieceAtlas &atlas, const BoardView &view,
                        const gfx::Rect &area, const chess::Position &position,
                        gfx::Color focus = gfx::Color::rgb(0x76d6ff)) const;

  private:
    // Legal moves for the piece on from (or premove targets when it is not
    // this side's turn).
    void targets(const chess::Position &position, chess::Square from, bool premove,
                 std::vector<chess::Square> *out, std::vector<chess::Move> *moves) const;
    bool can_control(const chess::Position &position, chess::Square square, const InputRules &rules,
                     bool *premove) const;
    void cycle(const chess::Position &position, chess::Color orientation, const InputRules &rules,
               int step, ui::Feedback &feedback);

    chess::Square cursor_ = chess::make_square(4, 1);
    chess::Square selected_ = chess::kNoSquare;
    bool selected_is_premove_ = false;
    std::vector<chess::Square> dests_;
    std::vector<chess::Move> moves_;
    chess::Move premove_;
    chess::Move promotion_; // pending promotion move (piece chosen in the picker)
    int promotion_choice_ = 0;
    tween::Spring picker_;
};

} // namespace pch::board
