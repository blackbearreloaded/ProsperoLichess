// ProsperoLichess - Host snapshots: waiting for a game, the game screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_file.hpp"
#include "core/strings.hpp"
#include "lichess/session.hpp"
#include "modes/game_scene.hpp"
#include "modes/game_script_link.hpp"
#include "modes/play_setup.hpp"
#include "modes/scenes.hpp"
#include "scenarios.hpp"

#include <memory>
#include <string_view>

namespace pch::host
{

namespace
{

// Where the board cursor is and which way the board faces, followed by the
// script: Pass & Play turns the board after every move.
struct Pad
{
    chess::Square cursor = chess::make_square(4, 1);
    chess::Color view = chess::Color::white;
};

void go(Run &run, Pad &pad, const char *square)
{
    const chess::Square target = chess::parse_square(square);
    int files = chess::file_of(target) - chess::file_of(pad.cursor);
    int ranks = chess::rank_of(target) - chess::rank_of(pad.cursor);
    if (pad.view == chess::Color::black)
    {
        files = -files;
        ranks = -ranks;
    }
    run.nav(files > 0 ? Direction::right : Direction::left, files > 0 ? files : -files);
    run.nav(ranks > 0 ? Direction::up : Direction::down, ranks > 0 ? ranks : -ranks);
    pad.cursor = target;
}

// Picks up the piece on from and places it on to. turns: the board then turns
// for the other player and the cursor keeps its place on screen.
void play(Run &run, Pad &pad, const char *from, const char *to, bool turns)
{
    go(run, pad, from);
    run.press(Action::confirm);
    go(run, pad, to);
    run.press(Action::confirm);
    if (!turns)
        return;
    run.idle(60);
    pad.cursor = static_cast<chess::Square>(63 - pad.cursor);
    pad.view = chess::opposite(pad.view);
}

void open_local(Run &run)
{
    run.open(modes::make_local_game(run.app.context()));
    run.idle(60);
}

// The Play page's time controls by their figure, so the waiting screens are
// opened with what the page itself would send and say.
const modes::TimeControl &pool(std::string_view label)
{
    for (const modes::TimeControl &tc : modes::pairing_pools())
    {
        if (label == tc.label)
            return tc;
    }
    return modes::pairing_pools()[0];
}

const modes::TimeControl &ai_clock(std::string_view label)
{
    for (const modes::TimeControl &tc : modes::ai_clocks())
    {
        if (label == tc.label)
            return tc;
    }
    return modes::ai_clocks()[0];
}

std::unique_ptr<app::Scene> seek(std::string_view label, bool rated)
{
    const modes::GameRequest request = modes::seek_request(pool(label), rated);
    return modes::make_seek(request.form, request.label);
}

std::unique_ptr<app::Scene> ai_game(std::string_view label, int level, modes::AiColor color)
{
    const modes::GameRequest request = modes::ai_request(ai_clock(label), level, color);
    return modes::make_ai_game(request.form, request.label);
}

// A rated rapid game in its opening, the signed-in player with White to move.
std::shared_ptr<modes::GameScript> rapid_game()
{
    auto script = std::make_shared<modes::GameScript>();
    script->white.name = "blackbear";
    script->white.rating = 1834;
    script->black.name = "NordicKnight";
    script->black.title = "FM";
    script->black.rating = 2290;
    script->moves = "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 c1e3 e7e5 d4b3 c8e6";
    script->clock_ms[0] = 307000;
    script->clock_ms[1] = 292000;
    script->speed = modes::seek_request(pool("10+5"), true).label;
    return script;
}

void open_online(Run &run, const std::shared_ptr<modes::GameScript> &script)
{
    run.open(modes::make_linked_game(modes::make_script_link(script)));
    run.idle(40);
}

} // namespace

void add_game_scenarios(Scenarios &all)
{
    // ---- Pass & Play ----
    all.push_back({"game-local", [](Run &run) { open_local(run); }});
    all.push_back({"game-local-moves", [](Run &run)
                   {
                       open_local(run);
                       Pad pad;
                       play(run, pad, "e2", "e4", true);
                       play(run, pad, "e7", "e5", true);
                       // White picks up the g1 knight: its destinations show.
                       go(run, pad, "g1");
                       run.press(Action::confirm);
                       run.idle(30);
                   }});
    all.push_back({"game-local-actions", [](Run &run)
                   {
                       open_local(run);
                       Pad pad;
                       play(run, pad, "d2", "d4", true);
                       run.press(Action::north);
                       run.nav(Direction::right);
                       run.idle(30);
                   }});
    all.push_back({"game-local-menu", [](Run &run)
                   {
                       open_local(run);
                       run.press(Action::menu);
                       run.nav(Direction::down, 3);
                       run.idle(40);
                   }});
    all.push_back({"game-local-mate", [](Run &run)
                   {
                       open_local(run);
                       Pad pad;
                       // Scholar's mate.
                       play(run, pad, "e2", "e4", true);
                       play(run, pad, "e7", "e5", true);
                       play(run, pad, "f1", "c4", true);
                       play(run, pad, "b8", "c6", true);
                       play(run, pad, "d1", "h5", true);
                       play(run, pad, "g8", "f6", true);
                       play(run, pad, "h5", "f7", false);
                       run.idle(20);
                       run.shot("board");
                       run.idle(70);
                       run.shot("confetti");
                       run.idle(200);
                       run.shot("result");
                       // Review game: the first move, then two steps on.
                       run.nav(Direction::right);
                       run.press(Action::confirm);
                       run.press(Action::jump_next);
                       run.press(Action::jump_next);
                       run.idle(40);
                       run.shot("review");
                       // Back at the final position: the row offers the result again.
                       run.press(Action::confirm);
                       run.press(Action::north);
                       run.idle(40);
                   }});
    // A saved game is resumed where it stopped: White is about to promote,
    // and with auto-queen off the picker asks which piece.
    all.push_back({"game-local-promotion", [](Run &run)
                   {
                       app::Context &ctx = run.app.context();
                       save::write_atomic(ctx.data_root + "/passplay.sav",
                                          save::encode(save::Kind::game, 1,
                                                       "h2h4 g7g5 h4g5 h7h6 g5h6 f8g7 h6g7 a7a6"));
                       ctx.settings->auto_queen = false;
                       open_local(run);
                       Pad pad;
                       go(run, pad, "g7");
                       run.press(Action::confirm);
                       go(run, pad, "h8");
                       run.press(Action::confirm);
                       run.nav(Direction::down);
                       run.idle(30);
                   }});
    all.push_back({"game-local-resign", [](Run &run)
                   {
                       open_local(run);
                       Pad pad;
                       play(run, pad, "e2", "e4", true);
                       // Black holds Square: half-way through the hold, then all of it.
                       InputFrame hold;
                       hold.pressed = action_bit(Action::west);
                       hold.held = action_bit(Action::west);
                       hold.connected = true;
                       run.app.update(hold, 1.0f / 60.0f);
                       hold.pressed = 0;
                       for (int i = 0; i < 40; ++i)
                           run.app.update(hold, 1.0f / 60.0f);
                       run.shot("holding");
                       for (int i = 0; i < 50; ++i)
                           run.app.update(hold, 1.0f / 60.0f);
                       run.idle(120);
                   }});

    // ---- waiting for a game ----
    all.push_back({"game-seek",
                   [](Run &run)
                   {
                       run.open(seek("10+5", true));
                       run.idle(150);
                   },
                   true});
    all.push_back({"game-ai-start",
                   [](Run &run)
                   {
                       run.open(ai_game("10+5", 3, modes::AiColor::random));
                       run.idle(100);
                   },
                   true});
    // Signed out: the screen says why no game can start.
    all.push_back({"game-ai-error", [](Run &run)
                   {
                       run.open(ai_game("Unlimited", 3, modes::AiColor::random));
                       run.idle(120);
                   }});
    // The waiting screen a few frames after it opened: the radar, the board
    // and the ticket's parts are arriving.
    all.push_back({"game-seek-arriving",
                   [](Run &run)
                   {
                       run.app.open(seek("30+20", true));
                       run.idle(14);
                       run.shot("early");
                       run.idle(12);
                   },
                   true});
    // A correspondence seek, casual: another colour, another sign.
    all.push_back({"game-seek-correspondence",
                   [](Run &run)
                   {
                       run.open(seek("3 days", false));
                       run.idle(200);
                   },
                   true});
    // Stockfish with Black and no clock: the board is set up from Black's side.
    all.push_back({"game-ai-start-black",
                   [](Run &run)
                   {
                       run.open(ai_game("Unlimited", 8, modes::AiColor::black));
                       run.idle(90);
                   },
                   true});

    // ---- a Lichess game (scripted: no network) ----
    all.push_back({"game-online-loading",
                   [](Run &run)
                   {
                       auto script = rapid_game();
                       script->ready = false;
                       open_online(run, script);
                       run.idle(40);
                   },
                   true});
    all.push_back({"game-online-failed",
                   [](Run &run)
                   {
                       auto script = rapid_game();
                       script->ready = false;
                       script->error =
                           tr("Lichess is not answering. Still trying; press Circle to go back.");
                       open_online(run, script);
                       run.idle(60);
                   },
                   true});
    all.push_back({"game-online",
                   [](Run &run)
                   {
                       auto script = rapid_game();
                       open_online(run, script);
                       // The f1 bishop in hand.
                       Pad pad;
                       go(run, pad, "f1");
                       run.press(Action::confirm);
                       run.idle(30);
                       run.shot("move");
                       run.press(Action::back);
                       run.press(Action::north);
                       run.idle(30);
                       run.shot("actions");
                       run.press(Action::menu);
                       run.idle(40);
                       run.shot("menu");
                       run.press(Action::back);
                       run.press(Action::back);
                       // The stream drops: the link says so and the session shows its notice.
                       script->connected = false;
                       run.app.session().set_issue(
                           "game", tr("Connection to the game lost. Reconnecting\xE2\x80\xA6"));
                       run.idle(60);
                       run.shot("reconnecting");
                       script->connected = true;
                       run.app.session().clear_issue("game", false);
                       // Our own draw offer waits for an answer.
                       script->draw_offer[0] = true;
                       script->touch();
                       run.idle(60);
                   },
                   true});
    all.push_back({"game-online-offer",
                   [](Run &run)
                   {
                       auto script = rapid_game();
                       open_online(run, script);
                       script->draw_offer[1] = true;
                       script->touch();
                       // The card on its way in, then at rest.
                       run.idle(5);
                       run.shot("arriving");
                       run.idle(45);
                       run.shot("draw");
                       run.press(Action::confirm); // declines
                       script->draw_offer[1] = false;
                       script->takeback_offer[1] = true;
                       script->touch();
                       run.idle(50);
                       run.nav(Direction::right);
                       run.idle(20);
                   },
                   true});
    all.push_back({"game-online-low-time",
                   [](Run &run)
                   {
                       auto script = rapid_game();
                       script->clock_ms[0] = 8400;
                       script->clock_ms[1] = 41000;
                       script->opponent_gone = 12;
                       open_online(run, script);
                       run.idle(40);
                   },
                   true});
    all.push_back({"game-online-early",
                   [](Run &run)
                   {
                       auto script = rapid_game();
                       script->moves.clear();
                       script->me = chess::Color::black;
                       script->white = script->black;
                       script->black.name = "blackbear";
                       script->black.title.clear();
                       script->black.rating = 1834;
                       script->clock_ms[0] = 600000;
                       script->clock_ms[1] = 600000;
                       open_online(run, script);
                       run.idle(40);
                   },
                   true});
    all.push_back({"game-online-ai",
                   [](Run &run)
                   {
                       auto script = rapid_game();
                       script->black = modes::PlayerInfo{};
                       script->black.name = "Stockfish";
                       script->black.ai_level = 3;
                       script->rated = false;
                       script->has_clock = false;
                       script->speed =
                           std::string(tr("Correspondence")) + "  \xC2\xB7  " + tr("Casual");
                       open_online(run, script);
                       run.idle(40);
                   },
                   true});
    all.push_back({"game-online-result",
                   [](Run &run)
                   {
                       auto script = rapid_game();
                       script->moves = "e2e4 e7e5 f1c4 b8c6 d1h5 g8f6";
                       open_online(run, script);
                       // Qxf7#: our move comes back from the server with the result.
                       Pad pad;
                       go(run, pad, "h5");
                       run.press(Action::confirm);
                       go(run, pad, "f7");
                       script->echo_moves = false;
                       run.press(Action::confirm);
                       script->moves += " h5f7";
                       script->status = modes::GameStatus::mate;
                       script->winner = 0;
                       script->touch();
                       // The result opens once the mate has been seen; its
                       // ceremony half-way (the mark has landed, the verdict
                       // is rising, the rest is still to come), then all of it.
                       run.idle(74);
                       run.shot("ceremony");
                       run.idle(186);
                   },
                   true});
    // The game a few frames after it opened: the board first, then the column.
    all.push_back({"game-online-arriving",
                   [](Run &run)
                   {
                       run.app.open(modes::make_linked_game(modes::make_script_link(rapid_game())));
                       run.idle(12);
                   },
                   true});
    // A draw, and a game that was aborted: the neutral verdicts.
    all.push_back({"game-online-draw",
                   [](Run &run)
                   {
                       auto script = rapid_game();
                       open_online(run, script);
                       script->status = modes::GameStatus::draw;
                       script->touch();
                       run.idle(200);
                   },
                   true});
    // Reduced motion: the result is simply there when it opens.
    all.push_back({"game-online-result-calm",
                   [](Run &run)
                   {
                       run.app.context().settings->reduced_motion = true;
                       auto script = rapid_game();
                       open_online(run, script);
                       script->status = modes::GameStatus::resign;
                       script->winner = 0;
                       script->touch();
                       run.idle(64);
                   },
                   true});
    all.push_back({"game-online-defeat",
                   [](Run &run)
                   {
                       auto script = rapid_game();
                       open_online(run, script);
                       script->status = modes::GameStatus::outoftime;
                       script->winner = 1;
                       script->clock_ms[0] = 0;
                       script->touch();
                       run.idle(200);
                   },
                   true});
}

} // namespace pch::host
