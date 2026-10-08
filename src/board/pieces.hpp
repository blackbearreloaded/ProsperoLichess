// ProsperoLichess - Piece set atlases baked by tools/bake-pieces.py.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "chess/chess.hpp"
#include "gfx/draw_list.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace pch::board
{

struct PieceSetInfo
{
    const char *id;    // file stem in assets/pieces
    const char *label; // shown in Settings
};

inline constexpr PieceSetInfo kPieceSets[] = {
    {"cburnett", "Classic"},
    {"merida", "Merida"},
    {"chessnut", "Chessnut"},
};
inline constexpr int kPieceSetCount = static_cast<int>(sizeof(kPieceSets) / sizeof(kPieceSets[0]));

// Parsed .pcha file: RGBA pixels, 6 columns (K Q R B N P) by 2 rows (white, black).
struct AtlasImage
{
    int width = 0;
    int height = 0;
    int cell = 0;
    std::string pixels; // width * height * 4 bytes
};
bool parse_atlas(std::string_view data, AtlasImage *out, std::string *error);

// UV rectangle of a piece inside an atlas image (normalised).
gfx::Rect piece_uv(chess::Piece piece);

// The GPU side: one texture per loaded atlas. The texture factory is the
// batch's create_texture (kept abstract for tests).
class PieceAtlas
{
  public:
    using TextureFactory = std::function<std::uint32_t(int, int, const std::uint8_t *)>;

    static constexpr int kLevels = 4;

    // Loads <assets>/pieces/<set>-<cell>.pcha, choosing the cell size that
    // best matches the largest square in output pixels, and builds halved
    // copies of it down to 32 px cells: the kit's textures have one level, so
    // a thumbnail samples a copy near its own size instead of shimmering.
    // surface_scale is output pixels per virtual pixel.
    bool load(const std::string &assets, int set, float square_output_pixels, float surface_scale,
              const TextureFactory &create, std::string *error);
    // The largest level's texture (0 when nothing is loaded).
    std::uint32_t texture() const
    {
        return levels_[0].texture;
    }
    // Every texture the atlas holds, for the owner to delete.
    std::uint32_t level_texture(int level) const
    {
        return levels_[static_cast<std::size_t>(level)].texture;
    }
    int set() const
    {
        return set_;
    }
    void forget()
    {
        for (Level &level : levels_)
            level = {};
    }

    void draw(gfx::DrawList &list, chess::Piece piece, const gfx::Rect &square,
              gfx::Color tint = {1, 1, 1, 1}) const;

  private:
    struct Level
    {
        std::uint32_t texture = 0;
        int cell = 0;
    };
    Level levels_[kLevels];
    float surface_scale_ = 1.0f;
    int set_ = -1;
};

} // namespace pch::board
