// ProsperoLichess - The text catalog, its patterns, capitals and the choice of a language.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/language.hpp"
#include "core/save_file.hpp"
#include "core/strings.hpp"
#include "gfx/font.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#ifndef PCH_SOURCE_DIR
#define PCH_SOURCE_DIR "."
#endif

namespace
{

using namespace pch;

// Every test leaves the app speaking English again.
class Strings : public ::testing::Test
{
  protected:
    void TearDown() override
    {
        catalog().clear();
        set_text_language("en-US");
    }
};

constexpr const char *kCatalog = "# a comment\n"
                                 "msgid \"\"\n"
                                 "msgstr \"Language: xx\\n\"\n"
                                 "\n"
                                 "#. a note\n"
                                 "#: modes/home_page.cpp\n"
                                 "msgid \"Play\"\n"
                                 "msgstr \"Jogar\"\n"
                                 "\n"
                                 "msgid \"{0} of {1}\"\n"
                                 "msgstr \"{1} / {0}\"\n"
                                 "\n"
                                 "msgid \"Two \"\n"
                                 "\"lines\"\n"
                                 "msgstr \"Duas \"\n"
                                 "  \"linhas \\\"aqui\\\"\"\r\n"
                                 "\n"
                                 "msgid \"Untranslated\"\n"
                                 "msgstr \"\"\n"
                                 "msgid \"{0} game waits\"\n"
                                 "msgstr \"{0} jogo espera\"\n"
                                 "msgid \"{0} games wait\"\n"
                                 "msgstr \"{0} jogos esperam\"\n";

TEST_F(Strings, CatalogReadsAPoFile)
{
    Catalog loaded;
    EXPECT_EQ(loaded.load(kCatalog), 5u);
    EXPECT_EQ(loaded.find("Play"), "Jogar");
    EXPECT_EQ(loaded.find("Two lines"), "Duas linhas \"aqui\"");
    // Text without a translation stays English.
    EXPECT_EQ(loaded.find("Untranslated"), "Untranslated");
    EXPECT_EQ(loaded.find("Never seen"), "Never seen");
    // A byte order mark is not text.
    EXPECT_EQ(loaded.load(std::string("\xEF\xBB\xBF") + kCatalog), 5u);
}

TEST_F(Strings, TrReadsTheSharedCatalog)
{
    EXPECT_STREQ(tr("Play"), "Play");
    catalog().load(kCatalog);
    EXPECT_STREQ(tr("Play"), "Jogar");
    EXPECT_EQ(tr(std::string("Play")), "Jogar");
    EXPECT_STREQ(tr("Watch"), "Watch");
}

TEST_F(Strings, AContextTellsTwoMeaningsApart)
{
    catalog().load("msgid \"Draw\"\nmsgstr \"Empate\"\n\n"
                   "msgctxt \"offer\"\nmsgid \"Draw\"\nmsgstr \"Propor empate\"\n\n"
                   "msgctxt \"result\"\nmsgid \"Lost\"\nmsgstr \"\"\n\n"
                   "msgid \"Won\"\nmsgstr \"Venceu\"\n");
    EXPECT_EQ(catalog().size(), 3u);
    EXPECT_STREQ(tr("Draw"), "Empate");
    EXPECT_STREQ(trc("offer", "Draw"), "Propor empate");
    // A pair the catalog lacks is the English text.
    EXPECT_STREQ(trc("result", "Lost"), "Lost");
    EXPECT_STREQ(trc("other", "Draw"), "Draw");
    // The context ends with its entry.
    EXPECT_STREQ(tr("Won"), "Venceu");
    EXPECT_STREQ(TRC("offer", "Draw"), "Draw");
}

TEST_F(Strings, FillPutsValuesInTheirPlaces)
{
    EXPECT_EQ(fill("{0} of {1}", {"3", "12"}), "3 of 12");
    EXPECT_EQ(fill("{1} / {0}", {"3", "12"}), "12 / 3");
    EXPECT_EQ(fill("{0}{0}", {"ab"}), "abab");
    // A place without a value is left out; braces that are not a place stay.
    EXPECT_EQ(fill("a {1} b {x}", {"only"}), "a  b {x}");
    // In a right-to-left sentence a Latin value stands between direction marks.
    EXPECT_EQ(fill("\xD9\x85\xD8\xB9 {0}", {"tester"}),
              "\xD9\x85\xD8\xB9 \xE2\x80\x8Etester\xE2\x80\x8E");
    EXPECT_EQ(fill("\xD9\x85\xD8\xB9 {0}", {"1500"}), "\xD9\x85\xD8\xB9 1500");
    // So do numbers with a sign between them, which would change places.
    EXPECT_EQ(fill("\xD9\x85\xD8\xB9 {0}", {"10+5"}), "\xD9\x85\xD8\xB9 \xE2\x80\x8E"
                                                      "10+5\xE2\x80\x8E");
    EXPECT_EQ(fill("with {0}", {"10+5"}), "with 10+5");
}

TEST_F(Strings, PluralPicksTheTextByTheNumber)
{
    EXPECT_EQ(plural(TR("{0} game waits"), TR("{0} games wait"), 1), "1 game waits");
    EXPECT_EQ(plural(TR("{0} game waits"), TR("{0} games wait"), 3), "3 games wait");
    catalog().load(kCatalog);
    EXPECT_EQ(plural(TR("{0} game waits"), TR("{0} games wait"), 1), "1 jogo espera");
    EXPECT_EQ(plural(TR("{0} game waits"), TR("{0} games wait"), 0), "0 jogos esperam");
}

TEST_F(Strings, CountsAndSharesAreWrittenTheLanguagesWay)
{
    EXPECT_EQ(grouped(0), "0");
    EXPECT_EQ(grouped(999), "999");
    EXPECT_EQ(grouped(42318), "42,318");
    EXPECT_EQ(grouped(-1234567), "-1,234,567");
    EXPECT_EQ(grouped(-123), "-123");
    EXPECT_EQ(percent(49), "49%");
    EXPECT_EQ(plural(TR("{0} game waits"), TR("{0} games wait"), 1301), "1,301 games wait");
    catalog().load("msgctxt \"thousands separator\"\nmsgid \",\"\nmsgstr \".\"\n\n"
                   "msgid \"{0}%\"\nmsgstr \"{0} %\"\n");
    EXPECT_EQ(grouped(42318), "42.318");
    EXPECT_EQ(percent(49), "49 %");
}

TEST_F(Strings, UpperKnowsTheLettersOfTheFonts)
{
    EXPECT_EQ(upper("Daily puzzle 12"), "DAILY PUZZLE 12");
    EXPECT_EQ(upper("op\xC3\xA7\xC3\xB5"
                    "es"),
              "OP\xC3\x87\xC3\x95"
              "ES");                                                          // opções
    EXPECT_EQ(upper("\xC5\xBE\xC5\x82\xC5\x91"), "\xC5\xBD\xC5\x81\xC5\x90"); // žłő
    EXPECT_EQ(upper("stra\xC3\x9F"
                    "e \xC3\xB7"),
              "STRASSE \xC3\xB7"); // ß is SS, ÷ stays
    EXPECT_EQ(upper("\xD0\xB8\xD0\xB3\xD1\x80\xD0\xB0"),
              "\xD0\x98\xD0\x93\xD0\xA0\xD0\x90");            // игра
    EXPECT_EQ(upper("\xD1\x97\xD2\x91"), "\xD0\x87\xD2\x90"); // їґ
    // Greek capitals drop the accent; the final sigma becomes a sigma.
    EXPECT_EQ(upper("\xCF\x80\xCE\xB1\xCE\xAF\xCE\xB6\xCF\x89 \xCF\x82"),
              "\xCE\xA0\xCE\x91\xCE\x99\xCE\x96\xCE\xA9 \xCE\xA3");
    EXPECT_EQ(upper("\xE1\xBB\x87\xC6\xB0"), "\xE1\xBB\x86\xC6\xAF"); // ệư
    // Turkish keeps the dot on its i, and gives the dotless one a plain capital.
    EXPECT_EQ(upper("oyun\xC4\xB1 izle"), "OYUNI IZLE");
    set_text_language("tr-TR");
    EXPECT_EQ(upper("oyun\xC4\xB1 izle"), "OYUNI \xC4\xB0ZLE");
    EXPECT_EQ(upper("Lichess TV izle, Stockfish"), "LICHESS TV \xC4\xB0ZLE, STOCKFISH");
    // Bytes that are not text pass through.
    EXPECT_EQ(upper("a\xFFz"), "A\xFFZ");
}

TEST_F(Strings, CandidatesFollowTheLanguage)
{
    EXPECT_TRUE(catalog_candidates("en-US").empty());
    EXPECT_TRUE(catalog_candidates("en-GB").empty());
    EXPECT_TRUE(catalog_candidates("").empty());
    EXPECT_EQ(catalog_candidates("de-DE"), std::vector<std::string>{"de-DE"});
    EXPECT_EQ(catalog_candidates("fr-CA"), (std::vector<std::string>{"fr-CA", "fr-FR"}));
    EXPECT_EQ(catalog_candidates("pt-PT"), (std::vector<std::string>{"pt-PT", "pt-BR"}));
    EXPECT_EQ(catalog_candidates("es-419"), (std::vector<std::string>{"es-419", "es-ES"}));
}

TEST_F(Strings, SystemLanguageIdsHaveTags)
{
    EXPECT_EQ(language::tag_for(0), "ja-JP");
    EXPECT_EQ(language::tag_for(1), "en-US");
    EXPECT_EQ(language::tag_for(17), "pt-BR");
    EXPECT_EQ(language::tag_for(18), "en-GB");
    EXPECT_EQ(language::tag_for(30), "uk-UA");
    EXPECT_EQ(language::tag_for(31), "en-US");
    EXPECT_EQ(language::tag_for(-1), "en-US");
}

class LanguageChoice : public Strings
{
  protected:
    void SetUp() override
    {
        char root[] = "/tmp/pch-language-test-XXXXXX";
        ASSERT_NE(mkdtemp(root), nullptr);
        root_ = root;
        assets_ = root_ + "/assets";
        ASSERT_TRUE(save::ensure_directory(assets_));
        ASSERT_TRUE(save::ensure_directory(assets_ + "/lang"));
        ASSERT_TRUE(save::write_atomic(assets_ + "/lang/pt-BR.po", kCatalog).empty());
    }
    std::string root_;
    std::string assets_;
};

TEST_F(LanguageChoice, TheConsoleLanguageLoadsItsCatalog)
{
    const language::Choice choice = language::choose(assets_, "pt-BR", nullptr);
    EXPECT_EQ(choice.tag, "pt-BR");
    EXPECT_EQ(choice.catalog, "pt-BR");
    EXPECT_EQ(choice.texts, 5u);
    EXPECT_STREQ(tr("Play"), "Jogar");
    EXPECT_EQ(text_language(), "pt-BR");
}

TEST_F(LanguageChoice, ARelatedCatalogServesWhenItsOwnIsMissing)
{
    const language::Choice choice = language::choose(assets_, "pt-PT", nullptr);
    EXPECT_EQ(choice.catalog, "pt-BR");
    EXPECT_STREQ(tr("Play"), "Jogar");
    EXPECT_EQ(text_language(), "pt-PT");
}

TEST_F(LanguageChoice, EnglishAndUnknownLanguagesNeedNoCatalog)
{
    for (const char *tag : {"en-US", "en-GB", "de-DE", ""})
    {
        const language::Choice choice = language::choose(assets_, tag, nullptr);
        EXPECT_EQ(choice.catalog, "none") << tag;
        EXPECT_EQ(choice.texts, 0u);
        EXPECT_STREQ(tr("Play"), "Play");
        EXPECT_EQ(text_language(), "en-US");
    }
}

TEST_F(LanguageChoice, AFileBesideTheAssetsChoosesTheLanguage)
{
    EXPECT_EQ(language::wanted(root_, "de-DE"), "de-DE");
    EXPECT_EQ(language::wanted(root_, ""), "en-US");
    ASSERT_TRUE(save::write_atomic(root_ + "/language.txt", "pt-BR\n").empty());
    EXPECT_EQ(language::wanted(root_, "de-DE"), "pt-BR");
    language::Choice choice = language::choose(assets_, language::wanted(root_, "de-DE"), nullptr);
    EXPECT_EQ(choice.tag, "pt-BR");
    EXPECT_STREQ(tr("Play"), "Jogar");
    ASSERT_TRUE(save::write_atomic(root_ + "/language.txt", "en-US").empty());
    choice = language::choose(assets_, language::wanted(root_, "pt-BR"), nullptr);
    EXPECT_EQ(choice.tag, "en-US");
    EXPECT_STREQ(tr("Play"), "Play");
}

TEST_F(LanguageChoice, ACatalogTheFontsCannotDrawIsNotUsed)
{
    const auto no_j = [](std::string_view text)
    { return text.find('J') == std::string_view::npos; };
    const language::Choice choice = language::choose(assets_, "pt-BR", no_j);
    EXPECT_EQ(choice.texts, 0u);
    EXPECT_NE(choice.catalog.find("not used"), std::string::npos);
    EXPECT_STREQ(tr("Play"), "Play");
    EXPECT_EQ(text_language(), "en-US");
}

// ---- the fonts the catalogs are drawn with ----

bool load(const char *name, gfx::Font *font)
{
    std::string data;
    return save::read_file(std::string(PCH_SOURCE_DIR) + "/assets/fonts/" + name, &data) &&
           font->load(data);
}

TEST(FontLetters, TheFacesHaveTheLettersOfTheCatalogs)
{
    gfx::Font regular;
    gfx::Font semibold;
    ASSERT_TRUE(load("inter-regular.pchfont", &regular));
    ASSERT_TRUE(load("inter-semibold.pchfont", &semibold));
    for (const gfx::Font *font : {&regular, &semibold})
    {
        EXPECT_TRUE(font->can_draw("Fran\xC3\xA7"
                                   "ais, Portugu\xC3\xAAs, \xC4\x8C"
                                   "e\xC5\xA1tina"));
        EXPECT_TRUE(font->can_draw(
            "\xD0\xA3\xD0\xBA\xD1\x80\xD0\xB0\xD1\x97\xD0\xBD\xD1\x81\xD1\x8C\xD0\xBA\xD0\xB0"));
        EXPECT_TRUE(
            font->can_draw("\xCE\x95\xCE\xBB\xCE\xBB\xCE\xB7\xCE\xBD\xCE\xB9\xCE\xBA\xCE\xAC"));
        EXPECT_TRUE(font->can_draw(
            "Ti\xE1\xBA\xBFng Vi\xE1\xBB\x87t, Rom\xC3\xA2n\xC4\x83 \xC8\x99\xC8\x9B"));
        // A no-break space is a space, and a zero-width space takes no glyph.
        EXPECT_TRUE(font->can_draw("10\xC2\xA0%\xE2\x80\x8B"));
        EXPECT_FALSE(font->can_draw("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"));
    }
}

TEST(FontLetters, AFaceBorrowsWhatItLacks)
{
    gfx::Font semibold;
    gfx::Font display;
    ASSERT_TRUE(load("inter-semibold.pchfont", &semibold));
    ASSERT_TRUE(load("montserrat-medium.pchfont", &display));
    const std::string_view greek = "\xCE\x95\xCE\xBB\xCE\xBB\xCE\xAC\xCE\xB4\xCE\xB1";
    EXPECT_FALSE(display.can_draw(greek));
    const float lacking = display.measure(greek, 40.0f);
    display.set_fallback(&semibold, 7);
    EXPECT_TRUE(display.can_draw(greek));
    // The borrowed letters have the other face's widths, and say where they come from.
    EXPECT_FLOAT_EQ(display.measure(greek, 40.0f), semibold.measure(greek, 40.0f));
    EXPECT_NE(display.measure(greek, 40.0f), lacking);
    std::vector<gfx::GlyphQuad> quads;
    display.layout(greek, 0.0f, 0.0f, 40.0f, gfx::Align::left, quads);
    ASSERT_EQ(quads.size(), 6u);
    for (const gfx::GlyphQuad &quad : quads)
    {
        EXPECT_EQ(quad.font, &semibold);
        EXPECT_EQ(quad.texture, 7u);
    }
    // Its own letters stay its own.
    quads.clear();
    display.layout("Play", 0.0f, 0.0f, 40.0f, gfx::Align::left, quads);
    ASSERT_EQ(quads.size(), 4u);
    EXPECT_EQ(quads[0].font, nullptr);
}

} // namespace
