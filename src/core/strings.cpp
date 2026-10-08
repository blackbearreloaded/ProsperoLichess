// ProsperoLichess - The app's text in the player's language.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/strings.hpp"

#include <cstdint>

namespace pch
{

namespace
{

// The text between the quotes of one .po line, with its escapes resolved;
// false when the line holds no quoted text.
bool quoted(std::string_view line, std::string *out)
{
    const std::size_t open = line.find('"');
    const std::size_t close = line.rfind('"');
    if (open == std::string_view::npos || close <= open)
        return false;
    for (std::size_t i = open + 1; i < close; ++i)
    {
        char c = line[i];
        if (c == '\\' && i + 1 < close)
        {
            c = line[++i];
            c = c == 'n' ? '\n' : c == 't' ? '\t' : c;
        }
        out->push_back(c);
    }
    return true;
}

// What stands between a context and its text in a catalog's key (gettext's own choice).
constexpr char kContextEnd = '\x04';

std::string &language_tag()
{
    static std::string tag = "en-US";
    return tag;
}

} // namespace

std::size_t Catalog::load(std::string_view po)
{
    entries_.clear();
    // A UTF-8 byte order mark is not text.
    if (po.substr(0, 3) == "\xef\xbb\xbf")
        po.remove_prefix(3);
    std::string context;
    std::string id;
    std::string text;
    enum class Part
    {
        none,
        context,
        id,
        text,
    } part = Part::none;
    const auto finish = [&]
    {
        if (!id.empty() && !text.empty())
            entries_[context.empty() ? id : context + kContextEnd + id] = text;
        context.clear();
        id.clear();
        text.clear();
    };
    while (!po.empty())
    {
        const std::size_t end = po.find('\n');
        std::string_view line = po.substr(0, end);
        po.remove_prefix(end == std::string_view::npos ? po.size() : end + 1);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.remove_suffix(1);
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
            line.remove_prefix(1);
        if (line.empty() || line.front() == '#')
            continue;
        if (line.substr(0, 8) == "msgctxt ")
        {
            finish();
            part = Part::context;
            quoted(line, &context);
        }
        else if (line.substr(0, 6) == "msgid ")
        {
            // A context just read belongs to this entry.
            if (part != Part::context)
                finish();
            part = Part::id;
            quoted(line, &id);
        }
        else if (line.substr(0, 7) == "msgstr ")
        {
            part = Part::text;
            quoted(line, &text);
        }
        else if (line.front() == '"')
        {
            // A continued string.
            if (part == Part::context)
                quoted(line, &context);
            else if (part == Part::id)
                quoted(line, &id);
            else if (part == Part::text)
                quoted(line, &text);
        }
    }
    finish();
    return entries_.size();
}

std::string_view Catalog::find(std::string_view english) const
{
    const auto found = entries_.find(english);
    return found != entries_.end() ? std::string_view{found->second} : english;
}

Catalog &catalog()
{
    static Catalog instance;
    return instance;
}

std::vector<std::string> catalog_candidates(std::string_view tag)
{
    // The same language as written in another place, when its own catalog is missing.
    static constexpr std::string_view kRelated[][2] = {
        {"fr-CA", "fr-FR"},  {"fr-FR", "fr-CA"}, {"es-419", "es-ES"},
        {"es-ES", "es-419"}, {"pt-PT", "pt-BR"}, {"pt-BR", "pt-PT"},
    };
    std::vector<std::string> result;
    if (tag.empty() || tag.substr(0, 2) == "en")
        return result;
    result.emplace_back(tag);
    for (const auto &pair : kRelated)
    {
        if (pair[0] == tag)
            result.emplace_back(pair[1]);
    }
    return result;
}

const char *tr(const char *english)
{
    const std::string_view text = catalog().find(english);
    // A translation is one of the catalog's strings: it ends in a zero like the English text.
    return text.data();
}

std::string tr(const std::string &english)
{
    return std::string{catalog().find(english)};
}

const char *trc(const char *context, const char *english)
{
    std::string key(context);
    key.push_back(kContextEnd);
    key.append(english);
    const std::string_view text = catalog().find(key);
    // Not in the catalog: find() gave the key back, and the English text is the answer.
    return text.data() == key.data() ? english : text.data();
}

namespace
{

// Whether text holds a letter written right to left (Hebrew, Arabic).
bool right_to_left(std::string_view text)
{
    for (std::size_t i = 0; i + 1 < text.size(); ++i)
    {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        const unsigned char next = static_cast<unsigned char>(text[i + 1]);
        // U+0590-U+08FF are the two-byte sequences D6 90 to DF BF and the
        // three-byte E0 A0-A3; the Arabic presentation forms U+FB1D-U+FEFC
        // start with EF AC-BB.
        if ((lead == 0xd6 && next >= 0x90) || (lead >= 0xd7 && lead <= 0xdf) ||
            (lead == 0xe0 && next >= 0xa0 && next <= 0xa3) ||
            (lead == 0xef && next >= 0xac && next <= 0xbb))
            return true;
    }
    return false;
}

// Whether a value has to keep its own order inside a right-to-left sentence:
// a Latin word, or numbers with a sign ("10+5" would read "5+10", "-12"
// would read "12-").
bool reads_left_to_right(std::string_view text)
{
    bool digit = false;
    bool sign = false;
    for (const char c : text)
    {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            return true;
        digit = digit || (c >= '0' && c <= '9');
        sign = sign || c == '+' || c == '-' || c == '/';
    }
    return digit && sign;
}

} // namespace

std::string fill(std::string_view pattern, std::initializer_list<std::string_view> values)
{
    // In a right-to-left sentence a left-to-right value (a name, a move, a
    // time control) keeps its own order and punctuation when it stands
    // between left-to-right marks (U+200E).
    static constexpr std::string_view kMark = "\xE2\x80\x8E";
    const bool mark = right_to_left(pattern);
    std::string out;
    out.reserve(pattern.size() + 16);
    for (std::size_t i = 0; i < pattern.size(); ++i)
    {
        if (pattern[i] == '{' && i + 2 < pattern.size() && pattern[i + 2] == '}' &&
            pattern[i + 1] >= '0' && pattern[i + 1] <= '9')
        {
            const std::size_t index = static_cast<std::size_t>(pattern[i + 1] - '0');
            if (index < values.size())
            {
                const std::string_view value = values.begin()[index];
                const bool wrap = mark && reads_left_to_right(value) && !right_to_left(value);
                if (wrap)
                    out.append(kMark);
                out.append(value);
                if (wrap)
                    out.append(kMark);
            }
            i += 2;
            continue;
        }
        out.push_back(pattern[i]);
    }
    return out;
}

std::string grouped(long long value)
{
    std::string digits = std::to_string(value);
    const std::string_view apart = trc("thousands separator", ",");
    const int first = value < 0 ? 1 : 0; // a minus sign is not a digit
    for (int at = static_cast<int>(digits.size()) - 3; at > first; at -= 3)
        digits.insert(static_cast<std::size_t>(at), apart);
    return digits;
}

std::string percent(int value)
{
    return fill(tr("{0}%"), {std::to_string(value)});
}

std::string plural(const char *one, const char *many, long long count)
{
    return fill(tr(count == 1 ? one : many), {grouped(count)});
}

void set_text_language(std::string_view tag)
{
    language_tag().assign(tag.empty() ? std::string_view("en-US") : tag);
}

const std::string &text_language()
{
    return language_tag();
}

namespace
{

// The capital of one letter; the letter itself when it has none here.
std::uint32_t upper_letter(std::uint32_t c, bool turkish)
{
    if (c < 0x80)
    {
        if (c == 'i' && turkish)
            return 0x130;
        return c >= 'a' && c <= 'z' ? c - 32 : c;
    }
    // Latin-1: the division sign sits among the letters.
    if (c >= 0xe0 && c <= 0xfe)
        return c == 0xf7 ? c : c - 32;
    if (c == 0xff)
        return 0x178;
    // Latin Extended-A: pairs, capital first, with three stretches where the
    // capital is the odd one.
    if (c == 0x131)
        return 'I';
    if (c == 0x17f)
        return 'S';
    if ((c >= 0x100 && c <= 0x12f) || (c >= 0x132 && c <= 0x137) || (c >= 0x14a && c <= 0x177))
        return (c & 1u) != 0 ? c - 1 : c;
    if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17e))
        return (c & 1u) == 0 ? c - 1 : c;
    // The horned letters of Vietnamese, and Romanian's comma below.
    if (c == 0x1a1 || c == 0x1b0 || c == 0x219 || c == 0x21b)
        return c == 0x1b0 ? 0x1af : c - 1;
    // Greek: capitals are written without the accent.
    switch (c)
    {
    case 0x386:
    case 0x3ac:
        return 0x391;
    case 0x388:
    case 0x3ad:
        return 0x395;
    case 0x389:
    case 0x3ae:
        return 0x397;
    case 0x38a:
    case 0x3af:
        return 0x399;
    case 0x38c:
    case 0x3cc:
        return 0x39f;
    case 0x38e:
    case 0x3cd:
        return 0x3a5;
    case 0x38f:
    case 0x3ce:
        return 0x3a9;
    case 0x390:
    case 0x3ca:
        return 0x3aa;
    case 0x3b0:
    case 0x3cb:
        return 0x3ab;
    case 0x3c2:
        return 0x3a3;
    default:
        break;
    }
    if (c >= 0x3b1 && c <= 0x3c9)
        return c - 32;
    // Cyrillic.
    if (c >= 0x430 && c <= 0x44f)
        return c - 32;
    if (c >= 0x450 && c <= 0x45f)
        return c - 80;
    if (c == 0x491)
        return 0x490;
    // Latin Extended Additional: the letters of Vietnamese, capital first.
    if (c >= 0x1ea0 && c <= 0x1ef9)
        return (c & 1u) != 0 ? c - 1 : c;
    return c;
}

