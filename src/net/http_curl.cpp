// ProsperoLichess - HTTP backend on libcurl, for the console and the PC.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One easy handle on its own multi handle per request, driven on the calling
// thread (a net::Client lane): perform a pass, and wait for the sockets only
// when the pass made no progress. Stream bytes reach on_chunk as they arrive,
// so live games need no one-byte reads (see http_scehttp.cpp for why the
// system library did).

#include "net/http_curl.hpp"

#include "net/redact.hpp"
#include "platform/ps5/system.hpp"

#include <curl/curl.h>

#include <cstdint>

namespace pch::net
{

namespace
{

struct Transfer
{
    const std::function<bool(std::string_view)> *on_chunk = nullptr;
    CURL *easy = nullptr;
    Response *response = nullptr;
    std::uint64_t received = 0; // header and body bytes, to tell a pass made progress
    bool stopped_by_callback = false;
    bool too_large = false;
};

std::size_t on_header(char *data, std::size_t size, std::size_t count, void *context)
{
    auto *transfer = static_cast<Transfer *>(context);
    const std::string_view line(data, size * count);
    transfer->received += line.size();
    if (line.substr(0, 5) == "HTTP/")
        transfer->response->headers.clear(); // a new response (after 100 Continue)
    else
    {
        std::vector<Header> parsed = parse_header_block(line);
        for (Header &header : parsed)
            transfer->response->headers.push_back(std::move(header));
    }
    return size * count;
}

std::size_t on_body(char *data, std::size_t size, std::size_t count, void *context)
{
    auto *transfer = static_cast<Transfer *>(context);
    const std::size_t bytes = size * count;
    transfer->received += bytes;
    long status = 0;
    curl_easy_getinfo(transfer->easy, CURLINFO_RESPONSE_CODE, &status);
    const bool success = status >= 200 && status < 300;
    if (transfer->on_chunk != nullptr && success)
    {
        if (!(*transfer->on_chunk)(std::string_view(data, bytes)))
        {
            transfer->stopped_by_callback = true;
            return CURL_WRITEFUNC_ERROR;
        }
        return bytes;
    }
    const std::size_t limit = success ? kMaxResponseBytes : kMaxErrorBodyBytes;
    std::string &body = transfer->response->body;
    if (body.size() + bytes > limit)
    {
        if (success)
        {
            transfer->too_large = true;
            return CURL_WRITEFUNC_ERROR;
        }
        body.append(data, limit - body.size());
        return bytes; // keep draining an oversize error page without storing it
    }
    body.append(data, bytes);
    return bytes;
}

void wake_multi(void *context)
{
    curl_multi_wakeup(static_cast<CURLM *>(context));
}

class CurlBackend final : public Backend
{
  public:
    using Backend::perform;

