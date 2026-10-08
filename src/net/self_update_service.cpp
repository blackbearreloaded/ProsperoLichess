// ProsperoLichess - Background signed update check and staged in-place install.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/self_update_service.hpp"

#include "platform/ps5/system.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>

#ifndef PCH_HOST
#include "core/save_file.hpp"
#include "platform/ps5/storage_paths.hpp"
#include "net/self_update/self_update.h"
#include <pthread.h>
#endif

extern "C" const char *pch_self_update_path(int which)
{
#ifdef PCH_HOST
    (void)which;
    return "";
#else
    static const std::string helper = pch::storage::app_file("self-updater.elf");
    static const std::string param = pch::storage::app_file("sce_sys/param.json");
    static const std::string sequence = pch::storage::data_file("self-update-sequence");
    return which == 0 ? helper.c_str() : which == 1 ? param.c_str() : sequence.c_str();
#endif
}

namespace pch::update
{
namespace
{
std::mutex lock;
bool found = false;
Offer pending;

#ifndef PCH_HOST
constexpr std::size_t kStackSize = 1024 * 1024;
bool started = false;
self_update_check_result answer = SELF_UPDATE_UNKNOWN;
self_update_offer offer{};
self_update_job job{};
bool begun = false;

// The value of a string field of param.json ("" when it is not there).
std::string param_field(std::string_view json, std::string_view name)
{
    const std::size_t key = json.find("\"" + std::string(name) + "\"");
    const std::size_t open =
        key == std::string_view::npos ? key : json.find('"', json.find(':', key));
    const std::size_t close = open == std::string_view::npos ? open : json.find('"', open + 1);
    return close == std::string_view::npos ? std::string()
                                           : std::string(json.substr(open + 1, close - open - 1));
}

// Test deployments only (they carry assets/test, releases do not): the file
// update-offer.txt there replaces the catalog's answer, signature included,
// so an update can be tried before the catalog lists a newer release. Five
// lines: the new content version, the release's name, its ZIP on GitHub, its
// SHA-256, its size in bytes; any further lines are the release notes. Only a
// newer version is offered.
bool test_offer(self_update_offer *out)
{
    std::string text;
    std::string param;
    if (!save::read_file(storage::app_file("assets/test/update-offer.txt"), &text, 32768) ||
        !save::read_file(storage::app_file("sce_sys/param.json"), &param, 65536))
        return false;
    std::string lines[5];
    std::size_t at = 0;
    for (std::string &line : lines)
    {
        const std::size_t end = text.find('\n', at);
        line = text.substr(at, end == std::string::npos ? end : end - at);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.empty())
            return false;
        at = end == std::string::npos ? text.size() : end + 1;
    }
    self_update_offer filled{};
    std::snprintf(filled.title, sizeof(filled.title), "%s", param_field(param, "titleId").c_str());
    std::snprintf(filled.installed, sizeof(filled.installed), "%s",
                  param_field(param, "contentVersion").c_str());
    std::snprintf(filled.name, sizeof(filled.name), "ProsperoLichess");
    std::snprintf(filled.available, sizeof(filled.available), "%s", lines[0].c_str());
    std::snprintf(filled.version, sizeof(filled.version), "%s", lines[1].c_str());
    std::snprintf(filled.artifact, sizeof(filled.artifact), "%s", lines[2].c_str());
    std::snprintf(filled.sha256, sizeof(filled.sha256), "%s", lines[3].c_str());
    filled.size = std::strtoull(lines[4].c_str(), nullptr, 10);
    std::snprintf(filled.notes, sizeof(filled.notes), "%s",
                  at < text.size() ? text.c_str() + at : "");
    if (filled.title[0] == '\0' || filled.installed[0] == '\0' ||
        std::strcmp(filled.available, filled.installed) <= 0)
        return false;
    *out = filled;
    return true;
}

void *check(void *)
{
    self_update_offer result{};
    self_update_check_result state = self_update_check_self(&result);
    if (test_offer(&result))
    {
        sys::log("[PCH] update offer from assets/test/update-offer.txt replaces the catalog's");
        state = SELF_UPDATE_AVAILABLE;
    }
    sys::log("[PCH] signed update check state=%d installed=%s available=%s version=%s size=%llu",
             static_cast<int>(state), result.installed[0] ? result.installed : "-",
             result.available[0] ? result.available : "-", result.version[0] ? result.version : "-",
             static_cast<unsigned long long>(result.size));
    if (state == SELF_UPDATE_AVAILABLE || state == SELF_UPDATE_NOT_INSTALLABLE)
    {
        const std::lock_guard guard(lock);
        answer = state;
        offer = result;
        pending = {state == SELF_UPDATE_AVAILABLE,
                   result.version[0] ? result.version : result.available,
                   result.available,
                   result.size,
                   result.notes,
                   result.notes_truncated != 0};
        found = true;
    }
    return nullptr;
}

Phase from_kit(self_update_phase phase)
{
    switch (phase)
    {
    case SELF_UPDATE_STARTING:
        return Phase::starting;
    case SELF_UPDATE_DOWNLOADING:
        return Phase::downloading;
    case SELF_UPDATE_UNPACKING:
        return Phase::unpacking;
    case SELF_UPDATE_READY:
        return Phase::ready;
    case SELF_UPDATE_APPLYING:
        return Phase::applying;
    case SELF_UPDATE_CANCELLED:
        return Phase::cancelled;
    case SELF_UPDATE_FAILED:
        return Phase::failed;
    default:
        return Phase::idle;
    }
}
#else
bool begun = false;
bool cancelling = false;
unsigned ticks = 0;
#endif
} // namespace

