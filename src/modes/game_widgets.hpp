// ProsperoLichess - What board screens share: player panels with clocks and the move table.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/chrome.hpp"
#include "app/scene.hpp"
#include "chess/chess.hpp"
#include "ui/components/badge.hpp"
#include "ui/components/surface.hpp"
#include "ui/components/table.hpp"

#include <string>
#include <vector>

namespace pch::modes
{

// A player's strip beside the board: who, how strong, what they took, how
// long they have. The game screen, the TV screen and the puzzle screen place
// one above and one below their side column.
//
//   PlayerPanel white;
//   white.set_bounds({app::kBoardColumn, 828, app::kRight - app::kBoardColumn, 132});
//   white.set_player("blackbear", "1834", "", chess::Color::white);
//   white.set_note("White \xC2\xB7 your move");
//   white.set_clock(307000, true, true);
//   ...
//   white.update(ctx, dt);
//   white.draw(ctx, frame.scene);
class PlayerPanel
{
  public:
    PlayerPanel();

    void set_bounds(const gfx::Rect &bounds);
    const gfx::Rect &bounds() const
    {
        return bounds_;
    }
    // rating is text as it should read ("1834", "1500?"); title is "GM",
    // "BOT" or empty; color says whose pieces these are.
    void set_player(const std::string &name, const std::string &rating, const std::string &title,
                    chess::Color color);
    // The line under the name: "Black", "White - your move", "Stockfish level 3".
    void set_note(const std::string &note);
    // The pieces this player has captured, most valuable first.
    void set_captured(std::vector<chess::Piece> pieces);
    // milliseconds < 0 or has_clock false hides the clock. active: this
    // player's clock is the one running. Under low_ms an active clock turns
    // to the danger colour.
    void set_clock(long long milliseconds, bool has_clock, bool active, long long low_ms = 20000);
    // A dot on the avatar: is the player connected to the game?
    void set_presence(ui::Presence presence);
    // The colour the panel is lit in while this player's clock runs (the
    // screen's accent; the theme's primary until set).
    void set_accent(gfx::Color accent);

    void update(app::Context &ctx, float dt);
    void draw(const app::Context &ctx, gfx::DrawList &list) const;

  private:
    gfx::Rect clock_box() const;

    gfx::Rect bounds_{};
    ui::Avatar avatar_;
    gfx::Color accent_ = app::look::accent(app::look::Section::game);
    std::string title_;
    std::string name_;
    std::string rating_;
    std::string note_;
    chess::Color color_ = chess::Color::white;
    std::vector<chess::Piece> captured_;
    long long clock_ms_ = -1;
    long long clock_peak_ms_ = 0; // the most this clock has shown: the level's full mark
    bool has_clock_ = false;
    bool active_ = false;
    bool low_ = false;
    tween::Spring active_amount_;
};

// The moves of a game as a table (number, White, Black) with the shown move
// marked. It scrolls to keep that move in view; it takes no input.
class MoveTable
{
  public:
    MoveTable();
    // The table's cells draw through this object: it must stay where it is.
    MoveTable(const MoveTable &) = delete;
    MoveTable &operator=(const MoveTable &) = delete;

    void set_bounds(const gfx::Rect &bounds);
    const gfx::Rect &bounds() const
    {
        return table_.bounds();
    }
    // sans: one move per ply, in standard notation. shown: the ply the board
    // shows (0 is before the first move, sans.size() is after the last).
    // first_number and black_first describe where the game started (a puzzle
    // or a game from a position may start on move 23 with Black to play).
    void set_moves(const std::vector<std::string> &sans, int shown, int first_number = 1,
                   bool black_first = false);
    // The colour of the mark on the shown move (the screen's accent).
    void set_accent(gfx::Color accent);

    void update(app::Context &ctx, float dt);
    void draw(const app::Context &ctx, gfx::DrawList &list) const;

  private:
    ui::Table table_;
    std::vector<std::string> sans_;
    int shown_ = -1;
    int first_number_ = 1;
    bool black_first_ = false;
};

// The pieces `taker` has captured in this position (what the other side is
// missing from a full set), most valuable first.
std::vector<chess::Piece> captured_by(const chess::Position &position, chess::Color taker);

// "1834", or "1500?" for a provisional rating; empty for 0.
std::string rating_text(int rating, bool provisional = false);

} // namespace pch::modes
