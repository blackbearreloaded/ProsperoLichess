// ProsperoLichess - Paths after optional Lapy filesystem access.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "platform/ps5/storage_paths.hpp"

#include <sys/stat.h>

namespace pch::storage
{
namespace
{
bool g_access = false;

bool file_exists(const std::string &path)
{
    struct stat info
    {
    };
    return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

std::string join(const std::string &root, const char *relative)
{
    return root + (relative != nullptr && relative[0] == '/' ? "" : "/") +
           (relative != nullptr ? relative : "");
}
} // namespace

void set_filesystem_access(bool available)
{
    g_access = available;
}

bool filesystem_access()
{
    return g_access;
}

const std::string &app_root()
{
    static const std::string root = []
    {
        if (!g_access)
            return std::string{"/app0"};
        for (const char *candidate :
             {"/app0", "/system_ex/app/PPSA99009", "/data/homebrew/PPSA99009",
              "/mnt/sandbox/PPSA99009_000/app0"})
        {
            if (file_exists(std::string(candidate) + "/eboot.bin"))
                return std::string{candidate};
        }
        return std::string{"/data/homebrew/PPSA99009"};
    }();
    return root;
}

const std::string &data_root()
{
    static const std::string root =
        g_access ? "/data/prosperolichess" : "/download0/prosperolichess";
    return root;
}

std::string app_file(const char *relative)
{
    return join(app_root(), relative);
}

std::string data_file(const char *relative)
{
    return join(data_root(), relative);
}

} // namespace pch::storage
