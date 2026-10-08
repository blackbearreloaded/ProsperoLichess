// ProsperoLichess - URL percent-encoding, form bodies and query-string parsing.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pch::net
{

using Fields = std::vector<std::pair<std::string, std::string>>;

// RFC 3986: everything except unreserved [A-Za-z0-9-._~] becomes %XX (uppercase).
std::string percent_encode(std::string_view text);

// %XX sequences decoded; '+' becomes a space when plus_is_space. Malformed
// escapes are kept verbatim.
std::string percent_decode(std::string_view text, bool plus_is_space = true);

// application/x-www-form-urlencoded body (also valid as a query string):
// "k1=v1&k2=v2" with both sides percent-encoded.
std::string form_encode(const Fields &fields);

// url + "?" (or "&" when it already has a query) + form_encode(fields).
std::string with_query(std::string_view url, const Fields &fields);

// Parses "a=1&b=two%20words" (a leading '?' is ignored). Keys without '='
// get an empty value; empty segments are skipped.
Fields parse_query(std::string_view query);

// First value for key in a query string.
std::optional<std::string> query_param(std::string_view query, std::string_view key);

} // namespace pch::net
