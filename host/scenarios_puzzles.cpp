// ProsperoLichess - Host snapshots: the Puzzles page and the puzzle screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/strings.hpp"
#include "modes/page.hpp"
#include "modes/puzzle_scene.hpp"
#include "modes/scenes.hpp"
#include "puzzles/pack.hpp"
#include "puzzles/pack_source.hpp"
#include "puzzles/records.hpp"
#include "scenarios.hpp"

#include <cstdlib>

namespace pch::host
{

namespace
{

using modes::PuzzleScene;

// Opens a puzzle screen and keeps hold of it, so the script can read what
// the puzzle expects. The app owns it: it is gone once the screen closes.
PuzzleScene *open(Run &run, std::unique_ptr<app::Scene> scene)
{
    PuzzleScene *puzzle = static_cast<PuzzleScene *>(scene.get());
    run.open(std::move(scene));
    return puzzle;
}

// The same, with a picture a few frames in: the screen while it assembles.
PuzzleScene *open_arriving(Run &run, std::unique_ptr<app::Scene> scene)
{
    PuzzleScene *puzzle = static_cast<PuzzleScene *>(scene.get());
    run.app.open(std::move(scene));
    run.idle(14);
    run.shot("arriving");
    run.idle(16);
    return puzzle;
}

// The Puzzles page as the rail shows it, with a picture a few frames after it
// was entered, then with the controller in it.
void puzzles_page(Run &run, const char *arriving = nullptr)
{
    run.press(Action::menu);
    run.nav(Direction::up, modes::kPageCount);
    run.nav(Direction::down, modes::kPagePuzzles - 1);
    // The last step shows the page: its parts are still on their way.
    InputFrame frame;
    frame.nav = Direction::down;
    frame.connected = true;
    run.app.update(frame, 1.0f / 60.0f);
    if (arriving != nullptr)
    {
        run.idle(16);
        run.shot(arriving);
    }
    run.press(Action::confirm);
    run.idle(120);
}

void keep_records(Run &run, int streak, int storm, int solved)
{
    puzzles::Records records;
    records.best_streak = streak;
    records.best_storm = storm;
    records.solved = solved;
    puzzles::save_records(run.app.context().data_root, records);
}

// Walks the board cursor to a square and presses Cross on it.
void choose(Run &run, const PuzzleScene &puzzle, chess::Square square)
{
    int files = chess::file_of(square) - chess::file_of(puzzle.cursor());
    int ranks = chess::rank_of(square) - chess::rank_of(puzzle.cursor());
    if (puzzle.orientation() == chess::Color::black)
    {
        files = -files;
        ranks = -ranks;
    }
    run.nav(files > 0 ? Direction::right : Direction::left, std::abs(files));
    run.nav(ranks > 0 ? Direction::up : Direction::down, std::abs(ranks));
    run.press(Action::confirm);
}

void play(Run &run, const PuzzleScene &puzzle, const chess::Move &move)
{
    choose(run, puzzle, move.from);
    choose(run, puzzle, move.to);
    // Without auto-queen the picker opens on the queen.
    if (puzzle.promotion_open())
        run.press(Action::confirm);
}

bool wait_for_turn(Run &run, const PuzzleScene &puzzle)
{
    for (int frame = 0; frame < 600 && puzzle.phase() != PuzzleScene::Phase::player; ++frame)
        run.idle(1);
    return puzzle.phase() == PuzzleScene::Phase::player;
}

// Plays the solution of the puzzle on the board, replies included.
void solve(Run &run, const PuzzleScene &puzzle)
{
    // In a run the next puzzle arrives a moment after the last one is solved.
    if (!wait_for_turn(run, puzzle))
        return;
    const std::string id = puzzle.puzzle().id;
    const int score = puzzle.score();
    for (int guard = 0; guard < 16; ++guard)
    {
        chess::Move move;
        if (!wait_for_turn(run, puzzle) || puzzle.puzzle().id != id || puzzle.score() != score ||
            !puzzle.expected(&move))
            return;
        play(run, puzzle, move);
        if (puzzle.phase() == PuzzleScene::Phase::solved)
            return;
    }
}

// A legal move the puzzle does not accept.
void blunder(Run &run, const PuzzleScene &puzzle)
{
    if (!wait_for_turn(run, puzzle))
        return;
    chess::Move expected;
    puzzle.expected(&expected);
    chess::MoveList moves;
    puzzle.position().legal_moves(moves);
    for (const chess::Move &move : moves)
    {
        if (move.promotion || move.from == expected.from ||
            puzzle.position().after(move).is_checkmate())
            continue;
        play(run, puzzle, move);
        return;
    }
}

// The offline pack with a puzzle rating beside it, as Puzzle Training shows
// for a signed-in player.
class RatedPack final : public puzzles::PackSource
{
  public:
    using puzzles::PackSource::PackSource;
    void report(app::Context &, const puzzles::Puzzle &, bool win, bool) override
    {
        change_ = win ? 9 : -11;
        rating_ += change_;
    }
    int player_rating() const override
    {
        return rating_;
    }
    int rating_change() const override
    {
        return change_;
    }

