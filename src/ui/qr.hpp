// ProsperoLichess - QR codes drawn with the 2D draw list.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "gfx/draw_list.hpp"

#include <string_view>
#include <vector>

namespace pch::ui
{

class QrCode
{
  public:
    // Encodes text (byte mode, medium error correction). Returns false if too long.
    bool encode(std::string_view text);
    int size() const
    {
        return size_;
    }
    // False until encode() succeeded: draw() then shows the empty plate.
    bool valid() const
    {
        return size_ > 0;
    }
    bool module(int x, int y) const
    {
        return modules_[static_cast<std::size_t>(y * size_ + x)] != 0;
    }
    // Draws the code on a white plate filling r (square); the plate keeps the
    // four-module quiet zone a scanner needs. Black on white whatever the
    // theme: a phone camera reads nothing else reliably.
    void draw(gfx::DrawList &list, const gfx::Rect &r) const;

  private:
    int size_ = 0;
    std::vector<unsigned char> modules_;
};

} // namespace pch::ui
