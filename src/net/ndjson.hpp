// ProsperoLichess - Newline-delimited JSON line splitter for streamed responses.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace pch::net
{

// Reassembles lines from arbitrarily fragmented bytes. LF ends a line; a CR
// before it is dropped. Blank (whitespace-only) lines are keep-alives and are
// counted, not delivered. A line longer than max_line bytes is discarded
// whole (up to its newline) and counted as oversize.
class NdjsonSplitter
{
  public:
    static constexpr std::size_t kDefaultMaxLine = 256u * 1024u;
    using LineFn = std::function<void(std::string_view line)>;

    explicit NdjsonSplitter(std::size_t max_line = kDefaultMaxLine) : max_line_(max_line)
    {
    }

    void feed(std::string_view bytes, const LineFn &on_line);
    // Delivers a final unterminated line (clean end of stream).
    void flush(const LineFn &on_line);
    void reset();

    bool has_partial() const
    {
        return !buffer_.empty();
    }
    std::uint64_t lines() const
    {
        return lines_;
    }
    std::uint64_t blank_lines() const
    {
        return blank_;
    }
    std::uint64_t oversize_lines() const
    {
        return oversize_;
    }

  private:
    void emit(std::string_view line, const LineFn &on_line);

    std::size_t max_line_;
    std::string buffer_;
    bool discarding_ = false;
    std::uint64_t lines_ = 0;
    std::uint64_t blank_ = 0;
    std::uint64_t oversize_ = 0;
};

} // namespace pch::net
