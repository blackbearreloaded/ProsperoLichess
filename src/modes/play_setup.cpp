// ProsperoLichess - Play page: the time controls on offer and the requests they make.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modes/play_setup.hpp"

#include "core/strings.hpp"

#include <algorithm>
#include <cstdio>

namespace pch::modes
{

namespace
{

constexpr TimeControl kPools[] = {
    {"10+0", TR("Rapid"), 10, 0, 0},       {"10+5", TR("Rapid"), 10, 5, 0},
    {"15+10", TR("Rapid"), 15, 10, 0},     {"30+0", TR("Classical"), 30, 0, 0},
    {"30+20", TR("Classical"), 30, 20, 0}, {"3 days", TR("Correspondence"), 0, 0, 3},
};

constexpr TimeControl kAiClocks[] = {
    {"5+3", TR("Blitz"), 5, 3, 0},       {"10+0", TR("Rapid"), 10, 0, 0},
    {"10+5", TR("Rapid"), 10, 5, 0},     {"15+10", TR("Rapid"), 15, 10, 0},
    {"30+0", TR("Classical"), 30, 0, 0}, {"Unlimited", TR("Correspondence"), 0, 0, 0},
};

constexpr const char *kDot = "  \xC2\xB7  ";

} // namespace

std::span<const TimeControl> pairing_pools()
{
    return kPools;
}

std::span<const TimeControl> ai_clocks()
{
    return kAiClocks;
}

std::string perf_key(const TimeControl &tc)
{
    std::string perf = tc.days > 0 ? "correspondence" : tc.speed;
    std::transform(perf.begin(), perf.end(), perf.begin(), [](unsigned char c)
                   { return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c); });
    return perf;
}

const char *speed_text(const TimeControl &tc)
{
    return tr(tc.speed);
}

std::string speed_name(std::string_view key)
{
    if (key == "bullet")
        return tr("Bullet");
    if (key == "blitz")
        return tr("Blitz");
    if (key == "rapid")
        return tr("Rapid");
    if (key == "classical")
        return tr("Classical");
    if (key == "correspondence")
        return tr("Correspondence");
    std::string name(key);
    if (!name.empty() && name[0] >= 'a' && name[0] <= 'z')
        name[0] = static_cast<char>(name[0] - 'a' + 'A');
    return name;
}

std::string control_text(const TimeControl &tc)
{
    if (tc.days > 0)
        return plural(TR("{0} day"), TR("{0} days"), tc.days);
    if (tc.minutes == 0)
        return tr("Unlimited");
    return tc.label;
}

std::string increment_text(const TimeControl &tc)
{
    if (tc.days > 0)
        return tr("Per move");
    if (tc.minutes == 0)
        return tr("No clock");
    if (tc.increment == 0)
        return tr("No increment");
    return fill(tr("+{0} s per move"), {std::to_string(tc.increment)});
}

std::string clock_sentence(const TimeControl &tc)
{
    if (tc.days > 0)
        return plural(TR("{0} day for every move"), TR("{0} days for every move"), tc.days);
    if (tc.minutes == 0)
        return tr("No clock");
    const std::string minutes = std::to_string(tc.minutes);
    if (tc.increment > 0)
        return fill(tr("{0} minutes each, +{1} s per move"),
                    {minutes, std::to_string(tc.increment)});
    return fill(tr("{0} minutes each"), {minutes});
}

GameRequest seek_request(const TimeControl &tc, bool rated)
{
    char buffer[96];
    if (tc.days > 0)
        std::snprintf(buffer, sizeof(buffer), "rated=%s&days=%d", rated ? "true" : "false",
                      tc.days);
    else
        std::snprintf(buffer, sizeof(buffer), "rated=%s&time=%d&increment=%d",
                      rated ? "true" : "false", tc.minutes, tc.increment);
    GameRequest request;
    request.form = buffer;
    // Two facts with a dot between them: what is played, and whether it counts.
    request.label =
        fill(trc("speed, time control", "{0} {1}"), {speed_text(tc), control_text(tc)}) + kDot +
        (rated ? tr("Rated") : tr("Casual"));
    return request;
}

GameRequest ai_request(const TimeControl &tc, int level, AiColor color)
{
    level = std::clamp(level, 1, kAiLevels);
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "level=%d&color=%s", level,
                  color == AiColor::random  ? "random"
                  : color == AiColor::white ? "white"
                                            : "black");
    GameRequest request;
    request.form = buffer;
    // A game without a clock is asked for by leaving the clock out.
    if (tc.minutes > 0)
    {
        std::snprintf(buffer, sizeof(buffer), "&clock.limit=%d&clock.increment=%d", tc.minutes * 60,
                      tc.increment);
        request.form += buffer;
    }
    request.label =
        fill(tr("Stockfish level {0}"), {std::to_string(level)}) + kDot + control_text(tc);
    return request;
}

} // namespace pch::modes
