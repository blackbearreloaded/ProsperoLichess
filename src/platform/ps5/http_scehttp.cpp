// ProsperoLichess - PS5 HTTP backend on the system sceHttp/sceSsl services (the fallback).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Follows the hardware-proven psiptv pattern (pool/SSL/HTTP init, template
// options, per-request timeouts, sceHttpReadData loop, sceHttpAbortRequest from
// another thread) and prospero-radio's keep-alive connection reuse.
//
// Hardware findings (PS5 at 4K, 2026-10-02):
//  - HTTPS GET to lichess.org works with the system CA store (daily puzzle,
//    puzzle batches, TV feeds).
//  - sceHttpReadData blocks until the buffer it is given is full, so streams
//    read one byte at a time; moves then arrive as they are sent.
//  - Aborting a blocked stream read from another thread works.
//  - POST bodies arrive intact (Lichess echoes /api/token/test).
//  - Certificate verification is on: expired, wrong-host and self-signed test
//    sites fail the handshake (0x8095f00d / 0x8095f00b / 0x8095f00c).
// Still unproven on hardware (need a signed-in account): the Authorization
// header and DELETE.

#include "net/http.hpp"
#include "net/redact.hpp"
#include "platform/ps5/curl_platform.hpp"
#include "platform/ps5/system.hpp"

#include <cstdint>
#include <cstdio>
#include <new>
#include <vector>

extern "C"
{
    int sceNetPoolCreate(const char *name, int size, int flags);
    int sceNetPoolDestroy(int pool);
    int sceSslInit(std::size_t pool_size);
    int sceSslTerm(int context);
    int sceHttpInit(int net_pool, int ssl_context, std::size_t pool_size);
    int sceHttpTerm(int context);
    int sceHttpCreateTemplate(int context, const char *user_agent, int version, int auto_proxy);
    int sceHttpDeleteTemplate(int template_id);
    int sceHttpCreateConnectionWithURL(int template_id, const char *url, int keep_alive);
    int sceHttpDeleteConnection(int connection);
    int sceHttpCreateRequestWithURL(int connection, int method, const char *url,
                                    std::uint64_t content_length);
    int sceHttpDeleteRequest(int request);
    int sceHttpAbortRequest(int request);
    int sceHttpAddRequestHeader(int id, const char *name, const char *value, std::uint32_t mode);
    int sceHttpSetAutoRedirect(int id, int enabled);
    int sceHttpSetResolveTimeOut(int id, std::uint32_t usec);
    int sceHttpSetConnectTimeOut(int id, std::uint32_t usec);
    int sceHttpSetSendTimeOut(int id, std::uint32_t usec);
    int sceHttpSetRecvTimeOut(int id, std::uint32_t usec);
    int sceHttpSetRecvBlockSize(int id, std::uint32_t bytes);
    int sceHttpSetResponseHeaderMaxSize(int id, std::size_t bytes);
    int sceHttpsEnableOption(int id, std::uint32_t flags);
    int sceHttpSendRequest(int request, const void *data, std::size_t size);
    int sceHttpGetStatusCode(int request, int *status);
    int sceHttpGetAllResponseHeaders(int request, char **headers, std::size_t *size);
    int sceHttpReadData(int request, void *data, std::size_t size);
}

