// ProsperoLichess - Which language the app speaks, and its catalog.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/language.hpp"

#include "core/save_file.hpp"
#include "core/strings.hpp"
#include "third_party/ps5_system_language/ps5_system_language.hpp"

namespace pch::language
{

namespace
{

constexpr std::size_t kLargestCatalog = 1u << 20;

// Loads po as the language tag; false (and English) when the fonts lack a
// letter of it.
bool take(std::string_view po, std::string_view tag,
          const std::function<bool(std::string_view)> &can_draw, Choice *choice)
{
    choice->texts = catalog().load(po);
    if (choice->texts != 0 && can_draw && !catalog().every(can_draw))
    {
        catalog().clear();
        choice->catalog += " (not used: no font for it)";
        choice->texts = 0;
        return false;
    }
    set_text_language(tag);
    return true;
}

} // namespace

std::string tag_for(int system_language)
{
    return std::string(ps5::i18n::language_tag(system_language));
}

std::string wanted(const std::string &data_root, std::string_view system_tag)
{
    std::string forced;
    if (save::read_file(data_root + "/language.txt", &forced, 64))
    {
        while (!forced.empty() && static_cast<unsigned char>(forced.back()) <= ' ')
            forced.pop_back();
        if (!forced.empty())
            return forced;
    }
    return system_tag.empty() ? std::string("en-US") : std::string(system_tag);
}

Choice choose(const std::string &assets, std::string_view tag,
              const std::function<bool(std::string_view)> &can_draw)
{
    Choice choice;
    choice.tag = tag.empty() ? std::string("en-US") : std::string(tag);
    catalog().clear();
    set_text_language("en-US");
    for (const std::string &candidate : catalog_candidates(choice.tag))
    {
        std::string po;
        if (!save::read_file(assets + "/lang/" + candidate + ".po", &po, kLargestCatalog))
            continue;
        choice.catalog = candidate;
        take(po, choice.tag, can_draw, &choice);
        break;
    }
    return choice;
}

Choice choose_file(const std::string &path, std::string_view tag,
                   const std::function<bool(std::string_view)> &can_draw)
{
    Choice choice;
    choice.tag = std::string(tag);
    catalog().clear();
    set_text_language("en-US");
    std::string po;
    if (save::read_file(path, &po, kLargestCatalog))
    {
        choice.catalog = path;
        take(po, tag, can_draw, &choice);
    }
    return choice;
}

} // namespace pch::language
