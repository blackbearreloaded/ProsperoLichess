# The interface: how screens are built

Everything on screen is drawn with the draw list and the component library
of [ps5-homebrew-ui](https://github.com/blackbearreloaded/ps5-homebrew-ui)
(the "kit"). On top of it the app has a look of its own, the **house look**:
lit panels, an accent and a sky per part of the app, figures with their lines
over time, a rail with the app's mark. [LOOK.md](LOOK.md) describes it; this
page says how screens are built and where things go.

## Layers

| Directory | What it is |
| --- | --- |
| `src/gfx`, `src/ui`, `src/ui/components` | The kit, copied in (commit in `src/ui/KIT_COMMIT`): draw list, renderer, backdrops, themes, `ui::Painter`, about a hundred components. Its guides apply unchanged: every component has a public `style`, `set_bounds`, `handle(input, feedback)`, `update(dt)` and a const `draw(canvas)`. Each component's header starts with a usage example. |
| `src/app` | The scene stack (`App`), what screens share (`Context`, `Frame`, `Scene` in `scene.hpp`), and the shared furniture (`chrome.hpp`: the layout grid, the status bar, the hint row, icons). |
| `src/modes` | The screens. `shell_scene.cpp` is the home screen: a side rail and six pages (`page.hpp`). The other files are full screens opened on top of it; `scenes.hpp` declares how each one is opened. |
| `src/board` | The animated board, controller input on it, the piece atlas, and `draw_mini_board` for thumbnails. |
| `host` | The PC renderer: `scenarios_*.cpp` script the real app and save pictures. |

Local changes to the kit, to carry over when it is updated: the `board` and
`arrow` shapes and `GlBatch::delete_texture` (`src/gfx`), the chess cues and
the `chess` sound set (`src/audio/cues.*`), `StatTileStyle::separator`, and
`ui::default_theme()` (`components/component.cpp`), which returns the house
theme: Acrylic with a deeper night, larger radii and softer hairlines.

## The frame

`App::compose` draws, back to front: the theme's backdrop, the screen
(`Frame::scene`), the status bar, then, above a blurred copy of all that, the
screen's `Frame::overlay` and the notices. A screen that opens a dialog, a
sheet or a pause menu draws it into `frame.overlay`, sets `frame.glass = true`
and passes `frame.glass_texture` to that canvas, so the overlay is frosted.

```cpp
void draw(app::Context &ctx, app::Frame &frame) const override
{
    ui::Canvas canvas = app::canvas_for(ctx, frame.scene);
    panel_.draw(canvas, area);
    ...
    frame.glass = dialog_.visible();
    ui::Canvas over = app::canvas_for(ctx, frame.overlay, frame.glass ? frame.glass_texture : 0);
    dialog_.draw(over);
}
```

`update()` owns all state and animation; `draw()` is const and may run more
than once in a frame (two screens are drawn while one gives way to the other).

## Rules for a screen

- **Theme**: components default to the house theme; give them the screen's
  accent with `look::tint(accent, a, b, ...)`. Colours come from
  `app/look.hpp` (`look::accent(section)`, `kInk`, `kGood`, `kBad`, `kGold`)
  and `ctx.theme()`; no other colour literals except on the board. Call
  `ctx.calm(a, b, ...)` in `update()` so the reduced-motion setting reaches
  every component.
- **Layout**: the 1920 x 1080 virtual canvas and the grid in `app/chrome.hpp`
  (`kMargin`, `kTop`, `kBottom`, `kGap`, `kContent`, `kBoardSquares`,
  `kBoardColumn`). The status bar occupies `kBar`; the hint row sits at the
  bottom right (`app::draw_hints`). Text is positioned by its baseline.
- **Components first**: a list is `ui::ListView`, a grid `ui::GridView`, tabs
  `ui::TabBar`, a dialog `ui::Dialog`, a pause menu `ui::PauseMenu`, a form
  `ui::Form`, text entry `ui::Keyboard`, a hold-to-confirm `ui::HoldButton`.
  Hand-drawn parts are for chess only (the board, pieces, move marks).
- **Focus**: one component receives `handle()` per frame; the screen moves
  the focus between components and mirrors it with `set_active` /
  `set_focused`. Circle goes back one level; on a page of the home screen it
  returns the controller to the rail (`PageResult::to_rail`), and Options does
  the same from anywhere in a page.
- **Sound**: components play their own cues. A screen asks for more with
  `ctx.cue(audio::Cue::...)`. Chess cues (`move`, `capture`, `check`, ...)
  also rumble the controller (`core/haptics.hpp`).
- **Messages**: `ctx.notify(text, app::Note::...)` shows a short line in the
  status bar. Connection trouble appears there by itself (`lichess::Session`
  issues and flashes).
- **No disk or network on the frame**: load in `enter()` or when a request
  completes; show `ui::Skeleton` or `ui::Spinner` meanwhile.

## Looking at it

```bash
bash tools/host-snapshots.sh build/snapshots      # every scenario to PNG
PCH_ONLY=home bash tools/host-snapshots.sh build/snapshots
make test-unit                                    # includes the whole app under random input
```

A scenario (`host/scenarios_*.cpp`) runs a fresh app, sends controller input
and saves a picture; `signed_in` scenarios show a previewed account without
any network. Add a scenario for every state a screen can be in, and look at
the pictures: a build that compiles proves nothing about a screen.
