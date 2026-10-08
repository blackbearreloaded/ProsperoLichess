// ProsperoLichess - End-to-end tests of net::Client over the host curl backend.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/client.hpp"
#include "net/url.hpp"
#include "unit/net_test_server.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

namespace
{

using Clock = std::chrono::steady_clock;
using pch::net::Result;

struct Timed
{
    Result result;
    Clock::time_point at;
};

long long ms_between(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
}

// Polls like the UI thread would until pred(all results so far) or timeout.
template <class Pred>
bool pump(pch::net::Client &client, std::vector<Timed> &seen, Pred pred, int timeout_ms = 5000)
{
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    while (Clock::now() < deadline)
    {
        Result result;
        while (client.poll(&result))
            seen.push_back({std::move(result), Clock::now()});
        if (pred(seen))
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

const Timed *find(const std::vector<Timed> &seen, std::uint64_t id, Result::Kind kind)
{
    for (const Timed &entry : seen)
    {
        if (entry.result.id == id && entry.result.kind == kind)
            return &entry;
    }
    return nullptr;
}

auto has(std::uint64_t id, Result::Kind kind)
{
    return [id, kind](const std::vector<Timed> &seen) { return find(seen, id, kind) != nullptr; };
}

pch::net::Request get(const std::string &url)
{
    pch::net::Request request;
    request.url = url;
    return request;
}

class NetClient : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        options.version = "test";
        options.rate_limit_pause_ms = 400;
    }
    void start()
    {
        client = std::make_unique<pch::net::Client>(pch::net::platform_backend(), options);
        std::string error;
        ASSERT_TRUE(client->start(&error)) << error;
    }

    pch::test::TestServer server;
    pch::net::ClientOptions options;
    std::unique_ptr<pch::net::Client> client;
    std::vector<Timed> seen;
};

} // namespace

TEST_F(NetClient, NotRunningRejectsWork)
{
    pch::net::Client idle(pch::net::platform_backend(), options);
    EXPECT_EQ(idle.submit(pch::net::Lane::general, get(server.url("/hello"))), 0u);
    EXPECT_EQ(idle.open_stream(get(server.url("/hang"))), 0u);
    Result result;
    EXPECT_FALSE(idle.poll(&result));
}

TEST_F(NetClient, Get)
{
    start();
    const auto id = client->submit(pch::net::Lane::general, get(server.url("/hello")));
    ASSERT_NE(id, 0u);
    ASSERT_TRUE(pump(*client, seen, has(id, Result::Kind::response)));
    const auto &response = find(seen, id, Result::Kind::response)->result.response;
    EXPECT_EQ(response.status, 200);
    EXPECT_TRUE(response.error.empty()) << response.error;
    EXPECT_TRUE(response.ok());
    EXPECT_EQ(response.body, "hello world");
    ASSERT_NE(response.header("content-length"), nullptr);
    EXPECT_EQ(*response.header("Content-Length"), "11");
}

TEST_F(NetClient, PostFormWithBearerTokenAndUserAgent)
{
    start();
    client->set_token("lip_testtoken123");
    EXPECT_TRUE(client->has_token());
    pch::net::Request request;
    request.method = pch::net::Method::post;
    request.url = server.url("/echo");
    request.content_type = "application/x-www-form-urlencoded";
    request.body = pch::net::form_encode({{"text", "good game"}, {"room", "a&b"}});
    const auto id = client->submit(pch::net::Lane::priority, request);
    ASSERT_TRUE(pump(*client, seen, has(id, Result::Kind::response)));
    const auto &response = find(seen, id, Result::Kind::response)->result.response;
    ASSERT_EQ(response.status, 200) << response.error;
    EXPECT_EQ(response.body, "method=POST\n"
                             "auth=Bearer lip_testtoken123\n"
                             "ctype=application/x-www-form-urlencoded\n"
                             "ua=ProsperoLichess/test (PS5; "
                             "+https://github.com/blackbearreloaded/ProsperoLichess)\n"
                             "body=text=good%20game&room=a%26b");

    // A request marked as not for the account never carries the token.
    pch::net::Request open = get(server.url("/echo"));
    open.authorize = false;
    const auto open_id = client->submit(pch::net::Lane::general, open);
    ASSERT_TRUE(pump(*client, seen, has(open_id, Result::Kind::response)));
    const auto &plain = find(seen, open_id, Result::Kind::response)->result.response;
    EXPECT_NE(plain.body.find("auth=\n"), std::string::npos) << plain.body;
    EXPECT_EQ(plain.body.find("lip_testtoken123"), std::string::npos) << plain.body;

    client->clear_token();
    pch::net::Request del = get(server.url("/echo"));
    del.method = pch::net::Method::del;
    const auto del_id = client->submit(pch::net::Lane::general, del);
    ASSERT_TRUE(pump(*client, seen, has(del_id, Result::Kind::response)));
    const auto &deleted = find(seen, del_id, Result::Kind::response)->result.response;
    EXPECT_NE(deleted.body.find("method=DELETE\nauth=\n"), std::string::npos) << deleted.body;
}

