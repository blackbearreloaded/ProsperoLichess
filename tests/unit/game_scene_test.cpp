// ProsperoLichess - The game screens: Pass & Play, scripted Lichess games, waiting for one.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/app.hpp"
#include "core/save_file.hpp"
#include "gfx/font.hpp"
#include "lichess/session.hpp"
#include "modes/game_scene.hpp"
#include "modes/game_script_link.hpp"
#include "modes/scenes.hpp"

#include <gtest/gtest.h>

#include <cmath>
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

class GameSceneTest : public ::testing::Test
{
  protected:
    static constexpr float kFrame = 1.0f / 60.0f;

    void SetUp() override
    {
        ASSERT_TRUE(faces().ok) << "baked fonts missing";
        char root[] = "/tmp/pch-game-test-XXXXXX";
        ASSERT_NE(mkdtemp(root), nullptr);
        root_ = root;
        app::Gpu gpu;
        gpu.create_texture = [this](int, int, const std::uint8_t *) { return ++textures_; };
        gpu.delete_texture = [](std::uint32_t) {};
        gpu.glass_texture = 9;
        app_ = std::make_unique<app::App>(std::move(gpu), faces().fonts, 1.0f, root_,
                                          std::string(PCH_SOURCE_DIR) + "/assets", false, false);
        idle(5);
    }

