// ProsperoLichess - Connection notices: issues, flashes and their priority.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lichess/session.hpp"

#include <gtest/gtest.h>

using pch::lichess::Notice;
using pch::lichess::Session;

TEST(SessionNotice, QuietByDefault)
{
    Session session("/tmp", false);
    EXPECT_EQ(session.notice().kind, Notice::Kind::none);
}

TEST(SessionNotice, FlashShowsThenExpires)
{
    Session session("/tmp", false);
    session.flash("Back online", Notice::Kind::good, 2.0);
    EXPECT_EQ(session.notice().kind, Notice::Kind::good);
    EXPECT_EQ(session.notice().text, "Back online");
    EXPECT_FALSE(session.notice().busy);
    session.pump(2.5f);
    EXPECT_EQ(session.notice().kind, Notice::Kind::none);
}

TEST(SessionNotice, IssuesOutrankFlashesAndClearWithAConfirmation)
{
    Session session("/tmp", false);
    session.flash("Puzzle result not saved yet.", Notice::Kind::warning, 5.0);
    session.set_issue("game", "Connection to the game lost. Reconnecting");
    session.set_issue("tv", "Lichess TV lost the feed. Reconnecting");
    EXPECT_TRUE(session.has_issue("game"));
    EXPECT_TRUE(session.notice().busy);
    EXPECT_EQ(session.notice().text, "Lichess TV lost the feed. Reconnecting");
    // Setting an existing issue again only updates its text.
    session.set_issue("tv", "Still reconnecting");
    EXPECT_EQ(session.notice().text, "Still reconnecting");

    session.clear_issue("tv");
    EXPECT_EQ(session.notice().text, "Connection to the game lost. Reconnecting");
    session.clear_issue("game");
    EXPECT_FALSE(session.has_issue("game"));
    EXPECT_EQ(session.notice().kind, Notice::Kind::good);
    EXPECT_EQ(session.notice().text, "Reconnected");
    session.pump(3.0f);
    EXPECT_EQ(session.notice().kind, Notice::Kind::none);
}

TEST(SessionNotice, QuietClearAndUnknownKeysSayNothing)
{
    Session session("/tmp", false);
    session.set_issue("game", "Reconnecting");
    session.clear_issue("game", false);
    EXPECT_EQ(session.notice().kind, Notice::Kind::none);
    session.clear_issue("never-set");
    EXPECT_EQ(session.notice().kind, Notice::Kind::none);
}
