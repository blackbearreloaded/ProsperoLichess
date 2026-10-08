// ProsperoLichess - LAN HTTP listener that receives the OAuth redirect (/callback).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "net/sync.hpp"

#include <atomic>
#include <cstdint>
#include <deque>
#include <pthread.h>
#include <string>
#include <utility>

namespace pch::net
{

// Serves one connection at a time on 0.0.0.0:<port> from its own pthread.
// GET /callback[?query] is answered 200 text/html with the set_reply() page
// and queued for poll(); other paths get 404, other methods 405, and requests
// whose head exceeds 4 KiB 431. The query is never logged.
class CallbackListener
{
  public:
    static constexpr std::size_t kMaxRequestBytes = 4096;

    CallbackListener() = default;
    ~CallbackListener();
    CallbackListener(const CallbackListener &) = delete;
    CallbackListener &operator=(const CallbackListener &) = delete;

    // port 0 picks a random port in 49152..65535 (a few attempts).
    bool start(std::uint16_t port, std::string *error);
    std::uint16_t port() const
    {
        return port_;
    }
    bool listening() const
    {
        return socket_ >= 0;
    }
    // Non-blocking; true once per received /callback request.
    bool poll(std::string *path, std::string *query);
    void set_reply(std::string html);
    void stop();

  private:
    static void *thread_main(void *context);
    void run();
    void serve(int client);

    int socket_ = -1;
    std::uint16_t port_ = 0;
    pthread_t thread_{};
    bool thread_started_ = false;
    std::atomic<bool> stop_{false};
    Mutex mutex_;
    std::string reply_ = "<!doctype html><title>ProsperoLichess</title><p>You can close this page.";
    std::deque<std::pair<std::string, std::string>> hits_;
};

// This console's LAN IPv4 address ("192.168.x.y"), or "" when unknown.
std::string lan_ipv4();

} // namespace pch::net
