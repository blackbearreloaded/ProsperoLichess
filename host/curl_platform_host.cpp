// ProsperoLichess - The PC side of the curl backend: system certificates, plain sockets.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/http_curl.hpp"

#include <curl/curl.h>

#include <cstdlib>

namespace pch::net
{

bool curl_platform_init(std::string *)
{
    return true;
}

void curl_platform_configure(void *easy)
{
    // PCH_HTTP_CA tests another certificate list (a copy of the console's).
    if (const char *list = std::getenv("PCH_HTTP_CA"))
        curl_easy_setopt(static_cast<CURL *>(easy), CURLOPT_CAINFO, list);
}

int curl_platform_idle_wait_ms()
{
    return 250; // the PC's poll wakes on activity
}

Backend &platform_backend()
{
    return curl_backend();
}

} // namespace pch::net
