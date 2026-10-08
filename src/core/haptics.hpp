// ProsperoLichess - Controller vibration for game events.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/cues.hpp"

namespace pch
{

struct Haptic
{
    float strength = 0.0f; // 0..1, 0 = none
    float seconds = 0.0f;
};

// The rumble that goes with a sound cue (none for interface sounds).
constexpr Haptic haptic_for(audio::Cue cue)
{
    switch (cue)
    {
    case audio::Cue::capture:
        return {0.45f, 0.07f};
    case audio::Cue::check:
        return {0.6f, 0.12f};
    case audio::Cue::castle:
    case audio::Cue::promote:
        return {0.3f, 0.08f};
    case audio::Cue::illegal:
    case audio::Cue::puzzle_wrong:
        return {0.5f, 0.05f};
    case audio::Cue::victory:
    case audio::Cue::new_record:
    case audio::Cue::puzzle_solved:
        return {0.7f, 0.3f};
    case audio::Cue::defeat:
    case audio::Cue::streak_end:
        return {0.8f, 0.35f};
    case audio::Cue::low_time:
    case audio::Cue::challenge:
        return {0.4f, 0.15f};
    default:
        return {};
    }
}

} // namespace pch
