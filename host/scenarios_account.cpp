// ProsperoLichess - Host snapshots: Profile, signing in, Settings.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lichess/session.hpp"
#include "modes/page.hpp"
#include "modes/scenes.hpp"
#include "scenarios.hpp"

namespace pch::host
{

namespace
{

// Settings with the controller in the form of one category (0 is Board).
void open_settings(Run &run, int category)
{
    run.page(modes::kPageSettings);
    run.nav(Direction::down, category);
    run.idle(30);
}

// Turns "Reduce motion" on the way a player would: Settings, Display, the
// third row.
void reduce_motion(Run &run)
{
    run.page(modes::kPageSettings);
    run.nav(Direction::down, 4);
    run.press(Action::confirm);
    run.nav(Direction::down, 2);
    run.nav(Direction::right);
    run.idle(10);
}

// From anywhere on the home screen: the rail shows a page and keeps the
// controller, and the picture is taken a few frames later.
void glance(Run &run, int page, int frames)
{
    run.press(Action::menu);
    run.nav(Direction::up, modes::kPageCount);
    run.nav(Direction::down, page);
    run.idle(frames);
}

// The Profile page with the controller in it, once it has assembled and its
// numbers have counted.
void open_profile(Run &run)
{
    run.page(modes::kPageProfile);
    run.idle(110);
}

// The sign-in screen on its token keyboard, with a few characters typed
// after the "lip_" it starts with.
void open_token_keyboard(Run &run, int characters)
{
    run.open(modes::make_account());
    run.press(Action::west);
    run.idle(40);
    for (int i = 0; i < characters; ++i)
    {
        run.press(Action::confirm);
        run.nav(Direction::right);
    }
    run.idle(20);
}

} // namespace

void add_account_scenarios(Scenarios &all)
{
    // ---- Profile ----
    all.push_back({"profile", [](Run &run) { open_profile(run); }, true});
    // A moment after the rail showed it: the page is assembling, its numbers
    // are counting and its charts are growing.
    all.push_back({"profile-arriving",
                   [](Run &run)
                   {
                       run.press(Action::menu);
                       run.nav(Direction::down, modes::kPageProfile);
                       run.idle(24);
                   },
                   true});
    // With reduced motion nothing slides, counts or grows.
    all.push_back({"profile-reduced-motion",
                   [](Run &run)
                   {
                       reduce_motion(run);
                       glance(run, modes::kPageProfile, 6);
                   },
                   true});
    all.push_back({"profile-signed-out", [](Run &run) { open_profile(run); }});
    all.push_back({"profile-signed-out-arriving", [](Run &run)
                   {
                       run.press(Action::menu);
                       run.nav(Direction::down, modes::kPageProfile);
                       run.idle(9);
                   }});
    all.push_back({"profile-signing-in", [](Run &run)
                   {
                       // Offline nothing answers, so the account check stays open.
                       run.app.session().sign_in("lip_snapshot", true);
                       open_profile(run);
                   }});
    all.push_back({"profile-second-game",
                   [](Run &run)
                   {
                       open_profile(run);
                       run.nav(Direction::right);
                       run.idle(30);
                   },
                   true});
    // An account that has hardly played: the longest name Lichess allows, a title, one
    // provisional rating, no games counted and no history yet.
    all.push_back({"profile-new-account",
                   [](Run &run)
                   {
                       lichess::Account account;
                       account.id = "a_rather_long_name42";
                       account.username = "A_Rather_Long_Name42";
                       account.title = "WFM";
                       account.perfs = {{"blitz", 1500, true, 3, 0, {}}};
                       run.app.session().preview(std::move(account), {});
                       open_profile(run);
                   },
                   true});
    all.push_back({"profile-no-games",
                   [](Run &run)
                   {
                       // A titled player with nothing in progress.
                       lichess::Account account = run.app.session().account();
                       account.title = "NM";
                       run.app.session().preview(std::move(account), {});
                       open_profile(run);
                   },
                   true});
    all.push_back({"profile-sign-out",
                   [](Run &run)
                   {
                       open_profile(run);
                       run.nav(Direction::up);
                       run.idle(20);
                       run.shot("focused");
                       // Half a hold: the button is filling.
                       InputFrame hold;
                       hold.connected = true;
                       hold.pressed = action_bit(Action::confirm);
                       hold.held = action_bit(Action::confirm);
                       run.app.update(hold, 1.0f / 60.0f);
                       hold.pressed = 0;
                       for (int i = 0; i < 40; ++i)
                           run.app.update(hold, 1.0f / 60.0f);
                   },
                   true});
    all.push_back({"profile-signed-out-after-hold",
                   [](Run &run)
                   {
                       run.page(modes::kPageProfile);
                       run.nav(Direction::up);
                       InputFrame hold;
                       hold.connected = true;
                       hold.pressed = action_bit(Action::confirm);
                       hold.held = action_bit(Action::confirm);
                       run.app.update(hold, 1.0f / 60.0f);
                       hold.pressed = 0;
                       for (int i = 0; i < 100; ++i)
                           run.app.update(hold, 1.0f / 60.0f);
                       run.idle(60);
                   },
                   true});

    // ---- signing in ----
    all.push_back({"signin", [](Run &run)
                   {
                       run.open(modes::make_account());
                       run.idle(70);
                   }});
    // The screen was just opened: its words, its steps and the code arrive.
    all.push_back({"signin-arriving", [](Run &run)
                   {
                       run.app.open(modes::make_account());
                       run.idle(13);
                   }});
    all.push_back({"signin-token", [](Run &run) { open_token_keyboard(run, 5); }});
    all.push_back({"signin-token-short", [](Run &run)
                   {
                       open_token_keyboard(run, 2);
                       run.press(Action::menu);
                       run.idle(30);
                   }});
    all.push_back({"signin-checking", [](Run &run)
                   {
                       open_token_keyboard(run, 6);
                       run.press(Action::menu);
                       run.idle(40);
                   }});
    all.push_back({"signin-failed", [](Run &run)
                   {
                       open_token_keyboard(run, 6);
                       run.press(Action::menu);
                       // Nothing answers offline: the check gives up.
                       run.idle(60 * 46);
                   }});
    all.push_back({"signin-expired", [](Run &run)
                   {
                       run.open(modes::make_account());
                       // The link is offered for five minutes.
                       run.idle(60 * 301);
                   }});
    all.push_back({"signin-done", [](Run &run)
                   {
                       run.open(modes::make_account());
                       run.idle(20);
                       preview_account(run.app, run.assets);
                       run.idle(50);
                   }});
    all.push_back({"signin-reduced-motion", [](Run &run)
                   {
                       reduce_motion(run);
                       run.app.open(modes::make_account());
                       run.idle(14);
                       preview_account(run.app, run.assets);
                       run.idle(4);
                   }});
    // The verdict as it arrives: the mark pops, a ring leaves it.
    all.push_back({"signin-done-arriving", [](Run &run)
                   {
                       run.open(modes::make_account());
                       run.idle(20);
                       preview_account(run.app, run.assets);
                       run.idle(9);
                   }});
    all.push_back({"signin-failed-arriving", [](Run &run)
                   {
                       open_token_keyboard(run, 6);
                       run.press(Action::menu);
                       // The check gives up after forty-five seconds.
                       run.idle(60 * 45 + 8);
                   }});
    all.push_back({"signin-already",
                   [](Run &run)
                   {
                       run.open(modes::make_account());
                       run.idle(40);
                   },
                   true});

    // ---- Settings ----
    all.push_back({"settings", [](Run &run) { run.page(modes::kPageSettings); }, true});
    all.push_back({"settings-board-changed",
                   [](Run &run)
                   {
                       open_settings(run, 0);
                       run.nav(Direction::right);    // into the form
                       run.nav(Direction::right, 2); // Walnut -> Marble
                       run.nav(Direction::down);
                       run.nav(Direction::right, 2); // Classic -> Chessnut
                       run.idle(40);
                   },
                   true});
    all.push_back({"settings-board-plain",
                   [](Run &run)
                   {
                       open_settings(run, 0);
                       run.nav(Direction::right);
                       run.nav(Direction::down, 2);
                       run.press(Action::confirm); // coordinates off
                       run.nav(Direction::down);
                       run.press(Action::confirm); // legal moves off
                       run.idle(40);
                   },
                   true});
    all.push_back({"settings-play",
                   [](Run &run)
                   {
                       open_settings(run, 1);
                       run.press(Action::confirm);
                       run.idle(30);
                   },
                   true});
    all.push_back({"settings-sound",
                   [](Run &run)
                   {
                       open_settings(run, 2);
                       run.press(Action::confirm);
                       run.nav(Direction::left, 2); // the slider moves
                       run.idle(30);
                   },
                   true});
    all.push_back({"settings-controller", [](Run &run) { open_settings(run, 3); }, true});
    all.push_back({"settings-display",
                   [](Run &run)
                   {
                       open_settings(run, 4);
                       run.press(Action::confirm);
                       run.idle(30);
                   },
                   true});
    all.push_back({"settings-about",
                   [](Run &run)
                   {
                       open_settings(run, 5);
                       run.press(Action::confirm);
                       run.idle(30);
                   },
                   true});
}

} // namespace pch::host
