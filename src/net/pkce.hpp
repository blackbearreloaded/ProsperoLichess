// ProsperoLichess - OAuth PKCE (RFC 7636) verifier/challenge and state generation.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "net/random.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace pch::net
{

inline constexpr std::size_t kVerifierLength = 64;

// kVerifierLength characters of [A-Za-z0-9-._~], unbiased; "" if rng fails.
std::string make_verifier(RandomSource rng = secure_random);

// base64url(SHA-256(verifier)) without padding (code_challenge_method=S256).
std::string challenge_s256(std::string_view verifier);

// Random opaque OAuth "state" (base64url of 24 bytes = 32 chars); "" if rng fails.
std::string make_state(RandomSource rng = secure_random);

} // namespace pch::net
