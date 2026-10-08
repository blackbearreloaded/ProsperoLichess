// ProsperoLichess - Scripted controller input for unattended hardware runs.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/input.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace pch
{

// A tiny script, one command per line ('#' comments):
//   wait <frames>        idle frames
//   press <action>       confirm back north west page_prev page_next jump_prev jump_next menu touch
//   hold <action> <n>    keeps the action down for n frames (hold-to-confirm buttons)
//   nav <direction>      up down left right
//   mark <text>          logs "[PCH] mark <text>" when reached
//   guest                (anywhere) the run uses storage of its own: no account, default
//                        settings, its own records; the player's data is not touched
//   fresh                (with guest) that storage is emptied first: a first launch
//   update <version>     shows the update announcement for that version
//   quit                 asks the system to close the title (the end of an unattended run)
// Test deployments place it at /app0/assets/test/script.txt; releases have none.
class TestScript
{
  public:
    bool parse(std::string_view text, std::string *error);
    // The script asked for storage of its own (the `guest` line).
    bool guest() const
    {
        return guest_;
    }
    // ... and for that storage to start empty (the `fresh` line).
    bool fresh() const
    {
        return fresh_;
    }
    bool active() const
    {
        return next_ < steps_.size();
    }
    // Replaces the frame's input while the script runs; returns a mark to log ("" if none).
    std::string step(InputFrame *frame);
    // The version an `update` command named this frame; "" otherwise.
    std::string take_update()
    {
        std::string version;
        version.swap(update_);
        return version;
    }
    // The script reached its quit command: the frame loop ends the title.
    bool quit_requested() const
    {
        return quit_;
    }

  private:
    struct Step
    {
        int wait = 0;
        std::uint32_t pressed = 0;
        std::uint32_t held = 0; // kept down for `wait` frames
        Direction nav = Direction::none;
        std::string mark;
        std::string update;
        bool quit = false;
    };
    std::vector<Step> steps_;
    std::size_t next_ = 0;
    int waited_ = 0;
    bool quit_ = false;
    bool guest_ = false;
    bool fresh_ = false;
    std::string update_;
};

} // namespace pch