    bool init(std::string *error) override
    {
        Guard<SpinLock> guard(lock_);
        if (state_ == 0)
        {
            std::string why;
            if (!curl_platform_init(&why))
            {
                state_ = -1;
                error_ = why;
            }
            else
            {
                const CURLcode code = curl_global_init(CURL_GLOBAL_DEFAULT);
                state_ = code == CURLE_OK ? 1 : -1;
                if (code != CURLE_OK)
                    error_ = curl_easy_strerror(code);
                else
                    sys::log("net: curl %s", curl_version());
            }
        }
        if (state_ < 0 && error != nullptr)
            *error = error_;
        return state_ > 0;
    }

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
    Response run(const Request &request, const std::function<bool(std::string_view)> *on_chunk,
                 CancelToken *cancel)
    {
        Response response;
        {
            Guard<SpinLock> guard(lock_);
            if (state_ <= 0)
            {
                response.error = "network not initialised";
                return response;
            }
        }
        CURL *easy = curl_easy_init();
        CURLM *multi = curl_multi_init();
        if (easy == nullptr || multi == nullptr)
        {
            if (easy != nullptr)
                curl_easy_cleanup(easy);
            if (multi != nullptr)
                curl_multi_cleanup(multi);
            response.error = "curl init failed";
            return response;
        }
        Transfer transfer;
        transfer.on_chunk = on_chunk;
        transfer.easy = easy;
        transfer.response = &response;

        curl_slist *headers = nullptr;
        bool unsafe = false;
        for (const Header &header : request.headers)
        {
            if (!is_safe_header(header.name, header.value))
            {
                unsafe = true;
                continue;
            }
            headers = curl_slist_append(headers, (header.name + ": " + header.value).c_str());
        }
        if (!request.content_type.empty())
        {
            if (!is_safe_header("Content-Type", request.content_type))
                unsafe = true;
            else
                headers =
                    curl_slist_append(headers, ("Content-Type: " + request.content_type).c_str());
        }
        headers = curl_slist_append(headers, "Expect:");

        char error_text[CURL_ERROR_SIZE] = {};
        curl_easy_setopt(easy, CURLOPT_URL, request.url.c_str());
        curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(easy, CURLOPT_USERAGENT, "ProsperoLichess");
        curl_easy_setopt(easy, CURLOPT_HTTP_VERSION, static_cast<long>(CURL_HTTP_VERSION_1_1));
        curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "https,http");
        curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
        curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, &on_header);
        curl_easy_setopt(easy, CURLOPT_HEADERDATA, &transfer);
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, &on_body);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, &transfer);
        curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, error_text);
        if (on_chunk != nullptr)
        {
            // A stream lives as long as data keeps coming (Lichess sends a
            // newline every few seconds to keep it open).
            curl_easy_setopt(easy, CURLOPT_LOW_SPEED_LIMIT, 1L);
            curl_easy_setopt(easy, CURLOPT_LOW_SPEED_TIME,
                             static_cast<long>(kStreamIdleTimeoutMs / 1000));
        }
        else
        {
            curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, static_cast<long>(request.timeout_ms));
        }
        if (request.method == Method::post)
        {
            curl_easy_setopt(easy, CURLOPT_POST, 1L);
            curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
            curl_easy_setopt(easy, CURLOPT_POSTFIELDS, request.body.c_str());
        }
        else if (request.method == Method::del)
        {
            curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, "DELETE");
            if (!request.body.empty())
            {
                curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE,
                                 static_cast<long>(request.body.size()));
                curl_easy_setopt(easy, CURLOPT_POSTFIELDS, request.body.c_str());
            }
        }
        curl_platform_configure(easy);

        CURLcode code = CURLE_OK;
        bool cancelled = false;
        if (unsafe)
        {
            response.error = "unsafe header rejected";
        }
        else if (cancel != nullptr && !cancel->arm(&wake_multi, multi))
        {
            cancelled = true;
        }
        else
        {
            curl_multi_add_handle(multi, easy);
            const int idle_wait = curl_platform_idle_wait_ms();
            int running = 1;
            while (running != 0)
            {
                const std::uint64_t before = transfer.received;
                if (curl_multi_perform(multi, &running) != CURLM_OK || running == 0)
                    break;
                if (cancel != nullptr && cancel->cancelled())
                {
                    cancelled = true;
                    break;
                }
                // Wait only when the pass made no progress: on the console a
                // wait after every pass held downloads to one buffer per wait.
                if (transfer.received == before)
                    curl_multi_poll(multi, nullptr, 0, idle_wait, nullptr);
            }
            if (cancel != nullptr)
            {
                cancel->disarm();
                cancelled = cancelled || cancel->cancelled();
            }
            int queued = 0;
            while (CURLMsg *message = curl_multi_info_read(multi, &queued))
            {
                if (message->msg == CURLMSG_DONE)
                    code = message->data.result;
            }
            curl_multi_remove_handle(multi, easy);
        }

        long status = 0;
        curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &status);
        response.status = static_cast<int>(status);
        if (cancelled)
            response.error = "cancelled";
        else if (transfer.too_large)
            response.error = "response too large";
        else if (code != CURLE_OK && !transfer.stopped_by_callback && response.error.empty())
            response.error =
                code == CURLE_OPERATION_TIMEDOUT ? "timeout" : curl_easy_strerror(code);
        if (!response.error.empty() && !cancelled)
        {
            sys::log("net: %s %s -> %d %s (curl %d%s%s)", method_name(request.method),
                     redact(request.url).c_str(), response.status, response.error.c_str(),
                     static_cast<int>(code), error_text[0] != '\0' ? ": " : "", error_text);
        }

        curl_slist_free_all(headers);
        curl_multi_cleanup(multi);
        curl_easy_cleanup(easy);
        return response;
    }

    SpinLock lock_;
    int state_ = 0; // 0 not yet, 1 ready, -1 failed
    std::string error_;
};

} // namespace

Backend &curl_backend()
{
    // Never destroyed: no static destructor runs when the title ends.
    static CurlBackend *backend = new CurlBackend();
    return *backend;
}

} // namespace pch::net
