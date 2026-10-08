// ProsperoLichess - Puzzles from lichess.org: the daily puzzle and rated training batches.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lichess/puzzle_sources.hpp"

#include "core/strings.hpp"
#include "lichess/json.hpp"
#include "platform/ps5/system.hpp"

#include <algorithm>
#include <deque>
#include <vector>

namespace pch::lichess
{

namespace
{

bool parse_puzzle_value(const Value &root, puzzles::Puzzle *out, std::string *error)
{
    const Value puzzle = root["puzzle"];
    const Value game = root["game"];
    std::vector<std::string> solution;
    const Value moves = puzzle["solution"];
    for (std::size_t i = 0; i < moves.size(); ++i)
        solution.push_back(moves.at(i).str());
    if (!puzzles::from_api(puzzle["id"].str(), game["pgn"].str(), solution,
                           static_cast<int>(puzzle["rating"].integer()), out, error))
        return false;
    const Value themes = puzzle["themes"];
    for (std::size_t i = 0; i < themes.size(); ++i)
        out->themes.push_back(themes.at(i).str());
    return true;
}

class DailySource : public puzzles::Source
{
  public:
    explicit DailySource(Session &session) : session_(session)
    {
    }
    ~DailySource() override
    {
        if (request_ != 0)
            session_.cancel(request_);
    }
    State next(app::Context &, int, puzzles::Puzzle *out, std::string *error) override
    {
        if (served_)
        {
            *error = tr("Come back tomorrow for a new daily puzzle");
            return State::exhausted;
        }
        if (!failed_.empty())
        {
            *error = failed_;
            failed_.clear();
            return State::error;
        }
        std::string json = session_.daily_json();
        if (json.empty() && !body_.empty())
            json = body_;
        if (json.empty())
        {
            if (request_ == 0)
                request_ = session_.get("/api/puzzle/daily",
                                        [this](const HttpResult &r)
                                        {
                                            request_ = 0;
                                            if (r.ok())
                                                body_ = r.body;
                                            else
                                                failed_ = r.status == 0
                                                              ? tr("No connection to Lichess")
                                                              : tr("Lichess did not answer");
                                        });
            return State::loading;
        }
        if (!parse_api_puzzle(json, out, error))
            return State::error;
        out->source = "daily";
        served_ = true;
        return State::ready;
    }

  private:
    Session &session_;
    std::uint64_t request_ = 0;
    std::string body_;
    std::string failed_;
    bool served_ = false;
};

class TrainingSource : public puzzles::Source
{
  public:
    TrainingSource(Session &session, std::string angle)
        : session_(session), angle_(std::move(angle))
    {
    }
    ~TrainingSource() override
    {
        for (std::uint64_t id : {fetch_, submit_})
            if (id != 0)
                session_.cancel(id);
    }
    State next(app::Context &, int, puzzles::Puzzle *out, std::string *error) override
    {
        if (!queue_.empty())
        {
            *out = std::move(queue_.front());
            queue_.pop_front();
            out->source = "training";
            if (queue_.size() < 3)
                fetch();
            return State::ready;
        }
        if (!failed_.empty())
        {
            *error = failed_;
            failed_.clear();
            return State::error;
        }
        fetch();
        return State::loading;
    }
    void report(app::Context &, const puzzles::Puzzle &puzzle, bool win, bool rated) override
    {
        if (!session_.signed_in())
            return;
        unsent_.push_back("{\"id\":\"" + json_escape(puzzle.id) +
                          "\",\"win\":" + (win ? "true" : "false") +
                          ",\"rated\":" + (rated ? "true" : "false") + "}");
        send_results();
    }
    int player_rating() const override
    {
        return rating_ > 0 ? rating_ : session_.rating("puzzle");
    }
    int rating_change() const override
    {
        return change_;
    }

  private:
    // Sends every result not yet accepted; failed ones stay queued for the next try.
    void send_results()
    {
        if (submit_ != 0 || unsent_.empty())
            return;
        std::string body = "{\"solutions\":[";
        for (std::size_t i = 0; i < unsent_.size(); ++i)
            body += (i > 0 ? "," : "") + unsent_[i];
        body += "]}";
        const std::size_t sent = unsent_.size();
        submit_ = session_.post_json(
            "/api/puzzle/batch/" + angle_ + "?nb=0", body,
            [this, sent](const HttpResult &r)
            {
                submit_ = 0;
                if (!r.ok())
                {
                    sys::log("[PCH] puzzle results not saved status=%d queued=%zu", r.status,
                             unsent_.size());
                    session_.flash(tr("Puzzle result not saved yet. It will be sent again."),
                                   Notice::Kind::warning, 5.0);
                    return;
                }
                unsent_.erase(unsent_.begin(),
                              unsent_.begin() +
                                  static_cast<std::ptrdiff_t>(std::min(sent, unsent_.size())));
                Document doc(r.body);
                const Value root = doc.root();
                const int rating = static_cast<int>(root["glicko"]["rating"].integer());
                if (rating > 0)
                    rating_ = rating;
                const Value rounds = root["rounds"];
                if (rounds.size() > 0)
                    change_ =
                        static_cast<int>(rounds.at(rounds.size() - 1)["ratingDiff"].integer());
                send_results();
            });
    }

    void fetch()
    {
        if (fetch_ != 0)
            return;
        fetch_ = session_.get("/api/puzzle/batch/" + angle_ + "?nb=15",
                              [this](const HttpResult &r)
                              {
                                  fetch_ = 0;
                                  if (!r.ok())
                                  {
                                      failed_ = r.status == 0 ? tr("No connection to Lichess")
                                                              : tr("Lichess did not send puzzles");
                                      return;
                                  }
                                  Document doc(r.body);
                                  const Value list = doc.root()["puzzles"];
                                  for (std::size_t i = 0; i < list.size(); ++i)
                                  {
                                      puzzles::Puzzle puzzle;
                                      std::string error;
                                      if (parse_puzzle_value(list.at(i), &puzzle, &error))
                                          queue_.push_back(std::move(puzzle));
                                  }
                                  const int rating =
                                      static_cast<int>(doc.root()["glicko"]["rating"].integer());
                                  if (rating > 0)
                                      rating_ = rating;
                                  if (queue_.empty())
                                      failed_ = tr("No puzzles right now");
                              });
    }

    Session &session_;
    std::string angle_;
    std::deque<puzzles::Puzzle> queue_;
    std::vector<std::string> unsent_; // result objects not yet accepted by Lichess
    std::uint64_t fetch_ = 0;
    std::uint64_t submit_ = 0;
    std::string failed_;
    int rating_ = 0;
    int change_ = 0;
};

} // namespace

bool parse_api_puzzle(std::string_view json, puzzles::Puzzle *out, std::string *error)
{
    Document doc(json);
    if (!doc.ok())
    {
        *error = tr("unreadable puzzle");
        return false;
    }
    return parse_puzzle_value(doc.root(), out, error);
}

std::unique_ptr<puzzles::Source> make_daily_source(Session &session)
{
    return std::make_unique<DailySource>(session);
}

std::unique_ptr<puzzles::Source> make_training_source(Session &session, std::string angle)
{
    return std::make_unique<TrainingSource>(session, std::move(angle));
}

} // namespace pch::lichess
