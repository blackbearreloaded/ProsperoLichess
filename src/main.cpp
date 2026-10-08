// ProsperoLichess - Application entry point.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Opens the display, controller and audio, then runs the app (home screen and
// game screens) every frame. Lifecycle markers, frame pacing and audio health
// are logged for hardware runs.

#include "app/app.hpp"
#include "audio/cues.hpp"
#include "audio/mixer.hpp"
#include "audio/music.hpp"
#include "core/version.hpp"
#include "core/frame_stats.hpp"
#include "core/haptics.hpp"
#include "core/input.hpp"
#include "core/save_file.hpp"
#include "core/settings.hpp"
#include "core/language.hpp"
#include "core/test_script.hpp"
#include "gfx/system_glyphs.hpp"
#include "gfx/canvas.hpp"
#include "gfx/font.hpp"
#include "gfx/renderer.hpp"
#include "platform/ps5/audio_out.hpp"
#include "platform/ps5/curl_platform.hpp"
#include "platform/ps5/display_egl.hpp"
#include "platform/ps5/elevation/elevation.hpp"
#include "platform/ps5/pad.hpp"
#include "platform/ps5/storage_paths.hpp"
#include "platform/ps5/system.hpp"
#include "ui/fonts.hpp"

#include <GL/glcorearb.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

extern "C" void pch_heap_stats(std::size_t *live_bytes, std::size_t *peak_bytes,
                               std::size_t *blocks, std::size_t *failures);

namespace
{

std::string g_assets = "/app0/assets";
std::string g_data_root = "/download0/prosperolichess";
// Unattended runs save a picture at every mark of their script. A PC reads
// them over FTP while the title runs (the title's storage is mounted only
// then): /mnt/sandbox/<TITLE_ID>_000/download0/prosperolichess/shots.
std::string g_shots = "/download0/prosperolichess/shots";
// An unattended run that asks for it (`guest`) keeps everything it saves
// here: it starts signed out with default settings and never reads or writes
// the player's account, settings or records.
std::string g_guest_root = "/download0/prosperolichess/guest";
constexpr int kShotWidth = 960;
constexpr int kShotHeight = 540;

// The bound framebuffer, bottom row first, as a 24-bit BMP.
bool save_picture(const std::string &path, int width, int height)
{
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) *
                                      static_cast<std::size_t>(height) * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    const std::uint32_t row = (static_cast<std::uint32_t>(width) * 3 + 3) & ~3u;
    const std::uint32_t size = 54 + row * static_cast<std::uint32_t>(height);
    unsigned char header[54] = {'B', 'M'};
    const auto put = [&](int at, std::uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
            header[at + i] = static_cast<unsigned char>(value >> (8 * i));
    };
    put(2, size);
    put(10, 54);
    put(14, 40);
    put(18, static_cast<std::uint32_t>(width));
    put(22, static_cast<std::uint32_t>(height));
    header[26] = 1;
    header[28] = 24;
    put(34, size - 54);
    std::FILE *file = std::fopen(path.c_str(), "wb");
    if (file == nullptr)
        return false;
    bool ok = std::fwrite(header, 1, sizeof(header), file) == sizeof(header);
    std::vector<unsigned char> line(row);
    for (int y = 0; ok && y < height; ++y)
    {
        const unsigned char *in =
            pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4;
        for (int x = 0; x < width; ++x)
        {
            line[static_cast<std::size_t>(x) * 3 + 0] = in[x * 4 + 2];
            line[static_cast<std::size_t>(x) * 3 + 1] = in[x * 4 + 1];
            line[static_cast<std::size_t>(x) * 3 + 2] = in[x * 4 + 0];
        }
        ok = std::fwrite(line.data(), 1, line.size(), file) == line.size();
    }
    return std::fclose(file) == 0 && ok;
}

void log_heap(std::uint64_t frames)
{
    std::size_t live = 0;
    std::size_t peak = 0;
    std::size_t blocks = 0;
    std::size_t failures = 0;
    pch_heap_stats(&live, &peak, &blocks, &failures);
    pch::sys::log("[PCH] heap frames=%llu live=%zu peak=%zu blocks=%zu failures=%zu",
                  static_cast<unsigned long long>(frames), live, peak, blocks, failures);
}

