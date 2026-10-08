// ProsperoLichess - Screens, the services they share, and the scene stack contract.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/look.hpp"
#include "audio/cues.hpp"
#include "board/board_view.hpp"
#include "board/pieces.hpp"
#include "core/input.hpp"
#include "core/settings.hpp"
#include "gfx/draw_list.hpp"
#include "ui/components/component.hpp"
#include "ui/feedback.hpp"
#include "ui/fonts.hpp"
#include "ui/theme.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace pch::lichess
{
class Session;
}
namespace pch::puzzles
{
class Pack;
}

namespace pch::app
{

class Scene;

// What kind of thing a short message is (it picks the icon and the colour).
enum class Note : std::uint8_t
{
    info,
    success,
    warning,
};

// Services every screen may use. Owned by the App; valid for the whole run.
struct Context
{
    const ui::Fonts *fonts = nullptr;
    board::PieceAtlas *pieces = nullptr;
    Settings *settings = nullptr;
    ui::Feedback *feedback = nullptr;    // sounds and rumble asked for this frame
    lichess::Session *lichess = nullptr; // online services (may be offline)
    const puzzles::Pack *pack = nullptr; // offline puzzles (nullptr if missing)
    std::string data_root;               // writable: /data/prosperolichess after elevation
    std::string assets;                  // read-only: /app0/assets
    std::string version;
    float time = 0.0f; // seconds since launch
    // The launch picture (the one the console shows while the app starts),
    // for the opening title; 0 when it could not be loaded.
    std::uint32_t title_art = 0;
    // The accent of whatever is showing, eased (the rail, the status bar).
    gfx::Color accent = look::kInk;
    // A short message that appears under the status bar and leaves by itself.
    std::function<void(const std::string &, Note)> notify;
    std::function<void()> settings_changed; // save and apply
    std::function<void()> quit;             // clean system close after an applied update

    void cue(audio::Cue c, float pitch = 1.0f, float pan = 0.0f, float gain = 1.0f) const
    {
        feedback->play(c, pitch, pan, gain);
    }
    void toast(const std::string &text) const
    {
        if (notify)
            notify(text, Note::info);
    }
    // The one look of the app: every component starts from it.
    const ui::Theme &theme() const
    {
        return ui::default_theme();
    }
    const board::BoardTheme &board_theme() const
    {
        const int index = settings->board_theme;
        return board::kBoardThemes[index >= 0 && index < board::kBoardThemeCount ? index : 0];
    }
    bool reduced_motion() const
    {
        return settings->reduced_motion;
    }
    // Mirrors the player's reduced-motion choice into components.
    template <typename... Components> void calm(Components &...components) const
    {
        ((components.style.reduced_motion = settings->reduced_motion), ...);
    }
};

// One frame of a screen, back to front:
//   the app's backdrop -> scene -> [glass capture] -> overlay
struct Frame
{
    gfx::DrawList scene;   // the screen itself
    gfx::DrawList overlay; // dialogs and menus, above the glass capture
    // Set glass to have everything under the overlay blurred into
    // glass_texture before the overlay is drawn; overlay components then draw
    // frosted panels (pass glass_texture to their canvas).
    bool glass = false;
    std::uint32_t glass_texture = 0;

    void reset()
    {
        scene.clear();
        overlay.clear();
        glass = false;
    }
};

// What a scene asks the stack to do after an update.
struct Transition
{
    enum class Kind
    {
        none,
        push,    // open next on top (this scene stays underneath)
        replace, // swap this scene for next
        pop,     // close this scene
        pop_to_root,
    };
    Kind kind = Kind::none;
    std::unique_ptr<Scene> next;

    static Transition stay()
    {
        return {};
    }
    static Transition push(std::unique_ptr<Scene> scene)
    {
        Transition t;
        t.kind = Kind::push;
        t.next = std::move(scene);
        return t;
    }
    static Transition replace(std::unique_ptr<Scene> scene)
    {
        Transition t;
        t.kind = Kind::replace;
        t.next = std::move(scene);
        return t;
    }
    static Transition pop()
    {
        Transition t;
        t.kind = Kind::pop;
        return t;
    }
    static Transition home()
    {
        Transition t;
        t.kind = Kind::pop_to_root;
        return t;
    }
};

// What the status strip says a screen is showing. A screen with a menu of its
// own beside it (the home screen's rail) names the place; the strip then
// starts beside that menu. No title: the strip spans the screen and carries
// the app's name.
struct Place
{
    const char *title = nullptr; // English, marked TR("..."): the strip translates it
    int icon = -1;               // an app::NavIcon, or -1 for none
    // The Touchpad opens the games the strip's bell counts from this screen.
    bool answers_bell = false;
};

class Scene
{
  public:
    virtual ~Scene() = default;
    // Called when the scene becomes the top of the stack (again).
    virtual void enter(Context &)
    {
    }
    // Owns all state and animation. Sounds go through ctx.cue or ctx.feedback.
    virtual Transition update(Context &ctx, const InputFrame &input, float dt) = 0;
    // Must not change state: a scene is drawn more than once in some frames.
    virtual void draw(Context &ctx, Frame &frame) const = 0;
    // Background tick for scenes hidden under the top one (clocks, streams).
    virtual void tick(Context &, float)
    {
    }
    // Releases GL objects before a display restart; rebuilt lazily after.
    virtual void release_gpu()
    {
    }
    // The accent and the sky of the screen: the app eases the backdrop to it.
    virtual look::Mood mood() const
    {
        return look::mood(look::Section::home);
    }
    virtual Place place() const
    {
        return {};
    }
    // False for screens that take the whole picture (the opening title).
    virtual bool shows_status_bar() const
    {
        return true;
    }
    virtual const char *name() const = 0;
};

} // namespace pch::app
