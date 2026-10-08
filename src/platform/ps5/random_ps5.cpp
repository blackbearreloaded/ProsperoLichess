// ProsperoLichess - PS5 secure random bytes from the kernel RNG (kern.rng_pseudo).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/random.hpp"

#include <cstring>

extern "C" int sysctlbyname(const char *name, void *old_value, std::size_t *old_length,
                            const void *new_value, std::size_t new_length);

namespace pch::net
{

// Same source ProsperoLight uses for mbedTLS entropy on hardware: each call
// returns up to 64 fresh bytes.
bool secure_random(void *out, std::size_t size)
{
    if (out == nullptr)
        return size == 0;
    auto *destination = static_cast<unsigned char *>(out);
    unsigned char block[64];
    std::size_t remaining = size;
    while (remaining != 0)
    {
        std::size_t length = sizeof(block);
        const std::size_t take = remaining < sizeof(block) ? remaining : sizeof(block);
        if (sysctlbyname("kern.rng_pseudo", block, &length, nullptr, 0) != 0 || length < take)
        {
            std::memset(block, 0, sizeof(block));
            std::memset(out, 0, size);
            return false;
        }
        std::memcpy(destination, block, take);
        destination += take;
        remaining -= take;
    }
    std::memset(block, 0, sizeof(block));
    return true;
}

} // namespace pch::net
