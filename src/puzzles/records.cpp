// ProsperoLichess - The player's own puzzle records, kept on the console.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "puzzles/records.hpp"

#include "core/bytes.hpp"
#include "core/save_file.hpp"

#include <cstdint>

namespace pch::puzzles
{

namespace
{
constexpr const char *kFile = "/puzzle_records.sav";
}

Records load_records(const std::string &data_root)
{
    Records records;
    std::string data;
    if (!save::read_file(data_root + kFile, &data))
        return records;
    const save::Decoded decoded = save::decode(save::Kind::stats, data);
    if (!decoded.ok)
        return records;
    bytes::Reader r(decoded.payload);
    records.best_streak = static_cast<int>(r.get<std::uint32_t>());
    records.best_storm = static_cast<int>(r.get<std::uint32_t>());
    records.solved = static_cast<int>(r.get<std::uint32_t>());
    if (!r.finished())
        return {};
    return records;
}

void save_records(const std::string &data_root, const Records &records)
{
    bytes::Writer w;
    w.put(static_cast<std::uint32_t>(records.best_streak));
    w.put(static_cast<std::uint32_t>(records.best_storm));
    w.put(static_cast<std::uint32_t>(records.solved));
    save::write_atomic(data_root + kFile, save::encode(save::Kind::stats, 1, w.data()));
}

} // namespace pch::puzzles
