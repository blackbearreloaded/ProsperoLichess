// ProsperoLichess - A Lichess Board API game: stream, clocks, moves and offers.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lichess/board_link.hpp"

#include "core/strings.hpp"
#include "lichess/json.hpp"
#include "platform/ps5/system.hpp"

#include <algorithm>
#include <cctype>

namespace pch::lichess
{

namespace
{

using modes::GameStatus;

constexpr long long kNoClock = 2147483647LL;

void read_player(const Value &v, modes::PlayerInfo *info, std::string *id)
{
    if (!v.exists())
        return;
    info->name = v["name"].str(v["id"].str());
    info->title = v["title"].str();
    info->rating = static_cast<int>(v["rating"].integer());
    info->provisional = v["provisional"].boolean();
    info->ai_level = static_cast<int>(v["aiLevel"].integer());
    if (info->ai_level > 0 && info->name.empty())
        info->name = "Stockfish";
    *id = v["id"].str();
}

void read_state(const Value &v, BoardState *state)
{
    state->moves = v["moves"].str();
    state->wtime = v["wtime"].integer(-1);
    state->btime = v["btime"].integer(-1);
    state->winc = v["winc"].integer();
    state->binc = v["binc"].integer();
    state->status = parse_status(v["status"].str());
    const std::string winner = v["winner"].str();
    state->winner = winner == "white" ? 0 : winner == "black" ? 1 : -1;
    state->wdraw = v["wdraw"].boolean();
    state->bdraw = v["bdraw"].boolean();
    state->wtakeback = v["wtakeback"].boolean();
    state->btakeback = v["btakeback"].boolean();
}

// The speed's name in the player's language. Lichess sends a key: "rapid",
// "ultraBullet".
std::string speed_name(const std::string &key)
{
    struct Named
    {
        std::string_view key;
        const char *name;
    };
    static constexpr Named kNames[] = {{"ultraBullet", TR("UltraBullet")},
                                       {"bullet", TR("Bullet")},
                                       {"blitz", TR("Blitz")},
                                       {"rapid", TR("Rapid")},
                                       {"classical", TR("Classical")},
                                       {"correspondence", TR("Correspondence")}};
    for (const Named &named : kNames)
    {
        if (key == named.key)
            return tr(named.name);
    }
    // A speed this app has no name for yet: Lichess's own word, with a capital.
    std::string name = key;
    if (!name.empty())
        name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    return name;
}

// "Rapid 10+5  ·  Rated": the speed's name, the time control, and whether
// the game counts for the rating. The game screen reads the speed from the
// name the label starts with.
std::string speed_label(const BoardState &s)
{
    std::string text;
    if (s.clock_initial >= 0)
        text = fill(trc("speed, time control", "{0} {1}"),
                    {speed_name(s.speed), std::to_string(s.clock_initial / 60000) + "+" +
                                              std::to_string(s.clock_increment / 1000)});
    else if (s.days_per_turn > 0)
        text = fill(trc("speed, time control", "{0} {1}"),
                    {tr("Correspondence"), plural(TR("{0} day"), TR("{0} days"), s.days_per_turn)});
    else
        text = speed_name(s.speed);
    return text + "  \xC2\xB7  " + (s.rated ? tr("Rated") : tr("Casual"));
}

class BoardLink : public modes::GameLink
{
  public:
    BoardLink(Session &session, std::string id) : session_(session), id_(std::move(id))
    {
        open();
    }
    ~BoardLink() override
    {
        if (stream_ != 0)
            session_.cancel(stream_);
        for (std::uint64_t request : requests_)
            session_.cancel(request);
        session_.clear_issue("game", false);
    }

