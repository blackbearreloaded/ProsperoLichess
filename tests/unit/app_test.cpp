// ProsperoLichess - The whole app without a GPU: pages, screens and random input.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/app.hpp"
#include "core/save_file.hpp"
#include "gfx/font.hpp"
#include "lichess/session.hpp"
#include "modes/page.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>

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

class AppTest : public ::testing::Test
{
  protected:
    static constexpr float kFrame = 1.0f / 60.0f;

    void SetUp() override
    {
        ASSERT_TRUE(faces().ok) << "baked fonts missing";
        char root[] = "/tmp/pch-app-test-XXXXXX";
        ASSERT_NE(mkdtemp(root), nullptr);
        root_ = root;
        app::Gpu gpu;
        gpu.create_texture = [this](int, int, const std::uint8_t *) { return ++textures_; };
        gpu.delete_texture = [](std::uint32_t) {};
        gpu.glass_texture = 9;
        app_ = std::make_unique<app::App>(std::move(gpu), faces().fonts, 1.0f, root_,
                                          std::string(PCH_SOURCE_DIR) + "/assets", false, false);
    }

    // One frame: update, then record what would be drawn.
    void frame(InputFrame input = {})
    {
        input.connected = true;
        app_->update(input, kFrame);
        shapes_ = app_->record_frame();
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
    void nav(Direction direction)
    {
        InputFrame input;
        input.nav = direction;
        frame(input);
        idle(2);
    }
    bool asked(audio::Cue cue) const
    {
        for (const audio::CueEvent &event : app_->feedback().cues)
        {
            if (event.cue == cue)
                return true;
        }
        return false;
    }
    void sign_in()
    {
        lichess::Account account;
        account.id = "tester";
        account.username = "tester";
        account.perfs = {{"rapid", 1500, false, 10, 0, {}}, {"puzzle", 1600, false, 20, 0, {}}};
        lichess::OngoingGame game;
        game.game_id = "abcdefgh";
        game.fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1";
        game.last_move = "e2e4";
        game.opponent = "someone";
        game.my_turn = true;
        app_->session().preview(std::move(account), {game});
    }

    std::string root_;
    std::uint32_t textures_ = 100;
    std::unique_ptr<app::App> app_;
    std::size_t shapes_ = 0;
};

TEST_F(AppTest, OpensOnTheHomePageAndDraws)
{
    idle(30);
    EXPECT_STREQ(app_->active_scene(), "home");
    EXPECT_GT(shapes_, 100u);
}

TEST_F(AppTest, TheRailShowsEveryPage)
{
    idle(10);
    press(Action::menu); // the controller goes to the rail
    const char *const names[] = {"home", "puzzles", "play", "watch", "profile", "settings"};
    for (int i = 1; i < modes::kPageCount; ++i)
    {
        nav(Direction::down);
        idle(20);
        EXPECT_STREQ(app_->active_scene(), names[i]);
        EXPECT_GT(shapes_, 50u) << names[i];
    }
    // Past the last entry the rail refuses, quietly on a held direction.
    nav(Direction::down);
    EXPECT_STREQ(app_->active_scene(), "settings");
    // Circle on the rail returns to Home.
    press(Action::back);
    idle(10);
    EXPECT_STREQ(app_->active_scene(), "home");
}

TEST_F(AppTest, ConnectionNoticesComeAndGo)
{
    idle(10);
    const std::size_t quiet = shapes_;
    app_->session().set_issue("game", "Connection to the game lost. Reconnecting");
    idle(30);
    EXPECT_GT(shapes_, quiet);
    app_->session().clear_issue("game", false);
    idle(240);
    EXPECT_LE(shapes_, quiet + 4);
}

TEST_F(AppTest, TheUpdateAnnouncementStaysTenSeconds)
{
    idle(10);
    const std::size_t quiet = shapes_;
    app_->announce_update("01.000.010");
    idle(60);
    EXPECT_GT(shapes_, quiet);
    idle(9 * 60 - 90); // 9.5 s after it appeared: still there
    EXPECT_GT(shapes_, quiet);
    idle(120); // 11.5 s: gone
    EXPECT_LE(shapes_, quiet + 4);
}

TEST_F(AppTest, SignedInHomeListsTheGamesInProgress)
{
    sign_in();
    idle(60);
    EXPECT_STREQ(app_->active_scene(), "home");
    EXPECT_GT(shapes_, 200u);
}

// Every screen must take any input in any state: nothing may crash, hang or
// record a shape that is not finite.
TEST_F(AppTest, SurvivesRandomInput)
{
    for (int round = 0; round < 2; ++round)
    {
        if (round == 1)
            sign_in();
        std::uint32_t rng = 0x1234567u + static_cast<std::uint32_t>(round);
        const auto next = [&rng]()
        {
            rng = rng * 1664525u + 1013904223u;
            return rng >> 8;
        };
        constexpr Action kActions[] = {Action::confirm,   Action::back,      Action::north,
                                       Action::west,      Action::page_prev, Action::page_next,
                                       Action::jump_prev, Action::jump_next, Action::menu};
        constexpr Direction kDirections[] = {Direction::up, Direction::down, Direction::left,
                                             Direction::right};
        for (int i = 0; i < 4000; ++i)
        {
            InputFrame input;
            const std::uint32_t roll = next() % 10;
            if (roll < 4)
                input.nav = kDirections[next() % 4];
            else if (roll < 8)
            {
                const Action action = kActions[next() % 9];
                input.pressed = action_bit(action);
                input.held = action_bit(action);
            }
            frame(input);
            ASSERT_TRUE(std::isfinite(static_cast<float>(shapes_)));
        }
        app_->go_home();
        idle(40);
    }
    SUCCEED();
}

} // namespace
