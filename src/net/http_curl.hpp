// ProsperoLichess - The libcurl transport and the platform parts it needs.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "net/http.hpp"

namespace pch::net
{

// libcurl (with OpenSSL) behind the Backend interface: the transport on the
// PC and on the console. The platform supplies the rest (curl_platform_*).
Backend &curl_backend();

// ---- supplied by the platform (src/platform/ps5/curl_platform.cpp on the
// console, host/curl_platform_host.cpp on the PC) ----

// Once, before curl_global_init: seed what TLS needs, report the setup.
bool curl_platform_init(std::string *error);
// For every transfer: certificates, socket options. easy is a CURL *.
void curl_platform_configure(void *easy);
// How long to wait for socket activity when a pass made no progress. The
// console's poll does not always wake early, so it waits briefly and often.
int curl_platform_idle_wait_ms();

} // namespace pch::net
