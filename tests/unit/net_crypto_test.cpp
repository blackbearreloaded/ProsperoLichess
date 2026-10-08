// ProsperoLichess - SHA-256, base64url and PKCE tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/pkce.hpp"
#include "net/random.hpp"
#include "net/sha256.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <set>
#include <string>

namespace
{

std::string hex_of(std::string_view data)
{
    const auto digest = pch::net::sha256(data);
    return pch::net::to_hex(digest.data(), digest.size());
}

bool zero_source(void *out, std::size_t size)
{
    std::memset(out, 0, size);
    return true;
}

bool failing_source(void *, std::size_t)
{
    return false;
}

bool high_bytes_source(void *out, std::size_t size)
{
    // Only bytes >= 198 (all rejected), so the verifier cannot be completed.
    std::memset(out, 0xff, size);
    return true;
}

} // namespace

TEST(NetSha256, NistVectors)
{
    EXPECT_EQ(hex_of(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(hex_of("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(hex_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    EXPECT_EQ(hex_of("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklm"
                     "nopqklmnopqrlmnopqrsmnopqrstnopqrstu"),
              "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
}

TEST(NetSha256, MillionAsInPieces)
{
    pch::net::Sha256 hash;
    const std::string block(1000, 'a');
    for (int i = 0; i < 1000; ++i)
        hash.update(block);
    const auto digest = hash.finish();
    EXPECT_EQ(pch::net::to_hex(digest.data(), digest.size()),
              "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(NetSha256, UnalignedUpdatesMatchOneShot)
{
    std::string data;
    for (int i = 0; i < 300; ++i)
        data.push_back(static_cast<char>(i * 7));
    for (std::size_t split : {1u, 55u, 56u, 63u, 64u, 65u, 127u})
    {
        pch::net::Sha256 hash;
        for (std::size_t i = 0; i < data.size(); i += split)
            hash.update(std::string_view(data).substr(i, split));
        EXPECT_EQ(hash.finish(), pch::net::sha256(data)) << split;
    }
}

TEST(NetBase64Url, Rfc4648VectorsWithoutPadding)
{
    const auto b64 = [](std::string_view text)
    { return pch::net::base64url(text.data(), text.size()); };
    EXPECT_EQ(b64(""), "");
    EXPECT_EQ(b64("f"), "Zg");
    EXPECT_EQ(b64("fo"), "Zm8");
    EXPECT_EQ(b64("foo"), "Zm9v");
    EXPECT_EQ(b64("foob"), "Zm9vYg");
    EXPECT_EQ(b64("fooba"), "Zm9vYmE");
    EXPECT_EQ(b64("foobar"), "Zm9vYmFy");
    const unsigned char url_chars[] = {0xfb, 0xff, 0xbf};
    EXPECT_EQ(pch::net::base64url(url_chars, sizeof(url_chars)), "-_-_");
}

TEST(NetPkce, Rfc7636AppendixB)
{
    EXPECT_EQ(pch::net::challenge_s256("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"),
              "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM");
}

TEST(NetPkce, VerifierShapeAndRandomness)
{
    const std::string allowed =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    std::set<std::string> seen;
    for (int i = 0; i < 20; ++i)
    {
        const std::string verifier = pch::net::make_verifier();
        ASSERT_EQ(verifier.size(), pch::net::kVerifierLength);
        EXPECT_EQ(verifier.find_first_not_of(allowed), std::string::npos);
        seen.insert(verifier);
    }
    EXPECT_EQ(seen.size(), 20u);
    EXPECT_EQ(pch::net::make_verifier(zero_source), std::string(64, 'A'));
    EXPECT_EQ(pch::net::make_verifier(failing_source), "");
    EXPECT_EQ(pch::net::make_verifier(high_bytes_source), "");
}

TEST(NetPkce, StateAndSecureRandom)
{
    const std::string state = pch::net::make_state();
    EXPECT_EQ(state.size(), 32u);
    EXPECT_NE(state, pch::net::make_state());
    EXPECT_EQ(pch::net::make_state(failing_source), "");
    unsigned char a[32] = {};
    unsigned char b[32] = {};
    ASSERT_TRUE(pch::net::secure_random(a, sizeof(a)));
    ASSERT_TRUE(pch::net::secure_random(b, sizeof(b)));
    EXPECT_NE(std::memcmp(a, b, sizeof(a)), 0);
}
