// ProsperoLichess - Profile, signing in and Settings, driven through the whole app.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/app.hpp"
#include "core/save_file.hpp"
#include "gfx/font.hpp"
#include "lichess/session.hpp"
#include "modes/page.hpp"
#include "modes/scenes.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#ifndef PCH_SOURCE_DIR
#define PCH_SOURCE_DIR "."
#endif

namespace
{

using namespace pch;

struct Faces
{
    gfx::Font regular;
    gfx::Font semibold;
    gfx::Font display;
    gfx::Font mono;
    ui::Fonts fonts;
    bool ok = false;
};

// The baked faces are large: load them once for every test.
Faces &faces()
{
    static Faces set;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        const std::string dir = std::string(PCH_SOURCE_DIR) + "/assets/fonts/";
        const auto load =
            [&](const char *name, gfx::Font *font, ui::FontRef *ref, std::uint32_t texture)
        {
            std::string data;
            if (!save::read_file(dir + name, &data) || !font->load(data))
                return false;
            *ref = {font, texture};
            return true;
        };
        set.ok = load("inter-regular.pchfont", &set.regular, &set.fonts.regular, 1) &&
                 load("inter-semibold.pchfont", &set.semibold, &set.fonts.semibold, 2) &&
                 load("montserrat-medium.pchfont", &set.display, &set.fonts.display, 3) &&
                 load("dejavu-sans-mono.pchfont", &set.mono, &set.fonts.mono, 4);
        set.fonts.pixel = set.fonts.mono;
        set.fonts.hand = set.fonts.regular;
    }
    return set;
}

class AccountScreens : public ::testing::Test
{
  protected:
    static constexpr float kFrame = 1.0f / 60.0f;

    void SetUp() override
    {
        ASSERT_TRUE(faces().ok) << "baked fonts missing";
        char root[] = "/tmp/pch-account-test-XXXXXX";
        ASSERT_NE(mkdtemp(root), nullptr);
        root_ = root;
        app::Gpu gpu;
        gpu.create_texture = [this](int, int, const std::uint8_t *) { return ++textures_; };
        gpu.delete_texture = [](std::uint32_t) {};
        gpu.glass_texture = 9;
        app_ = std::make_unique<app::App>(std::move(gpu), faces().fonts, 1.0f, root_,
                                          std::string(PCH_SOURCE_DIR) + "/assets", false, false);
        idle(10);
    }

    void frame(InputFrame input = {})
    {
        input.connected = true;
        app_->update(input, kFrame);
        app_->record_frame();
    }
    void idle(int frames)
    {
        for (int i = 0; i < frames; ++i)
            frame();
    }
    void press(Action action)
    {
        InputFrame input;
        input.pressed = action_bit(action);
        input.held = action_bit(action);
        frame(input);
        idle(2);
    }
    void nav(Direction direction, int times = 1)
    {
        for (int i = 0; i < times; ++i)
        {
            InputFrame input;
            input.nav = direction;
            frame(input);
            idle(2);
        }
    }
    // Keeps a button down for a while (a hold-to-confirm).
    void hold(Action action, float seconds)
    {
        InputFrame input;
        input.pressed = action_bit(action);
        input.held = action_bit(action);
        frame(input);
        input.pressed = 0;
        const int frames = static_cast<int>(seconds / kFrame) + 1;
        for (int i = 0; i < frames; ++i)
            frame(input);
        idle(2);
    }
    // Shows a rail entry and moves the controller into its page.
    void page(int index)
    {
        press(Action::menu);
        nav(Direction::up, modes::kPageCount);
        nav(Direction::down, index);
        press(Action::confirm);
        idle(20);
    }
    void sign_in(int games, int waiting = 1)
    {
        lichess::Account account;
        account.id = "tester";
        account.username = "tester";
        account.perfs = {{"rapid", 1500, false, 10, 0, {}}, {"puzzle", 1600, true, 20, 0, {}}};
        std::vector<lichess::OngoingGame> ongoing;
        for (int i = 0; i < games; ++i)
        {
            lichess::OngoingGame game;
            game.game_id = "game000" + std::to_string(i);
            game.fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1";
            game.last_move = "e2e4";
            game.opponent = "someone";
            game.my_turn = i < waiting;
            ongoing.push_back(game);
        }
        app_->session().preview(std::move(account), std::move(ongoing));
        idle(40);
    }
    std::string root_;
    std::uint32_t textures_ = 100;
    std::unique_ptr<app::App> app_;
};

// ---- Settings ----

