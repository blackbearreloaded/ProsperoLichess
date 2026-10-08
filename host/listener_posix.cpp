// ProsperoLichess - Host (BSD sockets) TCP primitives for the callback listener.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/listener.hpp"
#include "net/socket.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace pch::net
{

namespace sock
{

namespace
{

bool set_nonblocking(int fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

} // namespace

int listen_tcp(std::uint16_t port, std::string *error)
{
    const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
    {
        if (error != nullptr)
            *error = std::string("socket: ") + std::strerror(errno);
        return -1;
    }
    const int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 ||
        ::listen(fd, 4) != 0 || !set_nonblocking(fd))
    {
        if (error != nullptr)
            *error = std::string("bind/listen: ") + std::strerror(errno);
        ::close(fd);
        return -1;
    }
    return fd;
}

int accept_client(int listener)
{
    const int fd = ::accept(listener, nullptr, nullptr);
    if (fd < 0)
        return -1;
    if (!set_nonblocking(fd))
    {
        ::close(fd);
        return -1;
    }
    return fd;
}

int recv_some(int socket, void *buffer, std::size_t size, int timeout_ms)
{
    pollfd entry{socket, POLLIN, 0};
    const int ready = ::poll(&entry, 1, timeout_ms);
    if (ready == 0)
        return -2;
    if (ready < 0)
        return errno == EINTR ? -2 : -1;
    const ssize_t got = ::recv(socket, buffer, size, 0);
    if (got < 0)
        return (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) ? -2 : -1;
    return static_cast<int>(got);
}

bool send_all(int socket, const void *data, std::size_t size, int timeout_ms)
{
    const auto *bytes = static_cast<const char *>(data);
    while (size > 0)
    {
        pollfd entry{socket, POLLOUT, 0};
        if (::poll(&entry, 1, timeout_ms) <= 0)
            return false;
        const ssize_t sent = ::send(socket, bytes, size, MSG_NOSIGNAL);
        if (sent < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                continue;
            return false;
        }
        bytes += sent;
        size -= static_cast<std::size_t>(sent);
    }
    return true;
}

void close_socket(int socket)
{
    if (socket >= 0)
        ::close(socket);
}

} // namespace sock

std::string lan_ipv4()
{
    // Connecting a UDP socket sends nothing; it only selects the outgoing route.
    const int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return {};
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(53);
    target.sin_addr.s_addr = htonl(0x08080808u);
    std::string result;
    sockaddr_in local{};
    socklen_t length = sizeof(local);
    if (::connect(fd, reinterpret_cast<sockaddr *>(&target), sizeof(target)) == 0 &&
        ::getsockname(fd, reinterpret_cast<sockaddr *>(&local), &length) == 0)
    {
        char text[INET_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET, &local.sin_addr, text, sizeof(text)) != nullptr &&
            std::strcmp(text, "0.0.0.0") != 0)
        {
            result = text;
        }
    }
    ::close(fd);
    return result;
}

} // namespace pch::net
