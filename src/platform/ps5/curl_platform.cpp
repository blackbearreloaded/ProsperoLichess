// ProsperoLichess - The console side of the curl backend: certificates, sockets, entropy.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What libcurl needs on the console that it does not find by itself (see the
// ProsperoRadio curl guide, 2026-10-02):
//  - the system's list of certificate authorities, CA_LIST.cer (plain PEM),
//    found under /system with filesystem access and under the sandbox's own
//    root without it;
//  - non-blocking sockets: the C library refuses O_NONBLOCK on sockets, so the
//    system option SO_NBIO is set on every socket curl makes;
//  - in the sandbox, sockets made with sceNetSocket (net_sockets.c), unless
//    the C library's are chosen with set_curl_sockets (a hardware check);
//  - entropy for OpenSSL from the kernel's generator, in case its own sources
//    are closed to the sandbox.

#include "platform/ps5/curl_platform.hpp"

#include "net/http_curl.hpp"
#include "net/random.hpp"
#include "platform/ps5/system.hpp"

#include <curl/curl.h>
#include <openssl/rand.h>

#include <atomic>
#include <cstdio>
#include <sys/socket.h>
#include <sys/stat.h>

extern "C"
{
    const char *sceKernelGetFsSandboxRandomWord(void);
    int pch_net_open(int domain, int type, int protocol);
    int pch_net_close(int socket);
}

namespace pch::net
{

namespace
{

constexpr int kSoNbio = 0x1200; // the system's non-blocking switch

// Per thread: a check that tries the C library's sockets leaves the app's
// own requests alone.
thread_local CurlSockets g_sockets = CurlSockets::system;
char g_ca_path[128] = {};

curl_socket_t open_socket(void *, curlsocktype, curl_sockaddr *address)
{
    const int socket = pch_net_open(address->family, address->socktype, address->protocol);
    return socket < 0 ? CURL_SOCKET_BAD : socket;
}

int close_socket(void *, curl_socket_t socket)
{
    return pch_net_close(socket);
}

int on_libc_socket(void *, curl_socket_t socket, curlsocktype)
{
    int on = 1;
    const int result = setsockopt(socket, SOL_SOCKET, kSoNbio, &on, sizeof(on));
    static std::atomic<bool> logged{false};
    if (!logged.exchange(true))
        sys::log("net: C library sockets non-blocking: %s", result == 0 ? "set" : "NOT set");
    return CURL_SOCKOPT_OK;
}

const char *certificate_list()
{
    if (g_ca_path[0] == '\0')
    {
        // With filesystem access the process sees the console's own tree,
        // where the list is under /system; in the sandbox it is under the
        // sandbox's random word.
        struct stat info;
        std::snprintf(g_ca_path, sizeof(g_ca_path), "/system/common/cert/CA_LIST.cer");
        if (stat(g_ca_path, &info) != 0)
        {
            const char *word = sceKernelGetFsSandboxRandomWord();
            std::snprintf(g_ca_path, sizeof(g_ca_path), "/%s/common/cert/CA_LIST.cer",
                          word != nullptr ? word : "");
        }
    }
    return g_ca_path;
}

} // namespace

void set_curl_sockets(CurlSockets sockets)
{
    g_sockets = sockets;
}

CurlSockets curl_sockets()
{
    return g_sockets;
}

bool curl_platform_init(std::string *)
{
    // OpenSSL seeds itself from the system; add the kernel's generator in case
    // those sources are closed to the sandbox.
    unsigned char seed[64];
    if (secure_random(seed, sizeof(seed)))
        RAND_add(seed, sizeof(seed), static_cast<double>(sizeof(seed)));
    struct stat info;
    const bool list = stat(certificate_list(), &info) == 0;
    sys::log("net: curl tls rand=%d ca=%s (%s)", RAND_status(), certificate_list(),
             list ? "found" : "MISSING");
    return true;
}

void curl_platform_configure(void *handle)
{
    CURL *easy = static_cast<CURL *>(handle);
    curl_easy_setopt(easy, CURLOPT_CAINFO, certificate_list());
    if (curl_sockets() == CurlSockets::system)
    {
        curl_easy_setopt(easy, CURLOPT_OPENSOCKETFUNCTION, &open_socket);
        curl_easy_setopt(easy, CURLOPT_CLOSESOCKETFUNCTION, &close_socket);
    }
    else
    {
        curl_easy_setopt(easy, CURLOPT_SOCKOPTFUNCTION, &on_libc_socket);
    }
}

int curl_platform_idle_wait_ms()
{
    return 10;
}

Backend &platform_backend()
{
    return curl_backend();
}

} // namespace pch::net

extern "C" void pch_curl_platform_init()
{
    (void)pch::net::curl_platform_init(nullptr);
}

extern "C" void pch_curl_platform_configure(void *handle)
{
    pch::net::curl_platform_configure(handle);
}
