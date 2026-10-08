// ProsperoLichess - Host snapshots: the scripted scenarios that are rendered to pictures.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A scenario runs a fresh App over its own scratch data folder, sends it
// controller input, and ends with one picture named after the scenario
// (shot() takes more on the way). Each area of the app keeps its scenarios in
// a file of its own (host/scenarios_*.cpp) and adds them through one of the
// functions at the end of this header.

#pragma once

#include "app/app.hpp"
#include "core/input.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace pch::host
{

// What a scenario's script drives.
struct Run
{
    app::App &app;
    std::string assets; // the assets folder (tests/ is beside it)
    bool realtime;      // sleep between frames (scenarios that talk to lichess.org)
    std::function<void(const std::string &suffix)> picture;

    // Runs frames without input.
    void idle(int frames);
    // One frame with this input, then a few quiet ones.
    void send(InputFrame frame);
    void press(Action action);
    void nav(Direction direction, int times = 1);
    // From anywhere on the home screen: shows a rail entry (modes::PageIndex)
    // and moves the controller into its page.
    void page(int index);
    // Opens a screen directly, on top of whatever is showing.
    void open(std::unique_ptr<app::Scene> scene);
    // Saves "<scenario>-<suffix>.png" now.
    void shot(const std::string &suffix)
    {
        picture(suffix);
    }
};

struct Scenario
{
    std::string name;
    std::function<void(Run &)> script;
    bool signed_in = false; // a previewed account with ratings and games (no network)
    bool connect = false;   // talk to the real lichess.org (needs PCH_ONLINE=1)
    bool boot = false;      // start with the opening title
};

using Scenarios = std::vector<Scenario>;

// The account, games and daily puzzle a signed_in scenario shows.
void preview_account(app::App &app, const std::string &assets);

void add_shell_scenarios(Scenarios &all);   // boot, home, the rail, notices
void add_puzzle_scenarios(Scenarios &all);  // the Puzzles page and the puzzle screen
void add_play_scenarios(Scenarios &all);    // the Play page
void add_game_scenarios(Scenarios &all);    // waiting for a game, the game screen
void add_watch_scenarios(Scenarios &all);   // the Watch page and Lichess TV
void add_account_scenarios(Scenarios &all); // Profile, signing in, Settings

} // namespace pch::host
