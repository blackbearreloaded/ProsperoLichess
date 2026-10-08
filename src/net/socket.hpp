// ProsperoLichess - Tiny TCP primitives behind the callback listener (per platform).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Implemented by src/platform/ps5/listener_scenet.cpp (sceNet) and
// host/listener_posix.cpp (BSD sockets). Handles are plain ints.
namespace pch::net::sock
{

// Non-blocking listening socket on 0.0.0.0:port; -1 (and *error) on failure.
int listen_tcp(std::uint16_t port, std::string *error);
// A pending connection (switched to non-blocking), or -1 when none is waiting.
int accept_client(int listener);
// > 0 bytes read, 0 peer closed, -1 error, -2 nothing within timeout_ms.
int recv_some(int socket, void *buffer, std::size_t size, int timeout_ms);
// Sends everything within timeout_ms.
bool send_all(int socket, const void *data, std::size_t size, int timeout_ms);
void close_socket(int socket);

} // namespace pch::net::sock
