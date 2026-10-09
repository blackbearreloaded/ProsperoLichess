# Third-party notices

## Credits and acknowledgements

ProsperoLichess exists thanks to the maintainers and contributors of:

- [Lichess](https://lichess.org/) for the online play, puzzle, TV and
  Stockfish services the app connects to, and for the
  [puzzle database](https://database.lichess.org/#puzzles) (CC0) behind the
  offline puzzle pack;
- the chess piece artists Colin M.L. Burnett (cburnett), Armando Hernández
  Marroquín (merida) and Alexis Luengas (chessnut);
- [ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl), with
  [Mesa](https://mesa3d.org/) and OpenGNM PSBC, for OpenGL 4.6 on PS5;
- [ps5-homebrew-ui](https://github.com/blackbearreloaded/ps5-homebrew-ui) for
  the interface: its renderer, themes and component library;
- [PS5 Native App Boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)
  and the [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) by
  John Törnblom for the native foundation;
- [SharpProspero](https://github.com/SvenGDK/SharpProspero) by SvenGDK for the
  ELF/FSELF tooling and the PS5 HTTPS option flags;
- [yyjson](https://github.com/ibireme/yyjson), the
  [QR Code generator library](https://github.com/nayuki/QR-Code-generator) by
  Project Nayuki, [stb](https://github.com/nothings/stb), and the typefaces
  [Inter](https://github.com/rsms/inter),
  [Montserrat](https://github.com/JulietaUla/Montserrat) and
  [DejaVu](https://dejavu-fonts.github.io/);
- LLVM/Clang, Python, zlib and GoogleTest for build, packaging and validation
  tooling.

ProsperoLichess is Copyright (C) 2026 BlackBearReloaded and licensed under
GPL-3.0-or-later. It is built on `ps5-native-app-boilerplate`; the notices
below cover the boilerplate's dependencies and every third-party component the
project adds.

## Chess piece sets

`third_party/pieces/` holds the SVG sources of three piece sets, taken from
[lichess-org/lila](https://github.com/lichess-org/lila) (`public/piece/`), and
`assets/pieces/*.pcha` are atlases baked from them by `tools/bake-pieces.py`.
The authors and licenses are those listed in lila's
[COPYING.md](https://github.com/lichess-org/lila/blob/master/COPYING.md):

| Set | Author | License |
| --- | --- | --- |
| cburnett | [Colin M.L. Burnett](https://en.wikipedia.org/wiki/User:Cburnett) | GPL-2.0-or-later |
| merida | Armando Hernández Marroquín | GPL-2.0-or-later |
| chessnut | [Alexis Luengas](https://github.com/LexLuengas/chessnut-pieces) | Apache-2.0 (`third_party/pieces/chessnut/LICENSE.txt`) |

The GPL-2.0-or-later sets are used under GPL-3.0-or-later.

## Lichess puzzle database

`assets/puzzles/pack.bin` is a 101,888-puzzle selection from the
[Lichess puzzle database](https://database.lichess.org/#puzzles), released by
lichess.org under CC0 1.0. The snapshot, hash and selection rules are recorded
in [tools/PUZZLE_PACK.md](tools/PUZZLE_PACK.md).

## Lichess service

Online features use the public [Lichess API](https://lichess.org/api). Lichess
hosts the games, puzzles, TV feed and Stockfish analysis; no Lichess code is
included. ProsperoLichess is not affiliated with or endorsed by Lichess.

## QR codes

Login QR codes are drawn with the
[QR Code generator library](https://github.com/nayuki/QR-Code-generator)
(C version), Copyright (c) Project Nayuki, MIT licence. It is vendored in
`src/third_party/qrcodegen/` at the commit recorded in `UPSTREAM`, keeps its
original licence header, and is linked into the PS5 application.

## Interface kit

The drawing, theme and component layers (`src/gfx`, `src/ui`, and the input,
motion and sound-cue code they build on) are taken from
[ps5-homebrew-ui](https://github.com/blackbearreloaded/ps5-homebrew-ui),
Copyright (C) 2026 BlackBearReloaded, GPL-3.0-or-later, at the commit recorded
in `src/ui/KIT_COMMIT`. The changes made to it here are listed in
[docs/UI.md](docs/UI.md).

## Fonts and font baking

| Typeface | File | Copyright | Licence |
| --- | --- | --- | --- |
| Inter Regular, SemiBold | `third_party/fonts/Inter-*.ttf` | (c) 2016 The Inter Project Authors | SIL Open Font License 1.1 (`Inter-LICENSE.txt`) |
| Montserrat Medium | `third_party/fonts/Montserrat-Medium.ttf` | (c) 2011 The Montserrat Project Authors | SIL Open Font License 1.1 (`Montserrat-LICENSE.txt`) |
| DejaVu Sans Mono | `third_party/fonts/DejaVuSansMono.ttf` | (c) 2003 Bitstream, Inc.; DejaVu changes are public domain | Bitstream Vera licence (`DejaVu-LICENSE.txt`) |

The licence texts are in `third_party/fonts/` and are shipped with the app in
`assets/fonts/`. `assets/fonts/*.pchfont` are distance-field renderings of
these typefaces produced by `tools/bake-fonts.sh`.

The baker uses [stb_truetype](https://github.com/nothings/stb)
(`third_party/stb/stb_truetype.h`), and the host snapshot renderer uses
`third_party/stb/stb_image_write.h`; both are public domain or MIT (see the end
of each file). They are host-only and are not linked into the PS5 application.

## Sound effects

`assets/audio/sfx/chess/` holds the 66 chess and interface cues generated for
this project with ElevenLabs Sound Effects and prepared with
`tools/process-sfx.py` (trimmed, faded and levelled). The interface cues this
set does not record come from ps5-homebrew-ui: `assets/audio/sfx/glass/` and
the `tick` and `type` recordings in `assets/audio/sfx/chess/`. Those were
generated with ElevenLabs Sound Effects for ProsperoEden and ProsperoPuzzles,
are Copyright (C) 2026 BlackBearReloaded and are distributed under the project
licence.

## Music decoding

Music is decoded by [stb_vorbis](https://github.com/nothings/stb) (public
domain or MIT; see `src/third_party/stb/LICENSE`), vendored at the commit in
`src/third_party/stb/UPSTREAM` by `tools/update-stb.sh`. The opening title's
picture (`assets/art/title.jpg`, the launch artwork at 1080p) is decoded by
stb_image from the same commit (`src/third_party/stb/stb_image.inc`, same
terms). It is linked into the
PS5 application.

## Text in other scripts

Japanese, Korean, Chinese, Thai and Arabic text is drawn with the fonts the
console itself carries; none of them is shipped with the app. Their text is
shaped by [HarfBuzz](https://github.com/harfbuzz/harfbuzz) 12.3.2 (Copyright
the HarfBuzz authors, "Old MIT" licence), whose source the build fetches at
the version and checksum in `src/third_party/harfbuzz/UPSTREAM` and compiles
into the PS5 application through `src/third_party/harfbuzz/harfbuzz.cpp`; it
is not stored in this repository. Its licence ships with the app in
`assets/licenses/HarfBuzz.txt`. The glyph outlines are read with
[stb_truetype](https://github.com/nothings/stb) from the same commit as the
other stb files (`src/third_party/stb/stb_truetype.inc`, same terms), linked
into the PS5 application. The text direction code (`src/gfx/bidi.cpp`) and
the font code around it come from ProsperoEden by the same author.

## Console language ids

`src/third_party/ps5_system_language/ps5_system_language.hpp` lists the PS5's
system languages and their tags. It comes unmodified from
[ps5-system-language-research](https://github.com/blackbearreloaded/ps5-system-language-research),
Copyright (C) 2026 BlackBearReloaded, at the commit recorded in `UPSTREAM`.

## JSON parsing

Lichess API responses are parsed with [yyjson](https://github.com/ibireme/yyjson)
0.13.0, Copyright (c) 2020 YaoYuan, MIT licence (see
`src/third_party/yyjson/LICENSE`). It is vendored unmodified in
`src/third_party/yyjson/` (source archive and checksum recorded in `UPSTREAM`)
and is linked into the PS5 application.

## Host networking

Host-only builds (unit tests, PC pictures) use the system libcurl for HTTP.
The PS5 application links its own libcurl; see "HTTPS on the console" below.

## Update check

`third_party/update_check/` holds `update_check.h` and `update_check.c` from
[ProsperoEden](https://github.com/blackbearreloaded/ProsperoEden), derived
from the GPL-3.0-or-later PS5 Native App Boilerplate update kit at the commit
recorded in `UPSTREAM`. The signed-catalog checker and self-update engine use
the app's existing libcurl platform setup. The separately built
`self-updater.elf` performs the final in-place folder replacement.

The update helper uses [miniz](https://github.com/richgel999/miniz) 3.0.2,
MIT, vendored unmodified in `third_party/miniz` with its `LICENSE`. That
license is also shipped in `assets/licenses/miniz-MIT.txt`.

## Filesystem elevation

The package carries an exact-title one-shot helper built from the pinned
[compatibility fork](https://github.com/blackbearreloaded/PS5-Lapy-JB-Daemon) of
[mpereiraesaa's PS5-Lapy-JB-Daemon fork](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon)
(its donor-release and firmware 13.60 fixes were merged there as PR 48 and PR 49),
which builds on ArkSama's PS5-Lapy-JB-Daemon. It is built with the PS5 Payload
SDK v0.42. Lapy is MIT licensed and its
shared protocol header is LGPL-2.1-or-later. The pinned upstream source,
generated manifest, ELF hash, protocol hash and required retry feature are
verified before packaging; its license ships as `Lapy-MIT.txt`.

The helper build also uses the pinned single-header
[ps5log](https://github.com/mpereiraesaa/ps5-agc-gears/tree/1ae1f9182abd2770c131b97419034fb85173c2dc/native/ps5log)
client, GPL-3.0-or-later.

## OpenGL runtime

The PS5 build statically links the
[ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl) SDK
(GPL-3.0-or-later), which contains Mesa (MIT) and OpenGNM PSBC components under
their own licenses. The SDK is fetched or selected at build time and is not
stored in this repository; its license texts ship inside the SDK archive.

## Native build dependencies

The application build uses LLVM/Clang/lld, zlib 1.3.2, and the public
[PS5 payload SDK](https://github.com/ps5-payload-dev/sdk). The bootstrapper
downloads SDK v0.42 after verifying SHA-256
`8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da`.
It downloads zlib 1.3.2 from the upstream source archive after verifying
SHA-256 `bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16`
and compiles its static archive locally. Both dependencies remain under ignored
`.deps/native/`, retain their upstream licenses, and are not distributed by
this repository. No Sony SDK file is included.

Target C++ compilation uses the LLVM libc++ headers distributed by the public
SDK. Those headers retain the Apache-2.0 WITH LLVM-exception license recorded
upstream. The application does not redistribute or dynamically load the
complete libc++ or libc++abi archives.

The PS5 ELF converter and FSELF writer in `tooling/native/` are derived from
[SharpProspero](https://github.com/SvenGDK/SharpProspero), Copyright (C) 2026
SvenGDK, GPL-3.0, and were translated to C++ and modified by BlackBearReloaded.
The `sceHttpsEnableOption` flag values in `src/platform/ps5/http_scehttp.cpp`
also come from SharpProspero.

## Host test dependency

The host unit-test target downloads
[GoogleTest](https://github.com/google/googletest) 1.17.0 after verifying
SHA-256 `65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c`.
It remains under ignored `.deps/test/`, retains its BSD-3-Clause license, and
is not linked into any PS5 application, runtime, or package artifact.

## HTTPS on the console (libcurl and OpenSSL)

The console build talks to lichess.org through libcurl with OpenSSL, linked
statically from the PacBrew `v0.40.2` prebuilt ports (see the next section);
the PC build uses the system's libcurl. Linked into the PS5 application:

| Component | Version | Copyright | Licence |
| --- | --- | --- | --- |
| [libcurl](https://curl.se/) | 8.18.0 | Daniel Stenberg and contributors | curl licence (MIT/X derivative) |
| [OpenSSL](https://www.openssl.org/) | 3.5.2 | The OpenSSL Project Authors | Apache License 2.0 |
| [zlib](https://zlib.net/) | 1.3.2 | Jean-loup Gailly and Mark Adler | zlib licence |
| [Zstandard](https://facebook.github.io/zstd/) | 1.5.6 | Meta Platforms, Inc. and affiliates | BSD-3-Clause (dual-licensed with GPL-2.0; used under the BSD terms) |
| [libpsl](https://github.com/rockdaboot/libpsl) | 0.21.5 | Tim Rühsen | MIT; its built-in Public Suffix List data is MPL-2.0 |

The full licence texts of these components ship with the app in
`assets/licenses/`, taken from each project's release tag; the build adds this
file and the project's own licence beside them.
OpenSSL 3 under Apache-2.0 is compatible with this project's GPL-3.0-or-later.
The name-lookup and socket glue in `src/platform/ps5/net_sockets.c` follows
ProsperoRadio's curl integration and ProsperoLight's sceNet socket adapter
(both by BlackBearReloaded).

## Optional PacBrew dependencies

When selected through `PACBREW_*` build variables, the build downloads the prebuilt ports image
from [ps5-payload-dev/pacbrew-repo](https://github.com/ps5-payload-dev/pacbrew-repo)
release `v0.40.2`, verifies its published SHA-256, and extracts only the
`target/user/homebrew` prefix under ignored `.deps/pacbrew/`. It does not
replace the pinned SDK or install files globally. PacBrew recipes and every
linked third-party library retain their upstream licenses; applications must
review those terms before redistribution.

## Independently authored runtime shim

`tooling/native/libc_builder.cpp` and the manifests under
`tooling/native/runtime/` are independently authored for this project and
licensed under GPL-3.0-or-later. The generated `runtime/libc.prx` contains
project-authored compatibility stubs, startup code, and semantic loader
metadata. It contains no Sony runtime implementation.

Original ps5-native-app-boilerplate code is Copyright (C) 2026
BlackBearReloaded and licensed under GPL-3.0-or-later. Source and script files
carry matching SPDX identifiers.

## Presentation assets

`sce_sys/icon0.png`, `pic0.dds` and `pic1.dds` (the home-screen icon, and the
pictures behind the selected and the starting title) are artwork made for this
project by its author; their sources are `sce_sys/background-source.png` and
`launch-background-source.png`. `tools/render-art.sh` can still render plain
stand-ins from the app's own board and piece renderer. The soundtrack in `assets/audio/music/` and the selection music
`sce_sys/snd0.at9` (a cut of the menu song) were generated for this project with
ElevenLabs Music. All are distributed under GPL-3.0-or-later.

No proprietary runtime module, encryption key, or game file is included.
