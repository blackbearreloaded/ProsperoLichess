// ProsperoLichess - Animated in-place update screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The offer (Update now / What's new / Skip) each time the app opens while a
// newer release is listed; the release's notes in a scrolling view of their
// own; then a ring fills while it downloads and unpacks, and the app closes
// for the update helper to replace its files.

#include "modes/update_scene.hpp"

#include "app/chrome.hpp"
#include "core/strings.hpp"
#include "gfx/font.hpp"
#include "ui/widgets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <vector>

namespace pch::modes
{
namespace
{
constexpr gfx::Rect kPanel{560.0f, 184.0f, 800.0f, 712.0f};
constexpr float kCenterX = 960.0f;
constexpr float kRingY = 390.0f;
constexpr float kRadius = 92.0f;
constexpr float kPi = 3.14159265f;
// The buttons share one row, two or three of them.
constexpr float kButtonsLeft = 612.0f;
constexpr float kButtonsWidth = 696.0f;
constexpr float kButtonGap = 16.0f;
constexpr float kButtonsTop = 730.0f;
constexpr float kButtonHeight = 76.0f;
// The notes view: a taller panel, the text's window under its title.
constexpr gfx::Rect kNotesPanel{560.0f, 108.0f, 800.0f, 864.0f};
constexpr float kNotesLeft = kNotesPanel.x + 64.0f;
constexpr float kNotesWidth = kNotesPanel.w - 128.0f - 18.0f; // room for the scrollbar
constexpr float kNotesTop = kNotesPanel.y + 132.0f;
constexpr float kNotesWindow = kNotesPanel.h - 132.0f - 150.0f;
constexpr float kNotesButtonsTop = kNotesPanel.y + kNotesPanel.h - 118.0f;
constexpr float kBodySize = 21.0f;
constexpr float kHeadingSize = 26.0f;

enum class Stage
{
    offer,
    notes,
    working,
    cancelling,
    closing,
    failed,
};

std::string megabytes(std::uint64_t bytes)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    return text;
}

void arc(gfx::DrawList &list, float start, float sweep, gfx::Color color)
{
    const int steps = std::max(2, static_cast<int>(sweep / (kPi / 50.0f)));
    float x = kCenterX + kRadius * std::cos(start);
    float y = kRingY + kRadius * std::sin(start);
    for (int i = 1; i <= steps; ++i)
    {
        const float at = start + sweep * static_cast<float>(i) / static_cast<float>(steps);
        const float nx = kCenterX + kRadius * std::cos(at);
        const float ny = kRingY + kRadius * std::sin(at);
        list.line(x, y, nx, ny, 10.0f, color);
        x = nx;
        y = ny;
    }
}

gfx::Rect button_rect(int count, float index, float top)
{
    const float width =
        (kButtonsWidth - kButtonGap * static_cast<float>(count - 1)) / static_cast<float>(count);
    return {kButtonsLeft + (width + kButtonGap) * index, top, width, kButtonHeight};
}

bool starts_with(std::string_view text, std::string_view prefix)
{
    return text.substr(0, prefix.size()) == prefix;
}

// One laid-out line of the notes, and the box behind a callout.
struct NoteLine
{
    std::string text;
    float y = 0.0f;
    float height = 0.0f;
    float indent = 0.0f;
    bool heading = false;
    bool bullet = false;
    bool quiet = false;
};
struct NoteBox
{
    float top = 0.0f;
    float bottom = 0.0f;
    bool warning = false;
};

class UpdateScene final : public app::Scene
{
  public:
    explicit UpdateScene(update::Offer offer) : offer_(std::move(offer))
    {
    }

    void enter(app::Context &ctx) override
    {
        ctx.cue(audio::Cue::modal_open);
        open_.snap(0.0f);
        progress_.snap(0.0f);
    }

