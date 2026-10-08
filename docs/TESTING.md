# Testing

The repository separates fast host checks from behavior that only real PS5
hardware can prove.

## Commands

| Command | Scope |
| --- | --- |
| `make test-deps` | Fetch and verify the pinned host-only GoogleTest source. |
| `make test-unit` | Compile and run the host-native GoogleTest application tests. |
| `make test-integration` | Exercise repository scripts through subprocesses and temporary files. |
| `make test` | Run both host test suites. |
| `make check` | Run linting, all host tests, and a complete folder build. |

GitHub Actions runs `make test-unit` and `make test-integration` as separate
Ubuntu steps, so every pull request executes both layers with clear failure
reporting. Host tests must remain deterministic, must never contact a console,
and must be safe to run in parallel with unrelated console work. The first
unit-test run downloads a pinned GoogleTest archive after verifying its
SHA-256; later runs reuse `.deps/test/`.

Run one test or suite with normal GoogleTest arguments:

```bash
make test-unit GTEST_ARGS='--gtest_filter=AssetTextTest.MissingAssetUsesFallback'
```

## Unit-test policy

Write unit tests for reusable logic with meaningful behavior: parsers, state
transitions, bounds handling, input mapping, protocol messages, resource
ownership, and error paths. Keep platform calls behind a small boundary so the
logic can compile and run on Linux without a PS5 or proprietary SDK.

The starter suite in `tests/test_demo_renderer.cpp` uses GoogleTest to validate
fallback, line-ending, truncation, and null-termination behavior for packaged
text assets. GoogleTest is a host-only development dependency: it is never
compiled into `eboot.bin`, `libc.prx`, or a PS5 package.

Do not add tests for trivial constants or one-line drawing calls merely to
increase a coverage percentage. Test observable contracts and regressions.

## Host integration tests

`tests/test_tools.py` invokes complete repository scripts with temporary input
and controlled environment variables. Use this level for metadata updates,
build orchestration, package validation, and deployment resolution. Network
operations must be mocked or use an explicit dry-run mode; host CI must never
contact a console.

Each test must clean up its files, avoid shared mutable state, and include the
failure case that would have caught the associated bug.

## PS5 integration validation

Rendering, controller input, AudioOut, mounted paths, launch/closure behavior,
and firmware compatibility require hardware validation. A passing host suite
does not prove those properties.

For a hardware milestone:

1. Build an exact candidate from a clean commit and record its digest.
2. Acquire the shared console lock only for the test window.
3. Deploy the title through the documented LAN-only procedure.
4. Capture the expected visual result and relevant logs.
5. Close the title, release the lock, and record firmware, loader, result, and
   artifact identity.
6. Commit the validation record separately from the implementation when the
   project workflow requires one.

Follow [Deployment](DEPLOYMENT.md) and the separate
[PS5 Homebrew Development Protocol](https://github.com/blackbearreloaded/ps5-homebrew-dev-protocol)
for console coordination, evidence collection, and milestone policy.

### Scripted runs

A build that carries `assets/test/script.txt` plays that script instead of
waiting for a controller, logs every step to the kernel log, saves a picture
at every `mark`, and ends itself with `quit`, so nobody has to hold a
controller or close the title from outside. Releases carry no script.

The scripts are in `tests/hardware/` (the grammar is at the top of
`src/core/test_script.hpp`):

| Script | What it exercises |
| --- | --- |
| `title.txt` | The start: the launch picture, the opening title, the home screen |
| `look.txt` | Every page of the rail and every settings category, with frame statistics (uses the signed-in account, read only) |
| `settings.txt`, then `settings-kept.txt` | Every setting changed, the display restarted at each resolution; then a restart that finds them kept |
| `passplay.txt` | Pass & Play: a capture, checks, the promotion picker, en passant, castling, undo, flip, the move history, the menu, a resignation, the review |
| `resume-a.txt`, then `resume-b.txt` | A game left mid-way; then a restart that offers and resumes it |
| `puzzles-daily.txt` | Daily Puzzle with hints and solution, Puzzle Training without an account |
| `puzzles-streak.txt`, `puzzles-storm.txt`, `puzzles-themes.txt` | The offline modes: a run that ends, a full three-minute clock, the catalogue and its difficulties |
| `online.txt` | Lichess TV and its channels, the sign-in screen up to a refused (made-up) token |
| `tour.txt` | A Scholar's mate, Storm, Themes and Settings with frame statistics per screen |
| `update.txt` | The update announcement |
| `shortcuts.txt` | L1 and R1 from the side rail on the Puzzles and Play pages; the Touchpad with no game waiting |
| `soak.txt` | Ten minutes of Lichess TV on two channels, for the heap and the frame times (not replayed on the PC) |

Scripts that change anything start with `guest` (and `fresh`): the run then
keeps its settings, records and saved game in a folder of its own and starts
signed out, so the player's account and data are never touched. Nothing here
plays a game online; that needs a person and an account.

`moves.py` writes the controller steps of Pass & Play moves (the board turns
after every move). Every script is also a PC scenario
(`PCH_ONLY=hardware-passplay bash tools/host-snapshots.sh build/snapshots`,
with `PCH_ONLINE=1` for the two that talk to lichess.org): replay a script
there and look at its pictures before sending it to a console. While the
title runs, its pictures are read over FTP from
`/data/prosperolichess/shots` (or the sandbox fallback when elevation is unavailable).

## Trying an update

The app updates itself from the signed homebrew.page catalog. Before the
catalog lists a newer release, a test deployment can name one itself: put
`assets/test/update-offer.txt` beside the script, holding five lines (the new
content version, the release's name, the address of its ZIP on github.com, its
SHA-256, its size in bytes; any further lines are the release notes shown
under What's new). The app then offers that update at start-up,
without the catalog's signature. Releases carry no `assets/test` folder.

## Adding tests

- Add GoogleTest cases to C++ files under `tests/`; the `test-unit` recipe owns
  their host-only compilation.
- Add Python subprocess tests as `tests/test_*.py`; discovery is automatic.
- Preserve the GPL header on every test source.
- Run `make test`, `make lint`, and `make` before submitting a change.
- Reserve real-console claims for recorded hardware results.