// Opens at *resolution (an index into Settings::kResolutions), falling back
// to 1080p; *resolution reports the mode that opened.
bool open_display(pch::ps5::Display &display, int *resolution)
{
    using pch::Settings;
    const Settings::Resolution &mode = Settings::kResolutions[*resolution];
    if (display.open(mode.width, mode.height))
    {
        pch::sys::log("[PCH] display mode %s %dx%d", mode.label, display.width(), display.height());
        return true;
    }
    if (*resolution == 0)
        return false;
    pch::sys::log("[PCH] display mode %s failed, using 1080p", mode.label);
    *resolution = 0;
    return display.open(Settings::kResolutions[0].width, Settings::kResolutions[0].height);
}

// The four faces the app ships. The kit's pixel and handwriting roles are
// not used by the app's theme; they alias faces that are loaded.
struct FontSet
{
    pch::gfx::Font regular;
    pch::gfx::Font semibold;
    pch::gfx::Font display;
    pch::gfx::Font mono;
    // The console's own fonts, for the scripts no face has.
    pch::gfx::SystemGlyphs system;
};

bool load_font(const char *name, pch::gfx::Font *font)
{
    std::string data;
    const std::string path = g_assets + "/fonts/" + name;
    if (!pch::save::read_file(path, &data) || !font->load(data))
    {
        pch::sys::log("[PCH] font %s failed: %s", name, font->error().c_str());
        return false;
    }
    return true;
}

// Uploads the atlases. Font handles are slot numbers given in upload order,
// so after a display restart the same order yields the same handles and
// every screen's FontRef stays valid.
pch::ui::Fonts upload_fonts(pch::gfx::Renderer &renderer, FontSet &set)
{
    pch::ui::Fonts fonts;
    fonts.regular = {&set.regular, renderer.batch().create_font_texture(set.regular)};
    fonts.semibold = {&set.semibold, renderer.batch().create_font_texture(set.semibold)};
    fonts.display = {&set.display, renderer.batch().create_font_texture(set.display)};
    fonts.mono = {&set.mono, renderer.batch().create_font_texture(set.mono)};
    fonts.pixel = fonts.mono;
    fonts.hand = fonts.regular;
    // The console's fonts share one more atlas, filled as text needs them.
    if (!set.system.empty())
        set.system.set_texture(renderer.batch().create_glyph_texture(
            pch::gfx::SystemGlyphs::kWidth, pch::gfx::SystemGlyphs::kHeight,
            set.system.atlas().data()));
    return fonts;
}

// Changes the display mode: every GL object dies with the context, so the
// shell and the renderer release theirs first and rebuild them afterwards.
bool restart_display(pch::ps5::Display &display, pch::gfx::Renderer &renderer, FontSet &set,
                     const pch::ui::Fonts &fonts, pch::app::App &shell, int *resolution)
{
    using namespace pch;
    const std::int64_t start = sys::monotonic_us();
    shell.release_gpu();
    renderer.release();
    display.close();
    if (!open_display(display, resolution) || !renderer.init())
        return false;
    const ui::Fonts again = upload_fonts(renderer, set);
    if (again.regular.texture != fonts.regular.texture ||
        again.semibold.texture != fonts.semibold.texture ||
        again.display.texture != fonts.display.texture || again.mono.texture != fonts.mono.texture)
    {
        sys::log("[PCH] font handles changed across the display restart");
        return false;
    }
    shell.restore_gpu(gfx::fit_viewport(display.width(), display.height()).scale,
                      renderer.glass_texture());
    sys::log("[PCH] display restart %dx%d in %lld ms", display.width(), display.height(),
             static_cast<long long>((sys::monotonic_us() - start) / 1000));
    return true;
}

} // namespace

