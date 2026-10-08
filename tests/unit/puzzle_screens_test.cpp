// ProsperoLichess - The puzzle screen and the Puzzles page, played with a controller.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/app.hpp"
#include "core/save_file.hpp"
#include "gfx/font.hpp"
#include "modes/page.hpp"
#include "modes/puzzle_scene.hpp"
#include "modes/scenes.hpp"
#include "puzzles/pack_source.hpp"
#include "puzzles/records.hpp"

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
using modes::PuzzleRules;
using modes::PuzzleScene;
using Phase = modes::PuzzleScene::Phase;

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

struct Report
{
    std::string id;
    bool win;
    bool rated;
};

// The offline pack, with every result the screen reports written down.
class Watched final : public puzzles::Source
{
  public:
    Watched(const puzzles::Pack *pack, std::vector<Report> *log)
        : pack_(pack, -1, 900, 1100, 0x51ull), log_(log)
    {
    }
    State next(app::Context &ctx, int target, puzzles::Puzzle *out, std::string *error) override
    {
        return pack_.next(ctx, target, out, error);
    }
    void report(app::Context &, const puzzles::Puzzle &puzzle, bool win, bool rated) override
    {
        log_->push_back({puzzle.id, win, rated});
    }

  private:
    puzzles::PackSource pack_;
    std::vector<Report> *log_;
};

class PuzzleSceneTest : public ::testing::Test
{
  protected:
    static constexpr float kFrame = 1.0f / 60.0f;

    void SetUp() override
    {
        ASSERT_TRUE(faces().ok) << "baked fonts missing";
        char root[] = "/tmp/pch-puzzle-test-XXXXXX";
        ASSERT_NE(mkdtemp(root), nullptr);
        root_ = root;
        app::Gpu gpu;
        gpu.create_texture = [this](int, int, const std::uint8_t *) { return ++textures_; };
        gpu.delete_texture = [](std::uint32_t) {};
        gpu.glass_texture = 9;
        app_ = std::make_unique<app::App>(std::move(gpu), faces().fonts, 1.0f, root_,
                                          std::string(PCH_SOURCE_DIR) + "/assets", false, false);
        ASSERT_NE(app_->context().pack, nullptr) << "offline puzzle pack missing";
        idle(10);
    }

