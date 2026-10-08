// ProsperoLichess - A small, still board: thumbnails and previews.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "board/board_view.hpp"
#include "board/pieces.hpp"
#include "chess/chess.hpp"
#include "gfx/draw_list.hpp"

namespace pch::board
{

// Draws a position without animation. `squares` is the 8 x 8 area; the frame
// grows out of it by `frame` pixels (negative: sized from the board).
void draw_mini_board(gfx::DrawList &list, const PieceAtlas &atlas, const BoardTheme &theme,
                     const gfx::Rect &squares, const chess::Position &position,
                     chess::Color orientation = chess::Color::white,
                     const chess::Move &last_move = {}, float frame = -1.0f);

} // namespace pch::board
