// ProsperoLichess - Save container and atomic write tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_file.hpp"
#include "core/settings.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{

using pch::save::Kind;

TEST(SaveFile, Crc32MatchesReferenceVector)
{
    EXPECT_EQ(pch::save::crc32("123456789"), 0xcbf43926u);
    EXPECT_EQ(pch::save::crc32(""), 0u);
}

TEST(SaveFile, RoundTripsPayloadAndVersion)
{
    const std::string payload("game\0state", 10);
    const std::string encoded = pch::save::encode(Kind::game, 3, payload);
    const auto decoded = pch::save::decode(Kind::game, encoded);
    ASSERT_TRUE(decoded.ok) << decoded.error;
    EXPECT_EQ(decoded.version, 3);
    EXPECT_EQ(decoded.payload, payload);
}

TEST(SaveFile, RejectsCorruptionTruncationKindAndTrailingBytes)
{
    const std::string encoded = pch::save::encode(Kind::settings, 1, "volume=7");
    std::string flipped = encoded;
    flipped[14] ^= 0x01;
    EXPECT_FALSE(pch::save::decode(Kind::settings, flipped).ok);
    EXPECT_FALSE(pch::save::decode(Kind::settings, encoded.substr(0, encoded.size() - 1)).ok);
    EXPECT_FALSE(pch::save::decode(Kind::settings, encoded + "x").ok);
    EXPECT_FALSE(pch::save::decode(Kind::stats, encoded).ok);
    EXPECT_FALSE(pch::save::decode(Kind::settings, "PPZL").ok);
    EXPECT_FALSE(pch::save::decode(Kind::settings, "").ok);
}

TEST(SaveFile, WritesAtomicallyAndReadsBack)
{
    char directory[] = "/tmp/pch-save-XXXXXX";
    ASSERT_NE(mkdtemp(directory), nullptr);
    const std::string root = std::string(directory) + "/data";
    ASSERT_TRUE(pch::save::ensure_directory(root));
    ASSERT_TRUE(pch::save::ensure_directory(root));
    const std::string path = root + "/settings.bin";

    EXPECT_EQ(pch::save::write_atomic(path, "first"), "");
    EXPECT_EQ(pch::save::write_atomic(path, "second"), "");
    std::string data;
    ASSERT_TRUE(pch::save::read_file(path, &data));
    EXPECT_EQ(data, "second");
    EXPECT_NE(access((path + ".tmp").c_str(), F_OK), 0);
    EXPECT_FALSE(pch::save::read_file(root + "/missing.bin", &data));
    EXPECT_FALSE(pch::save::read_file(path, &data, 3));

    unlink(path.c_str());
    rmdir(root.c_str());
    rmdir(directory);
}

TEST(Settings, RoundTripsAndClampsVolumes)
{
    pch::Settings settings;
    settings.music_volume = 3;
    settings.swap_confirm = true;
    pch::Settings read;
    ASSERT_TRUE(pch::decode_settings(pch::encode_settings(settings), &read));
    EXPECT_EQ(read.music_volume, 3);
    EXPECT_TRUE(read.swap_confirm);
    std::string loud = pch::encode_settings(settings);
    loud[1] = 99; // music volume out of range
    ASSERT_TRUE(pch::decode_settings(loud, &read));
    EXPECT_EQ(read.music_volume, 10);
    EXPECT_FALSE(pch::decode_settings("", &read));
    EXPECT_FLOAT_EQ(pch::Settings::gain(10), 1.0f);
    EXPECT_FLOAT_EQ(pch::Settings::gain(0), 0.0f);
}

TEST(Settings, RoundTripsBoardAndPlayOptions)
{
    pch::Settings settings;
    settings.resolution = 2;
    settings.board_theme = 3;
    settings.piece_set = 1;
    settings.coordinates = false;
    settings.auto_queen = false;
    settings.premoves = false;
    pch::Settings read;
    const std::string data = pch::encode_settings(settings);
    ASSERT_TRUE(pch::decode_settings(data, &read));
    EXPECT_EQ(read.resolution, 2);
    EXPECT_EQ(pch::Settings::kResolutions[read.resolution].height, 2160);
    EXPECT_EQ(read.board_theme, 3);
    EXPECT_EQ(read.piece_set, 1);
    EXPECT_FALSE(read.coordinates);
    EXPECT_FALSE(read.auto_queen);
    EXPECT_FALSE(read.premoves);

    // The default resolution is 4K.
    EXPECT_EQ(pch::Settings{}.resolution, 2);
    EXPECT_TRUE(read.vibration);
    // A version 1 save (before Vibration existed) still loads, with vibration on.
    std::string v1 = data;
    v1[0] = 1;
    v1.pop_back();
    settings.vibration = false;
    ASSERT_TRUE(pch::decode_settings(pch::encode_settings(settings), &read));
    EXPECT_FALSE(read.vibration);
    ASSERT_TRUE(pch::decode_settings(v1, &read));
    EXPECT_TRUE(read.vibration);
    std::string unknown = data;
    unknown[0] = 9;
    EXPECT_FALSE(pch::decode_settings(unknown, &read));
    EXPECT_FALSE(pch::decode_settings(data.substr(0, data.size() - 1), &read));
}

} // namespace

TEST(SaveFile, ListFilesPrefersTheIndex)
{
    const std::string dir = ::testing::TempDir() + "pch_list_files";
    ASSERT_TRUE(pch::save::ensure_directory(dir));
    ::unlink((dir + "/index.txt").c_str());
    ASSERT_EQ(pch::save::write_atomic(dir + "/b.wav", "x"), "");
    ASSERT_EQ(pch::save::write_atomic(dir + "/a.wav", "x"), "");
    std::vector<std::string> listed = pch::save::list_files(dir);
    std::sort(listed.begin(), listed.end());
    EXPECT_EQ(listed, (std::vector<std::string>{"a.wav", "b.wav"}));

    ASSERT_EQ(pch::save::write_atomic(dir + "/index.txt", "one.ogg\r\ntwo.ogg\n\n../x\n"), "");
    EXPECT_EQ(pch::save::list_files(dir), (std::vector<std::string>{"one.ogg", "two.ogg"}));
    EXPECT_TRUE(pch::save::list_files(dir + "/missing").empty());
}
