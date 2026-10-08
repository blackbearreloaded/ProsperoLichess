// ProsperoLichess - PS5 sceNet TCP primitives for the callback listener, and the LAN address.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/listener.hpp"
#include "net/socket.hpp"
#include "platform/ps5/system.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

// Native apps must use sceNet directly: libc fcntl() rejects network handles,
// so non-blocking mode is the SO_NBIO socket option (as in ProsperoLight).
namespace
{

struct SceSockaddrIn
{
    std::uint8_t length;
    std::uint8_t family;
    std::uint16_t port;    // network order
    std::uint32_t address; // network order
    std::uint8_t zero[8];
};
static_assert(sizeof(SceSockaddrIn) == 16);

constexpr int kAfInet = 2;
constexpr int kSockStream = 1;
constexpr int kSockDgram = 2;
constexpr int kSolSocket = 0xffff;
constexpr int kSoReuseAddr = 0x0004;
constexpr int kSoNbio = 0x1200;
constexpr int kEWouldBlock = 35; // FreeBSD EAGAIN/EWOULDBLOCK
constexpr int kEInterrupted = 4;
constexpr int kPollSleepUs = 5000;
// sceNetCtlGetInfo code for the IPv4 address string (SCE_NET_CTL_INFO_IP_ADDRESS).
constexpr int kNetCtlInfoIpAddress = 14;

// SceNetCtlInfo is a union whose largest members are 256-byte strings; the IP
// address member is char[16] at offset 0. Oversized to be safe.
union NetCtlInfo
{
    char ip_address[16];
    std::uint8_t raw[512];
};

} // namespace

extern "C"
{
    int sceNetSocket(const char *name, int family, int type, int protocol);
    int sceNetSocketClose(int socket);
    int sceNetBind(int socket, const SceSockaddrIn *address, std::uint32_t length);
    int sceNetListen(int socket, int backlog);
    int sceNetAccept(int socket, SceSockaddrIn *address, std::uint32_t *length);
    int sceNetConnect(int socket, const SceSockaddrIn *address, std::uint32_t length);
    int sceNetGetsockname(int socket, SceSockaddrIn *address, std::uint32_t *length);
    int sceNetRecv(int socket, void *buffer, std::size_t length, int flags);
    int sceNetSend(int socket, const void *buffer, std::size_t length, int flags);
    int sceNetSetsockopt(int socket, int level, int option, const void *value,
                         std::uint32_t length);
    int *sceNetErrnoLoc(void);
    int sceNetCtlInit(void);
    int sceNetCtlGetInfo(int code, NetCtlInfo *info);
}

namespace
{

std::uint16_t to_network16(std::uint16_t value)
{
    return static_cast<std::uint16_t>((value >> 8) | (value << 8));
}

// sceNet calls return a negative SCE error (0x804101xx | errno) and also set
// the sceNet errno; accept either form of "would block".
bool would_block(int result)
{
    const int *error = sceNetErrnoLoc();
    const int code = error != nullptr ? *error : 0;
    const int embedded = result < 0 ? (result & 0xff) : 0;
    return code == kEWouldBlock || code == kEInterrupted || embedded == kEWouldBlock ||
           embedded == kEInterrupted;
}

bool set_nonblocking(int socket)
{
    const int enabled = 1;
    return sceNetSetsockopt(socket, kSolSocket, kSoNbio, &enabled, sizeof(enabled)) >= 0;
}

void format_ipv4(std::uint32_t network_address, char *text, std::size_t size)
{
    const auto *octets = reinterpret_cast<const std::uint8_t *>(&network_address);
    std::snprintf(text, size, "%u.%u.%u.%u", octets[0], octets[1], octets[2], octets[3]);
}

} // namespace

