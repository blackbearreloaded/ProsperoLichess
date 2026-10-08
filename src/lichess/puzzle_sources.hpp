// ProsperoLichess - Puzzles from lichess.org: the daily puzzle and rated training batches.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lichess/session.hpp"
#include "puzzles/source.hpp"

#include <memory>
#include <string_view>

namespace pch::lichess
{

// Parses one API puzzle object ({"game":{..},"puzzle":{..}}).
bool parse_api_puzzle(std::string_view json, puzzles::Puzzle *out, std::string *error);

std::unique_ptr<puzzles::Source> make_daily_source(Session &session);
// angle: "mix" or a theme; results are submitted when signed in.
std::unique_ptr<puzzles::Source> make_training_source(Session &session, std::string angle);

} // namespace pch::lichess
