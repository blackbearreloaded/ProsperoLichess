// ProsperoLichess - The application: scene stack, shared services, saves and the frame.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/chrome.hpp"
#include "app/scene.hpp"
#include "board/pieces.hpp"
#include "core/settings.hpp"
#include "core/tween.hpp"
#include "gfx/backdrop_spec.hpp"
#include "puzzles/pack.hpp"
#include "ui/motion.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace pch::lichess
{
class Session;
}
namespace pch::gfx
{
class Renderer;
}

namespace pch::app
{

// What the app needs from the graphics side. The console and the PC renderer
// fill it from a gfx::Renderer (gpu_for); tests pass stand-ins, so the whole
// app runs without OpenGL.
struct Gpu
{
    std::function<std::uint32_t(int width, int height, const std::uint8_t *rgba)> create_texture;
    std::function<void(std::uint32_t texture)> delete_texture;
    std::uint32_t glass_texture = 0; // the renderer's blurred copy of the frame
};
Gpu gpu_for(gfx::Renderer &renderer);

class App
{
  public:
    App(Gpu gpu, const ui::Fonts &fonts, float surface_scale, std::string data_root,
        std::string assets, bool connect = true, bool boot = true);
    ~App();

    static Settings load_settings(const std::string &data_root);

    void update(const InputFrame &input, float dt);
    // Queues this frame's layers on the renderer (the caller presents it).
    void compose(gfx::Renderer &renderer);
    // Records this frame's draw lists without a renderer (compose does it
    // too; tests call it alone). Returns how many shapes were recorded.
    std::size_t record_frame();

    // The sounds and rumble the last update asked for.
    const ui::Feedback &feedback() const
    {
        return feedback_;
    }
    void set_version(const std::string &version);
    // Asks homebrew.page, once per launch, whether a newer release is listed
    // (param_json: the installed app's own param.json). A newer one is
    // announced for ten seconds once the home screen is up; anything else
    // (not listed, no network) shows nothing.
    void check_for_update(const std::string &param_json);
    // The announcement itself (unattended runs and pictures show it directly).
    void announce_update(const std::string &version);
    // Opens the full update flow with a simulated offer (hardware scripts and host previews).
    void preview_update(const std::string &version);
    // Frames per second for the optional readout (0 hides it).
    void set_fps(float fps)
    {
        fps_ = fps;
    }
    const Settings &settings() const
    {
        return settings_;
    }
    void set_applied_resolution(int resolution)
    {
        applied_resolution_ = resolution;
    }
    bool take_settings_changed()
    {
        const bool changed = settings_changed_;
        settings_changed_ = false;
        return changed;
    }
    bool take_display_mode_changed()
    {
        const bool changed = display_mode_changed_;
        display_mode_changed_ = false;
        return changed;
    }
    bool quit_requested() const
    {
        return quit_requested_;
    }
    // Logs account-free network checks (POST body, certificate refusal); used by
    // unattended hardware runs.
    void network_selftest();
    // Opens a screen directly (host snapshots of screens that need an account).
    void open(std::unique_ptr<Scene> scene);
    // Closes every screen above the home screen.
    void go_home();
    Context &context()
    {
        return ctx_;
    }
    lichess::Session &session()
    {
        return *session_;
    }
    void release_gpu();
    // After a display restart: the renderer's textures have new names.
    void restore_gpu(float surface_scale, std::uint32_t glass_texture);
    // Name of the top scene (for logs).
    const char *active_scene() const;

  private:
    void apply(Transition transition);
    void save_settings();
    void reload_pieces();
    void record(Frame &frame, const Scene &scene, float opacity, float scale, float dy);

    Gpu gpu_;
    ui::Fonts fonts_;
    float surface_scale_;
    std::string root_;
    std::string assets_;
    Settings settings_;
    board::PieceAtlas pieces_;
    puzzles::Pack pack_;
    bool pack_loaded_ = false;
    std::unique_ptr<lichess::Session> session_;
    ui::Feedback feedback_;
    Context ctx_;
    std::vector<std::unique_ptr<Scene>> stack_;
    StatusChrome status_;
    tween::Spring bar_alpha_;
    tween::Timer fade_;
    // The scene being replaced or closed, kept alive while it animates out.
    std::unique_ptr<Scene> leaving_;
    bool entering_ = false; // the top scene is animating in (push / replace)
    // What compose() hands the renderer; they must outlive present().
    gfx::BackdropSpec backdrop_;
    std::array<Frame, 2> frames_; // [0] the top scene, [1] the one going away
    gfx::DrawList bar_list_;
    gfx::DrawList top_list_;
    float fps_ = 0.0f;
    std::array<ui::SpringColor, 4> sky_; // the backdrop, easing to the top screen's mood
    ui::SpringColor accent_;
    std::string update_ready_;   // a newer release, waiting for the home screen
    bool update_asked_ = false;  // one request per launch
    float update_shown_ = -1.0f; // seconds the announcement has been up; -1: none
    bool frame_glass_ = false;   // the recorded frame wants the blurred copy
    bool settings_changed_ = true;
    bool display_mode_changed_ = false;
    int applied_resolution_ = -1;
    int loaded_piece_set_ = -1;
    bool quit_requested_ = false;
};

} // namespace pch::app
