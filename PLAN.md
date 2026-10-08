# ProsperoLichess: implementation plan

Draft 1, 2026-09-27.

ProsperoLichess is a native PS5 homebrew client for **lichess.org**, driven entirely by the controller. It opens on a hub of chess "games": puzzle modes, ways to play, and Lichess TV. You pick one, play it, return to the hub and pick another.

**The end goal:** a beautiful chess board, running natively on the PS5, on which you play live games online against other people on Lichess.

It is built on `ps5-native-app-boilerplate` and `ps5-opengl` (OpenGL 4.6 Core). Input, audio and video follow the ProsperoLight and Quake II native implementations. HTTPS follows ProsperoTV and ProsperoRadio. Console work follows `ps5-agent-runbook`. The shell, rendering and UI layers reuse the design from `ProsperoPuzzles-PLAN.md` (draft 2). Where this plan says "as in ProsperoPuzzles", the details there apply unchanged.

> **Before committing this file:** it becomes the repo's `PLAN.md` in M0. Strip the "Local environment" box in section 7 first; it belongs in the gitignored `.local/ENVIRONMENT.md`. `<ws>` is shorthand for the local `Documents/PS5/workspace` folder. The boilerplate lint rejects absolute user paths in tracked files, which is why paths are written this way.

---

## 1. Scope

### The hub (v1)

| Row | Mode | Needs | Backed by | Milestone |
|---|---|---|---|---|
| Puzzles | **Daily Puzzle** | network | `GET /api/puzzle/daily` | M7 |
| Puzzles | **Puzzle Training** | network. Rated when signed in. | `GET /api/puzzle/batch/{angle}`; results via `POST` (signed in) | M7, M9 |
| Puzzles | **Puzzle Themes** (offline) | — | bundled puzzle pack, filtered by theme | M5 |
| Puzzles | **Puzzle Streak** (offline) | — | bundled pack, rising rating ladder, one mistake ends it | M5 |
| Puzzles | **Puzzle Storm** (offline) | — | bundled pack, 3-minute timer with combo bonuses | M5 |
| Play | **Quick Pairing** | account | `POST /api/board/seek` + Board API | M10 |
| Play | **Play vs Computer** | account | `POST /api/challenge/ai`, levels 1–8 | M10 |
| Play | **My Games** | account | `GET /api/account/playing` + the event stream | M10 |
| Play | **Pass & Play** | — | local chess core, two players on one controller | M5 |
| Watch | **Lichess TV** | network | `GET /api/tv/channels`, `GET /api/tv/{channel}/feed` | M7 |
| Me | **Profile** | account | `/api/account`, `/api/user/{name}`, `/api/puzzle/dashboard/{days}` | M9 |

Streak and Storm run locally with local records. Lichess's own `/api/streak` and `/api/storm` are undocumented endpoints used only by the mobile app, so we don't depend on them.

### Also in v1
- **Sign-in:** OAuth with a QR code scanned by your phone, plus a fallback (section 5.10).
- **A chessground-quality board** (section 5.7): animation, highlights, legal-move markers, premoves, promotion picker, figurine move list, clocks, several board themes and piece sets.
- **Shell:** boot, home hub, settings, about/licenses, diagnostics, pause and game menus, toasts, offline and signed-out states.
- Host unit tests, a host preview runner that talks to the real Lichess from WSL, and fixture-driven protocol tests.
- A tagged `01.000.000` release.

### Excluded from v1
- **Bullet and blitz seeks.** The Board API does not allow them (finding 6). Blitz is available against the AI and in direct challenges.
- Chat, tournaments and arenas (and so berserk), variants and Chess960, studies, broadcasts, Puzzle Racer.
- An on-device engine, analysis, the opening explorer and tablebases. All are backlog, and all must stay disabled during live games (finding 8).
- Trophies, HDR, IME text entry, multiple users, localization.

---

## 2. Sources and what we take from each

| Source | Revision | What we take |
|---|---|---|
| **ps5-native-app-boilerplate** | `4f531c4` | Repo skeleton, clang wrapper, ELF→FSELF tool, `libc.prx` shim, Makefile, CI, lint, `param.json` validation, presentation assets, GoogleTest wiring. Drop `src/demo_renderer.*`. |
| **ps5-opengl** | `122aa89`, release `v0.3.0` | GL 4.6 Core static SDK with EGL, and its native-app recipe (`tools/build-native-test-app.sh`, `native-app/{app_heap.c, runtime_shims.c, ps5-pie.ld, app-symbols.map, param.json}`). The GL 4.6 demo is our hardware baseline. |
| **ps5-agent-runbook** | `85559fb` | The console contract (section 7) |
| **ProsperoPuzzles plan** | draft 2 | Shell architecture, `gfx2d`, UI toolkit, input model, storage format, mixer, the M0–M4 bring-up sequence and its known pitfalls |
| **ProsperoLight** (PPSA99002) | `143561a` | Pad, AudioOut, atomic saves, splash hand-off, klog, the thread-naming pitfall (lists below). Also the fallback TLS path: vendored mbedTLS 3.6.4, `platform/ps5/ps5_sockets.c`, `platform/ps5/ps5_entropy.c`. |
| **ps5-yamagi** (Quake II, PPSA99007) | `bc40477` | EGL bring-up and ordered teardown, the exit path, the writable-directory probe, the GL performance rules, the build recipe |
| **psiptv / ProsperoTV** (PPSA99003) | `6dc64af` | **The sceHttp client pattern:** `src/iptv_http.cpp` covers init (`846-887`), per-request headers and timeouts (`812-842`), long-lived streams (`1080-1173`) and cross-thread abort (`628-659`, `905-912`) |
| **prospero-radio / ProsperoRadio** (PPSA99001) | `4fbbcd0` | sceHttp keep-alive connection reuse (`src/radio_service.cpp:1062-1110`) and abort on stop (`docs/ARCHITECTURE.md:216-221`) |
| **Lichess API** | OpenAPI v2.0.174 | Endpoint contracts (Appendix A). The spec version is pinned in `docs/LICHESS_API.md`. |
| **chessops** (niklasf) | v0.15.1, GPL-3.0-or-later | Reference semantics for the C++ chess core: square sets, attacks, legal movegen, FEN, SAN, UCI, outcome, castling |
| **chessground** | v10.4.0, GPL-3.0-or-later | The board's behaviour and UX parity list (section 5.7). We re-implement it; no TypeScript is copied. |
| **lichess-org/mobile** | `main`, GPL-3.0 | The puzzle batch flow and the game-screen information layout |
| **Lichess puzzle database** | 2026-09-09, CC0 | The offline puzzle pack |
| **yyjson**, **qrcodegen** (Nayuki), **stb_truetype** | pinned | JSON (MIT), QR codes (MIT), font baking (public domain/MIT) |

**From ProsperoLight:** as listed in ProsperoPuzzles section 2:
- native pad input: `src/radio_input.cpp:121-161`
- the 120-byte pad sample
- the pad-open retry loop
- reconnect handling
- AudioOut setup and teardown
- `write_atomic`
- the splash hand-off
- klog
- `ps5_compat.h`: never call `pthread_setname_np`

**From ps5-yamagi:** as listed in ProsperoPuzzles section 2:
- `ps5_egl.c`
- `ps5_main.c:18-60`
- `ps5_lifecycle.c`
- `native-app/`
- `tools/build-ps5.sh`
- the performance rules

**Licensing:**
- Everything we write is **GPL-3.0-or-later**. Porting chessops and chessground semantics requires that anyway.
- Compatible third-party code: yyjson (MIT), qrcodegen (MIT), stb (PD/MIT), mbedTLS (Apache-2.0, fallback only).
- Compatible assets: the puzzle data (CC0) and the piece sets chosen in D11.
- `NOTICE.md` and the About screen list every one of them.

---

## 3. Findings that shape the plan

1. **HTTPS: system `sceHttp` is the only path proven inside a native app.**
   - ProsperoTV loaded 12,863 channels over HTTPS and streamed 21.7 MB of HLS with 0 errors (`psiptv/docs/STATUS.md:48-73`). ProsperoRadio ships with it.
   - They link the SDK stubs for `libSceHttp`, `libSceSsl` and `libSceNet`. No sysmodule load is needed, and `param.json` needs no special attribute.
   - **libcurl + PacBrew OpenSSL failed in a native app.** It returned `CURLE_FAILED_INIT` because the getaddrinfo thread failed with ENOMEM (psiptv `IPTV_PROBE=1`).
   - The boilerplate's own recipe suggests PacBrew OpenSSL, but no native app has proven it.
   - → **D5: sceHttp first. mbedTLS + ProsperoLight's socket shim is the fallback. No curl.**
2. **Six sceHttp behaviours Lichess needs are unproven on console:**
   - POST with a body
   - an `Authorization: Bearer` header
   - transparent chunked decoding
   - **prompt delivery of small reads on a sparse NDJSON stream** (every existing stream was continuous)
   - whether certificates are actually verified
   - receive timeouts longer than the 2–5 s used so far
   - → **M6 is a second hard gate**, with a probe build and a checklist oracle.
3. **No native app has ever accepted an inbound TCP connection.**
   - Every proven listener is a payload. The listen and accept wrappers exist in ProsperoLight's shim but have never been used.
   - Our sign-in wants a LAN callback listener. → Probe it in R6b, and keep a fallback that needs no listener.
