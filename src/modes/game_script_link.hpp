// ProsperoLichess - A scripted GameLink: online game states without a network.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Host snapshots and tests cannot reach a Lichess game, so they describe one
// here and hand it to make_linked_game(). The script is shared: change it,
// call touch(), and the screen follows as it would follow the server.
//
//   auto script = std::make_shared<modes::GameScript>();
//   script->moves = "e2e4 e7e5";
//   run.open(modes::make_linked_game(modes::make_script_link(script)));
//   script->draw_offer[1] = true; // Black offers a draw
//   script->touch();

#pragma once

#include "modes/game_link.hpp"

#include <memory>
#include <string>
#include <vector>

namespace pch::modes
{

struct GameScript
{
    // ---- what the server says ----
    bool ready = true;
    std::string error;
    bool connected = true;
    std::uint64_t version = 1;
    std::string initial_fen;
    std::string moves;
    chess::Color me = chess::Color::white;
    PlayerInfo white;
    PlayerInfo black;
    bool has_clock = true;
    long long clock_ms[2] = {600000, 600000}; // White, Black
    GameStatus status = GameStatus::playing;
    int winner = -1;
    bool rated = true;
    std::string speed = "Rapid 10+5  \xC2\xB7  Rated";
    bool draw_offer[2] = {false, false}; // by White, by Black
    bool takeback_offer[2] = {false, false};
    int opponent_gone = -1;
    bool refused = false; // the next take_refusal() reports this message
    std::string refusal;
    bool echo_moves = true; // a sent move comes back as played

    // ---- what the screen asked for, in order ----
    // "move e2e4", "resign", "abort", "draw yes", "draw no", "takeback yes",
    // "takeback no", "claim".
    std::vector<std::string> sent;

    void touch()
    {
        ++version;
    }
    bool asked(const std::string &what) const
    {
        for (const std::string &entry : sent)
        {
            if (entry == what)
                return true;
        }
        return false;
    }
};

class ScriptLink final : public GameLink
{
  public:
    explicit ScriptLink(std::shared_ptr<GameScript> script) : script_(std::move(script))
    {
    }
    void pump(app::Context &, float) override
    {
    }
    bool ready() const override
    {
        return script_->ready;
    }
    std::string error() const override
    {
        return script_->error;
    }
    bool connected() const override
    {
        return script_->connected;
    }
    std::uint64_t version() const override
    {
        return script_->version;
    }
    const std::string &initial_fen() const override
    {
        return script_->initial_fen;
    }
    const std::string &moves() const override
    {
        return script_->moves;
    }
    chess::Color my_color() const override
    {
        return script_->me;
    }
    PlayerInfo player(chess::Color color) const override
    {
        return color == chess::Color::white ? script_->white : script_->black;
    }
    bool has_clock() const override
    {
        return script_->has_clock;
    }
    long long clock_ms(chess::Color color) const override
    {
        return script_->clock_ms[color == chess::Color::white ? 0 : 1];
    }
    GameStatus status() const override
    {
        return script_->status;
    }
    int winner() const override
    {
        return script_->winner;
    }
    bool rated() const override
    {
        return script_->rated;
    }
    std::string speed_label() const override
    {
        return script_->speed;
    }
    bool draw_offered_by(chess::Color color) const override
    {
        return script_->draw_offer[color == chess::Color::white ? 0 : 1];
    }
    bool takeback_offered_by(chess::Color color) const override
    {
        return script_->takeback_offer[color == chess::Color::white ? 0 : 1];
    }
    int opponent_gone_seconds() const override
    {
        return script_->opponent_gone;
    }

    void send_move(const chess::Move &move) override
    {
        const std::string uci = chess::to_uci(move);
        script_->sent.push_back("move " + uci);
        if (!script_->echo_moves)
            return;
        script_->moves += (script_->moves.empty() ? "" : " ") + uci;
        script_->touch();
    }
    void resign() override
    {
        script_->sent.push_back("resign");
    }
    void abort() override
    {
        script_->sent.push_back("abort");
    }
    void draw(bool yes) override
    {
        script_->sent.push_back(yes ? "draw yes" : "draw no");
    }
    void takeback(bool yes) override
    {
        script_->sent.push_back(yes ? "takeback yes" : "takeback no");
    }
    void claim_victory() override
    {
        script_->sent.push_back("claim");
    }
    bool take_refusal(std::string *message) override
    {
        if (!script_->refused)
            return false;
        script_->refused = false;
        *message = script_->refusal;
        return true;
    }
    std::string game_id() const override
    {
        return "scripted";
    }

  private:
    std::shared_ptr<GameScript> script_;
};

inline std::unique_ptr<GameLink> make_script_link(std::shared_ptr<GameScript> script)
{
    return std::make_unique<ScriptLink>(std::move(script));
}

} // namespace pch::modes
