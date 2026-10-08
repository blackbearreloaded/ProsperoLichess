// ProsperoLichess - Cryptographically secure random bytes (per-platform source).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>

namespace pch::net
{

// Fills size bytes; false (buffer zeroed) when the source is unavailable.
// PS5: sysctlbyname("kern.rng_pseudo"); host: getrandom().
bool secure_random(void *out, std::size_t size);

// Signature shared by secure_random and deterministic test sources.
using RandomSource = bool (*)(void *out, std::size_t size);

} // namespace pch::net
