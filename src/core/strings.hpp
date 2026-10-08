// ProsperoLichess - The app's text in the player's language.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Marks English text that is translated where it is used (in tables that
// cannot call tr()). tools/strings.py collects it for the catalogs.
#define TR(text) text
// The same for trc(): the context is given again where the entry is drawn.
#define TRC(context, text) text

namespace pch
{

// The code holds the English text. A catalog (assets/lang/<tag>.po, gettext's
// msgid/msgstr pairs) gives another language; text a catalog does not have
// stays English. An entry with a context (msgctxt) is kept apart from the
// same text without one.
class Catalog
{
  public:
    // Replaces the entries with those of a .po file; returns how many it holds.
    std::size_t load(std::string_view po);
    void clear()
    {
        entries_.clear();
    }
    // The translation, or the English text itself.
    std::string_view find(std::string_view english) const;
    std::size_t size() const
    {
        return entries_.size();
    }
    // Whether check(translation) holds for every entry.
    template <typename Check> bool every(Check &&check) const
    {
        for (const auto &entry : entries_)
        {
            if (!check(std::string_view{entry.second}))
                return false;
        }
        return true;
    }

  private:
    struct Hash
    {
        using is_transparent = void;
        std::size_t operator()(std::string_view text) const
        {
            return std::hash<std::string_view>{}(text);
        }
    };
    std::unordered_map<std::string, std::string, Hash, std::equal_to<>> entries_;
};

// The catalog every tr() reads. Load it before the first screen is built.
Catalog &catalog();

// The catalogs to try for a system language tag, the best first: the tag
// itself, then the same language as written elsewhere ("fr-CA": fr-CA,
// fr-FR). English ("en-US", "en-GB") needs none.
std::vector<std::string> catalog_candidates(std::string_view tag);

// The text in the player's language. The pointer is the catalog's (or english
// itself): it stays valid until the catalog is loaded again.
const char *tr(const char *english);
std::string tr(const std::string &english);

// The same for a word that needs different translations in different places
// ("Draw" the result and "Draw" the offer): context names the place, in a
// word or two of English. A catalog that lacks the pair gives the English text.
const char *trc(const char *context, const char *english);

// Puts values where the pattern says {0}, {1}...:
// fill(tr("{0} of {1}"), {"3", "12"}).
std::string fill(std::string_view pattern, std::initializer_list<std::string_view> values);

// A count with its thousands apart, the way the language writes them:
// "42,318" in English, "42.318" in German, "42 318" in French.
std::string grouped(long long value);

// A share of a hundred: "49%" in English, "49 %" in German, "%49" in Turkish.
std::string percent(int value);

// One of two texts by a number, which goes where the text says {0}, written
// as grouped() writes it: plural(TR("{0} game waits"), TR("{0} games wait"), 3).
std::string plural(const char *one, const char *many, long long count);

// The language tag in use ("en-US" until another is set): upper() needs it.
void set_text_language(std::string_view tag);
const std::string &text_language();

// Upper case for the letters the app's fonts have (Latin, Greek, Cyrillic),
// for small tracked labels. Greek capitals lose their accents, as they do in
// print; in Turkish a dotted i keeps its dot (but Lichess and Stockfish stay
// as they are); German's sharp s becomes SS.
std::string upper(std::string_view text);

} // namespace pch
