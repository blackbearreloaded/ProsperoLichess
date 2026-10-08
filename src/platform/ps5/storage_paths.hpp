// ProsperoLichess - Paths after optional Lapy filesystem access.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

namespace pch::storage
{

inline constexpr const char *kTitleId = "PPSA99009";

// Must be set once, immediately after the single-threaded elevation request.
void set_filesystem_access(bool available);
bool filesystem_access();

// The running app may have been mounted from /data, an external drive or an image.
const std::string &app_root();
const std::string &data_root();
std::string app_file(const char *relative);
std::string data_file(const char *relative);

} // namespace pch::storage
