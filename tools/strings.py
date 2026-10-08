#!/usr/bin/env python3
# ProsperoLichess - The app's translation catalogs: template, checks and a test language.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
"""strings.py extract          write tools/lang/prosperolichess.pot from the text marked in the code
strings.py new <tag>          start (or refresh) assets/lang/<tag>.po from the template
strings.py check [tag...]     check every catalog in assets/lang, or the named ones
strings.py source <file>      write the texts as a numbered JSON list (text, context, note)
strings.py fill <tag> <file>  write assets/lang/<tag>.po from JSON {"number": "translation"}
strings.py pseudo <file>      write a made-up catalog that marks and lengthens every text
strings.py unmarked <log>     list text a PCH_TEXT_LOG shows on screen without that mark

The code holds the English text: tr("...") where it is used, TR("...") in constant tables and
in the arguments of plural(), trc("context", "...") and TRC("context", "...") for a word that
means two things. A catalog is a gettext .po file named after the PS5 system
language's tag (src/third_party/ps5_system_language): pt-BR.po, fr-FR.po...

check fails when a catalog
  - translates text the code no longer has, or leaves text untranslated,
  - changes the {0} {1} placeholders of a text,
  - uses a character the app's fonts do not have.
It warns when a translation is much longer than the English text (it may not fit its place).

The app's own fonts have Latin, Greek and Cyrillic letters. Japanese, Korean, Chinese, Thai and
Arabic are drawn with the console's fonts: their catalogs are checked against those when
PCH_SYSTEM_FONTS names a folder holding copies of them, and only for their Latin text otherwise.

pseudo writes a catalog for the PC tools (PCH_LANG_FILE): every text in brackets, most of its
letters accented, and about a third longer, so text that was not marked, or does not fit, shows
in the pictures. unmarked reads the texts such a run drew (PCH_TEXT_LOG) and lists those that
still hold a plain letter: text the code did not mark, or a value put into a text (a player's
name, a move), which tools/lang/unmarked-ok.txt names, one regular expression a line.
"""

import json
import os
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ROOT / "src"
CATALOGS = ROOT / "assets/lang"
TEMPLATE = ROOT / "tools/lang/prosperolichess.pot"
# The faces text is set in. A letter must be in the first two; the display face borrows what it
# lacks from the semibold one (gfx::Font::set_fallback).
FONTS = [ROOT / "assets/fonts" / name for name in ("inter-regular.pchfont", "inter-semibold.pchfont")]

LITERALS = r'((?:"(?:[^"\\]|\\.)*"\s*)+)'
MARKED = re.compile(r"\b(?:tr|TR)\(\s*" + LITERALS)
MARKED_IN_CONTEXT = re.compile(r'\b(?:trc|TRC)\(\s*"((?:[^"\\]|\\.)*)"\s*,\s*' + LITERALS)
# What stands between a context and its text in a key (gettext's own choice).
SEP = "\x04"
ONE = re.compile(r'"((?:[^"\\]|\\.)*)"')

# What a translator cannot tell from the text alone: English text -> a note, from the files
# tools/lang/notes*.py.
NOTES = {}


def load_notes():
    # notes.py, and one file a group of screens may keep for itself (notes_game.py...).
    for path in sorted((ROOT / "tools/lang").glob("notes*.py")):
        scope = {}
        exec(compile(path.read_text(encoding="utf-8"), str(path), "exec"), scope)
        NOTES.update(scope.get("NOTES", {}))


def unescape(text):
    def one(match):
        code = match.group(1)
        if code.startswith("x"):
            return chr(int(code[1:], 16))
        return {"n": "\n", "t": "\t"}.get(code, code)

    raw = re.sub(r"\\(x[0-9A-Fa-f]{2}|.)", one, text)
    # "\xC2\xB7" in the code is UTF-8 written as bytes.
    try:
        return raw.encode("latin-1").decode("utf-8")
    except (UnicodeEncodeError, UnicodeDecodeError):
        return raw


def escape(text):
    return text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


def joined(literals):
    return "".join(unescape(part) for part in ONE.findall(literals))


def text_of(key):
    """The English text of a key (a key is the text, or context + SEP + text)."""
    return key.partition(SEP)[2] if SEP in key else key


