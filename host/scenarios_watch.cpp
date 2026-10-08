// ProsperoLichess - Host snapshots: the Watch page and Lichess TV.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/strings.hpp"
#include "chess/chess.hpp"
#include "lichess/session.hpp"
#include "modes/page.hpp"
#include "modes/scenes.hpp"
#include "modes/tv_feed.hpp"
#include "scenarios.hpp"

#include <initializer_list>

namespace pch::host
{

namespace
{

// The TV stream cannot be reached offline, so these pictures show a game
// handed to the screens directly (modes::set_tv_stand_in): a Najdorf, fed
// move by move the way the stream would.
class Broadcast
{
  public:
    Broadcast(Run &run, const char *channel) : run_(run)
    {
        game_.valid = true;
        game_.id = "pR3v1ewX";
        game_.channel = channel;
        chess::Position::from_fen(
            "r2q1rk1/1p1nbppp/p2pbn2/4p3/4P3/1NN1BP2/PPPQ2PP/2KR1B1R w - - 5 11", &game_.position);
        game_.white = "NordicKnight";
        game_.white_title = "IM";
        game_.white_rating = 2584;
        game_.white_clock = 148;
        game_.black = "silentrook";
        game_.black_title = "GM";
        game_.black_rating = 2671;
        game_.black_clock = 171;
        send();
    }

    // Plays moves (UCI) one at a time, leaving the screen time to animate each.
    void play(std::initializer_list<const char *> moves, int frames = 40)
    {
        for (const char *uci : moves)
        {
            chess::Move move;
            if (!chess::parse_uci(game_.position, uci, &move))
                continue;
            const bool white = game_.position.turn() == chess::Color::white;
            (white ? game_.white_clock : game_.black_clock) -= 4;
            game_.position = game_.position.after(move);
            game_.last_move = move;
            send();
            run_.idle(frames);
        }
    }

  private:
    void send()
    {
        ++game_.version;
        game_.received = run_.app.session().now();
        modes::set_tv_stand_in(&game_);
    }