TEST_F(NetClient, StreamDeliversLinesIncrementally)
{
    start();
    const auto id = client->open_stream(get(server.url("/stream")));
    ASSERT_NE(id, 0u);
    EXPECT_EQ(client->open_streams(), 1u);
    ASSERT_TRUE(pump(*client, seen, has(id, Result::Kind::stream_line)));
    EXPECT_GE(client->stream_idle_ms(id), 0);
    ASSERT_TRUE(pump(*client, seen, has(id, Result::Kind::stream_closed)));

    std::vector<const Timed *> lines;
    for (const Timed &entry : seen)
    {
        if (entry.result.id == id && entry.result.kind == Result::Kind::stream_line)
            lines.push_back(&entry);
    }
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0]->result.line, "{\"n\":1}");
    EXPECT_EQ(lines[1]->result.line, "{\"n\":2}");
    EXPECT_EQ(lines[2]->result.line, "{\"n\":3}");
    // Each line arrived when it was sent, not with the end of the response.
    const int pause = pch::test::TestServer::kStreamPauseMs;
    EXPECT_GE(ms_between(lines[0]->at, lines[1]->at), pause - 100);
    EXPECT_GE(ms_between(lines[1]->at, lines[2]->at), pause - 100);
    const auto &closed = find(seen, id, Result::Kind::stream_closed)->result.response;
    EXPECT_EQ(closed.status, 200);
    EXPECT_TRUE(closed.error.empty()) << closed.error;
    EXPECT_EQ(client->stream_idle_ms(id), -1);
}

TEST_F(NetClient, CloseStreamCancelsBlockedReadPromptly)
{
    start();
    const auto id = client->open_stream(get(server.url("/hang")));
    ASSERT_TRUE(pump(*client, seen, has(id, Result::Kind::stream_line)));
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // now blocked in a read
    const auto started = Clock::now();
    client->close_stream(id);
    ASSERT_TRUE(pump(*client, seen, has(id, Result::Kind::stream_closed), 2000));
    const Timed *closed = find(seen, id, Result::Kind::stream_closed);
    EXPECT_LT(ms_between(started, closed->at), 500);
    EXPECT_EQ(closed->result.response.error, "cancelled");
    EXPECT_EQ(client->open_streams(), 0u);
    // The server sees the disconnect.
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    while (server.hang_disconnects() == 0 && Clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_EQ(server.hang_disconnects(), 1);
}

TEST_F(NetClient, StreamLimitAndPromptShutdown)
{
    start();
    std::vector<std::uint64_t> ids;
    for (int i = 0; i < 4; ++i)
        ids.push_back(client->open_stream(get(server.url("/hang"))));
    for (std::uint64_t id : ids)
    {
        ASSERT_NE(id, 0u);
        ASSERT_TRUE(pump(*client, seen, has(id, Result::Kind::stream_line)));
    }
    EXPECT_EQ(client->open_stream(get(server.url("/hang"))), 0u);
    const auto started = Clock::now();
    client->shutdown();
    EXPECT_LT(ms_between(started, Clock::now()), 1000);
    EXPECT_FALSE(client->running());
}

TEST_F(NetClient, RateLimitPausesGeneralLaneOnly)
{
    start();
    const auto limited = client->submit(pch::net::Lane::general, get(server.url("/limited")));
    ASSERT_TRUE(pump(*client, seen, has(limited, Result::Kind::response)));
    const Timed hit = *find(seen, limited, Result::Kind::response); // seen grows below
    EXPECT_EQ(hit.result.response.status, 429);
    EXPECT_NE(hit.result.response.body.find("rate limited"), std::string::npos);
    const double wait = client->seconds_until_resume();
    EXPECT_GT(wait, 0.0);
    EXPECT_LE(wait, 0.4);

    const auto submitted = Clock::now();
    const auto general = client->submit(pch::net::Lane::general, get(server.url("/hello")));
    const auto priority = client->submit(pch::net::Lane::priority, get(server.url("/hello")));
    ASSERT_TRUE(pump(*client, seen, has(priority, Result::Kind::response)));
    EXPECT_EQ(find(seen, general, Result::Kind::response), nullptr);
    EXPECT_LT(ms_between(submitted, find(seen, priority, Result::Kind::response)->at), 250);
    ASSERT_TRUE(pump(*client, seen, has(general, Result::Kind::response)));
    const long long general_ms =
        ms_between(hit.at, find(seen, general, Result::Kind::response)->at);
    EXPECT_GE(general_ms, 300);
    EXPECT_EQ(find(seen, general, Result::Kind::response)->result.response.status, 200);
    EXPECT_EQ(client->seconds_until_resume(), 0.0);
}

TEST_F(NetClient, TransportErrorIsReported)
{
    start();
    // A port nothing listens on (the server's port + bind race is avoided by
    // using a closed listener's port).
    std::uint16_t dead_port = 0;
    {
        pch::test::TestServer temporary;
        dead_port = temporary.port();
    }
    const auto id = client->submit(pch::net::Lane::general,
                                   get("http://127.0.0.1:" + std::to_string(dead_port) + "/x"));
    ASSERT_TRUE(pump(*client, seen, has(id, Result::Kind::response)));
    const auto &response = find(seen, id, Result::Kind::response)->result.response;
    EXPECT_EQ(response.status, 0);
    EXPECT_FALSE(response.error.empty());
}
