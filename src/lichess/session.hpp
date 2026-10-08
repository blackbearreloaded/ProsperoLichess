// ProsperoLichess - The Lichess connection: account, requests, streams and live state.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "chess/chess.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pch::net
{
class Client;
struct Response;
} // namespace pch::net

namespace pch::lichess
{

inline constexpr const char *kBaseUrl = "https://lichess.org";

struct Perf
{
    std::string key; // "rapid", "puzzle", ...
    int rating = 0;
    bool provisional = false;
    int games = 0;
    int progress = 0;           // rating change over the last twelve games
    std::vector<float> history; // ratings, oldest first (empty until fetched)
};

struct Account
{
    std::string id;
    std::string username;
    std::string title;
    std::vector<Perf> perfs;
    // Games played, as Lichess counts them (0 until known).
    int games = 0;
    int wins = 0;
    int losses = 0;
    int draws = 0;
    long long play_seconds = 0; // time spent playing
};

// A game in progress for the signed-in player (from /api/account/playing
// and the event stream).
struct OngoingGame
{
    std::string game_id;
    std::string fen;
    std::string last_move; // UCI
    chess::Color color = chess::Color::white;
    std::string opponent;
    int opponent_rating = 0;
    int ai_level = 0;
    bool my_turn = false;
    long long seconds_left = -1; // -1: no clock
    std::string speed;           // "Rapid": Lichess's key with a capital, see speed_name()
    bool rated = false;
};

// A game's speed ("Rapid", or "rapid" as Lichess writes it) in the player's
// language, for showing it. One it does not know comes back as it is.
std::string speed_name(std::string_view speed);

// The featured Lichess TV game, kept current while wanted.
struct TvGame
{
    bool valid = false;
    std::string id;
    std::string channel;
    chess::Position position = chess::Position::start();
    chess::Move last_move;
    chess::Color orientation = chess::Color::white;
    std::string white;
    std::string black;
    std::string white_title;
    std::string black_title;
    int white_rating = 0;
    int black_rating = 0;
    long long white_clock = -1; // seconds, at received
    long long black_clock = -1;
    double received = 0.0; // session time of the last update
    std::uint64_t version = 0;
};

// Events from the account's event stream.
struct Event
{
    enum class Kind
    {
        game_start,
        game_finish,
        challenge,
        challenge_gone,
    };
    std::uint64_t sequence = 0;
    Kind kind = Kind::game_start;
    std::string id;   // game or challenge id
    std::string text; // challenge description
};

// A short status line about the connection, shown as a floating pill.
struct Notice
{
    enum class Kind
    {
        none,
        warning, // something is wrong and being retried
        good,    // recovered
    };
    Kind kind = Kind::none;
    std::string text;
    bool busy = false; // show a spinner (work in progress)
};

struct HttpResult
{
    int status = 0; // 0 = transport failure
    std::string body;
    std::string error; // transport error text
    bool ok() const
    {
        return status >= 200 && status < 300;
    }
};

class Session
{
  public:
    using Callback = std::function<void(const HttpResult &)>;
    using LineCallback = std::function<void(std::string_view line)>;

    // connect=false keeps the session offline (host snapshots, tests).
    explicit Session(std::string data_root, bool connect = true);
    ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    void pump(float dt);
    double now() const
    {
        return time_;
    }

    bool online() const;
    bool signed_in() const
    {
        return !token_.empty() && !account_.id.empty();
    }
    const std::string &username() const
    {
        return account_.username;
    }
    const Account &account() const
    {
        return account_;
    }
    int rating(std::string_view perf) const; // 0 if unknown
    // The account's figures for one way to play, or nullptr.
    const Perf *perf(std::string_view key) const;

    // ---- requests (paths are relative to kBaseUrl unless absolute) ----
    std::uint64_t get(const std::string &path, Callback done);
    // A GET of another site's public file: never carries the account's token.
    std::uint64_t get_public(const std::string &url, const std::string &user_agent, Callback done);
    std::uint64_t post(const std::string &path, const std::string &form, Callback done,
                       bool game_lane = false);
    std::uint64_t post_json(const std::string &path, const std::string &json, Callback done);
    std::uint64_t del(const std::string &path, Callback done);
    // NDJSON stream; form non-empty sends a POST (seeks). on_close fires once.
    std::uint64_t stream(const std::string &path, LineCallback on_line, Callback on_close,
                         const std::string &form = {});
    // Forgets a request's callbacks (and closes it if it is a stream).
    void cancel(std::uint64_t id);
    double rate_limited_for() const;

