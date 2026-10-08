# The house look

ProsperoLichess started on the kit's stock Acrylic theme with stock
components everywhere. It worked and it looked plain: every part of the app
was the same grey glass rectangle on the same sky. The house look is what the
kit's own bespoke designs do (Aurora Shelf, Pulse Dashboard, Trophy Room, Now
Playing; see ps5-homebrew-ui `docs/DESIGNS.md` and `docs/CRAFT.md`), applied
to this app. `src/app/look.hpp` holds its vocabulary and
`src/modes/home_page.cpp` is the reference screen: read both before changing
a screen.

## Redesign, do not recolour

The first attempt at this kept every layout and changed colours and corners.
The owner looked at it and saw the old screen. A screen is not done when it
uses the helpers; it is done when someone who knew the old one sees a new one:
a different composition, a clear hero, things with signs and colours of their
own, data drawn as charts, light on the focused thing. The rail
(`src/modes/rail.cpp`) is the measure: compare it with the stock side menu it
replaced.

## The frame around a page

- **The rail** runs down the left from the top of the screen
  (`app::kRail`): the app's mark, an entry per page with a sign on a tile in
  that page's colour, a lit plate under the page that is showing, a note card
  when a game waits for a move.
- **The strip** across the top (`app::kStrip` beside the rail, `app::kBar` on
  full screens) names the page with its sign (`Page::title()`), then the
  connection, the bell and the account. **A page does not repeat its name**:
  no big "Play" or "Watch" heading at the top of the page; that room belongs
  to the content.
- **The hint row** stays at the bottom right (`app::draw_hints`).

## What makes a screen "house"

1. **Colour follows content.** Each part of the app has an accent and a sky
   (`look::Section`, `look::mood`): home is night blue, puzzles amber, play
   green, watch rose, profile violet, settings teal; the board screens use
   the same hues, lower. A page says which it is (`Page::section()`), a full
   screen overrides `Scene::mood()`, and the app eases the backdrop to it.
   Inside a screen, the accent belongs to the thing: a rapid tile is the
   rapid colour (`look::speed_color`), a live game is rose, a rating line has
   the colour of its tile. Never one global highlight colour.
2. **Panels are lit, not boxed.** `look::panel(list, rect, lit, accent)`: a
   dark gradient with a hairline. `lit` (0..1, a spring) brings the accent
   into its edge and fill. The focused panel floats: `look::lift` under it
   (shadow and a breathing glow in its accent). Where the focus moves between
   panels of one screen, one `look::ring` glides between them
   (`ui::SpringRect`), in the focused panel's accent.
3. **Type has a hierarchy.** A tracked kicker in the accent
   (`look::kicker`), then the title in the display face (`paint.heading`, 56
   to 72 for a hero, 34 to 44 for a panel), then body text at 68 % ink
   (`look::kInk.with_alpha(look::kMuted)`). Figures that matter use
   `look::figure`; anything that ticks (clocks, timers, counters) uses
   `look::ticker`. Small facts are a 14 px kicker over a figure.
4. **Data is drawn, not listed.** A rating shows its line over time
   (`look::sparkline` with `Perf::history`) and how it moved lately
   (`look::delta` with `Perf::progress`); a share is a ring or a donut
   (`look::gauge`, `look::donut`); a count over categories is `look::bars`;
   time left is a `look::level`. The kit's components for larger charts are
   there too (`ui::LineChart`, `ui::BarChart`, `ui::DonutChart`,
   `ui::ProgressRing`, `ui::Meter`, `ui::Counter`, `ui::Timeline`).
   **Only real data**: what the session, the saved records or the game
   provide. A figure that is not known yet shows its honest empty state (the
   helpers draw one); nothing is invented.
5. **Screens assemble, numbers count, nothing teleports.** Keep
   `float since_` (seconds since `enter()`), and draw each part inside
   `push_opacity(in)` / `push_transform(1, 0, 0, 0, look::settle(in))` with
   `in = look::rise(since_, index)`: parts arrive 50 to 90 ms apart in
   reading order. Numbers ease to their value with a `tween::Spring`. Charts
   grow with their own `rise`. Content that changes cross-fades. All of it is
   skipped under reduced motion (`ctx.reduced_motion()`): values snap, parts
   are simply there.
6. **Signs, not only words.** Things have a drawn sign in their colour: the
   five speeds (`look::speed_icon`), a bolt for Storm, steps for Streak, a
   live dot that pulses (`look::live_dot`), a gold tag for a title
   (`look::tag`). Draw them from the draw list's shapes; no image files.
