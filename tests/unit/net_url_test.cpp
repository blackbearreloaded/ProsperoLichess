// ProsperoLichess - URL encoding, form bodies and query parsing tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/http.hpp"
#include "net/url.hpp"

#include <gtest/gtest.h>

using pch::net::Fields;

TEST(NetUrl, PercentEncodeKeepsOnlyUnreserved)
{
    EXPECT_EQ(pch::net::percent_encode("AZaz09-._~"), "AZaz09-._~");
    EXPECT_EQ(pch::net::percent_encode("a b&c=d/e?f+g"), "a%20b%26c%3Dd%2Fe%3Ff%2Bg");
    EXPECT_EQ(pch::net::percent_encode("\xC3\xA9"), "%C3%A9");
    EXPECT_EQ(pch::net::percent_encode(""), "");
}

TEST(NetUrl, PercentDecode)
{
    EXPECT_EQ(pch::net::percent_decode("a%20b+c%2b"), "a b c+");
    EXPECT_EQ(pch::net::percent_decode("a+b", false), "a+b");
    EXPECT_EQ(pch::net::percent_decode("%zz%4"), "%zz%4");
    EXPECT_EQ(pch::net::percent_decode("%C3%A9"), "\xC3\xA9");
}

TEST(NetUrl, FormEncode)
{
    const Fields fields = {{"grant_type", "authorization_code"},
                           {"redirect_uri", "http://192.168.1.2:50000/callback"},
                           {"text", "hi there & bye"}};
    EXPECT_EQ(pch::net::form_encode(fields),
              "grant_type=authorization_code&redirect_uri=http%3A%2F%2F192.168.1.2%3A50000%"
              "2Fcallback&text=hi%20there%20%26%20bye");
    EXPECT_EQ(pch::net::form_encode({}), "");
}

TEST(NetUrl, WithQuery)
{
    EXPECT_EQ(pch::net::with_query("https://lichess.org/oauth", {{"a", "1"}, {"b", "x y"}}),
              "https://lichess.org/oauth?a=1&b=x%20y");
    EXPECT_EQ(pch::net::with_query("https://h/p?x=1", {{"y", "2"}}), "https://h/p?x=1&y=2");
    EXPECT_EQ(pch::net::with_query("https://h/p", {}), "https://h/p");
}

TEST(NetUrl, ParseQuery)
{
    const Fields fields = pch::net::parse_query("?code=abc%2F123&state=s+t&&flag&empty=#frag");
    ASSERT_EQ(fields.size(), 4u);
    EXPECT_EQ(fields[0], (std::pair<std::string, std::string>{"code", "abc/123"}));
    EXPECT_EQ(fields[1], (std::pair<std::string, std::string>{"state", "s t"}));
    EXPECT_EQ(fields[2], (std::pair<std::string, std::string>{"flag", ""}));
    EXPECT_EQ(fields[3], (std::pair<std::string, std::string>{"empty", ""}));
    EXPECT_EQ(pch::net::query_param("a=1&b=2&a=3", "a").value_or("-"), "1");
    EXPECT_FALSE(pch::net::query_param("a=1", "b").has_value());
    EXPECT_TRUE(pch::net::parse_query("").empty());
}

TEST(NetUrl, RoundTrip)
{
    const Fields fields = {{"k y", "v=1&2"}, {"\xC3\xA9", "+"}};
    EXPECT_EQ(pch::net::parse_query(pch::net::form_encode(fields)), fields);
}

TEST(NetHttp, HeaderHelpers)
{
    EXPECT_TRUE(pch::net::iequals("Content-Type", "content-TYPE"));
    EXPECT_FALSE(pch::net::iequals("a", "ab"));
    EXPECT_TRUE(pch::net::is_safe_header("Authorization", "Bearer x"));
    EXPECT_FALSE(pch::net::is_safe_header("X", "a\r\nInjected: 1"));
    EXPECT_FALSE(pch::net::is_safe_header("Bad Name", "v"));
    const auto headers = pch::net::parse_header_block(
        "HTTP/1.1 200 OK\r\nContent-Type: application/json \r\nX-Rate:  5\r\n\r\n");
    ASSERT_EQ(headers.size(), 2u);
    EXPECT_EQ(headers[0].name, "Content-Type");
    EXPECT_EQ(headers[0].value, "application/json");
    EXPECT_EQ(headers[1].value, "5");
    EXPECT_EQ(pch::net::url_origin("https://Lichess.org/api/account?x=1"), "https://lichess.org");
    EXPECT_EQ(pch::net::url_origin("http://127.0.0.1:8080"), "http://127.0.0.1:8080");
    EXPECT_EQ(pch::net::url_origin("/relative"), "");
    pch::net::Response response;
    response.headers = headers;
    ASSERT_NE(response.header("content-type"), nullptr);
    EXPECT_EQ(*response.header("content-type"), "application/json");
    EXPECT_EQ(response.header("missing"), nullptr);
}