    Run &run_;
    lichess::TvGame game_;
};

// Every scenario says what is on air first: a stand-in set by an earlier one
// would otherwise still be showing.
void off_air()
{
    modes::set_tv_stand_in(nullptr);
}

// Turns "Reduce motion" on the way a player would: Settings, Display, the
// third row.
void reduce_motion(Run &run)
{
    run.page(modes::kPageSettings);
    run.nav(Direction::down, 4);
    run.press(Action::confirm);
    run.nav(Direction::down, 2);
    run.nav(Direction::right);
    run.idle(10);
}

// From anywhere on the home screen: the rail shows a page and keeps the
// controller, and the picture is taken a few frames later.
void glance(Run &run, int page, int frames)
{
    run.press(Action::menu);
    run.nav(Direction::up, modes::kPageCount);
    run.nav(Direction::down, page);
    run.idle(frames);
}

void open_watch(Run &run)
{
    run.page(modes::kPageWatch);
    // The page assembles and its numbers count: wait for them.
    run.idle(110);
}

} // namespace

void add_watch_scenarios(Scenarios &all)
{
    // ---- the Watch page ----
    all.push_back({"watch",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       tv.play({"g2g4", "b7b5", "g4g5", "b5b4"}, 2);
                       open_watch(run);
                   },
                   true});
    // As the rail shows it, before the controller enters the page.
    all.push_back({"watch-rail",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       run.press(Action::menu);
                       run.nav(Direction::down, modes::kPageWatch);
                       run.idle(150);
                   },
                   true});
    // A moment after the rail showed it: the page is still assembling.
    all.push_back({"watch-arriving",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       run.press(Action::menu);
                       run.nav(Direction::down, modes::kPageWatch);
                       run.idle(9);
                   },
                   true});
    // With reduced motion nothing slides, counts or grows: a few frames after
    // the rail showed the page it is all there.
    all.push_back({"watch-reduced-motion",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       reduce_motion(run);
                       glance(run, modes::kPageWatch, 6);
                   },
                   true});
    // The controller just moved to the next channel: the ring glides, the
    // station's name slides, the game gives way to its bones.
    all.push_back({"watch-switching",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       open_watch(run);
                       InputFrame down;
                       down.connected = true;
                       down.nav = Direction::down;
                       run.app.update(down, 1.0f / 60.0f);
                       run.idle(5);
                   },
                   true});
    // Up from the first channel: the tile and its ring answer with a nudge.
    all.push_back({"watch-refused",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       open_watch(run);
                       InputFrame up;
                       up.connected = true;
                       up.nav = Direction::up;
                       run.app.update(up, 1.0f / 60.0f);
                       run.idle(2);
                   },
                   true});
    // A move of the game on air, as it lands.
    all.push_back({"watch-move",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       open_watch(run);
                       tv.play({"g2g4", "b7b5"}, 30);
                       tv.play({"g4g5"}, 4);
                   },
                   true});
    // Another channel was chosen and its first game has not arrived yet.
    all.push_back({"watch-loading",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       open_watch(run);
                       run.nav(Direction::down, 2);
                       run.idle(60);
                   },
                   true});
    // ... and now it has.
    all.push_back({"watch-rapid",
                   [](Run &run)
                   {
                       off_air();
                       open_watch(run);
                       run.nav(Direction::down, 2);
                       run.idle(40);
                       Broadcast tv(run, "rapid");
                       tv.play({"g2g4", "b7b5", "g4g5", "b5b4", "c3e2", "f6e8"}, 2);
                       run.idle(60);
                   },
                   true});
    all.push_back({"watch-reconnecting",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       open_watch(run);
                       run.app.session().set_issue(
                           "tv", tr("Lichess TV lost the feed. Reconnecting\xE2\x80\xA6"));
                       run.idle(60);
                   },
                   true});

    // ---- Lichess TV, full screen ----
    all.push_back({"tv",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       open_watch(run);
                       run.press(Action::confirm);
                       run.idle(50);
                       tv.play({"g2g4", "b7b5", "g4g5", "b5b4", "c3e2", "f6e8", "f3f4", "a6a5",
                                "f4f5", "a5a4", "b3d4", "e5d4", "e2d4"});
                       run.idle(30);
                   },
                   true});
    // The first position of a game, before any move was seen.
    // The screen was just opened: its parts are arriving.
    all.push_back({"tv-arriving",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       run.idle(20);
                       run.app.open(modes::make_tv(""));
                       run.idle(13);
                   },
                   true});
    all.push_back({"tv-reduced-motion",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       reduce_motion(run);
                       run.app.open(modes::make_tv(""));
                       run.idle(14);
                       tv.play({"g2g4", "b7b5"}, 3);
                   },
                   true});
    // A move as it lands: the last move rises in, its mark pops, the row flows.
    all.push_back({"tv-move",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       run.open(modes::make_tv(""));
                       run.idle(60);
                       tv.play({"g2g4", "b7b5", "g4g5", "b5b4", "c3e2", "f6e8", "f3f4", "a6a5",
                                "f4f5", "a5a4"});
                       tv.play({"b3d4"}, 5);
                   },
                   true});
    // R1 was just pressed: the plate glides to the next channel in its colour.
    all.push_back({"tv-switching",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       run.open(modes::make_tv(""));
                       tv.play({"g2g4", "b7b5"});
                       InputFrame next;
                       next.connected = true;
                       next.pressed = action_bit(Action::page_next);
                       next.held = action_bit(Action::page_next);
                       run.app.update(next, 1.0f / 60.0f);
                       run.idle(5);
                   },
                   true});
    all.push_back({"tv-first",
                   [](Run &run)
                   {
                       Broadcast tv(run, "blitz");
                       run.open(modes::make_tv("blitz"));
                       run.idle(40);
                   },
                   true});
    all.push_back({"tv-flipped",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       run.open(modes::make_tv(""));
                       tv.play({"g2g4", "b7b5", "g4g5"});
                       run.press(Action::west);
                       run.idle(60);
                   },
                   true});
    all.push_back({"tv-reconnecting",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       run.open(modes::make_tv(""));
                       tv.play({"g2g4", "b7b5"});
                       run.app.session().set_issue(
                           "tv", tr("Lichess TV lost the feed. Reconnecting\xE2\x80\xA6"));
                       run.idle(60);
                   },
                   true});
    // Another channel was chosen (R1): bones until its first game arrives.
    all.push_back({"tv-loading",
                   [](Run &run)
                   {
                       Broadcast tv(run, "");
                       run.open(modes::make_tv(""));
                       run.press(Action::page_next);
                       run.press(Action::page_next);
                       run.idle(60);
                   },
                   true});

    // ---- with the real lichess.org TV feed (PCH_ONLINE=1; it needs no account) ----
    all.push_back({"watch-live",
                   [](Run &run)
                   {
                       off_air();
                       open_watch(run);
                       run.idle(360);
                       run.shot("top");
                       run.nav(Direction::down);
                       run.idle(30);
                       run.shot("switching");
                       run.idle(360);
                   },
                   false, true});
    all.push_back({"tv-live",
                   [](Run &run)
                   {
                       off_air();
                       run.idle(120);
                       run.open(modes::make_tv("bullet"));
                       run.idle(600);
                       run.shot("early");
                       run.idle(900);
                   },
                   false, true});

    // ---- without a connection (these also leave nothing on air for what follows) ----
    all.push_back({"tv-offline", [](Run &run)
                   {
                       off_air();
                       run.open(modes::make_tv(""));
                       run.idle(40);
                   }});
    all.push_back({"watch-offline", [](Run &run)
                   {
                       off_air();
                       open_watch(run);
                   }});
    all.push_back({"watch-offline-arriving", [](Run &run)
                   {
                       off_air();
                       run.press(Action::menu);
                       run.nav(Direction::down, modes::kPageWatch);
                       run.idle(9);
                   }});
}

} // namespace pch::host
