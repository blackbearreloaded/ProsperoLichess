// ProsperoLichess - Puzzle screen: classic, Streak and Storm rules over any puzzle source.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/chrome.hpp"
#include "app/scene.hpp"
#include "board/board_input.hpp"
#include "board/board_view.hpp"
#include "core/strings.hpp"
#include "puzzles/records.hpp"
#include "puzzles/source.hpp"
#include "ui/components/button.hpp"
#include "ui/components/dialog.hpp"
#include "ui/components/pause_menu.hpp"
#include "ui/components/progress.hpp"
#include "ui/components/quick_action.hpp"
#include "ui/components/skeleton.hpp"
#include "ui/components/stat.hpp"
#include "ui/confetti.hpp"
#include "ui/motion.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pch::modes
{

enum class PuzzleRules
{
    classic, // keep trying after a mistake; next puzzle on demand
    streak,  // one mistake ends the run (one skip allowed); difficulty climbs
    storm,   // three minutes, combos add time, mistakes cost time
};

// The ways to play puzzles. Each has a sign and a hue of its own inside the
// puzzle family; the Puzzles page and the puzzle screen share them, so the
// screen a tile opens wears that tile's colour.
enum class PuzzleKind : int
{
    daily,
    training,
    streak,
    storm,
    themes,
    count,
};
gfx::Color puzzle_hue(PuzzleKind kind);
// The kind's sign in one colour: a calendar leaf, a target, rising steps, a
// bolt, four squares.
void draw_puzzle_sign(gfx::DrawList &list, PuzzleKind kind, const gfx::Rect &box, gfx::Color ink);
// The sign on a rounded tile: the tile fills with the hue as lit goes to 1.
void draw_puzzle_badge(gfx::DrawList &list, PuzzleKind kind, const gfx::Rect &tile, float lit);

// The difficulty bands themed puzzles are offered in.
struct PuzzleBand
{
    const char *label; // English: draw it with puzzle_band_label()
    int min;           // ratings from here ...
    int max;           // ... up to here
};
inline constexpr PuzzleBand kPuzzleBands[] = {{TRC("difficulty", "Easier"), 600, 1200},
                                              {TRC("difficulty", "Normal"), 1200, 1700},
                                              {TRC("difficulty", "Harder"), 1700, 2200},
                                              {TRC("difficulty", "Hardest"), 2200, 3000}};
inline constexpr int kPuzzleBandCount = 4;
// A band's name in the player's language.
inline const char *puzzle_band_label(const PuzzleBand &band)
{
    return trc("difficulty", band.label);
}

// ---- the rules of the two runs, for whoever explains them --------------------

// Storm: the clock, what a mistake costs, and the combos that add seconds.
inline constexpr float kStormSeconds = 180.0f;
inline constexpr int kStormPenalty = 10;
inline constexpr int kStormComboSteps[] = {5, 12, 20, 30, 40};
inline constexpr int kStormComboBonus[] = {3, 5, 7, 10, 10};
inline constexpr int kStormComboCount = 5;
// The rating of the puzzle a run asks for after `solved` puzzles.
inline constexpr int storm_rating(int solved)
{
    return 800 + solved * 45;
}
inline constexpr int streak_rating(int solved)
{
    return 1000 + solved * 60;
}

// What a puzzle screen is opened with. The title and the subtitle are given
// in the player's language.
struct PuzzleSetup
{
    std::string title;    // "Puzzle Storm"
    std::string subtitle; // "Three minutes on the clock"
    PuzzleRules rules = PuzzleRules::classic;
    // The sign and the hue of a classic screen (Streak and Storm have theirs).
    PuzzleKind kind = PuzzleKind::training;
    std::unique_ptr<puzzles::Source> source;
};

// The board, and a column of lit panels beside it that says how the puzzle or
// the run is going. Options opens a pause menu; a Streak or Storm run ends in
// a dialog.
class PuzzleScene final : public app::Scene
{
  public:
    enum class Phase
    {
        loading,
        intro,    // showing the position before the setup move
        opponent, // playing a reply
        player,   // waiting for the player's move
        solved,
        showing,  // playing the solution after "View solution"
        run_over, // Streak / Storm finished
        error,
    };

    explicit PuzzleScene(PuzzleSetup setup);

    void enter(app::Context &ctx) override;
    app::Transition update(app::Context &ctx, const InputFrame &input, float dt) override;
    void draw(app::Context &ctx, app::Frame &frame) const override;
    // The puzzle sky, with the accent of the way that is being played.
    app::look::Mood mood() const override
    {
        app::look::Mood mood = app::look::mood(app::look::Section::puzzle);
        mood.accent = accent_;
        return mood;
    }

    const char *name() const override
    {
        return "puzzle";
    }

    // ---- what a script needs to play along (host scenarios, tests) ----
    Phase phase() const
    {
        return phase_;
    }
    const puzzles::Puzzle &puzzle() const
    {
        return puzzle_;
    }
    const chess::Position &position() const
    {
        return position_;
    }
    // The move the puzzle waits for; false unless it is the player's turn.
    bool expected(chess::Move *move) const;
    chess::Square cursor() const
    {
        return input_.cursor();
    }
    chess::Color orientation() const
    {
        return view_.orientation();
    }
    bool promotion_open() const
    {
        return input_.promotion_open();
    }
    bool menu_open() const
    {
        return pause_.is_open();
    }
    int score() const
    {
        return score_;
    }
    int combo() const
    {
        return combo_;
    }
    int skips() const
    {
        return skips_;
    }
    int mistakes() const
    {
        return mistakes_;
    }
    int hint_level() const
    {
        return hint_level_;
    }
    float clock() const
    {
        return clock_;
    }
    bool new_record() const
    {
        return new_record_;
    }

  private:
    // How the last move of a classic puzzle went.
    enum class Verdict
    {
        none,
        correct,
        wrong,
    };
    // One finished puzzle of a run.
    struct Recent
    {
        int rating = 0;
        std::string theme;
        int outcome = 0; // 1 solved, 0 wrong, 2 skipped
        float seconds = 0.0f;
    };
    // The mark the state strip wears.
    enum class Mark : std::uint8_t
    {
        quiet, // a ring: nothing to do yet
        live,  // a pulsing dot: the player's move
        good,  // a check
        bad,   // a cross
    };
    // What the state strip says right now.
    struct State
    {
        std::string words;
        Mark mark = Mark::quiet;
    };
    // A line of text that gives way to the next one instead of being replaced.
    struct Line
    {
        std::string now;
        std::string before;
        tween::Spring in; // 0 -> 1 as `now` arrives

        // True when the text changed.
        bool set(const std::string &text);
        void update(float dt, bool calm);
    };

    void next_puzzle(app::Context &ctx);
    void poll_source(app::Context &ctx);
    void begin(app::Context &ctx);
    void play(app::Context &ctx, const chess::Move &move, bool animate);
    void on_player_move(app::Context &ctx, const chess::Move &move);
    void solved(app::Context &ctx);
    void mistake(app::Context &ctx, const chess::Move &move);
    void end_run(app::Context &ctx);
    void restart_run(app::Context &ctx);
    void give_hint(app::Context &ctx);
    void show_solution();
    void flip(app::Context &ctx);
    int target_rating() const;
    bool clock_runs() const;

    void remember(int outcome);
    void reveal_themes();
    void animate(app::Context &ctx, float dt);
    void close_menu_quietly(app::Context &ctx);
    State strip_state() const;
    std::string task() const;

    // 0..1 for the index-th part of the screen since it opened.
    float arrive(int index) const;
    void draw_board(const app::Context &ctx, gfx::DrawList &list) const;
    void draw_heading(const app::Context &ctx, gfx::DrawList &list, const gfx::Rect &panel,
                      bool to_move) const;
    void draw_classic(const app::Context &ctx, ui::Canvas &canvas) const;
    void draw_storm(const app::Context &ctx, ui::Canvas &canvas) const;
    void draw_streak(const app::Context &ctx, ui::Canvas &canvas) const;
    void draw_last_puzzle(const app::Context &ctx, gfx::DrawList &list, const gfx::Rect &panel,
                          float top) const;
    void draw_loading(const app::Context &ctx, ui::Canvas &canvas) const;
    void draw_error(const app::Context &ctx, ui::Canvas &canvas) const;
    void draw_result(const app::Context &ctx, ui::Canvas &canvas) const;
    float draw_tags(gfx::DrawList &list, const ui::Fonts &fonts, float x, float y, float width,
                    int rows) const;

    std::string title_;
    std::string subtitle_;
    PuzzleRules rules_;
    PuzzleKind kind_;
    gfx::Color accent_;
    std::unique_ptr<puzzles::Source> source_;

    // ---- the puzzle ----
    Phase phase_ = Phase::loading;
    puzzles::Puzzle puzzle_;
    chess::Position position_ = chess::Position::start();
    chess::Move last_move_;
    std::size_t step_ = 0;
    float timer_ = 0.0f;
    bool failed_ = false;
    bool hinted_ = false;
    bool practice_ = false; // a retry: nothing is reported or counted
    bool shown_ = false;    // the solution was played for the player
    int hint_level_ = 0;
    Verdict verdict_ = Verdict::none;
    float puzzle_time_ = 0.0f; // seconds spent on this puzzle
    std::string error_;
    bool exhausted_ = false; // the source has nothing more (not a failure)
    int choice_ = 0;         // the focused button of the error state
    bool started_ = false;

    board::BoardView view_;
    board::BoardInput input_;
    chess::Square good_ = chess::kNoSquare;
    chess::Square bad_ = chess::kNoSquare;
    float feedback_ = 0.0f;

    // ---- the run (Streak / Storm) ----
    int score_ = 0;
    int combo_ = 0;
    int best_combo_ = 0;
    int skips_ = 1;
    int mistakes_ = 0;
    float clock_ = kStormSeconds;
    float bonus_flash_ = 0.0f;
    int bonus_seconds_ = 0;
    bool new_record_ = false;
    puzzles::Records records_;
    int puzzles_seen_ = 0;
    bool has_last_ = false;
    Recent last_;                     // the puzzle before this one
    std::vector<std::uint8_t> marks_; // every puzzle of the run, oldest first (a Recent outcome)

    // ---- what is on screen ----
    std::vector<std::string> tags_; // the themes on record, once they give nothing away
    ui::QuickActionBar run_bar_;    // Streak / Storm shortcuts
    ui::QuickActionBar play_bar_;   // classic, while solving
    ui::QuickActionBar done_bar_;   // classic, once solved
    ui::Spinner busy_;
    ui::Skeleton bones_head_;
    ui::Skeleton bones_facts_;
    ui::Skeleton bones_foot_;
    ui::EmptyState empty_;
    ui::PushButton retry_;
    ui::PushButton leave_;
    ui::PauseMenu pause_;
    ui::Dialog result_;
    ui::Confetti confetti_;

    // ---- how it moves (advanced in animate(), read by draw()) ----
    bool calm_ = false;   // reduced motion
    float since_ = 0.0f;  // seconds since the screen opened: parts arrive by it
    float waited_ = 0.0f; // seconds the loading state has been showing
    tween::Spring turn_;  // 0..1: it is the player's move (the heading and the board are lit)
    Line title_line_;     // "Black to play"
    Line task_line_;      // "Find the best move for Black"
    Line strip_line_;     // the state strip's words
    Mark strip_mark_ = Mark::quiet;
    Mark strip_mark_before_ = Mark::quiet;
    ui::SpringColor strip_ink_; // the strip's colour: the accent, good or bad
    ui::Pulse strip_flash_;     // a verdict just arrived
    ui::Pulse board_flash_;     // ... and the light under the board answers
    bool board_flash_good_ = true;
    tween::Spring puzzle_rating_; // the puzzle's rating, counting once it may be shown
    tween::Spring rating_reveal_; // 0..1: the rating is shown
    tween::Spring player_rating_; // the player's puzzle rating, counting
    ui::Pulse delta_pop_;         // the rating just changed
    int delta_seen_ = 0;
    tween::Spring solved_shown_; // "solved on this console", counting
    float tags_age_ = 10.0f;     // seconds since the themes were revealed
    tween::Spring score_shown_;  // the run's score, counting
    ui::Pulse score_pop_;        // the score just grew
    int score_seen_ = 0;
    tween::Spring ring_;         // Storm: the share of the clock that is left
    tween::Spring level_;        // Storm: the combo toward its next bonus
    tween::Spring heat_;         // Storm: 0..1, a combo is running
    ui::Pulse level_flash_;      // Storm: a bonus was just won
    tween::Spring best_share_;   // Streak: the run against the record
    float mark_age_ = 10.0f;     // seconds since the run's newest mark
    float step_age_ = 10.0f;     // Streak: seconds since the newest step
    ui::SpringRect choice_ring_; // the error state: the ring between its two answers
    bool choice_placed_ = false;
    ui::Pulse choice_nudge_;    // ... pushed against its end
    tween::Spring figures_;     // 0..1: the result's figures fading in
    tween::Spring final_shown_; // the run's score, counting up in the result
    tween::Spring final_share_; // ... and how close it came to the record
    int best_before_ = 0;       // the record as it stood when the run ended
    float over_age_ = 0.0f;     // seconds since the run ended
};

} // namespace pch::modes