    app::Transition update(app::Context &ctx, const InputFrame &input, float dt) override
    {
        time_ += dt;
        open_.target = 1.0f;
        open_.update(dt, ctx.reduced_motion() ? 60.0f : 12.0f);
        choice_x_.target = static_cast<float>(choice_);
        choice_x_.update(dt, ctx.reduced_motion() ? 60.0f : 18.0f);
        scroll_.target = scroll_target_;
        scroll_.update(dt, ctx.reduced_motion() ? 60.0f : 15.0f);

        if (stage_ == Stage::offer || stage_ == Stage::failed)
        {
            // The offer has What's new between its buttons when the release has notes.
            const bool notes = stage_ == Stage::offer && !offer_.notes.empty();
            const int count = notes ? 3 : 2;
            if (input.nav == Direction::left || input.nav == Direction::right)
            {
                const int next =
                    std::clamp(choice_ + (input.nav == Direction::right ? 1 : -1), 0, count - 1);
                if (next != choice_)
                {
                    choice_ = next;
                    ctx.cue(audio::Cue::focus);
                }
            }
            if (notes && (input.is_pressed(Action::north) ||
                          (input.is_pressed(Action::confirm) && choice_ == 1)))
            {
                open_notes(ctx);
            }
            else if (input.is_pressed(Action::confirm) && choice_ == 0)
            {
                begin(ctx);
            }
            else if (input.is_pressed(Action::confirm) || input.is_pressed(Action::back))
            {
                // Skipped: asked again the next time the app opens.
                update::finish();
                ctx.cue(audio::Cue::modal_close);
                return app::Transition::pop();
            }
            return app::Transition::stay();
        }

        if (stage_ == Stage::notes)
        {
            const float line = std::round(kBodySize * 1.6f);
            if (input.nav == Direction::up || input.nav == Direction::down)
                scroll_by(ctx, (input.nav == Direction::down ? 3.0f : -3.0f) * line);
            else if (input.is_pressed(Action::page_prev) || input.is_pressed(Action::page_next))
                scroll_by(ctx, (input.is_pressed(Action::page_next) ? 1.0f : -1.0f) *
                                   (kNotesWindow - 2.0f * line));
            else if (input.nav == Direction::left || input.nav == Direction::right)
            {
                const int next = input.nav == Direction::right ? 1 : 0;
                if (next != choice_)
                {
                    choice_ = next;
                    ctx.cue(audio::Cue::focus);
                }
            }
            else if (input.is_pressed(Action::confirm) && choice_ == 0)
            {
                begin(ctx);
            }
            else if (input.is_pressed(Action::confirm) || input.is_pressed(Action::back))
            {
                // Back on the offer, the highlight on What's new.
                stage_ = Stage::offer;
                time_ = 0.9f;
                choice_ = 1;
                choice_x_.snap(1.0f);
                ctx.cue(audio::Cue::back);
            }
            return app::Transition::stay();
        }

        if ((stage_ == Stage::working || stage_ == Stage::cancelling))
        {
            if (stage_ == Stage::working && input.is_pressed(Action::back))
            {
                update::cancel();
                stage_ = Stage::cancelling;
                time_ = 0.0f;
                ctx.cue(audio::Cue::back);
            }
            status_ = update::poll();
            if (status_.phase == update::Phase::cancelled)
            {
                update::finish();
                ctx.cue(audio::Cue::modal_close);
                return app::Transition::pop();
            }
            if (status_.phase == update::Phase::failed)
            {
                update::finish();
                stage_ = Stage::failed;
                choice_ = 0;
                choice_x_.snap(0.0f);
                time_ = 0.0f;
                ctx.cue(audio::Cue::error);
            }
            else if (status_.phase == update::Phase::ready)
            {
                if (update::apply())
                {
                    stage_ = Stage::closing;
                    time_ = 0.0f;
                    progress_.target = 1.0f;
                    ctx.cue(audio::Cue::saved);
                }
                else
                {
                    update::finish();
                    stage_ = Stage::failed;
                    status_.error = tr("The update helper did not answer");
                    ctx.cue(audio::Cue::error);
                }
            }
            if (status_.total != 0)
                progress_.target = static_cast<float>(static_cast<double>(status_.done) /
                                                      static_cast<double>(status_.total));
            else if (status_.phase == update::Phase::unpacking)
                progress_.target = 1.0f;
            // The speed, for the time left: sampled twice a second, smoothed.
            rate_wait_ += dt;
            if (status_.phase == update::Phase::downloading && rate_wait_ >= 0.5f)
            {
                const float now = status_.done >= rate_done_
                                      ? static_cast<float>(status_.done - rate_done_) / rate_wait_
                                      : 0.0f;
                rate_ = rate_ <= 0.0f ? now : rate_ * 0.75f + now * 0.25f;
                rate_done_ = status_.done;
                rate_wait_ = 0.0f;
            }
        }
        progress_.update(dt, ctx.reduced_motion() ? 60.0f : 9.0f);
        if (stage_ == Stage::closing && time_ >= 3.0f && ctx.quit)
            ctx.quit();
        return app::Transition::stay();
    }

