// ProsperoLichess - Log redaction and JSON (yyjson) parsing tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/client.hpp"
#include "net/redact.hpp"
#include "third_party/yyjson/yyjson.h"

#include <gtest/gtest.h>

#include <cstring>

using pch::net::redact;

TEST(NetRedact, BearerTokens)
{
    EXPECT_EQ(redact("Authorization: Bearer abc.DEF-123"), "Authorization: Bearer [redacted]");
    EXPECT_EQ(redact("authorization: bearer xyz, next"), "authorization: bearer [redacted], next");
    EXPECT_EQ(redact("token_type Bearer"), "token_type Bearer");
    EXPECT_EQ(redact("unbearer x"), "unbearer x");
}

TEST(NetRedact, FormAndQueryFields)
{
    EXPECT_EQ(redact("grant_type=authorization_code&code=S3cr3t&code_verifier=abc~._-&"
                     "redirect_uri=x"),
              "grant_type=authorization_code&code=[redacted]&code_verifier=[redacted]&"
              "redirect_uri=x");
    EXPECT_EQ(redact("/callback?code=liu_ABC&state=xyz"), "/callback?code=[redacted]&state=xyz");
    EXPECT_EQ(redact("zipcode=12345"), "zipcode=12345");
    EXPECT_EQ(redact("access_token=abc refresh_token=def"),
              "access_token=[redacted] refresh_token=[redacted]");
}

TEST(NetRedact, JsonMembers)
{
    EXPECT_EQ(redact(R"({"token_type":"Bearer","access_token":"xyz\"q","expires_in":3600})"),
              R"({"token_type":"Bearer","access_token":"[redacted]","expires_in":3600})");
    EXPECT_EQ(redact(R"({"code" : "abc"})"), R"({"code" : "[redacted]"})");
}

TEST(NetRedact, LichessTokens)
{
    EXPECT_EQ(redact("token lip_AbC123xyz end"), "token [redacted] end");
    EXPECT_EQ(redact("x=lio_Zz9;"), "x=[redacted];");
    EXPECT_EQ(redact("lip_"), "lip_");
    EXPECT_EQ(redact("Bearer lip_abc"), "Bearer [redacted]");
}

TEST(NetUserAgent, Format)
{
    EXPECT_EQ(
        pch::net::user_agent("01.002.003"),
        "ProsperoLichess/01.002.003 (PS5; +https://github.com/blackbearreloaded/ProsperoLichess)");
    EXPECT_EQ(pch::net::user_agent(""),
              "ProsperoLichess/dev (PS5; +https://github.com/blackbearreloaded/ProsperoLichess)");
}

TEST(NetJson, YyjsonParsesLichessEvent)
{
    const char *text =
        R"({"type":"gameState","moves":"e2e4 e7e5","wtime":180000,"status":"started"})";
    yyjson_doc *doc = yyjson_read(text, std::strlen(text), 0);
    ASSERT_NE(doc, nullptr);
    yyjson_val *root = yyjson_doc_get_root(doc);
    EXPECT_STREQ(yyjson_get_str(yyjson_obj_get(root, "type")), "gameState");
    EXPECT_STREQ(yyjson_get_str(yyjson_obj_get(root, "moves")), "e2e4 e7e5");
    EXPECT_EQ(yyjson_get_int(yyjson_obj_get(root, "wtime")), 180000);
    yyjson_doc_free(doc);
    EXPECT_EQ(yyjson_read("{bad", 4, 0), nullptr);
}
