// ProsperoLichess - Settings page: board, play, sound, controller, display, about.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A control room in the house look (app/look.hpp):
//
//   - the categories are a lit menu like the rail: a sign on a tile each, a
//     plate that glides to the one that is showing, a ring while they have
//     the controller;
//   - the chosen category opens with its sign, its name in display type and
//     a line about it; changing category cross-fades the whole pane;
//   - beside the form, every category shows what its settings do: the board
//     itself, volume meters, the two buttons trading places, the picture
//     sizes, a dot that glides or stands still.

#include "board/board_view.hpp"
#include "core/strings.hpp"
#include "modes/page.hpp"
#include "ui/components/form.hpp"
#include "ui/components/list.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"
#include "ui/components/text_view.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace pch::modes
{

namespace
{

namespace look = app::look;
using gfx::Align;
using gfx::Color;
using gfx::Rect;

// The categories on the left, in order. Every one but About is a form.
enum Category : int
{
    kBoard,
    kPlay,
    kSound,
    kController,
    kDisplay,
    kAbout,
    kCategoryCount,
};
constexpr int kFormCount = kAbout;
constexpr const char *kCategoryLines[kCategoryCount] = {
    TR("How the board and the pieces look, here and in every game."),
    TR("How moves are made and what is asked before they count."),
    TR("Music, game sounds and the sounds of the menus."),
    TR("Which button confirms, and whether the controller rumbles."),
    TR("The picture's size, the frame counter and how much moves."),
    TR("What this app is, where its games come from and whose work it uses."),
};

// What a category is called. "Play" here is how moves are made, not the page
// that starts a game: it has a context, so a language may use another word.
const char *category_name(int category)
{
    switch (category)
    {
    case kBoard:
        return tr("Board");
    case kPlay:
        return trc("settings", "Play");
    case kSound:
        return tr("Sound");
    case kController:
        return tr("Controller");
    case kDisplay:
        return tr("Display");
    default:
        return tr("About");
    }
}

// What a piece set is called. Two have names ("Merida", "Chessnut"), which
// stay; the first one's is a word, and a board is called the same: the context
// keeps the two apart. board/pieces.hpp holds the names; the word is marked here.
const char *piece_set_name(int index)
{
    static constexpr std::string_view kWord = TRC("piece set", "Classic");
    const char *label = board::kPieceSets[std::clamp(index, 0, board::kPieceSetCount - 1)].label;
    return label == kWord ? trc("piece set", kWord.data()) : label;
}

// look::kicker inside a width: a translated label shrinks, then ends in an
// ellipsis, instead of running into what stands beside it.
float kicker_fit(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
                 float baseline, float max_width, Color color, Align align = Align::left,
                 float size = 16.0f)
{
    return ui::text_fit(list, fonts.semibold, ui::upper(text), x, baseline, size, max_width, color,
                        align, 3.0f);
}

// One id per setting, whichever form it is on.
enum Row : int
{
    kRowBoard = 1,
    kRowPieces,
    kRowCoordinates,
    kRowDests,
    kRowAutoQueen,
    kRowPremoves,
    kRowConfirmResign,
    kRowMusic,
    kRowEffects,
    kRowInterface,
    kRowSwapConfirm,
    kRowVibration,
    kRowResolution,
    kRowReducedMotion,
    kRowShowFps,
};

constexpr Rect kPage{app::kContent, app::kTop, app::kRight - app::kContent,
                     app::kBottom - app::kTop};
// The categories take one column of the home page's four; the pane the rest.
constexpr float kMenuWidth = (kPage.w - 3.0f * app::kGap) / 4.0f;
constexpr Rect kMenu{kPage.x, kPage.y, kMenuWidth, kPage.h};
constexpr Rect kPane{kPage.x + kMenuWidth + app::kGap, kPage.y, kPage.w - kMenuWidth - app::kGap,
                     kPage.h};
constexpr float kMenuInset = 14.0f;
constexpr float kRowHeight = 68.0f;
constexpr float kRowGap = 8.0f;
constexpr float kRowRadius = 18.0f;
constexpr float kTile = 44.0f;
constexpr float kPanePad = 32.0f;
constexpr float kHeader = 124.0f;  // the category's sign, name and line
constexpr float kPreview = 400.0f; // the column that shows what the settings do
constexpr float kPreviewBoard = 352.0f;
constexpr float kPreviewGap = 36.0f;

// A quiet opening everyone knows, with Black to move: the bishop has just
// come out (the last move), and the knight on g8 is picked up so the preview
// shows the legal-move dots too.
constexpr const char *kPreviewFen =
    "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3";

class SettingsPage final : public Page
{
  public:
    explicit SettingsPage(app::Context &ctx)
    {
        // The list keeps the focus and the sounds; the page draws the menu.
        categories_.style.entrance_step = 0.0f;
        std::vector<ui::ListItem> items;
        for (int i = 0; i < kCategoryCount; ++i)
        {
            ui::ListItem item;
            item.title = category_name(i);
            items.push_back(std::move(item));
        }
        categories_.set_items(std::move(items));
        categories_.set_bounds(kMenu);
        categories_.set_focus(kBoard);

        detail_ = {kPane.x + kPanePad, kPane.y + kPanePad + kHeader, kPane.w - 2.0f * kPanePad,
                   kPane.h - 2.0f * kPanePad - kHeader};
        build_forms(*ctx.settings, *ctx.fonts);
        version_ = ctx.version;
        build_about(ctx);

        const Color accent = look::accent(look::Section::settings);
        for (ui::Form &form : forms_)
            look::tint(accent, form);
        look::tint(accent, about_);
        chess::Position position;
        if (!chess::Position::from_fen(kPreviewFen, &position))
            position = chess::Position::start();
        preview_.snap(position);
        shown_.snap(1.0f);
        plate_.snap(row(kBoard));
        ring_.snap(row(kBoard));
        sync_visuals(*ctx.settings, true);
    }

    void enter(app::Context &ctx) override
    {
        // The page always shows what is saved, whoever changed it.
        sync(*ctx.settings);
        since_ = 0.0f;
    }

    PageResult update(app::Context &ctx, const InputFrame &input, float dt, bool focused) override
    {
        ctx.calm(categories_, about_);
        calm_ = ctx.reduced_motion();
        since_ += dt;
        clock_ += dt;
        for (ui::Form &form : forms_)
            ctx.calm(form);
        // The version is given to the app after its screens are made.
        if (version_ != ctx.version)
        {
            version_ = ctx.version;
            build_about(ctx);
        }

        PageResult result;
        if (!focused)
            zone_ = Zone::categories; // coming back from the rail starts at the categories
        else if (zone_ == Zone::categories)
            handle_categories(ctx, input, &result);
        else
            handle_detail(ctx, input);

        const bool in_detail = focused && zone_ == Zone::detail;
        categories_.set_active(focused && zone_ == Zone::categories);
        for (int i = 0; i < kFormCount; ++i)
            forms_[static_cast<std::size_t>(i)].set_active(in_detail && category_ == i);
        about_.set_active(in_detail && category_ == kAbout);

        const float omega = calm_ ? 60.0f : 14.0f;
        shown_.target = 1.0f;
        shown_.update(dt, omega);
        plate_.target(row(category_));
        plate_.update(dt, calm_ ? 60.0f : 16.0f);
        ring_.target(row(categories_.focus()));
        ring_.update(dt, calm_ ? 60.0f : 18.0f);
        menu_focus_.target = focused && zone_ == Zone::categories ? 1.0f : 0.0f;
        menu_focus_.update(dt, calm_ ? 60.0f : 18.0f);
        pane_lit_.target = in_detail ? 1.0f : 0.0f;
        pane_lit_.update(dt, omega);
        sync_visuals(*ctx.settings, false);
        for (tween::Spring &level : levels_)
            level.update(dt, calm_ ? 60.0f : 12.0f);
        swap_.update(dt, omega);
        rumble_.update(dt, omega);
        size_.update(dt, omega);
        counter_.update(dt, omega);
        still_.update(dt, omega);
        categories_.update(dt);
        for (ui::Form &form : forms_)
            form.update(dt);
        about_.update(dt);
        preview_.update(dt, preview_overlay(ctx));
        return result;
    }

    void draw(app::Context &ctx, app::Frame &frame, bool) const override
    {
        gfx::DrawList &list = frame.scene;
        const ui::Fonts &fonts = *ctx.fonts;
        ui::Canvas canvas = app::canvas_for(ctx, list);
        const Color accent = look::accent(look::Section::settings);

        draw_menu(ctx, list, accent);

        // ---- the pane: lit while the controller is in it
        const float in = arrive(1);
        list.push_opacity(in);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in));
        look::lift(list, kPane, pane_lit_.value * 0.6f, accent, ctx.time, calm_);
        look::panel(list, kPane, pane_lit_.value * 0.7f, accent);

        // A category that was just chosen fades in from a few pixels lower.
        const float t = tween::clamp01(shown_.value);
        list.push_opacity(t);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, calm_ ? 0.0f : 14.0f * (1.0f - t));
        {
            // Its sign, its name and a line about it.
            const Rect tile{kPane.x + kPanePad, kPane.y + kPanePad, 72.0f, 72.0f};
            list.glow(tile, 20.0f, 14.0f, accent.with_alpha(0.3f));
            list.gradient_rect(tile, 20.0f, gfx::mix(accent, look::kInk, 0.2f),
                               gfx::mix(accent, look::kNight, 0.45f));
            draw_sign(list, category_, {tile.cx() - 20.0f, tile.cy() - 20.0f, 40.0f, 40.0f},
                      look::kNight);
            const float x = tile.x + tile.w + 24.0f;
            const float room = kPane.x + kPane.w - kPanePad - x;
            ui::text_fit(list, fonts.display, category_name(category_), x - 2.0f, tile.y + 38.0f,
                         44.0f, room, look::kInk);
            ui::text_fit(list, fonts.regular, tr(kCategoryLines[category_]), x, tile.y + 68.0f,
                         22.0f, room, look::kInk.with_alpha(look::kMuted));
            look::rule(list, kPane.x + kPanePad, kPane.y + kPanePad + kHeader - 26.0f,
                       kPane.w - 2.0f * kPanePad);
        }
        if (category_ == kAbout)
        {
            about_.draw(canvas);
        }
        else
        {
            forms_[static_cast<std::size_t>(category_)].draw(canvas);
            draw_preview(ctx, canvas);
        }
        list.pop_transform();
        list.pop_opacity();

        list.pop_transform();
        list.pop_opacity();
    }

    std::span<const ui::Hint> hints() const override
    {
        static constexpr ui::Hint kCategories[] = {{ui::Button::cross, TR("Open")},
                                                   {ui::Button::dpad, TR("Choose")},
                                                   {ui::Button::options, TR("Menu")}};
        static constexpr ui::Hint kForm[] = {{ui::Button::dpad, TR("Change")},
                                             {ui::Button::circle, TR("Back")}};
        static constexpr ui::Hint kReading[] = {{ui::Button::dpad, TR("Scroll")},
                                                {ui::Button::circle, TR("Back")}};
        if (zone_ == Zone::categories)
            return kCategories;
        if (category_ == kAbout)
            return kReading;
        return kForm;
    }

    app::look::Section section() const override
    {
        return app::look::Section::settings;
    }

    const char *title() const override
    {
        return tr("Settings");
    }

    const char *name() const override
    {
        return "settings";
    }

  private:
    enum class Zone
    {
        categories,
        detail,
    };

    // ---- building ------------------------------------------------------------

    void build_forms(const Settings &s, const ui::Fonts &fonts)
    {
        const Rect area{detail_.x, detail_.y, detail_.w - kPreview - kPreviewGap, detail_.h};
        for (ui::Form &form : forms_)
        {
            form.style.panel = false;
            form.style.on_page = false; // the rows lie on the pane's panel
            form.style.entrance_step = 0.0f;
            form.style.control_width = 280.0f;
            form.style.label_size = 26.0f;
            form.style.label_ratio = 0.5f;
            form.set_bounds(area);
        }

        ui::Form &board = forms_[kBoard];
        board.add_header(tr("Appearance"));
        std::vector<std::string> finishes;
        for (const board::BoardTheme &theme : board::kBoardThemes)
            finishes.emplace_back(tr(theme.label));
        board.add_choice(kRowBoard, tr("Board"), std::move(finishes), s.board_theme).description =
            tr("Wood, marble or flat colours.");
        std::vector<std::string> sets;
        for (int i = 0; i < board::kPieceSetCount; ++i)
            sets.emplace_back(piece_set_name(i));
        board.add_choice(kRowPieces, tr("Pieces"), std::move(sets), s.piece_set).description =
            tr("The piece set used everywhere.");
        board.add_header(tr("On the board"));
        board.add_toggle(kRowCoordinates, tr("Coordinates"), s.coordinates).description =
            tr("Files and ranks along the board's edge.");
        board.add_toggle(kRowDests, tr("Legal moves"), s.show_dests).description =
            tr("Dots on the squares the selected piece can reach.");

        ui::Form &play = forms_[kPlay];
        play.add_header(tr("Moves"));
        play.add_toggle(kRowAutoQueen, tr("Always promote to queen"), s.auto_queen).description =
            tr("Off shows a picker for every promotion.");
        play.add_toggle(kRowPremoves, tr("Premoves"), s.premoves).description =
            tr("Queue your next move during the opponent's turn.");
        play.add_header(tr("Online games"));
        play.add_toggle(kRowConfirmResign, tr("Confirm resignation"), s.confirm_resign)
            .description = tr("Ask before resigning an online game.");

        ui::Form &sound = forms_[kSound];
        sound.add_header(tr("Volume"));
        sound
            .add_slider(kRowMusic, tr("Music"), static_cast<float>(s.music_volume), 0.0f, 10.0f,
                        1.0f)
            .description = tr("Background music.");
        sound
            .add_slider(kRowEffects, tr("Game sounds"), static_cast<float>(s.sfx_volume), 0.0f,
                        10.0f, 1.0f)
            .description = tr("Moves, captures, checks and results.");
        sound
            .add_slider(kRowInterface, tr("Interface sounds"), static_cast<float>(s.ui_volume),
                        0.0f, 10.0f, 1.0f)
            .description = tr("Menu navigation and selection.");

        ui::Form &controller = forms_[kController];
        controller.add_header(tr("Buttons"));
        controller.add_toggle(kRowSwapConfirm, tr("Swap Cross and Circle"), s.swap_confirm)
            .description = tr("Circle confirms and Cross goes back.");
        controller.add_header(tr("Feedback"));
        controller.add_toggle(kRowVibration, tr("Vibration"), s.vibration).description =
            tr("Rumble on captures, checks and results.");

        ui::Form &display = forms_[kDisplay];
        display.add_header(tr("Picture"));
        // The sizes are called the same in every language: "1080p", "4K".
        std::vector<std::string> sizes;
        for (const Settings::Resolution &resolution : Settings::kResolutions)
            sizes.emplace_back(resolution.label);
        display.add_choice(kRowResolution, tr("Resolution"), std::move(sizes), s.resolution)
            .description = tr("The PS5 scales the picture to your TV.");
        display.add_toggle(kRowShowFps, tr("Show FPS"), s.show_fps).description =
            tr("Frames per second in the top-right corner.");
        display.add_header(tr("Motion"));
        display.add_toggle(kRowReducedMotion, tr("Reduced motion"), s.reduced_motion).description =
            tr("Replaces slides and bounces with quick fades.");

        static constexpr int kFirst[kFormCount] = {kRowBoard, kRowAutoQueen, kRowMusic,
                                                   kRowSwapConfirm, kRowResolution};
        for (int i = 0; i < kFormCount; ++i)
        {
            fit_form(forms_[static_cast<std::size_t>(i)], fonts);
            forms_[static_cast<std::size_t>(i)].focus_row(kFirst[i]);
        }
    }

    // A form sets a row's label and its description on one line each, and ends
    // what is too long in dots. Where a language's words are longer the form's
    // type is set smaller, all rows alike, down to what still reads, so its
    // lines stay whole. The rooms are the ones ui::Form gives a row.
    static void fit_form(ui::Form &form, const ui::Fonts &fonts)
    {
        gfx::DrawList scratch; // a Painter needs a list, even to measure
        const ui::Painter paint(scratch, fonts, form.style.theme);
        const ui::FormStyle &style = form.style;
        // The rows stand inside the form's panel, when it has one.
        const float inner = form.bounds().w - (style.panel ? 2.0f * style.panel_padding : 0.0f);
        const float line = inner - 2.0f * style.padding;
        const float state = std::max(paint.label_width(tr(style.on_text), style.value_size),
                                     paint.label_width(tr(style.off_text), style.value_size));
        float label = 1.0f;
        float description = 1.0f;
        for (int i = 0; i < form.row_count(); ++i)
        {
            const ui::FormRow &row = form.row_at(i);
            if (row.kind == ui::FormRowKind::header)
                continue;
            // What stands at the right of the row's line: a switch and its
            // word, or a control.
            const float taken = row.kind == ui::FormRowKind::toggle
                                    ? style.toggle_width + 16.0f + state
                                    : style.control_width;
            const float room = line - taken - 18.0f - style.focus_shift;
            label = std::min(label,
                             room / std::max(paint.label_width(row.label, style.label_size), 1.0f));
            description = std::min(
                description,
                line / std::max(paint.body_width(row.description, style.description_size), 1.0f));
        }
        // A hair under the room: the form measures the line again at the new size.
        if (label < 1.0f)
            form.style.label_size *= std::max(label * 0.99f, 0.72f);
        if (description < 1.0f)
            form.style.description_size *= std::max(description * 0.99f, 0.78f);
    }

    // What the old About screen said, as an article: what the app is, where
    // its services come from, and whose work it is built on.
    void build_about(const app::Context &ctx)
    {
        using ui::TextBlock;
        std::vector<TextBlock> blocks;
        // Names stay as they are (the app, people, projects, licences); what
        // is said about them is text, with the name as its {0}.
        blocks.push_back(TextBlock::heading("ProsperoLichess"));
        blocks.push_back(TextBlock::paragraph(tr("An unofficial PS5 homebrew Lichess client.")));
        blocks.push_back(TextBlock::key_value(tr("Brought to you by"), "BlackBearReloaded"));
        blocks.push_back(TextBlock::key_value(
            tr("Version"), ctx.version.empty() ? std::string(tr("Unknown")) : ctx.version));
        blocks.push_back(TextBlock::key_value(tr("Licence"), "GPL-3.0-or-later"));
        blocks.push_back(
            TextBlock::key_value(tr("Source"), "github.com/blackbearreloaded/ProsperoLichess"));

        blocks.push_back(TextBlock::heading(tr("Powered by Lichess"), 2));
        blocks.push_back(TextBlock::paragraph(
            tr("Online games, puzzles and TV come from Lichess, the free and open-source chess "
               "server at lichess.org. ProsperoLichess uses its public API and is not affiliated "
               "with or endorsed by Lichess.")));

        blocks.push_back(TextBlock::heading(tr("Also thanks to"), 2));
        blocks.push_back(TextBlock::key_value(tr("Puzzles"), tr("Lichess puzzle database (CC0)")));
        blocks.push_back(
            TextBlock::key_value(tr("Pieces"), fill(tr("{0} and {1} (GPLv2+), {2} (Apache-2.0)"),
                                                    {"cburnett", "merida", "chessnut"})));
        blocks.push_back(TextBlock::key_value(
            "ps5-opengl", fill(tr("{0} for PS5, built on {1}"), {"OpenGL 4.6", "Mesa"})));
        blocks.push_back(TextBlock::key_value("ps5-homebrew-ui",
                                              tr("The interface kit this app is drawn with")));
        blocks.push_back(TextBlock::key_value(
            "Inter", fill(tr("Typeface by {0} (SIL OFL 1.1)"), {"Rasmus Andersson"})));
        blocks.push_back(
            TextBlock::key_value("Montserrat", fill(tr("Typeface by {0} (SIL OFL 1.1)"),
                                                    {"The Montserrat Project Authors"})));
        blocks.push_back(TextBlock::key_value(
            "DejaVu Sans Mono",
            fill(tr("Typeface by {0} and {1} (Bitstream Vera)"), {"Bitstream", "DejaVu"})));
        blocks.push_back(TextBlock::key_value("stb_vorbis",
                                              fill(tr("Music decoding by {0}"), {"Sean Barrett"})));
        blocks.push_back(
            TextBlock::key_value("yyjson", fill(tr("JSON parsing by {0} (MIT)"), {"YaoYuan"})));
        blocks.push_back(TextBlock::key_value(
            "libcurl", fill(tr("HTTPS by {0} and contributors (curl)"), {"Daniel Stenberg"})));
        blocks.push_back(TextBlock::key_value(
            "OpenSSL", fill(tr("TLS by the {0} (Apache-2.0)"), {"OpenSSL Project"})));
        blocks.push_back(TextBlock::key_value(
            "zlib, zstd, libpsl",
            fill(tr("Compression and domain rules for {0} (zlib, BSD, MIT)"), {"libcurl"})));
        blocks.push_back(TextBlock::key_value(
            tr("QR codes"), fill(tr("QR Code generator by {0} (MIT)"), {"Project Nayuki"})));
        blocks.push_back(TextBlock::key_value(tr("Sound and music"),
                                              fill(tr("Created with {0}"), {"ElevenLabs"})));
        blocks.push_back(TextBlock::paragraph(
            fill(tr("The full notices and licence texts ship with the source, in {0}."),
                 {"THIRD_PARTY_NOTICES.md"})));
        about_.set_content(std::move(blocks));

        about_.style.panel = false; // it lies on the pane's panel
        about_.style.footer = false;
        about_.style.padding = 12.0f;
        about_.style.body_size = 22.0f;
        about_.style.heading_size = 34.0f;
        about_.style.subheading_size = 26.0f;
        about_.style.block_gap = 16.0f;
        about_.style.heading_gap = 26.0f;
        about_.style.item_gap = 8.0f;
        about_.set_bounds(detail_);
    }

    // The forms show what the settings say (silent: nothing animates or sounds).
    void sync(const Settings &s)
    {
        forms_[kBoard].set_choice(kRowBoard, s.board_theme);
        forms_[kBoard].set_choice(kRowPieces, s.piece_set);
        forms_[kBoard].set_toggle(kRowCoordinates, s.coordinates);
        forms_[kBoard].set_toggle(kRowDests, s.show_dests);
        forms_[kPlay].set_toggle(kRowAutoQueen, s.auto_queen);
        forms_[kPlay].set_toggle(kRowPremoves, s.premoves);
        forms_[kPlay].set_toggle(kRowConfirmResign, s.confirm_resign);
        forms_[kSound].set_slider(kRowMusic, static_cast<float>(s.music_volume));
        forms_[kSound].set_slider(kRowEffects, static_cast<float>(s.sfx_volume));
        forms_[kSound].set_slider(kRowInterface, static_cast<float>(s.ui_volume));
        forms_[kController].set_toggle(kRowSwapConfirm, s.swap_confirm);
        forms_[kController].set_toggle(kRowVibration, s.vibration);
        forms_[kDisplay].set_choice(kRowResolution, s.resolution);
        forms_[kDisplay].set_toggle(kRowShowFps, s.show_fps);
        forms_[kDisplay].set_toggle(kRowReducedMotion, s.reduced_motion);
    }

    // ---- input ---------------------------------------------------------------

    void handle_categories(app::Context &ctx, const InputFrame &input, PageResult *result)
    {
        if (input.nav == Direction::left)
        {
            result->to_rail = true;
            return;
        }
        if (input.nav == Direction::right)
        {
            ctx.cue(audio::Cue::select);
            zone_ = Zone::detail;
            return;
        }
        switch (categories_.handle(input, *ctx.feedback))
        {
        case ui::Event::moved:
            show(categories_.focus());
            break;
        case ui::Event::activated:
            zone_ = Zone::detail;
            break;
        case ui::Event::cancelled:
            result->to_rail = true;
            break;
        default:
            break;
        }
    }

    void handle_detail(app::Context &ctx, const InputFrame &input)
    {
        if (category_ == kAbout)
        {
            // Left is never the article's: it leads back to the categories.
            if (about_.handle(input, *ctx.feedback) == ui::Event::cancelled)
            {
                zone_ = Zone::categories;
            }
            else if (input.nav == Direction::left)
            {
                ctx.cue(audio::Cue::back);
                zone_ = Zone::categories;
            }
            return;
        }
        ui::Form &form = forms_[static_cast<std::size_t>(category_)];
        // A row that edits with left and right keeps them; on any other row
        // left leads back to the categories.
        const bool leaves = input.nav == Direction::left && !form.uses_horizontal();
        switch (form.handle(input, *ctx.feedback))
        {
        case ui::Event::changed:
            apply(ctx, form, form.changed_id());
            break;
        case ui::Event::cancelled:
            zone_ = Zone::categories;
            break;
        case ui::Event::none:
            if (leaves)
            {
                ctx.cue(audio::Cue::back);
                zone_ = Zone::categories;
            }
            break;
        default:
            break;
        }
    }

    void show(int category)
    {
        category_ = std::clamp(category, 0, kCategoryCount - 1);
        shown_.value = 0.0f;
        shown_.velocity = 0.0f;
        if (category_ == kAbout)
            about_.scroll_to(0.0f, true);
    }

    // Writes one row's value into the settings; every change is saved and
    // applied at once (volumes, the piece set, the display size).
    void apply(app::Context &ctx, const ui::Form &form, int id)
    {
        Settings &s = *ctx.settings;
        const auto volume = [&form](int row)
        { return std::clamp(static_cast<int>(std::lround(form.slider_value(row))), 0, 10); };
        switch (id)
        {
        case kRowBoard:
            s.board_theme = std::clamp(form.choice_index(id), 0, board::kBoardThemeCount - 1);
            break;
        case kRowPieces:
            s.piece_set = std::clamp(form.choice_index(id), 0, board::kPieceSetCount - 1);
            break;
        case kRowCoordinates:
            s.coordinates = form.toggle_value(id);
            break;
        case kRowDests:
            s.show_dests = form.toggle_value(id);
            break;
        case kRowAutoQueen:
            s.auto_queen = form.toggle_value(id);
            break;
        case kRowPremoves:
            s.premoves = form.toggle_value(id);
            break;
        case kRowConfirmResign:
            s.confirm_resign = form.toggle_value(id);
            break;
        case kRowMusic:
            s.music_volume = volume(id);
            break;
        case kRowEffects:
            s.sfx_volume = volume(id);
            break;
        case kRowInterface:
            s.ui_volume = volume(id);
            break;
        case kRowSwapConfirm:
            s.swap_confirm = form.toggle_value(id);
            break;
        case kRowVibration:
            s.vibration = form.toggle_value(id);
            break;
        case kRowResolution:
            s.resolution = std::clamp(form.choice_index(id), 0, Settings::kResolutionCount - 1);
            break;
        case kRowReducedMotion:
            s.reduced_motion = form.toggle_value(id);
            break;
        case kRowShowFps:
            s.show_fps = form.toggle_value(id);
            break;
        default:
            return;
        }
        if (ctx.settings_changed)
            ctx.settings_changed();
    }

    // ---- the preview ---------------------------------------------------------

    board::Overlay preview_overlay(const app::Context &ctx) const
    {
        board::Overlay overlay;
        overlay.last_move.from = chess::parse_square("f1");
        overlay.last_move.to = chess::parse_square("c4");
        overlay.selected = chess::parse_square("g8");
        overlay.dests = {chess::parse_square("f6"), chess::parse_square("h6"),
                         chess::parse_square("e7")};
        overlay.coordinates = ctx.settings->coordinates;
        overlay.show_dests = ctx.settings->show_dests;
        overlay.cursor_color = ctx.theme().primary;
        return overlay;
    }

    // ---- the menu ------------------------------------------------------------

    static Rect row(int index)
    {
        return {kMenu.x + kMenuInset,
                kMenu.y + kMenuInset + 6.0f + static_cast<float>(index) * (kRowHeight + kRowGap),
                kMenu.w - 2.0f * kMenuInset, kRowHeight};
    }

    // 0..1 for the index-th part of the page since it was entered.
    float arrive(int index) const
    {
        return calm_ ? 1.0f : look::rise(since_, index, 0.06f, 0.5f);
    }

    // A category's sign, drawn from shapes in one colour.
    static void draw_sign(gfx::DrawList &list, int category, const Rect &box, Color ink)
    {
        const float s = box.w;
        const float x = box.x;
        const float y = box.y;
        const float cx = box.cx();
        const float cy = box.cy();
        const float pen = std::max(s * 0.09f, 2.2f);
        const Color clear{ink.r, ink.g, ink.b, 0.0f};
        switch (category)
        {
        case kBoard:
            // Four squares of a board.
            list.bordered_rect({x + s * 0.06f, y + s * 0.06f, s * 0.88f, s * 0.88f}, s * 0.14f,
                               clear, pen, ink);
            list.rounded_rect({x + s * 0.06f, y + s * 0.06f, s * 0.44f, s * 0.44f}, s * 0.12f, ink);
            list.rounded_rect({cx, cy, s * 0.44f, s * 0.44f}, s * 0.12f, ink);
            break;
        case kPlay:
            // A move: from a square to a square.
            list.circle(x + s * 0.2f, y + s * 0.78f, s * 0.14f, ink);
            list.arrow(x + s * 0.3f, y + s * 0.68f, x + s * 0.84f, y + s * 0.16f, pen * 1.1f,
                       s * 0.3f, ink);
            break;
        case kSound:
            // A speaker and two waves.
            list.rounded_rect({x + s * 0.06f, cy - s * 0.15f, s * 0.2f, s * 0.3f}, 2.0f, ink);
            list.triangle({x + s * 0.02f, cy - s * 0.2f, s * 0.6f, s * 0.4f}, ink, 0.0f, -1.5708f);
            list.arc(x + s * 0.44f, cy, s * 0.3f, pen, 0.6f, 1.94f, ink);
            list.arc(x + s * 0.44f, cy, s * 0.5f, pen, 0.6f, 1.94f, ink);
            break;
        case kController:
            // A pad: its outline, a stick and two buttons.
            list.bordered_rect({x + s * 0.02f, y + s * 0.22f, s * 0.96f, s * 0.56f}, s * 0.26f,
                               clear, pen, ink);
            list.circle(x + s * 0.3f, cy, s * 0.1f, ink);
            list.circle(x + s * 0.6f, cy + s * 0.07f, s * 0.07f, ink);
            list.circle(x + s * 0.76f, cy - s * 0.07f, s * 0.07f, ink);
            break;
        case kDisplay:
            // A screen on its foot.
            list.bordered_rect({x + s * 0.04f, y + s * 0.12f, s * 0.92f, s * 0.6f}, s * 0.1f, clear,
                               pen, ink);
            list.rounded_rect({cx - s * 0.2f, y + s * 0.84f, s * 0.4f, pen}, pen * 0.5f, ink);
            list.rounded_rect({cx - pen * 0.5f, y + s * 0.72f, pen, s * 0.14f}, 0.0f, ink);
            break;
        default:
            // An "i" in a ring.
            list.ring(cx, cy, s * 0.46f, pen, ink);
            list.circle(cx, cy - s * 0.2f, pen * 0.75f, ink);
            list.rounded_rect({cx - pen * 0.5f, cy - s * 0.06f, pen, s * 0.3f}, pen * 0.5f, ink);
            break;
        }
    }

    void draw_menu(const app::Context &ctx, gfx::DrawList &list, Color accent) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const float in = arrive(0);
        list.push_opacity(in);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in));
        look::panel(list, kMenu);

        // The lit plate under the category that is showing.
        const Rect plate = plate_.value();
        list.glow(plate, kRowRadius, 16.0f, accent.with_alpha(0.1f));
        list.gradient_rect_h(plate, kRowRadius, accent.with_alpha(0.3f), accent.with_alpha(0.06f));
        list.bordered_rect(plate, kRowRadius, look::kClear, 1.5f, accent.with_alpha(0.5f));

        for (int i = 0; i < kCategoryCount; ++i)
        {
            const Rect r = row(i);
            const float lit = tween::clamp01(1.0f - std::abs(plate.y - r.y) / kRowHeight);
            const float part = calm_ ? 1.0f : look::rise(since_, i, 0.045f, 0.4f);
            list.push_opacity(part);
            list.push_transform(1.0f, 0.0f, 0.0f, -14.0f * (1.0f - part), 0.0f);
            const Rect tile{r.x + 12.0f, r.cy() - kTile * 0.5f, kTile, kTile};
            if (lit > 0.01f)
                list.glow(tile, 13.0f, 10.0f, accent.with_alpha(0.35f * lit));
            list.rounded_rect(tile, 13.0f, gfx::mix(accent.with_alpha(0.15f), accent, lit));
            draw_sign(list, i, {tile.cx() - 12.0f, tile.cy() - 12.0f, 24.0f, 24.0f},
                      gfx::mix(accent, look::kNight, lit));
            const float x = tile.x + tile.w + 16.0f;
            ui::text_fit(list, fonts.semibold, category_name(i), x, r.cy() + 25.0f * 0.35f, 25.0f,
                         r.x + r.w - 12.0f - x,
                         look::kInk.with_alpha(tween::lerp(look::kMuted, 1.0f, lit)));
            list.pop_transform();
            list.pop_opacity();
        }
        // The ring, while the categories have the controller.
        look::ring(list, ring_.value(), look::kInk, menu_focus_.value, kRowRadius);

        ui::paragraph(
            list, fonts.regular, tr("Changes apply at once and are saved for this console."),
            kMenu.x + kMenuInset + 12.0f, kMenu.y + kMenu.h - 74.0f, 19.0f,
            kMenu.w - 2.0f * kMenuInset - 24.0f, 27.0f, look::kInk.with_alpha(look::kFaint), 3);
        list.pop_transform();
        list.pop_opacity();
    }

    // ---- what the settings do, beside the form ----------------------------------

    // The springs behind the pictures follow the settings (snap: at once).
    void sync_visuals(const Settings &s, bool snap)
    {
        levels_[0].target = static_cast<float>(s.music_volume) / 10.0f;
        levels_[1].target = static_cast<float>(s.sfx_volume) / 10.0f;
        levels_[2].target = static_cast<float>(s.ui_volume) / 10.0f;
        swap_.target = s.swap_confirm ? 1.0f : 0.0f;
        rumble_.target = s.vibration ? 1.0f : 0.0f;
        size_.target = static_cast<float>(s.resolution);
        counter_.target = s.show_fps ? 1.0f : 0.0f;
        still_.target = s.reduced_motion ? 1.0f : 0.0f;
        if (!snap)
            return;
        for (tween::Spring &level : levels_)
            level.snap(level.target);
        swap_.snap(swap_.target);
        rumble_.snap(rumble_.target);
        size_.snap(size_.target);
        counter_.snap(counter_.target);
        still_.snap(still_.target);
    }

    void draw_preview(const app::Context &ctx, ui::Canvas &canvas) const
    {
        gfx::DrawList &list = canvas.list;
        const ui::Fonts &fonts = *ctx.fonts;
        const Color accent = look::accent(look::Section::settings);
        const float pad = (kPreview - kPreviewBoard) * 0.5f;
        const Rect well{detail_.x + detail_.w - kPreview, detail_.y, kPreview,
                        72.0f + kPreviewBoard + pad};
        look::panel(list, well, 0.0f, accent, 20.0f);
        const Settings &s = *ctx.settings;
        const Rect area{well.x + pad, well.y + 72.0f, kPreviewBoard, kPreviewBoard};
        const char *kicker = tr("Preview");
        std::string note;
        switch (category_)
        {
        case kSound:
            kicker = tr("Levels");
            draw_levels(list, fonts, area, accent);
            break;
        case kController:
            kicker = tr("Buttons");
            draw_buttons(ctx, list, area, accent);
            break;
        case kDisplay:
            kicker = tr("Picture");
            note =
                Settings::kResolutions[std::clamp(s.resolution, 0, Settings::kResolutionCount - 1)]
                    .label;
            draw_picture(ctx, list, area, accent);
            break;
        default:
            // The board's name and the piece set's: two names side by side.
            note = std::string(tr(ctx.board_theme().label)) + " \xC2\xB7 " +
                   piece_set_name(s.piece_set);
            look::frame_board(list, area, accent, 0.4f);
            preview_.draw(list, fonts, *ctx.pieces, area, preview_overlay(ctx), ctx.board_theme(),
                          ctx.time);
            break;
        }
        // The label has up to half of the line; the names beside it the rest.
        const float label = kicker_fit(list, fonts, kicker, well.x + pad, well.y + 44.0f,
                                       note.empty() ? area.w : area.w * 0.5f, accent);
        if (!note.empty())
            ui::text_fit(list, fonts.regular, note, well.x + well.w - pad, well.y + 44.0f, 20.0f,
                         area.w - label - 16.0f, look::kInk.with_alpha(look::kMuted), Align::right);
        // Changing the resolution is the one change the player sees happen to
        // the whole picture: say so while that row has the focus.
        const bool resolution = zone_ == Zone::detail && category_ == kDisplay &&
                                forms_[kDisplay].focus_id() == kRowResolution;
        if (resolution)
            ui::paragraph(list, fonts.regular,
                          tr("A new resolution applies at once: the display restarts with the "
                             "size you chose."),
                          well.x + 4.0f, well.y + well.h + 44.0f, 21.0f, well.w - 8.0f, 30.0f,
                          look::kInk.with_alpha(look::kMuted), 5);
    }

    // Sound: the three volumes as meters of ten steps.
    void draw_levels(gfx::DrawList &list, const ui::Fonts &fonts, const Rect &area,
                     Color accent) const
    {
        static constexpr const char *kNames[3] = {TR("Music"), TR("Game sounds"), TR("Interface")};
        constexpr int kSteps = 10;
        const float pitch = area.h / 3.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float y = area.y + static_cast<float>(i) * pitch;
            const float level = levels_[static_cast<std::size_t>(i)].value;
            // The name stops before the number at the right.
            kicker_fit(list, fonts, tr(kNames[i]), area.x, y + 26.0f, area.w - 56.0f,
                       look::kInk.with_alpha(look::kMuted), Align::left, 14.0f);
            char text[8];
            std::snprintf(text, sizeof(text), "%d", static_cast<int>(level * 10.0f + 0.5f));
            look::figure(list, fonts, text, area.x + area.w, y + 30.0f, 28.0f, look::kInk,
                         Align::right);
            const float gap = 6.0f;
            const float width =
                (area.w - gap * static_cast<float>(kSteps - 1)) / static_cast<float>(kSteps);
            for (int k = 0; k < kSteps; ++k)
            {
                // A step lights as the level passes it; taller toward the top.
                const float on =
                    tween::clamp01(level * static_cast<float>(kSteps) - static_cast<float>(k));
                const float h =
                    14.0f + 26.0f * static_cast<float>(k + 1) / static_cast<float>(kSteps);
                const Rect step{area.x + static_cast<float>(k) * (width + gap),
                                y + pitch - 22.0f - h, width, h};
                list.rounded_rect(step, 4.0f, gfx::mix(look::kInk.with_alpha(0.1f), accent, on));
            }
        }
    }

    // Controller: which button confirms, and whether the pad rumbles.
    void draw_buttons(const app::Context &ctx, gfx::DrawList &list, const Rect &area,
                      Color accent) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const ui::GlyphStyle glyphs = ui::GlyphStyle::dark();
        constexpr float kSize = 92.0f;
        const float cy = area.y + 96.0f;
        const float left = area.x + area.w * 0.25f;
        const float right = area.x + area.w * 0.75f;
        const float swapped = swap_.value;
        // The two buttons stay; the two words trade places.
        ui::draw_button(list, fonts, glyphs, ui::Button::cross,
                        left - ui::button_width(ui::Button::cross, kSize) * 0.5f, cy, kSize);
        ui::draw_button(list, fonts, glyphs, ui::Button::circle,
                        right - ui::button_width(ui::Button::circle, kSize) * 0.5f, cy, kSize);
        const float confirm = tween::lerp(left, right, swapped);
        const float back = tween::lerp(right, left, swapped);
        look::tag(list, fonts, tr("Confirm"), confirm, cy + 82.0f, accent, look::kNight,
                  Align::center, 17.0f);
        look::tag(list, fonts, tr("Back"), back, cy + 82.0f, look::kInk.with_alpha(0.14f),
                  look::kInk, Align::center, 17.0f);

        // A pad that hums while vibration is on.
        look::rule(list, area.x, area.y + 216.0f, area.w);
        const float on = rumble_.value;
        const float py = area.y + 284.0f;
        const float shake = calm_ ? 0.0f : std::sin(clock_ * 46.0f) * 2.2f * on;
        const Rect pad{area.cx() - 70.0f + shake, py - 30.0f, 140.0f, 60.0f};
        list.rounded_rect(pad, 28.0f, gfx::mix(look::kInk.with_alpha(0.16f), accent, on * 0.85f));
        list.circle(pad.x + 38.0f, pad.cy(), 9.0f, look::kNight.with_alpha(0.6f));
        list.circle(pad.x + pad.w - 38.0f, pad.cy(), 9.0f, look::kNight.with_alpha(0.6f));
        for (int k = 0; k < 2; ++k)
        {
            const float reach = 22.0f + 16.0f * static_cast<float>(k);
            const float alpha = on * (0.7f - 0.3f * static_cast<float>(k));
            list.arc(pad.x - 6.0f, py, reach, 3.0f, 3.9f, 1.6f, accent.with_alpha(alpha));
            list.arc(pad.x + pad.w + 6.0f, py, reach, 3.0f, 0.78f, 1.6f, accent.with_alpha(alpha));
        }
        ui::text_fit(list, fonts.regular, on > 0.5f ? tr("Vibration on") : tr("Vibration off"),
                     area.cx(), py + 62.0f, 20.0f, area.w, look::kInk.with_alpha(look::kMuted),
                     Align::center);
    }

    // Display: the three picture sizes, the frame counter and a dot that
    // shows what "reduced motion" means.
    void draw_picture(const app::Context &ctx, gfx::DrawList &list, const Rect &area,
                      Color accent) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        // Three frames of the same shape, the chosen one lit.
        const Rect outer{area.x, area.y + 6.0f, area.w, area.w * 9.0f / 16.0f};
        for (int i = Settings::kResolutionCount - 1; i >= 0; --i)
        {
            const float share = static_cast<float>(Settings::kResolutions[i].height) / 2160.0f;
            const Rect r{outer.x, outer.y + outer.h * (1.0f - share), outer.w * share,
                         outer.h * share};
            const float lit = tween::clamp01(1.0f - std::abs(size_.value - static_cast<float>(i)));
            list.bordered_rect(r, 10.0f, accent.with_alpha(0.14f * lit), 2.0f,
                               gfx::mix(look::kInk.with_alpha(0.22f), accent, lit));
            look::kicker(list, fonts, Settings::kResolutions[i].label, r.x + r.w - 12.0f,
                         r.y + 24.0f, gfx::mix(look::kInk.with_alpha(look::kFaint), accent, lit),
                         Align::right, 13.0f);
        }
        // The frame counter, where it shows on the real picture.
        if (counter_.value > 0.01f)
            look::ticker(list, fonts, "60 FPS", outer.x + outer.w - 12.0f, outer.y - 6.0f, 15.0f,
                         look::kInk.with_alpha(0.6f * counter_.value), Align::right);

        // Motion: a dot that glides, or stands still.
        const float y = area.y + area.h - 52.0f;
        // The label has up to two fifths of the line; what it means the rest.
        const float label = kicker_fit(list, fonts, tr("Motion"), area.x, y - 26.0f, area.w * 0.4f,
                                       look::kInk.with_alpha(look::kMuted), Align::left, 14.0f);
        ui::text_fit(list, fonts.regular,
                     still_.value > 0.5f ? tr("Quick fades") : tr("Slides and springs"),
                     area.x + area.w, y - 26.0f, 19.0f, area.w - label - 16.0f,
                     look::kInk.with_alpha(look::kMuted), Align::right);
        const Rect track{area.x, y, area.w, 8.0f};
        list.rounded_rect(track, 4.0f, look::kInk.with_alpha(0.1f));
        const float moving = 1.0f - still_.value;
        const float phase = calm_ ? 0.5f : 0.5f - 0.5f * std::cos(clock_ * 1.9f);
        const float at = tween::lerp(0.5f, tween::cubic_in_out(phase), moving);
        const float cx = track.x + 12.0f + (track.w - 24.0f) * at;
        list.glow({cx - 12.0f, track.cy() - 12.0f, 24.0f, 24.0f}, 12.0f, 12.0f,
                  accent.with_alpha(0.4f));
        list.circle(cx, track.cy(), 12.0f, accent);
    }

    ui::ListView categories_; // the focus and the sounds of the menu; not drawn
    std::array<ui::Form, kFormCount> forms_;
    ui::TextView about_;
    board::BoardView preview_;

    Rect detail_{}; // the right pane's content area
    Zone zone_ = Zone::categories;
    int category_ = kBoard;
    std::string version_;
    tween::Spring shown_;      // 0 -> 1 as a newly chosen category settles
    float since_ = 0.0f;       // seconds since the page was entered: parts arrive by it
    float clock_ = 0.0f;       // for the pictures that move by themselves
    bool calm_ = false;        // reduced motion
    ui::SpringRect plate_;     // under the category that is showing
    ui::SpringRect ring_;      // around the focused category
    tween::Spring menu_focus_; // 0..1: the categories have the controller
    tween::Spring pane_lit_;   // 0..1: the pane has it
    // What the pictures beside the forms show, easing to the settings.
    std::array<tween::Spring, 3> levels_;
    tween::Spring swap_;    // Circle confirms
    tween::Spring rumble_;  // vibration is on
    tween::Spring size_;    // the resolution's index
    tween::Spring counter_; // the frame counter shows
    tween::Spring still_;   // reduced motion is on
};

} // namespace

std::unique_ptr<Page> make_settings_page(app::Context &ctx)
{
    return std::make_unique<SettingsPage>(ctx);
}

} // namespace pch::modes
