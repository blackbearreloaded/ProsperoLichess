// ProsperoLichess - Unattended runs: which transports reach lichess.org from this console.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Logs one line per transport ("[PCH] transport ...") for the same request,
// a live stream through curl, and a certificate refusal. It runs on its own
// thread; the socket choice is per thread, so the app's own requests keep
// the default meanwhile.

#include "platform/ps5/curl_platform.hpp"

#include "net/http_curl.hpp"
#include "platform/ps5/system.hpp"

#include <cstdint>
#include <pthread.h>
#include <string>

namespace pch::net
{

namespace
{

constexpr const char *kDaily = "https://lichess.org/api/puzzle/daily";

void check(const char *name, Backend &backend, const char *url)
{
    std::string error;
    if (!backend.init(&error))
    {
        sys::log("[PCH] transport %s init failed: %s", name, error.c_str());
        return;
    }
    Request request;
    request.url = url;
    request.headers.push_back({"Accept", "application/json"});
    const std::int64_t start = sys::monotonic_us();
    const Response response = backend.perform(request);
    sys::log("[PCH] transport %s %s status=%d bytes=%zu ms=%lld %s", name, url, response.status,
             response.body.size(), static_cast<long long>((sys::monotonic_us() - start) / 1000),
             response.error.empty() ? "ok" : response.error.c_str());
}

void check_stream()
{
    Request request;
    request.url = "https://lichess.org/api/tv/feed";
    request.headers.push_back({"Accept", "application/x-ndjson"});
    CancelToken cancel;
    const std::int64_t start = sys::monotonic_us();
    std::int64_t first = -1;
    int chunks = 0;
    std::size_t bytes = 0;
    const Response response = curl_backend().stream(
        request,
        [&](std::string_view chunk)
        {
            if (first < 0)
                first = sys::monotonic_us() - start;
            ++chunks;
            bytes += chunk.size();
            // Ten seconds of the featured game is enough to see moves arrive.
            return sys::monotonic_us() - start < 10000000;
        },
        cancel);
    sys::log("[PCH] transport curl-stream status=%d first_ms=%lld chunks=%d bytes=%zu %s",
             response.status, static_cast<long long>(first / 1000), chunks, bytes,
             response.error.empty() ? "ok" : response.error.c_str());
}

void *run(void *)
{
    set_curl_sockets(CurlSockets::system);
    check("curl-system", curl_backend(), kDaily);
    check_stream();
    check("curl-system-expired", curl_backend(), "https://expired.badssl.com/");
    check("curl-system-wronghost", curl_backend(), "https://wrong.host.badssl.com/");
    set_curl_sockets(CurlSockets::libc);
    check("curl-libc", curl_backend(), kDaily);
    set_curl_sockets(CurlSockets::system);
    check("scehttp", scehttp_backend(), kDaily);
    sys::log("[PCH] transport check done");
    return nullptr;
}

} // namespace

void start_transport_check()
{
    pthread_t thread;
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, 512 * 1024);
    if (pthread_create(&thread, &attributes, &run, nullptr) == 0)
        pthread_detach(thread);
    else
        sys::log("[PCH] transport check: no thread");
    pthread_attr_destroy(&attributes);
}

} // namespace pch::net