void start_check()
{
#ifndef PCH_HOST
    {
        const std::lock_guard guard(lock);
        if (started)
            return;
        started = true;
    }
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0)
        return;
    (void)pthread_attr_setstacksize(&attributes, kStackSize);
    (void)pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    pthread_t thread;
    if (pthread_create(&thread, &attributes, check, nullptr) != 0)
        sys::log("[PCH] signed update check thread failed");
    (void)pthread_attr_destroy(&attributes);
#endif
}

bool take_offer(Offer *out)
{
    const std::lock_guard guard(lock);
    if (!found)
        return false;
    found = false;
    if (out != nullptr)
        *out = pending;
    return true;
}

void preview(std::string version, std::uint64_t size, std::string notes)
{
    const std::lock_guard guard(lock);
    pending = {true, std::move(version), {}, size, std::move(notes), false};
    found = true;
}

bool begin()
{
    const std::lock_guard guard(lock);
#ifndef PCH_HOST
    if (answer != SELF_UPDATE_AVAILABLE)
        return false;
    if (begun)
        self_update_finish(&job);
    begun = self_update_start(&job, self_update_console(), &offer) == 1;
#else
    begun = true;
    cancelling = false;
    ticks = 0;
#endif
    return begun;
}

Progress poll()
{
    const std::lock_guard guard(lock);
    if (!begun)
        return {};
#ifndef PCH_HOST
    self_update_status status{};
    self_update_poll(&job, &status);
    // Each step of an update is in the log once.
    static self_update_phase logged = SELF_UPDATE_IDLE;
    if (status.phase != logged)
    {
        logged = status.phase;
        sys::log("[PCH] update phase=%d done=%llu total=%llu %s", static_cast<int>(status.phase),
                 static_cast<unsigned long long>(status.done),
                 static_cast<unsigned long long>(status.total), status.error);
    }
    return {from_kit(status.phase), status.done, status.total, status.time_left, status.error};
#else
    ++ticks;
    if (cancelling)
        return {ticks > 8 ? Phase::cancelled : Phase::starting, 0, 0, {}, {}};
    if (ticks < 25)
        return {Phase::starting, 0, 0, {}, {}};
    if (ticks < 205)
        return {Phase::downloading, (ticks - 25u) * (32u << 20) / 180u, 32u << 20, {}, {}};
    if (ticks < 245)
        return {Phase::unpacking, ticks - 205u, 40, {}, {}};
    return {Phase::ready, 1, 1, {}, {}};
#endif
}

void cancel()
{
    const std::lock_guard guard(lock);
#ifndef PCH_HOST
    if (begun)
        self_update_cancel(&job);
#else
    cancelling = true;
    ticks = 0;
#endif
}

bool apply()
{
    const std::lock_guard guard(lock);
#ifndef PCH_HOST
    return begun && self_update_apply(&job) == 1;
#else
    return begun;
#endif
}

void finish()
{
    const std::lock_guard guard(lock);
#ifndef PCH_HOST
    if (begun)
        self_update_finish(&job);
#endif
    begun = false;
}

} // namespace pch::update
