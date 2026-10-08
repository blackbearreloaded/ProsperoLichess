// ProsperoLichess - SHA-256 (FIPS 180-4) and unpadded base64url encoding.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace pch::net
{

using Sha256Digest = std::array<std::uint8_t, 32>;

class Sha256
{
  public:
    Sha256();
    void update(const void *data, std::size_t size);
    void update(std::string_view text)
    {
        update(text.data(), text.size());
    }
    // Finishes the hash; the object must be reset() before reuse.
    Sha256Digest finish();
    void reset();

  private:
    void compress(const std::uint8_t *block);

    std::uint32_t state_[8];
    std::uint8_t block_[64];
    std::size_t used_ = 0;
    std::uint64_t length_ = 0;
};

Sha256Digest sha256(std::string_view data);
std::string to_hex(const std::uint8_t *data, std::size_t size);
// RFC 4648 section 5 alphabet, no '=' padding.
std::string base64url(const void *data, std::size_t size);

} // namespace pch::net
