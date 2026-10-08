// ProsperoLichess - Player settings and their save format.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/settings.hpp"

#include "core/bytes.hpp"

#include <algorithm>

namespace pch
{

namespace
{
// Version 1: audio, motion, buttons, FPS, resolution, board and play options.
// Version 2 appends vibration; version 1 saves still load.
constexpr std::uint8_t kVersion = 2;
constexpr int kMaxThemes = 32;
} // namespace

std::string encode_settings(const Settings &settings)
{
    bytes::Writer w;
    w.put(kVersion);
    w.put(static_cast<std::uint8_t>(settings.music_volume));
    w.put(static_cast<std::uint8_t>(settings.sfx_volume));
    w.put(static_cast<std::uint8_t>(settings.ui_volume));
    w.put_bool(settings.reduced_motion);
    w.put_bool(settings.swap_confirm);
    w.put_bool(settings.show_fps);
    w.put(static_cast<std::uint8_t>(settings.resolution));
    w.put(static_cast<std::uint8_t>(settings.board_theme));
    w.put(static_cast<std::uint8_t>(settings.piece_set));
    w.put_bool(settings.coordinates);
    w.put_bool(settings.show_dests);
    w.put_bool(settings.auto_queen);
    w.put_bool(settings.premoves);
    w.put_bool(settings.confirm_resign);
    w.put_bool(settings.vibration);
    return w.data();
}

bool decode_settings(std::string_view data, Settings *settings)
{
    bytes::Reader r(data);
    const std::uint8_t version = r.get<std::uint8_t>();
    if (version != 1 && version != kVersion)
        return false;
    Settings s;
    s.music_volume = std::clamp<int>(r.get<std::uint8_t>(), 0, 10);
    s.sfx_volume = std::clamp<int>(r.get<std::uint8_t>(), 0, 10);
    s.ui_volume = std::clamp<int>(r.get<std::uint8_t>(), 0, 10);
    s.reduced_motion = r.get_bool();
    s.swap_confirm = r.get_bool();
    s.show_fps = r.get_bool();
    s.resolution = std::clamp<int>(r.get<std::uint8_t>(), 0, Settings::kResolutionCount - 1);
    s.board_theme = std::clamp<int>(r.get<std::uint8_t>(), 0, kMaxThemes);
    s.piece_set = std::clamp<int>(r.get<std::uint8_t>(), 0, kMaxThemes);
    s.coordinates = r.get_bool();
    s.show_dests = r.get_bool();
    s.auto_queen = r.get_bool();
    s.premoves = r.get_bool();
    s.confirm_resign = r.get_bool();
    if (version >= 2)
        s.vibration = r.get_bool();
    if (!r.finished())
        return false;
    *settings = s;
    return true;
}

} // namespace pch
