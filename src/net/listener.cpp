// ProsperoLichess - LAN HTTP listener that receives the OAuth redirect (/callback).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/listener.hpp"

#include "net/random.hpp"
#include "net/socket.hpp"
#include "platform/ps5/system.hpp"

#include <cstdio>
#include <string_view>

namespace pch::net
{

namespace
{

constexpr int kRequestDeadlineMs = 3000;
constexpr int kSendTimeoutMs = 3000;
constexpr int kAcceptPollUs = 20000;
constexpr int kPortAttempts = 8;

std::string response_text(int status, const char *reason, std::string_view content_type,
                          std::string_view body)
{
    char head[320];
    std::snprintf(head, sizeof(head),
                  "HTTP/1.1 %d %s\r\n"
                  "Content-Type: %.*s\r\n"
                  "Content-Length: %zu\r\n"
                  "Cache-Control: no-store\r\n"
                  "Referrer-Policy: no-referrer\r\n"
                  "Connection: close\r\n\r\n",
                  status, reason, static_cast<int>(content_type.size()), content_type.data(),
                  body.size());
    std::string out(head);
    out.append(body);
    return out;
}

std::string plain(int status, const char *reason)
{
    std::string body(reason);
    body.push_back('\n');
    return response_text(status, reason, "text/plain; charset=utf-8", body);
}

} // namespace

CallbackListener::~CallbackListener()
{
    stop();
}

bool CallbackListener::start(std::uint16_t port, std::string *error)
{
    stop();
    std::string last_error;
    for (int attempt = 0; attempt < (port == 0 ? kPortAttempts : 1); ++attempt)
    {
        std::uint16_t candidate = port;
        if (candidate == 0)
        {
            std::uint16_t random = 0;
            if (!secure_random(&random, sizeof(random)))
                random = static_cast<std::uint16_t>(sys::monotonic_us() * 2654435761u);
            // Alternate between the registered range and the dynamic range: some
            // systems refuse listeners in one of them.
            candidate = attempt % 2 == 0 ? static_cast<std::uint16_t>(20000u + random % 20000u)
                                         : static_cast<std::uint16_t>(49152u + random % 16384u);
        }
        const int handle = sock::listen_tcp(candidate, &last_error);
        if (handle < 0)
            sys::log("net: callback listener attempt %d failed: %s", attempt, last_error.c_str());
        if (handle >= 0)
        {
            socket_ = handle;
            port_ = candidate;
            break;
        }
    }
    if (socket_ < 0)
    {
        if (error != nullptr)
            *error = last_error.empty() ? "cannot open the callback port" : last_error;
        return false;
    }
    stop_.store(false);
    // Never name threads: pthread_setname_np hangs on PS5.
    if (pthread_create(&thread_, nullptr, &CallbackListener::thread_main, this) != 0)
    {
        sock::close_socket(socket_);
        socket_ = -1;
        port_ = 0;
        if (error != nullptr)
            *error = "cannot start the callback listener thread";
        return false;
    }
    thread_started_ = true;
    sys::log("net: callback listener on port %u", static_cast<unsigned>(port_));
    return true;
}

void CallbackListener::stop()
{
    stop_.store(true);
    if (thread_started_)
    {
        pthread_join(thread_, nullptr);
        thread_started_ = false;
    }
    if (socket_ >= 0)
    {
        sock::close_socket(socket_);
        socket_ = -1;
    }
    port_ = 0;
    Guard<Mutex> guard(mutex_);
    hits_.clear();
}

bool CallbackListener::poll(std::string *path, std::string *query)
{
    Guard<Mutex> guard(mutex_);
    if (hits_.empty())
        return false;
    if (path != nullptr)
        *path = std::move(hits_.front().first);
    if (query != nullptr)
        *query = std::move(hits_.front().second);
    hits_.pop_front();
    return true;
}

void CallbackListener::set_reply(std::string html)
{
    Guard<Mutex> guard(mutex_);
    reply_ = std::move(html);
}

void *CallbackListener::thread_main(void *context)
{
    static_cast<CallbackListener *>(context)->run();
    return nullptr;
}

void CallbackListener::run()
{
    while (!stop_.load())
    {
        const int client = sock::accept_client(socket_);
        if (client < 0)
        {
            sys::sleep_us(kAcceptPollUs);
            continue;
        }
        serve(client);
        sock::close_socket(client);
    }
}

void CallbackListener::serve(int client)
{
    std::string request;
    char buffer[1024];
    const std::int64_t deadline = sys::monotonic_us() + kRequestDeadlineMs * 1000;
    bool complete = false;
    while (!stop_.load())
    {
        const std::int64_t left_ms = (deadline - sys::monotonic_us()) / 1000;
        if (left_ms <= 0)
            break;
        const int got = sock::recv_some(client, buffer, sizeof(buffer),
                                        static_cast<int>(left_ms > 100 ? 100 : left_ms));
        if (got == -2)
            continue;
        if (got <= 0)
            break;
        request.append(buffer, static_cast<std::size_t>(got));
        if (request.find("\r\n\r\n") != std::string::npos ||
            request.find("\n\n") != std::string::npos)
        {
            complete = true;
            break;
        }
        if (request.size() > kMaxRequestBytes)
            break;
    }
    std::size_t head_end = request.find("\r\n\r\n");
    head_end = head_end == std::string::npos ? request.find("\n\n") : head_end;
    if (head_end == std::string::npos ? request.size() > kMaxRequestBytes
                                      : head_end > kMaxRequestBytes)
    {
        const std::string reply = plain(431, "Request Header Fields Too Large");
        sock::send_all(client, reply.data(), reply.size(), kSendTimeoutMs);
        sys::log("net: callback listener rejected an oversize request");
        // Lingering close: drain what the peer still sends so closing does not
        // reset the connection before it has read the 431.
        std::size_t drained = 0;
        const std::int64_t drain_until = sys::monotonic_us() + 300000;
        while (drained < 64u * 1024u && sys::monotonic_us() < drain_until)
        {
            const int got = sock::recv_some(client, buffer, sizeof(buffer), 50);
            if (got == 0 || got == -1)
                break;
            if (got > 0)
                drained += static_cast<std::size_t>(got);
        }
        return;
    }
    if (!complete)
        return;

    const std::string_view text(request);
    const std::size_t line_end = text.find_first_of("\r\n");
    const std::string_view line = text.substr(0, line_end);
    const std::size_t first_space = line.find(' ');
    const std::size_t second_space =
        first_space == std::string_view::npos ? first_space : line.find(' ', first_space + 1);
    if (first_space == std::string_view::npos || second_space == std::string_view::npos ||
        line.substr(second_space + 1).substr(0, 5) != "HTTP/")
    {
        const std::string reply = plain(400, "Bad Request");
        sock::send_all(client, reply.data(), reply.size(), kSendTimeoutMs);
        return;
    }
    const std::string_view method = line.substr(0, first_space);
    const std::string_view target = line.substr(first_space + 1, second_space - first_space - 1);
    if (method != "GET")
    {
        const std::string reply = plain(405, "Method Not Allowed");
        sock::send_all(client, reply.data(), reply.size(), kSendTimeoutMs);
        return;
    }
    std::string_view path = target;
    std::string_view query;
    const std::size_t question = target.find('?');
    if (question != std::string_view::npos)
    {
        path = target.substr(0, question);
        query = target.substr(question + 1);
    }
    const std::size_t hash = query.find('#');
    if (hash != std::string_view::npos)
        query = query.substr(0, hash);
    if (path != "/callback")
    {
        sys::log("net: callback listener 404 for %.*s",
                 static_cast<int>(path.size() > 64 ? 64 : path.size()), path.data());
        const std::string reply = plain(404, "Not Found");
        sock::send_all(client, reply.data(), reply.size(), kSendTimeoutMs);
        return;
    }
    std::string page;
    {
        // Queue first: the page may make the user look back at the TV at once.
        Guard<Mutex> guard(mutex_);
        hits_.emplace_back(std::string(path), std::string(query));
        page = reply_;
    }
    sys::log("net: callback listener received /callback");
    const std::string reply = response_text(200, "OK", "text/html; charset=utf-8", page);
    sock::send_all(client, reply.data(), reply.size(), kSendTimeoutMs);
}

} // namespace pch::net
