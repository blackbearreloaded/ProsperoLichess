// ProsperoLichess - Opening title: the app's mark lands, the wordmark draws in.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/chrome.hpp"
#include "core/strings.hpp"
#include "core/tween.hpp"
#include "lichess/session.hpp"
#include "modes/scenes.hpp"
#include "ui/widgets.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace pch::modes
{

namespace
{

using gfx::Color;

constexpr float kSeconds = 2.6f;

class BootScene final : public app::Scene
{
  public:
    app::Transition update(app::Context &ctx, const InputFrame &input, float dt) override
    {
        time_ += dt;
        if (!chimed_ && time_ > 0.25f)
        {
            chimed_ = true;
            ctx.cue(audio::Cue::game_start);
        }
        const bool skip =
            time_ > 0.4f && (input.is_pressed(Action::confirm) || input.is_pressed(Action::back) ||
                             input.is_pressed(Action::menu));
        if (time_ >= kSeconds || skip || (ctx.reduced_motion() && time_ > 0.8f))
            return app::Transition::pop();
        return app::Transition::stay();
    }

    void draw(app::Context &ctx, app::Frame &frame) const override
    {
        namespace look = app::look;
        gfx::DrawList &list = frame.scene;
        const ui::Fonts &fonts = *ctx.fonts;
        const Color accent = look::accent(look::Section::boot);
        const bool calm = ctx.reduced_motion();
        // Under reduced motion the title is simply there.
        const float t = calm ? kSeconds : time_;
        constexpr float kCx = 960.0f;
        constexpr float kMarkY = 380.0f;

        const gfx::Rect screen{0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight};
        if (ctx.title_art != 0)
        {
            // The picture the console showed while the app started is still
            // there on the first frame; it dims as the title arrives over it.
            const float dim = calm ? 1.0f : tween::cubic_out(tween::clamp01((time_ - 0.1f) / 0.7f));
            list.image(ctx.title_art, screen, gfx::kFullUv, Color{1.0f, 1.0f, 1.0f, 1.0f});
            list.gradient_rect(screen, 0, Color::rgb(0x050812, 0.5f * dim),
                               Color::rgb(0x050812, 0.82f * dim));
        }
        else
        {
            // The page darkens so the title has the screen to itself.
            list.rounded_rect(screen, 0, Color::rgb(0x050812, 0.5f));
        }
        // Light blooms behind the mark.
        const float bloom = tween::cubic_out((t - 0.1f) / 1.2f);
        list.glow({kCx - 300, kMarkY - 300, 600, 600}, 300, 260, accent.with_alpha(0.22f * bloom));
        // Rings spread as the mark lands.
        for (int i = 0; i < 3; ++i)
        {
            const float r = tween::clamp01((t - 0.45f - 0.12f * static_cast<float>(i)) / 1.1f);
            if (r > 0.0f && r < 1.0f)
                list.ring(kCx, kMarkY, 150 + 380 * tween::cubic_out(r), 3.0f * (1.0f - r),
                          accent.with_alpha(0.5f * (1.0f - r)));
        }

        // The app's mark, the one the rail carries: a knight on a lit tile. It
        // drops in and settles with a small overshoot.
        const float land = tween::clamp01((t - 0.15f) / 0.55f);
        const float scale = 0.6f + 0.4f * tween::back_out(land);
        const float drop = (1.0f - tween::cubic_out(land)) * -110.0f;
        const float side = 232.0f * scale;
        const gfx::Rect tile{kCx - side * 0.5f, kMarkY - side * 0.5f + drop, side, side};
        list.push_opacity(land);
        list.shadow({tile.x, tile.y + 26.0f * scale, tile.w, tile.h}, 60.0f * scale, 50.0f,
                    Color::rgb(0x000000, 0.5f));
        list.glow(tile, 60.0f * scale, 40.0f, accent.with_alpha(0.35f));
        list.gradient_rect(tile, 60.0f * scale, gfx::mix(accent, look::kInk, 0.22f),
                           gfx::mix(accent, look::kNight, 0.55f));
        list.bordered_rect(tile, 60.0f * scale, look::kClear, 2.0f, look::kInk.with_alpha(0.35f));
        ctx.pieces->draw(list, {chess::Color::white, chess::Role::knight},
                         tile.inset(18.0f * scale));
        list.pop_opacity();

        // "PROSPERO", letter-spaced, then "Lichess" rising one letter at a time.
        const float small = tween::cubic_out(tween::clamp01((t - 0.6f) / 0.5f));
        ui::text(list, fonts.semibold, "PROSPERO", kCx + 9.0f, 606.0f + 12.0f * (1.0f - small),
                 30.0f, look::kInk.with_alpha(look::kMuted * small), gfx::Align::center, 18.0f);
        const std::string word = "Lichess";
        const float text_size = 148.0f;
        const float total = fonts.display.measure(word, text_size);
        const float left = kCx - total * 0.5f;
        for (std::size_t i = 0; i < word.size(); ++i)
        {
            const float p = tween::clamp01((t - 0.75f - 0.05f * static_cast<float>(i)) / 0.45f);
            const float e = tween::cubic_out(p);
            ui::text(list, fonts.display, word.substr(i, 1),
                     left + fonts.display.measure(word.substr(0, i), text_size),
                     750.0f + 30.0f * (1.0f - e), text_size, look::kInk.with_alpha(e));
        }
        // A rule sweeps out from the centre under the wordmark.
        const float sweep = tween::cubic_in_out(tween::clamp01((t - 1.2f) / 0.6f));
        list.glow({kCx - 200 * sweep, 792, 400 * sweep, 4}, 2, 12, accent.with_alpha(0.4f * sweep));
        list.rounded_rect({kCx - 200 * sweep, 792, 400 * sweep, 4}, 2, accent.with_alpha(sweep));
        const float tag = tween::cubic_out(tween::clamp01((t - 1.45f) / 0.5f));
        ui::text_fit(list, fonts.regular, tr("Chess on lichess.org, native on PS5"), kCx, 852.0f,
                     32.0f, app::kRight - app::kMargin, look::kInk.with_alpha(look::kMuted * tag),
                     gfx::Align::center);

        // What the app knows about the connection, said honestly. The line
        // is centred and ends before the note at the right, however long a
        // language makes either.
        const bool online = ctx.lichess != nullptr && ctx.lichess->online();
        const std::string state =
            online ? tr("Connected to lichess.org") : tr("Checking the connection");
        const char *client = tr("An unofficial client");
        constexpr float kClientRoom = 520.0f;
        const float client_width = std::min(fonts.regular.measure(client, 20.0f), kClientRoom);
        const float room = 2.0f * (app::kRight - client_width - 48.0f - kCx);
        const float width = std::min(fonts.regular.measure(state, 22.0f), room);
        look::live_dot(list, kCx - width * 0.5f - 18.0f, 1002.0f, 5.0f,
                       (online ? look::kGood : look::kGold).with_alpha(tag), time_,
                       calm || tag < 1.0f);
        ui::text_fit(list, fonts.regular, state, kCx, 1010.0f, 22.0f, room,
                     look::kInk.with_alpha(look::kFaint * tag), gfx::Align::center);
        ui::text_fit(list, fonts.regular, client, app::kRight, 1010.0f, 20.0f, kClientRoom,
                     look::kInk.with_alpha(look::kFaint * tag), gfx::Align::right);
    }

    bool shows_status_bar() const override
    {
        return false;
    }

    app::look::Mood mood() const override
    {
        return app::look::mood(app::look::Section::boot);
    }

    const char *name() const override
    {
        return "boot";
    }

  private:
    float time_ = 0.0f;
    bool chimed_ = false;
};

} // namespace

std::unique_ptr<app::Scene> make_boot()
{
    return std::make_unique<BootScene>();
}

} // namespace pch::modes
