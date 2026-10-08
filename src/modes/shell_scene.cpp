// ProsperoLichess - The home screen: a side rail and the page it shows.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/chrome.hpp"
#include "core/strings.hpp"
#include "lichess/session.hpp"
#include "modes/page.hpp"
#include "modes/scenes.hpp"
#include "modes/rail.hpp"

#include <array>
#include <memory>

namespace pch::modes
{

namespace
{

class ShellScene final : public app::Scene
{
  public:
    explicit ShellScene(app::Context &ctx)
    {
        using app::look::Section;
        nav_.set_entries({{tr("Home"), app::NavIcon::home, Section::home},
                          {tr("Puzzles"), app::NavIcon::puzzles, Section::puzzles},
                          {tr("Play"), app::NavIcon::play, Section::play},
                          {tr("Watch"), app::NavIcon::watch, Section::watch},
                          {tr("Profile"), app::NavIcon::profile, Section::profile},
                          {tr("Settings"), app::NavIcon::settings, Section::settings}});
        nav_.set_bounds(app::kRail);
        nav_.set_current(kPageHome, true);
        nav_.set_focus(kPageHome);

        pages_[kPageHome] = make_home_page(ctx);
        pages_[kPagePuzzles] = make_puzzles_page(ctx);
        pages_[kPagePlay] = make_play_page(ctx);
        pages_[kPageWatch] = make_watch_page(ctx);
        pages_[kPageProfile] = make_profile_page(ctx);
        pages_[kPageSettings] = make_settings_page(ctx);
        // The app opens with the controller in the home page.
        in_page_ = true;
        nav_.set_focused(false);
        arrive_.snap(1.0f);
    }

    void enter(app::Context &ctx) override
    {
        page().enter(ctx);
    }

    app::Transition update(app::Context &ctx, const InputFrame &input, float dt) override
    {
        nav_.set_note(note(ctx));
        // A live game on TV marks the Watch entry.
        nav_.entry(kPageWatch).live = ctx.lichess != nullptr && ctx.lichess->online();

        app::Transition transition;
        const bool answered = input.is_pressed(Action::touch) && answer_bell(ctx, &transition);
        // L1 and R1 turn a page's tabs from the rail too: the page takes the
        // controller and changes tab.
        if (!answered && !in_page_ && page().tabbed() &&
            (input.is_pressed(Action::page_prev) || input.is_pressed(Action::page_next)))
        {
            in_page_ = true;
            nav_.set_focused(false);
        }
        if (answered)
        {
            page().update(ctx, InputFrame{}, dt, in_page_);
        }
        else if (in_page_)
        {
            // Options is the way back to the rail from anywhere in a page.
            if (input.is_pressed(Action::menu))
            {
                ctx.cue(audio::Cue::back);
                to_rail();
                page().update(ctx, InputFrame{}, dt, false);
            }
            else
            {
                PageResult result = page().update(ctx, input, dt, true);
                if (result.go_to >= 0 && result.go_to < kPageCount)
                    show(ctx, result.go_to, true);
                else if (result.to_rail)
                    to_rail();
                transition = std::move(result.transition);
            }
        }
        else
        {
            const ui::Event event =
                input.nav == Direction::right ? ui::Event::activated : nav_.handle(input, ctx);
            if (event == ui::Event::moved)
            {
                show(ctx, nav_.focus(), false);
            }
            else if (event == ui::Event::activated)
            {
                if (input.nav == Direction::right)
                    ctx.cue(audio::Cue::select);
                in_page_ = true;
                nav_.set_focused(false);
            }
            else if (event == ui::Event::cancelled && current_ != kPageHome)
            {
                nav_.set_focus(kPageHome);
                show(ctx, kPageHome, false);
            }
            page().update(ctx, InputFrame{}, dt, false);
        }
        nav_.update(ctx, dt);
        arrive_.target = 1.0f;
        arrive_.update(dt, ctx.reduced_motion() ? 60.0f : 14.0f);
        return transition;
    }