TEST_F(AccountScreens, SettingsChangeAppliesAndIsSaved)
{
    page(modes::kPageSettings);
    const int board = app_->settings().board_theme;
    EXPECT_TRUE(app_->take_settings_changed()); // the app starts with one pending
    press(Action::confirm);                     // into the Board form
    nav(Direction::right);                      // the next board finish
    EXPECT_EQ(app_->settings().board_theme, (board + 1) % board::kBoardThemeCount);
    EXPECT_TRUE(app_->take_settings_changed());
    // Saved at once: a fresh read of the folder gives the new value.
    EXPECT_EQ(app::App::load_settings(root_).board_theme, app_->settings().board_theme);

    nav(Direction::down);
    nav(Direction::left); // the piece set before the first: the last one
    EXPECT_EQ(app_->settings().piece_set, board::kPieceSetCount - 1);
    nav(Direction::down);
    press(Action::confirm); // coordinates off
    EXPECT_FALSE(app_->settings().coordinates);
    EXPECT_FALSE(app::App::load_settings(root_).coordinates);
}

TEST_F(AccountScreens, SettingsReachEveryValue)
{
    page(modes::kPageSettings);
    // Sound: three sliders from 0 to 10.
    nav(Direction::down, 2);
    press(Action::confirm);
    const int music = app_->settings().music_volume;
    nav(Direction::left);
    EXPECT_EQ(app_->settings().music_volume, music - 1);
    nav(Direction::left, 12);
    EXPECT_EQ(app_->settings().music_volume, 0);
    nav(Direction::right, 14);
    EXPECT_EQ(app_->settings().music_volume, 10);
    nav(Direction::down, 2);
    nav(Direction::left);
    EXPECT_EQ(app_->settings().ui_volume, 6);

    // Circle returns to the categories, then Controller and Display.
    press(Action::back);
    nav(Direction::down);
    nav(Direction::right);
    press(Action::confirm);
    EXPECT_TRUE(app_->settings().swap_confirm);
    // With the buttons swapped the test's "confirm" is still the logical action.
    press(Action::confirm);
    EXPECT_FALSE(app_->settings().swap_confirm);
    nav(Direction::down);
    nav(Direction::left);
    EXPECT_FALSE(app_->settings().vibration);

    press(Action::back);
    nav(Direction::down);
    press(Action::confirm);
    const int resolution = app_->settings().resolution;
    nav(Direction::left);
    EXPECT_NE(app_->settings().resolution, resolution);
    nav(Direction::down);
    press(Action::confirm);
    EXPECT_FALSE(app_->settings().show_fps);
    nav(Direction::down);
    nav(Direction::right);
    EXPECT_TRUE(app_->settings().reduced_motion);
    idle(30); // every component now moves without motion
    EXPECT_STREQ(app_->active_scene(), "settings");
}

TEST_F(AccountScreens, ResolutionChangeAsksForADisplayRestart)
{
    app_->set_applied_resolution(app_->settings().resolution);
    page(modes::kPageSettings);
    nav(Direction::down, 4);
    press(Action::confirm);
    EXPECT_FALSE(app_->take_display_mode_changed());
    nav(Direction::right);
    EXPECT_TRUE(app_->take_display_mode_changed());
}

TEST_F(AccountScreens, SettingsFocusGoesCategoriesFormRail)
{
    page(modes::kPageSettings);
    nav(Direction::down, 5); // About
    nav(Direction::down);    // the end of the list refuses
    press(Action::confirm);  // into the text
    nav(Direction::down, 3);
    nav(Direction::left); // back to the categories
    const int board = app_->settings().board_theme;
    nav(Direction::up, 5);
    nav(Direction::right); // into the Board form, nothing changed yet
    EXPECT_EQ(app_->settings().board_theme, board);
    press(Action::back); // categories
    press(Action::back); // rail
    EXPECT_STREQ(app_->active_scene(), "settings");
    press(Action::back); // the rail's Circle shows Home
    idle(10);
    EXPECT_STREQ(app_->active_scene(), "home");
}

// ---- Profile ----

TEST_F(AccountScreens, SignedOutProfileOpensTheSignInScreen)
{
    page(modes::kPageProfile);
    EXPECT_STREQ(app_->active_scene(), "profile");
    press(Action::confirm);
    idle(30);
    EXPECT_STREQ(app_->active_scene(), "account");
    press(Action::back);
    idle(30);
    EXPECT_STREQ(app_->active_scene(), "profile");
}