namespace pch::net
{

namespace
{

constexpr int kNetPoolSize = 1024 * 1024;
constexpr std::size_t kSslPoolSize = 304u * 1024u;
constexpr std::size_t kHttpPoolSize = 4u * 1024u * 1024u;
constexpr int kHttpVersion11 = 2;
constexpr std::uint32_t kHeaderOverwrite = 0;
constexpr std::uint32_t kPhaseTimeoutUsec = 10000000; // resolve / connect / send
constexpr std::uint32_t kStreamRecvTimeoutUsec = kStreamIdleTimeoutMs * 1000u;
constexpr std::uint32_t kRestBlockSize = 64u * 1024u;
constexpr std::uint32_t kStreamBlockSize = 1024u;
constexpr std::size_t kMaxHeaderBytes = 64u * 1024u;
constexpr std::size_t kMaxIdleConnections = 4;

// sceHttpsEnableOption flags (SharpProspero Http.cs).
constexpr std::uint32_t kSslServerVerify = 0x01;
constexpr std::uint32_t kSslCnCheck = 0x04;
constexpr std::uint32_t kSslNotAfterCheck = 0x08;
constexpr std::uint32_t kSslNotBeforeCheck = 0x10;
constexpr std::uint32_t kSslKnownCaCheck = 0x20;
constexpr std::uint32_t kSslSni = 0x80;
constexpr std::uint32_t kSslVerifyFlags = kSslServerVerify | kSslCnCheck | kSslNotAfterCheck |
                                          kSslNotBeforeCheck | kSslKnownCaCheck | kSslSni;

int method_code(Method method)
{
    switch (method)
    {
    case Method::get:
        return 0;
    case Method::post:
        return 1;
    case Method::del:
        return 5;
    }
    return 0;
}

std::string sce_error(const char *what, int code)
{
    char text[64];
    std::snprintf(text, sizeof(text), "%s failed (0x%08x)", what, static_cast<unsigned>(code));
    return text;
}

void abort_request(void *context)
{
    sceHttpAbortRequest(static_cast<int>(reinterpret_cast<std::intptr_t>(context)));
}

struct IdleConnection
{
    std::string origin;
    int connection = -1;
};

class SceHttpBackend final : public Backend
{
  public:
    using Backend::perform;

    bool init(std::string *error) override;
    Response perform(const Request &request, CancelToken *cancel) override
    {
        return run(request, nullptr, cancel);
    }
    Response stream(const Request &request,
                    const std::function<bool(std::string_view chunk)> &on_chunk,
                    CancelToken &cancel) override
    {
        return run(request, &on_chunk, &cancel);
    }

  private:
    int configure_template(int template_id, std::uint32_t block_size, std::uint32_t recv_usec);
    Response run(const Request &request, const std::function<bool(std::string_view)> *on_chunk,
                 CancelToken *cancel);
    int take_idle(const std::string &origin);
    void give_back(const std::string &origin, int connection);

