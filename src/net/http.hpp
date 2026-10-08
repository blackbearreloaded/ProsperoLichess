// ProsperoLichess - Backend-neutral HTTP request/response types and transport interface.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "net/sync.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace pch::net
{

enum class Method : std::uint8_t
{
    get,
    post,
    del
};

struct Header
{
    std::string name;
    std::string value;
};

struct Request
{
    Method method = Method::get;
    std::string url;
    std::vector<Header> headers;
    std::string body;
    std::string content_type;
    // false: the client never adds the account's token (anything but lichess.org).
    bool authorize = true;
    // Whole-request budget for perform(); streams use kStreamIdleTimeoutMs instead.
    int timeout_ms = 10000;
};

struct Response
{
    int status = 0;
    std::string body;
    // Transport error text ("cancelled", "timeout", ...); empty when the exchange
    // completed. A stream that dies after its status line keeps both.
    std::string error;
    std::vector<Header> headers;

    bool ok() const
    {
        return error.empty() && status >= 200 && status < 300;
    }
    // First header with this name (ASCII case-insensitive), or nullptr.
    const std::string *header(std::string_view name) const;
};

// A stream with no bytes at all (not even keep-alive newlines) for this long is dead.
inline constexpr int kStreamIdleTimeoutMs = 25000;
// Largest body perform() accepts; larger responses fail with "response too large".
inline constexpr std::size_t kMaxResponseBytes = 8u * 1024u * 1024u;
// Non-2xx bodies (error JSON) are kept up to this size, also for streams.
inline constexpr std::size_t kMaxErrorBodyBytes = 64u * 1024u;

// Lets another thread abort a blocked transfer. A backend arms the token with a
// hook while a request is in flight; cancel() sets the flag and runs the hook
// under the same lock that disarm() takes, so the hook never sees a request id
// the backend has already released.
class CancelToken
{
  public:
    using AbortHook = void (*)(void *context);

    CancelToken() = default;
    CancelToken(const CancelToken &) = delete;
    CancelToken &operator=(const CancelToken &) = delete;

    void cancel();
    bool cancelled() const
    {
        return cancelled_.load(std::memory_order_acquire);
    }
    // Registers the abort hook; false (nothing registered) if already cancelled.
    bool arm(AbortHook hook, void *context);
    void disarm();
    // Clears the cancelled flag for reuse (only between transfers).
    void reset()
    {
        cancelled_.store(false, std::memory_order_release);
    }

  private:
    SpinLock lock_;
    std::atomic<bool> cancelled_{false};
    AbortHook hook_ = nullptr;
    void *context_ = nullptr;
};

class Backend
{
  public:
    virtual ~Backend() = default;

    // Idempotent; call once from the main thread before any request.
    virtual bool init(std::string *error) = 0;

    // Blocking request. cancel may be null.
    virtual Response perform(const Request &request, CancelToken *cancel) = 0;
    Response perform(const Request &request)
    {
        return perform(request, nullptr);
    }

    // Blocking open; then on_chunk receives raw 2xx body bytes as they arrive
    // until EOF, error, cancel, or on_chunk returning false. Non-2xx bodies are
    // collected into Response::body instead. Returns the final status/error.
    virtual Response stream(const Request &request,
                            const std::function<bool(std::string_view chunk)> &on_chunk,
                            CancelToken &cancel) = 0;
};

// The platform transport: sceHttp on PS5, libcurl on the host.
Backend &platform_backend();

const char *method_name(Method method);
bool iequals(std::string_view a, std::string_view b);
// Header names/values must be free of control characters (no CR/LF injection).
bool is_safe_header(std::string_view name, std::string_view value);
// Parses "Name: value" lines (CRLF or LF); lines without a colon (the status
// line) are skipped and values are trimmed.
std::vector<Header> parse_header_block(std::string_view block);
// "https://host:port" of an absolute URL, or "" when it is not one.
std::string url_origin(std::string_view url);

} // namespace pch::net
