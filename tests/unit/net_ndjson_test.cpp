// ProsperoLichess - NDJSON line splitter tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/ndjson.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{

std::vector<std::string> split_all(pch::net::NdjsonSplitter &splitter, const std::string &input,
                                   std::size_t piece)
{
    std::vector<std::string> lines;
    const auto collect = [&](std::string_view line) { lines.emplace_back(line); };
    for (std::size_t i = 0; i < input.size(); i += piece)
        splitter.feed(std::string_view(input).substr(i, piece), collect);
    return lines;
}

} // namespace

TEST(NetNdjson, SplitsWholeInput)
{
    pch::net::NdjsonSplitter splitter;
    const auto lines = split_all(splitter, "{\"a\":1}\n{\"b\":2}\n", 1000);
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0], "{\"a\":1}");
    EXPECT_EQ(lines[1], "{\"b\":2}");
    EXPECT_FALSE(splitter.has_partial());
}

TEST(NetNdjson, ByteByByteWithCrlfAndKeepAlives)
{
    const std::string input = "\n{\"type\":\"gameFull\"}\r\n\n\r\n  \n{\"type\":\"gameState\"}\n\n";
    for (std::size_t piece : {1u, 2u, 3u, 5u, 7u, 64u})
    {
        pch::net::NdjsonSplitter splitter;
        const auto lines = split_all(splitter, input, piece);
        ASSERT_EQ(lines.size(), 2u) << "piece " << piece;
        EXPECT_EQ(lines[0], "{\"type\":\"gameFull\"}");
        EXPECT_EQ(lines[1], "{\"type\":\"gameState\"}");
        EXPECT_EQ(splitter.blank_lines(), 5u);
        EXPECT_EQ(splitter.lines(), 2u);
    }
}

TEST(NetNdjson, PartialLineWaitsForNewlineAndFlushes)
{
    pch::net::NdjsonSplitter splitter;
    std::vector<std::string> lines;
    const auto collect = [&](std::string_view line) { lines.emplace_back(line); };
    splitter.feed("{\"x\":", collect);
    EXPECT_TRUE(lines.empty());
    EXPECT_TRUE(splitter.has_partial());
    splitter.feed("1}", collect);
    EXPECT_TRUE(lines.empty());
    splitter.flush(collect);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "{\"x\":1}");
}

TEST(NetNdjson, DropsOversizeLinesAndRecovers)
{
    pch::net::NdjsonSplitter splitter(16);
    const std::string input = "short\n" + std::string(40, 'x') + "\nafter\n" +
                              std::string(16, 'y') + "\r\n" + std::string(17, 'z') + "\n";
    for (std::size_t piece : {1u, 4u, 100u})
    {
        splitter.reset();
        const auto lines = split_all(splitter, input, piece);
        ASSERT_EQ(lines.size(), 3u) << "piece " << piece;
        EXPECT_EQ(lines[0], "short");
        EXPECT_EQ(lines[1], "after");
        EXPECT_EQ(lines[2], std::string(16, 'y')); // exactly the limit is accepted
        EXPECT_EQ(splitter.oversize_lines(), 2u);
    }
}

TEST(NetNdjson, DefaultLimitIs256KiB)
{
    pch::net::NdjsonSplitter splitter;
    std::vector<std::string> lines;
    const auto collect = [&](std::string_view line) { lines.emplace_back(line); };
    const std::string big(256 * 1024, 'a');
    splitter.feed(big + "\n", collect);
    splitter.feed(big + "b\n", collect);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].size(), big.size());
    EXPECT_EQ(splitter.oversize_lines(), 1u);
}
