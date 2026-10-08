// ProsperoLichess - Test fixture: the whole app without a GPU, for the Play and Watch tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/app.hpp"
#include "core/save_file.hpp"
#include "gfx/font.hpp"
#include "lichess/session.hpp"
#include "modes/page.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <string>

#ifndef PCH_SOURCE_DIR
#define PCH_SOURCE_DIR "."
#endif

namespace pch::play_tests
{

struct Faces
{
    gfx::Font regular;
    gfx::Font semibold;
    gfx::Font display;
    gfx::Font mono;
    ui::Fonts fonts;
    bool ok = false;
};

// The baked faces are large: load them once for every test of a file.
inline Faces &faces()
{
    static Faces set;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        const std::string dir = std::string(PCH_SOURCE_DIR) + "/assets/fonts/";
        const auto load =
            [&](const char *name, gfx::Font *font, ui::FontRef *ref, std::uint32_t texture)
        {
            std::string data;
            if (!save::read_file(dir + name, &data) || !font->load(data))
                return false;
            *ref = {font, texture};
            return true;
        };
        set.ok = load("inter-regular.pchfont", &set.regular, &set.fonts.regular, 1) &&
                 load("inter-semibold.pchfont", &set.semibold, &set.fonts.semibold, 2) &&
                 load("montserrat-medium.pchfont", &set.display, &set.fonts.display, 3) &&
                 load("dejavu-sans-mono.pchfont", &set.mono, &set.fonts.mono, 4);
        set.fonts.pixel = set.fonts.mono;
        set.fonts.hand = set.fonts.regular;
    }
    return set;
}

// Drives the real app (offline) one frame at a time, as a controller would.
class AppFixture : public ::testing::Test
{
  protected:
    static constexpr float kFrame = 1.0f / 60.0f;

    void SetUp() override
    {
        ASSERT_TRUE(faces().ok) << "baked fonts missing";
        char root[] = "/tmp/pch-play-test-XXXXXX";
        ASSERT_NE(mkdtemp(root), nullptr);
        app::Gpu gpu;
        gpu.create_texture = [this](int, int, const std::uint8_t *) { return ++textures_; };
        gpu.delete_texture = [](std::uint32_t) {};
        gpu.glass_texture = 9;
        app_ = std::make_unique<app::App>(std::move(gpu), faces().fonts, 1.0f, root,
                                          std::string(PCH_SOURCE_DIR) + "/assets", false, false);
    }

    // One frame: update, then record what would be drawn.
    void frame(InputFrame input = {})
    {
        input.connected = true;
        app_->update(input, kFrame);
        shapes_ = app_->record_frame();
    }
    void idle(int frames)
    {
        for (int i = 0; i < frames; ++i)
            frame();
    }
    void press(Action action)
    {
        InputFrame input;
        input.pressed = action_bit(action);
        input.held = action_bit(action);
        frame(input);
    }
    void nav(Direction direction)
    {
        InputFrame input;
        input.nav = direction;
        frame(input);
    }
    bool asked(audio::Cue cue) const
    {
        for (const audio::CueEvent &event : app_->feedback().cues)
        {
            if (event.cue == cue)
                return true;
        }
        return false;
    }
    // A previewed account: online and signed in, nothing is sent.
    void sign_in()
    {
        lichess::Account account;
        account.id = "tester";
        account.username = "tester";
        account.perfs = {{"rapid", 1500, false, 10, 0, {}}, {"puzzle", 1600, false, 20, 0, {}}};
        app_->session().preview(std::move(account), {});
    }
    // Shows a page of the home screen with the controller in it.
    void open_page(int index)
    {
        idle(5);
        press(Action::menu);
        for (int i = 0; i < modes::kPageCount; ++i)
            nav(Direction::up);
        for (int i = 0; i < index; ++i)
            nav(Direction::down);
        press(Action::confirm);
        idle(30);
    }
    std::string scene() const
    {
        return app_->active_scene();
    }

    std::uint32_t textures_ = 100;
    std::unique_ptr<app::App> app_;
    std::size_t shapes_ = 0;
};

} // namespace pch::play_tests
