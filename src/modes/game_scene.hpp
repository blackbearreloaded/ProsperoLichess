// ProsperoLichess - The game screen opened on a link of the caller's own.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/scene.hpp"
#include "modes/game_link.hpp"

#include <memory>
#include <string>

namespace pch::modes
{

// The game screen driven by any GameLink. make_online_game() passes the
// Lichess Board API link; host snapshots and tests pass a scripted one, so
// every online state can be shown and exercised without a network.
std::unique_ptr<app::Scene> make_linked_game(std::unique_ptr<GameLink> link);

// The saved Pass & Play game: its moves (space-separated UCI), for a screen
// that shows where it stands. False, and moves untouched, when none is saved.
bool saved_local_game(const app::Context &ctx, std::string *moves);

} // namespace pch::modes