int main()
{
    using namespace pch;
    sys::log("[PCH] entry");
    const auto elevation_status = elevation::request(elevation::Capability::filesystem);
    storage::set_filesystem_access(elevation_status == elevation::Status::ok);
    g_assets = storage::app_file("assets");
    g_data_root = storage::data_root();
    g_shots = storage::data_file("shots");
    g_guest_root = storage::data_file("guest");
    sys::log("[PCH] elevation status=%u path=%s app=%s data=%s",
             static_cast<unsigned>(elevation_status), elevation::path(),
             storage::app_root().c_str(), g_data_root.c_str());
    sys::log("[PCH] storage dir=%d", save::ensure_directory(g_data_root) ? 1 : 0);

    // Unattended test deployments carry a controller script; releases do not.
    TestScript script;
    {
        std::string text;
        std::string error;
        if (save::read_file(g_assets + "/test/script.txt", &text))
            sys::log("[PCH] test script %s", script.parse(text, &error) ? "loaded" : error.c_str());
    }
    std::string data_root = g_data_root;
    if (script.guest())
    {
        data_root = g_guest_root;
        sys::log("[PCH] guest storage dir=%d", save::ensure_directory(g_guest_root) ? 1 : 0);
        if (script.fresh())
        {
            // Everything the app saves, by name: a first launch again.
            int removed = 0;
            for (const char *name :
                 {"/settings.bin", "/account.bin", "/puzzle_records.sav", "/passplay.sav"})
                removed += std::remove((g_guest_root + name).c_str()) == 0 ? 1 : 0;
            sys::log("[PCH] guest storage emptied files=%d", removed);
        }
    }

    // The display opens at the saved resolution (1080p if that fails).
    const Settings saved = app::App::load_settings(data_root);
    int resolution = ps5::Display::supports_display_modes() ? saved.resolution : 0;
    ps5::Display display;
    if (!open_display(display, &resolution))
    {
        sys::log("[PCH] fatal: display open failed");
        sys::park();
    }
    // Static: the faces are large and must outlive every screen.
    static FontSet font_set;
    gfx::Renderer renderer;
    if (!load_font("inter-regular.pchfont", &font_set.regular) ||
        !load_font("inter-semibold.pchfont", &font_set.semibold) ||
        !load_font("montserrat-medium.pchfont", &font_set.display) ||
        !load_font("dejavu-sans-mono.pchfont", &font_set.mono) || !renderer.init())
    {
        sys::log("[PCH] fatal: renderer init failed");
        sys::park();
    }
    // The app speaks the console's language when it has a catalog for it
    // (assets/lang/<tag>.po); the data-root language.txt names another.
    int system_language = -1;
    const int language_rc = sys::system_language(&system_language);
    const std::string language_tag = language::wanted(
        data_root, language_rc == 0 ? language::tag_for(system_language) : std::string("en-US"));
    // Scripts no face has (Japanese, Korean, Chinese, Thai, Arabic) are drawn
    // with the console's own fonts, that language's first.
    std::string font_folder = "none";
    std::size_t font_files = 0;
    for (const std::string &folder : gfx::system_font_folders())
    {
        std::vector<std::string> files = gfx::system_font_files(folder, language_tag);
        if (files.empty())
            continue;
        font_folder = folder;
        font_files = files.size();
        font_set.system.use(std::move(files), language_tag);
        break;
    }
    const ui::Fonts fonts = upload_fonts(renderer, font_set);
    // Letters a face lacks come from Inter: the display face has no Greek,
    // the mono face little Vietnamese. (The handles outlive a display restart.)
    font_set.display.set_fallback(&font_set.semibold, fonts.semibold.texture);
    font_set.mono.set_fallback(&font_set.regular, fonts.regular.texture);
    for (gfx::Font *face :
         {&font_set.regular, &font_set.semibold, &font_set.display, &font_set.mono})
        face->use_system(&font_set.system);
    renderer.set_system_glyphs(&font_set.system);
    {
        const language::Choice choice = language::choose(
            g_assets, language_tag, [](std::string_view text)
            { return font_set.regular.can_draw(text) && font_set.semibold.can_draw(text); });
        sys::log("[PCH] language system=%d rc=0x%x tag=%s catalog=%s texts=%zu", system_language,
                 static_cast<unsigned>(language_rc), choice.tag.c_str(), choice.catalog.c_str(),
                 choice.texts);
        sys::log("[PCH] system fonts folder=%s files=%zu read=%s", font_folder.c_str(), font_files,
                 font_set.system.loaded().c_str());
    }
    gfx::Viewport viewport = gfx::fit_viewport(display.width(), display.height());

    ps5::Pad pad;
    pad.open();
    InputTracker tracker;
    audio::Mixer mixer;
    // The music stream attaches to the mixer before the audio thread starts.
    // The playlist order is shuffled from the launch time, so it differs
    // every time the app opens.
    audio::MusicPlayer music;
    const int tracks = music.init(mixer, g_assets + "/audio/music",
                                  static_cast<std::uint64_t>(sys::monotonic_us()));
    sys::log("[PCH] music songs=%d", tracks);
    ps5::AudioOut audio_out;
    audio_out.start(mixer);
    audio::SoundBank sounds;
    const auto bank = sounds.load(g_assets + "/audio/sfx");
    sys::log("[PCH] sounds files=%d rejected=%d", bank.files, bank.rejected);
    for (const std::string &error : bank.errors)
        sys::log("[PCH] sound rejected %s", error.c_str());

    app::App shell(app::gpu_for(renderer), fonts, viewport.scale, data_root, g_assets);
    shell.set_applied_resolution(resolution);
    std::string param_json;
    save::read_file(storage::app_file("sce_sys/param.json"), &param_json);
    const std::string version = content_version(param_json);
    shell.set_version(version);
    shell.check_for_update(param_json);
    sys::log("[PCH] version %s", version.empty() ? "unknown" : version.c_str());

    if (script.active())
    {
        shell.network_selftest();
        net::start_transport_check();
        sys::log("[PCH] shots dir=%d", save::ensure_directory(g_shots) ? 1 : 0);
    }
    gfx::Canvas capture; // the off-screen target of a script's pictures
    std::string shot;    // the picture to take of this frame ("" for none)

    std::int64_t previous = sys::monotonic_us();
    std::uint64_t frames = 0;
    FrameStats stats;
    // The FPS overlay averages over half a second so the number is readable.
    double fps_seconds = 0.0;
    int fps_frames = 0;
    double fps_shown = 0.0;
    PadSample samples[64];
    std::int64_t last_frame_start = sys::monotonic_us();
    for (;;)
    {
        const std::int64_t now = sys::monotonic_us();
        // Animation time is start-to-start (one full frame), not the gap
        // between the previous swap returning and this frame beginning.
        const float dt =
            frames == 0 ? 1.0f / 60.0f : static_cast<float>(now - last_frame_start) / 1e6f;
        last_frame_start = now;
        const std::size_t count = pad.read(samples);
        InputFrame input = tracker.update(std::span<const PadSample>(samples, count),
                                          static_cast<std::uint64_t>(now));
        if (script.active())
        {
            const std::string mark = script.step(&input);
            if (!mark.empty())
            {
                sys::log("[PCH] mark %s scene=%s", mark.c_str(), shell.active_scene());
                shot = mark;
            }
            const std::string update = script.take_update();
            if (!update.empty())
                shell.preview_update(update);
            if (script.quit_requested())
            {
                // An unattended run ends itself; a title is never killed from outside.
                pad.close();
                sys::quit();
            }
        }
        if (input.focus_lost)
            sys::log("[PCH] input focus lost connected=%d", input.connected ? 1 : 0);
        if (input.pressed != 0 || input.nav != Direction::none)
            sys::log("[PCH] input pressed=0x%x held=0x%x nav=%d repeat=%d samples=%zu raw=0x%x",
                     input.pressed, input.held, static_cast<int>(input.nav),
                     input.nav_repeat ? 1 : 0, count, count > 0 ? samples[count - 1].buttons : 0u);

        shell.update(input, dt > 0.05f ? 0.05f : dt);
        if (shell.quit_requested())
        {
            pad.close();
            sys::quit();
        }
        if (shell.take_settings_changed())
        {
            const Settings &settings = shell.settings();
            mixer.set_bus_gain(audio::Bus::music, Settings::gain(settings.music_volume));
            mixer.set_bus_gain(audio::Bus::sfx, Settings::gain(settings.sfx_volume));
            mixer.set_bus_gain(audio::Bus::ui, Settings::gain(settings.ui_volume));
            InputSettings input_settings = tracker.settings();
            input_settings.swap_confirm = settings.swap_confirm;
            tracker.set_settings(input_settings);
        }
        // Every cue plays in the app's own sound set unless a component names
        // another; the kit's set fills in the interface cues ours lacks.
        const ui::Feedback &feedback = shell.feedback();
        Haptic rumble{feedback.rumble_strength, feedback.rumble_seconds};
        for (const audio::CueEvent &event : feedback.cues)
        {
            sounds.play(mixer,
                        event.set == audio::SoundSet::count ? audio::SoundSet::chess : event.set,
                        event);
            const Haptic haptic = haptic_for(event.cue);
            if (haptic.strength > rumble.strength)
                rumble = haptic;
            if (event.cue == audio::Cue::victory || event.cue == audio::Cue::new_record)
                music.duck();
        }
        if (shell.settings().vibration && rumble.strength > 0.0f)
            pad.rumble(rumble.strength, rumble.seconds);
        music.pump(dt > 0.05f ? 0.05f : dt);
        pad.update(dt);
        if (shell.take_display_mode_changed())
        {
            resolution = shell.settings().resolution;
            capture.destroy(); // its GL objects go with the display
            if (!restart_display(display, renderer, font_set, fonts, shell, &resolution))
            {
                sys::log("[PCH] fatal: display restart failed");
                sys::park();
            }
            viewport = gfx::fit_viewport(display.width(), display.height());
            shell.set_applied_resolution(resolution);
            // The restart takes a moment; do not animate or count FPS across it.
            last_frame_start = sys::monotonic_us();
            previous = last_frame_start;
            fps_seconds = 0.0;
            fps_frames = 0;
        }

        fps_seconds += static_cast<double>(dt);
        ++fps_frames;
        if (fps_seconds >= 0.5)
        {
            fps_shown = fps_frames / fps_seconds;
            fps_seconds = 0.0;
            fps_frames = 0;
        }
        shell.set_fps(static_cast<float>(fps_shown));
        shell.compose(renderer);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        renderer.present(0, display.width(), display.height());
        bool pictured = false;
        if (!shot.empty())
        {
            // The picture is the same frame drawn once more into a small
            // off-screen target: reading the display surface back is slow.
            if (capture.texture() == 0)
                capture.create(kShotWidth, kShotHeight, 1);
            capture.bind();
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            renderer.present(capture.framebuffer(), kShotWidth, kShotHeight);
            glBindFramebuffer(GL_FRAMEBUFFER, capture.framebuffer());
            const bool ok = save_picture(g_shots + "/" + shot + ".bmp", kShotWidth, kShotHeight);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            sys::log("[PCH] shot %s ok=%d", shot.c_str(), ok ? 1 : 0);
            shot.clear();
            pictured = true;
        }
        if (!display.swap())
        {
            sys::log("[PCH] fatal: swap failed frame=%llu error=%s",
                     static_cast<unsigned long long>(frames),
                     ps5::egl_error_name(display.last_error()));
            sys::park();
        }
        ++frames;
        const std::int64_t presented = sys::monotonic_us();
        if (frames == 1)
        {
            sys::log("[PCH] first-swap ok instances=%zu draws=%zu", renderer.last_instances(),
                     renderer.last_draw_calls());
            const bool hidden = sys::hide_splash_screen();
            sys::log("[PCH] ready splash_hidden=%d", hidden ? 1 : 0);
            log_heap(frames);
        }
        else if (!pictured)
        {
            stats.add(static_cast<double>(presented - previous) / 1000.0);
        }
        previous = presented;
        // Saving a picture is slow: that frame is neither animated nor counted.
        if (pictured)
            last_frame_start = presented;
        if (stats.count() == 600)
        {
            char summary[160];
            stats.format(summary, sizeof(summary));
            sys::log("[PCH] %s draws=%zu shapes=%zu", summary, renderer.last_draw_calls(),
                     renderer.last_instances());
            sys::log("[PCH] audio grains=%llu errors=%llu voices=%d music=%s underruns=%llu",
                     static_cast<unsigned long long>(audio_out.grains()),
                     static_cast<unsigned long long>(audio_out.errors()), mixer.active_voices(),
                     music.current().c_str(),
                     static_cast<unsigned long long>(mixer.stream_underruns()));
            stats.reset();
            if (frames % 3600 < 600)
                log_heap(frames);
        }
    }
}
