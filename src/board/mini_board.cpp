// ProsperoLichess - A small, still board: thumbnails and previews.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "board/mini_board.hpp"

#include <algorithm>

namespace pch::board
{

void draw_mini_board(gfx::DrawList &list, const PieceAtlas &atlas, const BoardTheme &theme,
                     const gfx::Rect &squares, const chess::Position &position,
                     chess::Color orientation, const chess::Move &last_move, float frame)
{
    using gfx::Color;
    if (frame < 0.0f)
        frame = std::clamp(squares.w * 0.028f, 4.0f, 12.0f);
    const gfx::Rect outer = squares.inset(-frame);
    const float corner = std::min(frame, 10.0f);
    list.shadow({outer.x, outer.y + frame, outer.w, outer.h}, corner, frame * 3.0f,
                Color::rgb(0x000000, 0.45f));
    list.gradient_rect(outer, corner, Color::rgb(theme.frame_top), Color::rgb(theme.frame_bottom));
    list.bordered_rect(outer, corner, Color::rgb(0xffffff, 0.0f), 1.5f,
                       Color::rgb(0xffffff, 0.14f));
    // Grain finer than a pixel would only shimmer: small boards are flat.
    list.board(squares, Color::rgb(theme.light), Color::rgb(theme.dark),
               squares.w < 360.0f ? gfx::BoardStyle::flat : theme.style,
               static_cast<float>(theme.light % 97));

    const float size = squares.w / 8.0f;
    const auto rect_of = [&](chess::Square square)
    {
        const int file = chess::file_of(square);
        const int rank = chess::rank_of(square);
        const int col = orientation == chess::Color::white ? file : 7 - file;
        const int row = orientation == chess::Color::white ? 7 - rank : rank;
        return gfx::Rect{squares.x + static_cast<float>(col) * size,
                         squares.y + static_cast<float>(row) * size, size, size};
    };
    if (last_move.valid())
    {
        list.rounded_rect(rect_of(last_move.from), 0, Color::rgb(0x9bc700, 0.41f));
        list.rounded_rect(rect_of(last_move.to), 0, Color::rgb(0x9bc700, 0.41f));
    }
    for (chess::Square square = 0; square < 64; ++square)
    {
        if (const auto piece = position.piece_at(square))
            atlas.draw(list, *piece, rect_of(square));
    }
}

} // namespace pch::board
