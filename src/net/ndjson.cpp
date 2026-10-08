// ProsperoLichess - Newline-delimited JSON line splitter for streamed responses.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/ndjson.hpp"

namespace pch::net
{

namespace
{

bool is_blank(std::string_view line)
{
    for (char c : line)
    {
        if (c != ' ' && c != '\t' && c != '\r')
            return false;
    }
    return true;
}

} // namespace

void NdjsonSplitter::emit(std::string_view line, const LineFn &on_line)
{
    if (!line.empty() && line.back() == '\r')
        line.remove_suffix(1);
    if (line.size() > max_line_)
    {
        ++oversize_;
        return;
    }
    if (is_blank(line))
    {
        ++blank_;
        return;
    }
    ++lines_;
    on_line(line);
}

void NdjsonSplitter::feed(std::string_view bytes, const LineFn &on_line)
{
    while (!bytes.empty())
    {
        const std::size_t newline = bytes.find('\n');
        if (newline == std::string_view::npos)
        {
            if (discarding_)
                return;
            // Allow one extra byte for a CR that may precede the coming LF.
            if (buffer_.size() + bytes.size() > max_line_ + 1)
            {
                buffer_.clear();
                discarding_ = true;
                ++oversize_;
                return;
            }
            buffer_.append(bytes);
            return;
        }
        const std::string_view segment = bytes.substr(0, newline);
        bytes.remove_prefix(newline + 1);
        if (discarding_)
        {
            discarding_ = false;
            continue;
        }
        if (buffer_.empty())
        {
            emit(segment, on_line);
            continue;
        }
        if (buffer_.size() + segment.size() > max_line_ + 1)
        {
            buffer_.clear();
            ++oversize_;
            continue;
        }
        buffer_.append(segment);
        std::string line;
        line.swap(buffer_);
        emit(line, on_line);
    }
}

void NdjsonSplitter::flush(const LineFn &on_line)
{
    if (!discarding_ && !buffer_.empty())
    {
        std::string line;
        line.swap(buffer_);
        emit(line, on_line);
    }
    buffer_.clear();
    discarding_ = false;
}

void NdjsonSplitter::reset()
{
    buffer_.clear();
    discarding_ = false;
    lines_ = 0;
    blank_ = 0;
    oversize_ = 0;
}

} // namespace pch::net
