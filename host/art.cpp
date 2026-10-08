// ProsperoLichess - System presentation art (icon0, pic0/pic1) drawn with the app renderer.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "art.hpp"

#include "board/board_view.hpp"
#include "chess/chess.hpp"
#include "board/mini_board.hpp"
#include "ui/fonts.hpp"

namespace pch::host
{

namespace
{

using gfx::Color;

void backdrop(gfx::DrawList &list)
{
    list.gradient_rect({0, 0, 1920, 1080}, 0, Color::rgb(0x0c1330), Color::rgb(0x241444));
    list.shadow({1100, 120, 760, 760}, 380, 260, Color::rgb(0xd8a657, 0.22f));
    list.shadow({-200, 600, 900, 700}, 350, 300, Color::rgb(0x6a5cff, 0.18f));
}

} // namespace

bool render_art(gfx::DrawList &list, const ui::Fonts &fonts, const board::PieceAtlas &pieces,
                const std::function<bool(const char *)> &write)
{
    const board::BoardTheme &walnut = board::kBoardThemes[0];

    // Icon: the centred 1080x1080 square. A knight and king on a wooden board.
    list.clear();
    backdrop(list);
    const float left = 420.0f;
    list.shadow({left + 100, 640, 880, 420}, 60, 80, Color::rgb(0x000000, 0.6f));
    list.board({left + 60, 610, 960, 960}, Color::rgb(walnut.light), Color::rgb(walnut.dark),
               walnut.style, 3.0f);
    list.gradient_rect({left, 560, 1080, 520}, 0, Color::rgb(0x0c1330, 0.0f),
                       Color::rgb(0x0c1330, 0.55f));
    list.shadow({left + 250, 860, 600, 120}, 60, 60, Color::rgb(0x000000, 0.55f));
    pieces.draw(list, {chess::Color::black, chess::Role::king}, {left + 520, 170, 520, 520},
                Color{0.92f, 0.92f, 0.95f, 1});
    pieces.draw(list, {chess::Color::white, chess::Role::knight}, {left + 70, 230, 640, 640});
    ui::text(list, fonts.semibold, "ProsperoLichess", 960, 1010, 104, Color::rgb(0xf5f3ff),
             gfx::Align::center);
    bool ok = write("art-icon");

    // Background (pic0 / pic1): wordmark on the left, a live-looking board on the right.
    list.clear();
    backdrop(list);
    chess::Position position;
    chess::Position::from_fen(
        "r1bq1rk1/ppp2ppp/2np1n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w - - 0 7", &position);
    const gfx::Rect board{1110, 170, 720, 720};
    board::draw_mini_board(list, pieces, walnut, board, position, chess::Color::white,
                           {chess::make_square(6, 0), chess::make_square(5, 2)}, 14.0f);
    pieces.draw(list, {chess::Color::white, chess::Role::knight}, {120, 250, 220, 220});
    ui::text(list, fonts.semibold, "ProsperoLichess", 120, 600, 98, Color::rgb(0xf5f3ff));
    ui::text(list, fonts.regular, "Chess on lichess.org, native on PS5", 128, 690, 48,
             Color::rgb(0xd8a657));
    ui::text(list, fonts.regular, "Puzzles  \xC2\xB7  Online play  \xC2\xB7  Lichess TV", 128, 770,
             40, Color::rgb(0xa9a8c8));
    ok = write("art-background") && ok;
    return ok;
}

} // namespace pch::host