def marked_text():
    """English text (with its context, if it has one) -> the files that use it."""
    found = {}
    sources = sorted(p for pattern in ("*.cpp", "*.hpp", "*.h") for p in SOURCES.rglob(pattern))
    for path in sources:
        relative = path.relative_to(SOURCES).as_posix()
        if relative.startswith("third_party/"):
            continue
        source = path.read_text(encoding="utf-8")
        # Comments may quote tr("...") too; drop them.
        source = re.sub(r"//[^\n]*", "", source)
        for match in MARKED.finditer(source):
            text = joined(match.group(1))
            if text:
                found.setdefault(text, []).append(relative)
        for match in MARKED_IN_CONTEXT.finditer(source):
            text = joined(match.group(2))
            if text:
                found.setdefault(unescape(match.group(1)) + SEP + text, []).append(relative)
    return found


def parse_po(path):
    """msgid -> msgstr of a catalog (the header entry is skipped)."""
    entries = {}
    context, key, value, part = "", None, "", None
    for line in path.read_text(encoding="utf-8").splitlines() + ["msgid \"\""]:
        line = line.strip()
        starts_entry = line.startswith("msgctxt ") or (line.startswith("msgid ") and part != "ctx")
        if starts_entry:
            if key:
                entries[context + SEP + key if context else key] = value
            context, key, value = "", None, ""
        if line.startswith("msgctxt "):
            context, part = po_text(line[8:]), "ctx"
        elif line.startswith("msgid "):
            key, part = po_text(line[6:]), "id"
        elif line.startswith("msgstr "):
            value, part = po_text(line[7:]), "str"
        elif line.startswith('"'):
            if part == "ctx":
                context += po_text(line)
            elif part == "id":
                key += po_text(line)
            elif part == "str":
                value += po_text(line)
    return entries


def po_text(literals):
    """The text of a .po line: its escapes resolved, its UTF-8 kept as written."""
    return "".join(re.sub(r"\\(.)", lambda m: {"n": "\n", "t": "\t"}.get(m.group(1), m.group(1)), part)
                   for part in ONE.findall(literals))


def ordered(texts):
    """The texts in the order of the template and of every catalog."""
    return sorted(texts, key=lambda key: (text_of(key).lower(), key))


def write_catalog(path, texts, translations, language):
    lines = [f"# ProsperoLichess - {language}",
             "# Copyright (C) 2026 BlackBearReloaded",
             "# SPDX-License-Identifier: GPL-3.0-or-later",
             "#",
             "# English text is the key (msgid); msgstr is the translation. Keep {0} {1} as they are,",
             "# keep UPPERCASE labels uppercase, and keep names (ProsperoLichess, Lichess, Stockfish,",
             "# PS5, Chess960, lichess.org, homebrew.page) unchanged.",
             ""]
    for text in ordered(texts):
        note = NOTES.get(text) or NOTES.get(text_of(text))
        if note:
            lines.append(f"#. {note}")
        lines.append("#: " + ", ".join(sorted(set(texts[text]))))
        if SEP in text:
            lines.append(f'msgctxt "{escape(text.partition(SEP)[0])}"')
        lines.append(f'msgid "{escape(text_of(text))}"')
        lines.append(f'msgstr "{escape(translations.get(text, ""))}"')
        lines.append("")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8", newline="\n")


def font_characters(path):
    data = path.read_bytes()
    magic, _version, _w, _h, _size, _range, _asc, _desc, _gap, glyphs, _kerns = struct.unpack_from("<IIHHfffffII", data)
    assert magic == 0x46505A50, f"{path.name} is not a .pchfont file"
    return {struct.unpack_from("<I", data, 40 + 24 * index)[0] for index in range(glyphs)}


# Catalogs written in scripts the console's fonts draw.
SYSTEM_FONT_CATALOGS = {"ja-JP", "ko-KR", "zh-Hans", "zh-Hant", "th-TH", "ar"}
# Characters that take no room (the zero-width space, where a line may break, and the direction
# marks) or are drawn as a space (the no-break spaces).
INVISIBLE = {0x00A0, 0x00AD, 0x200B, 0x200C, 0x200D, 0x200E, 0x200F, 0x202F, 0x2060}


