// ProsperoLichess - OAuth callback listener tests (host sockets).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/http.hpp"
#include "net/listener.hpp"
#include "unit/net_test_server.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace
{

std::string local_url(const pch::net::CallbackListener &listener, const std::string &target)
{
    return "http://127.0.0.1:" + std::to_string(listener.port()) + target;
}

pch::net::Response fetch(const std::string &url)
{
    std::string error;
    EXPECT_TRUE(pch::net::platform_backend().init(&error)) << error;
    pch::net::Request request;
    request.url = url;
    request.timeout_ms = 3000;
    return pch::net::platform_backend().perform(request);
}

} // namespace

TEST(NetListener, CallbackIsAnsweredAndQueued)
{
    pch::net::CallbackListener listener;
    std::string error;
    ASSERT_TRUE(listener.start(0, &error)) << error;
    EXPECT_GE(listener.port(), 20000);
    listener.set_reply("<html>Signed in. Return to your PS5.</html>");

    std::string path;
    std::string query;
    EXPECT_FALSE(listener.poll(&path, &query));
    const auto response = fetch(local_url(listener, "/callback?code=abc123&state=xyz"));
    EXPECT_EQ(response.status, 200) << response.error;
    EXPECT_EQ(response.body, "<html>Signed in. Return to your PS5.</html>");
    ASSERT_NE(response.header("content-type"), nullptr);
    EXPECT_EQ(response.header("content-type")->rfind("text/html", 0), 0u);
    ASSERT_NE(response.header("cache-control"), nullptr);
    EXPECT_EQ(*response.header("cache-control"), "no-store");

    ASSERT_TRUE(listener.poll(&path, &query));
    EXPECT_EQ(path, "/callback");
    EXPECT_EQ(query, "code=abc123&state=xyz");
    EXPECT_FALSE(listener.poll(&path, &query));
    listener.stop();
    EXPECT_FALSE(listener.listening());
}

TEST(NetListener, WrongPathMethodAndOversize)
{
    pch::net::CallbackListener listener;
    std::string error;
    ASSERT_TRUE(listener.start(0, &error)) << error;

    const auto missing = fetch(local_url(listener, "/favicon.ico"));
    EXPECT_EQ(missing.status, 404);
    const auto callback_prefix = fetch(local_url(listener, "/callbackx?code=1"));
    EXPECT_EQ(callback_prefix.status, 404);

    const std::string post =
        pch::test::raw_exchange(listener.port(), "POST /callback HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(post.rfind("HTTP/1.1 405", 0), 0u) << post;

    const std::string huge =
        "GET /callback?code=" + std::string(5000, 'a') + " HTTP/1.1\r\nHost: x\r\n\r\n";
    const std::string oversize = pch::test::raw_exchange(listener.port(), huge);
    EXPECT_EQ(oversize.rfind("HTTP/1.1 431", 0), 0u) << oversize.substr(0, 40);

    const std::string garbage = pch::test::raw_exchange(listener.port(), "hello\r\n\r\n");
    EXPECT_EQ(garbage.rfind("HTTP/1.1 400", 0), 0u);

    std::string path;
    std::string query;
    EXPECT_FALSE(listener.poll(&path, &query));

    // Still serving after the rejects.
    EXPECT_EQ(fetch(local_url(listener, "/callback")).status, 200);
    ASSERT_TRUE(listener.poll(&path, &query));
    EXPECT_EQ(query, "");
}

TEST(NetListener, RestartOnFixedPort)
{
    pch::net::CallbackListener listener;
    std::string error;
    ASSERT_TRUE(listener.start(0, &error)) << error;
    const std::uint16_t port = listener.port();
    listener.stop();
    ASSERT_TRUE(listener.start(port, &error)) << error;
    EXPECT_EQ(listener.port(), port);
    EXPECT_EQ(fetch(local_url(listener, "/callback?code=z")).status, 200);

    pch::net::CallbackListener second;
    EXPECT_FALSE(second.start(port, &error)); // already bound
    EXPECT_FALSE(error.empty());
}

TEST(NetListener, LanAddressIsEmptyOrDottedQuad)
{
    const std::string address = pch::net::lan_ipv4();
    if (!address.empty())
    {
        int dots = 0;
        for (char c : address)
        {
            dots += c == '.';
            EXPECT_TRUE((c >= '0' && c <= '9') || c == '.') << address;
        }
        EXPECT_EQ(dots, 3);
    }
}