    // ---- connection status for the player ----
    // The most important thing to tell the player right now (kind none = nothing).
    Notice notice() const;
    // Shows text for a few seconds.
    void flash(std::string text, Notice::Kind kind, double seconds = 4.0);
    // A lasting problem (a dropped stream being reconnected); cleared by its key.
    void set_issue(const std::string &key, std::string text);
    // announce shows a brief "Reconnected" when the issue existed.
    void clear_issue(const std::string &key, bool announce = true);
    bool has_issue(const std::string &key) const;
    // Status of the last failed account check: -1 none, 0 no connection, else HTTP status.
    int account_error_status() const
    {
        return account_error_status_;
    }

    // ---- account ----
    void sign_in(const std::string &token, bool personal);
    void sign_out();
    bool signing_in() const
    {
        return !token_.empty() && account_.id.empty() && !account_failed_;
    }

    // Host snapshots and tests only: shows the app as a signed-in player with
    // these games (and, when given, this daily puzzle) without any network.
    void preview(Account account, std::vector<OngoingGame> ongoing, std::string daily_json = {});

    // ---- live state ----
    const std::vector<OngoingGame> &ongoing() const
    {
        return ongoing_;
    }
    void refresh_ongoing();
    const std::deque<Event> &events() const
    {
        return events_;
    }
    std::uint64_t event_sequence() const
    {
        return event_sequence_;
    }
    // Keep a TV channel streaming ("" = the featured game).
    void watch_tv(const std::string &channel);
    void stop_tv();
    const TvGame &tv() const
    {
        return tv_;
    }
    // The daily puzzle JSON (empty until fetched; fetched at boot when online).
    const std::string &daily_json() const
    {
        return daily_json_;
    }

  private:
    struct Pending
    {
        Callback done;
        LineCallback line;
        bool stream = false;
        // The request went to lichess.org: only those say whether Lichess is
        // reachable, and only their 401 means the token was rejected.
        bool lichess = true;
    };

    std::uint64_t submit(int method, const std::string &path, const std::string &body,
                         const std::string &content_type, Callback done, bool game_lane);
    void load_token();
    void save_token() const;
    void fetch_account();
    void fetch_rating_history();
    void open_event_stream();
    void on_event_line(std::string_view line);
    void on_tv_line(std::string_view line);
    void mark_network(bool reachable);

    std::string root_;
    std::unique_ptr<net::Client> client_;
    std::unordered_map<std::uint64_t, Pending> pending_;
    double time_ = 0.0;
    std::string token_;
    bool personal_token_ = false;
    Account account_;
    bool account_failed_ = false;
    int reachable_ = -1;   // -1 unknown, 0 no, 1 yes
    bool preview_ = false; // preview(): nothing is sent, the given state stands
    double last_success_ = -1000.0;
    double retry_probe_ = 0.0;
    std::vector<OngoingGame> ongoing_;
    std::deque<Event> events_;
    std::uint64_t event_sequence_ = 0;
    std::uint64_t event_stream_ = 0;
    double event_retry_ = 0.0;
    std::uint64_t tv_stream_ = 0;
    std::string tv_channel_;
    bool tv_wanted_ = false;
    double tv_retry_ = 0.0;
    TvGame tv_;
    std::vector<std::pair<std::string, std::string>> issues_;
    std::string flash_text_;
    Notice::Kind flash_kind_ = Notice::Kind::none;
    double flash_until_ = -1.0;
    double last_failure_flash_ = -1000.0;
    double event_opened_ = -1.0; // when the event stream was (re)opened
    int event_failures_ = 0;
    int tv_failures_ = 0;
    int account_error_status_ = -1;
    int tv_moves_ = 0;
    std::string daily_json_;
};

} // namespace pch::lichess
