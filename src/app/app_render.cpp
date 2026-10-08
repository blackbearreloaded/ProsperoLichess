// ProsperoLichess - The app's frame composition (the part that needs the renderer).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/app.hpp"

#include "gfx/renderer.hpp"

namespace pch::app
{

Gpu gpu_for(gfx::Renderer &renderer)
{
    Gpu gpu;
    gpu.create_texture = [&renderer](int width, int height, const std::uint8_t *rgba)
    { return renderer.batch().create_texture(width, height, rgba); };
    gpu.delete_texture = [&renderer](std::uint32_t texture)
    { renderer.batch().delete_texture(texture); };
    gpu.glass_texture = renderer.glass_texture();
    return gpu;
}

// Back to front: the backdrop, the screen going away, the screen, the status
// bar, then (above the blurred copy of all that) dialogs and notices.
void App::compose(gfx::Renderer &renderer)
{
    record_frame();
    renderer.begin();
    renderer.backdrop(backdrop_);
    renderer.draw(frames_[1].scene);
    renderer.draw(frames_[1].overlay);
    renderer.draw(frames_[0].scene);
    renderer.draw(bar_list_);
    if (frame_glass_)
        renderer.glass();
    renderer.draw(frames_[0].overlay);
    renderer.draw(top_list_);
}

} // namespace pch::app