    void draw(app::Context &ctx, app::Frame &frame) const override
    {
        nav_.draw(ctx, frame.scene);

        // A page that was just chosen fades in from a few pixels lower.
        const float t = arrive_.value;
        const bool moving = t < 0.995f;
        if (moving)
        {
            for (gfx::DrawList *list : {&frame.scene, &frame.overlay})
            {
                list->push_opacity(t);
                list->push_transform(1.0f, 0.0f, 0.0f, 0.0f,
                                     ctx.reduced_motion() ? 0.0f : 18.0f * (1.0f - t));
            }
        }
        page().draw(ctx, frame, in_page_);
        if (moving)
        {
            for (gfx::DrawList *list : {&frame.scene, &frame.overlay})
            {
                list->pop_transform();
                list->pop_opacity();
            }
        }

        if (in_page_)
        {
            const std::span<const ui::Hint> hints = page().hints();
            app::draw_hints(ctx, frame.scene, hints.data(), static_cast<int>(hints.size()));
        }
        else
        {
            const ui::Hint hints[] = {{ui::Button::cross, TR("Open")},
                                      {ui::Button::dpad, TR("Choose")}};
            app::draw_hints(ctx, frame.scene, hints, 2);
        }
    }

    void tick(app::Context &ctx, float dt) override
    {
        page().tick(ctx, dt);
    }

    app::look::Mood mood() const override
    {
        return page().mood();
    }

    // The strip names the page (its icons are the rail's, in the same order)
    // and translates the name: title() gives the English one, marked TR().
    app::Place place() const override
    {
        return {page().title(), current_, true};
    }

    const char *name() const override
    {
        return page().name();
    }

  private:
    // The games in which it is the player's move.
    struct Waiting
    {
        int count = 0;
        const lichess::OngoingGame *first = nullptr;
    };
    static Waiting waiting_games(const app::Context &ctx)
    {
        Waiting waiting;
        const lichess::Session *session = ctx.lichess;
        if (session == nullptr || !session->signed_in())
            return waiting;
        for (const lichess::OngoingGame &game : session->ongoing())
        {
            if (!game.my_turn)
                continue;
            if (waiting.first == nullptr)
                waiting.first = &game;
            ++waiting.count;
        }
        return waiting;
    }

    // The Touchpad opens what the strip's bell counts: the game that waits for
    // a move, or the list of them on Profile when there are several. False
    // when no game waits.
    bool answer_bell(app::Context &ctx, app::Transition *transition)
    {
        const Waiting waiting = waiting_games(ctx);
        if (waiting.count == 0)
            return false;
        if (waiting.count == 1)
        {
            ctx.cue(audio::Cue::launch);
            *transition = app::Transition::push(make_online_game(ctx, waiting.first->game_id));
        }
        else
        {
            ctx.cue(audio::Cue::select);
            show(ctx, kPageProfile, true);
        }
        return true;
    }

    // What the rail's card says: a game that waits, or why online play is out
    // of reach. Nothing when all is well.
    static RailNote note(const app::Context &ctx)
    {
        using app::look::Section;
        const lichess::Session *session = ctx.lichess;
        if (session == nullptr || !session->online())
            return {tr("Offline"), tr("No connection"), tr("Puzzles still work"), Section::watch};
        if (!session->signed_in())
            return session->signing_in() ? RailNote{}
                                         : RailNote{tr("Guest"), tr("Not signed in"),
                                                    tr("Sign in under Profile"), Section::profile};
        const Waiting games = waiting_games(ctx);
        if (games.count == 0)
            return {};
        const int waiting = games.count;
        const lichess::OngoingGame *first = games.first;
        const std::string opponent =
            first->ai_level > 0 ? fill(tr("Stockfish level {0}"), {std::to_string(first->ai_level)})
                                : first->opponent;
        return {tr("Your move"), plural(TR("{0} game waits"), TR("{0} games wait"), waiting),
                fill(waiting > 1 ? tr("vs {0} and more") : tr("vs {0}"), {opponent}),
                Section::play};
    }

    Page &page() const
    {
        return *pages_[static_cast<std::size_t>(current_)];
    }

    void show(app::Context &ctx, int index, bool focus_page)
    {
        if (index != current_)
        {
            current_ = index;
            arrive_.snap(0.0f);
            page().enter(ctx);
        }
        nav_.set_current(index);
        nav_.set_focus(index);
        in_page_ = focus_page;
        nav_.set_focused(!focus_page);
    }

    void to_rail()
    {
        in_page_ = false;
        nav_.set_focus(current_);
        nav_.set_focused(true);
    }

    Rail nav_;
    std::array<std::unique_ptr<Page>, kPageCount> pages_;
    int current_ = kPageHome;
    bool in_page_ = true;
    tween::Spring arrive_; // 0 -> 1 as a newly chosen page settles
};

} // namespace

std::unique_ptr<app::Scene> make_shell(app::Context &ctx)
{
    return std::make_unique<ShellScene>(ctx);
}

} // namespace pch::modes
