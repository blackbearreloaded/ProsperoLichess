// ProsperoLichess - Home page: today's puzzle, the player's figures and things to continue.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reference screen of the house look (app/look.hpp):
//
//   - the hero is a lit panel: the board stands off it on a shadow, the title
//     is large display type under a tracked kicker, and its facts are figures;
//   - the three figures beside it are small dashboard tiles: a number that
//     counts up, how it moved lately, and its line over time;
//   - every card of the shelf has the colour of what it opens, and the sky
//     behind the page leans toward the colour of whatever has the focus;
//   - the page assembles: parts rise into place a moment apart.

#include "board/mini_board.hpp"
#include "core/strings.hpp"
#include "lichess/json.hpp"
#include "lichess/puzzle_sources.hpp"
#include "lichess/session.hpp"
#include "modes/page.hpp"
#include "modes/scenes.hpp"
#include "puzzles/pack.hpp"
#include "puzzles/puzzle.hpp"
#include "puzzles/records.hpp"
#include "ui/components/badge.hpp"
#include "ui/components/button.hpp"
#include "ui/components/carousel.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace pch::modes
{

namespace
{

using gfx::Align;
using gfx::Color;
using gfx::Rect;

// The page is four columns wide. The hero spans three, the figures take the
// fourth, and the shelf below shows one card per column, so every edge of the
// shelf lines up with an edge above it whichever cards it has scrolled to.
constexpr int kColumns = 4;
constexpr float kColumnWidth =
    (app::kRight - app::kContent - static_cast<float>(kColumns - 1) * app::kGap) /
    static_cast<float>(kColumns);
constexpr float kColumnPitch = kColumnWidth + app::kGap;
constexpr Rect kHero{app::kContent, app::kTop, 3.0f * kColumnPitch - app::kGap, 468.0f};
constexpr float kHeroBoard = 420.0f;
constexpr float kStatsX = app::kContent + 3.0f * kColumnPitch;
constexpr float kStatHeight = 140.0f;
constexpr float kShelfY = 632.0f;
constexpr float kShelfTitle = 40.0f; // the room the shelf's kicker takes

using app::look::Section;

// What the shelf's cards say under their art, apart from players' names and
// the line under a game (the texts refresh() gives the cards). Their type is
// sized so that all of them fit a column in the player's language.
constexpr const char *kCardTitles[] = {TR("Pass & Play"), TR("Puzzle Storm"), TR("Puzzle Streak"),
                                       TR("Lichess TV")};
constexpr const char *kCardNotes[] = {TR("Two players, one controller"), TR("Your game is saved"),
                                      TR("Race the clock"), TR("One mistake ends the run"),
                                      TR("The best games, live")};

// What one card of the Continue shelf opens.
enum class Kind
{
    online_game,
    local_game,
    storm,
    streak,
    tv,
};

struct Entry
{
    Kind kind = Kind::local_game;
    std::string game_id;
    chess::Position position = chess::Position::start();
    chess::Move last_move;
    chess::Color orientation = chess::Color::white;
    std::string big;   // the words beside the thumbnail
    std::string small; // ... and the quieter line under them
    Section section() const
    {
        switch (kind)
        {
        case Kind::storm:
        case Kind::streak:
            return Section::puzzles;
        case Kind::tv:
            return Section::watch;
        default:
            return Section::play;
        }
    }
};

// One of the three tiles beside the hero.
struct Figure
{
    enum class Sign
    {
        none,
        bolt,
        steps,
        check,
    };
    std::string label;
    std::string note;
    int value = 0;
    int change = 0;             // how the rating moved lately (0: unknown or none)
    std::vector<float> history; // its line over time (empty: none)
    Color accent = app::look::kInk;
    Sign sign = Sign::none; // drawn where the line would be, for figures without one
    tween::Spring shown;    // the number on screen, counting toward value
};

// A fact of the hero: a small label over a figure.
struct Fact
{
    std::string label;
    std::string value;
};

class HomePage final : public Page
{
  public:
    explicit HomePage(app::Context &ctx)
    {
        // refresh() places the two buttons: their widths follow their labels.
        primary_.glyph = ui::Button::cross;
        primary_.style.role = ui::ButtonRole::primary;
        secondary_.style.role = ui::ButtonRole::secondary;
        app::look::tint(app::look::accent(Section::puzzles), primary_, secondary_);

        shelf_.style.item_width = kColumnWidth;
        shelf_.style.gap = app::kGap;
        shelf_.style.counter = false;
        shelf_.style.entrance_step = 0.0f;
        shelf_.style.peek = 0.0f;
        shelf_.style.exits.left = true; // back to the rail
        shelf_.style.card.art_aspect = 16.0f / 9.0f;
        // A longer translation makes the type under the cards smaller, for
        // every card alike; a player's name is cut instead. The card leaves
        // its text the column less 4 px and cuts a line that fills it to the
        // last bit: the type is sized for a little less.
        constexpr float kRoom = kColumnWidth - 6.0f;
        float title_scale = 1.0f;
        float note_scale = 1.0f;
        const auto fit =
            [](float *scale, const ui::FontRef &font, std::string_view text, float size)
        { *scale = std::min(*scale, ui::fit_scale(font, text, size, kRoom)); };
        for (const char *text : kCardTitles)
            fit(&title_scale, ctx.fonts->semibold, tr(text), 24.0f);
        for (const char *text : kCardNotes)
            fit(&note_scale, ctx.fonts->regular, tr(text), 20.0f);
        // The line under a game names its speed: the longest one is the measure.
        const std::string speed = lichess::speed_name("Correspondence");
        for (const char *turn : {TR("Your turn \xC2\xB7 {0}"), TR("Their turn \xC2\xB7 {0}")})
            fit(&note_scale, ctx.fonts->regular, fill(tr(turn), {speed}), 20.0f);
        shelf_.style.card.title_size = 24.0f * title_scale;
        shelf_.style.card.subtitle_size = 20.0f * note_scale;
        // The cards keep their places on the grid; the focused one is lit.
        shelf_.style.card.focus_scale = 1.0f;
        shelf_.style.card.lift = 0.0f;
        shelf_.style.card.glow = true;
        shelf_.art = [this, &ctx](ui::Canvas &canvas, const Rect &art, float radius,
                                  const ui::CardItem &item, float)
        { draw_shelf_art(ctx, canvas, art, radius, item); };
        shelf_.set_bounds({app::kContent, kShelfY + kShelfTitle, app::kRight - app::kContent,
                           app::kBottom - kShelfY - kShelfTitle});
        live_.style.kind = ui::Status::danger;
        live_.style.height = 28.0f;
        live_.style.text_size = 16.0f;
        live_.set_text(tr("LIVE"));

        refresh(ctx, true);
    }

    void enter(app::Context &ctx) override
    {
        records_ = puzzles::load_records(ctx.data_root);
        since_ = 0.0f;
        for (Figure &figure : figures_)
            figure.shown.snap(0.0f);
        if (ctx.lichess != nullptr)
        {
            ctx.lichess->refresh_ongoing();
            ctx.lichess->watch_tv("");
        }
        refresh(ctx, false);
    }

    PageResult update(app::Context &ctx, const InputFrame &input, float dt, bool focused) override
    {
        ctx.calm(primary_, secondary_, shelf_, live_);
        calm_ = ctx.reduced_motion();
        since_ += dt;
        // Live data (the TV thumbnail, whose turn it is) is read twice a second.
        refresh_in_ -= dt;
        if (refresh_in_ <= 0.0f)
            refresh(ctx, false);

        PageResult result;
        if (focused)
        {
            if (input.is_pressed(Action::back))
            {
                ctx.cue(audio::Cue::back);
                result.to_rail = true;
            }
            else if (zone_ == Zone::hero)
            {
                handle_hero(ctx, input, &result);
            }
            else
            {
                handle_shelf(ctx, input, &result);
            }
        }
        primary_.set_active(focused && zone_ == Zone::hero && button_ == 0);
        secondary_.set_active(focused && zone_ == Zone::hero && button_ == 1);
        shelf_.set_active(focused && zone_ == Zone::shelf);

        // The ring of the shelf takes the colour of the card it is on.
        shelf_.style.theme.focus = app::look::accent(focus_section());
        hero_lit_.target = focused && zone_ == Zone::hero ? 1.0f : 0.0f;
        hero_lit_.update(dt, calm_ ? 60.0f : 14.0f);
        for (Figure &figure : figures_)
        {
            figure.shown.target = static_cast<float>(figure.value);
            // The numbers start counting once their tile has arrived.
            if (since_ > 0.35f || calm_)
                figure.shown.update(dt, calm_ ? 60.0f : 7.0f);
        }

        primary_.update(dt);
        secondary_.update(dt);
        shelf_.update(dt);
        live_.update(dt);
        return result;
    }

    void draw(app::Context &ctx, app::Frame &frame, bool) const override
    {
        namespace look = app::look;
        gfx::DrawList &list = frame.scene;
        const ui::Fonts &fonts = *ctx.fonts;
        ui::Canvas canvas = app::canvas_for(ctx, list);
        const Color accent = look::accent(Section::puzzles);

        // ---- the hero: today's puzzle, or the offline run when there is none
        {
            const float in = arrive(0);
            list.push_opacity(in);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in));
            look::lift(list, kHero, hero_lit_.value, accent, ctx.time, calm_);
            look::panel(list, kHero, hero_lit_.value, accent);
            const Rect squares{kHero.x + 24.0f, kHero.y + 24.0f, kHeroBoard, kHeroBoard};
            look::frame_board(list, squares, accent, 0.5f + 0.5f * hero_lit_.value);
            board::draw_mini_board(list, *ctx.pieces, ctx.board_theme(), squares, hero_position_,
                                   hero_orientation_, hero_last_, 10.0f);
            list.pop_transform();
            list.pop_opacity();
        }
        const float x = kHero.x + 24.0f + kHeroBoard + 44.0f;
        const float room = kHero.x + kHero.w - 36.0f - x;
        // The words arrive after the panel, top to bottom.
        const auto part = [&](int index, const auto &draw)
        {
            const float in = arrive(index);
            list.push_opacity(in);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in, 16.0f));
            draw();
            list.pop_transform();
            list.pop_opacity();
        };
        part(1, [&]() { look::kicker(list, fonts, hero_kicker_, x, kHero.y + 62.0f, accent); });
        part(2,
             [&]()
             {
                 ui::text_fit(list, fonts.display, hero_title_, x - 3.0f, kHero.y + 136.0f, 66.0f,
                              room, look::kInk);
             });
        part(3,
             [&]()
             {
                 // The facts stand side by side, each as wide as its label or
                 // its figure. Longer labels than the column holds close up
                 // first, then shrink.
                 constexpr float kGap = 44.0f;
                 std::array<float, 3> cells{};
                 float sum = 0.0f;
                 float gaps = -1.0f;
                 for (std::size_t i = 0; i < facts_.size(); ++i)
                 {
                     if (facts_[i].value.empty())
                         continue;
                     cells[i] = std::max(
                         {fonts.semibold.measure(facts_[i].value, 30.0f),
                          fonts.semibold.measure(ui::upper(facts_[i].label), 14.0f, 3.0f), 96.0f});
                     sum += cells[i];
                     gaps += 1.0f;
                 }
                 const bool fits = sum + gaps * kGap <= room || gaps <= 0.0f;
                 const float gap = fits ? kGap : std::max((room - sum) / gaps, 20.0f);
                 const float squeeze = fits ? 1.0f : std::min((room - gaps * gap) / sum, 1.0f);
                 float fx = x;
                 for (std::size_t i = 0; i < facts_.size(); ++i)
                 {
                     const Fact &fact = facts_[i];
                     if (fact.value.empty())
                         continue;
                     const float cell = cells[i] * squeeze;
                     look::kicker(list, fonts, fact.label, fx, kHero.y + 196.0f,
                                  look::kInk.with_alpha(look::kFaint), Align::left, 14.0f, cell);
                     look::figure(list, fonts, fact.value, fx, kHero.y + 232.0f, 30.0f, look::kInk);
                     fx += cell + gap;
                 }
                 look::rule(list, x, kHero.y + 258.0f, room);
             });
        part(4,
             [&]()
             {
                 // Two lines in English. A longer translation takes three
                 // smaller ones in the same room over the buttons.
                 const Color ink = look::kInk.with_alpha(look::kMuted);
                 if (fonts.regular.font->wrap(hero_body_, 24.0f, room).size() <= 2)
                     ui::paragraph(list, fonts.regular, hero_body_, x, kHero.y + 300.0f, 24.0f,
                                   room, 34.0f, ink, 2);
                 else
                     ui::paragraph(list, fonts.regular, hero_body_, x, kHero.y + 296.0f, 21.0f,
                                   room, 28.0f, ink, 3);
             });
        part(5,
             [&]()
             {
                 primary_.draw(canvas);
                 secondary_.draw(canvas);
             });

        // ---- three figures beside it
        for (std::size_t i = 0; i < figures_.size(); ++i)
            draw_figure(list, fonts, figures_[i], static_cast<int>(i));

        // ---- the shelf
        {
            const float in = arrive(6);
            list.push_opacity(in);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in));
            look::kicker(list, fonts, trc("heading", "Continue"), app::kContent, kShelfY + 22.0f,
                         look::kInk.with_alpha(look::kMuted));
            shelf_.draw(canvas);
            list.pop_transform();
            list.pop_opacity();
        }
    }

    Section section() const override
    {
        return Section::home;
    }

    // The sky leans toward the colour of whatever has the focus.
    app::look::Mood mood() const override
    {
        app::look::Mood mood = app::look::mood(Section::home);
        const app::look::Mood &item = app::look::mood(focus_section());
        for (std::size_t i = 0; i < 4; ++i)
            mood.sky[i] = gfx::mix(mood.sky[i], item.sky[i], 0.5f);
        return mood;
    }

    std::span<const ui::Hint> hints() const override
    {
        static constexpr ui::Hint kHints[] = {{ui::Button::cross, TR("Select")},
                                              {ui::Button::dpad, TR("Move")},
                                              {ui::Button::options, TR("Menu")}};
        return kHints;
    }

    const char *title() const override
    {
        return TR("Home");
    }

    const char *name() const override
    {
        return "home";
    }

  private:
    enum class Zone
    {
        hero,
        shelf,
    };

    void handle_hero(app::Context &ctx, const InputFrame &input, PageResult *result)
    {
        if (input.nav == Direction::down && !entries_.empty())
        {
            zone_ = Zone::shelf;
            ctx.cue(audio::Cue::focus);
        }
        else if (input.nav == Direction::right && button_ == 0)
        {
            button_ = 1;
            ctx.cue(audio::Cue::focus);
        }
        else if (input.nav == Direction::left)
        {
            if (button_ == 1)
            {
                button_ = 0;
                ctx.cue(audio::Cue::focus);
            }
            else
            {
                result->to_rail = true;
            }
        }
        ui::PushButton &button = button_ == 0 ? primary_ : secondary_;
        if (button.handle(input, *ctx.feedback) != ui::Event::activated)
            return;
        if (daily_)
            result->transition = app::Transition::push(button_ == 0 ? make_daily_puzzle(ctx)
                                                                    : make_puzzle_training(ctx));
        else
            result->transition = app::Transition::push(button_ == 0 ? make_puzzle_streak(ctx)
                                                                    : make_puzzle_storm(ctx));
    }

    void handle_shelf(app::Context &ctx, const InputFrame &input, PageResult *result)
    {
        if (input.nav == Direction::up)
        {
            zone_ = Zone::hero;
            ctx.cue(audio::Cue::focus);
            return;
        }
        const ui::Event event = shelf_.handle(input, *ctx.feedback);
        if (event == ui::Event::none && shelf_.exit() == Direction::left)
        {
            result->to_rail = true;
            return;
        }
        if (event != ui::Event::activated || entries_.empty())
            return;
        const Entry &entry = entries_[static_cast<std::size_t>(
            std::clamp(shelf_.focus(), 0, static_cast<int>(entries_.size()) - 1))];
        switch (entry.kind)
        {
        case Kind::online_game:
            result->transition = app::Transition::push(make_online_game(ctx, entry.game_id));
            break;
        case Kind::local_game:
            result->transition = app::Transition::push(make_local_game(ctx));
            break;
        case Kind::storm:
            result->transition = app::Transition::push(make_puzzle_storm(ctx));
            break;
        case Kind::streak:
            result->transition = app::Transition::push(make_puzzle_streak(ctx));
            break;
        case Kind::tv:
            result->transition = app::Transition::push(make_tv(""));
            break;
        }
    }

    // Reads everything the page shows from the session, the pack and the
    // saved records. first: settle without animation.
    void refresh(app::Context &ctx, bool first)
    {
        refresh_in_ = 0.5f;
        lichess::Session *session = ctx.lichess;
        const bool online = session != nullptr && session->online();
        const bool signed_in = session != nullptr && session->signed_in();

        // ---- the hero ----
        puzzles::Puzzle puzzle;
        std::string error;
        const bool daily = online && !session->daily_json().empty() &&
                           lichess::parse_api_puzzle(session->daily_json(), &puzzle, &error);
        if (daily)
        {
            daily_ = true;
            hero_position_ = puzzle.start();
            hero_last_ = puzzle.setup;
            hero_orientation_ = puzzle.solver();
            hero_kicker_ = tr("Daily puzzle");
            hero_title_ =
                puzzle.solver() == chess::Color::white ? tr("White to play") : tr("Black to play");
            hero_body_ = tr("One new puzzle every day, shared by every player on Lichess. Find "
                            "the best move.");
            lichess::Document doc(session->daily_json());
            const long long plays = doc.root()["puzzle"]["plays"].integer();
            facts_[0] = {tr("Rating"), std::to_string(puzzle.rating)};
            facts_[1] = {tr("Played"), plays > 0 ? grouped(plays) : std::string()};
            facts_[2] = {};
            primary_.label = tr("Solve");
            secondary_.label = tr("Training");
        }
        else if (first || daily_)
        {
            daily_ = false;
            hero_position_ = chess::Position::start();
            hero_last_ = {};
            hero_orientation_ = chess::Color::white;
            if (ctx.pack != nullptr && ctx.pack->size() > 0)
            {
                std::uint64_t rng = 0x5eed | 1u;
                std::size_t index = 0;
                puzzles::PackPuzzle raw;
                if (ctx.pack->pick(
                        1100, 1300, -1, &rng, [](std::size_t) { return false; }, &index) &&
                    ctx.pack->get(index, &raw) &&
                    puzzles::from_csv(raw.id, raw.fen, raw.moves, raw.rating, &puzzle, &error))
                {
                    hero_position_ = puzzle.start();
                    hero_last_ = puzzle.setup;
                    hero_orientation_ = puzzle.solver();
                }
            }
            hero_kicker_ = tr("Offline puzzles");
            hero_title_ = tr("Puzzle Streak");
            hero_body_ = tr("Solve puzzles of rising difficulty. One mistake ends the run, and "
                            "you get a single skip.");
            facts_[1] = {tr("Puzzles"), ctx.pack != nullptr
                                            ? grouped(static_cast<long long>(ctx.pack->size()))
                                            : std::string()};
            facts_[2] = {tr("Best storm"), std::to_string(records_.best_storm)};
            primary_.label = tr("Start");
            secondary_.label = tr("Storm");
        }
        if (!daily_)
            facts_[0] = {tr("Best streak"), std::to_string(records_.best_streak)};
        place_buttons(ctx);

        // ---- the figures ----
        const Color gold = app::look::accent(Section::puzzles);
        if (signed_in)
        {
            set_rating(0, tr("Rapid"), session->perf("rapid"),
                       app::look::speed_color(app::look::Speed::rapid), first);
            set_rating(1, tr("Puzzles"), session->perf("puzzle"), gold, first);
            set_record(2, tr("Best streak"), records_.best_streak, tr("On this console"),
                       Figure::Sign::steps, first);
        }
        else
        {
            set_record(0, tr("Best streak"), records_.best_streak, tr("Puzzle Streak"),
                       Figure::Sign::steps, first);
            set_record(1, tr("Best storm"), records_.best_storm, tr("Puzzle Storm"),
                       Figure::Sign::bolt, first);
            set_record(2, tr("Puzzles solved"), records_.solved, tr("On this console"),
                       Figure::Sign::check, first);
        }

        // ---- the shelf ----
        std::vector<Entry> entries;
        std::vector<ui::CardItem> items;
        const auto add = [&](Entry entry, std::string title, std::string subtitle)
        {
            ui::CardItem item;
            item.title = std::move(title);
            item.subtitle = std::move(subtitle);
            item.tag = static_cast<int>(entries.size());
            item.accent = app::look::accent(entry.section());
            items.push_back(std::move(item));
            entries.push_back(std::move(entry));
        };
        if (signed_in)
        {
            // Games waiting for the player come first.
            for (const bool mine : {true, false})
            {
                for (const lichess::OngoingGame &game : session->ongoing())
                {
                    if (game.my_turn != mine)
                        continue;
                    Entry entry;
                    entry.kind = Kind::online_game;
                    entry.game_id = game.game_id;
                    if (!chess::Position::from_fen(game.fen, &entry.position))
                        entry.position = chess::Position::start();
                    if (game.last_move.size() >= 4)
                    {
                        entry.last_move.from = chess::parse_square(game.last_move.substr(0, 2));
                        entry.last_move.to = chess::parse_square(game.last_move.substr(2, 2));
                    }
                    entry.orientation = game.color;
                    entry.big = game.my_turn ? tr("Your move") : tr("Waiting");
                    entry.small = time_left(game.seconds_left);
                    const std::string opponent =
                        game.ai_level > 0
                            ? fill(tr("Stockfish level {0}"), {std::to_string(game.ai_level)})
                            : game.opponent;
                    const std::string speed = lichess::speed_name(game.speed);
                    std::string turn = game.my_turn ? tr("Your turn") : tr("Their turn");
                    if (!speed.empty())
                        turn = fill(game.my_turn ? tr("Your turn \xC2\xB7 {0}")
                                                 : tr("Their turn \xC2\xB7 {0}"),
                                    {speed});
                    add(std::move(entry), fill(tr("vs {0}"), {opponent}), std::move(turn));
                }
            }
        }
        {
            Entry entry;
            entry.kind = Kind::local_game;
            const bool saved = has_saved_local_game(ctx);
            entry.big = saved ? tr("Resume") : tr("New game");
            entry.small = tr("Offline");
            add(std::move(entry), tr("Pass & Play"),
                saved ? tr("Your game is saved") : tr("Two players, one controller"));
        }
        {
            Entry entry;
            entry.kind = Kind::storm;
            entry.big = "3:00";
            entry.small = fill(tr("Best {0}"), {std::to_string(records_.best_storm)});
            add(std::move(entry), tr("Puzzle Storm"), tr("Race the clock"));
        }
        {
            Entry entry;
            entry.kind = Kind::streak;
            entry.big = tr("Streak");
            entry.small = fill(tr("Best {0}"), {std::to_string(records_.best_streak)});
            add(std::move(entry), tr("Puzzle Streak"), tr("One mistake ends the run"));
        }
        if (online)
        {
            const lichess::TvGame &tv = session->tv();
            Entry entry;
            entry.kind = Kind::tv;
            std::string subtitle = tr("The best games, live");
            if (tv.valid)
            {
                entry.position = tv.position;
                entry.last_move = tv.last_move;
                entry.orientation = tv.orientation;
                entry.big = std::to_string(std::max(tv.white_rating, tv.black_rating));
                entry.small = tr("Top rated");
                subtitle = fill(tr("{0} vs {1}"), {tv.white, tv.black});
            }
            else
            {
                entry.big = tr("Live");
                entry.small = tr("Top rated");
            }
            add(std::move(entry), tr("Lichess TV"), std::move(subtitle));
        }

        // The shelf is rebuilt only when what it lists changed, so the focus
        // and the scroll stay where the player left them.
        std::string signature;
        for (std::size_t i = 0; i < entries.size(); ++i)
            signature += std::to_string(static_cast<int>(entries[i].kind)) + entries[i].game_id +
                         items[i].title + items[i].subtitle + "|";
        entries_ = std::move(entries);
        if (signature != shelf_signature_)
        {
            const int focus = shelf_.focus();
            shelf_signature_ = signature;
            const int count = static_cast<int>(items.size());
            shelf_.set_items(std::move(items));
            shelf_.set_focus(std::clamp(focus, 0, std::max(0, count - 1)));
        }
    }

    // The hero's two buttons, side by side. Each keeps the width the layout
    // gives it, which holds the English label, or grows to what a longer one
    // needs.
    void place_buttons(const app::Context &ctx)
    {
        const float x = kHero.x + 24.0f + kHeroBoard + 44.0f;
        const float y = kHero.y + kHero.h - 28.0f - 64.0f;
        const float room = kHero.x + kHero.w - 36.0f - x - 16.0f;
        const float first = std::clamp(primary_.preferred_width(*ctx.fonts), 216.0f, room - 188.0f);
        const float second =
            std::clamp(secondary_.preferred_width(*ctx.fonts), 188.0f, room - first);
        primary_.set_bounds({x, y, first, 64.0f});
        secondary_.set_bounds({x + first + 16.0f, y, second, 64.0f});
    }

    // A rating from the account, with how it moved and its line over time.
    void set_rating(std::size_t index, const char *label, const lichess::Perf *perf, Color accent,
                    bool snap)
    {
        Figure &figure = figures_[index];
        figure.label = label;
        figure.note = perf == nullptr || perf->rating <= 0 ? tr("No rating yet")
                      : perf->provisional                  ? tr("Provisional rating")
                                                           : tr("Lichess rating");
        figure.value = perf != nullptr ? perf->rating : 0;
        figure.change = perf != nullptr ? perf->progress : 0;
        if (perf != nullptr)
            figure.history = perf->history;
        else
            figure.history.clear();
        figure.accent = accent;
        figure.sign = Figure::Sign::none;
        if (snap)
            figure.shown.snap(static_cast<float>(figure.value));
    }

    // A record kept on this console.
    void set_record(std::size_t index, const char *label, int value, const char *note,
                    Figure::Sign sign, bool snap)
    {
        Figure &figure = figures_[index];
        figure.label = label;
        figure.note = note;
        figure.value = value;
        figure.change = 0;
        figure.history.clear();
        figure.accent = app::look::accent(Section::puzzles);
        figure.sign = sign;
        if (snap)
            figure.shown.snap(static_cast<float>(value));
    }

    // 0..1 for the index-th part of the page since it was entered.
    float arrive(int index) const
    {
        return calm_ ? 1.0f : app::look::rise(since_, index, 0.06f, 0.5f);
    }

    Section focus_section() const
    {
        if (zone_ == Zone::hero || entries_.empty())
            return Section::puzzles;
        return entries_[static_cast<std::size_t>(
                            std::clamp(shelf_.focus(), 0, static_cast<int>(entries_.size()) - 1))]
            .section();
    }

    void draw_figure(gfx::DrawList &list, const ui::Fonts &fonts, const Figure &figure,
                     int index) const
    {
        namespace look = app::look;
        const Rect r{kStatsX, app::kTop + static_cast<float>(index) * (kStatHeight + app::kGap),
                     app::kRight - kStatsX, kStatHeight};
        const float in = arrive(2 + index);
        list.push_opacity(in);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in));
        look::panel(list, r, 0.0f, figure.accent);
        // The label ends before the change beside it.
        const float change = look::delta(list, fonts, figure.change, r.x + r.w - look::kPad,
                                         r.y + 40.0f, 19.0f, Align::right);
        look::kicker(list, fonts, figure.label, r.x + look::kPad, r.y + 40.0f, figure.accent,
                     Align::left, 16.0f,
                     r.w - 2.0f * look::kPad - (change > 0.0f ? change + 14.0f : 0.0f));
        char text[16];
        std::snprintf(text, sizeof(text), "%d", static_cast<int>(figure.shown.value + 0.5f));
        // A rating the account does not have yet is a dash, not a zero.
        if (figure.sign == Figure::Sign::none && figure.value <= 0)
            list.rounded_rect({r.x + look::kPad, r.y + 76.0f, 30.0f, 5.0f}, 2.5f,
                              look::kInk.with_alpha(look::kFaint));
        else
            look::figure(list, fonts, text, r.x + look::kPad - 2.0f, r.y + 94.0f, 46.0f,
                         look::kInk);
        // The note runs under a line; a record's sign reaches lower, and the
        // note ends before it.
        ui::text_fit(list, fonts.regular, figure.note, r.x + look::kPad, r.y + 122.0f, 17.0f,
                     r.w - 2.0f * look::kPad - (figure.sign == Figure::Sign::none ? 0.0f : 70.0f),
                     look::kInk.with_alpha(look::kFaint));

        const Rect chart{r.x + r.w - look::kPad - 132.0f, r.y + 58.0f, 132.0f, 42.0f};
        const float grow = calm_ ? 1.0f : look::rise(since_, 5 + index, 0.08f, 0.9f);
        if (figure.sign == Figure::Sign::none)
        {
            look::sparkline(list, chart, figure.history, figure.accent, grow);
        }
        else
        {
            // A record has no line: its sign stands where the line would be.
            const float cx = chart.x + chart.w - 30.0f;
            const float cy = chart.cy() + 4.0f;
            const float side = 60.0f;
            list.circle(cx, cy, side * 0.5f, figure.accent.with_alpha(0.14f * grow));
            list.ring(cx, cy, side * 0.5f, 2.0f, figure.accent.with_alpha(0.7f * grow));
            draw_sign(list, figure.sign, cx, cy, side * 0.5f, figure.accent.with_alpha(grow));
        }
        list.pop_transform();
        list.pop_opacity();
    }

    // The signs of the offline modes, inside a disc of radius `reach`.
    static void draw_sign(gfx::DrawList &list, Figure::Sign sign, float cx, float cy, float reach,
                          Color ink)
    {
        const float side = reach * 2.0f;
        switch (sign)
        {
        case Figure::Sign::bolt:
            app::look::speed_icon(list,
                                  {cx - reach * 0.6f, cy - reach * 0.6f, side * 0.6f, side * 0.6f},
                                  app::look::Speed::blitz, ink);
            break;
        case Figure::Sign::steps:
        {
            const float w = side * 0.13f;
            for (int i = 0; i < 4; ++i)
            {
                const float h = side * (0.16f + 0.12f * static_cast<float>(i));
                list.rounded_rect({cx - side * 0.3f + static_cast<float>(i) * (w + side * 0.035f),
                                   cy + side * 0.26f - h, w, h},
                                  3.0f, ink);
            }
            break;
        }
        case Figure::Sign::check:
        {
            const float u = reach * 0.42f;
            const float pen = std::max(reach * 0.16f, 2.4f);
            list.line(cx - u, cy + u * 0.1f, cx - u * 0.25f, cy + u * 0.8f, pen, ink);
            list.line(cx - u * 0.25f, cy + u * 0.8f, cx + u, cy - u * 0.7f, pen, ink);
            break;
        }
        case Figure::Sign::none:
            break;
        }
    }

    static const char *kind_word(Kind kind)
    {
        switch (kind)
        {
        case Kind::online_game:
            return tr("Your game");
        case Kind::local_game:
            return tr("Offline");
        case Kind::storm:
            return tr("Storm");
        case Kind::streak:
            return tr("Streak");
        case Kind::tv:
            return tr("Lichess TV");
        }
        return "";
    }

    static std::string time_left(long long seconds)
    {
        if (seconds < 0)
            return tr("No clock");
        if (seconds >= 86400)
            return fill(tr("{0}d {1}h left"), {std::to_string(seconds / 86400),
                                               std::to_string((seconds % 86400) / 3600)});
        if (seconds >= 3600)
            return fill(tr("{0}h {1}m left"),
                        {std::to_string(seconds / 3600), std::to_string((seconds % 3600) / 60)});
        char clock[32];
        std::snprintf(clock, sizeof(clock), "%lld:%02lld", seconds / 60, seconds % 60);
        return fill(trc("time", "{0} left"), {clock});
    }

    void draw_shelf_art(app::Context &ctx, ui::Canvas &canvas, const Rect &art, float radius,
                        const ui::CardItem &item) const
    {
        if (item.tag < 0 || item.tag >= static_cast<int>(entries_.size()))
            return;
        const Entry &entry = entries_[static_cast<std::size_t>(item.tag)];
        gfx::DrawList &list = canvas.list;
        const ui::Theme &theme = ctx.theme();
        ui::Painter paint(list, canvas.fonts, theme, canvas.glass);
        const Color accent = app::look::accent(entry.section());
        list.gradient_rect(art, radius, gfx::mix(theme.page, accent, 0.2f).with_alpha(0.92f),
                           gfx::mix(theme.page, accent, 0.05f).with_alpha(0.92f));
        list.bordered_rect(art, radius, app::look::kClear, 1.5f, app::look::kInk.with_alpha(0.1f));
        const float side = art.h - 32.0f;
        const Rect squares{art.x + 16.0f, art.y + 16.0f, side, side};
        const float tx = squares.x + side + 22.0f;
        const float room = art.x + art.w - tx - 14.0f;
        const float cx = squares.cx();
        const float cy = squares.cy();
        switch (entry.kind)
        {
        case Kind::storm:
        case Kind::streak:
            list.circle(cx, cy, side * 0.5f, accent.with_alpha(0.16f));
            list.ring(cx, cy, side * 0.5f, 3.0f, accent);
            draw_sign(list, entry.kind == Kind::storm ? Figure::Sign::bolt : Figure::Sign::steps,
                      cx, cy, side * 0.5f, accent);
            break;
        default:
            board::draw_mini_board(list, *ctx.pieces, ctx.board_theme(), squares, entry.position,
                                   entry.orientation, entry.last_move, 5.0f);
            break;
        }
        if (entry.kind == Kind::tv)
        {
            ui::Badge badge = live_;
            const float width = badge.width(paint);
            badge.set_bounds({art.x + art.w - 14.0f - width, art.y + 14.0f, width, 28.0f});
            badge.draw(canvas);
        }
        // Three lines beside the thumbnail. When the middle one is too long
        // for the room even with smaller letters, it takes two lines and its
        // neighbours move apart.
        const ui::Fonts &fonts = canvas.fonts;
        const app::look::Fitted big = app::look::fit_lines(fonts.semibold, entry.big, 24.0f, room);
        const bool two = !big.second.empty();
        app::look::kicker(list, fonts, kind_word(entry.kind), tx, art.cy() - (two ? 44.0f : 34.0f),
                          accent, Align::left, 13.0f, room);
        ui::text(list, fonts.semibold, big.first, tx, art.cy() + (two ? -8.0f : 2.0f), big.size,
                 theme.text);
        if (two)
            ui::text(list, fonts.semibold, big.second, tx, art.cy() - 5.0f + big.size, big.size,
                     theme.text);
        ui::text_fit(list, fonts.regular, entry.small, tx, art.cy() + (two ? 44.0f : 32.0f), 19.0f,
                     room, theme.text_muted);
    }

    ui::PushButton primary_;
    ui::PushButton secondary_;
    std::array<Fact, 3> facts_;
    std::array<Figure, 3> figures_;
    ui::Carousel shelf_;
    ui::Badge live_;

    Zone zone_ = Zone::hero;
    int button_ = 0;
    float refresh_in_ = 0.0f;
    float since_ = 0.0f;     // seconds since the page was entered: parts arrive by it
    bool calm_ = false;      // reduced motion
    tween::Spring hero_lit_; // 0..1: the hero has the focus
    puzzles::Records records_;
    bool daily_ = false;
    chess::Position hero_position_ = chess::Position::start();
    chess::Move hero_last_;
    chess::Color hero_orientation_ = chess::Color::white;
    std::string hero_kicker_;
    std::string hero_title_;
    std::string hero_body_;
    std::vector<Entry> entries_;
    std::string shelf_signature_;
};

} // namespace

std::unique_ptr<Page> make_home_page(app::Context &ctx)
{
    return std::make_unique<HomePage>(ctx);
}

} // namespace pch::modes
