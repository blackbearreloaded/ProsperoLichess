// ProsperoLichess - Host snapshots: the Play page.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_file.hpp"
#include "lichess/session.hpp"
#include "modes/page.hpp"
#include "modes/scenes.hpp"
#include "scenarios.hpp"

namespace pch::host
{

namespace
{

// The Play page with the controller in it, on a tab (0 Online, 1 Computer, 2 Friend).
void open_tab(Run &run, int tab)
{
    run.page(modes::kPagePlay);
    for (int i = 0; i < tab; ++i)
        run.press(Action::page_next);
    run.idle(40);
}

// The rail moves onto Play: the page is shown on this very frame, with the
// controller still on the rail.
void show_from_rail(Run &run)
{
    run.press(Action::menu);
    run.nav(Direction::up, modes::kPageCount);
    run.nav(Direction::down, modes::kPagePlay - 1);
    InputFrame frame;
    frame.nav = Direction::down;
    frame.connected = true;
    run.app.update(frame, 1.0f / 60.0f);
}

} // namespace

void add_play_scenarios(Scenarios &all)
{
    // ---- Online ----
    all.push_back({"play", [](Run &run) { open_tab(run, 0); }, true});
    // Another time control: 30+20, a rating the account has too.
    all.push_back({"play-online-classical",
                   [](Run &run)
                   {
                       open_tab(run, 0);
                       run.nav(Direction::down);
                       run.idle(40);
                   },
                   true});
    // Correspondence, and casual chosen in the options.
    all.push_back({"play-online-casual",
                   [](Run &run)
                   {
                       open_tab(run, 0);
                       run.nav(Direction::right);
                       run.nav(Direction::down);
                       run.nav(Direction::right); // the button
                       run.nav(Direction::up);    // the last choice above it
                       run.press(Action::confirm);
                       run.idle(40);
                   },
                   true});
    // Cross on a time control leads to the button that starts the search.
    all.push_back({"play-online-find",
                   [](Run &run)
                   {
                       open_tab(run, 0);
                       run.nav(Direction::left);
                       run.press(Action::confirm);
                       run.idle(40);
                   },
                   true});
    // The tab row has the controller.
    all.push_back({"play-online-tabs",
                   [](Run &run)
                   {
                       open_tab(run, 0);
                       run.nav(Direction::up);
                       run.idle(40);
                   },
                   true});
    // The page as the rail shows it, before the controller enters it.
    all.push_back({"play-rail",
                   [](Run &run)
                   {
                       run.press(Action::menu);
                       run.nav(Direction::down, modes::kPagePlay);
                       // Long enough for the page to assemble and its rating to count.
                       run.idle(100);
                   },
                   true});

    // ---- Computer ----
    all.push_back({"play-computer", [](Run &run) { open_tab(run, 1); }, true});
    // Unlimited time, level 6, playing Black.
    all.push_back({"play-computer-setup",
                   [](Run &run)
                   {
                       open_tab(run, 1);
                       run.nav(Direction::down);
                       run.nav(Direction::right); // the button
                       run.nav(Direction::up);    // Black
                       run.press(Action::confirm);
                       run.nav(Direction::up, 3); // the level
                       run.nav(Direction::right, 3);
                       run.idle(40);
                   },
                   true});
    all.push_back({"play-computer-level",
                   [](Run &run)
                   {
                       open_tab(run, 1);
                       run.nav(Direction::right);
                       run.nav(Direction::right, 5);
                       run.idle(40);
                   },
                   true});

    // ---- Friend ----
    all.push_back({"play-friend", [](Run &run) { open_tab(run, 2); }, true});
    all.push_back({"play-friend-offline", [](Run &run) { open_tab(run, 2); }});
    // A game was left half-way: the panel shows where it stands and whose move it is.
    all.push_back(
        {"play-friend-saved", [](Run &run)
         {
             app::Context &ctx = run.app.context();
             save::write_atomic(
                 ctx.data_root + "/passplay.sav",
                 save::encode(save::Kind::game, 1, "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1"));
             open_tab(run, 2);
         }});

    // ---- motion: pictures taken while things are on their way ----
    // The page a few frames after the rail showed it: its parts are arriving.
    all.push_back({"play-arriving",
                   [](Run &run)
                   {
                       show_from_rail(run);
                       run.idle(5);
                       run.shot("early");
                       run.idle(8);
                   },
                   true});
    // Online gives way to Computer: one leaves to the left as the other comes in.
    all.push_back({"play-tab-change",
                   [](Run &run)
                   {
                       open_tab(run, 0);
                       InputFrame frame;
                       frame.pressed = action_bit(Action::page_next);
                       frame.held = frame.pressed;
                       frame.connected = true;
                       run.app.update(frame, 1.0f / 60.0f);
                       run.idle(3);
                       run.shot("early");
                       run.idle(7);
                   },
                   true});
    // Another time control: the ring on its way to it, the ticket's words and
    // colour changing, the rating counting and its line growing again. Then
    // Cross: the ring on its way across to the ticket's button.
    all.push_back({"play-ring-glide",
                   [](Run &run)
                   {
                       open_tab(run, 0);
                       InputFrame frame;
                       frame.nav = Direction::down;
                       frame.connected = true;
                       run.app.update(frame, 1.0f / 60.0f);
                       run.idle(3);
                       run.shot("words");
                       run.idle(40);
                       frame.nav = Direction::none;
                       frame.pressed = action_bit(Action::confirm);
                       frame.held = frame.pressed;
                       run.app.update(frame, 1.0f / 60.0f);
                       run.idle(3);
                   },
                   true});
    // Reduced motion: at the moment "play-arriving-early" is taken, the page is simply there.
    all.push_back({"play-calm",
                   [](Run &run)
                   {
                       run.app.context().settings->reduced_motion = true;
                       show_from_rail(run);
                       run.idle(5);
                   },
                   true});

    // ---- tabs that cannot be used ----
    all.push_back({"play-offline", [](Run &run) { open_tab(run, 0); }});
    all.push_back({"play-offline-computer", [](Run &run) { open_tab(run, 1); }});
    // Online, without an account.
    all.push_back({"play-signed-out",
                   [](Run &run)
                   {
                       run.app.session().sign_out();
                       open_tab(run, 0);
                   },
                   true});
    all.push_back({"play-signed-out-computer",
                   [](Run &run)
                   {
                       run.app.session().sign_out();
                       open_tab(run, 1);
                   },
                   true});
    // The account is being checked: nothing to press yet.
    all.push_back({"play-signing-in",
                   [](Run &run)
                   {
                       run.app.session().sign_out();
                       run.app.session().sign_in("lip_snapshot", true);
                       open_tab(run, 0);
                   },
                   true});
}

} // namespace pch::host
