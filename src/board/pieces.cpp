// ProsperoLichess - Piece set atlases baked by tools/bake-pieces.py.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "board/pieces.hpp"

#include "core/save_file.hpp"

#include <cstring>

namespace pch::board
{

namespace
{

std::uint16_t read_u16(std::string_view data, std::size_t offset)
{
    return static_cast<std::uint16_t>(static_cast<std::uint8_t>(data[offset]) |
                                      (static_cast<std::uint8_t>(data[offset + 1]) << 8));
}

int column_of(chess::Role role)
{
    switch (role)
    {
    case chess::Role::king:
        return 0;
    case chess::Role::queen:
        return 1;
    case chess::Role::rook:
        return 2;
    case chess::Role::bishop:
        return 3;
    case chess::Role::knight:
        return 4;
    case chess::Role::pawn:
        return 5;
    }
    return 5;
}

} // namespace

bool parse_atlas(std::string_view data, AtlasImage *out, std::string *error)
{
    if (data.size() < 12 || data.substr(0, 4) != "PCHA")
    {
        *error = "not a piece atlas";
        return false;
    }
    const int width = read_u16(data, 4);
    const int height = read_u16(data, 6);
    const int cell = read_u16(data, 8);
    if (cell == 0 || width != cell * 6 || height != cell * 2)
    {
        *error = "bad atlas geometry";
        return false;
    }
    const std::size_t bytes =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    if (data.size() != 12 + bytes)
    {
        *error = "atlas size mismatch";
        return false;
    }
    out->width = width;
    out->height = height;
    out->cell = cell;
    out->pixels.assign(data.substr(12));
    return true;
}

gfx::Rect piece_uv(chess::Piece piece)
{
    const float u = static_cast<float>(column_of(piece.role)) / 6.0f;
    const float v = piece.color == chess::Color::white ? 0.0f : 0.5f;
    return {u, v, 1.0f / 6.0f, 0.5f};
}

namespace
{

// Halves an RGBA image with a box filter. Colours are weighted by alpha so a
// transparent neighbour does not darken a piece's edge.
std::string halve(const std::string &pixels, int width, int height)
{
    const int w = width / 2;
    const int h = height / 2;
    std::string out(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4, '\0');
    const auto *in = reinterpret_cast<const std::uint8_t *>(pixels.data());
    auto *dst = reinterpret_cast<std::uint8_t *>(out.data());
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            unsigned sum[3] = {0, 0, 0};
            unsigned alpha = 0;
            for (int dy = 0; dy < 2; ++dy)
            {
                for (int dx = 0; dx < 2; ++dx)
                {
                    const std::uint8_t *p = in + (static_cast<std::size_t>(y * 2 + dy) *
                                                      static_cast<std::size_t>(width) +
                                                  static_cast<std::size_t>(x * 2 + dx)) *
                                                     4;
                    for (int c = 0; c < 3; ++c)
                        sum[c] += static_cast<unsigned>(p[c]) * p[3];
                    alpha += p[3];
                }
            }
            std::uint8_t *q = dst + (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                                     static_cast<std::size_t>(x)) *
                                        4;
            for (int c = 0; c < 3; ++c)
                q[c] = static_cast<std::uint8_t>(alpha == 0 ? 0 : (sum[c] + alpha / 2) / alpha);
            q[3] = static_cast<std::uint8_t>((alpha + 2) / 4);
        }
    }
    return out;
}

} // namespace

bool PieceAtlas::load(const std::string &assets, int set, float square_output_pixels,
                      float surface_scale, const TextureFactory &create, std::string *error)
{
    if (set < 0 || set >= kPieceSetCount)
        set = 0;
    // The 128 px atlas serves 1080p squares (120 px); larger squares use 256.
    const int cell = square_output_pixels > 150.0f ? 256 : 128;
    std::string data;
    const std::string path =
        assets + "/pieces/" + kPieceSets[set].id + "-" + std::to_string(cell) + ".pcha";
    if (!save::read_file(path, &data, 8u << 20))
    {
        *error = "cannot read " + path;
        return false;
    }
    AtlasImage image;
    if (!parse_atlas(data, &image, error))
        return false;
    forget();
    surface_scale_ = surface_scale > 0.0f ? surface_scale : 1.0f;
    for (Level &level : levels_)
    {
        level.texture = create(image.width, image.height,
                               reinterpret_cast<const std::uint8_t *>(image.pixels.data()));
        level.cell = image.cell;
        if (level.texture == 0 || image.cell <= 32)
            break;
        image.pixels = halve(image.pixels, image.width, image.height);
        image.width /= 2;
        image.height /= 2;
        image.cell /= 2;
    }
    set_ = set;
    return levels_[0].texture != 0;
}

void PieceAtlas::draw(gfx::DrawList &list, chess::Piece piece, const gfx::Rect &square,
                      gfx::Color tint) const
{
    // The smallest level that is not magnified at this size.
    const Level *best = &levels_[0];
    const float needed = square.w * surface_scale_ * 0.92f;
    for (const Level &level : levels_)
    {
        if (level.texture != 0 && static_cast<float>(level.cell) >= needed)
            best = &level;
    }
    if (best->texture == 0)
        return;
    // Half a texel in from the cell's edge: the neighbour never bleeds in.
    gfx::Rect uv = piece_uv(piece);
    const float inset = 0.5f / static_cast<float>(best->cell);
    uv.x += inset / 6.0f;
    uv.y += inset / 2.0f;
    uv.w -= inset / 3.0f;
    uv.h -= inset;
    list.image(best->texture, square, uv, tint);
}

} // namespace pch::board