    void pump(app::Context &, float dt) override
    {
        time_ += dt;
        if (reconnect_ > 0.0)
        {
            reconnect_ -= dt;
            if (reconnect_ <= 0.0)
                open();
        }
    }
    bool ready() const override
    {
        return state_.full;
    }
    std::string error() const override
    {
        return error_;
    }
    bool connected() const override
    {
        return stream_ != 0 && !dropped_;
    }
    std::uint64_t version() const override
    {
        return version_;
    }
    const std::string &initial_fen() const override
    {
        return state_.initial_fen;
    }
    const std::string &moves() const override
    {
        return state_.moves;
    }
    chess::Color my_color() const override
    {
        const std::string &me = session_.account().id;
        if (!state_.black_id.empty() && state_.black_id == me)
            return chess::Color::black;
        return chess::Color::white;
    }
    modes::PlayerInfo player(chess::Color color) const override
    {
        return color == chess::Color::white ? state_.white : state_.black;
    }
    bool has_clock() const override
    {
        return state_.wtime >= 0 && state_.wtime < kNoClock;
    }
    long long clock_ms(chess::Color color) const override
    {
        long long ms = color == chess::Color::white ? state_.wtime : state_.btime;
        // The side to move counts down from when the state arrived.
        if (state_.status == GameStatus::playing && color == to_move() && ply_count() >= 2)
            ms -= static_cast<long long>((time_ - received_) * 1000.0);
        if (optimistic_ && color == my_color())
            ms = frozen_ms_;
        return std::max(0LL, ms);
    }
    GameStatus status() const override
    {
        return state_.status == GameStatus::waiting && state_.full ? GameStatus::playing
                                                                   : state_.status;
    }
    int winner() const override
    {
        return state_.winner;
    }
    bool rated() const override
    {
        return state_.rated;
    }
    std::string speed_label() const override
    {
        return lichess::speed_label(state_);
    }
    bool draw_offered_by(chess::Color color) const override
    {
        return color == chess::Color::white ? state_.wdraw : state_.bdraw;
    }
    bool takeback_offered_by(chess::Color color) const override
    {
        return color == chess::Color::white ? state_.wtakeback : state_.btakeback;
    }
    int opponent_gone_seconds() const override
    {
        if (state_.opponent_gone < 0)
            return -1;
        return std::max(0, state_.opponent_gone - static_cast<int>(time_ - gone_received_));
    }

    void send_move(const chess::Move &move) override
    {
        frozen_ms_ = clock_ms(my_color());
        optimistic_ = true;
        const std::string uci = chess::to_uci(move);
        sys::log("[PCH] game %s move %s", id_.c_str(), uci.c_str());
        track(session_.post(
            "/api/board/game/" + id_ + "/move/" + uci, "",
            [this](const HttpResult &r)
            {
                if (!r.ok())
                {
                    optimistic_ = false;
                    refused_ = true;
                    refusal_ = message_of(r);
                    sys::log("[PCH] move refused status=%d %s", r.status, refusal_.c_str());
                }
            },
            true));
    }
    void resign() override
    {
        action("resign");
    }
    void abort() override
    {
        action("abort");
    }
    void draw(bool yes) override
    {
        action(yes ? "draw/yes" : "draw/no");
    }
    void takeback(bool yes) override
    {
        action(yes ? "takeback/yes" : "takeback/no");
    }
    void claim_victory() override
    {
        action("claim-victory");
    }
    bool take_refusal(std::string *message) override
    {
        if (!refused_)
            return false;
        refused_ = false;
        *message = refusal_;
        return true;
    }
    std::string game_id() const override
    {
        return id_;
    }

  private:
    static std::string message_of(const HttpResult &r)
    {
        if (r.status == 0)
            return tr("Connection problem");
        // What Lichess answers is in its own words.
        Document doc(r.body);
        std::string error = doc.root()["error"].str();
        return error.empty() ? tr("Lichess refused the request") : error;
    }

    std::size_t ply_count() const
    {
        if (state_.moves.empty())
            return 0;
        return static_cast<std::size_t>(std::count(state_.moves.begin(), state_.moves.end(), ' ')) +
               1;
    }
    chess::Color to_move() const
    {
        chess::Color start = chess::Color::white;
        if (!state_.initial_fen.empty() && state_.initial_fen != "startpos")
        {
            chess::Position position;
            if (chess::Position::from_fen(state_.initial_fen, &position))
                start = position.turn();
        }
        return ply_count() % 2 == 0 ? start : chess::opposite(start);
    }

    void track(std::uint64_t id)
    {
        requests_.push_back(id);
        if (requests_.size() > 32)
            requests_.erase(requests_.begin());
    }

    void action(const std::string &what)
    {
        sys::log("[PCH] game %s %s", id_.c_str(), what.c_str());
        track(session_.post(
            "/api/board/game/" + id_ + "/" + what, "",
            [this](const HttpResult &r)
            {
                if (!r.ok())
                {
                    refused_ = true;
                    refusal_ = message_of(r);
                }
            },
            true));
    }

