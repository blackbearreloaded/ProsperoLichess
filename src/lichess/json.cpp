// ProsperoLichess - Small read-only JSON view over yyjson for Lichess responses.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lichess/json.hpp"

#include "third_party/yyjson/yyjson.h"

#include <cstdio>

namespace pch::lichess
{

bool Value::is_object() const
{
    return value_ != nullptr && yyjson_is_obj(value_);
}

bool Value::is_array() const
{
    return value_ != nullptr && yyjson_is_arr(value_);
}

bool Value::is_null() const
{
    return value_ == nullptr || yyjson_is_null(value_);
}

Value Value::operator[](std::string_view key) const
{
    if (!is_object())
        return Value();
    return Value(yyjson_obj_getn(value_, key.data(), key.size()));
}

Value Value::at(std::size_t index) const
{
    if (!is_array())
        return Value();
    return Value(yyjson_arr_get(value_, index));
}

std::size_t Value::size() const
{
    if (is_array())
        return yyjson_arr_size(value_);
    if (is_object())
        return yyjson_obj_size(value_);
    return 0;
}

std::string Value::str(std::string_view fallback) const
{
    if (value_ == nullptr || !yyjson_is_str(value_))
        return std::string(fallback);
    return std::string(yyjson_get_str(value_), yyjson_get_len(value_));
}

long long Value::integer(long long fallback) const
{
    if (value_ == nullptr)
        return fallback;
    if (yyjson_is_int(value_))
        return static_cast<long long>(yyjson_get_sint(value_));
    if (yyjson_is_real(value_))
        return static_cast<long long>(yyjson_get_real(value_));
    return fallback;
}

bool Value::boolean(bool fallback) const
{
    if (value_ == nullptr || !yyjson_is_bool(value_))
        return fallback;
    return yyjson_get_bool(value_);
}

Document::Document(std::string_view json)
{
    doc_ = yyjson_read(json.data(), json.size(), 0);
}

Document::~Document()
{
    if (doc_ != nullptr)
        yyjson_doc_free(doc_);
}

Document::Document(Document &&other) noexcept : doc_(other.doc_)
{
    other.doc_ = nullptr;
}

Document &Document::operator=(Document &&other) noexcept
{
    if (this != &other)
    {
        if (doc_ != nullptr)
            yyjson_doc_free(doc_);
        doc_ = other.doc_;
        other.doc_ = nullptr;
    }
    return *this;
}

Value Document::root() const
{
    return doc_ != nullptr ? Value(yyjson_doc_get_root(doc_)) : Value();
}

std::string json_escape(std::string_view text)
{
    std::string out;
    for (const char c : text)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(c));
                out += buffer;
            }
            else
            {
                out += c;
            }
        }
    }
    return out;
}

} // namespace pch::lichess
