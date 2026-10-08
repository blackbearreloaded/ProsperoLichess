// ProsperoLichess - Strips credentials from text before it is logged.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/redact.hpp"

namespace pch::net
{

namespace
{

constexpr std::string_view kRedacted = "[redacted]";
constexpr std::string_view kKeys[] = {"code_verifier", "code", "access_token", "refresh_token",
                                      "client_secret"};

bool is_alnum(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

bool is_word(char c)
{
    return is_alnum(c) || c == '_';
}

char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool starts_with_ci(std::string_view text, std::size_t at, std::string_view prefix)
{
    if (text.size() - at < prefix.size())
        return false;
    for (std::size_t i = 0; i < prefix.size(); ++i)
    {
        if (lower(text[at + i]) != prefix[i])
            return false;
    }
    return true;
}

bool ends_field(char c)
{
    return c == '&' || c == ' ' || c == '"' || c == '\'' || c == ',' || c == ';' || c == '\r' ||
           c == '\n' || c == '\t' || c == '#' || c == '}' || c == ')';
}

// Handles key=value and "key": "value" at text[at] (at = first key byte).
// Returns the index after the redacted value, or 0 when nothing matched.
std::size_t redact_field(std::string_view text, std::size_t at, std::string &out)
{
    for (std::string_view key : kKeys)
    {
        if (text.compare(at, key.size(), key) != 0)
            continue;
        std::size_t i = at + key.size();
        if (i < text.size() && text[i] == '=')
        {
            ++i;
            out.append(text.substr(at, i - at));
            if (i < text.size() && !ends_field(text[i]))
                out.append(kRedacted);
            while (i < text.size() && !ends_field(text[i]))
                ++i;
            return i;
        }
        if (i < text.size() && text[i] == '"')
        {
            std::size_t j = i + 1;
            while (j < text.size() && (text[j] == ' ' || text[j] == '\t'))
                ++j;
            if (j >= text.size() || text[j] != ':')
                return 0;
            ++j;
            while (j < text.size() && (text[j] == ' ' || text[j] == '\t'))
                ++j;
            if (j >= text.size() || text[j] != '"')
                return 0;
            ++j;
            out.append(text.substr(at, j - at));
            out.append(kRedacted);
            while (j < text.size() && text[j] != '"')
                j += (text[j] == '\\' && j + 1 < text.size()) ? 2 : 1;
            return j;
        }
        return 0;
    }
    return 0;
}

} // namespace

std::string redact(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size())
    {
        const bool boundary = i == 0 || !is_word(text[i - 1]);
        if ((text.compare(i, 4, "lip_") == 0 || text.compare(i, 4, "lio_") == 0) &&
            i + 4 < text.size() && is_alnum(text[i + 4]))
        {
            out.append(kRedacted);
            i += 4;
            while (i < text.size() && is_alnum(text[i]))
                ++i;
            continue;
        }
        if (boundary && starts_with_ci(text, i, "bearer") && i + 6 < text.size() &&
            (text[i + 6] == ' ' || text[i + 6] == '\t'))
        {
            std::size_t j = i + 6;
            while (j < text.size() && (text[j] == ' ' || text[j] == '\t'))
                ++j;
            out.append(text.substr(i, j - i));
            if (j < text.size() && !ends_field(text[j]))
                out.append(kRedacted);
            while (j < text.size() && !ends_field(text[j]))
                ++j;
            i = j;
            continue;
        }
        if (boundary)
        {
            const std::size_t next = redact_field(text, i, out);
            if (next != 0)
            {
                i = next;
                continue;
            }
        }
        out.push_back(text[i]);
        ++i;
    }
    return out;
}

} // namespace pch::net