namespace pch::net
{

namespace sock
{

int listen_tcp(std::uint16_t port, std::string *error)
{
    const int socket = sceNetSocket("pch_oauth", kAfInet, kSockStream, 0);
    if (socket < 0)
    {
        if (error != nullptr)
        {
            char text[48];
            std::snprintf(text, sizeof(text), "sceNetSocket 0x%08x", static_cast<unsigned>(socket));
            *error = text;
        }
        return -1;
    }
    const int one = 1;
    sceNetSetsockopt(socket, kSolSocket, kSoReuseAddr, &one, sizeof(one));
    SceSockaddrIn address{};
    address.length = sizeof(address);
    address.family = kAfInet;
    address.port = to_network16(port);
    address.address = 0; // INADDR_ANY
    const char *stage = "bind";
    int result = sceNetBind(socket, &address, sizeof(address));
    if (result >= 0)
    {
        stage = "listen";
        result = sceNetListen(socket, 4);
    }
    if (result >= 0 && !set_nonblocking(socket))
    {
        stage = "nonblocking";
        result = -1;
    }
    if (result < 0)
    {
        if (error != nullptr)
        {
            char text[64];
            std::snprintf(text, sizeof(text), "%s port %u 0x%08x", stage,
                          static_cast<unsigned>(port), static_cast<unsigned>(result));
            *error = text;
        }
        sceNetSocketClose(socket);
        return -1;
    }
    return socket;
}

int accept_client(int listener)
{
    SceSockaddrIn peer{};
    std::uint32_t length = sizeof(peer);
    const int client = sceNetAccept(listener, &peer, &length);
    if (client < 0)
        return -1;
    if (!set_nonblocking(client))
    {
        sceNetSocketClose(client);
        return -1;
    }
    return client;
}

int recv_some(int socket, void *buffer, std::size_t size, int timeout_ms)
{
    const std::int64_t deadline = pch::sys::monotonic_us() + std::int64_t{timeout_ms} * 1000;
    for (;;)
    {
        const int got = sceNetRecv(socket, buffer, size, 0);
        if (got >= 0)
            return got;
        if (!would_block(got))
            return -1;
        if (pch::sys::monotonic_us() >= deadline)
            return -2;
        pch::sys::sleep_us(kPollSleepUs);
    }
}

bool send_all(int socket, const void *data, std::size_t size, int timeout_ms)
{
    const auto *bytes = static_cast<const char *>(data);
    const std::int64_t deadline = pch::sys::monotonic_us() + std::int64_t{timeout_ms} * 1000;
    while (size > 0)
    {
        const int sent = sceNetSend(socket, bytes, size, 0);
        if (sent > 0)
        {
            bytes += sent;
            size -= static_cast<std::size_t>(sent);
            continue;
        }
        if (sent < 0 && !would_block(sent))
            return false;
        if (pch::sys::monotonic_us() >= deadline)
            return false;
        pch::sys::sleep_us(kPollSleepUs);
    }
    return true;
}

void close_socket(int socket)
{
    if (socket >= 0)
        sceNetSocketClose(socket);
}

} // namespace sock

std::string lan_ipv4()
{
    sceNetCtlInit(); // already-initialised errors are harmless
    NetCtlInfo info{};
    if (sceNetCtlGetInfo(kNetCtlInfoIpAddress, &info) >= 0)
    {
        info.ip_address[sizeof(info.ip_address) - 1] = '\0';
        if (info.ip_address[0] != '\0' && std::strcmp(info.ip_address, "0.0.0.0") != 0)
            return info.ip_address;
    }
    // Fallback: a connected UDP socket's local address (no packet is sent).
    const int socket = sceNetSocket("pch_route", kAfInet, kSockDgram, 0);
    if (socket < 0)
        return {};
    SceSockaddrIn target{};
    target.length = sizeof(target);
    target.family = kAfInet;
    target.port = to_network16(53);
    target.address = 0x08080808u;
    std::string result;
    SceSockaddrIn local{};
    std::uint32_t length = sizeof(local);
    if (sceNetConnect(socket, &target, sizeof(target)) >= 0 &&
        sceNetGetsockname(socket, &local, &length) >= 0 && local.address != 0)
    {
        char text[16];
        format_ipv4(local.address, text, sizeof(text));
        result = text;
    }
    sceNetSocketClose(socket);
    return result;
}

} // namespace pch::net