    // One frame: update, then record what would be drawn. Cues asked for
    // since the last forget() are remembered.
    void frame(InputFrame input = {})
    {
        input.connected = true;
        app_->update(input, kFrame);
        for (const audio::CueEvent &event : app_->feedback().cues)
            heard_.push_back(event.cue);
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
    void hold(Action action, int frames)
    {
        InputFrame input;
        input.pressed = action_bit(action);
        input.held = action_bit(action);
        frame(input);
        input.pressed = 0;
        for (int i = 1; i < frames; ++i)
            frame(input);
        idle(2);
    }
    bool heard(audio::Cue cue) const
    {
        for (const audio::Cue one : heard_)
        {
            if (one == cue)
                return true;
        }
        return false;
    }
    void forget()
    {
        heard_.clear();
    }

    void open_local()
    {
        app_->open(modes::make_local_game(app_->context()));
        idle(70);
    }
    // 1. e4 from the opening position: the cursor starts on e2.
    void play_e4()
    {
        press(Action::confirm);
        nav(Direction::up, 2);
        press(Action::confirm);
    }
    std::string saved_moves() const
    {
        std::string data;
        if (!save::read_file(root_ + "/passplay.sav", &data))
            return "<none>";
        const save::Decoded decoded = save::decode(save::Kind::game, data);
        return decoded.ok ? decoded.payload : "<bad>";
    }

    // A rated game after 1. e4 e5 2. Nf3 Nc6, the player with White to move.
    std::shared_ptr<modes::GameScript> open_online()
    {
        auto script = std::make_shared<modes::GameScript>();
        script->white.name = "tester";
        script->white.rating = 1500;
        script->black.name = "someone";
        script->black.rating = 1600;
        script->moves = "e2e4 e7e5 g1f3 b8c6";
        app_->open(modes::make_linked_game(modes::make_script_link(script)));
        idle(40);
        return script;
    }
    void sign_in()
    {
        lichess::Account account;
        account.id = "tester";
        account.username = "tester";
        app_->session().preview(std::move(account), {});
    }

    std::string root_;
    std::uint32_t textures_ = 100;
    std::unique_ptr<app::App> app_;
    std::size_t shapes_ = 0;
    std::vector<audio::Cue> heard_;
};

// ---- Pass & Play ------------------------------------------------------------

TEST_F(GameSceneTest, LocalGameOpensWithTheStartCue)
{
    open_local();
    EXPECT_STREQ(app_->active_scene(), "pass-and-play");
    EXPECT_TRUE(heard(audio::Cue::game_start));
    EXPECT_GT(shapes_, 100u);
    EXPECT_FALSE(modes::has_saved_local_game(app_->context()));
}

TEST_F(GameSceneTest, LocalMovesAreSavedAndResumed)
{
    open_local();
    forget();
    play_e4();
    EXPECT_TRUE(heard(audio::Cue::move));
    EXPECT_EQ(saved_moves(), "e2e4");
    EXPECT_TRUE(modes::has_saved_local_game(app_->context()));

    // Leaving through the menu keeps the save; a new screen resumes it and
    // the next move joins the first.
    press(Action::menu);
    nav(Direction::up); // the menu wraps: the last row returns to the home screen
    press(Action::confirm);
    idle(40);
    EXPECT_STREQ(app_->active_scene(), "home");
    open_local();
    // Black to move, the board turned: e7 is under the cursor's start.
    press(Action::confirm);
    nav(Direction::up, 2);
    press(Action::confirm);
    EXPECT_EQ(saved_moves(), "e2e4 e7e5");
}

// The Play page shows the saved game as it was left: it reads the moves here.
TEST_F(GameSceneTest, TheSavedGameCanBeReadWithoutOpeningIt)
{
    std::string moves = "untouched";
    EXPECT_FALSE(modes::saved_local_game(app_->context(), &moves));
    EXPECT_EQ(moves, "untouched");
    open_local();
    play_e4();
    EXPECT_TRUE(modes::saved_local_game(app_->context(), &moves));
    EXPECT_EQ(moves, "e2e4");
    EXPECT_TRUE(modes::has_saved_local_game(app_->context()));
}

TEST_F(GameSceneTest, UndoFromTheActionRowTakesTheMoveBack)
{
    open_local();
    play_e4();
    idle(60); // the board turns for Black
    press(Action::north);
    forget();
    press(Action::confirm); // Undo is the first button
    EXPECT_EQ(saved_moves(), "");
    EXPECT_FALSE(modes::has_saved_local_game(app_->context()));
    // Circle hands the controller back to the board.
    press(Action::back);
    EXPECT_TRUE(heard(audio::Cue::back));
    EXPECT_STREQ(app_->active_scene(), "pass-and-play");
}

TEST_F(GameSceneTest, NewGameFromTheMenuClearsTheSave)
{
    open_local();
    play_e4();
    press(Action::menu);
    nav(Direction::down, 4); // Resume, Take back, Flip, Auto-flip, New game
    forget();
    press(Action::confirm);
    idle(10);
    EXPECT_TRUE(heard(audio::Cue::game_start));
    EXPECT_FALSE(modes::has_saved_local_game(app_->context()));
}

TEST_F(GameSceneTest, HoldingSquareResignsALocalGame)
{
    open_local();
    play_e4();
    idle(60);
    // A tap does nothing but ask for a hold.
    press(Action::west);
    idle(60);
    EXPECT_EQ(saved_moves(), "e2e4");
    forget();
    hold(Action::west, 100);
    // The game is over: nothing is left to resume, and the result arrives.
    EXPECT_EQ(saved_moves(), "");
    idle(60);
    EXPECT_TRUE(heard(audio::Cue::victory));
    // New game is the result's first answer.
    forget();
    press(Action::confirm);
    idle(10);
    EXPECT_TRUE(heard(audio::Cue::game_start));
}

TEST_F(GameSceneTest, ScholarsMateEndsTheGame)
{
    open_local();
    // Every move by cursor: the board turns after each one, so "up" is
    // always away from the player to move.
    const auto move = [&](int right, int up, int to_right, int to_up)
    {
        nav(right >= 0 ? Direction::right : Direction::left, std::abs(right));
        nav(up >= 0 ? Direction::up : Direction::down, std::abs(up));
        press(Action::confirm);
        nav(to_right >= 0 ? Direction::right : Direction::left, std::abs(to_right));
        nav(to_up >= 0 ? Direction::up : Direction::down, std::abs(to_up));
        press(Action::confirm);
        idle(60);
    };
    move(0, 0, 0, 2);   // e4; the cursor lands on e4, d5 for Black
    move(-1, -2, 0, 2); // e5 from e7
    move(2, -3, -3, 3); // Bc4 from f1 (the cursor was on d4)
    move(4, -3, -1, 2); // Nc6 from b8 (the cursor was on f5)
    move(-2, -2, 4, 4); // Qh5 from d1 (the cursor was on f3)
    forget();
    move(-6, -4, 1, 2); // Nf6 from g8 (the cursor was on a4)
    move(5, 2, -2, 2);  // Qxf7# from h5 (the cursor was on c3)
    idle(60);
    EXPECT_EQ(saved_moves(), "");
    EXPECT_TRUE(heard(audio::Cue::check));
    EXPECT_TRUE(heard(audio::Cue::victory));
    // Review game, step to the end, and back out to the home screen.
    nav(Direction::right);
    press(Action::confirm);
    forget();
    press(Action::jump_next);
    EXPECT_TRUE(heard(audio::Cue::cursor));
    press(Action::confirm);
    press(Action::menu);
    nav(Direction::up);
    press(Action::confirm);
    idle(40);
    EXPECT_STREQ(app_->active_scene(), "home");
}

// ---- a Lichess game ---------------------------------------------------------

TEST_F(GameSceneTest, OnlineGameLoadsFailsAndGoesBack)
{
    auto script = std::make_shared<modes::GameScript>();
    script->ready = false;
    app_->open(modes::make_linked_game(modes::make_script_link(script)));
    idle(30);
    EXPECT_STREQ(app_->active_scene(), "online-game");
    EXPECT_GT(shapes_, 20u); // the skeleton, not a blank screen
    script->error = "Game not found";
    idle(30);
    EXPECT_GT(shapes_, 5u);
    press(Action::back);
    idle(40);
    EXPECT_STREQ(app_->active_scene(), "home");
}

TEST_F(GameSceneTest, OnlineMovesGoToTheLinkAndPremovesFire)
{
    const auto script = open_online();
    EXPECT_TRUE(heard(audio::Cue::game_start));
    // 3. d4: the cursor starts on e2.
    nav(Direction::left);
    press(Action::confirm);
    nav(Direction::up, 2);
    press(Action::confirm);
    EXPECT_TRUE(script->asked("move d2d4"));
    // A premove while the opponent thinks: c2-c3.
    nav(Direction::left);
    nav(Direction::down, 2);
    forget();
    press(Action::confirm);
    nav(Direction::up);
    press(Action::confirm);
    EXPECT_TRUE(heard(audio::Cue::premove));
    EXPECT_FALSE(script->asked("move c2c3"));
    script->moves += " e5d4";
    script->touch();
    idle(5);
    EXPECT_TRUE(script->asked("move c2c3"));
}

TEST_F(GameSceneTest, ARefusedMoveIsTakenBack)
{
    const auto script = open_online();
    script->echo_moves = false;
    nav(Direction::left);
    press(Action::confirm);
    nav(Direction::up, 2);
    forget();
    press(Action::confirm);
    EXPECT_TRUE(script->asked("move d2d4"));
    script->refused = true;
    script->refusal = "Not your turn";
    idle(3);
    EXPECT_TRUE(heard(audio::Cue::illegal));
    // The pawn is back on d2: the same move can be made again.
    script->sent.clear();
    nav(Direction::down, 2);
    press(Action::confirm);
    nav(Direction::up, 2);
    press(Action::confirm);
    EXPECT_TRUE(script->asked("move d2d4"));
}

TEST_F(GameSceneTest, HoldingSquareResignsAndATapDoesNot)
{
    const auto script = open_online();
    press(Action::west);
    idle(30);
    EXPECT_TRUE(script->sent.empty());
    hold(Action::west, 100);
    EXPECT_TRUE(script->asked("resign"));
    EXPECT_FALSE(script->asked("abort"));
}

TEST_F(GameSceneTest, AGameThatHasBarelyBegunIsAborted)
{
    auto script = std::make_shared<modes::GameScript>();
    script->moves = "e2e4";
    script->me = chess::Color::black;
    app_->open(modes::make_linked_game(modes::make_script_link(script)));
    idle(40);
    // Draw and takeback are not on offer yet: the row refuses them.
    press(Action::north);
    press(Action::confirm);
    EXPECT_TRUE(script->sent.empty());
    hold(Action::west, 100);
    EXPECT_TRUE(script->asked("abort"));
    EXPECT_FALSE(script->asked("resign"));
}

TEST_F(GameSceneTest, ResignWithoutTheSafeguardTakesOnePress)
{
    app_->context().settings->confirm_resign = false;
    const auto script = open_online();
    press(Action::west);
    EXPECT_TRUE(script->asked("resign"));
}

TEST_F(GameSceneTest, TheActionRowOffersADrawAndATakeback)
{
    const auto script = open_online();
    press(Action::north);
    press(Action::confirm);
    EXPECT_TRUE(script->asked("draw yes"));
    nav(Direction::right);
    press(Action::confirm);
    EXPECT_TRUE(script->asked("takeback yes"));
    // While our offer waits, its button does not send it again.
    script->sent.clear();
    script->draw_offer[0] = true;
    script->touch();
    idle(3);
    nav(Direction::left);
    press(Action::confirm);
    EXPECT_TRUE(script->sent.empty());
}

TEST_F(GameSceneTest, TheOpponentsOffersAreAnswered)
{
    const auto script = open_online();
    forget();
    script->draw_offer[1] = true;
    script->touch();
    idle(10);
    EXPECT_TRUE(heard(audio::Cue::challenge));
    // The question opens on Decline; Accept is beside it.
    nav(Direction::right);
    press(Action::confirm);
    EXPECT_TRUE(script->asked("draw yes"));

    script->sent.clear();
    script->draw_offer[1] = false;
    script->takeback_offer[1] = true;
    script->touch();
    idle(10);
    press(Action::back); // Circle declines
    EXPECT_TRUE(script->asked("takeback no"));

    // An offer that is withdrawn takes its question with it: the board has
    // the controller again.
    script->sent.clear();
    script->takeback_offer[1] = false;
    script->touch();
    idle(10);
    script->draw_offer[1] = true;
    script->touch();
    idle(10);
    script->draw_offer[1] = false;
    script->touch();
    idle(10);
    nav(Direction::left);
    press(Action::confirm);
    nav(Direction::up, 2);
    press(Action::confirm);
    EXPECT_TRUE(script->asked("move d2d4"));
    EXPECT_FALSE(script->asked("draw yes"));
    EXPECT_FALSE(script->asked("draw no"));
}

TEST_F(GameSceneTest, TheMenuAbortsClaimsAndLeaves)
{
    const auto script = open_online();
    script->opponent_gone = 0;
    idle(3);
    // Resume, Offer draw, Propose takeback, Claim victory, Flip board, Return.
    press(Action::menu);
    nav(Direction::down, 3);
    press(Action::confirm);
    EXPECT_TRUE(script->asked("claim"));
    press(Action::menu);
    nav(Direction::down);
    press(Action::confirm);
    EXPECT_TRUE(script->asked("draw yes"));
    press(Action::menu);
    nav(Direction::up);
    press(Action::confirm);
    idle(40);
    EXPECT_STREQ(app_->active_scene(), "home");
}

TEST_F(GameSceneTest, LowTimeWarnsAndTheLastSecondsTick)
{
    const auto script = open_online();
    forget();
    script->clock_ms[0] = 15000;
    idle(3);
    EXPECT_TRUE(heard(audio::Cue::low_time));
    EXPECT_FALSE(heard(audio::Cue::clock_tick));
    script->clock_ms[0] = 8500;
    idle(3);
    EXPECT_TRUE(heard(audio::Cue::clock_tick));
}

TEST_F(GameSceneTest, AWonGameShowsItsResultAndCanBeReviewed)
{
    const auto script = open_online();
    forget();
    script->status = modes::GameStatus::resign;
    script->winner = 0;
    script->touch();
    idle(90);
    EXPECT_TRUE(heard(audio::Cue::victory));
    // The game is over: holding Square asks for nothing any more.
    press(Action::confirm); // Review game
    hold(Action::west, 100);
    EXPECT_TRUE(script->sent.empty());
    // L2 steps back; at the first position it answers softly and stays.
    forget();
    press(Action::jump_prev);
    EXPECT_TRUE(heard(audio::Cue::cursor));
    forget();
    press(Action::jump_prev);
    EXPECT_TRUE(heard(audio::Cue::error));
    press(Action::back);
    // The action row brings the result back; Menu leaves.
    press(Action::north);
    press(Action::confirm);
    nav(Direction::right);
    press(Action::confirm);
    idle(40);
    EXPECT_STREQ(app_->active_scene(), "home");
}

TEST_F(GameSceneTest, ALostGameSoundsLikeOne)
{
    const auto script = open_online();
    forget();
    script->status = modes::GameStatus::outoftime;
    script->winner = 1;
    script->touch();
    idle(90);
    EXPECT_TRUE(heard(audio::Cue::defeat));
    EXPECT_FALSE(heard(audio::Cue::victory));
}

// The scripted states under random input: nothing may crash or hang.
TEST_F(GameSceneTest, OnlineGameSurvivesRandomInput)
{
    const auto script = open_online();
    std::uint32_t rng = 0xbeef01u;
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
    for (int i = 0; i < 6000; ++i)
    {
        if (std::string(app_->active_scene()) != "online-game")
        {
            // The input found the way out: open the game again.
            app_->open(modes::make_linked_game(modes::make_script_link(script)));
        }
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
        // The server changes its mind now and then.
        switch (next() % 400)
        {
        case 0:
            script->draw_offer[1] = !script->draw_offer[1];
            break;
        case 1:
            script->takeback_offer[1] = !script->takeback_offer[1];
            break;
        case 2:
            script->connected = !script->connected;
            break;
        case 3:
            script->opponent_gone = script->opponent_gone < 0 ? 3 : -1;
            break;
        case 4:
            script->clock_ms[0] = 4000;
            break;
        case 5:
            script->moves = "e2e4 e7e5";
            break;
        case 6:
            script->status = script->status == modes::GameStatus::playing
                                 ? modes::GameStatus::draw
                                 : modes::GameStatus::playing;
            break;
        case 7:
            script->refused = true;
            script->refusal = "Refused";
            break;
        default:
            break;
        }
        script->touch();
        frame(input);
        ASSERT_TRUE(std::isfinite(static_cast<float>(shapes_)));
    }
    SUCCEED();
}

// ---- waiting for a game -----------------------------------------------------

TEST_F(GameSceneTest, ASeekWithoutAnAccountSaysSoAndGoesBack)
{
    forget();
    app_->open(modes::make_seek("rated=true&time=10&increment=5", "Rapid 10+5"));
    idle(20);
    EXPECT_STREQ(app_->active_scene(), "seek");
    EXPECT_TRUE(heard(audio::Cue::error));
    press(Action::confirm);
    idle(40);
    EXPECT_STREQ(app_->active_scene(), "home");
}

TEST_F(GameSceneTest, ASeekWaitsAndCancels)
{
    sign_in();
    forget();
    app_->open(modes::make_seek("rated=true&time=10&increment=5", "Rapid 10+5"));
    idle(120);
    EXPECT_STREQ(app_->active_scene(), "seek");
    EXPECT_FALSE(heard(audio::Cue::error));
    EXPECT_GT(shapes_, 20u);
    // Cross is not a way out while it searches; Circle cancels.
    press(Action::confirm);
    EXPECT_STREQ(app_->active_scene(), "seek");
    press(Action::back);
    idle(40);
    EXPECT_STREQ(app_->active_scene(), "home");
}

TEST_F(GameSceneTest, AnAiGameWaitsAndCancels)
{
    sign_in();
    app_->open(modes::make_ai_game("level=3&color=random", "Stockfish level 3"));
    idle(60);
    EXPECT_STREQ(app_->active_scene(), "ai-start");
    press(Action::back);
    idle(40);
    EXPECT_STREQ(app_->active_scene(), "home");
}

} // namespace
