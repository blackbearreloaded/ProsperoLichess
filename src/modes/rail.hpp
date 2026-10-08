// ProsperoLichess - The rail: the app's mark and the menu down the left of the home screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A bespoke menu in the house look (app/look.hpp), not the kit's SideNav:
//
//   - it runs the height of the screen and opens with the app's mark;
//   - every entry has a sign on a tile in the colour of the part it opens;
//     the entry that is showing is a lit plate in that colour, and the plate
//     glides when the page changes;
//   - while the rail has the controller a ring glides between entries, the
//     focused one leans out, and an edge answers with a nudge;
//   - Watch carries a pulsing mark while a game is on air.

#pragma once

#include "app/chrome.hpp"
#include "app/scene.hpp"
#include "ui/motion.hpp"

#include <string>
#include <vector>

namespace pch::modes
{

struct RailEntry
{
    std::string label; // in the player's language; a long one is drawn smaller, as all then are
    app::NavIcon icon = app::NavIcon::home;
    app::look::Section section = app::look::Section::home;
    bool live = false; // a pulsing "LIVE" mark at the row's end
};

// A small card in the rail's free space, above its last entry: what the
// player should know wherever they are (a game waits for a move, the app is
// offline). It takes no focus. An empty title hides it. The kicker and the
// title are one line each; a long body takes two.
struct RailNote
{
    std::string kicker;
    std::string title;
    std::string body;
    app::look::Section section = app::look::Section::home;
};

//   Rail rail;
//   // The last entry is pinned low.
//   rail.set_entries({{tr("Home"), app::NavIcon::home, Section::home}, ...});
//   rail.set_bounds(app::kRail);
//   ...
//   switch (rail.handle(input, ctx)) { case ui::Event::moved: show(rail.focus()); ... }
//   rail.update(ctx, dt);
//   rail.draw(ctx, frame.scene);
class Rail
{
  public:
    void set_entries(std::vector<RailEntry> entries);
    RailEntry &entry(int index)
    {
        return entries_[static_cast<std::size_t>(index)];
    }
    void set_bounds(const gfx::Rect &bounds);
    void set_note(RailNote note);
    // The entry whose page is showing.
    void set_current(int index, bool snap = false);
    void set_focus(int index);
    int focus() const
    {
        return focus_;
    }
    // Whether the rail has the controller (the ring shows only then).
    void set_focused(bool focused)
    {
        focused_ = focused;
    }

    // moved: the focus went to another entry. activated: confirm.
    // cancelled: back. Plays its own cues.
    ui::Event handle(const InputFrame &input, app::Context &ctx);
    void update(app::Context &ctx, float dt);
    void draw(const app::Context &ctx, gfx::DrawList &list) const;

  private:
    gfx::Rect row(int index) const;

    gfx::Rect bounds_{};
    std::vector<RailEntry> entries_;
    RailNote note_;
    tween::Spring note_in_;           // 0..1: the note is there
    std::vector<tween::Spring> lit_;  // per entry, 0..1: its page is showing
    std::vector<tween::Spring> lean_; // per entry, 0..1: it has the focus
    int current_ = 0;
    int focus_ = 0;
    bool focused_ = false;
    bool placed_ = false;       // the plate and the ring have been put somewhere
    ui::SpringRect plate_;      // the lit plate under the current entry
    ui::SpringColor plate_ink_; // ... in that entry's colour
    ui::SpringRect ring_;       // the focus ring
    tween::Spring ring_alpha_;
    ui::Pulse refusal_; // an edge was pushed
    ui::Pulse press_;
    float age_ = 0.0f; // seconds since it first appeared: entries arrive by it
    bool calm_ = false;
};

} // namespace pch::modes
