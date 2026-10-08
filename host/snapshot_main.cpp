// ProsperoLichess - Headless host renderer: drives the app and draws screens to PNG files.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// usage: pch_snapshots <assets dir> <output dir> [width height]
// Runs the real App through scripted controller input (host/scenarios_*.cpp),
// rendering with the same renderer as the console through Mesa's surfaceless
// EGL (llvmpipe) into an offscreen framebuffer.
//
//   PCH_ONLY=<text>   only scenarios whose name contains the text
//   PCH_ONLINE=1      also run the scenarios that talk to lichess.org
//   PCH_LIVE=1        exercise the Lichess client against lichess.org and exit
//   PCH_ART=1         render the presentation art and exit
//   PCH_LANG=<tag>    the pictures in that language (assets/lang/<tag>.po)
//   PCH_LANG_FILE=<file>  ... in the language of that catalog, wherever it is
//   PCH_SYSTEM_FONTS=<folder>  copies of the console's fonts, for the languages
//                     written in scripts the app's own faces lack
//   PCH_TEXT_LOG=<file>   write every text each scenario drew (scenario, tab, text)
// Each scenario also reports the lines ui::text_fit had to cut.

#include "app/app.hpp"
#include "art.hpp"
#include "core/language.hpp"
#include "core/save_file.hpp"
#include "core/test_script.hpp"
#include "gfx/font.hpp"
#include "gfx/gl_program.hpp"
#include "gfx/renderer.hpp"
#include "gfx/system_glyphs.hpp"
#include "lichess/puzzle_sources.hpp"
#include "lichess/session.hpp"
#include "modes/page.hpp"
#include "scenarios.hpp"
#include "ui/fonts.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>

namespace pch::host
{

void Run::idle(int frames)
{
    for (int i = 0; i < frames; ++i)
    {
        app.update(InputFrame{}, 1.0f / 60.0f);
        if (realtime)
            usleep(1000000 / 60);
    }
}

void Run::send(InputFrame frame)
{
    frame.connected = true;
    app.update(frame, 1.0f / 60.0f);
    idle(2);
}

void Run::press(Action action)
{
    InputFrame frame;
    frame.pressed = action_bit(action);
    frame.held = action_bit(action);
    send(frame);
}

void Run::nav(Direction direction, int times)
{
    for (int i = 0; i < times; ++i)
    {
        InputFrame frame;
        frame.nav = direction;
        send(frame);
    }
}

void Run::page(int index)
{
    // Options hands the controller to the rail from anywhere in a page.
    press(Action::menu);
    nav(Direction::up, modes::kPageCount);
    nav(Direction::down, index);
    press(Action::confirm);
    idle(40);
}

void Run::open(std::unique_ptr<app::Scene> scene)
{
    app.open(std::move(scene));
    idle(30);
}

void preview_account(app::App &app, const std::string &assets)
{
    lichess::Account account;
    account.id = "blackbear";
    account.username = "blackbear";
    account.perfs = {{"rapid", 1834, false, 412},    {"blitz", 1756, false, 230},
                     {"classical", 1902, false, 38}, {"correspondence", 1811, false, 17},
                     {"puzzle", 2015, false, 1960},  {"bullet", 1622, true, 9}};
    account.games = 2666;
    account.wins = 1301;
    account.losses = 1122;
    account.draws = 243;
    account.play_seconds = 412LL * 3600;
    // A plausible walk for each line: the same shape, ending on the rating.
    static constexpr float kWalk[] = {-58, -66, -49, -52, -61, -40, -44, -31, -37, -22,
                                      -28, -34, -19, -25, -12, -18, -6,  -14, -9,  3,
                                      -4,  -11, -2,  8,   2,   -5,  6,   12,  4,   0};
    static constexpr int kProgress[] = {18, -7, 24, 11, 36, -12};
    for (std::size_t i = 0; i < account.perfs.size(); ++i)
    {
        lichess::Perf &perf = account.perfs[i];
        perf.progress = kProgress[i % 6];
        for (const float step : kWalk)
            perf.history.push_back(static_cast<float>(perf.rating) +
                                   step * (1.0f + 0.12f * static_cast<float>(i)));
    }
    std::vector<lichess::OngoingGame> games;
    {
        lichess::OngoingGame game;
        game.game_id = "preview01";
        game.fen = "2r2rk1/pp1bqppp/2n1pn2/3p4/3P1B2/2PBPN2/PP1N1PPP/R2Q1RK1 w - - 5 11";
        game.last_move = "f8c8";
        game.color = chess::Color::white;
        game.opponent = "penguingm1";
        game.opponent_rating = 1871;
        game.my_turn = true;
        game.seconds_left = 2 * 86400 + 4 * 3600;
        game.speed = "correspondence";
        game.rated = true;
        games.push_back(game);
        game.game_id = "preview02";
        game.fen = "8/5pk1/R5p1/7p/5P1P/6P1/r4PK1/8 w - - 3 41";
        game.last_move = "a3a2";
        game.color = chess::Color::black;
        game.opponent = "NordicKnight";
        game.opponent_rating = 1790;
        game.my_turn = false;
        game.seconds_left = 86400;
        game.speed = "correspondence";
        games.push_back(game);
    }
    std::string daily;
    save::read_file(assets + "/../tests/fixtures/lichess/daily.json", &daily);
    app.session().preview(std::move(account), std::move(games), std::move(daily));
}

} // namespace pch::host