    SpinLock init_lock_;
    bool ready_ = false;
    int pool_ = -1;
    int ssl_ = -1;
    int http_ = -1;
    int rest_template_ = -1;
    int stream_template_ = -1;
    SpinLock idle_lock_;
    std::vector<IdleConnection> idle_;
};

int SceHttpBackend::configure_template(int template_id, std::uint32_t block_size,
                                       std::uint32_t recv_usec)
{
    int result = sceHttpSetAutoRedirect(template_id, 0);
    if (result >= 0)
        result = sceHttpSetRecvBlockSize(template_id, block_size);
    if (result >= 0)
        result = sceHttpSetResolveTimeOut(template_id, kPhaseTimeoutUsec);
    if (result >= 0)
        result = sceHttpSetConnectTimeOut(template_id, kPhaseTimeoutUsec);
    if (result >= 0)
        result = sceHttpSetSendTimeOut(template_id, kPhaseTimeoutUsec);
    if (result >= 0)
        result = sceHttpSetRecvTimeOut(template_id, recv_usec);
    if (result >= 0)
        sceHttpSetResponseHeaderMaxSize(template_id, kMaxHeaderBytes);
    if (result >= 0)
    {
        const int verify = sceHttpsEnableOption(template_id, kSslVerifyFlags);
        sys::log("net: template %d block=%u recv=%ums https verify flags 0x%02x -> 0x%08x",
                 template_id, block_size, recv_usec / 1000, kSslVerifyFlags,
                 static_cast<unsigned>(verify));
    }
    return result;
}

bool SceHttpBackend::init(std::string *error)
{
    Guard<SpinLock> guard(init_lock_);
    if (ready_)
        return true;
    const char *stage = "sceNetPoolCreate";
    int result = pool_ = sceNetPoolCreate("pch_http", kNetPoolSize, 0);
    if (result >= 0)
    {
        stage = "sceSslInit";
        result = ssl_ = sceSslInit(kSslPoolSize);
    }
    if (result >= 0)
    {
        stage = "sceHttpInit";
        result = http_ = sceHttpInit(pool_, ssl_, kHttpPoolSize);
    }
    if (result >= 0)
    {
        stage = "sceHttpCreateTemplate";
        result = rest_template_ =
            sceHttpCreateTemplate(http_, "ProsperoLichess", kHttpVersion11, 0);
    }
    if (result >= 0)
        result = stream_template_ =
            sceHttpCreateTemplate(http_, "ProsperoLichess", kHttpVersion11, 0);
    if (result >= 0)
    {
        stage = "template options";
        result = configure_template(rest_template_, kRestBlockSize, 10000000u);
    }
    if (result >= 0)
        result = configure_template(stream_template_, kStreamBlockSize, kStreamRecvTimeoutUsec);
    if (result < 0)
    {
        sys::log("net: %s failed 0x%08x", stage, static_cast<unsigned>(result));
        if (error != nullptr)
            *error = sce_error(stage, result);
        if (stream_template_ >= 0)
            sceHttpDeleteTemplate(stream_template_);
        if (rest_template_ >= 0)
            sceHttpDeleteTemplate(rest_template_);
        if (http_ >= 0)
            sceHttpTerm(http_);
        if (ssl_ >= 0)
            sceSslTerm(ssl_);
        if (pool_ >= 0)
            sceNetPoolDestroy(pool_);
        pool_ = ssl_ = http_ = rest_template_ = stream_template_ = -1;
        return false;
    }
    sys::log("net: sceHttp ready (pool %d, ssl %d, http %d)", pool_, ssl_, http_);
    ready_ = true;
    return true;
}

int SceHttpBackend::take_idle(const std::string &origin)
{
    Guard<SpinLock> guard(idle_lock_);
    for (std::size_t i = 0; i < idle_.size(); ++i)
    {
        if (idle_[i].origin == origin)
        {
            const int connection = idle_[i].connection;
            idle_.erase(idle_.begin() + static_cast<std::ptrdiff_t>(i));
            return connection;
        }
    }
    return -1;
}

void SceHttpBackend::give_back(const std::string &origin, int connection)
{
    int evicted = -1;
    {
        Guard<SpinLock> guard(idle_lock_);
        if (idle_.size() >= kMaxIdleConnections)
        {
            evicted = idle_.front().connection;
            idle_.erase(idle_.begin());
        }
        idle_.push_back({origin, connection});
    }
    if (evicted >= 0)
        sceHttpDeleteConnection(evicted);
}

Response SceHttpBackend::run(const Request &request,
                             const std::function<bool(std::string_view)> *on_chunk,
                             CancelToken *cancel)
{
    Response response;
    {
        Guard<SpinLock> guard(init_lock_);
        if (!ready_)
        {
            response.error = "network not initialised";
            return response;
        }
    }
    const bool streaming = on_chunk != nullptr;
    const std::string origin = url_origin(request.url);
    if (origin.empty())
    {
        response.error = "invalid url";
        return response;
    }
    for (const Header &header : request.headers)
    {
        if (!is_safe_header(header.name, header.value))
        {
            response.error = "unsafe header rejected";
            return response;
        }
    }
    if (!request.content_type.empty() && !is_safe_header("Content-Type", request.content_type))
    {
        response.error = "unsafe header rejected";
        return response;
    }

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        response = Response{};
        int connection = streaming ? -1 : take_idle(origin);
        const bool reused = connection >= 0;
        if (connection < 0)
        {
            connection =
                sceHttpCreateConnectionWithURL(streaming ? stream_template_ : rest_template_,
                                               request.url.c_str(), streaming ? 0 : 1);
        }
        if (connection < 0)
        {
            response.error = sce_error("connect", connection);
            return response;
        }
        const int req = sceHttpCreateRequestWithURL(connection, method_code(request.method),
                                                    request.url.c_str(), request.body.size());
        if (req < 0)
        {
            sceHttpDeleteConnection(connection);
            response.error = sce_error("create request", req);
            return response;
        }
        const std::uint32_t recv_usec =
            streaming
                ? kStreamRecvTimeoutUsec
                : static_cast<std::uint32_t>(request.timeout_ms > 0 ? request.timeout_ms : 10000) *
                      1000u;
        int result = sceHttpSetAutoRedirect(req, 0);
        if (result >= 0)
            result = sceHttpSetRecvTimeOut(req, recv_usec);
        for (const Header &header : request.headers)
        {
            if (result >= 0)
                result = sceHttpAddRequestHeader(req, header.name.c_str(), header.value.c_str(),
                                                 kHeaderOverwrite);
        }
        if (result >= 0 && !request.content_type.empty())
            result = sceHttpAddRequestHeader(req, "Content-Type", request.content_type.c_str(),
                                             kHeaderOverwrite);
        bool cancelled = false;
        if (result >= 0 && cancel != nullptr &&
            !cancel->arm(&abort_request, reinterpret_cast<void *>(static_cast<std::intptr_t>(req))))
        {
            cancelled = true;
        }
        if (result >= 0 && !cancelled)
            result = sceHttpSendRequest(req, request.body.empty() ? nullptr : request.body.data(),
                                        request.body.size());
        if (result >= 0 && !cancelled)
            result = sceHttpGetStatusCode(req, &response.status);
        if (result < 0 || cancelled)
        {
            if (cancel != nullptr)
            {
                cancel->disarm();
                cancelled = cancelled || cancel->cancelled();
            }
            sceHttpDeleteRequest(req);
            sceHttpDeleteConnection(connection);
            response.status = 0;
            if (cancelled)
            {
                response.error = "cancelled";
                return response;
            }
            // A kept-alive connection the server already closed fails here;
            // retry idempotent GETs once on a fresh connection.
            if (reused && request.method == Method::get && attempt == 0)
                continue;
            response.error = sce_error("send", result);
            return response;
        }

        char *header_block = nullptr;
        std::size_t header_size = 0;
        if (sceHttpGetAllResponseHeaders(req, &header_block, &header_size) >= 0 &&
            header_block != nullptr && header_size <= kMaxHeaderBytes)
        {
            response.headers = parse_header_block(std::string_view(header_block, header_size));
        }

        const bool success = response.status >= 200 && response.status < 300;
        const bool deliver = streaming && success;
        const std::size_t limit = success ? kMaxResponseBytes : kMaxErrorBodyBytes;
        // Hardware finding: sceHttpReadData blocks until the buffer is full, so a
        // 4 KiB read delivered a live NDJSON stream in bursts of dozens of moves.
        // Streams therefore read one byte at a time (the data rate is tiny).
        std::vector<char> buffer(deliver ? 1 : 16384);
        bool clean_eof = false;
        for (;;)
        {
            const int got = sceHttpReadData(req, buffer.data(), buffer.size());
            if (got < 0)
            {
                response.error = sce_error("read", got);
                break;
            }
            if (got == 0)
            {
                clean_eof = true;
                break;
            }
            const std::string_view chunk(buffer.data(), static_cast<std::size_t>(got));
            if (deliver)
            {
                if (!(*on_chunk)(chunk))
                    break;
                continue;
            }
            if (response.body.size() + chunk.size() > limit)
            {
                if (success)
                {
                    response.error = "response too large";
                    break;
                }
                response.body.append(chunk.substr(0, limit - response.body.size()));
                continue;
            }
            response.body.append(chunk);
        }
        if (cancel != nullptr)
        {
            cancel->disarm();
            if (cancel->cancelled())
                response.error = "cancelled";
        }
        sceHttpDeleteRequest(req);
        if (!streaming && clean_eof && response.error.empty())
            give_back(origin, connection);
        else
            sceHttpDeleteConnection(connection);
        if (!response.error.empty())
        {
            sys::log("net: %s %s -> %d %s", method_name(request.method),
                     redact(request.url).c_str(), response.status, response.error.c_str());
        }
        return response;
    }
    return response;
}

alignas(SceHttpBackend) unsigned char g_backend_storage[sizeof(SceHttpBackend)];
SceHttpBackend *g_backend = nullptr;
SpinLock g_backend_lock;

} // namespace

// Constructed on first use in static storage and never destroyed: no static
// constructor ordering, no __cxa_guard, no atexit registration.
Backend &scehttp_backend()
{
    Guard<SpinLock> guard(g_backend_lock);
    if (g_backend == nullptr)
        g_backend = new (g_backend_storage) SceHttpBackend();
    return *g_backend;
}

} // namespace pch::net