    void open()
    {
        stream_ = session_.stream(
            "/api/board/game/stream/" + id_,
            [this](std::string_view line)
            {
                const bool was_full = state_.full;
                if (!apply_board_line(line, &state_))
                    return;
                if (line.find("\"opponentGone\"") != std::string_view::npos)
                    gone_received_ = time_;
                received_ = time_;
                optimistic_ = false;
                retries_ = 0;
                error_.clear();
                if (dropped_)
                {
                    dropped_ = false;
                    session_.clear_issue("game");
                }
                ++version_;
                if (!was_full && state_.full)
                    sys::log("[PCH] game %s full rated=%d speed=%s", id_.c_str(),
                             state_.rated ? 1 : 0, state_.speed.c_str());
            },
            [this](const HttpResult &r)
            {
                stream_ = 0;
                if (modes::is_over(state_.status))
                    return;
                if (!state_.full && r.status >= 400)
                {
                    error_ = message_of(r);
                    return;
                }
                // Dropped mid-game: reconnect; the new gameFull resyncs.
                sys::log("[PCH] game stream closed status=%d, reconnecting", r.status);
                reconnect_ = std::min(8.0, 1.0 + static_cast<double>(retries_++));
                if (state_.full)
                {
                    dropped_ = true;
                    session_.set_issue("game",
                                       tr("Connection to the game lost. Reconnecting\xE2\x80\xA6"));
                }
                else if (retries_ >= 3)
                {
                    // Never connected: say so, but keep trying in the background.
                    error_ = tr("Lichess is not answering. Still trying; press Circle to go back.");
                }
            });
    }

    Session &session_;
    std::string id_;
    std::uint64_t stream_ = 0;
    std::vector<std::uint64_t> requests_;
    BoardState state_;
    std::uint64_t version_ = 0;
    double time_ = 0.0;
    double received_ = 0.0;
    double gone_received_ = 0.0;
    double reconnect_ = 0.0;
    int retries_ = 0;
    bool optimistic_ = false;
    bool dropped_ = false; // lost the stream after the game had loaded
    long long frozen_ms_ = 0;
    bool refused_ = false;
    std::string refusal_;
    std::string error_;
};

} // namespace

GameStatus parse_status(std::string_view s)
{
    if (s == "created")
        return GameStatus::waiting;
    if (s == "started")
        return GameStatus::playing;
    if (s == "aborted")
        return GameStatus::aborted;
    if (s == "mate")
        return GameStatus::mate;
    if (s == "resign")
        return GameStatus::resign;
    if (s == "stalemate")
        return GameStatus::stalemate;
    if (s == "timeout")
        return GameStatus::timeout;
    if (s == "draw")
        return GameStatus::draw;
    if (s == "outoftime")
        return GameStatus::outoftime;
    if (s == "cheat")
        return GameStatus::cheat;
    if (s == "noStart")
        return GameStatus::no_start;
    if (s == "insufficientMaterialClaim")
        return GameStatus::insufficient;
    if (s == "variantEnd")
        return GameStatus::variant_end;
    if (s.empty())
        return GameStatus::playing;
    return GameStatus::unknown_end;
}

bool apply_board_line(std::string_view line, BoardState *state)
{
    Document doc(line);
    if (!doc.ok())
        return false;
    const Value root = doc.root();
    const std::string type = root["type"].str();
    if (type == "gameFull")
    {
        state->full = true;
        const std::string fen = root["initialFen"].str();
        state->initial_fen = fen == "startpos" ? std::string() : fen;
        read_player(root["white"], &state->white, &state->white_id);
        read_player(root["black"], &state->black, &state->black_id);
        state->rated = root["rated"].boolean();
        state->speed = root["speed"].str();
        const Value clock = root["clock"];
        state->clock_initial = clock.exists() ? clock["initial"].integer(-1) : -1;
        state->clock_increment = clock.exists() ? clock["increment"].integer() : 0;
        state->days_per_turn = static_cast<int>(root["daysPerTurn"].integer());
        state->opponent_gone = -1;
        read_state(root["state"], state);
        return true;
    }
    if (type == "gameState")
    {
        read_state(root, state);
        state->opponent_gone = -1;
        return true;
    }
    if (type == "opponentGone")
    {
        state->opponent_gone =
            root["gone"].boolean() ? static_cast<int>(root["claimWinInSeconds"].integer()) : -1;
        return true;
    }
    return type == "chatLine";
}

std::unique_ptr<modes::GameLink> make_board_link(Session &session, std::string game_id)
{
    return std::make_unique<BoardLink>(session, std::move(game_id));
}

} // namespace pch::lichess
