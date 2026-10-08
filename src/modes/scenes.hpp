// ProsperoLichess - Every screen of the app and how it is opened.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/scene.hpp"

#include <memory>
#include <string>

namespace pch::modes
{

// ---- the frame of the app ----
// The home screen: the side rail and its pages.
std::unique_ptr<app::Scene> make_shell(app::Context &ctx);
// The opening title.
std::unique_ptr<app::Scene> make_boot();

// ---- puzzles ----
std::unique_ptr<app::Scene> make_daily_puzzle(app::Context &ctx);
std::unique_ptr<app::Scene> make_puzzle_training(app::Context &ctx);
std::unique_ptr<app::Scene> make_puzzle_streak(app::Context &ctx);
std::unique_ptr<app::Scene> make_puzzle_storm(app::Context &ctx);
// Offline puzzles of one theme of the pack, around a rating.
std::unique_ptr<app::Scene> make_puzzle_theme(app::Context &ctx, int theme, int rating);

// ---- games ----
// Pass & Play on this controller; resumes the saved game when there is one.
std::unique_ptr<app::Scene> make_local_game(app::Context &ctx);
// True when a Pass & Play game is saved and can be resumed.
bool has_saved_local_game(const app::Context &ctx);
// Waits for Lichess to pair a seek (form is the Board API seek form), then
// opens the game. label says what is being looked for ("Rapid 10+5 - Rated").
std::unique_ptr<app::Scene> make_seek(std::string form, std::string label);
// Asks Lichess for a game against Stockfish, then opens it.
std::unique_ptr<app::Scene> make_ai_game(std::string form, std::string label);
// A Lichess game in progress.
std::unique_ptr<app::Scene> make_online_game(app::Context &ctx, const std::string &game_id);

// ---- watching ----
// Lichess TV, full screen, starting on a channel ("" is the featured game).
std::unique_ptr<app::Scene> make_tv(const std::string &channel);

// ---- account ----
// Signing in with a phone (QR code) or a personal token.
std::unique_ptr<app::Scene> make_account();

} // namespace pch::modes
