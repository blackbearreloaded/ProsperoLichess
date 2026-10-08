// ProsperoLichess - OAuth PKCE (RFC 7636) verifier/challenge and state generation.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/pkce.hpp"

#include "net/sha256.hpp"

#include <cstdint>
#include <cstring>

namespace pch::net
{

namespace
{

constexpr char kUnreserved[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
constexpr std::size_t kAlphabetSize = sizeof(kUnreserved) - 1; // 66
// Largest multiple of 66 below 256: bytes at or above it are rejected so every
// character is equally likely.
constexpr unsigned kAcceptBelow = (256 / kAlphabetSize) * kAlphabetSize;

} // namespace

std::string make_verifier(RandomSource rng)
{
    if (rng == nullptr)
        return {};
    std::string out;
    out.reserve(kVerifierLength);
    std::uint8_t pool[96];
    for (int round = 0; round < 16 && out.size() < kVerifierLength; ++round)
    {
        if (!rng(pool, sizeof(pool)))
            return {};
        for (std::uint8_t byte : pool)
        {
            if (byte >= kAcceptBelow)
                continue;
            out.push_back(kUnreserved[byte % kAlphabetSize]);
            if (out.size() == kVerifierLength)
                break;
        }
    }
    std::memset(pool, 0, sizeof(pool));
    if (out.size() != kVerifierLength)
        return {};
    return out;
}

std::string challenge_s256(std::string_view verifier)
{
    const Sha256Digest digest = sha256(verifier);
    return base64url(digest.data(), digest.size());
}

std::string make_state(RandomSource rng)
{
    if (rng == nullptr)
        return {};
    std::uint8_t bytes[24];
    if (!rng(bytes, sizeof(bytes)))
        return {};
    std::string state = base64url(bytes, sizeof(bytes));
    std::memset(bytes, 0, sizeof(bytes));
    return state;
}

} // namespace pch::net