    void frame(InputFrame input = {}, float dt = kFrame)
    {
        input.connected = true;
        app_->update(input, dt);
        app_->record_frame();
        ++frames_;
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

    // Opens a puzzle screen over the watched pack. The app owns the screen;
    // scene_ is only good while it is open.
    void open(PuzzleRules rules)
    {
        modes::PuzzleSetup setup;
        setup.title = "Test";
        setup.subtitle = "Under test";
        setup.rules = rules;
        setup.source = std::make_unique<Watched>(app_->context().pack, &log_);
        auto scene = std::make_unique<PuzzleScene>(std::move(setup));
        scene_ = scene.get();
        app_->open(std::move(scene));
        frames_ = 0;
    }

    bool wait_for_turn()
    {
        for (int i = 0; i < 600 && scene_->phase() != Phase::player; ++i)
            frame();
        return scene_->phase() == Phase::player;
    }
    void choose(chess::Square square)
    {
        int files = chess::file_of(square) - chess::file_of(scene_->cursor());
        int ranks = chess::rank_of(square) - chess::rank_of(scene_->cursor());
        if (scene_->orientation() == chess::Color::black)
        {
            files = -files;
            ranks = -ranks;
        }
        nav(files > 0 ? Direction::right : Direction::left, std::abs(files));
        nav(ranks > 0 ? Direction::up : Direction::down, std::abs(ranks));
        press(Action::confirm);
    }
    void play(const chess::Move &move)
    {
        choose(move.from);
        choose(move.to);
    }
    // Plays the solution of the puzzle on the board, replies included.
    void solve()
    {
        ASSERT_TRUE(wait_for_turn());
        const std::string id = scene_->puzzle().id;
        const int score = scene_->score();
        for (int guard = 0; guard < 16; ++guard)
        {
            chess::Move move;
            if (!wait_for_turn() || scene_->puzzle().id != id || scene_->score() != score ||
                !scene_->expected(&move))
                return;
            play(move);
            if (scene_->phase() == Phase::solved)
                return;
        }
    }
    // A legal move the puzzle does not accept.
    void blunder()
    {
        ASSERT_TRUE(wait_for_turn());
        chess::Move expected;
        ASSERT_TRUE(scene_->expected(&expected));
        chess::MoveList moves;
        scene_->position().legal_moves(moves);
        for (const chess::Move &move : moves)
        {
            if (move.promotion || move.from == expected.from ||
                scene_->position().after(move).is_checkmate())
                continue;
            play(move);
            return;
        }
        FAIL() << "no wrong move in this position";
    }

    // From the home page: shows the Puzzles page and moves the controller into it.
    void open_page()
    {
        press(Action::menu);
        nav(Direction::up, modes::kPageCount);
        nav(Direction::down, modes::kPagePuzzles);
        press(Action::confirm);
        idle(30);
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

    std::string root_;
    std::uint32_t textures_ = 100;
    std::unique_ptr<app::App> app_;
    PuzzleScene *scene_ = nullptr;
    std::vector<Report> log_;
    int frames_ = 0; // since the screen opened
};

TEST_F(PuzzleSceneTest, StormCountsSolvedPuzzlesAndPunishesMistakes)
{
    open(PuzzleRules::storm);
    solve();
    solve();
    ASSERT_TRUE(wait_for_turn());
    EXPECT_EQ(scene_->score(), 2);
    EXPECT_EQ(scene_->combo(), 2);
    ASSERT_EQ(log_.size(), 2u);
    EXPECT_TRUE(log_[0].win);
    EXPECT_TRUE(log_[0].rated);
    EXPECT_NE(log_[0].id, log_[1].id);

    const std::string before = scene_->puzzle().id;
    const float clock = scene_->clock();
    blunder();
    // A mistake costs ten seconds and the combo, and the next puzzle comes.
    EXPECT_EQ(scene_->mistakes(), 1);
    EXPECT_EQ(scene_->combo(), 0);
    EXPECT_EQ(scene_->score(), 2);
    EXPECT_LT(scene_->clock(), clock - 10.0f);
    EXPECT_GT(scene_->clock(), clock - 13.0f);
    ASSERT_TRUE(wait_for_turn());
    EXPECT_NE(scene_->puzzle().id, before);
    ASSERT_EQ(log_.size(), 3u);
    EXPECT_FALSE(log_[2].win);
    EXPECT_EQ(log_[2].id, before);
    EXPECT_EQ(puzzles::load_records(root_).solved, 2);
}

TEST_F(PuzzleSceneTest, StormComboOfFiveAddsThreeSeconds)
{
    open(PuzzleRules::storm);
    for (int i = 0; i < 5; ++i)
        solve();
    EXPECT_EQ(scene_->combo(), 5);
    // Three minutes, less the time played, plus the bonus.
    EXPECT_NEAR(scene_->clock() + static_cast<float>(frames_) * kFrame, 183.0f, 0.25f);
}

TEST_F(PuzzleSceneTest, StormEndsWhenTheClockRunsOutAndStartsAgain)
{
    open(PuzzleRules::storm);
    solve();
    ASSERT_TRUE(wait_for_turn());
    // Three minutes pass, a quarter of a second at a time.
    for (int i = 0; i < 800 && scene_->phase() != Phase::run_over; ++i)
        frame({}, 0.25f);
    ASSERT_EQ(scene_->phase(), Phase::run_over);
    EXPECT_TRUE(scene_->new_record());
    EXPECT_EQ(puzzles::load_records(root_).best_storm, 1);

    // The menu stays shut over the result, and "Play again" has the focus.
    press(Action::menu);
    EXPECT_FALSE(scene_->menu_open());
    idle(30);
    press(Action::confirm);
    EXPECT_NE(scene_->phase(), Phase::run_over);
    EXPECT_EQ(scene_->score(), 0);
    EXPECT_GT(scene_->clock(), 179.0f);
    EXPECT_STREQ(app_->active_scene(), "puzzle");
}

TEST_F(PuzzleSceneTest, StreakEndsOnAMistakeAndKeepsTheRecord)
{
    open(PuzzleRules::streak);
    solve();
    solve();
    EXPECT_EQ(scene_->score(), 2);
    blunder();
    ASSERT_EQ(scene_->phase(), Phase::run_over);
    EXPECT_TRUE(scene_->new_record());
    EXPECT_EQ(puzzles::load_records(root_).best_streak, 2);
    EXPECT_EQ(puzzles::load_records(root_).solved, 2);
    ASSERT_EQ(log_.size(), 3u);
    EXPECT_FALSE(log_[2].win);

    // The other answer leaves the screen.
    idle(30);
    nav(Direction::right);
    press(Action::confirm);
    idle(40);
    EXPECT_STREQ(app_->active_scene(), "home");
}

TEST_F(PuzzleSceneTest, StreakAllowsOneSkip)
{
    open(PuzzleRules::streak);
    ASSERT_TRUE(wait_for_turn());
    const std::string first = scene_->puzzle().id;
    press(Action::west);
    ASSERT_TRUE(wait_for_turn());
    const std::string second = scene_->puzzle().id;
    EXPECT_NE(second, first);
    EXPECT_EQ(scene_->skips(), 0);
    // The second one is refused.
    press(Action::west);
    idle(30);
    EXPECT_EQ(scene_->puzzle().id, second);
    EXPECT_EQ(scene_->phase(), Phase::player);
    EXPECT_TRUE(log_.empty());
}

TEST_F(PuzzleSceneTest, ClassicLetsThePlayerTryAgainAndReportsOnce)
{
    open(PuzzleRules::classic);
    blunder();
    EXPECT_EQ(scene_->phase(), Phase::player);
    ASSERT_EQ(log_.size(), 1u);
    EXPECT_FALSE(log_[0].win);
    EXPECT_TRUE(log_[0].rated);
    blunder();
    EXPECT_EQ(log_.size(), 1u);

    // Two hints: the piece, then the move. A third changes nothing.
    press(Action::north);
    EXPECT_EQ(scene_->hint_level(), 1);
    press(Action::north);
    press(Action::north);
    EXPECT_EQ(scene_->hint_level(), 2);

    const std::string first = scene_->puzzle().id;
    solve();
    ASSERT_EQ(scene_->phase(), Phase::solved);
    ASSERT_EQ(log_.size(), 2u);
    EXPECT_FALSE(log_[1].win);
    EXPECT_FALSE(log_[1].rated);
    EXPECT_EQ(puzzles::load_records(root_).solved, 1);

    // Cross takes the next puzzle.
    press(Action::confirm);
    ASSERT_TRUE(wait_for_turn());
    const std::string second = scene_->puzzle().id;
    EXPECT_NE(second, first);

    // The solution plays itself; nothing more is reported.
    press(Action::west);
    EXPECT_EQ(scene_->phase(), Phase::showing);
    for (int i = 0; i < 900 && scene_->phase() != Phase::solved; ++i)
        frame();
    ASSERT_EQ(scene_->phase(), Phase::solved);
    EXPECT_EQ(log_.size(), 2u);

    // Square plays the same puzzle again, for practice.
    press(Action::west);
    ASSERT_TRUE(wait_for_turn());
    EXPECT_EQ(scene_->puzzle().id, second);
    solve();
    EXPECT_EQ(scene_->phase(), Phase::solved);
    EXPECT_EQ(log_.size(), 2u);
    EXPECT_EQ(puzzles::load_records(root_).solved, 1);
}

TEST_F(PuzzleSceneTest, TheMenuPausesFlipsAndLeaves)
{
    open(PuzzleRules::storm);
    ASSERT_TRUE(wait_for_turn());
    const chess::Color side = scene_->orientation();
    press(Action::menu);
    ASSERT_TRUE(scene_->menu_open());
    const float clock = scene_->clock();
    idle(60);
    EXPECT_FLOAT_EQ(scene_->clock(), clock);

    // Resume, Flip board, End run, Return to menu.
    nav(Direction::down);
    press(Action::confirm);
    EXPECT_FALSE(scene_->menu_open());
    EXPECT_NE(scene_->orientation(), side);
    idle(30);
    EXPECT_LT(scene_->clock(), clock);

    press(Action::menu);
    ASSERT_TRUE(scene_->menu_open());
    press(Action::back);
    EXPECT_FALSE(scene_->menu_open());
    EXPECT_STREQ(app_->active_scene(), "puzzle");

    press(Action::menu);
    nav(Direction::down, 3);
    press(Action::confirm);
    idle(40);
    EXPECT_STREQ(app_->active_scene(), "home");
}

// Every mode must take any input in any state, time running fast enough for
// Storm's clock to run out: nothing may crash or hang.
TEST_F(PuzzleSceneTest, EveryModeSurvivesRandomInput)
{
    constexpr Action kActions[] = {
        Action::confirm,   Action::back,      Action::north,     Action::west, Action::page_prev,
        Action::page_next, Action::jump_prev, Action::jump_next, Action::menu, Action::touch};
    constexpr Direction kDirections[] = {Direction::up, Direction::down, Direction::left,
                                         Direction::right};
    std::uint32_t rng = 0x2468aceu;
    const auto next = [&rng]()
    {
        rng = rng * 1664525u + 1013904223u;
        return rng >> 8;
    };
    for (int mode = 0; mode < 5; ++mode)
    {
        for (int i = 0; i < 2500; ++i)
        {
            // Circle closes the screen now and then: open it again.
            if (std::string(app_->active_scene()) != "puzzle")
            {
                app::Context &ctx = app_->context();
                switch (mode)
                {
                case 0:
                    app_->open(modes::make_puzzle_storm(ctx));
                    break;
                case 1:
                    app_->open(modes::make_puzzle_streak(ctx));
                    break;
                case 2:
                    app_->open(modes::make_puzzle_theme(ctx, i % 40 - 1, 600 + (i % 5) * 500));
                    break;
                case 3:
                    app_->open(modes::make_daily_puzzle(ctx));
                    break;
                default:
                    app_->open(modes::make_puzzle_training(ctx));
                    break;
                }
            }
            InputFrame input;
            const std::uint32_t roll = next() % 10;
            if (roll < 5)
            {
                input.nav = kDirections[next() % 4];
            }
            else if (roll < 9)
            {
                const Action action = kActions[next() % 10];
                input.pressed = action_bit(action);
                input.held = action_bit(action);
            }
            frame(input, next() % 50 == 0 ? 2.0f : kFrame);
        }
        app_->go_home();
        idle(40);
        EXPECT_STREQ(app_->active_scene(), "home");
    }
}

class PuzzlesPageTest : public PuzzleSceneTest
{
};

TEST_F(PuzzlesPageTest, LichessModesRefuseOfflineAndTheOthersOpen)
{
    open_page();
    ASSERT_STREQ(app_->active_scene(), "puzzles");
    // Daily Puzzle needs the network: Cross is refused, softly.
    InputFrame confirm;
    confirm.pressed = action_bit(Action::confirm);
    confirm.held = action_bit(Action::confirm);
    frame(confirm);
    EXPECT_TRUE(asked(audio::Cue::error));
    idle(30);
    EXPECT_STREQ(app_->active_scene(), "puzzles");

    // Puzzle Streak is the third card.
    nav(Direction::right, 2);
    press(Action::confirm);
    idle(40);
    EXPECT_STREQ(app_->active_scene(), "puzzle");
    press(Action::menu);
    nav(Direction::down, 3);
    press(Action::confirm);
    idle(40);
    EXPECT_STREQ(app_->active_scene(), "puzzles");
}

TEST_F(PuzzlesPageTest, AThemeOpensItsPuzzles)
{
    open_page();
    press(Action::page_next); // Themes
    press(Action::jump_next); // Harder
    press(Action::jump_next); // Hardest
    press(Action::jump_next); // refused: there is nothing harder
    nav(Direction::down, 9);
    nav(Direction::right, 3);
    nav(Direction::right); // refused at the edge
    idle(20);
    press(Action::confirm);
    idle(40);
    EXPECT_STREQ(app_->active_scene(), "puzzle");
}

TEST_F(PuzzlesPageTest, LeftAndCircleHandTheControllerToTheRail)
{
    open_page();
    nav(Direction::left);
    // The rail has it now: down shows the next page.
    nav(Direction::down);
    idle(20);
    EXPECT_STREQ(app_->active_scene(), "play");

    nav(Direction::up);
    press(Action::confirm);
    idle(20);
    ASSERT_STREQ(app_->active_scene(), "puzzles");
    press(Action::page_next);
    press(Action::back);
    nav(Direction::down);
    idle(20);
    EXPECT_STREQ(app_->active_scene(), "play");
}

TEST_F(PuzzlesPageTest, ShoulderButtonsTurnTheTabsFromTheRail)
{
    open_page();
    nav(Direction::left);     // the rail has the controller
    press(Action::page_next); // Themes, and the page takes the controller
    idle(20);
    ASSERT_STREQ(app_->active_scene(), "puzzles");
    // On the Modes tab Cross is refused offline; on Themes it opens a theme.
    press(Action::confirm);
    idle(40);
    EXPECT_STREQ(app_->active_scene(), "puzzle");
}

TEST_F(PuzzlesPageTest, ShoulderButtonsLeaveTheRailAloneOnAPageWithoutTabs)
{
    press(Action::menu); // Home's page hands the controller to the rail
    press(Action::page_next);
    nav(Direction::down); // still the rail: the next page shows
    idle(20);
    EXPECT_STREQ(app_->active_scene(), "puzzles");
}

} // namespace
