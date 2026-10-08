// ProsperoLichess - The Lichess connection: account, requests, streams and live state.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lichess/session.hpp"

#include "core/save_file.hpp"
#include "core/strings.hpp"
#include "lichess/json.hpp"
#include "net/client.hpp"
#include "net/listener.hpp"
#include "net/redact.hpp"
#include "platform/ps5/system.hpp"

#include <algorithm>

namespace pch::lichess
{

namespace
{

constexpr int kGet = 0;
constexpr int kPost = 1;
constexpr int kDelete = 2;
constexpr const char *kTokenFile = "/account.bin";
constexpr std::size_t kMaxEvents = 64;

HttpResult to_result(const net::Response &r)
{
    HttpResult out;
    out.status = r.error.empty() ? r.status : (r.status > 0 ? r.status : 0);
    out.body = r.body;
    out.error = r.error;
    return out;
}

std::string absolute(const std::string &path)
{
    if (path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0)
        return path;
    return std::string(kBaseUrl) + path;
}

// The account's token is for lichess.org alone.
bool on_lichess(const std::string &url)
{
    return url.rfind(std::string(kBaseUrl) + "/", 0) == 0;
}

} // namespace

std::string speed_name(std::string_view speed)
{
    static constexpr const char *kSpeeds[] = {TR("Bullet"), TR("Blitz"), TR("Rapid"),
                                              TR("Classical"), TR("Correspondence")};
    for (const char *name : kSpeeds)
    {
        // Lichess writes a speed in small letters; refresh_ongoing() gives it
        // a capital. Either is the same speed.
        const std::string_view english = name;
        if (speed.size() != english.size() || speed.substr(1) != english.substr(1) ||
            (speed[0] | 0x20) != (english[0] | 0x20))
            continue;
        // Without a translation the speed stays as it was written.
        const std::string_view translated = tr(name);
        return std::string(translated == english ? speed : translated);
    }
    return std::string(speed);
}

Session::Session(std::string data_root, bool connect) : root_(std::move(data_root))
{
    net::ClientOptions options;
    options.version = "1.0";
    client_ = std::make_unique<net::Client>(net::platform_backend(), options);
    std::string error;
    if (!connect)
    {
        reachable_ = 0;
        retry_probe_ = 1e9;
        return;
    }
    if (!client_->start(&error))
    {
        sys::log("[PCH] network start failed: %s", error.c_str());
        reachable_ = 0;
    }
    load_token();
    // Probe the connection with something worth having: the daily puzzle.
    get("/api/puzzle/daily",
        [this](const HttpResult &r)
        {
            if (r.ok())
                daily_json_ = r.body;
            sys::log("[PCH] lichess probe status=%d %s", r.status, r.error.c_str());
        });
    if (!token_.empty())
        fetch_account();
    sys::log("[PCH] lan ip %s", net::lan_ipv4().c_str());
}

Session::~Session()
{
    pending_.clear();
    if (client_)
        client_->shutdown();
}

bool Session::online() const
{
    return reachable_ == 1;
}

void Session::preview(Account account, std::vector<OngoingGame> ongoing, std::string daily_json)
{
    preview_ = true;
    token_ = "preview";
    account_ = std::move(account);
    ongoing_ = std::move(ongoing);
    if (!daily_json.empty())
        daily_json_ = std::move(daily_json);
    reachable_ = 1;
}

int Session::rating(std::string_view perf) const
{
    for (const Perf &p : account_.perfs)
        if (p.key == perf)
            return p.rating;
    return 0;
}

const Perf *Session::perf(std::string_view key) const
{
    for (const Perf &p : account_.perfs)
        if (p.key == key)
            return &p;
    return nullptr;
}

double Session::rate_limited_for() const
{
    return client_ ? client_->seconds_until_resume() : 0.0;
}

void Session::mark_network(bool reachable)
{
    if (reachable)
    {
        last_success_ = time_;
        if (reachable_ != 1)
        {
            sys::log("[PCH] lichess reachable");
            if (reachable_ == 0)
                flash(tr("Back online"), Notice::Kind::good, 3.0);
        }
        reachable_ = 1;
        return;
    }
    if (time_ - last_success_ > 20.0)
    {
        if (reachable_ != 0)
        {
            sys::log("[PCH] lichess unreachable");
            if (reachable_ == 1)
                flash(tr("Connection lost. Online modes will return when it is back."),
                      Notice::Kind::warning, 6.0);
        }
        reachable_ = 0;
    }
    else if (reachable_ == 1 && time_ - last_failure_flash_ > 8.0)
    {
        // One request failed while we believed we were online.
        last_failure_flash_ = time_;
        flash(tr("Connection problem. Retrying\xE2\x80\xA6"), Notice::Kind::warning, 4.0);
    }
}

Notice Session::notice() const
{
    Notice notice;
    const double paused = rate_limited_for();
    if (paused > 0.5)
    {
        notice.kind = Notice::Kind::warning;
        notice.text = fill(tr("Lichess asked us to slow down. Resuming in {0} s"),
                           {std::to_string(static_cast<int>(paused) + 1)});
        return notice;
    }
    if (!issues_.empty())
    {
        notice.kind = Notice::Kind::warning;
        notice.text = issues_.back().second;
        notice.busy = true;
        return notice;
    }
    if (time_ < flash_until_)
    {
        notice.kind = flash_kind_;
        notice.text = flash_text_;
    }
    return notice;
}

void Session::flash(std::string text, Notice::Kind kind, double seconds)
{
    flash_text_ = std::move(text);
    flash_kind_ = kind;
    flash_until_ = time_ + seconds;
}

void Session::set_issue(const std::string &key, std::string text)
{
    for (auto &issue : issues_)
    {
        if (issue.first == key)
        {
            issue.second = std::move(text);
            return;
        }
    }
    sys::log("[PCH] network issue %s", key.c_str());
    issues_.emplace_back(key, std::move(text));
}

void Session::clear_issue(const std::string &key, bool announce)
{
    for (auto it = issues_.begin(); it != issues_.end(); ++it)
    {
        if (it->first == key)
        {
            issues_.erase(it);
            sys::log("[PCH] network issue %s cleared", key.c_str());
            if (announce)
                flash(tr("Reconnected"), Notice::Kind::good, 2.5);
            return;
        }
    }
}

bool Session::has_issue(const std::string &key) const
{
    for (const auto &issue : issues_)
        if (issue.first == key)
            return true;
    return false;
}

std::uint64_t Session::submit(int method, const std::string &path, const std::string &body,
                              const std::string &content_type, Callback done, bool game_lane)
{
    if (preview_)
        return 0;
    net::Request request;
    request.method = method == kPost     ? net::Method::post
                     : method == kDelete ? net::Method::del
                                         : net::Method::get;
    request.url = absolute(path);
    request.authorize = on_lichess(request.url);
    request.body = body;
    request.content_type = content_type;
    if (method == kPost && content_type.empty())
        request.content_type = "application/x-www-form-urlencoded";
    request.headers.push_back({"Accept", "application/json"});
    const bool lichess = request.authorize;
    const std::uint64_t id =
        client_->submit(game_lane ? net::Lane::priority : net::Lane::general, std::move(request));
    Pending p;
    p.done = std::move(done);
    p.lichess = lichess;
    pending_[id] = std::move(p);
    return id;
}

std::uint64_t Session::get_public(const std::string &url, const std::string &user_agent,
                                  Callback done)
{
    if (preview_)
        return 0;
    net::Request request;
    request.url = url;
    request.authorize = false;
    request.headers.push_back({"Accept", "application/json"});
    if (!user_agent.empty())
        request.headers.push_back({"User-Agent", user_agent});
    const std::uint64_t id = client_->submit(net::Lane::general, std::move(request));
    Pending p;
    p.done = std::move(done);
    p.lichess = false;
    pending_[id] = std::move(p);
    return id;
}

std::uint64_t Session::get(const std::string &path, Callback done)
{
    return submit(kGet, path, "", "", std::move(done), false);
}

std::uint64_t Session::post(const std::string &path, const std::string &form, Callback done,
                            bool game_lane)
{
    return submit(kPost, path, form, "application/x-www-form-urlencoded", std::move(done),
                  game_lane);
}

std::uint64_t Session::post_json(const std::string &path, const std::string &json, Callback done)
{
    return submit(kPost, path, json, "application/json", std::move(done), false);
}

std::uint64_t Session::del(const std::string &path, Callback done)
{
    return submit(kDelete, path, "", "", std::move(done), false);
}

std::uint64_t Session::stream(const std::string &path, LineCallback on_line, Callback on_close,
                              const std::string &form)
{
    if (preview_)
        return 0;
    net::Request request;
    request.url = absolute(path);
    request.authorize = on_lichess(request.url);
    request.headers.push_back({"Accept", "application/x-ndjson"});
    if (!form.empty())
    {
        request.method = net::Method::post;
        request.body = form;
        request.content_type = "application/x-www-form-urlencoded";
    }
    request.timeout_ms = net::kStreamIdleTimeoutMs;
    const bool lichess = request.authorize;
    const std::uint64_t id = client_->open_stream(std::move(request));
    Pending p;
    p.done = std::move(on_close);
    p.line = std::move(on_line);
    p.stream = true;
    p.lichess = lichess;
    pending_[id] = std::move(p);
    return id;
}

void Session::cancel(std::uint64_t id)
{
    const auto it = pending_.find(id);
    if (it == pending_.end())
        return;
    if (it->second.stream)
        client_->close_stream(id);
    pending_.erase(it);
}

void Session::pump(float dt)
{
    time_ += dt;
    net::Result result;
    int handled = 0;
    while (handled < 256 && client_->poll(&result))
    {
        ++handled;
        const auto it = pending_.find(result.id);
        if (result.kind == net::Result::Kind::stream_line)
        {
            if (it == pending_.end() || it->second.lichess)
                mark_network(true);
            if (it != pending_.end() && it->second.line)
            {
                // Copy: the callback may cancel (and erase) itself.
                LineCallback line = it->second.line;
                line(result.line);
            }
            continue;
        }
        const HttpResult r = to_result(result.response);
        // Requests and streams we cancelled ourselves say nothing about the
        // network, and neither does another site (the update check): only an
        // answer from lichess.org does, and only its 401 rejects the token.
        const bool lichess = it != pending_.end() && it->second.lichess;
        if (lichess)
            mark_network(r.status > 0);
        if (lichess && r.status == 401 && !token_.empty() && result.response.error.empty())
        {
            sys::log("[PCH] token rejected (401): signing out");
            sign_out();
        }
        if (it == pending_.end())
            continue;
        Callback done = std::move(it->second.done);
        pending_.erase(it);
        if (done)
            done(r);
    }

    // Retry the connection probe while offline.
    if (reachable_ != 1)
    {
        retry_probe_ -= dt;
        if (retry_probe_ <= 0.0)
        {
            retry_probe_ = 15.0;
            get("/api/puzzle/daily",
                [this](const HttpResult &r)
                {
                    if (r.ok())
                        daily_json_ = r.body;
                });
        }
    }

    // The event stream sends a keep-alive every few seconds; one that has stayed
    // open for a while is healthy again.
    if (event_stream_ != 0 && event_opened_ >= 0.0 && time_ - event_opened_ > 15.0)
    {
        event_failures_ = 0;
        clear_issue("events");
    }
    // The account's event stream, while signed in.
    if (signed_in() && event_stream_ == 0)
    {
        event_retry_ -= dt;
        if (event_retry_ <= 0.0)
            open_event_stream();
    }
    // TV, while a screen wants it.
    if (tv_wanted_ && tv_stream_ == 0)
    {
        tv_retry_ -= dt;
        if (tv_retry_ <= 0.0)
        {
            const std::string path =
                tv_channel_.empty() ? "/api/tv/feed" : "/api/tv/" + tv_channel_ + "/feed";
            const std::string channel = tv_channel_;
            tv_stream_ = stream(
                path, [this](std::string_view line) { on_tv_line(line); },
                [this](const HttpResult &r)
                {
                    tv_stream_ = 0;
                    tv_retry_ = r.status == 429 ? 60.0 : 3.0;
                    // A dropped feed keeps the last board on screen while it reconnects.
                    if (tv_wanted_ && ++tv_failures_ >= 1 && tv_.valid)
                        set_issue("tv", tr("Lichess TV lost the feed. Reconnecting\xE2\x80\xA6"));
                });
            if (tv_.channel != channel)
                tv_.valid = false;
            tv_.channel = channel;
        }
    }
}

// ---- account ----

void Session::load_token()
{
    std::string data;
    if (!save::read_file(root_ + kTokenFile, &data))
        return;
    const save::Decoded decoded = save::decode(save::Kind::prefs, data);
    if (!decoded.ok || decoded.payload.size() < 2)
        return;
    personal_token_ = decoded.payload[0] == 'p';
    token_ = decoded.payload.substr(1);
    client_->set_token(token_);
    sys::log("[PCH] token loaded (%s, %zu chars)", personal_token_ ? "personal" : "oauth",
             token_.size());
}

void Session::save_token() const
{
    const std::string payload =
        token_.empty() ? std::string() : (personal_token_ ? "p" : "o") + token_;
    save::write_atomic(root_ + kTokenFile, save::encode(save::Kind::prefs, 1, payload));
}

void Session::sign_in(const std::string &token, bool personal)
{
    token_ = token;
    personal_token_ = personal;
    account_ = {};
    account_failed_ = false;
    account_error_status_ = -1;
    client_->set_token(token_);
    save_token();
    fetch_account();
}

void Session::sign_out()
{
    if (!token_.empty() && !personal_token_)
        del("/api/token",
            [](const HttpResult &r) { sys::log("[PCH] token revoked status=%d", r.status); });
    if (event_stream_ != 0)
        cancel(event_stream_);
    event_stream_ = 0;
    token_.clear();
    account_ = {};
    ongoing_.clear();
    client_->clear_token();
    save_token();
}

void Session::fetch_account()
{
    get("/api/account",
        [this](const HttpResult &r)
        {
            if (!r.ok())
            {
                account_failed_ = true;
                account_error_status_ = r.status;
                sys::log("[PCH] account fetch failed status=%d", r.status);
                return;
            }
            Document doc(r.body);
            const Value root = doc.root();
            Account account;
            account.id = root["id"].str();
            account.username = root["username"].str(account.id);
            account.title = root["title"].str();
            const Value perfs = root["perfs"];
            for (const char *key : {"bullet", "blitz", "rapid", "classical", "correspondence",
                                    "puzzle", "storm", "streak"})
            {
                const Value p = perfs[key];
                if (!p.exists())
                    continue;
                Perf perf;
                perf.key = key;
                perf.rating = static_cast<int>(p["rating"].integer(p["score"].integer()));
                perf.provisional = p["prov"].boolean();
                perf.games = static_cast<int>(p["games"].integer(p["runs"].integer()));
                perf.progress = static_cast<int>(p["prog"].integer());
                account.perfs.push_back(perf);
            }
            if (account.id.empty())
            {
                account_failed_ = true;
                return;
            }
            const Value count = root["count"];
            account.games = static_cast<int>(count["all"].integer());
            account.wins = static_cast<int>(count["win"].integer());
            account.losses = static_cast<int>(count["loss"].integer());
            account.draws = static_cast<int>(count["draw"].integer());
            account.play_seconds = root["playTime"]["total"].integer();
            account_ = std::move(account);
            fetch_rating_history();
            sys::log("[PCH] signed-in user=%s", account_.username.c_str());
            refresh_ongoing();
        });
}

// The rating lines of the profile and the home screen. Lichess names the
// series by their titles; a failure only leaves the lines out.
void Session::fetch_rating_history()
{
    const std::string id = account_.id;
    get("/api/user/" + id + "/rating-history",
        [this, id](const HttpResult &r)
        {
            if (!r.ok() || account_.id != id)
                return;
            static constexpr struct
            {
                const char *name;
                const char *key;
            } kSeries[] = {{"Bullet", "bullet"},
                           {"Blitz", "blitz"},
                           {"Rapid", "rapid"},
                           {"Classical", "classical"},
                           {"Correspondence", "correspondence"},
                           {"Puzzles", "puzzle"}};
            constexpr std::size_t kKeep = 48;
            Document doc(r.body);
            const Value list = doc.root();
            for (std::size_t i = 0; i < list.size(); ++i)
            {
                const Value series = list.at(i);
                const std::string name = series["name"].str();
                const Value points = series["points"];
                for (const auto &known : kSeries)
                {
                    if (name != known.name)
                        continue;
                    for (Perf &perf : account_.perfs)
                    {
                        if (perf.key != known.key)
                            continue;
                        perf.history.clear();
                        const std::size_t total = points.size();
                        for (std::size_t p = total > kKeep ? total - kKeep : 0; p < total; ++p)
                            perf.history.push_back(
                                static_cast<float>(points.at(p).at(3).integer()));
                    }
                }
            }
        });
}

void Session::refresh_ongoing()
{
    if (token_.empty())
        return;
    get("/api/account/playing?nb=20",
        [this](const HttpResult &r)
        {
            if (!r.ok())
                return;
            Document doc(r.body);
            const Value list = doc.root()["nowPlaying"];
            std::vector<OngoingGame> games;
            for (std::size_t i = 0; i < list.size(); ++i)
            {
                const Value g = list.at(i);
                OngoingGame game;
                game.game_id = g["gameId"].str();
                game.fen = g["fen"].str();
                game.last_move = g["lastMove"].str();
                game.color =
                    g["color"].str() == "black" ? chess::Color::black : chess::Color::white;
                const Value opponent = g["opponent"];
                game.opponent = opponent["username"].str(opponent["id"].str());
                game.opponent_rating = static_cast<int>(opponent["rating"].integer());
                game.ai_level = static_cast<int>(opponent["ai"].integer());
                game.my_turn = g["isMyTurn"].boolean();
                game.seconds_left = g["secondsLeft"].integer(-1);
                game.speed = g["speed"].str();
                if (!game.speed.empty())
                    game.speed[0] = static_cast<char>(game.speed[0] - 32 * (game.speed[0] >= 'a'));
                game.rated = g["rated"].boolean();
                if (!game.game_id.empty())
                    games.push_back(std::move(game));
            }
            ongoing_ = std::move(games);
        });
}

void Session::open_event_stream()
{
    if (preview_)
        return;
    event_stream_ = stream(
        "/api/stream/event", [this](std::string_view line) { on_event_line(line); },
        [this](const HttpResult &r)
        {
            event_stream_ = 0;
            event_retry_ = r.status == 429 ? 60.0 : 5.0;
            sys::log("[PCH] event stream closed status=%d", r.status);
            // Without this stream new games and challenges are not announced.
            if (++event_failures_ >= 2 && r.status != 401)
                set_issue("events", tr("Connection to Lichess lost. Reconnecting\xE2\x80\xA6"));
        });
    event_opened_ = time_;
    sys::log("[PCH] event stream open");
}

void Session::on_event_line(std::string_view line)
{
    event_failures_ = 0;
    clear_issue("events");
    Document doc(line);
    const Value root = doc.root();
    const std::string type = root["type"].str();
    Event event;
    if (type == "gameStart")
    {
        event.kind = Event::Kind::game_start;
        event.id = root["game"]["gameId"].str(root["game"]["id"].str());
        refresh_ongoing();
    }
    else if (type == "gameFinish")
    {
        event.kind = Event::Kind::game_finish;
        event.id = root["game"]["gameId"].str(root["game"]["id"].str());
        refresh_ongoing();
    }
    else if (type == "challenge")
    {
        event.kind = Event::Kind::challenge;
        const Value c = root["challenge"];
        event.id = c["id"].str();
        event.text = fill(tr("{0} challenges you"), {c["challenger"]["name"].str()});
    }
    else if (type == "challengeCanceled" || type == "challengeDeclined")
    {
        event.kind = Event::Kind::challenge_gone;
        event.id = root["challenge"]["id"].str();
    }
    else
    {
        return;
    }
    event.sequence = ++event_sequence_;
    sys::log("[PCH] event %s %s", type.c_str(), event.id.c_str());
    events_.push_back(std::move(event));
    while (events_.size() > kMaxEvents)
        events_.pop_front();
}

// ---- TV ----

void Session::watch_tv(const std::string &channel)
{
    tv_wanted_ = true;
    if (channel == tv_channel_ && (tv_stream_ != 0 || tv_retry_ > 0.0))
        return;
    if (tv_stream_ != 0)
    {
        cancel(tv_stream_);
        tv_stream_ = 0;
    }
    tv_channel_ = channel;
    tv_retry_ = 0.0;
}

void Session::stop_tv()
{
    tv_wanted_ = false;
    for (auto it = issues_.begin(); it != issues_.end(); ++it)
    {
        if (it->first == "tv")
        {
            issues_.erase(it);
            break;
        }
    }
    if (tv_stream_ != 0)
        cancel(tv_stream_);
    tv_stream_ = 0;
}

void Session::on_tv_line(std::string_view line)
{
    tv_failures_ = 0;
    clear_issue("tv");
    Document doc(line);
    const Value root = doc.root();
    const std::string t = root["t"].str();
    const Value d = root["d"];
    if (t == "featured")
    {
        tv_.id = d["id"].str();
        tv_.orientation =
            d["orientation"].str() == "black" ? chess::Color::black : chess::Color::white;
        const Value players = d["players"];
        for (std::size_t i = 0; i < players.size(); ++i)
        {
            const Value p = players.at(i);
            const bool white = p["color"].str() == "white";
            const Value user = p["user"];
            (white ? tv_.white : tv_.black) = user["name"].str(user["id"].str(tr("Anonymous")));
            (white ? tv_.white_title : tv_.black_title) = user["title"].str();
            (white ? tv_.white_rating : tv_.black_rating) = static_cast<int>(p["rating"].integer());
            (white ? tv_.white_clock : tv_.black_clock) = p["seconds"].integer(-1);
        }
        tv_.last_move = {};
        tv_moves_ = 0;
        sys::log("[PCH] tv game %s %s vs %s", tv_.id.c_str(), tv_.white.c_str(), tv_.black.c_str());
    }
    else if (t == "fen")
    {
        if (++tv_moves_ <= 12 || tv_moves_ % 10 == 0)
            sys::log("[PCH] tv moves=%d t=%.1f", tv_moves_, time_);
        const std::string lm = d["lm"].str();
        tv_.last_move = {};
        if (lm.size() >= 4)
        {
            tv_.last_move.from = chess::parse_square(lm.substr(0, 2));
            tv_.last_move.to = chess::parse_square(lm.substr(2, 2));
            if (lm.size() >= 5)
            {
                switch (lm[4])
                {
                case 'n':
                    tv_.last_move.promotion = chess::Role::knight;
                    break;
                case 'b':
                    tv_.last_move.promotion = chess::Role::bishop;
                    break;
                case 'r':
                    tv_.last_move.promotion = chess::Role::rook;
                    break;
                default:
                    tv_.last_move.promotion = chess::Role::queen;
                    break;
                }
            }
        }
        tv_.white_clock = d["wc"].integer(tv_.white_clock);
        tv_.black_clock = d["bc"].integer(tv_.black_clock);
    }
    else
    {
        return;
    }
    // The feed's FEN has only the placement and turn; complete it.
    std::string fen = d["fen"].str();
    if (fen.find(' ') == std::string::npos)
    {
        // The side to move follows the last move's mover.
        chess::Color turn = chess::Color::white;
        if (tv_.last_move.valid())
        {
            chess::Position probe;
            // Identify the mover from the piece now on the destination square.
            if (chess::Position::from_fen(fen + " w - - 0 1", &probe))
                if (const auto piece = probe.piece_at(tv_.last_move.to))
                    turn = chess::opposite(piece->color);
        }
        fen += turn == chess::Color::white ? " w - - 0 1" : " b - - 0 1";
    }
    chess::Position position;
    if (!chess::Position::from_fen(fen, &position))
    {
        // A side not to move in check fails validation; flip the turn once.
        std::string other = fen;
        const std::size_t space = other.find(' ');
        if (space != std::string::npos)
            other[space + 1] = other[space + 1] == 'w' ? 'b' : 'w';
        if (!chess::Position::from_fen(other, &position))
            return;
    }
    tv_.position = position;
    tv_.valid = true;
    tv_.received = time_;
    ++tv_.version;
}

} // namespace pch::lichess