4. **Lichess OAuth** (lila `modules/oauth`):
   - PKCE (S256) works with any unregistered `client_id`.
   - **An `http://<LAN-IP>:<port>/callback` redirect is accepted.** Lichess only shows a "Does not use a secure connection" note on the consent page.
   - There is no device-code flow and no refresh token.
   - The authorization code expires in **120 s**. Tokens last **12 months**.
   - A personal-token page can be pre-filled with scopes by URL.
5. **The official mobile app's websocket is closed to us.**
   - It needs an HMAC-signed bearer and the `web:mobile` scope, which only the official client can get.
   - A User-Agent starting `Lichess Mobile/` is treated as the official app.
   - → **We use the public Board API only (REST + NDJSON), with our own User-Agent.**
6. **Board API limits:**

   | Limit | Detail |
   |---|---|
   | Seek speeds | Rapid, classical or correspondence only (estimated time = limit + 40 × increment ≥ 480 s) |
   | Blitz | Only vs the AI or in direct challenges |
   | Bullet | Never |
   | Real-time seek | A request **held open** until paired. Closing it cancels the seek. The game ID arrives on the event stream. |
   | Concurrency | One seek per user; 5 seek POSTs per minute per IP |
   | Event stream | **One per token.** Opening a second closes the first. At most 30 opens per 10 minutes. Keep-alive every 7 s. |
   | General | "Only make one request at a time". After a 429, wait a full minute. |
   | Castling moves | Standard castling arrives as `e1g1` |

7. **Puzzle semantics differ between the API and the CSV.**
   - **API:** `solution[0]` is the player's move. `game.pgn` ends with the opponent's last move (length `initialPly + 1`). `daily` and `/{id}` also return `fen`, with the player to move.
   - **CSV:** `FEN` is the position *before* the opponent's move, and `Moves[0]` is that move.
   - For mate puzzles, any mating move counts as correct.
8. **Fair play.**
   - Engine assistance is forbidden in Board API games.
   - lila flags Board API accounts whose moves come from engine-assist referers.
   - The opening explorer now requires a token.
   - → **No engine, cloud eval, tablebase or explorer while any live game is in progress.** The same rule covers any future on-device engine.
9. **Asset licensing.**
   - Lichess's default "standard" sound set is **non-free**. The other sets are AGPL or NC.
   - lila's board images are AGPL.
   - Many piece sets are NC. **cburnett is GPLv2+** (and BSD via Wikimedia). merida is GPLv2+, chessnut Apache-2.0, fantasy/spatial/celtic MIT, rhosgfx CC0.
   - → D11–D13.
10. **The GL 4.6 first frame is the first risk** (ProsperoPuzzles finding 1).
    - Tenfold's only hardware launch failed its first swap with `EGL_BAD_SURFACE`.
    - Tenfold's `param.json` lacked the `kernel` and `amm` fields.
    - → M1 is a hard gate, as in ProsperoPuzzles.
11. **Chess needs textures** (a piece atlas), unlike ProsperoPuzzles v1. ps5-opengl's texture path is exercised by Quake II. We keep to a single mip level and linear filtering (Yamagi rules).
12. **C++ runtime:**
    - The boilerplate baseline has no exceptions, no RTTI and no full libc++.
    - The ps5-opengl recipe links `libc++.a` for Mesa, but `std::thread`, iostreams and locale remain unvalidated.
    - → Use pthreads directly. There are no streams, and errors are `Status` / `Result<T>`.

---

## 4. Decisions (defaults; flag any you want changed)

| # | Decision | Default | Why | Alternative |
|---|---|---|---|---|
| D1 | Input and audio | **Native `scePad` + `sceAudioOut`** (ProsperoLight) | Proven; no SDL | PacBrew SDL2 |
| D2 | Display | **1080p60 first; evaluate 4K60 in M11** (pieces baked at 128 and 256 px) | Lowest risk. Chess gains from sharpness, not from 120 Hz. | 4K from the start |
| D3 | UI technology | **Custom `gfx2d`** (ProsperoPuzzles D3) **plus a texture-atlas mode and a board shader** | One or two draw calls per frame; full control of the look | Dear ImGui, RmlUi |
| D4 | SDK | **Pin one SDK by its `manifest.sha256`** (chosen in R1) | — | — |
| D5 | HTTPS transport | **System `sceHttp`/`sceSsl`** behind a `net::Backend` interface | Proven in two native apps | Fallback: **mbedTLS 3.6.4 + ProsperoLight `ps5_sockets.c` + bundled Mozilla CA roots**, `VERIFY_REQUIRED`, time-date checks on. It may be used for streams only if only sparse streaming fails. **Not curl.** |
| D6 | Lichess protocol | **Public Board API over REST + NDJSON** | The mobile websocket is closed to third parties | — |
| D7 | Sign-in | **OAuth2 PKCE** with `client_id=prosperolichess`. A QR code on the TV; your phone approves; Lichess redirects the phone to `http://<ps5-ip>:<port>/callback` on the PS5. | No typing; Lichess accepts LAN redirects | Fallback if R6b fails: a QR code to the pre-filled **personal token** page, then type the token on our own on-screen keyboard |
| D8 | Scopes | `board:play puzzle:read puzzle:write challenge:read challenge:write` | Covers every v1 mode | + `follow:read` when friend challenges arrive |
| D9 | Chess core | **Our own C++20 bitboard library following chessops semantics**, perft-verified | Lichess-identical SAN, castling and outcome rules; no exceptions or allocation | Disservin `chess-library` (MIT), after an exceptions/allocation audit |
| D10 | JSON | **yyjson**, vendored; tolerant of unknown fields | C, no exceptions, fast | cJSON |
| D11 | Piece sets | **cburnett** (default), **merida**, **chessnut**, **fantasy**, **spatial**, **rhosgfx**. Baked offline into RGBA atlases. | All free and GPL-compatible | Add CC BY sets (kiwen-suwi, firi) with attribution. **No NC or non-free sets.** |
| D12 | Board looks | **Flat themes** (brown, blue, green, grey, purple) plus **procedural wood and marble shaders** | No AGPL images; looks premium at any resolution | — |
| D13 | Sounds | **Synthesized placeholders now; your final set later, per Appendix B** | lila's standard set is non-free | lila's AGPL sets (piano, nes, sfx, futuristic), with attribution, only if you accept AGPL assets |
| D14 | Repository | **`blackbearreloaded/ProsperoLichess`**, public, GPL-3.0-or-later, from the template, cloned to `~/ProsperoLichess` in WSL | Matches the other Prospero apps | Private |
| D15 | Title IDs | **Release PPSA99009, development PPSA99009.** Verify both are unregistered first. | Already used: 99001–99005, 99007, 99008, 99020, 99021, 99137, 99751, 99790, 99997, 99999, 88900. 99030/31 are reserved for ProsperoPuzzles. | — |
| D16 | Name and branding | **"ProsperoLichess — an unofficial lichess.org client."** No Lichess logo. Our own User-Agent: `ProsperoLichess/<ver> (PS5; +https://github.com/blackbearreloaded/ProsperoLichess)`. | Avoids implying it is official | — |
| D17 | Relation to ProsperoPuzzles | **Separate repo, same layer design.** `platform/`, `core/`, `gfx/`, `ui/` and `audio/` have no dependencies on chess or puzzle code, so a shared kit can be extracted later. Whichever project proves a layer on hardware first donates it. | Neither exists yet; avoids coupling two releases | Build a shared kit first |
| D18 | Engine | **None in v1.** Offline Stockfish is an M12 spike. It is hard-disabled while any online game is in progress. | Fair play; unvalidated libc++ threading; large NNUE files | Stockfish in v1 |
| D19 | Test opponents | **The Lichess AI** for automated runs, **you** for human games. **lila-docker** on the host for scripted two-player tests. | Lichess allows one account per person, so no second production account | A BOT account (your call, casual only) |
| D20 | Commits | One-line imperative subject, pushed to `main` as BlackBearReloaded, no trailers | Your standing instruction | — |

---

## 5. Architecture

### 5.1 Repository layout

