// ProsperoLichess - The Play page under a controller: tabs, focus, what starts a game.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "play_test_app.hpp"

namespace
{

using namespace pch;

class PlayPageTest : public play_tests::AppFixture
{
  protected:
    // True when the controller is on the side rail: down then shows Watch.
    bool on_rail()
    {
        nav(Direction::down);
        idle(3);
        return scene() == "watch";
    }
};

TEST_F(PlayPageTest, OfflineLeadsToPassAndPlay)
{
    open_page(modes::kPagePlay);
    ASSERT_EQ(scene(), "play");
    // Online cannot be used offline: its one button shows the Friend tab ...
    press(Action::confirm);
    idle(20);
    EXPECT_EQ(scene(), "play");
    // ... whose button opens the game.
    press(Action::confirm);
    idle(40);
    EXPECT_NE(scene(), "play");
}

TEST_F(PlayPageTest, TriangleStartsFromAnywhereInATab)
{
    sign_in();
    open_page(modes::kPagePlay);
    press(Action::north);
    EXPECT_TRUE(asked(audio::Cue::launch));
    idle(40);
    EXPECT_NE(scene(), "play");
    press(Action::back);
    idle(40);
    EXPECT_EQ(scene(), "play");

    // The Computer tab the same.
    press(Action::page_next);
    idle(10);
    press(Action::north);
    idle(40);
    EXPECT_NE(scene(), "play");
}

TEST_F(PlayPageTest, CrossChoosesThenStarts)
{
    sign_in();
    open_page(modes::kPagePlay);
    nav(Direction::down); // another time control
    press(Action::confirm);
    idle(10);
    EXPECT_EQ(scene(), "play"); // chosen, not started
    press(Action::confirm);
    idle(40);
    EXPECT_NE(scene(), "play");
}

TEST_F(PlayPageTest, ShoulderButtonsTurnTheTabs)
{
    sign_in();
    open_page(modes::kPagePlay);
    press(Action::page_next);
    EXPECT_TRUE(asked(audio::Cue::tab));
    press(Action::page_next);
    idle(10);
    // Friend: no Triangle here, Cross on its button opens the game.
    press(Action::north);
    idle(10);
    EXPECT_EQ(scene(), "play");
    press(Action::confirm);
    idle(40);
    EXPECT_NE(scene(), "play");
}

TEST_F(PlayPageTest, CircleReturnsToTheRail)
{
    sign_in();
    open_page(modes::kPagePlay);
    press(Action::back);
    EXPECT_TRUE(asked(audio::Cue::back));
    EXPECT_TRUE(on_rail());
}

TEST_F(PlayPageTest, LeftOfTheLeftmostControlReturnsToTheRail)
{
    sign_in();
    open_page(modes::kPagePlay);
    nav(Direction::left); // 10+5 -> 10+0
    nav(Direction::left); // out of the grid
    EXPECT_TRUE(on_rail());
}

TEST_F(PlayPageTest, LeftOnTheFirstTabReturnsToTheRail)
{
    sign_in();
    open_page(modes::kPagePlay);
    nav(Direction::up); // the tab row
    nav(Direction::right);
    nav(Direction::left);
    idle(5);
    EXPECT_EQ(scene(), "play");
    nav(Direction::left);
    EXPECT_TRUE(on_rail());
}

TEST_F(PlayPageTest, SignedOutOffersToSignIn)
{
    sign_in();
    app_->session().sign_out(); // online, without an account
    open_page(modes::kPagePlay);
    idle(40);
    press(Action::confirm);
    idle(40);
    EXPECT_NE(scene(), "play");
}

TEST_F(PlayPageTest, EveryTabDrawsInEveryState)
{
    for (int round = 0; round < 2; ++round)
    {
        if (round == 1)
            sign_in();
        open_page(modes::kPagePlay);
        for (int tab = 0; tab < 3; ++tab)
        {
            idle(20);
            EXPECT_GT(shapes_, 80u) << "round " << round << " tab " << tab;
            press(Action::page_next);
        }
        press(Action::page_prev);
        press(Action::page_prev);
    }
}

// A tab gives way to the next one over a few frames: both are drawn
// meanwhile, and the controller is not kept waiting.
TEST_F(PlayPageTest, ATabChangeDrawsBothTabsAndTakesInputAtOnce)
{
    sign_in();
    open_page(modes::kPagePlay);
    idle(20);
    const std::size_t settled = shapes_;
    press(Action::page_next);
    idle(5);
    EXPECT_GT(shapes_, settled) << "the leaving tab is still drawn while the new one arrives";
    // Triangle in the middle of it starts the Computer tab's game.
    press(Action::north);
    idle(40);
    EXPECT_NE(scene(), "play");
}

// Reduced motion: every tab is simply there, and the page works the same.
TEST_F(PlayPageTest, ReducedMotionKeepsEveryTabAndEveryFlow)
{
    sign_in();
    app_->context().settings->reduced_motion = true;
    open_page(modes::kPagePlay);
    for (int tab = 0; tab < 3; ++tab)
    {
        frame();
        EXPECT_GT(shapes_, 80u) << "tab " << tab;
        press(Action::page_next);
    }
    // Friend: Cross on its button opens the game.
    press(Action::confirm);
    idle(40);
    EXPECT_NE(scene(), "play");
}

// The Friend tab shows a saved game as it was left; the page must draw it
// and still open it.
TEST_F(PlayPageTest, TheFriendTabShowsASavedGameAndResumesIt)
{
    ASSERT_EQ(save::write_atomic(app_->context().data_root + "/passplay.sav",
                                 save::encode(save::Kind::game, 1, "e2e4 e7e5 g1f3")),
              "");
    open_page(modes::kPagePlay);
    press(Action::page_next);
    press(Action::page_next);
    idle(40);
    EXPECT_EQ(scene(), "play");
    EXPECT_GT(shapes_, 80u);
    press(Action::confirm);
    idle(40);
    EXPECT_EQ(scene(), "pass-and-play");
}

} // namespace
