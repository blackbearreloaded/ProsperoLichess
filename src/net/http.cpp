// ProsperoLichess - Shared HTTP helpers and the cross-thread cancel token.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/http.hpp"

namespace pch::net
{

namespace
{

char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string_view trim(std::string_view text)
{
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
        text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
        text.remove_suffix(1);
    return text;
}

} // namespace

const std::string *Response::header(std::string_view name) const
{
    for (const Header &entry : headers)
    {
        if (iequals(entry.name, name))
            return &entry.value;
    }
    return nullptr;
}

void CancelToken::cancel()
{
    Guard<SpinLock> guard(lock_);
    cancelled_.store(true, std::memory_order_release);
    if (hook_ != nullptr)
        hook_(context_);
}

bool CancelToken::arm(AbortHook hook, void *context)
{
    Guard<SpinLock> guard(lock_);
    if (cancelled_.load(std::memory_order_acquire))
        return false;
    hook_ = hook;
    context_ = context;
    return true;
}

void CancelToken::disarm()
{
    Guard<SpinLock> guard(lock_);
    hook_ = nullptr;
    context_ = nullptr;
}

const char *method_name(Method method)
{
    switch (method)
    {
    case Method::get:
        return "GET";
    case Method::post:
        return "POST";
    case Method::del:
        return "DELETE";
    }
    return "GET";
}

bool iequals(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (lower(a[i]) != lower(b[i]))
            return false;
    }
    return true;
}

bool is_safe_header(std::string_view name, std::string_view value)
{
    if (name.empty() || name.size() > 256 || value.size() > 8192)
        return false;
    for (char c : name)
    {
        const auto byte = static_cast<unsigned char>(c);
        if (byte <= 0x20 || byte >= 0x7f || c == ':')
            return false;
    }
    for (char c : value)
    {
        const auto byte = static_cast<unsigned char>(c);
        if ((byte < 0x20 && c != '\t') || byte == 0x7f)
            return false;
    }
    return true;
}

std::vector<Header> parse_header_block(std::string_view block)
{
    std::vector<Header> headers;
    while (!block.empty())
    {
        const std::size_t end = block.find('\n');
        std::string_view line = block.substr(0, end);
        block = end == std::string_view::npos ? std::string_view{} : block.substr(end + 1);
        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos || colon == 0)
            continue;
        const std::string_view name = trim(line.substr(0, colon));
        if (name.empty() || name.find(' ') != std::string_view::npos)
            continue;
        headers.push_back({std::string(name), std::string(trim(line.substr(colon + 1)))});
    }
    return headers;
}

std::string url_origin(std::string_view url)
{
    const std::size_t scheme = url.find("://");
    if (scheme == std::string_view::npos || scheme == 0)
        return {};
    const std::size_t host_start = scheme + 3;
    std::size_t end = url.find_first_of("/?#", host_start);
    if (end == std::string_view::npos)
        end = url.size();
    if (end == host_start)
        return {};
    std::string origin(url.substr(0, end));
    for (std::size_t i = 0; i < origin.size(); ++i)
        origin[i] = lower(origin[i]);
    return origin;
}

} // namespace pch::net