    void draw(app::Context &ctx, app::Frame &frame) const override
    {
        frame.glass = true;
        gfx::DrawList &list = frame.overlay;
        const auto &theme = ctx.theme();
        const float open = tween::cubic_out(open_.value);
        list.rounded_rect({0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0.0f,
                          gfx::Color::rgb(0x050913, 0.72f * open));
        list.push_opacity(open);
        list.push_transform(0.96f + 0.04f * open, kCenterX, 540.0f, 0.0f,
                            28.0f * (1.0f - open) * (ctx.reduced_motion() ? 0.0f : 1.0f));
        ui::Painter paint(list, *ctx.fonts, theme, frame.glass_texture);
        if (stage_ == Stage::notes)
        {
            draw_notes(ctx, list, paint);
            list.pop_transform();
            list.pop_opacity();
            return;
        }
        paint.panel(kPanel);

        const bool failed = stage_ == Stage::failed;
        const gfx::Color accent = failed ? theme.warning : theme.accent;
        const float breathe = 0.5f + 0.5f * std::sin(ctx.time * 2.4f);
        paint.halo({kCenterX - kRadius, kRingY - kRadius, kRadius * 2.0f, kRadius * 2.0f}, kRadius,
                   52.0f, accent.with_alpha(0.08f + 0.07f * breathe));
        list.ring(kCenterX, kRingY, kRadius, 10.0f, theme.outline.with_alpha(0.24f));

        const auto center = [&](std::string_view text, float baseline, float size, gfx::Color color)
        { paint.body(text, kCenterX, baseline, size, color, gfx::Align::center); };

        if (stage_ == Stage::offer)
        {
            const float ring_fill = tween::cubic_out(time_ / 0.9f);
            arc(list, -kPi * 0.5f, 2.0f * kPi * ring_fill, accent);
            const float bob =
                std::sin(ctx.time * 2.2f) * 3.0f * (ctx.reduced_motion() ? 0.0f : 1.0f);
            const float ay = kRingY - 4.0f + bob;
            list.line(kCenterX, ay - 30.0f, kCenterX, ay + 16.0f, 6.0f, accent);
            list.line(kCenterX - 18.0f, ay - 2.0f, kCenterX, ay + 16.0f, 6.0f, accent);
            list.line(kCenterX + 18.0f, ay - 2.0f, kCenterX, ay + 16.0f, 6.0f, accent);
            paint.heading(tr("Update available"), kCenterX, 548.0f, 48.0f, theme.text,
                          gfx::Align::center);
            center(fill(tr("Version {0} is ready to install."), {offer_.version}), 602.0f, 25.0f,
                   theme.text_muted);
            if (offer_.size != 0)
                center(fill(tr("Download size: {0}"), {megabytes(offer_.size)}), 640.0f, 21.0f,
                       accent);
            center(tr("Your account, games and settings are kept."), 674.0f, 20.0f,
                   theme.text_muted);
            center(tr("ProsperoLichess closes to finish the update."), 702.0f, 20.0f,
                   theme.text_muted);
            if (offer_.notes.empty())
                buttons(paint, kButtonsTop, {TR("Update now"), TR("Skip")});
            else
                buttons(paint, kButtonsTop, {TR("Update now"), TR("What's new"), TR("Skip")});
        }
        else if (stage_ == Stage::working || stage_ == Stage::cancelling)
        {
            const bool measured = status_.total != 0;
            const float share = tween::clamp01(progress_.value);
            const bool downloading =
                measured && status_.phase == update::Phase::downloading && stage_ == Stage::working;
            if (measured)
                arc(list, -kPi * 0.5f, 2.0f * kPi * share, accent);
            else
                arc(list, time_ * 4.2f, kPi * (0.7f + 0.25f * std::sin(time_ * 2.1f)), accent);
            if (downloading)
                paint.heading(percent(std::min(static_cast<int>(share * 100.0f + 0.5f), 100)),
                              kCenterX, kRingY + 14.0f, 40.0f, theme.text, gfx::Align::center);
            const char *heading = stage_ == Stage::cancelling                   ? "Cancelling"
                                  : status_.phase == update::Phase::downloading ? "Downloading"
                                  : status_.phase == update::Phase::unpacking   ? "Unpacking"
                                                                                : "Preparing";
            paint.heading(tr(heading), kCenterX, 548.0f, 42.0f, theme.text, gfx::Align::center);
            center(fill(tr("Version {0}"), {offer_.version}), 590.0f, 22.0f, theme.text_muted);
            paint.progress({656.0f, 638.0f, 608.0f, 10.0f}, share);
            if (measured)
            {
                // Bytes, and the time left once the speed is known.
                std::string line = megabytes(status_.done) + "  /  " + megabytes(status_.total);
                if (downloading && rate_ > 1.0f && time_ > 1.5f && status_.total > status_.done)
                {
                    const float seconds = static_cast<float>(status_.total - status_.done) / rate_;
                    const int whole = std::max(1, static_cast<int>(std::ceil(seconds)));
                    line += "  \xC2\xB7  ";
                    line += whole < 90 ? fill(tr("About {0} s left"), {std::to_string(whole)})
                                       : fill(tr("About {0} min left"),
                                              {std::to_string((whole + 59) / 60)});
                }
                center(line, 688.0f, 20.0f, accent);
            }
        }
        else if (stage_ == Stage::closing)
        {
            arc(list, -kPi * 0.5f, 2.0f * kPi, accent);
            const float stroke = tween::cubic_out((time_ - 0.15f) / 0.45f);
            const float first = tween::clamp01(stroke / 0.4f);
            const float second = tween::clamp01((stroke - 0.4f) / 0.6f);
            list.line(kCenterX - 34.0f, kRingY + 2.0f, kCenterX - 34.0f + 24.0f * first,
                      kRingY + 2.0f + 24.0f * first, 9.0f, accent);
            if (second > 0.0f)
                list.line(kCenterX - 10.0f, kRingY + 26.0f, kCenterX - 10.0f + 48.0f * second,
                          kRingY + 26.0f - 52.0f * second, 9.0f, accent);
            paint.heading(tr("Update ready"), kCenterX, 548.0f, 48.0f, theme.text,
                          gfx::Align::center);
            center(tr("ProsperoLichess closes now."), 602.0f, 25.0f, theme.text_muted);
            center(fill(tr("Open it again to use version {0}."), {offer_.version}), 640.0f, 20.0f,
                   theme.text_muted);
            paint.progress({656.0f, 690.0f, 608.0f, 6.0f}, 1.0f - tween::clamp01(time_ / 3.0f));
        }
        else
        {
            arc(list, -kPi * 0.5f, 2.0f * kPi * tween::cubic_out(time_ / 0.6f), accent);
            list.line(kCenterX, kRingY - 38.0f, kCenterX, kRingY + 12.0f, 10.0f, accent);
            list.circle(kCenterX, kRingY + 38.0f, 7.0f, accent);
            paint.heading(tr("The update could not finish"), kCenterX, 548.0f, 38.0f, theme.text,
                          gfx::Align::center);
            center(tr("ProsperoLichess was not changed."), 602.0f, 24.0f, theme.text_muted);
            if (!status_.error.empty())
                center(status_.error, 642.0f, 19.0f, theme.text_muted);
            buttons(paint, kButtonsTop, {TR("Try again"), TR("Close")});
        }
        list.pop_transform();
        list.pop_opacity();
    }

    app::look::Mood mood() const override
    {
        return app::look::mood(app::look::Section::settings);
    }
    bool shows_status_bar() const override
    {
        return false;
    }
    const char *name() const override
    {
        return "update";
    }

  private:
    void begin(app::Context &ctx)
    {
        status_ = {};
        time_ = 0.0f;
        progress_.snap(0.0f);
        rate_ = 0.0f;
        rate_done_ = 0;
        rate_wait_ = 0.0f;
        choice_ = 0;
        choice_x_.snap(0.0f);
        if (update::begin())
        {
            stage_ = Stage::working;
            ctx.cue(audio::Cue::select);
        }
        else
        {
            stage_ = Stage::failed;
            status_.error = tr("The update helper could not start");
            ctx.cue(audio::Cue::error);
        }
    }

    void open_notes(app::Context &ctx)
    {
        stage_ = Stage::notes;
        time_ = 0.0f;
        scroll_target_ = 0.0f;
        scroll_.snap(0.0f);
        choice_ = 0;
        choice_x_.snap(0.0f);
        ctx.cue(audio::Cue::modal_open);
        // Laid out now, so that scrolling knows the text's height before the
        // view has been drawn (a Painter needs a list, even to measure).
        gfx::DrawList scratch;
        const ui::Painter paint(scratch, *ctx.fonts, ctx.theme());
        layout_notes(paint);
    }

    void scroll_by(app::Context &ctx, float by)
    {
        const float most = std::max(0.0f, notes_height_ - kNotesWindow);
        const float target = std::clamp(scroll_target_ + by, 0.0f, most);
        if (target == scroll_target_)
            return;
        scroll_target_ = target;
        ctx.cue(audio::Cue::focus);
    }

    void buttons(ui::Painter &paint, float top, std::initializer_list<const char *> labels) const
    {
        const int count = static_cast<int>(labels.size());
        int index = 0;
        for (const char *label : labels)
        {
            ui::Look look;
            look.focus =
                1.0f - std::min(1.0f, std::abs(choice_x_.value - static_cast<float>(index)));
            paint.button(button_rect(count, static_cast<float>(index), top), tr(label),
                         index == 0 ? ui::ButtonKind::primary : ui::ButtonKind::secondary, look);
            ++index;
        }
    }

    // The notes as lines: laid out once, the first time they are drawn. The
    // catalog gives plain text: list items start with "- ", GitHub's boxes
    // arrive as "Warning: ..." or "Note: ...", a short line without closing
    // punctuation is a heading.
    void layout_notes(const ui::Painter &paint) const
    {
        if (laid_out_)
            return;
        laid_out_ = true;
        float y = 0.0f;
        bool gap_before = false;
        const std::string &all = offer_.notes;
        std::size_t at = 0;
        while (at <= all.size())
        {
            const std::size_t end = std::min(all.find('\n', at), all.size());
            std::string_view line = std::string_view{all}.substr(at, end - at);
            at = end + 1;
            while (!line.empty() && (line.back() == ' ' || line.back() == '\r'))
                line.remove_suffix(1);
            if (line.empty())
            {
                gap_before = !lines_.empty();
                if (end >= all.size())
                    break;
                continue;
            }
            const bool bullet = starts_with(line, "- ");
            if (bullet)
                line.remove_prefix(2);
            const bool warning =
                !bullet && (starts_with(line, "Warning:") || starts_with(line, "Caution:") ||
                            starts_with(line, "Important:"));
            const bool note = !bullet && (starts_with(line, "Note:") || starts_with(line, "Tip:"));
            const char last = line.back();
            const bool heading = !bullet && !warning && !note && line.size() <= 48 && last != '.' &&
                                 last != ':' && last != '!' && last != '?' && last != ',' &&
                                 last != ';' && last != ')';
            const float size = heading ? kHeadingSize : kBodySize;
            const float pitch = std::round(size * (heading ? 1.45f : 1.6f));
            const bool boxed = warning || note;
            const float indent = bullet ? 30.0f : boxed ? 26.0f : 0.0f;
            const float width = kNotesWidth - indent - (boxed ? 22.0f : 0.0f);
            if (!lines_.empty())
                y += heading ? 22.0f : boxed ? 18.0f : gap_before ? 14.0f : bullet ? 4.0f : 8.0f;
            gap_before = false;
            const float block_top = y;
            if (boxed)
                y += 14.0f;
            const std::vector<std::string> pieces = gfx::wrap_lines(
                line, width,
                [&](std::string_view text) {
                    return heading ? paint.heading_width(text, size) : paint.body_width(text, size);
                });
            for (std::size_t i = 0; i < pieces.size(); ++i)
            {
                NoteLine out;
                out.text = pieces[i];
                out.y = y;
                out.height = pitch;
                out.indent = indent;
                out.heading = heading;
                out.bullet = bullet && i == 0;
                lines_.push_back(std::move(out));
                y += pitch;
            }
            if (boxed)
            {
                y += 14.0f;
                boxes_.push_back({block_top, y, warning});
            }
            if (end >= all.size())
                break;
        }
        if (offer_.notes_truncated)
        {
            y += 20.0f;
            NoteLine out;
            out.text = tr("The rest is on the app's page on homebrew.page.");
            out.y = y;
            out.height = std::round(kBodySize * 1.6f);
            out.quiet = true;
            lines_.push_back(out);
            y += out.height;
        }
        notes_height_ = y;
    }

    void draw_notes(app::Context &ctx, gfx::DrawList &list, ui::Painter &paint) const
    {
        const auto &theme = ctx.theme();
        layout_notes(paint);
        paint.panel(kNotesPanel);
        const float motion = ctx.reduced_motion() ? 0.0f : 1.0f;
        const float arrive = tween::cubic_out(time_ / 0.35f);

        // The title, with a small mark: a page with lines on it.
        list.push_opacity(arrive);
        const float mark_x = kNotesLeft + 22.0f;
        const float mark_y = kNotesPanel.y + 66.0f;
        list.circle(mark_x, mark_y, 24.0f, theme.accent.with_alpha(0.16f));
        list.ring(mark_x, mark_y, 24.0f, 2.0f, theme.accent.with_alpha(0.55f));
        for (int i = 0; i < 3; ++i)
        {
            const float ly = mark_y - 8.0f + 8.0f * static_cast<float>(i);
            list.line(mark_x - 9.0f, ly, mark_x + (i == 2 ? 3.0f : 9.0f), ly, 2.6f, theme.accent);
        }
        const std::string title = fill(tr("What's new in version {0}"), {offer_.version});
        const float room = kNotesPanel.w - 128.0f - 64.0f;
        const float title_size =
            36.0f * std::min(1.0f, room / std::max(paint.heading_width(title, 36.0f), 1.0f));
        paint.heading(title, kNotesLeft + 64.0f, kNotesPanel.y + 79.0f, title_size, theme.text);
        list.rounded_rect({kNotesLeft, kNotesPanel.y + 112.0f, kNotesPanel.w - 128.0f, 1.0f}, 0.0f,
                          theme.outline.with_alpha(0.5f));
        list.pop_opacity();

        // The text, in its window, moved by the scroll.
        const gfx::Rect area{kNotesLeft - 6.0f, kNotesTop, kNotesWidth + 12.0f, kNotesWindow};
        const float scroll = scroll_.value;
        list.push_clip(area);
        for (const NoteBox &box : boxes_)
        {
            const float top = kNotesTop + box.top - scroll;
            const float bottom = kNotesTop + box.bottom - scroll;
            if (bottom < area.y || top > area.y + area.h)
                continue;
            const gfx::Color accent = box.warning ? theme.warning : theme.accent;
            list.push_opacity(arrive);
            list.rounded_rect({kNotesLeft, top, kNotesWidth, bottom - top}, 14.0f,
                              accent.with_alpha(0.09f));
            list.rounded_rect({kNotesLeft, top + 10.0f, 4.0f, bottom - top - 20.0f}, 2.0f,
                              accent.with_alpha(0.85f));
            list.pop_opacity();
        }
        int shown = 0;
        for (const NoteLine &line : lines_)
        {
            const float top = kNotesTop + line.y - scroll;
            if (top + line.height < area.y || top > area.y + area.h)
                continue;
            // The lines first shown come in one after another.
            const float delay = 0.10f + 0.03f * static_cast<float>(std::min(shown++, 14));
            const float in = time_ > 1.2f ? 1.0f : tween::cubic_out((time_ - delay) / 0.32f);
            if (in <= 0.0f)
                continue;
            list.push_opacity(in);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, (1.0f - in) * 14.0f * motion);
            if (line.bullet)
                list.circle(kNotesLeft + 11.0f, top + line.height * 0.5f, 3.6f,
                            theme.accent.with_alpha(0.9f));
            const float size = line.heading ? kHeadingSize : kBodySize;
            const float baseline = top + line.height * 0.5f + size * 0.35f;
            if (line.heading)
                paint.heading(line.text, kNotesLeft, baseline, size, theme.text);
            else
                paint.body(line.text, kNotesLeft + line.indent, baseline, size,
                           line.quiet ? theme.text_muted.with_alpha(0.7f) : theme.text_muted);
            list.pop_transform();
            list.pop_opacity();
        }
        list.pop_clip();

        // The scrollbar: a track and a thumb that follows the scroll.
        const float most = std::max(0.0f, notes_height_ - kNotesWindow);
        if (most > 0.0f)
        {
            const float track_x = kNotesLeft + kNotesWidth + 14.0f;
            const float thumb = std::max(48.0f, kNotesWindow * kNotesWindow / notes_height_);
            const float place = tween::clamp01(scroll / most);
            list.push_opacity(arrive);
            list.rounded_rect({track_x, area.y, 4.0f, kNotesWindow}, 2.0f,
                              theme.outline.with_alpha(0.22f));
            list.rounded_rect(
                {track_x - 1.0f, area.y + (kNotesWindow - thumb) * place, 6.0f, thumb}, 3.0f,
                theme.accent.with_alpha(0.85f));
            list.pop_opacity();
        }
        list.push_opacity(arrive);
        buttons(paint, kNotesButtonsTop, {TR("Update now"), TR("Back")});
        list.pop_opacity();
    }

    update::Offer offer_;
    Stage stage_ = Stage::offer;
    update::Progress status_;
    int choice_ = 0;
    float time_ = 0.0f;
    float rate_ = 0.0f; // bytes per second, smoothed
    std::uint64_t rate_done_ = 0;
    float rate_wait_ = 0.0f;
    float scroll_target_ = 0.0f;
    tween::Spring open_;
    tween::Spring choice_x_;
    tween::Spring progress_;
    tween::Spring scroll_;
    // Laid out when first drawn (drawing measures the text).
    mutable bool laid_out_ = false;
    mutable std::vector<NoteLine> lines_;
    mutable std::vector<NoteBox> boxes_;
    mutable float notes_height_ = 0.0f;
};
} // namespace

std::unique_ptr<app::Scene> make_update(update::Offer offer)
{
    return std::make_unique<UpdateScene>(std::move(offer));
}

} // namespace pch::modes
