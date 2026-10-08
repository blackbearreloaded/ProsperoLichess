# Architecture

ProsperoLichess is one native PS5 title: a home screen with a side rail of
pages over a shared engine, with lichess.org as the online back end. The design and
milestones are in [PLAN.md](../PLAN.md); this page records the structure as
built.

## Origin

- Created from the `ps5-native-app-boilerplate` template at
  `4f531c4b517f80bcb6b1267135848168d2250047`, with the engine layers
  (`src/audio`, `src/core`, `src/platform/ps5`) taken from ProsperoPuzzles.
- The interface layers (`src/gfx`, `src/ui`, `src/ui/components`) are the
  `ps5-homebrew-ui` kit at the commit in `src/ui/KIT_COMMIT`; see
  [UI.md](UI.md) for how screens are built on it.
- Title `PPSA99009` for both console testing and releases.

## Frame loop

`src/main.cpp` opens the EGL display (4K by default, falling back to 1080p),
the pad, AudioOut and the music player, then every frame:

1. reads every buffered pad sample into one `InputFrame` (or a test script's
   input on unattended hardware runs);
2. updates `app::App`, which pumps the Lichess session and the scene stack;
3. plays the cues and the rumble the screens asked for (`ui::Feedback`)
   through the sound bank and the pad;
4. composes the frame with `gfx::Renderer`: the theme's backdrop, the screen,
   the status bar, then, above a blurred copy of those, dialogs and notices
   (one instanced program, a handful of draw calls);
5. swaps (vsync paces the loop at 60 Hz).

## Scenes

`app::Scene` screens live on a stack owned by `app::App`. The root is the home
screen (`modes/shell_scene.cpp`): a side rail and six pages (Home, Puzzles,
Play, Watch, Profile, Settings; `modes/page.hpp`). Pushed on top of it are the
game, puzzle, Lichess TV, waiting-for-a-game and sign-in screens
(`modes/scenes.hpp` lists how each is opened). Scenes under the top keep
ticking (`Scene::tick`), so a game's stream and clocks survive a visit
elsewhere. `app::Context` lends them fonts, the piece atlas, settings, the
puzzle pack, the Lichess session and the frame's feedback. The status bar and
the connection notices belong to the app (`app/chrome.hpp`) and stay in place
while screens change.

## Board

- `chess/` implements the rules with bitboards (perft-verified), FEN, SAN, UCI
  and PGN movetext, following chessops semantics.
- `board::BoardView` draws the board (a shader-drawn chequer with procedural
  wood or marble), highlights, pieces from a baked RGBA atlas (128 px or 256 px
  cells chosen by output size), move and capture animations, the flip, check
  glow, arrows and the controller cursor.
- `board::BoardInput` turns controller input into selections, moves, premoves
  and promotions.
- `modes::GameScene` shows a `chess::Game` with players, clocks and the move
  list. Its `GameLink` is either absent (Pass & Play) or a Lichess Board API
  game (`lichess::make_board_link`).
- `modes::PuzzleScene` runs classic, Streak and Storm rules over any
  `puzzles::Source`: the offline pack, the daily puzzle or training batches.

## Networking

- `net::Client` runs HTTP on worker threads: a serial general lane, a priority
  lane for moves, and up to four NDJSON streams. Results reach the main thread
  through `poll()`. A 429 pauses the general lane for a minute.
- Backend: libcurl with OpenSSL (`net/http_curl.cpp`) on the console and on
  the host. On the console it uses the system's CA list, sockets opened with
  `sceNetSocket` (curl's socket calls are routed to them by `--wrap`, see
  `platform/ps5/net_sockets.c`) and the app's own name lookups on the system
  resolver. The system HTTP/SSL service (`platform/ps5/http_scehttp.cpp`)
  stays as a fallback and for comparison in unattended runs.
- `lichess::Session` owns the client, the token (`/data/prosperolichess/account.bin`),
  the account, the event stream, ongoing games, the TV stream and the daily
  puzzle, and turns responses into callbacks on the main thread.
- Sign-in uses OAuth PKCE with a QR code; the phone is redirected to
  `net::CallbackListener` on the console's LAN address. A personal token typed
  on `ui::Keyboard` is the fallback.

## Audio

`audio::Mixer` mixes sound-bank clips and the music streams on the audio
thread; the main thread only posts commands. Sound effects are 48 kHz WAVs in
`assets/audio/sfx` named by cue (see `audio/cues.hpp`); music is a shuffled
playlist of OGG files in `assets/audio/music`.

## Host tooling

- `make test-unit`: GoogleTest over the platform-neutral code (chess rules,
  puzzles, pack, networking with a local test server, Lichess parsing).
- `make host-snapshots`: the real App, offline, driven by scripted input and
  rendered to PNG through Mesa llvmpipe. `PCH_LIVE=1` exercises the client
  against lichess.org; `PCH_ART=1` renders the system artwork.
