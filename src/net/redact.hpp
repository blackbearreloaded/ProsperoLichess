// ProsperoLichess - Strips credentials from text before it is logged.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>

namespace pch::net
{

// Replaces with "[redacted]":
//  - the credential after "Bearer " (any case);
//  - values of code, code_verifier, access_token, refresh_token and
//    client_secret, both as form/query fields (key=value) and JSON members
//    ("key": "value");
//  - any lip_/lio_ Lichess token (lip_ followed by [A-Za-z0-9]+).
std::string redact(std::string_view text);

} // namespace pch::net