def cmap_characters(data):
    """The characters a TrueType or OpenType font file maps (its Unicode cmap, format 4 or 12)."""
    offset = struct.unpack_from(">I", data, 12)[0] if data[:4] == b"ttcf" else 0
    tables = {}
    for index in range(struct.unpack_from(">H", data, offset + 4)[0]):
        tag, _sum, start, _length = struct.unpack_from(">4sIII", data, offset + 12 + 16 * index)
        tables[tag] = start
    cmap = tables[b"cmap"]
    best = None
    for index in range(struct.unpack_from(">H", data, cmap + 2)[0]):
        platform, encoding, at = struct.unpack_from(">HHI", data, cmap + 4 + 8 * index)
        kind = struct.unpack_from(">H", data, cmap + at)[0]
        rank = {(3, 10): 4, (0, 4): 4, (0, 6): 4, (3, 1): 2, (0, 3): 2}.get((platform, encoding), 0)
        if kind in (4, 12) and (best is None or rank > best[0]):
            best = (rank, kind, cmap + at)
    characters = set()
    if best is None:
        return characters
    _rank, kind, at = best
    if kind == 12:
        for index in range(struct.unpack_from(">I", data, at + 12)[0]):
            first, last, _glyph = struct.unpack_from(">III", data, at + 16 + 12 * index)
            characters.update(range(first, last + 1))
        return characters
    segments = struct.unpack_from(">H", data, at + 6)[0] // 2
    ends = struct.unpack_from(f">{segments}H", data, at + 14)
    starts = struct.unpack_from(f">{segments}H", data, at + 16 + 2 * segments)
    deltas = struct.unpack_from(f">{segments}h", data, at + 16 + 4 * segments)
    ranges_at = at + 16 + 6 * segments
    ranges = struct.unpack_from(f">{segments}H", data, ranges_at)
    for i in range(segments):
        for code in range(starts[i], min(ends[i], 0xFFFE) + 1):
            if ranges[i] == 0:
                glyph = (code + deltas[i]) & 0xFFFF
            else:
                glyph = struct.unpack_from(">H", data, ranges_at + 2 * i + ranges[i] + 2 * (code - starts[i]))[0]
            if glyph:
                characters.add(code)
    return characters


def system_font_characters():
    """The characters of the console's fonts in PCH_SYSTEM_FONTS, or None when it is not set."""
    folder = os.environ.get("PCH_SYSTEM_FONTS")
    if not folder:
        return None
    characters = set()
    for path in sorted(Path(folder).iterdir()):
        if path.suffix.lower() in (".otf", ".ttf"):
            characters |= cmap_characters(path.read_bytes())
    return characters


def check(only=()):
    texts = marked_text()
    baked = set.intersection(*(font_characters(path) for path in FONTS))
    system = system_font_characters()
    failed = False
    catalogs = sorted(path for path in CATALOGS.glob("*.po") if not only or path.stem in only)
    if not catalogs:
        print("no catalogs in", CATALOGS.relative_to(ROOT))
    for path in catalogs:
        entries = parse_po(path)
        problems, warnings = [], []
        # What can be drawn: the app's fonts, and for the catalogs of other scripts the console's
        # fonts (when they are at hand; otherwise only their Latin text is checked).
        characters = baked | INVISIBLE
        unchecked = set()
        if path.stem in SYSTEM_FONT_CATALOGS:
            if system is None:
                unchecked = {ord(c) for text in entries.values() for c in text if ord(c) not in characters and c != "\n"}
                characters = characters | unchecked
            else:
                characters = characters | system
        for text in texts:
            if not entries.get(text):
                problems.append(f"untranslated: {text!r}")
        for text, translation in entries.items():
            if text not in texts:
                problems.append(f"not in the code any more: {text!r}")
                continue
            english = text_of(text)
            if sorted(re.findall(r"\{\d\}", english)) != sorted(re.findall(r"\{\d\}", translation)) and translation:
                problems.append(f"placeholders differ: {text!r} -> {translation!r}")
            missing = sorted({c for c in translation if ord(c) not in characters and c not in "\n"})
            if missing:
                problems.append(f"characters the fonts lack {''.join(missing)!r} in {translation!r}")
            if english.isupper() and translation and translation != translation.upper():
                warnings.append(f"label not uppercase: {text!r} -> {translation!r}")
            if translation and len(translation) > max(len(english) * 1.7, len(english) + 12):
                warnings.append(f"long ({len(english)} -> {len(translation)}): {translation!r}")
        note = f", {len(unchecked)} characters of the console's fonts not checked" if unchecked else ""
        print(f"{path.name}: {len(entries)} texts, {len(problems)} problems, {len(warnings)} warnings{note}")
        for line in problems[:40]:
            print("   ", line)
        if os.environ.get("PCH_LANG_WARNINGS"):
            for line in warnings[:int(os.environ["PCH_LANG_WARNINGS"])]:
                print("    warning:", line)
        failed |= bool(problems)
    print(f"{len(texts)} texts in the code, {len(catalogs)} catalogs" + (" FAIL" if failed else " PASS"))
    return 1 if failed else 0


# Letters the test language writes with a mark. A text that reaches the screen with one of them
# unmarked was not translated.
PLAIN = "aeiouycnstrldzgAEIOUYCNSTRLDZG"
ACCENTS = str.maketrans(PLAIN, "áéíóúýçñšťřľďžğÁÉÍÓÚÝÇÑŠŤŘĽĎŽĞ")