void append_utf8(std::string &out, std::uint32_t c)
{
    if (c < 0x80)
    {
        out.push_back(static_cast<char>(c));
    }
    else if (c < 0x800)
    {
        out.push_back(static_cast<char>(0xc0 | (c >> 6)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3f)));
    }
    else if (c < 0x10000)
    {
        out.push_back(static_cast<char>(0xe0 | (c >> 12)));
        out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3f)));
    }
    else
    {
        out.push_back(static_cast<char>(0xf0 | (c >> 18)));
        out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3f)));
    }
}

} // namespace

std::string upper(std::string_view text)
{
    const bool turkish = language_tag().compare(0, 2, "tr") == 0;
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();)
    {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        const std::size_t length = lead < 0x80 ? 1 : lead >= 0xf0 ? 4 : lead >= 0xe0 ? 3 : 2;
        // Bytes that are not a whole character pass through as they are.
        if (lead < 0x80 || (lead >= 0x80 && lead < 0xc0) || i + length > text.size())
        {
            if (lead < 0x80)
                append_utf8(out, upper_letter(lead, turkish));
            else
                out.push_back(static_cast<char>(lead));
            ++i;
            continue;
        }
        std::uint32_t c = length == 2 ? lead & 0x1fu : length == 3 ? lead & 0x0fu : lead & 0x07u;
        for (std::size_t k = 1; k < length; ++k)
            c = (c << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3fu);
        // German's sharp s is written SS in capitals.
        if (c == 0xdf)
        {
            out.append("SS");
            i += length;
            continue;
        }
        append_utf8(out, upper_letter(c, turkish));
        i += length;
    }
    // Names that are not Turkish words keep their plain I.
    if (turkish)
    {
        for (const std::string_view name : {"L\xC4\xB0"
                                            "CHESS",
                                            "STOCKF\xC4\xB0"
                                            "SH"})
        {
            for (std::size_t at = out.find(name); at != std::string::npos; at = out.find(name, at))
                out.replace(at + name.find('\xC4'), 2, "I");
        }
    }
    return out;
}

} // namespace pch
