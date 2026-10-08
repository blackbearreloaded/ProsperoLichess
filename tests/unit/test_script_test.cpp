// ProsperoLichess - Scripted controller input parsing and playback.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/test_script.hpp"

#include <gtest/gtest.h>

TEST(TestScript, PlaysWaitsPressesAndMarks)
{
    pch::TestScript script;
    std::string error;
    ASSERT_TRUE(script.parse("# tour\nwait 2\npress confirm\nnav left\nmark done\n", &error));
    pch::InputFrame frame;
    EXPECT_EQ(script.step(&frame), "");
    EXPECT_EQ(frame.pressed, 0u);
    script.step(&frame);
    script.step(&frame);
    EXPECT_TRUE(frame.is_pressed(pch::Action::confirm));
    script.step(&frame);
    EXPECT_EQ(frame.nav, pch::Direction::left);
    EXPECT_EQ(script.step(&frame), "done");
    EXPECT_FALSE(script.active());
}

TEST(TestScript, RejectsUnknownCommands)
{
    pch::TestScript script;
    std::string error;
    EXPECT_FALSE(script.parse("jump 3\n", &error));
    EXPECT_FALSE(script.parse("press sideways\n", &error));
    EXPECT_FALSE(error.empty());
}

TEST(TestScript, HoldsAnActionForItsFrames)
{
    pch::TestScript script;
    std::string error;
    ASSERT_TRUE(script.parse("hold west 3\npress back\n", &error));
    pch::InputFrame frame;
    script.step(&frame);
    EXPECT_TRUE(frame.is_pressed(pch::Action::west));
    EXPECT_TRUE(frame.is_held(pch::Action::west));
    script.step(&frame);
    EXPECT_FALSE(frame.is_pressed(pch::Action::west));
    EXPECT_TRUE(frame.is_held(pch::Action::west));
    script.step(&frame);
    EXPECT_TRUE(frame.is_held(pch::Action::west));
    script.step(&frame);
    EXPECT_FALSE(frame.is_held(pch::Action::west));
    EXPECT_TRUE(frame.is_pressed(pch::Action::back));
    EXPECT_FALSE(script.parse("hold west\n", &error));
}

TEST(TestScript, QuitIsReportedWhenReached)
{
    pch::TestScript script;
    std::string error;
    ASSERT_TRUE(script.parse("wait 1\nquit\n", &error));
    pch::InputFrame frame;
    script.step(&frame);
    EXPECT_FALSE(script.quit_requested());
    script.step(&frame);
    EXPECT_TRUE(script.quit_requested());
    EXPECT_FALSE(script.active());
}
TEST(TestScript, UpdateNamesAVersionOnce)
{
    pch::TestScript script;
    std::string error;
    ASSERT_TRUE(script.parse("update 01.000.010\nwait 1\n", &error)) << error;
    pch::InputFrame frame;
    script.step(&frame);
    EXPECT_EQ(script.take_update(), "01.000.010");
    EXPECT_EQ(script.take_update(), "");
    EXPECT_FALSE(script.parse("update\n", &error));
}
TEST(TestScript, GuestAsksForStorageOfItsOwn)
{
    pch::TestScript script;
    std::string error;
    ASSERT_TRUE(script.parse("wait 1\n", &error)) << error;
    EXPECT_FALSE(script.guest());
    ASSERT_TRUE(script.parse("guest\nwait 1\n", &error)) << error;
    EXPECT_TRUE(script.guest());
    EXPECT_FALSE(script.fresh());
    EXPECT_TRUE(script.active());
    ASSERT_TRUE(script.parse("guest\nfresh\nwait 1\n", &error)) << error;
    EXPECT_TRUE(script.fresh());
}
