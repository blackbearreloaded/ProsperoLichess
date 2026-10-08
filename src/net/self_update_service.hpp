// ProsperoLichess - Background signed update check and staged in-place install.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>

namespace pch::update
{

struct Offer
{
    bool installable = false;
    std::string version;
    std::string available;
    std::uint64_t size = 0;
    std::string notes; // the release's notes, plain text; empty when it has none
    bool notes_truncated = false;
};

enum class Phase
{
    idle,
    starting,
    downloading,
    unpacking,
    ready,
    applying,
    cancelled,
    failed,
};

struct Progress
{
    Phase phase = Phase::idle;
    std::uint64_t done = 0;
    std::uint64_t total = 0;
    std::string time_left;
    std::string error;
};

void start_check();
bool take_offer(Offer *offer);
bool begin();
Progress poll();
void cancel();
bool apply();
void finish();

// Host previews and unattended scripts exercise the real screen without an install.
void preview(std::string version, std::uint64_t size = 32u << 20, std::string notes = {});

} // namespace pch::update
