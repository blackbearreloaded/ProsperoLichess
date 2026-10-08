// ProsperoLichess - Play page: the requests a choice of time control makes.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modes/play_setup.hpp"

#include <gtest/gtest.h>

namespace
{

using namespace pch::modes;

TEST(PlaySetup, OffersThePoolsTheBoardApiPairs)
{
    const auto pools = pairing_pools();
    ASSERT_EQ(pools.size(), 6u);
    // Rapid or slower: limit + 40 * increment is at least 480 seconds.
    for (const TimeControl &tc : pools)
        EXPECT_TRUE(tc.days > 0 || tc.minutes * 60 + 40 * tc.increment >= 480) << tc.label;
    EXPECT_STREQ(pools[0].label, "10+0");
    EXPECT_STREQ(pools[5].label, "3 days");
    EXPECT_EQ(ai_clocks().size(), 6u);
    EXPECT_STREQ(ai_clocks()[0].label, "5+3");
    EXPECT_STREQ(ai_clocks()[5].label, "Unlimited");
}

TEST(PlaySetup, SeeksAreTheFormsLichessExpects)
{
    const auto pools = pairing_pools();
    GameRequest request = seek_request(pools[1], true);
    EXPECT_EQ(request.form, "rated=true&time=10&increment=5");
    EXPECT_EQ(request.label, "Rapid 10+5  \xC2\xB7  Rated");
    request = seek_request(pools[4], false);
    EXPECT_EQ(request.form, "rated=false&time=30&increment=20");
    EXPECT_EQ(request.label, "Classical 30+20  \xC2\xB7  Casual");
    // Correspondence is asked for in days.
    request = seek_request(pools[5], true);
    EXPECT_EQ(request.form, "rated=true&days=3");
    EXPECT_EQ(request.label, "Correspondence 3 days  \xC2\xB7  Rated");
}

TEST(PlaySetup, ComputerGamesNameLevelColourAndClock)
{
    const auto clocks = ai_clocks();
    GameRequest request = ai_request(clocks[2], 3, AiColor::random);
    EXPECT_EQ(request.form, "level=3&color=random&clock.limit=600&clock.increment=5");
    EXPECT_EQ(request.label, "Stockfish level 3  \xC2\xB7  10+5");
    request = ai_request(clocks[0], 8, AiColor::black);
    EXPECT_EQ(request.form, "level=8&color=black&clock.limit=300&clock.increment=3");
    // No clock: the clock fields are left out.
    request = ai_request(clocks[5], 1, AiColor::white);
    EXPECT_EQ(request.form, "level=1&color=white");
    EXPECT_EQ(request.label, "Stockfish level 1  \xC2\xB7  Unlimited");
    // A level outside 1..8 never reaches Lichess.
    EXPECT_EQ(ai_request(clocks[5], 12, AiColor::white).form, "level=8&color=white");
    EXPECT_EQ(ai_request(clocks[5], 0, AiColor::white).form, "level=1&color=white");
}

TEST(PlaySetup, DescribesATimeControl)
{
    const auto pools = pairing_pools();
    EXPECT_EQ(perf_key(pools[0]), "rapid");
    EXPECT_EQ(perf_key(pools[3]), "classical");
    EXPECT_EQ(perf_key(pools[5]), "correspondence");
    EXPECT_EQ(perf_key(ai_clocks()[0]), "blitz");
    EXPECT_EQ(increment_text(pools[0]), "No increment");
    EXPECT_EQ(increment_text(pools[1]), "+5 s per move");
    EXPECT_EQ(increment_text(pools[5]), "Per move");
    EXPECT_EQ(increment_text(ai_clocks()[5]), "No clock");
    EXPECT_EQ(clock_sentence(pools[1]), "10 minutes each, +5 s per move");
    EXPECT_EQ(clock_sentence(pools[3]), "30 minutes each");
    EXPECT_EQ(clock_sentence(pools[5]), "3 days for every move");
}

} // namespace