  private:
    int rating_ = 2015;
    int change_ = 0;
};

// A source that cannot deliver.
class BrokenSource final : public puzzles::Source
{
  public:
    State next(app::Context &, int, puzzles::Puzzle *, std::string *error) override
    {
        *error = tr("No connection to Lichess");
        return State::error;
    }
};

// A source that never answers.
class SlowSource final : public puzzles::Source
{
  public:
    State next(app::Context &, int, puzzles::Puzzle *, std::string *) override
    {
        return State::loading;
    }
};

std::unique_ptr<app::Scene> training(std::unique_ptr<puzzles::Source> source)
{
    modes::PuzzleSetup setup;
    setup.title = tr("Puzzle Training");
    setup.subtitle = tr("Rated on your Lichess account");
    setup.kind = modes::PuzzleKind::training;
    setup.source = std::move(source);
    return std::make_unique<PuzzleScene>(std::move(setup));
}

std::unique_ptr<app::Scene> fork_puzzles(app::Context &ctx)
{
    return modes::make_puzzle_theme(ctx, ctx.pack != nullptr ? ctx.pack->theme_index("fork") : -1,
                                    1450);
}

} // namespace

void add_puzzle_scenarios(Scenarios &all)
{
    // ---- the Puzzles page ----
    // Signed in: today's puzzle against the player's rating. "arriving" is the
    // page a quarter of a second after the rail showed it.
    all.push_back({"puzzles", [](Run &run) { puzzles_page(run, "arriving"); }, true});
    // Offline: the Lichess modes are dimmed and say why; Cross on one refuses.
    all.push_back({"puzzles-offline", [](Run &run)
                   {
                       puzzles_page(run);
                       run.press(Action::confirm);
                       run.idle(2);
                       run.shot("refused");
                       run.idle(60);
                   }});
    // No rating to show: the figure, its line and its tile say so honestly.
    all.push_back({"puzzles-offline-training", [](Run &run)
                   {
                       puzzles_page(run);
                       run.nav(Direction::right);
                       run.idle(120);
                   }});
    // The focus moves along the tiles: the hero gives way to the next mode.
    all.push_back({"puzzles-training",
                   [](Run &run)
                   {
                       keep_records(run, 14, 27, 412);
                       puzzles_page(run);
                       run.nav(Direction::right);
                       run.idle(4);
                       run.shot("moving");
                       run.idle(120);
                   },
                   true});
    all.push_back({"puzzles-streak-card", [](Run &run)
                   {
                       keep_records(run, 14, 27, 412);
                       puzzles_page(run);
                       run.nav(Direction::right, 2);
                       run.idle(120);
                   }});
    all.push_back({"puzzles-storm-card", [](Run &run)
                   {
                       keep_records(run, 14, 27, 412);
                       puzzles_page(run);
                       run.nav(Direction::right, 3);
                       run.idle(120);
                       // The far edge refuses: the tile and its ring shake it off.
                       run.nav(Direction::right);
                       run.idle(1);
                       run.shot("edge");
                       run.idle(60);
                   }});
    all.push_back({"puzzles-themes",
                   [](Run &run)
                   {
                       puzzles_page(run);
                       run.press(Action::page_next);
                       run.idle(5);
                       run.shot("arriving");
                       run.idle(90);
                   },
                   true});
    all.push_back({"puzzles-themes-harder",
                   [](Run &run)
                   {
                       puzzles_page(run);
                       run.press(Action::page_next);
                       run.press(Action::jump_next);
                       run.nav(Direction::down, 7);
                       run.nav(Direction::right, 2);
                       run.idle(90);
                   },
                   true});
    // The last choice: the plate has glided to the end, and R2 is refused.
    all.push_back({"puzzles-themes-hardest",
                   [](Run &run)
                   {
                       puzzles_page(run);
                       run.press(Action::page_next);
                       run.press(Action::jump_next);
                       run.press(Action::jump_next);
                       run.nav(Direction::down, 2);
                       run.nav(Direction::right);
                       run.idle(60);
                       run.press(Action::jump_next);
                       run.shot("refused");
                       run.idle(60);
                   },
                   true});

    // ---- Puzzle Storm ----
    all.push_back({"puzzle-storm", [](Run &run)
                   {
                       keep_records(run, 0, 27, 412);
                       PuzzleScene *puzzle =
                           open_arriving(run, modes::make_puzzle_storm(run.app.context()));
                       for (int i = 0; i < 4; ++i)
                           solve(run, *puzzle);
                       run.shot("solved");
                       wait_for_turn(run, *puzzle);
                       run.idle(40);
                   }});
    all.push_back({"puzzle-storm-combo", [](Run &run)
                   {
                       PuzzleScene *puzzle = open(run, modes::make_puzzle_storm(run.app.context()));
                       for (int i = 0; i < 5; ++i)
                           solve(run, *puzzle);
                       // The fifth in a row adds three seconds.
                       run.idle(8);
                   }});
    all.push_back({"puzzle-storm-wrong", [](Run &run)
                   {
                       PuzzleScene *puzzle = open(run, modes::make_puzzle_storm(run.app.context()));
                       solve(run, *puzzle);
                       solve(run, *puzzle);
                       blunder(run, *puzzle);
                       run.idle(6);
                   }});
    // The last seconds: the clock turns to the warning colour.
    all.push_back({"puzzle-storm-low", [](Run &run)
                   {
                       keep_records(run, 0, 27, 412);
                       PuzzleScene *puzzle = open(run, modes::make_puzzle_storm(run.app.context()));
                       for (int i = 0; i < 3; ++i)
                           solve(run, *puzzle);
                       blunder(run, *puzzle);
                       solve(run, *puzzle);
                       wait_for_turn(run, *puzzle);
                       for (int frame = 0; frame < 60 * 180 && puzzle->clock() > 7.4f; ++frame)
                           run.idle(1);
                   }});
    all.push_back({"puzzle-storm-over", [](Run &run)
                   {
                       keep_records(run, 0, 27, 0);
                       PuzzleScene *puzzle = open(run, modes::make_puzzle_storm(run.app.context()));
                       for (int i = 0; i < 3; ++i)
                           solve(run, *puzzle);
                       wait_for_turn(run, *puzzle);
                       run.press(Action::north); // End run
                       run.idle(120);
                   }});
    // A first run: any score is a record.
    all.push_back({"puzzle-storm-record", [](Run &run)
                   {
                       PuzzleScene *puzzle = open(run, modes::make_puzzle_storm(run.app.context()));
                       for (int i = 0; i < 6; ++i)
                           solve(run, *puzzle);
                       wait_for_turn(run, *puzzle);
                       run.press(Action::north); // End run
                       run.idle(30);
                       run.shot("arriving");
                       run.idle(120);
                   }});

    // ---- Puzzle Streak ----
    all.push_back({"puzzle-streak", [](Run &run)
                   {
                       keep_records(run, 14, 0, 412);
                       PuzzleScene *puzzle =
                           open_arriving(run, modes::make_puzzle_streak(run.app.context()));
                       for (int i = 0; i < 3; ++i)
                           solve(run, *puzzle);
                       wait_for_turn(run, *puzzle);
                       run.press(Action::west); // the one skip
                       wait_for_turn(run, *puzzle);
                       run.idle(40);
                   }});
    // Past the record: the run turns to gold.
    all.push_back({"puzzle-streak-beyond", [](Run &run)
                   {
                       keep_records(run, 2, 0, 412);
                       PuzzleScene *puzzle =
                           open(run, modes::make_puzzle_streak(run.app.context()));
                       for (int i = 0; i < 4; ++i)
                           solve(run, *puzzle);
                       run.idle(8);
                       run.shot("step");
                       wait_for_turn(run, *puzzle);
                       run.idle(40);
                   }});
    all.push_back({"puzzle-streak-over", [](Run &run)
                   {
                       keep_records(run, 14, 0, 412);
                       PuzzleScene *puzzle =
                           open(run, modes::make_puzzle_streak(run.app.context()));
                       for (int i = 0; i < 3; ++i)
                           solve(run, *puzzle);
                       blunder(run, *puzzle);
                       run.idle(120);
                   }});
    // A first run: any score is a record.
    all.push_back({"puzzle-streak-record", [](Run &run)
                   {
                       PuzzleScene *puzzle =
                           open(run, modes::make_puzzle_streak(run.app.context()));
                       for (int i = 0; i < 3; ++i)
                           solve(run, *puzzle);
                       blunder(run, *puzzle);
                       run.idle(20);
                       run.shot("arriving");
                       run.idle(120);
                   }});

    // ---- one puzzle at a time ----
    all.push_back({"puzzle-theme", [](Run &run)
                   {
                       PuzzleScene *puzzle = open_arriving(run, fork_puzzles(run.app.context()));
                       wait_for_turn(run, *puzzle);
                       run.idle(40);
                   }});
    all.push_back({"puzzle-theme-solved", [](Run &run)
                   {
                       PuzzleScene *puzzle = open(run, fork_puzzles(run.app.context()));
                       solve(run, *puzzle);
                       // The verdict lands, the themes arrive one after another.
                       run.idle(6);
                       run.shot("landing");
                       run.idle(60);
                   }});
    all.push_back({"puzzle-wrong", [](Run &run)
                   {
                       PuzzleScene *puzzle = open(run, fork_puzzles(run.app.context()));
                       blunder(run, *puzzle);
                       // The cross pops and the strip's words give way.
                       run.idle(5);
                       run.shot("landing");
                       run.idle(40);
                   }});
    // A right move that is not the last: the strip turns to the good colour.
    all.push_back({"puzzle-correct", [](Run &run)
                   {
                       PuzzleScene *puzzle = open(run, fork_puzzles(run.app.context()));
                       wait_for_turn(run, *puzzle);
                       chess::Move move;
                       if (puzzle->expected(&move))
                           play(run, *puzzle, move);
                       run.idle(22);
                   }});
    all.push_back({"puzzle-hint", [](Run &run)
                   {
                       PuzzleScene *puzzle = open(run, fork_puzzles(run.app.context()));
                       wait_for_turn(run, *puzzle);
                       run.idle(30);
                       run.press(Action::north);
                       run.idle(20);
                       run.shot("piece");
                       run.press(Action::north);
                       run.idle(30);
                   }});
    all.push_back({"puzzle-solution", [](Run &run)
                   {
                       PuzzleScene *puzzle = open(run, fork_puzzles(run.app.context()));
                       wait_for_turn(run, *puzzle);
                       run.press(Action::west);
                       run.idle(30);
                       run.shot("playing");
                       run.idle(400);
                   }});
    all.push_back({"puzzle-pause", [](Run &run)
                   {
                       PuzzleScene *puzzle = open(run, modes::make_puzzle_storm(run.app.context()));
                       solve(run, *puzzle);
                       wait_for_turn(run, *puzzle);
                       run.press(Action::menu);
                       run.nav(Direction::down);
                       run.idle(50);
                   }});
    // Training as a signed-in player sees it: the rating moves with the result.
    all.push_back({"puzzle-training",
                   [](Run &run)
                   {
                       app::Context &ctx = run.app.context();
                       PuzzleScene *puzzle = open(run, training(std::make_unique<RatedPack>(
                                                           ctx.pack, -1, 1900, 2100, 0x51ull)));
                       solve(run, *puzzle);
                       run.idle(90);
                   },
                   true});
    all.push_back({"puzzle-daily",
                   [](Run &run)
                   {
                       PuzzleScene *puzzle = open(run, modes::make_daily_puzzle(run.app.context()));
                       wait_for_turn(run, *puzzle);
                       run.idle(40);
                   },
                   true});
    // The daily puzzle has no next one.
    all.push_back({"puzzle-daily-done",
                   [](Run &run)
                   {
                       PuzzleScene *puzzle = open(run, modes::make_daily_puzzle(run.app.context()));
                       solve(run, *puzzle);
                       run.idle(30);
                       run.press(Action::confirm);
                       run.idle(50);
                   },
                   true});
    all.push_back({"puzzle-loading",
                   [](Run &run)
                   {
                       open(run, training(std::make_unique<SlowSource>()));
                       run.idle(50);
                   },
                   true});
    all.push_back({"puzzle-error", [](Run &run)
                   {
                       open(run, training(std::make_unique<BrokenSource>()));
                       run.idle(50);
                       // The ring is on its way to the other answer.
                       run.nav(Direction::right);
                       run.idle(1);
                       run.shot("moving");
                       run.idle(40);
                   }});

    // ---- reduced motion: everything is simply there, nothing slides or pops ----
    all.push_back({"puzzles-reduced-motion", [](Run &run)
                   {
                       run.app.context().settings->reduced_motion = true;
                       keep_records(run, 14, 27, 412);
                       puzzles_page(run, "arriving");
                       run.nav(Direction::right, 2);
                   }});
    all.push_back({"puzzle-storm-reduced-motion", [](Run &run)
                   {
                       run.app.context().settings->reduced_motion = true;
                       PuzzleScene *puzzle = open(run, modes::make_puzzle_storm(run.app.context()));
                       for (int i = 0; i < 5; ++i)
                           solve(run, *puzzle);
                       run.idle(4);
                   }});
}

} // namespace pch::host