def pseudo_text(text):
    """The text in brackets, accented, about a third longer; its {0} placeholders kept."""
    parts = re.split(r"(\{\d\})", text)
    body = "".join(part if re.fullmatch(r"\{\d\}", part) else part.translate(ACCENTS) for part in parts)
    letters = sum(1 for c in text if c.isalpha())
    # A sign (the thousands separator, "{0}%") is kept as it is.
    if letters == 0:
        return text
    extra = max(1, letters // 3)
    pad = "ë" * extra if not text.isupper() else "Ë" * extra
    return "[" + body + pad + "]"


def unmarked(log):
    """Texts of a PCH_TEXT_LOG (scenario, tab, text) that hold a letter the test language marks."""
    allowed = []
    for allowed_path in sorted((ROOT / "tools/lang").glob("unmarked-ok*.txt")):
        for line in allowed_path.read_text(encoding="utf-8").splitlines():
            if line.strip() and not line.startswith("#"):
                allowed.append(re.compile(line.strip()))
    plain = re.compile("[" + PLAIN + "]")
    found = {}
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
        scenario, _tab, text = line.partition("\t")
        rest = text
        for pattern in allowed:
            rest = pattern.sub("", rest)
        if plain.search(rest):
            found.setdefault(text, []).append(scenario)
    for text in sorted(found):
        scenarios = sorted(set(found[text]))
        print(f"{text!r}  ({', '.join(scenarios[:3])}{'...' if len(scenarios) > 3 else ''})")
    print(f"{len(found)} texts on screen are not marked for translation" + ("" if found else ": PASS"))
    return 1 if found else 0


def main():
    # Translations are printed as they are, whatever the console's own encoding.
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    load_notes()
    command = sys.argv[1] if len(sys.argv) > 1 else ""
    if command == "extract":
        texts = marked_text()
        write_catalog(TEMPLATE, texts, {}, "template")
        print(f"{TEMPLATE.relative_to(ROOT)}: {len(texts)} texts, "
              f"{sum(len(text_of(t).split()) for t in texts)} words")
    elif command == "new" and len(sys.argv) == 3:
        path = CATALOGS / f"{sys.argv[2]}.po"
        existing = parse_po(path) if path.exists() else {}
        texts = marked_text()
        write_catalog(path, texts, {k: v for k, v in existing.items() if k in texts}, sys.argv[2])
        print(f"{path.relative_to(ROOT)}: {sum(1 for k in texts if existing.get(k))} translations kept, "
              f"{sum(1 for k in texts if not existing.get(k))} to translate")
    elif command == "check":
        sys.exit(check(sys.argv[2:]))
    elif command == "source" and len(sys.argv) == 3:
        texts = marked_text()
        rows = []
        for number, key in enumerate(ordered(texts)):
            row = {"n": number, "text": text_of(key)}
            if SEP in key:
                row["context"] = key.partition(SEP)[0]
            note = NOTES.get(key) or NOTES.get(text_of(key))
            if note:
                row["note"] = note
            rows.append(row)
        Path(sys.argv[2]).write_text(
            "[\n" + ",\n".join(json.dumps(row, ensure_ascii=False) for row in rows) + "\n]\n",
            encoding="utf-8", newline="\n")
        print(f"{sys.argv[2]}: {len(rows)} texts")
    elif command == "fill" and len(sys.argv) == 4:
        texts = marked_text()
        keys = ordered(texts)
        given = json.loads(Path(sys.argv[3]).read_text(encoding="utf-8"))
        translations = {}
        for number, translation in given.items():
            if not number.isdigit() or int(number) >= len(keys):
                sys.exit(f"no text has the number {number!r}")
            translations[keys[int(number)]] = translation
        path = CATALOGS / f"{sys.argv[2]}.po"
        write_catalog(path, texts, translations, sys.argv[2])
        missing = [str(n) for n, key in enumerate(keys) if not translations.get(key)]
        print(f"{path.relative_to(ROOT)}: {len(keys) - len(missing)} of {len(keys)} texts translated"
              + (f"; missing numbers: {', '.join(missing[:40])}" if missing else ""))
    elif command == "unmarked" and len(sys.argv) == 3:
        sys.exit(unmarked(Path(sys.argv[2])))
    elif command == "pseudo" and len(sys.argv) == 3:
        texts = marked_text()
        write_catalog(Path(sys.argv[2]), texts, {text: pseudo_text(text_of(text)) for text in texts},
                      "a made-up test language")
        print(f"{sys.argv[2]}: {len(texts)} texts")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
