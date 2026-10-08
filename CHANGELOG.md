# Changelog

## 01.000.000

- The release ZIP stores every file and folder as readable, writable and
  runnable by all (0777), which the console needs to start an app unpacked
  from it; the build checks this.
- Builds of pull requests are named by their number and commit, in the
  downloadable build and in the app folder, so a test build is told apart
  from a release.

- The app updates itself: when homebrew.page lists a newer release, it is
  offered as the app opens (Update now, What's new, Skip). What's new shows
  the release's notes; the download shows its share and the time left; the
  app then closes and its files are replaced in place. Your account, games
  and settings are kept.
- Filesystem access through the bundled one-shot Lapy helper (built from the
  pin with the firmware 13.60 fix). With it the app keeps its data in
  `/data/prosperolichess`; without it the app runs in its sandbox as before.
- The app speaks the language the console is set to: 29 languages besides
  English (Arabic, Chinese Simplified and Traditional, Czech, Danish, Dutch,
  Finnish, French of France and of Canada, German, Greek, Hungarian,
  Indonesian, Italian, Japanese, Korean, Norwegian, Polish, Portuguese of
  Brazil and of Portugal, Romanian, Russian, Spanish of Spain and of Latin
  America, Swedish, Thai, Turkish, Ukrainian, Vietnamese), with the chess
  words lichess.org uses in each. Japanese, Korean, Chinese, Thai and Arabic
  are drawn with the console's own fonts. Moves keep the English piece
  letters, and the layout is not mirrored for Arabic.
- The Touchpad opens the game that waits for your move, or the list of them
  on Profile when there are several; its sign sits beside the bell in the
  status strip, which could not be reached before.
- L1 and R1 turn the tabs of the Puzzles and Play pages from the side rail
  too. The README said they change section between Puzzles and Play; they
  switch tabs inside those pages.

- Home screen with a side rail (Home, Puzzles, Play, Watch, Profile, Settings),
  today's puzzle, your ratings and a shelf of things to continue.
- Interface built on the ps5-homebrew-ui kit with a look of its own: a rail
  with the app's mark, a colour and a sky per part of the app, lit panels,
  ratings with their lines over time, the side to move lit, results with
  ceremony, staggered arrivals and a gliding focus on every screen, frosted
  dialogs, hold-to-resign, a settings page that shows what each setting does.
- The opening title continues the launch picture the console shows.
- Puzzles: Lichess Daily Puzzle and rated Puzzle Training; offline Puzzle
  Streak, Puzzle Storm and Puzzle Themes from a bundled pack of 101,888 puzzles.
- Play on lichess.org through the Board API: rapid, classical and
  correspondence pairing, Stockfish levels 1-8, My Games, offers, clocks and
  premoves. Lichess TV.
- Pass & Play on one controller.
- Sign in with a phone (OAuth PKCE and QR code) or a personal token.
- 4K60 OpenGL 4.6 board with animated moves, three piece sets and seven boards.
- ElevenLabs sound effects and a four-song soundtrack.
- Opening title, animated screen transitions, pieces dealt onto the board,
  capture, check and checkmate effects.
- Controller vibration on captures, checks and results.
- Tells you when a newer release is listed on homebrew.page: a notification
  at the top right for ten seconds, once per launch.
- Built on ps5-opengl SDK 1.0.0.
