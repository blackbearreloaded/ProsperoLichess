// ProsperoLichess - The application: scene stack, shared services, saves and the frame.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/app.hpp"

#include "core/save_file.hpp"
#include "core/strings.hpp"
#include "lichess/session.hpp"
#include "net/self_update_service.hpp"
#include "third_party/stb/image.h"
#include "modes/scenes.hpp"
#include "modes/update_scene.hpp"
#include "platform/ps5/system.hpp"

#include <algorithm>
#include <cstdio>
#include <string_view>

namespace pch::app
{

App::App(Gpu gpu, const ui::Fonts &fonts, float surface_scale, std::string data_root,
         std::string assets, bool connect, bool boot)
    : gpu_(std::move(gpu)), fonts_(fonts), surface_scale_(surface_scale),
      root_(std::move(data_root)), assets_(std::move(assets))
{
    save::ensure_directory(root_);
    settings_ = load_settings(root_);
    reload_pieces();

    std::string pack;
    std::string error;
    if (save::read_file(assets_ + "/puzzles/pack.bin", &pack, 64u << 20) &&
        pack_.load(std::move(pack), &error))
    {
        pack_loaded_ = true;
        sys::log("[PCH] puzzle pack puzzles=%zu themes=%d", pack_.size(), pack_.theme_count());
    }
    else
    {
        sys::log("[PCH] puzzle pack unavailable: %s", error.c_str());
    }
    session_ = std::make_unique<lichess::Session>(root_, connect);

    ctx_.fonts = &fonts_;
    ctx_.pieces = &pieces_;
    ctx_.settings = &settings_;
    ctx_.feedback = &feedback_;
    ctx_.lichess = session_.get();
    ctx_.pack = pack_loaded_ ? &pack_ : nullptr;
    ctx_.data_root = root_;
    ctx_.assets = assets_;
    ctx_.notify = [this](const std::string &text, Note kind) { status_.notify(text, kind); };
    ctx_.settings_changed = [this]()
    {
        save_settings();
        settings_changed_ = true;
        if (settings_.resolution != applied_resolution_ && applied_resolution_ >= 0)
            display_mode_changed_ = true;
        if (settings_.piece_set != loaded_piece_set_)
            reload_pieces();
    };
    ctx_.quit = [this]() { quit_requested_ = true; };
    stack_.push_back(modes::make_shell(ctx_));
    stack_.back()->enter(ctx_);
    if (boot)
    {
        // The opening title continues the picture the console showed while
        // the app started. Decoded here, once, before the first frame.
        std::string jpeg;
        if (save::read_file(assets_ + "/art/title.jpg", &jpeg, 4u << 20))
        {
            int width = 0;
            int height = 0;
            unsigned char *pixels =
                pch_image_decode_jpeg(reinterpret_cast<const unsigned char *>(jpeg.data()),
                                      static_cast<int>(jpeg.size()), &width, &height);
            if (pixels != nullptr)
            {
                ctx_.title_art = gpu_.create_texture(width, height, pixels);
                pch_image_free(pixels);
            }
        }
        sys::log("[PCH] title art %s", ctx_.title_art != 0 ? "loaded" : "missing");
        stack_.push_back(modes::make_boot());
    }
    bar_alpha_.snap(stack_.back()->shows_status_bar() ? 1.0f : 0.0f);
    const look::Mood mood = stack_.back()->mood();
    for (std::size_t i = 0; i < sky_.size(); ++i)
        sky_[i].snap(mood.sky[i]);
    accent_.snap(mood.accent);
    ctx_.accent = mood.accent;
}

App::~App() = default;

Settings App::load_settings(const std::string &data_root)
{
    Settings settings;
    std::string data;
    if (save::read_file(data_root + "/settings.bin", &data))
    {
        const auto decoded = save::decode(save::Kind::settings, data);
        if (!decoded.ok || !decode_settings(decoded.payload, &settings))
            sys::log("[PCH] settings.bin ignored: %s", decoded.error.c_str());
    }
    settings.board_theme = std::clamp(settings.board_theme, 0, board::kBoardThemeCount - 1);
    settings.piece_set = std::clamp(settings.piece_set, 0, board::kPieceSetCount - 1);
    return settings;
}

void App::save_settings()
{
    const std::string error = save::write_atomic(
        root_ + "/settings.bin", save::encode(save::Kind::settings, 1, encode_settings(settings_)));
    if (!error.empty())
        sys::log("[PCH] settings save failed: %s", error.c_str());
}

void App::reload_pieces()
{
    const float square = kBoardSquares.w / 8.0f * surface_scale_;
    std::string error;
    std::uint32_t old[board::PieceAtlas::kLevels];
    for (int level = 0; level < board::PieceAtlas::kLevels; ++level)
        old[level] = pieces_.level_texture(level);
    if (!pieces_.load(assets_, settings_.piece_set, square, surface_scale_, gpu_.create_texture,
                      &error))
    {
        sys::log("[PCH] pieces failed: %s", error.c_str());
        return;
    }
    for (const std::uint32_t texture : old)
    {
        if (texture != 0)
            gpu_.delete_texture(texture);
    }
    loaded_piece_set_ = settings_.piece_set;
    sys::log("[PCH] pieces %s square=%.0fpx", board::kPieceSets[loaded_piece_set_].id, square);
}

void App::set_version(const std::string &version)
{
    ctx_.version = version;
}

const char *App::active_scene() const
{
    return stack_.empty() ? "" : stack_.back()->name();
}

void App::apply(Transition transition)
{
    if (transition.kind == Transition::Kind::none)
        return;
    leaving_.reset();
    entering_ = false;
    switch (transition.kind)
    {
    case Transition::Kind::none:
        return;
    case Transition::Kind::push:
        if (transition.next)
        {
            stack_.push_back(std::move(transition.next));
            entering_ = true;
        }
        break;
    case Transition::Kind::replace:
        if (transition.next)
        {
            leaving_ = std::move(stack_.back());
            stack_.pop_back();
            stack_.push_back(std::move(transition.next));
            entering_ = true;
        }
        break;
    case Transition::Kind::pop:
        if (stack_.size() > 1)
        {
            leaving_ = std::move(stack_.back());
            stack_.pop_back();
        }
        break;
    case Transition::Kind::pop_to_root:
        if (stack_.size() > 1)
            leaving_ = std::move(stack_.back());
        while (stack_.size() > 1)
            stack_.pop_back();
        break;
    }
    fade_.start(settings_.reduced_motion ? 0.12f : 0.34f);
    sys::log("[PCH] scene %s depth=%zu", stack_.back()->name(), stack_.size());
    stack_.back()->enter(ctx_);
}

void App::update(const InputFrame &input, float dt)
{
    feedback_.clear();
    ctx_.time += dt;
    fade_.update(dt);
    if (!fade_.running)
    {
        leaving_.reset();
        entering_ = false;
    }
    session_->pump(dt);
    update::Offer offer;
    if (update::take_offer(&offer))
    {
        if (offer.installable)
            apply(Transition::push(modes::make_update(std::move(offer))));
        else
            announce_update(offer.version);
    }
    status_.set_place(stack_.back()->place());
    if (!update_ready_.empty() && std::string_view(stack_.back()->name()) != "boot")
    {
        announce_update(update_ready_);
        update_ready_.clear();
    }
    status_.update(ctx_, dt);
    if (update_shown_ >= 0.0f)
    {
        update_shown_ += dt;
        if (!status_.announcing())
        {
            sys::log("[PCH] update notice gone after %.1fs", static_cast<double>(update_shown_));
            update_shown_ = -1.0f;
        }
    }
    // Scenes under the top keep their clocks and streams alive.
    for (std::size_t i = 0; i + 1 < stack_.size(); ++i)
        stack_[i]->tick(ctx_, dt);
    apply(stack_.back()->update(ctx_, input, dt));
    // Colour follows content: the sky drifts to the mood of the top screen.
    const look::Mood mood = stack_.back()->mood();
    for (std::size_t i = 0; i < sky_.size(); ++i)
    {
        sky_[i].target(mood.sky[i]);
        sky_[i].update(dt, settings_.reduced_motion ? 60.0f : 2.6f);
    }
    accent_.target(mood.accent);
    accent_.update(dt, settings_.reduced_motion ? 60.0f : 7.0f);
    ctx_.accent = accent_.value();
    bar_alpha_.target = stack_.back()->shows_status_bar() ? 1.0f : 0.0f;
    bar_alpha_.update(dt, settings_.reduced_motion ? 60.0f : 10.0f);
}

void App::record(Frame &frame, const Scene &scene, float opacity, float scale, float dy)
{
    frame.reset();
    frame.glass_texture = gpu_.glass_texture;
    if (opacity < 1.0f || scale != 1.0f || dy != 0.0f)
    {
        const bool calm = settings_.reduced_motion;
        for (gfx::DrawList *list : {&frame.scene, &frame.overlay})
        {
            list->push_opacity(opacity);
            list->push_transform(calm ? 1.0f : scale, gfx::kVirtualWidth * 0.5f,
                                 gfx::kVirtualHeight * 0.5f, 0.0f, calm ? 0.0f : dy);
        }
    }
    scene.draw(ctx_, frame);
}

std::size_t App::record_frame()
{
    backdrop_ = ctx_.theme().backdrop;
    for (std::size_t i = 0; i < sky_.size(); ++i)
        backdrop_.colors[i] = sky_[i].value();
    // The backdrop drifts slowly; with reduced motion it stands still.
    backdrop_.time = 40.0f + (settings_.reduced_motion ? 0.0f : ctx_.time);

    Frame &now = frames_[0];
    Frame &old = frames_[1];
    old.reset();
    const std::size_t top = stack_.size() - 1;
    const bool animating = fade_.running;
    const float t = animating ? tween::cubic_out(fade_.progress()) : 1.0f;
    // One screen gives way to the next: the old one fades as the new one
    // rises in (opening) or settles back (closing).
    const Scene *going = nullptr;
    if (animating)
        going = leaving_ ? leaving_.get() : entering_ && top > 0 ? stack_[top - 1].get() : nullptr;
    if (going != nullptr && entering_)
    {
        record(old, *going, 1.0f - t, 1.0f + 0.03f * t, 0.0f);
        record(now, *stack_[top], t, 0.96f + 0.04f * t, 28.0f * (1.0f - t));
    }
    else if (going != nullptr)
    {
        record(old, *going, 1.0f - t, 1.0f - 0.04f * t, 28.0f * t);
        record(now, *stack_[top], t, 1.03f - 0.03f * t, 0.0f);
    }
    else
    {
        record(now, *stack_[top], 1.0f, 1.0f, 0.0f);
    }

    bar_list_.clear();
    if (bar_alpha_.value > 0.01f)
    {
        bar_list_.push_opacity(bar_alpha_.value);
        status_.draw_bar(ctx_, bar_list_);
        bar_list_.pop_opacity();
    }

    // Notices belong to the status bar: a screen without it (the opening
    // title) is left alone.
    const bool notices = bar_alpha_.value > 0.01f && status_.has_notices();
    frame_glass_ = now.glass || notices;
    top_list_.clear();
    if (notices)
    {
        top_list_.push_opacity(bar_alpha_.value);
        status_.draw_notices(ctx_, top_list_, frame_glass_ ? gpu_.glass_texture : 0);
        top_list_.pop_opacity();
    }
    if (settings_.show_fps && fps_ > 0.0f)
    {
        char text[32];
        std::snprintf(text, sizeof(text), "%.0f FPS", static_cast<double>(fps_));
        ui::text(top_list_, fonts_.mono, text, 1900.0f, 30.0f, 18.0f,
                 gfx::Color::rgb(0xffffff, 0.6f), gfx::Align::right);
    }
    return old.scene.instances().size() + old.overlay.instances().size() +
           now.scene.instances().size() + now.overlay.instances().size() +
           bar_list_.instances().size() + top_list_.instances().size();
}

void App::open(std::unique_ptr<Scene> scene)
{
    apply(Transition::push(std::move(scene)));
}

void App::go_home()
{
    apply(Transition::home());
}

void App::check_for_update(const std::string &param_json)
{
    if (update_asked_)
        return;
    update_asked_ = true;
    (void)param_json;
    update::start_check();
}

void App::preview_update(const std::string &version)
{
    // What a release's notes look like once the catalog has made them plain text.
    update::preview(
        version, 32u << 20,
        "Play\n"
        "- The Touchpad opens the game that waits for your move.\n"
        "- L1 and R1 turn the tabs of the Puzzles and Play pages from the side rail too.\n"
        "\n"
        "Languages\n"
        "The app now speaks the language the console is set to: 29 languages besides "
        "English, with the chess words lichess.org uses in each.\n"
        "- Japanese, Korean, Chinese, Thai and Arabic are drawn with the console's own "
        "fonts.\n"
        "- Counts and shares are written as each language writes them.\n"
        "\n"
        "Note: your account, games and settings are kept by an update.\n"
        "\n"
        "Updates\n"
        "- A newer release is offered when the app opens, with these notes.\n"
        "- The download shows its share and the time left.\n"
        "Warning: the app closes to finish an update; open it again afterwards.\n"
        "\n"
        "Fixes\n"
        "- Lichess is reachable again once the app has filesystem access.\n"
        "- Settings lines, game clocks and hold buttons stay whole in every language.\n"
        "- A time control keeps its order inside right-to-left text.");
}

void App::announce_update(const std::string &version)
{
    constexpr float kSeconds = 10.0f;
    status_.announce(tr("Update available"), fill(tr("Version {0} is on homebrew.page"), {version}),
                     kSeconds);
    update_shown_ = 0.0f;
    sys::log("[PCH] update notice shown version=%s", version.c_str());
}

void App::network_selftest()
{
    lichess::Session *session = session_.get();
    // Lichess echoes each tested token: proves the POST body arrived intact.
    session->post("/api/token/test", "lip_selftestprobe",
                  [](const lichess::HttpResult &r)
                  {
                      sys::log("[PCH] selftest post status=%d echoed=%d %s", r.status,
                               r.body.find("lip_selftestprobe") != std::string::npos ? 1 : 0,
                               r.error.c_str());
                  });
    // Certificate checks: each of these must fail the TLS handshake.
    for (const char *url : {"https://expired.badssl.com/", "https://wrong.host.badssl.com/",
                            "https://self-signed.badssl.com/"})
    {
        const std::string target = url;
        session->get(target,
                     [target](const lichess::HttpResult &r)
                     {
                         sys::log("[PCH] selftest tls %s status=%d %s (%s)", target.c_str(),
                                  r.status, r.error.c_str(),
                                  r.status == 0 ? "refused" : "ACCEPTED");
                     });
    }
}

void App::release_gpu()
{
    // The title's picture goes with the display; the title is long over.
    ctx_.title_art = 0;
    for (auto &scene : stack_)
        scene->release_gpu();
    pieces_.forget();
    loaded_piece_set_ = -1;
}

void App::restore_gpu(float surface_scale, std::uint32_t glass_texture)
{
    surface_scale_ = surface_scale;
    gpu_.glass_texture = glass_texture;
    reload_pieces();
}

} // namespace pch::app
