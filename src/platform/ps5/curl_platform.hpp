// ProsperoLichess - Console transport choices, for hardware checks.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "net/http.hpp"

namespace pch::net
{

// Which sockets libcurl uses on the console.
enum class CurlSockets
{
    system, // sceNetSocket (works in a sandboxed title)
    libc,   // the C library's (did not connect from ProsperoRadio's sandbox)
};

// For the calling thread only.
void set_curl_sockets(CurlSockets sockets);
CurlSockets curl_sockets();

// The system's sceHttp transport, kept for comparison and as a fallback.
Backend &scehttp_backend();

// Unattended runs: logs which transports reach lichess.org (own thread).
void start_transport_check();

} // namespace pch::net

// C bridge used by the vendored self-update engine.
extern "C" void pch_curl_platform_init();
extern "C" void pch_curl_platform_configure(void *handle);
