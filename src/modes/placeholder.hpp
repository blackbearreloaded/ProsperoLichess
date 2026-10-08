// ProsperoLichess - Stand-ins for screens that are being rebuilt.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/chrome.hpp"
#include "modes/page.hpp"
#include "ui/components/stat.hpp"

#include <memory>
#include <string>

namespace pch::modes
{

// A page that only says what will be here.
class PlaceholderPage final : public Page
{
  public:
    PlaceholderPage(const char *name, const char *title) : name_(name)
    {
        empty_.title = title;
        empty_.body = "This page is being rebuilt.";
        empty_.action.clear();
        empty_.set_bounds(
            {app::kContent, app::kTop, app::kRight - app::kContent, app::kBottom - app::kTop});
    }
    PageResult update(app::Context &ctx, const InputFrame &input, float dt, bool focused) override
    {
        PageResult result;
        if (focused && (input.is_pressed(Action::back) || input.nav == Direction::left))
        {
            ctx.cue(audio::Cue::back);
            result.to_rail = true;
        }
        empty_.update(dt);
        return result;
    }
    void draw(app::Context &ctx, app::Frame &frame, bool) const override
    {
        ui::Canvas canvas = app::canvas_for(ctx, frame.scene);
        empty_.draw(canvas);
    }
    std::span<const ui::Hint> hints() const override
    {
        static constexpr ui::Hint kHints[] = {{ui::Button::circle, "Back"}};
        return kHints;
    }
    const char *name() const override
    {
        return name_;
    }

  private:
    const char *name_;
    ui::EmptyState empty_;
};

// A screen that only says what will be here; Circle closes it.
class PlaceholderScene final : public app::Scene
{
  public:
    PlaceholderScene(const char *name, std::string title) : name_(name)
    {
        empty_.title = std::move(title);
        empty_.body = "This screen is being rebuilt.";
        empty_.set_bounds(
            {app::kMargin, app::kTop, app::kRight - app::kMargin, app::kBottom - app::kTop});
    }
    app::Transition update(app::Context &ctx, const InputFrame &input, float dt) override
    {
        empty_.update(dt);
        if (input.is_pressed(Action::back))
        {
            ctx.cue(audio::Cue::back);
            return app::Transition::pop();
        }
        return app::Transition::stay();
    }
    void draw(app::Context &ctx, app::Frame &frame) const override
    {
        ui::Canvas canvas = app::canvas_for(ctx, frame.scene);
        empty_.draw(canvas);
        const ui::Hint hints[] = {{ui::Button::circle, "Back"}};
        app::draw_hints(ctx, frame.scene, hints, 1);
    }
    const char *name() const override
    {
        return name_;
    }

  private:
    const char *name_;
    ui::EmptyState empty_;
};

} // namespace pch::modes
