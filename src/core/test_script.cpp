// ProsperoLichess - Scripted controller input for unattended hardware runs.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/test_script.hpp"

#include <cstdlib>

namespace pch
{

namespace
{

std::string_view trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
        s.remove_suffix(1);
    return s;
}

bool action_named(std::string_view name, Action *out)
{
    struct Named
    {
        const char *name;
        Action action;
    };
    static const Named kNames[] = {
        {"confirm", Action::confirm},     {"back", Action::back},
        {"north", Action::north},         {"west", Action::west},
        {"page_prev", Action::page_prev}, {"page_next", Action::page_next},
        {"jump_prev", Action::jump_prev}, {"jump_next", Action::jump_next},
        {"menu", Action::menu},           {"touch", Action::touch},
    };
    for (const Named &n : kNames)
    {
        if (name == n.name)
        {
            *out = n.action;
            return true;
        }
    }
    return false;
}

} // namespace

bool TestScript::parse(std::string_view text, std::string *error)
{
    steps_.clear();
    next_ = 0;
    waited_ = 0;
    quit_ = false;
    guest_ = false;
    fresh_ = false;
    int line_number = 0;
    while (!text.empty())
    {
        ++line_number;
        const std::size_t end = text.find('\n');
        std::string_view line = trim(text.substr(0, end));
        text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
        if (const std::size_t hash = line.find('#'); hash != std::string_view::npos)
            line = trim(line.substr(0, hash));
        if (line.empty())
            continue;
        const std::size_t space = line.find(' ');
        const std::string_view verb = line.substr(0, space);
        const std::string_view arg =
            space == std::string_view::npos ? std::string_view() : trim(line.substr(space + 1));
        if (verb == "guest")
        {
            guest_ = true;
            continue;
        }
        if (verb == "fresh")
        {
            fresh_ = true;
            continue;
        }
        Step step;
        if (verb == "wait")
        {
            step.wait = std::atoi(std::string(arg).c_str());
        }
        else if (verb == "press")
        {
            Action action;
            if (!action_named(arg, &action))
            {
                *error = "line " + std::to_string(line_number) + ": unknown action";
                return false;
            }
            step.pressed = action_bit(action);
        }
        else if (verb == "hold")
        {
            const std::size_t split = arg.find(' ');
            Action action;
            const int frames = split == std::string_view::npos
                                   ? 0
                                   : std::atoi(std::string(trim(arg.substr(split + 1))).c_str());
            if (!action_named(arg.substr(0, split), &action) || frames <= 0)
            {
                *error =
                    "line " + std::to_string(line_number) + ": hold needs an action and frames";
                return false;
            }
            step.held = action_bit(action);
            step.wait = frames;
        }
        else if (verb == "nav")
        {
            step.nav = arg == "up"      ? Direction::up
                       : arg == "down"  ? Direction::down
                       : arg == "left"  ? Direction::left
                       : arg == "right" ? Direction::right
                                        : Direction::none;
            if (step.nav == Direction::none)
            {
                *error = "line " + std::to_string(line_number) + ": unknown direction";
                return false;
            }
        }
        else if (verb == "mark")
        {
            step.mark = std::string(arg);
        }
        else if (verb == "update")
        {
            if (arg.empty())
            {
                *error = "line " + std::to_string(line_number) + ": update needs a version";
                return false;
            }
            step.update = std::string(arg);
        }
        else if (verb == "quit")
        {
            step.quit = true;
        }
        else
        {
            *error = "line " + std::to_string(line_number) + ": unknown command";
            return false;
        }
        steps_.push_back(std::move(step));
    }
    return true;
}

std::string TestScript::step(InputFrame *frame)
{
    if (!active())
        return {};
    InputFrame scripted;
    scripted.connected = true;
    const Step &s = steps_[next_];
    std::string mark;
    if (s.wait > 0)
    {
        // A held action goes down on its first frame and stays down.
        scripted.held = s.held;
        if (waited_ == 0)
            scripted.pressed = s.held;
        if (++waited_ >= s.wait)
        {
            waited_ = 0;
            ++next_;
        }
    }
    else
    {
        scripted.pressed = s.pressed;
        scripted.held = s.pressed;
        scripted.nav = s.nav;
        mark = s.mark;
        quit_ = quit_ || s.quit;
        if (!s.update.empty())
            update_ = s.update;
        ++next_;
    }
    *frame = scripted;
    return mark;
}

} // namespace pch