7. **The board leads on board screens.** `look::frame_board` under the
   board (shadow, a glow in the screen's accent); the side to move is lit
   (`modes::PlayerPanel` does it); results arrive with ceremony (the verdict
   mark pops with `tween::back_out`, the rating change counts, confetti for a
   win is already there).

## Motion: the checklist

The owner asked that every screen has animation and transitions. Each screen
and each of its states has all of these; all are driven by `dt` in `update()`
and snapped or skipped under reduced motion.

1. A staggered entrance that replays on `enter()` (`look::rise`,
   `look::settle`, a `since_` clock).
2. One focus ring or plate that glides between items (`ui::SpringRect`); a
   highlight never jumps.
3. Content that changes cross-fades or slides: a tab, the detail of the
   focused item, a state line, the page's name in the strip.
4. Numbers count to their value; lines, rings, donuts and levels grow from
   zero.
5. A press answers (a short squeeze or pulse, `ui::Pulse`), and the end of a
   list answers a refused direction with a nudge (`ui::shake`), silent while
   the direction is only held.
6. A little idle life on the focused thing: the breathing glow of
   `look::lift`, `look::live_dot` on anything live.
7. Results arrive with ceremony: the verdict pops (`tween::back_out`), a
   record gets gold.

Between screens the app itself fades and scales one screen into the next
(`App::record_frame`), the home screen's pages rise in as the rail changes
them, and the backdrop drifts to the new screen's sky.

## What does not change

- **Behaviour.** Every control, flow, sound and network call stays as it is.
  This is a visual pass: the unit tests must keep passing unchanged except
  where they assert a layout or a shape count that you deliberately changed.
- **The grid.** `app/chrome.hpp`: content from `kContent` to `kRight`,
  between `kTop` and `kBottom`, gaps of `kGap`. Panels of one row share their
  top and bottom edges, columns share their left and right edges, and a row of
  cards fills its width exactly (the home page's four columns are the model).
  The owner asked for perfect alignment: check edges in the pictures.
- **The components.** Lists, grids, tabs, forms, dialogs, menus, keyboards
  and tables are still the kit's (`docs/UI.md`). They wear the house theme by
  default; give them the screen's accent with `look::tint(accent, a, b, ...)`
  once, in the constructor. Replace a stock component with drawing of your own
  only where it carries the screen (a hero, a tile, a figure).
- **The rules of `docs/UI.md`**: `update()` owns state, `draw()` is const and
  may run twice a frame, no disk or network on the frame, hints for every
  button used, text never overflows (`ui::fit_label`, `ui::paragraph`).
- **Ten-foot sizes.** Body text 22 to 28, nothing the player must read under
  19 (tracked capitals may be 13 to 17), everything inside the safe area.

## The helpers, in one table

| Call | Draws |
| --- | --- |
| `look::accent(Section)`, `look::mood(Section)` | A part's accent; its accent and sky |
| `look::speed_color(Speed)`, `look::speed_from(key)`, `look::speed_icon` | The five ways to play |
| `look::panel`, `look::lift`, `look::ring`, `look::rule` | A panel, its focus light, the gliding ring, a hairline |
| `look::frame_board`, `look::halo` | Depth and light behind a board or art |
| `look::kicker`, `look::figure`, `look::ticker`, `look::delta`, `look::tag` | Type and marks |
| `look::rise`, `look::settle` | Staggered arrival |
| `look::sparkline`, `look::gauge`, `look::donut`, `look::bars`, `look::level`, `look::live_dot` | Small charts and indicators |
| `look::tint(accent, components...)` | The accent for kit components |
| `look::kInk`, `kNight`, `kGood`, `kBad`, `kGold`, `kMuted`, `kFaint`, `kRadius`, `kPad` | Palette and measures |

Data for charts: `Session::perf(key)` gives `rating`, `games`, `provisional`,
`progress` (change over the last twelve games) and `history` (up to 48
ratings, oldest first, empty until fetched); `Session::account()` gives
`games`, `wins`, `losses`, `draws`, `play_seconds`. The PC preview account
(`host/snapshot_main.cpp`) fills all of them.

## Looking at it

```bash
PCH_ONLY=puzzle bash tools/host-snapshots.sh build/snapshots   # scenarios whose name contains the text
make test-unit
```

Open the PNGs and look at them at full size: alignment, overflow, contrast,
whether the focus is findable in a second, whether the empty states read.
A screen is done when its pictures are, in every scenario it has.
