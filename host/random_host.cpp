// ProsperoLichess - Host secure random bytes through getrandom().
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/random.hpp"

#include <cerrno>
#include <cstring>
#include <sys/random.h>

namespace pch::net
{

bool secure_random(void *out, std::size_t size)
{
    if (out == nullptr)
        return size == 0;
    auto *destination = static_cast<unsigned char *>(out);
    std::size_t done = 0;
    while (done < size)
    {
        const ssize_t got = getrandom(destination + done, size - done, 0);
        if (got < 0)
        {
            if (errno == EINTR)
                continue;
            std::memset(out, 0, size);
            return false;
        }
        done += static_cast<std::size_t>(got);
    }
    return true;
}

} // namespace pch::net
