// ProsperoLichess - Host snapshots: the opening title, the home screen, the rail, notices.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/strings.hpp"
#include "core/save_file.hpp"
#include "core/test_script.hpp"
#include "lichess/session.hpp"
#include "modes/page.hpp"
#include "platform/ps5/system.hpp"
#include "scenarios.hpp"

#include <string>

namespace pch::host
{

void add_shell_scenarios(Scenarios &all)
{
    all.push_back({"boot", [](Run &run) { run.idle(100); }, false, false, true});
    // Offline and signed out: the hero falls back to the offline run.
    all.push_back({"home-offline", [](Run &run) { run.idle(60); }});
    all.push_back({"home", [](Run &run) { run.idle(60); }, true});
    all.push_back({"home-update",
                   [](Run &run)
                   {
                       run.idle(30);
                       run.app.preview_update("01.000.010");
                       run.idle(60);
                   },
                   true});
    all.push_back({"update-notes",
                   [](Run &run)
                   {
                       run.idle(30);
                       run.app.preview_update("01.000.010");
                       run.idle(30);
                       run.press(Action::north);
                       run.idle(90);
                   },
                   true});
    all.push_back({"update-notes-scrolled",
                   [](Run &run)
                   {
                       run.idle(30);
                       run.app.preview_update("01.000.010");
                       run.idle(30);
                       run.press(Action::north);
                       run.idle(60);
                       run.press(Action::page_next);
                       run.idle(60);
                   },
                   true});
    all.push_back({"update-progress",
                   [](Run &run)
                   {
                       run.idle(30);
                       run.app.preview_update("01.000.010");
                       run.idle(30);
                       run.press(Action::confirm);
                       run.idle(100);
                   },
                   true});
    all.push_back({"home-shelf",
                   [](Run &run)
                   {
                       run.idle(30);
                       run.nav(Direction::down);
                       run.nav(Direction::right, 2);
                       run.idle(40);
                   },
                   true});
    all.push_back({"home-rail",
                   [](Run &run)
                   {
                       run.idle(30);
                       run.press(Action::menu);
                       run.nav(Direction::down, 2);
                       run.idle(40);
                   },
                   true});
    all.push_back({"notice-reconnecting",
                   [](Run &run)
                   {
                       run.app.session().set_issue(
                           "game", tr("Connection to the game lost. Reconnecting\xE2\x80\xA6"));
                       run.idle(60);
                       run.shot("on");
                       run.app.session().clear_issue("game");
                       run.idle(40);
                   },
                   true});
    // The hardware tours, replayed on the PC with a picture at every mark, so
    // a script is known to reach its screens before it is sent to a console.
    for (const char *name : {"tour", "online", "look", "settings", "settings-kept", "title",
                             "passplay", "resume-a", "resume-b", "puzzles-daily", "puzzles-streak",
                             "puzzles-storm", "puzzles-themes", "update", "shortcuts"})
    {
        Scenario scenario;
        scenario.name = std::string("hardware-") + name;
        scenario.boot = true;
        scenario.connect = std::string(name) == "online" || std::string(name) == "puzzles-daily";
        const std::string file = std::string("/../tests/hardware/") + name + ".txt";
        scenario.script = [file](Run &run)
        {
            std::string text;
            std::string error;
            TestScript script;
            if (!save::read_file(run.assets + file, &text) || !script.parse(text, &error))
                return;
            while (script.active())
            {
                InputFrame frame;
                const std::string mark = script.step(&frame);
                const std::string offered = script.take_update();
                if (!offered.empty())
                    run.app.preview_update(offered);
                run.app.update(frame, 1.0f / 60.0f);
                // A console draws every frame, and some components lay
                // themselves out when first drawn: record each frame here too.
                run.app.record_frame();
                if (run.realtime)
                    sys::sleep_us(1000000 / 60);
                if (!mark.empty())
                    run.shot(mark);
            }
        };
        all.push_back(std::move(scenario));
    }
}

} // namespace pch::host
