// ProsperoLichess - Play page: the time controls on offer and the requests they make.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>
#include <string>
#include <string_view>

namespace pch::modes
{

struct TimeControl
{
    const char *label; // "10+5"; control_text() is what a screen shows
    const char *speed; // "Rapid", in English; speed_text() is what a screen shows
    int minutes;
    int increment;
    int days; // correspondence when > 0
};

// What can be paired from the Board API: rapid or slower
// (limit + 40 * increment >= 480 s), and correspondence.
std::span<const TimeControl> pairing_pools();
// Games against Stockfish may also be blitz, or have no clock at all.
std::span<const TimeControl> ai_clocks();

// The rating a time control counts for: "rapid", "classical", "correspondence".
std::string perf_key(const TimeControl &tc);

// What follows is text for the screen, in the player's language.

// The speed's name: "Rapid".
const char *speed_text(const TimeControl &tc);
// The name of one of Lichess' speeds by its key ("rapid": "Rapid"); a key the
// app has no name for is given a capital.
std::string speed_name(std::string_view key);
// The control itself: "10+5" as it is, "3 days", "Unlimited".
std::string control_text(const TimeControl &tc);
// "+5 s per move", "No increment", "Per move", "No clock".
std::string increment_text(const TimeControl &tc);
// "10 minutes each, +5 s per move", "3 days for every move".
std::string clock_sentence(const TimeControl &tc);

// What a waiting screen is opened with (modes::make_seek, modes::make_ai_game).
struct GameRequest
{
    std::string form;  // the request body Lichess expects
    std::string label; // what the waiting screen says is being looked for
};

GameRequest seek_request(const TimeControl &tc, bool rated);

enum class AiColor : int
{
    random,
    white,
    black,
};
inline constexpr int kAiLevels = 8; // Stockfish levels 1 to 8

GameRequest ai_request(const TimeControl &tc, int level, AiColor color);

} // namespace pch::modes