namespace
{

bool open_context()
{
    auto get_platform_display = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
        eglGetProcAddress("eglGetPlatformDisplayEXT"));
    EGLDisplay display =
        get_platform_display != nullptr
            ? get_platform_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr)
            : eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0;
    EGLint minor = 0;
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor) ||
        !eglBindAPI(EGL_OPENGL_API))
        return false;
    const EGLint context_attributes[] = {EGL_CONTEXT_MAJOR_VERSION,
                                         4,
                                         EGL_CONTEXT_MINOR_VERSION,
                                         5,
                                         EGL_CONTEXT_OPENGL_PROFILE_MASK,
                                         EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                         EGL_NONE};
    EGLContext context =
        eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, context_attributes);
    return context != EGL_NO_CONTEXT &&
           eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
}

// Every text the scenario in hand drew, for PCH_TEXT_LOG.
std::set<std::string> &drawn_texts()
{
    static std::set<std::string> texts;
    return texts;
}

void note_text(std::string_view text)
{
    drawn_texts().emplace(text);
}

bool load_font(pch::gfx::Renderer &renderer, const std::string &path, pch::gfx::Font *font,
               pch::ui::FontRef *ref)
{
    std::string data;
    if (!pch::save::read_file(path, &data) || !font->load(data))
    {
        std::fprintf(stderr, "cannot load font %s\n", path.c_str());
        return false;
    }
    ref->font = font;
    ref->texture = renderer.batch().create_font_texture(*font);
    return true;
}

