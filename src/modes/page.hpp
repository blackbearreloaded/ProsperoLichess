// ProsperoLichess - A page of the home screen: what the side rail switches between.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/chrome.hpp"
#include "app/scene.hpp"

#include <memory>
#include <span>

namespace pch::modes
{

// What a page asks for after an update.
struct PageResult
{
    app::Transition transition; // open a screen on top of the home screen
    bool to_rail = false;       // hand the controller back to the side rail
    int go_to = -1;             // show another page (an index into the rail), with the focus in it
};

// One page beside the rail. It lays out in the area right of app::kContent,
// between app::kTop and app::kBottom, and owns its components.
class Page
{
  public:
    virtual ~Page() = default;
    // The page became the visible one, or the home screen came back to the
    // front with it showing. Refresh what it shows.
    virtual void enter(app::Context &)
    {
    }
    // focused: the page has the controller; otherwise the rail has it and
    // input is empty. Animations advance either way.
    virtual PageResult update(app::Context &ctx, const InputFrame &input, float dt,
                              bool focused) = 0;
    // Draws into frame.scene; dialogs go to frame.overlay (set frame.glass).
    virtual void draw(app::Context &ctx, app::Frame &frame, bool focused) const = 0;
    // While another screen covers the home screen (keep streams alive).
    virtual void tick(app::Context &, float)
    {
    }
    // The page's name, shown in the status strip (pages do not repeat it).
    virtual const char *title() const = 0;
    // Which part of the app this is: its accent and its sky.
    virtual app::look::Section section() const
    {
        return app::look::Section::home;
    }
    // The sky right now; a page whose focus moves over things of different
    // kinds returns the mood of the focused one.
    virtual app::look::Mood mood() const
    {
        return app::look::mood(section());
    }
    // True when L1 and R1 turn tabs on this page: the rail then passes them on.
    virtual bool tabbed() const
    {
        return false;
    }
    // The hint row while the page has the controller.
    virtual std::span<const ui::Hint> hints() const = 0;
    virtual const char *name() const = 0;
};

// The rail's entries, in order.
enum PageIndex : int
{
    kPageHome,
    kPagePuzzles,
    kPagePlay,
    kPageWatch,
    kPageProfile,
    kPageSettings,
    kPageCount,
};

std::unique_ptr<Page> make_home_page(app::Context &ctx);
std::unique_ptr<Page> make_puzzles_page(app::Context &ctx);
std::unique_ptr<Page> make_play_page(app::Context &ctx);
std::unique_ptr<Page> make_watch_page(app::Context &ctx);
std::unique_ptr<Page> make_profile_page(app::Context &ctx);
std::unique_ptr<Page> make_settings_page(app::Context &ctx);

} // namespace pch::modes
