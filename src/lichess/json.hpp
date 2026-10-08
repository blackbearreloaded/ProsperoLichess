// ProsperoLichess - Small read-only JSON view over yyjson for Lichess responses.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

struct yyjson_doc;
struct yyjson_val;

namespace pch::lichess
{

// A value inside a parsed document; missing keys give an empty Value, so
// lookups can be chained without checks: doc.root()["game"]["id"].str().
class Value
{
  public:
    Value() = default;
    explicit Value(yyjson_val *value) : value_(value)
    {
    }
    bool exists() const
    {
        return value_ != nullptr;
    }
    bool is_object() const;
    bool is_array() const;
    bool is_null() const;
    Value operator[](std::string_view key) const;
    Value at(std::size_t index) const;
    std::size_t size() const; // array length or object key count
    std::string str(std::string_view fallback = {}) const;
    long long integer(long long fallback = 0) const; // accepts integral and real numbers
    bool boolean(bool fallback = false) const;

  private:
    yyjson_val *value_ = nullptr;
};

class Document
{
  public:
    Document() = default;
    explicit Document(std::string_view json);
    ~Document();
    Document(Document &&other) noexcept;
    Document &operator=(Document &&other) noexcept;
    Document(const Document &) = delete;
    Document &operator=(const Document &) = delete;

    bool ok() const
    {
        return doc_ != nullptr;
    }
    Value root() const;

  private:
    yyjson_doc *doc_ = nullptr;
};

// Escapes a string for a JSON string literal (without the quotes).
std::string json_escape(std::string_view text);

} // namespace pch::lichess
