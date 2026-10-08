// ProsperoLichess - URL percent-encoding, form bodies and query-string parsing.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/url.hpp"

namespace pch::net
{

namespace
{

bool is_unreserved(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
           c == '.' || c == '_' || c == '~';
}

int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

} // namespace

std::string percent_encode(std::string_view text)
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size() * 3);
    for (char c : text)
    {
        if (is_unreserved(c))
        {
            out.push_back(c);
            continue;
        }
        const auto byte = static_cast<unsigned char>(c);
        out.push_back('%');
        out.push_back(kHex[byte >> 4]);
        out.push_back(kHex[byte & 0x0f]);
    }
    return out;
}

std::string percent_decode(std::string_view text, bool plus_is_space)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        if (c == '+' && plus_is_space)
        {
            out.push_back(' ');
            continue;
        }
        if (c == '%' && i + 2 < text.size() && hex_value(text[i + 1]) >= 0 &&
            hex_value(text[i + 2]) >= 0)
        {
            out.push_back(static_cast<char>(hex_value(text[i + 1]) * 16 + hex_value(text[i + 2])));
            i += 2;
            continue;
        }
        out.push_back(c);
    }
    return out;
}

std::string form_encode(const Fields &fields)
{
    std::string out;
    for (const auto &[key, value] : fields)
    {
        if (!out.empty())
            out.push_back('&');
        out += percent_encode(key);
        out.push_back('=');
        out += percent_encode(value);
    }
    return out;
}

std::string with_query(std::string_view url, const Fields &fields)
{
    std::string out(url);
    if (fields.empty())
        return out;
    out.push_back(url.find('?') == std::string_view::npos ? '?' : '&');
    out += form_encode(fields);
    return out;
}

Fields parse_query(std::string_view query)
{
    Fields fields;
    if (!query.empty() && query.front() == '?')
        query.remove_prefix(1);
    const std::size_t hash = query.find('#');
    if (hash != std::string_view::npos)
        query = query.substr(0, hash);
    while (!query.empty())
    {
        const std::size_t amp = query.find('&');
        const std::string_view part = query.substr(0, amp);
        query = amp == std::string_view::npos ? std::string_view{} : query.substr(amp + 1);
        if (part.empty())
            continue;
        const std::size_t eq = part.find('=');
        if (eq == std::string_view::npos)
            fields.emplace_back(percent_decode(part), std::string{});
        else
            fields.emplace_back(percent_decode(part.substr(0, eq)),
                                percent_decode(part.substr(eq + 1)));
    }
    return fields;
}

std::optional<std::string> query_param(std::string_view query, std::string_view key)
{
    for (auto &[name, value] : parse_query(query))
    {
        if (name == key)
            return std::move(value);
    }
    return std::nullopt;
}

} // namespace pch::net
