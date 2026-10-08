// ProsperoLichess - System presentation art (icon0, pic0/pic1) drawn with the app renderer.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "board/pieces.hpp"
#include "gfx/draw_list.hpp"
#include "ui/fonts.hpp"

#include <functional>

namespace pch::host
{

// Draws art-icon (centred 1080x1080 square of the 1920x1080 frame) and
// art-background, handing each finished frame to write(name).
bool render_art(gfx::DrawList &list, const ui::Fonts &fonts, const board::PieceAtlas &pieces,
                const std::function<bool(const char *)> &write);

} // namespace pch::host
