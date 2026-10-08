// ProsperoLichess - The furniture every screen shares: layout grid, status bar, hints, icons.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/scene.hpp"
#include "ui/components/badge.hpp"
#include "ui/components/notification.hpp"
#include "ui/components/notification_bell.hpp"
#include "ui/components/progress.hpp"
#include "ui/components/status.hpp"
#include "ui/glyphs.hpp"
#include "ui/widgets.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace pch::app
{

// ---- the layout grid (virtual pixels, 1920 x 1080) --------------------------

inline constexpr float kMargin = 96.0f;                        // the safe area's sides
inline constexpr float kRight = gfx::kVirtualWidth - kMargin;  // its right edge
inline constexpr float kTop = 132.0f;                          // under the status bar
inline constexpr float kBottom = 952.0f;                       // over the hint row
inline constexpr float kGap = 24.0f;                           // between panels
inline constexpr float kNavWidth = 248.0f;                     // the side rail
inline constexpr float kContent = kMargin + kNavWidth + 32.0f; // a page's left edge beside it
inline constexpr gfx::Rect kBar{kMargin, 48.0f, kRight - kMargin, 60.0f};
// The home screen's menu runs from the top of the bar to the bottom of the
// pages; beside it the strip starts at the pages' left edge.
inline constexpr gfx::Rect kRail{kMargin, kBar.y, kNavWidth, kBottom - kBar.y};
inline constexpr gfx::Rect kStrip{kContent, kBar.y, kRight - kContent, kBar.h};
// Board screens: the squares, and the column beside them.
inline constexpr float kStage = 124.0f;
inline constexpr gfx::Rect kBoardSquares{108.0f, kStage + 12.0f, 860.0f, 860.0f};
inline constexpr float kBoardColumn = 1008.0f;

// A canvas for components drawing into list (glass: the frame's blurred copy).
inline ui::Canvas canvas_for(const Context &ctx, gfx::DrawList &list, std::uint32_t glass = 0)
{
    return ui::Canvas{list, *ctx.fonts, glass, ctx.time};
}

// A small tracked label in capitals ("DAILY PUZZLE").
void kicker(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
            float baseline, gfx::Color color);

// The row of controller hints at the bottom right. over_scrim: the row sits on
// a dimmed screen (a dialog is open). The row is drawn smaller when its
// labels, in a longer language, would make it wider than max_width (the safe
// area's width unless a screen leaves it less).
void draw_hints(const Context &ctx, gfx::DrawList &list, const ui::Hint *hints, int count,
                bool over_scrim = false, float max_width = kRight - kMargin);

// A check mark or a cross on a disc.
void draw_verdict(gfx::DrawList &list, float cx, float cy, float radius, bool good, gfx::Color fill,
                  gfx::Color ink);

enum class NavIcon : int
{
    home,
    puzzles,
    play,
    watch,
    profile,
    settings,
};
// A line icon for the side rail, drawn from shapes in one colour.
void draw_nav_icon(gfx::DrawList &list, NavIcon icon, const gfx::Rect &box, gfx::Color ink);

// "5:07", "1:02:45" or "0:09.4" under ten seconds; "--" for no clock.
std::string format_clock(long long milliseconds);

// The strip along the top of every screen: the wordmark, the connection, how
// many games wait for a move (a bell, with the Touchpad's glyph where that
// button opens them), and who is signed in. Connection trouble and short
// messages appear as notifications anchored under it.
class StatusChrome
{
  public:
    StatusChrome();

    // A short message that leaves by itself.
    void notify(const std::string &text, Note kind);
    // A floating card under the bar's right end that stays for `seconds`.
    void announce(const std::string &title, const std::string &body, float seconds);
    bool announcing() const
    {
        return !floating_.empty();
    }

    // What the top screen shows (Scene::place): with a title the strip starts
    // beside the rail and names the place, without one it spans the screen
    // and carries the app's name.
    void set_place(const Place &place);
    void update(Context &ctx, float dt);
    // The bar itself, into the screen's own layer.
    void draw_bar(const Context &ctx, gfx::DrawList &list) const;
    // The notifications, above everything (glass: the frame's blurred copy).
    void draw_notices(const Context &ctx, gfx::DrawList &list, std::uint32_t glass) const;
    // True while a notification is on screen (the frame then needs its glass).
    bool has_notices() const
    {
        return !notices_.empty() || !floating_.empty();
    }

  private:
    // Sizes and places the notices for the lines they hold.
    void fit_notices(const Context &ctx);

    gfx::Rect strip_ = kBar;
    std::string place_; // empty: the app's name
    int place_icon_ = -1;
    tween::Spring place_in_; // 0 -> 1 as a new place's name settles
    ui::Chip state_;
    ui::NotificationBell bell_;
    ui::Avatar avatar_;
    ui::Spinner spinner_;
    ui::NotificationStack notices_;
    std::vector<int> notice_ids_; // the notices showing or waiting
    ui::ToastStack floating_;
    int issue_id_ = 0;       // the connection notice on screen, or 0
    std::string issue_text_; // ... and what it says
    bool issue_busy_ = false;
    bool issue_warning_ = false;
    bool answers_bell_ = false; // the Touchpad opens the games the bell counts here
    float touch_x_ = 0.0f;      // ... and its glyph's left edge, beside the bell
    bool signed_in_ = false;
    bool online_ = false;
    bool trouble_ = false;
    std::string name_;
    std::string rating_;
    float name_right_ = 0.0f; // right edges of the name and the rating in the bar
    float rating_right_ = 0.0f;
    gfx::DrawList scratch_; // for measuring text outside a draw
};

} // namespace pch::app
