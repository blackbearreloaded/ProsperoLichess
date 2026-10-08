// ProsperoLichess - Host (Linux/WSL) implementation of the system services.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "platform/ps5/system.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>

namespace pch::sys
{

std::int64_t monotonic_us()
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<std::int64_t>(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
}

void log(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, format, arguments);
    va_end(arguments);
    std::fputc('\n', stderr);
}

bool hide_splash_screen()
{
    return true;
}

void sleep_us(std::uint32_t microseconds)
{
    usleep(microseconds);
}

int system_language(int *)
{
    // A PC has no console language: the tools name one (PCH_LANG).
    return -1;
}

void park()
{
    std::exit(1);
}

void quit()
{
    std::exit(0);
}

} // namespace pch::sys