```
ProsperoLichess/
  Makefile .env.example build.ps1 README.md PLAN.md NOTICE.md CHANGELOG.md LICENSE
  sce_sys/     param.json icon0.png pic0.dds pic1.dds snd0.at9 (+ sources)
  assets/      -> /app0/assets
    fonts/       inter-*.sdf (R8 + metrics)
    pieces/      <set>/{128,256}.rgba (+ shadow layer)
    sfx/         *.wav (48 kHz S16 stereo)
    puzzles/     pack.bin (bundled puzzles), themes.txt (CC0 theme names)
    certs/       roots.pem (only if the D5 fallback is used)
  src/
    main.cpp
    runtime/     app_heap.c, runtime_shims.c              (ps5-opengl native-app)
    platform/    platform.hpp  (Display, Pad, AudioOut, Storage, System, Clock, Net, Listener)
      ps5/       display_egl.cpp pad.cpp audio_out.cpp storage.cpp system.cpp
                 net_scehttp.cpp   net_info.cpp (sceNetCtl IP)   listener.cpp (sceNet accept)
    core/        app (scene stack, frame loop), mailbox (thread -> main events), input_map,
                 repeat, settings, save_container, save_worker, result/status, log (+ redaction), rng
    gfx/         batch2d, texture, font, tween, theme, board_shader
    ui/          widgets (panel, card, button, list, rows, dialog, toast, hint bar, keyboard, qr)
                 scenes (boot, home, settings, about, diagnostics, sign_in, profile, game_menu)
    audio/       mixer, synth, clips, cues
    chess/       square_set, attacks, position, movegen, fen, san, uci, pgn_mainline, zobrist, outcome, game
    board/       board_model (chessground-like state), board_view, board_input (cursor, smart
                 targeting, premove), promotion_picker, move_list_view, clock_view, player_card
    puzzles/     puzzle (shared controller), sources (api batch, daily, offline pack), pack_reader
    net/         http.hpp (Request/Response/Stream), ndjson, form/url encode, lanes, backoff, rate_gate
    lichess/     api (typed endpoints), models (yyjson), pkce (sha256, base64url), session,
                 event_stream, game_session, seek, puzzles_api, tv
    modes/       registry + daily_puzzle, puzzle_training, puzzle_themes, puzzle_streak,
                 puzzle_storm, quick_pairing, vs_ai, my_games, pass_and_play, tv, profile
    third_party/ yyjson/ qrcodegen/ sha256/ (mbedtls/ only for the fallback)
  host/        host backends: SDL2 window + keyboard, SDL/null audio, libcurl HTTP, POSIX listener
  tests/       GoogleTest host tests + fixtures/ (recorded NDJSON, JSON, redacted)
  tools/       bake-font.py bake-pieces.py build-puzzle-pack.py gen-sfx-placeholders.py
               deploy-verified.sh inspect-save.py record-fixture.sh net-probe-compare.py
  docs/        ARCHITECTURE.md OPENGL_INTEGRATION.md NETWORKING.md LICHESS_API.md CONTROLS.md
               HARDWARE_TESTING.md
```

### 5.2 Runtime model

**Threads** (all created with `pthread_create`; never `pthread_setname_np`):

| Thread | Job |
|---|---|
| **Main** | Drain the pad, drain the **mailbox** (network and save results), update scenes, build the batch, draw, swap. **It never blocks on I/O.** |
| **Audio** | Mix a 256-frame grain, then call the blocking `sceAudioOutOutput` |
| **Save** | Coalescing atomic writer (ProsperoPuzzles 5.6) |
| **Net lane: general** | Runs REST calls **one at a time**, as the Lichess guidance asks. Paused while a live game is in progress, except for calls the game needs. |
| **Net lane: game** | Moves, draw, takeback, resign and abort for the active game only, so a move never waits behind a slow request |
| **Stream workers** | One thread per open NDJSON stream: event (while signed in), game (while playing), seek (while seeking), TV (while watching). Each owns one sceHttp request and delivers parsed lines to the mailbox. |
| **Listener** | Exists only during sign-in; one connection at a time |

**Cancellation and reconnects**
- Main cancels a stream with `sceHttpAbortRequest` behind ProsperoTV's atomic guard. The worker then exits. Leaving a mode never waits on the network.
- Reconnect backoff is 1, 2, 4, 8, 16, then 30 s. **Any 429 pauses the general lane and all reconnects for 60 s** and shows a toast.
- The event stream respects the 30-opens-per-10-minutes limit with a local token bucket.

**Timeouts**
- Resolve, connect and send: 5 s.
- REST receive: 10 s.
- Stream receive: 25 s. Lichess keep-alive lines arrive every 7 s; the exact game-stream cadence is confirmed in R6a and R8. A timeout counts as a dead stream and triggers a reconnect.

**Display, exit and saving** follow ProsperoPuzzles 5.2:
- one EGL display, surface and context for the life of the process
- never return from `main`
- exit via `fflush(NULL)` then `sceSystemServiceLoadExec("exit")`
- save when something changes, never on quit

**Focus loss in live games:** the Options/PS-button focus loss **does not pause a live game**, because the server clock keeps running. The shell shows "Game in progress — your clock is running" and does not open a modal. Closing the app does not stop the clock either, so the game menu says so.

### 5.3 Mode interface

```cpp
namespace pc {
struct Services {                         // owned by the shell, lent to the active mode
    gfx::Batch& gfx; audio::Mixer& audio; SaveService& saves; const Settings& settings;
    const Clock& clock; lichess::Client& lichess; const NetStatus& net; Rng seed_source;
};
enum class Needs : uint8_t { none = 0, network = 1, account = 2 };
enum class SceneRequest { none, open_menu, exit_to_hub };

class ModeScene {
public:
    virtual ~ModeScene() = default;
    virtual void enter(Services&, const ModeArgs&) = 0;     // resume / game id / puzzle id
    virtual void update(const InputFrame&, double dt) = 0;
    virtual void on_event(const MailboxEvent&) = 0;        // network results for this mode
    virtual void draw(gfx::Batch&, const Rect& area) = 0;
    virtual SceneRequest request() = 0;
    virtual void menu_items(GameMenu&) = 0;                // Resign, Offer draw, Flip, Hint...
    virtual void on_menu_item(int id) = 0;
    virtual bool pausable() const = 0;                     // false during live online games
    virtual void persist() = 0;
};

struct ModeInfo {
    const char* id; const char* title; const char* subtitle; HubRow row; Needs needs;
    gfx::Color accent;
    void (*draw_card)(gfx::Batch&, Rect, const CardState&, float t);   // live mini board / stat
    std::unique_ptr<ModeScene> (*create)();
};
std::span<const ModeInfo> registry();     // static table; unique ids (tested)
}
```

**Gating:** a card whose `needs` aren't met shows a chip ("Sign in" or "Offline"). Confirming it opens the sign-in flow or a network-status dialog instead of the mode.

### 5.4 Input model

**Shell mapping:** as in ProsperoPuzzles 5.4:
- button bits from ProsperoLight
- left stick as a D-pad (deadzone 64/192)
- repeat after 350 ms, then every 110 ms
- a Cross/Circle swap setting
- `focus_lost` on intercept or disconnect

**Board mapping:** everything is reachable by D-pad plus Cross, and the rest are shortcuts. A controller is slower than a mouse, which suits the Board API's rapid-or-slower rule. Premoves and smart targeting close most of the gap.

| Input | No piece selected | Piece selected | Other context |
|---|---|---|---|
| D-pad / left stick | Move the cursor one square (with repeat) | Same | — |
| **Right stick flick** | Jump to the movable piece nearest the flick direction | **Jump to the legal destination nearest the flick direction** | — |
| **R1 / L1** | Cycle through movable pieces | Cycle through legal destinations | — |
| **Cross** | Pick up the piece under the cursor | Move there (or re-select another own piece) | — |
| **Circle** | — | Cancel the selection | Also cancels a queued premove |
| **L2 / R2** | Step back / forward through the move history | — | Cross returns to the live position. It auto-returns when it becomes your turn. |
| **Triangle** | — | — | Puzzles: hint (loses the rated result). Pass & Play: undo. |
| **Square** | — | — | Flip the board (TV, puzzles, Pass & Play). Live games: toggle the info panel. |
| **Options** / touchpad click | — | — | Game menu: offer draw, takeback, resign or abort, flip, sound, return to hub |

- **Promotion:** a Lichess-style picker fans out over the target square. Left/right chooses; Cross confirms. The **auto-queen** setting is on by default.
- **Premoves:** the "select, then target" flow also works on the opponent's turn. It queues a premove with a distinct highlight. The game session sends it as soon as it is legal on our turn.

### 5.5 Rendering

**Base:** `gfx2d` exactly as ProsperoPuzzles 5.5:
- a virtual 1920×1080 canvas
- one instanced program with triangle lists only
- a stream VBO orphaned every frame
- cached uniform locations
- one clear
- shader sources without `#version`

**Additions for chess**
- **Texture atlas mode.** The program binds two samplers, the R8 SDF font atlas and the RGBA8 piece atlas (premultiplied alpha, one mip level, linear filtering). The instance `mode` field chooses which to sample, so it stays **one draw call**.
  - `tools/bake-pieces.py` rasterizes each set's 12 SVGs with `rsvg-convert` on the host, at **128 px** (1080p squares are 120 px) and **256 px** (4K squares are 240 px). It also bakes a blurred silhouette as a shadow layer.
  - Output is raw `.rgba` with a small header, so the console needs no image decoder.
  - At runtime the atlas is chosen from the physical square size.
- **Board shader mode.** One instance covers the whole board. The fragment shader computes the checker pattern from the UVs, the theme colours and an optional procedural texture (wood grain from layered value noise, marble from domain-warped noise), plus a subtle vignette and a rounded frame.
  - Coordinates are drawn as glyphs inside the edge squares, Lichess style.
- **Highlights and overlays** use existing instance modes:

  | Overlay | Drawn as |
  |---|---|
  | Last move | Tinted squares |
  | Selected piece | Tinted square |
  | Legal destination | Dot |
  | Capture destination | Ring |
  | Check | Radial red gradient |
  | Premove | Desaturated blue |
  | Cursor | Animated glow ring |
  | Hint arrows | Rect plus triangle |

- **Piece motion**
  - Moves tween over 180 ms (cubic out). A captured piece fades.
  - Castling animates both pieces.
  - A lifted piece scales to 1.08× and its shadow grows.
  - Flipping the board cross-fades.
  - Reduced motion makes all of these instant.
- **Game screen layout** (virtual 1920×1080):
  - The board is 960×960 at the left of centre.
  - The right panel, 700 px wide, holds:
    - the opponent card (name, title badge, rating, clock)
    - the material difference
    - the figurine move list (piece icons from the atlas, 2 columns, auto-scroll)
    - a status line (offers, "Your turn", `opponentGone` countdown)
    - your card and clock
  - The hint bar runs along the bottom.
