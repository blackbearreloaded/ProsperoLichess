// ProsperoLichess - QR codes drawn with the 2D draw list.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/qr.hpp"

#include "third_party/qrcodegen/qrcodegen.h"

#include <string>

namespace pch::ui
{

bool QrCode::encode(std::string_view text)
{
    std::vector<std::uint8_t> qr(qrcodegen_BUFFER_LEN_MAX);
    std::vector<std::uint8_t> temp(qrcodegen_BUFFER_LEN_MAX);
    const std::string input(text);
    if (!qrcodegen_encodeText(input.c_str(), temp.data(), qr.data(), qrcodegen_Ecc_MEDIUM,
                              qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO,
                              true))
    {
        size_ = 0;
        modules_.clear();
        return false;
    }
    size_ = qrcodegen_getSize(qr.data());
    modules_.assign(static_cast<std::size_t>(size_ * size_), 0);
    for (int y = 0; y < size_; ++y)
        for (int x = 0; x < size_; ++x)
            modules_[static_cast<std::size_t>(y * size_ + x)] =
                qrcodegen_getModule(qr.data(), x, y) ? 1 : 0;
    return true;
}

void QrCode::draw(gfx::DrawList &list, const gfx::Rect &r) const
{
    list.rounded_rect(r, r.w * 0.04f, gfx::Color::rgb(0xffffff));
    if (size_ == 0)
        return;
    const float quiet = 4.0f;
    const float cell = r.w / (static_cast<float>(size_) + 2.0f * quiet);
    const gfx::Color ink = gfx::Color::rgb(0x111111);
    // Merge horizontal runs into single rectangles to keep the batch small.
    for (int y = 0; y < size_; ++y)
    {
        int x = 0;
        while (x < size_)
        {
            if (!module(x, y))
            {
                ++x;
                continue;
            }
            int end = x;
            while (end < size_ && module(end, y))
                ++end;
            list.rounded_rect({r.x + (quiet + static_cast<float>(x)) * cell,
                               r.y + (quiet + static_cast<float>(y)) * cell,
                               static_cast<float>(end - x) * cell + 0.5f, cell + 0.5f},
                              0.0f, ink);
            x = end;
        }
    }
}

} // namespace pch::ui
