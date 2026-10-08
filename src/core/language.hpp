// ProsperoLichess - Which language the app speaks, and its catalog.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace pch::language
{

// What was chosen at start-up, for the log.
struct Choice
{
    std::string tag = "en-US";    // the language asked for
    std::string catalog = "none"; // the catalog in use, or why there is none
    std::size_t texts = 0;        // how many texts it translates
};

// The PS5 tag of a system language id ("pt-BR" for 17); "en-US" for an id the
// app does not know. The ids are those of third_party/ps5_system_language.
std::string tag_for(int system_language);

// The language the app is to speak: the console's (system_tag), unless the
// supplied data-root language.txt holds another tag ("en-US": English).
std::string wanted(const std::string &data_root, std::string_view system_tag);

// The app speaks that language when it has a catalog for it
// (assets/lang/<tag>.po) and English otherwise.
//
// tag is what wanted() gave. can_draw says whether the fonts have every
// letter of a text: a catalog they cannot draw in full is not used, because
// English is better than missing letters. Call this before the first screen
// is built; every tr() after it reads the catalog it loaded.
Choice choose(const std::string &assets, std::string_view tag,
              const std::function<bool(std::string_view)> &can_draw);

// The same from one catalog file, whatever its name (the PC tools' way to try
// a catalog that is not installed).
Choice choose_file(const std::string &path, std::string_view tag,
                   const std::function<bool(std::string_view)> &can_draw);

} // namespace pch::language