- **Performance target:** 60 fps with main-thread CPU time under 4 ms. The board plus the UI is about 400 instances, far below the 5,000 in the M3 check.

### 5.6 Chess core (`src/chess/`)

**Scope:** standard chess, with 960-capable castling internally so variants can be added later without a rewrite.

**Representation**
- A `SquareSet` (u64) type.
- Attacks by hyperbola quintessence, or by magic bitboards if perft timing needs it.
- A `Position` holds these fields:
  - `board` (role and colour square sets)
  - `turn`
  - `castles` (as rook squares)
  - `ep_square`
  - `halfmoves` and `fullmoves`
  - `zobrist`
- `Position` is immutable: `play(Move)` returns a copy. It is about 100 bytes and needs no allocation.

**API**
- `from_fen` and `to_fen`
- `legal_moves(MoveList&)`: a fixed array of 256
- `is_legal`, `play`, `is_check`
- `outcome()`: checkmate, stalemate or insufficient material. Fifty-move and threefold are tracked by `Game`.
- `parse_uci`: accepts both `e1g1` and king-takes-rook `e1h1`. `to_uci` emits `e1g1` for standard games (Lichess's form).
- `to_san` / `parse_san`: disambiguation, `+` and `#`, `=Q`, and `O-O`/`O-O-O`.
- `pgn_mainline`: SAN tokens only. It covers the puzzle `game.pgn`, which has no headers.

**`Game`**
- The initial position, the move list, and the positions after each ply (for history browsing).
- A repetition table keyed by Zobrist hash.
- `apply_uci_list(string_view)` for Board API `gameState.moves`. It is incremental: the new list must extend the known prefix, or the game resyncs from scratch.

**Errors:** `Result<T>` with an error enum. No exceptions and no heap use in the hot paths.

### 5.7 Board widget (chessground parity list)

**Implemented in v1:**
- orientation and flip
- click-to-move via the cursor (drag has no controller equivalent)
- move animation with capture fade
- last-move and check highlights
- legal destinations (`showDests`)
- premoves, including castle premoves, cancelled with Circle
- auto-castle display
- coordinates
- a view-only mode (TV and history)
- hint arrows and circles (puzzles), snapped to legal moves
- a promotion picker
- blindfold, as a lila-style UI option that hides pieces

**Not in v1:** the board editor, free drawing of shapes, and predrops (no Crazyhouse).

**Board model:** a pure `BoardModel` holds position, orientation, selection, destinations, premove, highlights, shapes and animation state. It is updated by modes, and `BoardView` renders it. Host tests drive `BoardModel` + `BoardInput` with scripted `InputFrame`s.

### 5.8 Networking layer (`src/net/`, `platform/*/net_*`)

```cpp
namespace net {
struct Request  { Method method; Url url; HeaderList headers; ByteSpan body; Timeouts t; };
struct Response { int status; HeaderList headers; Buffer body; Status transport; };
class Stream {                                // a long-lived NDJSON response
public:  virtual Status read_some(MutableByteSpan, size_t* got) = 0;   // blocks; honours recv timeout
         virtual void abort() = 0;             // thread-safe; unblocks read_some
};
class Backend {
public:  virtual Result<Response> perform(const Request&) = 0;
         virtual Result<std::unique_ptr<Stream>> open_stream(const Request&) = 0;
};
}
```

**The PS5 `sceHttp` backend** (ProsperoTV pattern):
- Init runs once: `sceNetPoolCreate` (1 MiB), `sceSslInit` (304 KiB), `sceHttpInit` (1 MiB), then one template with HTTP/1.1, auto-proxy, auto-redirect **off**, and the timeouts from section 5.2.
- **Connections:**
  - REST uses keep-alive connections: `CreateConnectionWithURL(...,1)`, as ProsperoRadio does.
  - Streams use a dedicated connection with keep-alive off.
- **Requests:**
  - POST uses `CreateRequestWithURL(conn, POST, url, len)` then `sceHttpSendRequest(req, body, len)`.
  - Headers are added with `sceHttpAddRequestHeader`: `Authorization`, `Accept`, `Content-Type` and `User-Agent`.
- **Receive block size:** `RecvBlockSize` is set small for streams (1 KiB). **R6a proves** that reads return promptly.
- **Certificate verification must be proven on** in R6a. If it isn't on by default, enable it with `sceHttpsEnableOption`. If it can't be enabled, fall back as D5 describes.

**Other backends**
- **Host backend:** libcurl, with the same interface, so `make host-run` talks to real Lichess from WSL.
- **Fallback PS5 backend:** mbedTLS 3.6.4, built with ProsperoLight's config but with `MBEDTLS_HAVE_TIME_DATE` re-enabled and `VERIFY_REQUIRED` set. It runs over `ps5_sockets.c`, with its resolver, epoll-based `poll` and `FIONBIO`. The CA bundle is shipped in `assets/certs`, and our own small HTTP/1.1 client does chunked decoding.

**Shared pieces**
- **NDJSON splitter:** handles arbitrary fragmentation, skips empty keep-alive lines (but counts them as liveness), and caps line length at 256 KiB.
- **Log redaction:** the `Authorization` header, `code=`, `code_verifier=`, `access_token` and anything matching `lip_[A-Za-z0-9]+` never reach klog or `app.log`.
- **Referer:** we never send one (finding 8).

### 5.9 Lichess client layer (`src/lichess/`)

**`Client`**
- Holds the base URL. It defaults to `https://lichess.org`. The dev title may override it from `/download0/prosperolichess/dev.cfg` to point at lila-docker. Release builds ignore that file.
- Also holds the session (token and account), the two lanes and the rate gate.
- Typed calls return through the mailbox as tagged events. The endpoint list is in Appendix A.

**`EventStream`** is always on while signed in:
- `gameStart`: if we started it (a seek or AI challenge), go to the game. Otherwise, add it to My Games and show a toast.
- `gameFinish`: refresh My Games.
- `challenge`: show a toast plus a "Challenges" list. Accept or decline from the game menu.
- `challengeCanceled` and `challengeDeclined`: update the list.

**`GameSession`** (one per open game):
- **Stream handling**
  - Stream `/api/board/game/stream/{id}`. The first line is `gameFull`.
  - Apply `gameState.moves` incrementally (section 5.6). A new opponent move animates and plays a sound.
- **Clock model**
  - Each `gameState` records `wtime`/`btime`, `winc`/`binc` and the monotonic receive time.
  - The side to move counts down locally.
  - After we move, our clock freezes at the shown value and the opponent's starts. The next `gameState` corrects both.
  - `Int.MaxValue` means there is no clock.
- **Our moves**
  - Our move is applied **optimistically** and then POSTed on the game lane.
  - On a 4xx, revert with a shake animation and an "illegal/refused" sound, then resync from the next `gameState`.
- **Premove:** on each `gameState` where it is our turn, if a premove is queued and legal, send it immediately.
- **Offers**
  - `wdraw`/`bdraw` and `wtakeback`/`btakeback` open a non-modal prompt: Cross accepts, Circle declines (`/draw/yes|no`, `/takeback/yes|no`).
  - The game menu offers a draw or asks for a takeback.
  - A move can carry `offeringDraw=true`.
- **Ending the game**
  - Abort is available before both sides have moved; after that it is resign, with a confirmation.
  - `opponentGone` shows a countdown. "Claim victory" and "Claim draw" become available when `claimWinInSeconds` reaches 0.
  - When the status is terminal, show the result dialog. It offers "New opponent" (same seek), "Return to hub" and "Board" (review the game).
- **Reconnect:** a dropped stream reconnects, and the fresh `gameFull` resyncs everything.
- **Ignored in v1:** `chatLine` (chat is out of scope).

**`Seek`** (Quick Pairing):
- Presets are **10+0, 10+5, 15+10, 30+0, 30+20** and correspondence 1, 3 or 7 days, each rated or casual.
- The seek request is held open on a stream worker; the "Searching…" screen shows the elapsed time.
- Circle aborts the request, which cancels the seek.
- The game ID arrives by `gameStart` on the event stream, **so the event stream must be open before the seek starts**.
- If Lichess says "You must also play some games as <color>", show it verbatim.

**`VsAi`**
- Level 1–8; colour white, black or random.
- Clocks 5+3, 10+0, 10+5, 15+10, 30+0 or correspondence. Blitz is allowed vs the AI.
- `POST /api/challenge/ai` returns the game ID directly, then `GameSession` takes over.

**`MyGames`:** `GET /api/account/playing` plus event-stream updates. Each entry shows a mini board, the opponent, "your turn" and the time left. Correspondence games work the same way, with an optional "confirm move" setting.

### 5.10 Sign-in

**Primary: PKCE with a LAN callback** (needs R6b)
1. Get the PS5's LAN IPv4 from `sceNetCtlGetInfo`. The fallback is the address of a UDP socket "connected" to a public IP.
2. Open the listener on a random port between 49152 and 65535, retrying up to 3 ports.
3. Generate `code_verifier`: 64 characters from a CSPRNG seeded from `kern.rng_pseudo`. Also generate `state`: 16 random bytes, hex.
4. Show a **QR code** (qrcodegen, ECC M) of:
   ```
   https://lichess.org/oauth?response_type=code&client_id=prosperolichess
     &redirect_uri=http://<ip>:<port>/callback&code_challenge_method=S256
     &code_challenge=<b64url(sha256(verifier))>&scope=<D8>&state=<state>
   ```
   The screen shows the steps, the shortened URL, and a 5-minute countdown. It also warns that the phone must be on the same network and that Lichess will say the connection is "not secure" because it is a local address.
5. The phone signs in and approves. Lichess redirects it to `GET /callback?code=…&state=…`.
6. The listener **checks `state`**, replies with a small "Signed in — you can put your phone away" HTML page, then closes.
   - A wrong `state` or a wrong path gets a 404, and the listener keeps waiting.
   - `error=access_denied` shows "Cancelled".
7. Immediately (the code expires in 120 s), `POST /api/token` with the code, verifier, redirect URI and client ID.
8. `GET /api/account`, then store `account.bin`, start the event stream, and show a "Signed in as X" toast.

**Fallback: personal token** (used if R6b fails, or chosen by you)
- A QR code opens `https://lichess.org/account/oauth/token/create?scopes[]=board:play&…&description=ProsperoLichess%20PS5`.
- You create the token on your phone, then type it on **our own on-screen keyboard**. It is an `A–Z a–z 0–9 _` grid with the `lip_` prefix pre-filled.
- `POST /api/token/test` verifies the user and scopes before saving.

**Token lifetime**
- **Sign out:** `DELETE /api/token`, then wipe `account.bin`.
- Any 401 on an authenticated call puts the app in the signed-out state and shows a toast.
- A PKCE token (12 months) triggers a "re-sign-in soon" prompt 14 days before it expires. A personal token does not expire.

**Listener hardening:**
- It exists only during the flow, for at most 5 minutes.
- It handles one connection at a time, with a 4 KiB request cap.
- It serves only `GET /callback`.
- It never logs the query string.

### 5.11 Puzzles

**One `PuzzleController` serves every source.**
- **Input:** a start position (the player to move), the opponent's last move (for the highlight), the solution as UCI, and the rating and themes.
- **Playback**
  - Before the first move, the opponent's last move is replayed with a 500 ms animation, so the position makes sense.
  - A correct move plays a sound, then the scripted reply follows after 400 ms.
  - **Any checkmating move is accepted** at a mating step.
  - A wrong move shows a red flash and an "incorrect" sound. It counts as a fail, but you may continue ("View solution" or "Try again").
- **Hints:** a hint highlights the piece first, then draws an arrow. Using one makes the result unrated.

**Sources**
- **API** (`game.pgn` + `initialPly`, or `fen` when present):
  - Replay the PGN to get the start position.
  - `solution[0]` is the player's move.
  - Signed in, a batch of 15 comes from `GET /api/puzzle/batch/{angle}?nb=15&difficulty=…`.
  - Each result goes to `POST /api/puzzle/batch/{angle}?nb=0` with `{"solutions":[{"id","win","rated"}]}`, and the response's `glicko` and `ratingDiff` update the rating display.
  - The batch is refilled when 3 puzzles remain.
  - Anonymous play uses batch GET only (it is cheaper on rate limits than `/next`).
- **Daily:** `GET /api/puzzle/daily`, cached per UTC date in `cache/daily.json`.
- **Offline pack** (CSV semantics):
  - Apply `Moves[0]` to `FEN` to get the start position; the rest of `Moves` is the solution.

**The offline pack** (`tools/build-puzzle-pack.py`, run on the dev machine):
- **Input:** `lichess_db_puzzle.csv.zst` (about 290 MiB, 6.1 M puzzles, CC0), downloaded locally and never committed.
- **Selection:**
  - Popularity ≥ 80, NbPlays ≥ 300, RatingDeviation ≤ 90.
  - Stratified into 50-point rating buckets from 400 to 3000, **about 100 k puzzles** in total.
  - Topped up so every theme has at least 200 puzzles.
- **Record format:**

  | Field | Size |
  |---|---|
  | id | 5 bytes |
  | board | 32 bytes (nibbles) |
  | turn, castling, en passant | 3 bytes |
  | move count | 1 byte |
  | moves | 2 bytes each (6+6 bits plus promotion) |
  | rating | u16 |
  | popularity | i8 |
  | theme bitmask | u64 |

- A header carries the source date, the source SHA-256, the rating-bucket index and the theme-name table.
- The pack is about 6 MB and is committed together with its source hash.
- **Offline modes:**
  - **Themes:** choose a theme, then a rating band.
  - **Streak:** the ladder climbs 50 points per solve, starting at 1000. One mistake ends the run, and you get one skip.
  - **Storm:** starts at 3:00. Each solve adds time on combos, each mistake costs 10 s, and the difficulty ramps up.
  - A per-mode "seen" bitmap avoids repeats.
  - Records are kept in `stats.bin`.

### 5.12 Storage

The root is `/download0/prosperolichess/`, with `downloadDataSize` set to 256. At boot we `mkdir` with mode 0700 and run a write probe (Yamagi). The container format, atomic writes and save thread are as in ProsperoPuzzles 5.6, with the magic `'PCHS'`.

| File | Contents |
|---|---|
| `settings.bin` | Board theme, piece set, coordinates, legal dots, last-move highlight, animation speed, premoves, auto-queen, confirm resign, correspondence move confirmation, clock tenths, Cross/Circle swap, volumes, reduced motion, blindfold, last hub focus |
| `account.bin` | Token, token kind (PKCE or personal), expiry, username, scopes. **Stored unencrypted, like every console homebrew secret today.** Sign-out deletes it. |
| `stats.bin` | Streak and Storm bests, offline puzzles solved per theme, seen bitmaps, Pass & Play game in progress |
| `cache/daily.json` | The last daily puzzle |
| `app.log` | stdout and stderr through `runtime_shims.c`, redacted |
| `dev.cfg` | Dev title only: base URL override, autoplay hook, extra logging |

`tools/inspect-save.py` decodes every one of these, but **prints only the token's prefix and length**.

### 5.13 Audio

The mixer, synth, clips and SPSC command ring are as in ProsperoPuzzles 5.7: 16 voices, master/music/SFX buses, soft clip, and silence on underrun.

**Cues** (Appendix B):
- `move`, `capture`, `check`, `castle`, `promote`
- `illegal`
- `game_start`, `victory`, `defeat`, `draw`
- `low_time`, `countdown_tick`
- `puzzle_correct`, `puzzle_wrong`, `puzzle_solved`, `combo`
- `notify` (challenge, draw offer, `opponentGone`)
- `ui_move`, `ui_confirm`, `ui_back`

`tools/gen-sfx-placeholders.py` synthesizes wood-click placeholders until your set arrives.

### 5.14 Shell UX

**Boot**
- The wordmark is the first frame; then `HideSplashScreen` and a 1.5 s fade (ProsperoPuzzles).
- Network init, the account load and the event stream all start **after** the first frame. ProsperoLight froze at startup on a synchronous connect with no deadline, so nothing here may block the first frame.

**Home hub**
- **Top bar:** the wordmark; an account chip (username with rapid and puzzle ratings, or "Sign in"); a network dot; a challenge badge; the time.
- **Three rows:** Puzzles, Play and Watch. D-pad up/down moves between rows and left/right between cards; Cross opens a card.
- **Hero area:** a large live preview of the focused card:

  | Focused card | Hero shows |
  |---|---|
  | Daily Puzzle | Its position |
  | Lichess TV | The featured game, **updating live** |
  | My Games | The position in your most urgent game |
  | Streak | Your best run |

- An ongoing game triggers a **"Resume game"** banner that takes priority on the hub.
- Cards animate as in ProsperoPuzzles: scale spring, shadow, accent-tinted background.

**Other screens**
- **Game menu:** see 5.4. Live games show "Your clock keeps running" in the header.
- **Settings sections:** Board, Gameplay, Controls, Audio, Account (sign in or out, token expiry), Accessibility, About.
- **About:** version, credits, the unofficial-client notice, and the full license list (section 2, Appendix C).
- **Diagnostics:** live buttons, a tone test, FPS and frame-time histograms, heap statistics, and a **network panel** (backend, last errors, open streams, 429 pause state, keep-alive ages).
- **Error UX**
  - Offline: cards needing the network dim, and a toast shows a retry.
  - Signed out: cards needing an account dim.
  - A 429 shows a "Lichess asked us to slow down" toast with a countdown.
  - A 5xx retries with backoff.

---

## 6. Milestones

**Dependencies:** M0 → M1 (hard gate) → M2 → M3 → M5 → M6 (hard gate 2) → M7 → M8 → M9 → M10 → M11.
- **M4 (chess core) has no console dependency** and runs in parallel from M0.
- Console runs (R*n*) need your go-ahead, because a build request does not authorize a launch. I'll ask before each run unless you pre-authorize the list.

### M0: Bootstrap (offline)
1. `gh repo create blackbearreloaded/ProsperoLichess --template blackbearreloaded/ps5-native-app-boilerplate --public --clone` in WSL. Record the template HEAD.
2. `make init TITLE_ID=PPSA99009 APP_NAME="ProsperoLichess" CONTENT_SUFFIX=PROSPEROLICHESS1`.
   - Check the content ID against the validator's regex.
   - Check that PPSA99009 and 99041 are unregistered (a console listing, done in R1's preflight).
3. `make doctor deps test app` unchanged.
4. Adapt the lint header rule to "ProsperoLichess". Remove the demo renderer and its test.
5. Vendor `third_party/{yyjson, qrcodegen, sha256}` at pinned versions, each with its license file.
6. Commit stub `README.md`, `NOTICE.md` and `CHANGELOG.md`; this plan as `PLAN.md`; and `docs/LICHESS_API.md` (spec v2.0.174 plus Appendix A).
7. Create `.local/ENVIRONMENT.md` from the runbook template.

**A0:** `make lint test app` passes in WSL; CI host jobs pass.

### M1: GL 4.6 first frame (hard gate)
These are ProsperoPuzzles M1 steps 1–7 unchanged:
- `tools/fetch-opengl-sdk.sh` with the manifest-hash pin
- `tools/prepare-opengl.sh`, which every packaging target depends on
- the tooling changes: module heap `0x10000000`, `--wrap` for the malloc family, `--eh-frame-hdr`, stub AGC libraries, `ps5-pie.ld` PROVIDEs, `app-symbols.map`, and the **`param.json` `kernel`/`amm` fields**
- `runtime/` from ps5-opengl
- `display_egl.cpp` from Yamagi
- a spike `main.cpp`
- the template's presentation assets kept

| Run | Question | Oracle |
|---|---|---|
| **R1** | Does the candidate SDK present on this console? | ps5-opengl's own GL 4.6 demo runs. May be skipped if ProsperoPuzzles R1 already passed with the **identical SDK manifest hash and firmware**. |
| **R2** | Does our integration present? | `[PCH] first-swap ok`; ≥ 3,600 swaps in 60 s; screenshot of the test pattern; clean close with runtime layers released |

If R2 fails while R1 passes, diff `dist/` against ps5-opengl's staged app before changing anything else. No blind retries.

**A1:** R2 passes. The SDK manifest hash and `eboot.bin` hash are recorded.

### M2: Platform services and the host runner
These are ProsperoPuzzles M2:
- `pad.cpp`
- `input_map` and `repeat`
- `audio_out.cpp` and the mixer
- `storage.cpp`, the save container and the save worker
- `system.cpp` (splash, exit, klog, toast, clock)
- the diagnostics scene
- the host runner (`make host-run` under WSLg, `make host-snapshots` via llvmpipe)

Additions:
- the `core/mailbox`
- `log` with redaction and its tests

**R3:** button edges are logged, audio is healthy (0 underruns), and a boot counter persists across two cycles.

**A2:** R3 passes, and the host tests pass.

### M3: `gfx2d`, textures, fonts, UI toolkit, board rendering
1. `gfx2d` as in ProsperoPuzzles M3, **plus the texture-atlas sampling mode and the board shader** (flat, wood, marble).
2. `tools/bake-font.py` (Inter, OFL) and `tools/bake-pieces.py` (D11 sets, 128 and 256 px, with shadow layer), plus `gfx/texture` (upload once, then immutable).
3. The UI widgets from ProsperoPuzzles M3, plus the **on-screen keyboard** and the **QR widget**.
4. `BoardView` rendering a static FEN with every highlight type, arrows and coordinates, in both orientations.
5. **Host tests:** layout math, text wrapping, and a widget and board gallery snapshot per theme and piece set.

**R4:** a gallery with a board and 5,000 instances holds 60 fps with main-thread CPU time under 4 ms. A native screenshot shows the pieces crisp and correctly alpha-blended on every board theme.

**A3:** the snapshots are reviewed by you, and R4 passes.

### M4: Chess core (host only; in parallel from M0)
1. `square_set`, `attacks`, `position`, `movegen` (with pins, checks, en passant with a discovered check, and castling through check), `fen`, `uci`, `san`, `pgn_mainline`, `zobrist`, `outcome`, `game`.
2. **Tests:**
   - **Perft:** the standard suite (start position to depth 5 = 4,865,609; Kiwipete; positions 3–6), in the release test config.
   - **Corpus:** 20,000 puzzles sampled from the CSV. Check FEN round-trips; that each UCI move is legal; SAN out and back in giving the same move; and that the final position of every mate puzzle is checkmate.
   - The RFC-style SAN edge cases: disambiguation by file, by rank and by both; promotion with capture; `O-O` with check.
   - Threefold repetition, the fifty-move rule, and insufficient material (including same-colour bishops).
   - Board API move-list replay, both incremental and resync.
   - Fuzzing `from_fen` and `parse_san` (libFuzzer on the host, 10 minutes, in CI nightly).

**A4:** all tests are green under ASan and UBSan. Perft to depth 5 from the start position takes under 1 s on the host in release.

### M5: Board widget, shell and offline modes: first playable chess on PS5
1. `BoardModel`, `BoardInput` (cursor, smart targeting, R1/L1 cycling, history stepping), the promotion picker, the move list, `clock_view` and `player_card`.
2. The shell: `core/app` scene stack, the Boot, Home hub, Settings, About and game-menu scenes, and the mode registry with gating.
3. `tools/build-puzzle-pack.py` + `pack_reader` + `PuzzleController`.
4. Modes: **Pass & Play** (with undo; the game in progress persists), **Puzzle Themes**, **Puzzle Streak**, **Puzzle Storm**.
5. Placeholder sounds (`tools/gen-sfx-placeholders.py`) wired to the cues.
6. **Host tests:**
   - board input scripts (select, move, cancel, promotion, flip, history)
   - `PuzzleController` against pack records, including accepting an alternative mate
   - Streak and Storm rules with a fake clock
   - the scene flow: Hub → mode → menu → Hub → Settings → Hub
   - registry invariants

**R5:** in Pass & Play, play the scripted fool's mate (you, or approved input injection). Then solve 3 offline puzzles, return to the hub, close, relaunch, and check that the Streak best and the Pass & Play game persist.
- **Oracle:** klog scene markers, a `[PCH] outcome checkmate` marker, the extracted `stats.bin` decoded by `inspect-save.py`, and a screenshot.

**A5:** R5 passes. **Chess is playable on the PS5, offline.**

### M6: Network transport (hard gate 2)
1. `net::Backend` + the `sceHttp` backend + the NDJSON splitter + lanes + stream workers + abort + backoff + the rate gate. The host libcurl backend too.
2. `platform/ps5/listener.cpp` (sceNet socket, bind, listen, accept, from ProsperoLight's shim) and `net_info.cpp` (`sceNetCtlGetInfo`).
3. **Probe build** (`PCH_NET_PROBE=1`, dev title): runs the checklist below at boot and writes a JSONL receipt to `app.log` and klog.
4. **Host tests:** NDJSON fragmentation fuzz, chunked-boundary cases, URL and form encoding, backoff and the 429 gate with a fake clock, and redaction.

**R6a** (outbound): the probe runs each check, and every item must pass.

| # | Check | Pass |
|---|---|---|
| 1 | `GET https://lichess.org/api/puzzle/daily` | 200; JSON parses; puzzle ID logged |
| 2 | `POST https://lichess.org/api/token/test` with the form body `notatoken` | 200 and `{"notatoken":null}`, which proves the POST body and content type |
| 3 | `GET https://httpbin.org/headers` with `Authorization: Bearer probe-not-a-token` and our UA | The echo contains both headers |
| 4 | `GET https://lichess.org/api/tv/feed` for 180 s. At the same time, WSL runs `curl -N` on the same feed. | Every message parses. **Each message arrives as its own delivery**; comparing inter-arrival intervals with the WSL run (`tools/net-probe-compare.py`, matched by FEN) gives a median gap under 250 ms and no message later than 1 s. No receive timeouts with the 25 s setting. |
| 5 | `https://expired.badssl.com/`, `https://wrong.host.badssl.com/`, `https://self-signed.badssl.com/` | **All three fail** with TLS errors, while lichess.org succeeds |
| 6 | Open the TV feed, then abort from main after 5 s | The worker exits within 500 ms |
| 7 | 50 sequential REST calls on a keep-alive connection | Net-pool statistics return to baseline; no errors |

**R6b** (inbound):
- The probe logs the LAN IP from `sceNetCtl` and listens on a port.
- WSL runs `curl http://192.168.4.40:<port>/probe` and must receive `ok`. The app logs the request line.
- After the app closes the listener, a second curl is refused.

**Decisions after M6**
- Items 4, 5 or 6 fail → implement the D5 fallback backend and re-run R6a against it. A mixed setup is allowed: sceHttp for REST, mbedTLS for streams.
- R6b fails → the D7 fallback (personal token + keyboard) becomes the only sign-in path.

**A6:** R6a passes on the selected backend. R6b's outcome is recorded and D7 is settled.

### M7: Anonymous online modes
1. The `lichess::Client` base, the models, and the general lane.
2. **Daily Puzzle**, and **Puzzle Training** anonymous (batch GET; a theme and difficulty picker).
3. **Lichess TV:**
   - the channel list
   - a full-screen live board with players, ratings and clocks from `wc`/`bc`
   - Square flips the board
   - R1/L1 switch channels
   - the live **hero preview on Home**
4. Network status, the offline and 429 UX, and the daily-puzzle cache.
5. **Host tests** on recorded fixtures (`tools/record-fixture.sh` captures real responses from WSL): daily, batch and TV feed parsing, and the TV board following a recorded feed.

**R7:** solve today's Daily Puzzle, then watch TV for 2 minutes and switch channels twice.
- **Oracle:** klog markers for each fetch and stream (open, first message, close), a screenshot of the TV board, and the TV stream closed after returning to the hub (worker exit logged).

**A7:** R7 passes.

### M8: Sign-in
1. The PKCE helpers (tested against the RFC 7636 Appendix B vector), the sign-in scene with the QR code, the listener flow, the token exchange, `/api/account`, `account.bin`, sign-out with revoke, and 401 handling.
2. The personal-token fallback with the on-screen keyboard and `/api/token/test`.
3. The always-on `EventStream`, with reconnect and the token bucket.
4. **Host tests:**
   - PKCE vectors
   - listener request parsing (valid, wrong `state`, wrong path, oversize, `error=`)
   - the sign-in state machine with a fake backend
   - event-stream parsing from fixtures

**R8** (you take part): **you scan the QR code with your phone and approve.**
- **Oracle:**
  - the `[PCH] signed-in` marker (username only)
  - event-stream keep-alive ages logged about every 7 s for 60 s
  - after a close and relaunch, the app is signed in without a prompt
  - signing out logs `DELETE /api/token` returning 204
  - a second relaunch shows the app signed out

**A8:** R8 passes. The primary sign-in path, or the fallback if R6b failed, works end to end.

### M9: Rated puzzles and profile
1. Signed-in Puzzle Training: batch GET/POST, rated and unrated handling (hints), rating display and deltas, and a refill policy.
2. **Profile:** ratings for each speed, puzzle rating, the puzzle dashboard (30 days), and the storm dashboard (public).
3. **Host tests:** batch POST body building, response handling (`glicko`, `rounds`), and refill timing.

**R9:** solve 5 rated puzzles.
- **Oracle:** 5 POST batch responses logged with their `ratingDiff`s, and the Profile puzzle rating matching the last `glicko`.

**A9:** R9 passes.

### M10: Online play
1. `GameSession`, the clock model, optimistic moves, premoves, offers, abort and resign, `opponentGone` and claims, the result dialog, and reconnect/resync.
2. **Play vs Computer**, **Quick Pairing** (the seek stream and cancel), **My Games** (including correspondence), and incoming **challenges**.
3. The fair-play guard: a single `LiveGameGuard` flag that the future analysis, engine and explorer features must check (unit tested).
4. **Debug autoplay hook** (dev title and `dev.cfg` only; compiled out of release): plays the first legal move each turn, and resigns after N moves.
5. **Host tests**
   - The `GameSession` state machine against recorded and hand-written NDJSON:
     - normal moves
     - our move refused
     - premove firing
     - draw and takeback offers, both ways
     - `opponentGone`, then a claim
     - every terminal status
     - a mid-game stream drop, then a `gameFull` resync
     - `Int.MaxValue` clocks
     - correspondence `daysPerTurn`
   - Clock display error under 50 ms against the fixture timestamps.
6. **Optional host integration:** lila-docker on WSL, with the host app playing a scripted Board API opponent on local accounts. This needs no production accounts.

**Hardware runs**

| Run | What | Oracle |
|---|---|---|
| **R10a** (automated) | vs AI level 1, **casual, 10+5**, autoplay makes 12 moves and then resigns | The app log shows `gameFull`, then each `gameState` and each move POST returning 200, then `status: resign`. WSL fetches the public `https://lichess.org/game/export/<id>`; **its moves and result match the app log exactly.** |
| **R10b** (you play) | One Quick Pairing game, casual rapid 10+0, against a real opponent. Use at least one premove and one draw offer or takeback if the game allows. | The export matches the log with no desync markers. The clock display, sampled at each `gameState`, is within 1 s of the server values. |
| **R10c** (resilience) | vs AI level 1, 15+10: close the app mid-game (title-aware close), relaunch, and resume from My Games | The resumed position equals the server export. The clocks resync. The game completes. |

**A10:** R10a–c pass. **The end goal is met: native online chess on the PS5.**

### M11: Polish and release
1. **Art:** `icon0.png`, and `pic0`/`pic1` DDS and `snd0.at9` via the boilerplate pipeline and `ps5-at9-converter`. Run `make assets-check`.
2. **Audio:** your final set per Appendix B, balanced levels, and the limiter.
3. **4K60 evaluation (D2):** an SDK built with 2160p scanout and the 256 px atlases; adopt it only if frame time holds and a hardware run passes.
4. Final board themes, piece-set list, move-animation tuning, and a hub motion pass.
5. **Soak test:**
   - 30 minutes alternating TV, rated puzzles and an AI game.
   - 3 clean launch/close cycles.
   - No heap growth.
   - Network-pool statistics stable.
   - No stream-worker leaks (a count of open workers is logged).
6. **Documentation:**
   - `README.md` (features, controls, sign-in, Board API limits, fair play)
   - `NOTICE.md`
   - `docs/CONTROLS.md`
   - `docs/HARDWARE_TESTING.md`
   - `CHANGELOG.md`
7. **CI:** lint, host tests, fuzz smoke, the `libc` reproduction, the native build if the SDK asset is public, and a release on a `contentVersion` tag.
8. Switch to **PPSA99009**, freeze the candidate, run the evidence ladder, and **tag `01.000.000`**.

**A11:** the soak and cycles pass on the frozen candidate, and the release assets verify (they download and their checksums match).

### M12: Backlog
- **Offline Stockfish.** Spike first:
  - build it as an in-process library with pthreads and no iostream UCI loop
  - check exceptions and `std::thread` against libc++
  - load NNUE from `/app0`
- If the spike passes, it enables **Play vs Stockfish offline** and **post-game analysis**. Both are behind `LiveGameGuard`.
- Cloud eval and the opening explorer (which needs a token), post-game only.
- Chess960 and other variants; tournaments with berserk; a chat of preset phrases only; friends via `follow:read` and direct challenges; rematch.
- A touchpad pointer mode; broadcasts; HDR; localization using Lichess's CC0 translation files where licensed.
- Extracting the shared kit with ProsperoPuzzles (D17).

---

## 7. Hardware test protocol (runbook `85559fb`)

**Candidate record:** the commit, SDK manifest hash, backend (sceHttp or mbedTLS), title ID, `contentVersion`, and the SHA-256 and size of `eboot.bin` and every changed asset (including `pack.bin` and the piece atlases). A rebuild is a new candidate.

**Deploy:** `tools/deploy-verified.sh`:
1. Upload to a temporary name.
2. Read it back. Use `curl --quote SELF` for `eboot.bin` and `*.prx`.
3. Compare SHA-256.
4. Promote with RNFR/RNTO, `eboot.bin` and `param.json` last.
5. Confirm registration.

Never overwrite a running title. Presentation assets must be present, or the launch fails with 0x80940033.

**Capture and lifecycle**
1. Preflight with `nc -z -w 3` on 2121, 3232 and 9021.
2. Start a bounded klog capture before launch and prove it connected.
3. Take a baseline of the ShadowMount debug log.
4. Launch and close with the title-aware `send-controller.sh`.
5. After closing, pull `/user/download/PPSA99009/download0.dat` and extract it with `ufs2tool`.

**Owner-played runs (R8, R10b)**
1. I arm capture first, then tell you "ready" with the capture deadline.
2. You play without having to chat during the run.
3. I say when collection is done.

These are the only runs that need you in person. R5 and R7 can use approved input injection, or you if you prefer.

**Network-specific rules**
- Probe runs contact lichess.org, httpbin.org and badssl.com only.
- No production account other than yours. Automated games are **casual** only.
- Tokens never appear in logs, receipts, commits or chat.
- `download0.dat` contains your token, so it stays local and is never published.

**Evidence ladder:** transport → registration → launch → entry (`[PCH] entry`) → ready (`[PCH] ready`) → functional oracle → teardown → available.
- One question per run, with a hard timeout.
- **No blind retries after a suspected panic or GPU fault.**
- Never open Settings, the Store, updates or sign-in dialogs. The PS5's own sign-in is out of scope; our Lichess sign-in happens on your phone.

---

## 8. Testing strategy

**Host unit tests** (GoogleTest with ASan and UBSan):
- chess core (perft, corpus, edge cases)
- puzzles (all three sources' semantics, alternative mates, Streak and Storm rules)
- board input
- the net layer (NDJSON, encoding, backoff, 429 gate, redaction)
- Lichess models and state machines on fixtures
- PKCE vectors and listener parsing
- the save container and migration
- the mixer and command ring
- font layout
- scene flows
- registry invariants

**Fixtures**
- Recorded from WSL with `tools/record-fixture.sh`, which strips `Authorization` and tokens and replaces usernames on request.
- Hand-written NDJSON covers rare statuses.
- Fixtures are committed; the tool's redaction is unit tested.

**Host preview**
- `make host-run` is the full app under WSLg with keyboard mapping and the **libcurl backend against real Lichess**. Almost all online work is developed and exercised here before any console run.
- `make host-snapshots` renders every scene and theme to PNG.

**Fuzzing:** `from_fen`, `parse_san`, the NDJSON splitter and the listener's request parser run nightly in CI for 10 minutes each.

**Optional:** lila-docker two-player integration (M10.6).

**Hardware:** runs R1–R10 plus the M11 soak (section 7).

---

## 9. Risks

| Risk | Evidence | Mitigation |
|---|---|---|
| The first swap fails with `EGL_BAD_SURFACE` | Tenfold's only launch | R1 baseline; the exact native-app recipe including `kernel`/`amm`; diff against the staged demo |
| sceHttp buffers sparse streams, so moves arrive late | All proven streams were continuous | R6a item 4 with the WSL comparison; a small `RecvBlockSize`; the mbedTLS stream fallback |
| sceHttp does not verify certificates | ProsperoTV never configured a CA or verify mode | R6a item 5; `sceHttpsEnableOption`; the mbedTLS fallback with `VERIFY_REQUIRED` and bundled roots |
| A native app cannot accept inbound TCP | No native app has done it | R6b; the personal-token + keyboard fallback needs no listener |
| The phone is not on the same LAN, or client isolation is on | Common on guest Wi-Fi | On-screen guidance and a 5-minute window; the token fallback is always offered |
| Board API limits disappoint (no blitz seeks, no bullet) | Spec plus lila code | Explained in the UI; blitz is offered vs the AI; rapid pools are active on Lichess |
| Controller input is too slow under time pressure | Inherent | Smart targeting, R1/L1 cycling, premoves, auto-queen; rapid+ only (matches the API) |
| 429 or rate limits | Lichess guidance | One general lane; a separate game lane; a global 60 s pause; an event-stream token bucket; batch puzzle fetching |
| Fair-play flags on your account | lila `BoardReport` | No engine or aids during games (`LiveGameGuard`); no referer; casual-only automation, and only vs the AI |
| The app is closed or backgrounded mid-game | Normal console use | A clear in-game warning; My Games resume; the server remains authoritative |
| API drift | Lichess changes endpoints (explorer auth 2026-03) | Spec version pinned; tolerant parsing; fixture tests; a diagnostics panel |
| The token is at rest on `/download0` | No secure store validated | Sign-out revokes it; `download0.dat` is never published; a scoped token (no `web:` or `email:` scopes) |
| Asset licenses | lila `COPYING.md` | Only the D11 sets, D12 procedural boards, D13 sounds; license table in About and NOTICE |
| Branding or trademark confusion | — | Our own name and logo; an "unofficial client" notice; our own User-Agent |
| No `std::thread` or iostream (Stockfish later) | Boilerplate C++ profile | pthreads everywhere; the Stockfish spike is isolated in M12 |
| The puzzle pack's size or selection bias | 6.1 M source puzzles | A deterministic selection tool with a documented filter; about 6 MB; rebuilt per release |

---

## 10. Open questions

1. **Name and branding (D16):** "ProsperoLichess — unofficial lichess.org client", or something else?
2. **Repo and title IDs (D14, D15):** public `blackbearreloaded/ProsperoLichess`, with PPSA99009 for release and 99041 for development?
3. **Sounds (D13):** will you supply a final set (Appendix B)? Or do you accept lila's AGPL sets (piano, nes, sfx, futuristic) with attribution?
4. **Engine (D18):** is on-device Stockfish backlog, or a v1 requirement?
5. **Online testing (D19):**
   - Are automated **casual** games vs the Lichess AI on your account OK? They appear in your game history.
   - Will you play the R10b human game yourself?
   - Should I set up lila-docker for scripted two-player tests?
6. **ProsperoPuzzles ordering (D17):** build ProsperoLichess first, ProsperoPuzzles first, or interleave with a shared kit?
7. **Display (D2):** start at 1080p60 and evaluate 4K60 in M11, or 4K from the start?

---

## Appendix A: Lichess endpoint map (spec v2.0.174)

| Endpoint | Auth / scope | Used by | Notes |
|---|---|---|---|
| `GET /oauth` (browser) | — | Sign-in | PKCE S256; LAN `http` redirect allowed |
| `POST /api/token` | — | Sign-in | Form fields: `grant_type`, `code`, `code_verifier`, `redirect_uri`, `client_id`. The code lives 120 s. |
| `DELETE /api/token` | token | Sign-out | — |
| `POST /api/token/test` | — | Token fallback, probe | Body: comma-separated tokens |
| `GET /api/account` | token | Session | — |
| `GET /api/user/{username}` | — | Profile | — |
| `GET /api/account/playing` | token | My Games | — |
| `GET /api/stream/event` | `board:play` | Session | NDJSON; **one per token**; keep-alive every 7 s; 30 opens per 10 min |
| `POST /api/board/seek` | `board:play` | Quick Pairing | Real-time: **held open**; rapid or slower; 1 per user; 5 per minute per IP. Correspondence returns `{id}`. |
| `GET /api/board/game/stream/{id}` | `board:play` | GameSession | `gameFull`, `gameState`, `chatLine` (ignored), `opponentGone` |
| `POST /api/board/game/{id}/move/{uci}` | `board:play` | GameSession | `?offeringDraw=` |
| `POST /api/board/game/{id}/{abort,resign}` | `board:play` | GameSession | — |
| `POST /api/board/game/{id}/draw/{yes,no}` | `board:play` | GameSession | — |
| `POST /api/board/game/{id}/takeback/{yes,no}` | `board:play` | GameSession | — |
| `POST /api/board/game/{id}/claim-victory`, `/claim-draw` | `board:play` | GameSession | After `opponentGone` |
| `POST /api/challenge/ai` | `board:play` | vs Computer | `level` 1–8, `clock.limit`/`clock.increment`, `days`, `color` |
| `GET /api/challenge` | `challenge:read` | Challenges | — |
| `POST /api/challenge/{id}/{accept,decline}` | `board:play` | Challenges | — |
| `GET /api/puzzle/daily` | — | Daily Puzzle | Includes `fen` and `lastMove` |
| `GET /api/puzzle/{id}` | — | Deep links | Includes `fen` |
| `GET /api/puzzle/batch/{angle}` | optional `puzzle:read` | Training | `nb` ≤ 50, `difficulty` |
| `POST /api/puzzle/batch/{angle}` | `puzzle:write` | Training | `{"solutions":[{id,win,rated}]}` returns `glicko` and `rounds` |
| `GET /api/puzzle/dashboard/{days}` | `puzzle:read` | Profile | — |
| `GET /api/storm/dashboard/{username}` | — | Profile | — |
| `GET /api/tv/channels` | — | TV | — |
| `GET /api/tv/{channel}/feed`, `/api/tv/feed` | — | TV, Home hero | `featured` first, then a `fen` message per move (`lm`, `wc`, `bc`) |
| `https://database.lichess.org/lichess_db_puzzle.csv.zst` | — | Pack tool (host only) | CC0 |

**Not used:** the mobile websocket (`socket.lichess.org`); `/api/streak` and `/api/storm` (undocumented); the explorer, cloud eval and tablebase (backlog, post-game only).

## Appendix B: Sound cue specification (for your final set)

**Format:** 48 kHz, 16-bit PCM, stereo WAV, peak-normalized to −1 dBFS with no leading silence. One file per cue, named as below.

| Cue | Character | Length |
|---|---|---|
| `move` | A soft wooden piece placement | ≤ 150 ms |
| `capture` | A heavier, sharper placement | ≤ 200 ms |
| `check` | `move` plus a subtle accent | ≤ 250 ms |
| `castle` | Two quick placements | ≤ 300 ms |
| `promote` | A placement plus a bright shimmer | ≤ 400 ms |
| `illegal` | A muted, low "thunk" | ≤ 150 ms |
| `game_start` | A gentle two-note chime | ≤ 800 ms |
| `victory` / `defeat` / `draw` | Short stingers; positive, soft and neutral | ≤ 1.5 s |
| `low_time` | A single warning tone at the low-time threshold | ≤ 400 ms |
| `countdown_tick` | A dry tick, once per second under 10 s | ≤ 80 ms |
| `puzzle_correct` | A light positive blip | ≤ 250 ms |
| `puzzle_wrong` | A soft negative blip | ≤ 300 ms |
| `puzzle_solved` | A rewarding arpeggio | ≤ 1 s |
| `combo` | A rising blip for Storm combos, pitched up by the mixer | ≤ 200 ms |
| `notify` | A challenge, draw offer or `opponentGone` alert | ≤ 500 ms |
| `ui_move` / `ui_confirm` / `ui_back` | Hub navigation ticks | ≤ 120 ms |

**Optional:** `hub_ambient.ogg`, a loopable ambient bed of 1–3 minutes (OGG Vorbis, decoded by `stb_vorbis` as in ProsperoPuzzles D11). It is off by default.

## Appendix C: Third-party components and licenses

| Component | License | Use |
|---|---|---|
| ps5-native-app-boilerplate, ps5-opengl | GPL-3.0 (see their repos) | Build, runtime and GL |
| Mesa (inside the ps5-opengl SDK) | MIT | GL implementation |
| chessops and chessground (semantics and UX reference) | GPL-3.0-or-later | Ported behaviour; no code copied verbatim without a license header |
| yyjson | MIT | JSON |
| qrcodegen (Nayuki) | MIT | QR codes |
| stb_truetype (host tool), stb_vorbis (optional) | Public domain / MIT | Font baking, music |
| mbedTLS 3.6.4 (fallback only) | Apache-2.0 | TLS |
| Mozilla CA bundle (fallback only) | MPL-2.0 | Root certificates |
| Inter font | OFL-1.1 | UI text |
| cburnett, merida pieces | GPLv2+ (cburnett also BSD via Wikimedia) | Pieces |
| chessnut pieces | Apache-2.0 | Pieces |
| fantasy, spatial pieces | MIT | Pieces |
| rhosgfx pieces | CC0 | Pieces |
| Lichess puzzle database, puzzle theme names | CC0 | Offline pack, theme labels |