// PCH_LIVE=1: the daily puzzle, a training batch and the TV stream from the
// real lichess.org.
int run_live()
{
    char live_root[] = "/tmp/pch-live-XXXXXX";
    pch::lichess::Session session(mkdtemp(live_root) != nullptr ? live_root : "/tmp", true);
    auto source = pch::lichess::make_training_source(session, "mix");
    bool training = false;
    session.watch_tv("");
    std::uint64_t tv_updates = 0;
    pch::app::Context ctx;
    for (int i = 0; i < 20 * 60 && (!training || tv_updates < 3); ++i)
    {
        session.pump(1.0f / 60.0f);
        if (!training)
        {
            pch::puzzles::Puzzle puzzle;
            std::string error;
            const auto state = source->next(ctx, 0, &puzzle, &error);
            if (state == pch::puzzles::Source::State::ready)
            {
                training = true;
                std::fprintf(stderr, "live: training puzzle %s rating %d, %zu moves\n",
                             puzzle.id.c_str(), puzzle.rating, puzzle.solution.size());
            }
            else if (state == pch::puzzles::Source::State::error)
            {
                std::fprintf(stderr, "live: training error %s\n", error.c_str());
                return 1;
            }
        }
        if (session.tv().valid && session.tv().version != tv_updates)
        {
            tv_updates = session.tv().version;
            std::fprintf(stderr, "live: tv %s %s vs %s fen %s\n", session.tv().id.c_str(),
                         session.tv().white.c_str(), session.tv().black.c_str(),
                         session.tv().position.fen().c_str());
        }
        usleep(1000000 / 60);
    }
    pch::puzzles::Puzzle daily;
    std::string error;
    const bool daily_ok = pch::lichess::parse_api_puzzle(session.daily_json(), &daily, &error);
    std::fprintf(stderr, "live: online=%d daily=%s %s training=%d tv_updates=%llu\n",
                 session.online() ? 1 : 0, daily_ok ? daily.id.c_str() : "FAILED", error.c_str(),
                 training ? 1 : 0, static_cast<unsigned long long>(tv_updates));
    return session.online() && daily_ok && training && tv_updates >= 2 ? 0 : 1;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: %s <assets dir> <output dir> [width height]\n", argv[0]);
        return 2;
    }
    const std::string assets = argv[1];
    const std::string output = argv[2];
    const int width = argc > 4 ? std::atoi(argv[3]) : 1920;
    const int height = argc > 4 ? std::atoi(argv[4]) : 1080;

    if (std::getenv("PCH_LIVE") != nullptr)
        return run_live();

    if (!open_context())
    {
        std::fprintf(stderr, "no surfaceless EGL OpenGL 4.5 context\n");
        return 1;
    }
    std::fprintf(stderr, "GL %s / %s\n", reinterpret_cast<const char *>(glGetString(GL_VERSION)),
                 reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    pch::gfx::set_glsl_prefix("#version 450 core\n");

    pch::gfx::Renderer renderer;
    pch::gfx::Font regular;
    pch::gfx::Font semibold;
    pch::gfx::Font display;
    pch::gfx::Font mono;
    pch::ui::Fonts fonts;
    if (!renderer.init() ||
        !load_font(renderer, assets + "/fonts/inter-regular.pchfont", &regular, &fonts.regular) ||
        !load_font(renderer, assets + "/fonts/inter-semibold.pchfont", &semibold,
                   &fonts.semibold) ||
        !load_font(renderer, assets + "/fonts/montserrat-medium.pchfont", &display,
                   &fonts.display) ||
        !load_font(renderer, assets + "/fonts/dejavu-sans-mono.pchfont", &mono, &fonts.mono))
        return 1;
    fonts.pixel = fonts.mono;
    fonts.hand = fonts.regular;
    display.set_fallback(&semibold, fonts.semibold.texture);
    mono.set_fallback(&regular, fonts.regular.texture);

    // The console's fonts, when PCH_SYSTEM_FONTS names copies of them.
    static pch::gfx::SystemGlyphs console_fonts;
    {
        const char *wanted = std::getenv("PCH_LANG");
        for (const std::string &folder : pch::gfx::system_font_folders())
        {
            std::vector<std::string> files =
                pch::gfx::system_font_files(folder, wanted != nullptr ? wanted : "en-US");
            if (files.empty())
                continue;
            console_fonts.use(std::move(files), wanted != nullptr ? wanted : "en-US");
            console_fonts.set_texture(renderer.batch().create_glyph_texture(
                pch::gfx::SystemGlyphs::kWidth, pch::gfx::SystemGlyphs::kHeight,
                console_fonts.atlas().data()));
            break;
        }
        for (pch::gfx::Font *face : {&regular, &semibold, &display, &mono})
            face->use_system(&console_fonts);
        renderer.set_system_glyphs(&console_fonts);
    }

    {
        const auto can_draw = [&](std::string_view text)
        { return regular.can_draw(text) && semibold.can_draw(text); };
        const char *tag = std::getenv("PCH_LANG");
        const char *file = std::getenv("PCH_LANG_FILE");
        const pch::language::Choice choice =
            file != nullptr
                ? pch::language::choose_file(file, tag != nullptr ? tag : "en-US", can_draw)
                : pch::language::choose(assets, tag != nullptr ? tag : "en-US", can_draw);
        std::fprintf(stderr, "language tag=%s catalog=%s texts=%zu\n", choice.tag.c_str(),
                     choice.catalog.c_str(), choice.texts);
    }
    const char *text_log = std::getenv("PCH_TEXT_LOG");
    std::FILE *texts_out = text_log != nullptr ? std::fopen(text_log, "w") : nullptr;
    if (texts_out != nullptr)
        pch::gfx::set_text_probe(note_text);
    pch::ui::fit_stats().record = true;

    GLuint framebuffer = 0;
    GLuint color = 0;
    glGenFramebuffers(1, &framebuffer);
    glGenRenderbuffers(1, &color);
    glBindRenderbuffer(GL_RENDERBUFFER, color);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return 1;

    std::vector<unsigned char> pixels(static_cast<std::size_t>(width * height * 4));
    stbi_flip_vertically_on_write(1);
    const auto save_frame = [&](const std::string &name)
    {
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        const std::string path = output + "/" + name + ".png";
        const bool ok =
            stbi_write_png(path.c_str(), width, height, 4, pixels.data(), width * 4) != 0;
        std::fprintf(stderr, "wrote %s: %zu shapes, %zu draw calls, GL error 0x%x\n", path.c_str(),
                     renderer.last_instances(), renderer.last_draw_calls(), glGetError());
        return ok;
    };
    const auto write = [&](pch::app::App &app, const std::string &name)
    {
        app.compose(renderer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        renderer.present(framebuffer, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        return save_frame(name);
    };

    // PCH_SPECIMEN=<file>: the file's lines in each face, to look at letters
    // (and the console's fonts) without a screen of the app.
    if (const char *specimen = std::getenv("PCH_SPECIMEN"))
    {
        std::string lines;
        if (!pch::save::read_file(specimen, &lines))
        {
            std::fprintf(stderr, "cannot read %s\n", specimen);
            return 1;
        }
        pch::gfx::DrawList list;
        list.rounded_rect({0.0f, 0.0f, 1920.0f, 1080.0f}, 0.0f, pch::gfx::Color::rgb(0x0b1020));
        const pch::gfx::Color ink = pch::gfx::Color::rgb(0xeef2ff);
        float y = 70.0f;
        std::size_t start = 0;
        while (start < lines.size() && y < 1060.0f)
        {
            std::size_t end = lines.find('\n', start);
            if (end == std::string::npos)
                end = lines.size();
            const std::string line = lines.substr(start, end - start);
            start = end + 1;
            if (line.empty())
                continue;
            pch::ui::text(list, fonts.regular, line, 60.0f, y, 34.0f, ink);
            pch::ui::text(list, fonts.display, line, 1860.0f, y, 34.0f, ink,
                          pch::gfx::Align::right);
            y += 46.0f;
            for (const std::string &wrapped : fonts.semibold.font->wrap(line, 26.0f, 520.0f))
            {
                pch::ui::text(list, fonts.semibold, wrapped, 700.0f, y, 26.0f, ink,
                              pch::gfx::Align::left, 2.0f);
                y += 34.0f;
            }
            y += 12.0f;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        renderer.begin();
        renderer.draw(list);
        renderer.present(framebuffer, width, height);
        const bool saved = save_frame("specimen");
        if (!console_fonts.empty())
            std::fprintf(stderr, "system fonts read=%s\n", console_fonts.loaded().c_str());
        return saved ? 0 : 1;
    }

    // PCH_ART=1: presentation art only (tools/render-art.sh turns it into sce_sys files).
    if (std::getenv("PCH_ART") != nullptr)
    {
        pch::board::PieceAtlas pieces;
        std::string error;
        if (!pieces.load(
                assets, 0, 400.0f, static_cast<float>(width) / 1920.0f,
                [&](int w, int h, const std::uint8_t *rgba)
                { return renderer.batch().create_texture(w, h, rgba); }, &error))
        {
            std::fprintf(stderr, "art: %s\n", error.c_str());
            return 1;
        }
        pch::gfx::DrawList list;
        const auto write_frame = [&](const char *name)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            renderer.begin();
            renderer.draw(list);
            renderer.present(framebuffer, width, height);
            return save_frame(name);
        };
        return pch::host::render_art(list, fonts, pieces, write_frame) ? 0 : 1;
    }

    pch::host::Scenarios scenarios;
    pch::host::add_shell_scenarios(scenarios);
    pch::host::add_puzzle_scenarios(scenarios);
    pch::host::add_play_scenarios(scenarios);
    pch::host::add_game_scenarios(scenarios);
    pch::host::add_watch_scenarios(scenarios);
    pch::host::add_account_scenarios(scenarios);

    const std::string only = std::getenv("PCH_ONLY") != nullptr ? std::getenv("PCH_ONLY") : "";
    const bool online = std::getenv("PCH_ONLINE") != nullptr;
    // The storage `guest` scripts share: one folder for this run, so two runs
    // at once (two work trees) do not change each other's pictures.
    char guest_template[] = "/tmp/pch-hardware-guest-XXXXXX";
    const std::string guest_root = mkdtemp(guest_template) != nullptr ? guest_template : "/tmp";
    bool ok = true;
    for (const pch::host::Scenario &scenario : scenarios)
    {
        if (!only.empty() && scenario.name.find(only) == std::string::npos)
            continue;
        if (scenario.connect && !online)
            continue;
        // Each scenario runs a fresh App over its own scratch data folder. A
        // hardware script that asks for storage of its own (`guest`) gets one
        // shared folder here too, emptied when it says `fresh`, so a pair of
        // scripts (a change, then a restart) replays as it runs on a console.
        std::string root;
        if (scenario.name.rfind("hardware-", 0) == 0)
        {
            std::string text;
            std::string error;
            pch::TestScript script;
            if (pch::save::read_file(
                    assets + "/../tests/hardware/" + scenario.name.substr(9) + ".txt", &text) &&
                script.parse(text, &error) && script.guest())
            {
                root = guest_root;
                pch::save::ensure_directory(root);
                if (script.fresh())
                {
                    for (const char *name :
                         {"/settings.bin", "/account.bin", "/puzzle_records.sav", "/passplay.sav"})
                        std::remove((root + name).c_str());
                }
            }
        }
        if (root.empty())
        {
            char root_template[] = "/tmp/pch-snapshots-XXXXXX";
            root = mkdtemp(root_template) != nullptr ? root_template : "/tmp";
        }
        pch::app::App app(pch::app::gpu_for(renderer), fonts, static_cast<float>(width) / 1920.0f,
                          root, assets, scenario.connect, scenario.boot);
        app.set_applied_resolution(2);
        app.set_version("01.000.000");
        if (scenario.signed_in)
            pch::host::preview_account(app, assets);
        pch::host::Run run{app, assets, scenario.connect, [&](const std::string &suffix)
                           { ok = write(app, scenario.name + "-" + suffix) && ok; }};
        run.idle(scenario.boot ? 0 : 20);
        scenario.script(run);
        ok = write(app, scenario.name) && ok;
        if (texts_out != nullptr)
        {
            for (const std::string &text : drawn_texts())
                std::fprintf(texts_out, "%s\t%s\n", scenario.name.c_str(), text.c_str());
            drawn_texts().clear();
        }
        pch::ui::FitStats &fit = pch::ui::fit_stats();
        if (fit.cut != 0)
        {
            const std::set<std::string> cut(fit.cut_texts.begin(), fit.cut_texts.end());
            for (const std::string &text : cut)
                std::fprintf(stderr, "cut %s: %s\n", scenario.name.c_str(), text.c_str());
        }
        fit.shrunk = 0;
        fit.cut = 0;
        fit.cut_texts.clear();
    }
    if (texts_out != nullptr)
        std::fclose(texts_out);
    if (!console_fonts.empty())
        std::fprintf(stderr, "system fonts read=%s\n", console_fonts.loaded().c_str());
    return ok ? 0 : 1;
}