TEST_F(AccountScreens, ProfileOpensAGameInProgress)
{
    sign_in(2);
    page(modes::kPageProfile);
    nav(Direction::right);
    nav(Direction::right); // the end of the shelf refuses
    press(Action::confirm);
    idle(10);
    EXPECT_STRNE(app_->active_scene(), "profile");
    app_->go_home();
    idle(30);
    EXPECT_STREQ(app_->active_scene(), "profile");
}

TEST_F(AccountScreens, ProfileWithoutGamesLeadsToPlay)
{
    sign_in(0);
    page(modes::kPageProfile);
    press(Action::confirm);
    idle(20);
    EXPECT_STREQ(app_->active_scene(), "play");
}

TEST_F(AccountScreens, SigningOutNeedsAHold)
{
    sign_in(1);
    page(modes::kPageProfile);
    nav(Direction::up); // the Sign out button
    press(Action::confirm);
    idle(30);
    EXPECT_TRUE(app_->session().signed_in()) << "a tap must not sign out";
    hold(Action::confirm, 0.5f);
    idle(60);
    EXPECT_TRUE(app_->session().signed_in()) << "half a hold must not sign out";
    hold(Action::confirm, 1.6f);
    EXPECT_FALSE(app_->session().signed_in());
    idle(40);
    EXPECT_STREQ(app_->active_scene(), "profile");
    // Signed out, the page offers to sign in again.
    press(Action::confirm);
    idle(30);
    EXPECT_STREQ(app_->active_scene(), "account");
}

// ---- the bell in the strip ----

TEST_F(AccountScreens, TouchpadOpensTheGameThatWaits)
{
    sign_in(2);
    ASSERT_STREQ(app_->active_scene(), "home");
    press(Action::touch);
    idle(10);
    EXPECT_STRNE(app_->active_scene(), "home");
    EXPECT_STRNE(app_->active_scene(), "profile");
    app_->go_home();
    idle(30);
    EXPECT_STREQ(app_->active_scene(), "home");
}

TEST_F(AccountScreens, TouchpadOpensTheGameFromTheRailToo)
{
    sign_in(1);
    press(Action::menu); // the rail has the controller
    press(Action::touch);
    idle(10);
    EXPECT_STRNE(app_->active_scene(), "home");
}

TEST_F(AccountScreens, TouchpadShowsTheGamesWhenSeveralWait)
{
    sign_in(3, 2);
    press(Action::touch);
    idle(20);
    ASSERT_STREQ(app_->active_scene(), "profile");
    // The controller is in the list, on the first game that waits.
    press(Action::confirm);
    idle(10);
    EXPECT_STRNE(app_->active_scene(), "profile");
}

TEST_F(AccountScreens, TouchpadDoesNothingWhenNoGameWaits)
{
    press(Action::touch); // signed out
    idle(10);
    EXPECT_STREQ(app_->active_scene(), "home");
    sign_in(1, 0);
    press(Action::touch);
    idle(10);
    EXPECT_STREQ(app_->active_scene(), "home");
}

// ---- signing in ----

TEST_F(AccountScreens, TokenNeedsEightCharacters)
{
    app_->open(modes::make_account());
    idle(30);
    ASSERT_STREQ(app_->active_scene(), "account");
    press(Action::west); // the token keyboard (it is where the screen starts without a network)
    idle(20);
    press(Action::confirm); // "lip_" + one character
    press(Action::menu);    // Done: too short
    idle(10);
    EXPECT_FALSE(app_->session().signing_in());
    for (int i = 0; i < 5; ++i)
        press(Action::confirm);
    press(Action::menu);
    EXPECT_TRUE(app_->session().signing_in());
    idle(30);
    EXPECT_STREQ(app_->active_scene(), "account");
    // Nothing answers in a test: the check gives up and says so.
    idle(60 * 46);
    EXPECT_FALSE(app_->session().signed_in());
    press(Action::back);
    idle(30);
    EXPECT_STREQ(app_->active_scene(), "home");
}

TEST_F(AccountScreens, SignInScreenClosesOnceSignedIn)
{
    app_->open(modes::make_account());
    idle(30);
    sign_in(1);
    EXPECT_STREQ(app_->active_scene(), "account");
    // The welcome leaves by itself after two seconds.
    idle(150);
    EXPECT_STREQ(app_->active_scene(), "home");
}

TEST_F(AccountScreens, SignInScreenSaysSoWhenAlreadySignedIn)
{
    sign_in(1);
    app_->open(modes::make_account());
    idle(200); // no timer closes this state
    EXPECT_STREQ(app_->active_scene(), "account");
    press(Action::back);
    idle(30);
    EXPECT_STREQ(app_->active_scene(), "home");
    EXPECT_TRUE(app_->session().signed_in());
}

} // namespace
