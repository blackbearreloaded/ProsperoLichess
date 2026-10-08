# Languages

ProsperoLichess speaks the language the console is set to. The code holds the
English text; a catalog per language gives the rest. This page is for whoever
adds a screen, a text or a language.

## How a language is chosen

At start-up the app asks the console for its language
(`sceSystemServiceParamGetInt(1, …)`; the ids and their tags are in
`src/third_party/ps5_system_language`) and loads `assets/lang/<tag>.po`:
`pt-BR.po` for Brazilian Portuguese, `de-DE.po` for German. A language written
in two places borrows the other's catalog when its own is missing (`fr-CA` and
`fr-FR`, `es-419` and `es-ES`, `pt-PT` and `pt-BR`). English needs no catalog.
A catalog the fonts cannot draw in full is not used: English is better than
missing letters.

A file `/data/prosperolichess/language.txt`,
holding a tag, chooses another language whatever the console says; `en-US`
there keeps the app in English.

The log says what happened:

```text
[PCH] language system=17 rc=0x0 tag=pt-BR catalog=pt-BR texts=742
```

The language is fixed for the run. Screens translate their text when they are
built or drawn, and nothing has to follow a change.

## Marking text

Every text a player can read is marked where it is written (`core/strings.hpp`):

```cpp
paint.label(tr("Find opponent"), x, y, 24.0f, ink);          // translated here

static constexpr ui::Hint kHints[] = {{ui::Button::cross, TR("Play")}};  // a table:
                                                              // marked, translated where it is drawn

fill(tr("vs {0}"), {opponent});                               // a text with a value in it
fill(tr("{0} of {1}"), {std::to_string(i), std::to_string(n)});
plural(TR("{0} game waits"), TR("{0} games wait"), waiting);  // by a number
```

- `tr("…")` returns the text in the player's language (a `const char *` that
  stays valid for the run; `tr(std::string)` returns a `std::string`).
- `TR("…")` only marks: use it in constant tables and in the arguments of
  `plural()`, and call `tr()` on the entry where it is drawn. The hint row
  (`ui::draw_hints`) already translates its labels.
- **Never build a sentence from pieces.** `"vs " + name`, `count + " puzzles"`
  and `"Level " + std::to_string(n)` cannot be translated: other languages
  order the words differently. Write one pattern with `{0}`, `{1}` and `fill()`.
- A count needs two texts, the one for exactly one and the one for any other
  number: `plural()`. Languages with more forms word the second one so that it
  reads well with any number.
- A count that may reach the thousands is written with `grouped()`
  ("42,318", "42.318", "42 318": the separator is a text of the catalog),
  which `plural()` uses too, and a share with `percent()` ("49%", "49 %",
  "%49"). Ratings and years stay plain numbers.
- Capitals come from `ui::upper()` (it knows accents, Greek and Cyrillic, and
  the Turkish i): mark the text in its normal case, or in capitals when it is
  written so in the code, but do not upper-case a text by hand.
- Text that is the same in every language is not marked: names (ProsperoLichess,
  Lichess, Stockfish, a player's name), numbers, ratings, clocks ("10+5"),
  moves ("Nxd4"), web addresses, and anything written to the log.
- A word that means two things needs a context, because another language
  may use two words: `trc("result", "Draw")` for the game's result and
  `trc("offer", "Draw")` for the button that offers one (`TRC("offer",
  "Draw")` in a table, with `trc()` given the same context where it is
  drawn). The English text stays as it is; catalogs hold the pair as
  `msgctxt` and `msgid`. Use it only where the meanings really differ.

## Text that fits

A translation is often a third longer than the English. Where a line sits in
a box, draw it with `ui::text_fit()` (`ui/fonts.hpp`), which shrinks the
letters a little and, when that is not enough, ends the line in an ellipsis.
Text that fits is drawn exactly as `ui::text()` draws it, so English screens
do not change. `ui::paragraph()` wraps running text over lines, and a
component that measures text its own way breaks lines with
`gfx::wrap_lines()`, which knows where Japanese, Chinese and Thai may break. Measure a
translated label before placing things beside it instead of assuming the
English width.

## Checking on the PC

```bash
PCH_BASELINE=<folder of reference pictures> tools/lang-check.sh [scenario filter]
```

renders the PC scenarios in English and compares them with the reference
pictures (marking text must not change an English screen), then renders them
in a made-up test language and lists

- **text that reached the screen unmarked**: the test language writes most
  letters with a mark (`[Fíñď óppóñéñťëëë]`), so a text that still holds a plain
  one was not marked, or is a value such as a name or a move
  (the files `tools/lang/unmarked-ok*.txt` list those as regular expressions);
- **lines that had to be cut**: they do not fit their place even when shrunk,
  and the layout has to give them room.

The pictures stay in `build/lang-check/en` and `build/lang-check/xx`: look at
the second set, where text is a third longer than in English.

`PCH_LANG=<tag>` renders the scenarios in a real language
(`PCH_LANG=de-DE tools/host-snapshots.sh <folder>`).

## Catalogs

```bash
python3 tools/strings.py extract      # tools/lang/prosperolichess.pot, from the marked text
python3 tools/strings.py new pt-BR    # start or refresh assets/lang/pt-BR.po
python3 tools/strings.py check        # every catalog: untranslated, obsolete, placeholders, letters
tools/lang-fit.sh pt-BR               # what does not fit its place in that language
```

A catalog is a gettext `.po` file: `msgid` is the English text, `msgstr` the
translation. `{0}` and `{1}` stay as they are (their order may change), labels
written in capitals stay in capitals, names stay unchanged. The files
`tools/lang/notes*.py` hold what a translator cannot tell from a text alone
(where it stands, what a `{0}` is, how long it may be); the notes are written
above the text in every catalog. `make lint` runs the check.

Chess has a settled vocabulary in every language: use the words players of
that language use (the ones lichess.org uses), not a literal translation.

## Fonts

The app's faces (Inter, Montserrat, DejaVu Sans Mono, baked by
`tools/bake-fonts.sh`) hold the Latin alphabet with the marks of European
languages and Vietnamese, Greek and Cyrillic. Montserrat has no Greek and
DejaVu Sans Mono little Vietnamese: such letters are borrowed from Inter
(`gfx::Font::set_fallback`).

Japanese, Korean, Chinese, Thai and Arabic are drawn with the fonts the
console carries in `/preinst/common/font` (`gfx/system_fonts.hpp`): nothing
is shipped for them. A file is read the first time one of its characters is
needed, the language's own font first, so that characters Japanese, Chinese
and Korean share get that language's forms. HarfBuzz shapes the text (Arabic
letters join, Thai marks stack), `gfx/bidi.hpp` orders it (Arabic runs right
to left, with numbers and Latin words inside it left to right), and each
glyph is turned into a distance field in one atlas the four faces share
(`gfx::SystemGlyphs`). Lines break between the characters of Japanese and
Chinese text and where a zero-width space (U+200B) allows it, which Thai
catalogs put between words. The layout of a screen is not mirrored for
Arabic.

On the PC the console's fonts are not there: `PCH_SYSTEM_FONTS=<folder>`
names copies of them for the pictures and for `tools/strings.py check`
(copies stay out of the repository and of any package), and
`PCH_SPECIMEN=<text file> tools/host-snapshots.sh <folder>` draws a file's
lines in each face. The log says which fonts were read:

```text
[PCH] system fonts folder=/preinst/common/font files=6 read=SSTJpPro-Regular.otf (2965828 bytes) ...
```
