// ProsperoLichess - Animated chess board renderer (chessground-style).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "board/board_view.hpp"

#include <algorithm>
#include <cmath>

namespace pch::board
{

namespace
{

using chess::Square;
using gfx::Color;

constexpr float kMoveSeconds = 0.2f;
constexpr float kFlipSeconds = 0.45f;
constexpr float kShakeSeconds = 0.35f;

// Lichess-like highlight colours.
const Color kLastMove = Color::rgb(0x9bc700, 0.41f);
const Color kSelected = Color::rgb(0x14551e, 0.5f);
const Color kDest = Color::rgb(0x14551e, 0.5f);
const Color kPremove = Color::rgb(0x141e55, 0.45f);
const Color kCheck = Color::rgb(0xff0000, 1.0f);

void column_row(chess::Color orientation, Square square, float *col, float *row)
{
    const int file = chess::file_of(square);
    const int rank = chess::rank_of(square);
    if (orientation == chess::Color::white)
    {
        *col = static_cast<float>(file);
        *row = static_cast<float>(7 - rank);
    }
    else
    {
        *col = static_cast<float>(7 - file);
        *row = static_cast<float>(rank);
    }
}

} // namespace

BoardView::BoardView()
{
    position_ = chess::Position::start();
    cursor_alpha_.snap(0.0f);
}

void BoardView::snap(const chess::Position &position)
{
    position_ = position;
    moving_count_ = 0;
    has_captured_ = false;
    move_timer_ = {};
}

void BoardView::play(const chess::Position &before, const chess::Move &move,
                     const chess::Position &after, float speed)
{
    position_ = after;
    moving_count_ = 0;
    has_captured_ = false;
    if (speed <= 0.0f || !move.valid())
    {
        move_timer_ = {};
        return;
    }
    const auto piece = before.piece_at(move.from);
    if (!piece)
    {
        move_timer_ = {};
        return;
    }
    Sprite main;
    main.piece = after.piece_at(move.to).value_or(*piece); // promotions show the new piece
    main.from = move.from;
    main.to = move.to;
    moving_[moving_count_++] = main;
    Square rook_from = chess::kNoSquare;
    Square rook_to = chess::kNoSquare;
    if (before.is_castle(move) && before.castle_rook_squares(move, &rook_from, &rook_to))
    {
        // Standard castling moves the king two squares; the move's "to" may be
        // the rook square in king-takes-rook notation.
        Square king_to = move.to;
        const int rank = chess::rank_of(move.from);
        if (chess::file_of(rook_to) == 5)
            king_to = chess::make_square(6, rank);
        else if (chess::file_of(rook_to) == 3)
            king_to = chess::make_square(2, rank);
        moving_[0].to = king_to;
        Sprite rook;
        rook.piece = {piece->color, chess::Role::rook};
        rook.from = rook_from;
        rook.to = rook_to;
        moving_[moving_count_++] = rook;
    }
    else if (before.is_capture(move))
    {
        Square victim = move.to;
        if (before.is_en_passant(move))
            victim = chess::make_square(chess::file_of(move.to), chess::rank_of(move.from));
        if (const auto taken = before.piece_at(victim))
        {
            captured_ = {*taken, victim, victim};
            has_captured_ = true;
        }
    }
    move_timer_.start(kMoveSeconds / speed);
    // Effects: chips fly off a captured piece; check and mate ring the king.
    if (has_captured_)
    {
        const bool light = captured_.piece.color == chess::Color::white;
        burst(captured_.to, light ? Color::rgb(0xf4f1ea) : Color::rgb(0x2b2b36), 14, 3.2f);
        ripple(captured_.to, Color::rgb(0xffffff, 0.5f), 0.9f, 0.35f);
    }
    if (after.in_check())
    {
        const Square king = after.king_square(after.turn());
        const bool mate = after.is_checkmate();
        ripple(king, kCheck.with_alpha(0.8f), mate ? 3.2f : 1.5f, mate ? 0.9f : 0.5f);
        if (mate)
        {
            ripple(king, kCheck.with_alpha(0.6f), 5.0f, 1.3f);
            burst(king, Color::rgb(0xff5a5a), 26, 4.2f);
            shake(king);
        }
    }
}

void BoardView::deal()
{
    deal_timer_.start(0.75f);
}

float BoardView::random01()
{
    rng_ = rng_ * 1664525u + 1013904223u;
    return static_cast<float>(rng_ >> 8) / 16777216.0f;
}

void BoardView::burst(Square square, Color color, int count, float speed)
{
    float col = 0.0f;
    float row = 0.0f;
    column_row(orientation_, square, &col, &row);
    for (int i = 0; i < count; ++i)
    {
        const float angle = random01() * 6.2831853f;
        const float velocity = speed * (0.35f + 0.65f * random01());
        Particle particle;
        particle.x = col + 0.5f;
        particle.y = row + 0.5f;
        particle.vx = std::cos(angle) * velocity;
        particle.vy = std::sin(angle) * velocity - 0.8f;
        particle.age = 0.0f;
        particle.life = 0.35f + 0.35f * random01();
        particle.size = 0.035f + 0.05f * random01();
        particle.color = color;
        particles_.push_back(particle);
    }
}

void BoardView::ripple(Square square, Color color, float reach, float life)
{
    float col = 0.0f;
    float row = 0.0f;
    column_row(orientation_, square, &col, &row);
    ripples_.push_back({col + 0.5f, row + 0.5f, 0.0f, life, reach, color});
}

void BoardView::set_orientation(chess::Color side, bool animate)
{
    if (side == orientation_)
        return;
    orientation_ = side;
    if (animate)
        flip_timer_.start(kFlipSeconds);
    cursor_placed_ = false;
}

void BoardView::shake(Square square)
{
    shake_square_ = square;
    shake_timer_.start(kShakeSeconds);
}

void BoardView::update(float dt, const Overlay &overlay)
{
    move_timer_.update(dt);
    flip_timer_.update(dt);
    shake_timer_.update(dt);
    deal_timer_.update(dt);
    for (Particle &particle : particles_)
    {
        particle.age += dt;
        particle.x += particle.vx * dt;
        particle.y += particle.vy * dt;
        particle.vy += 7.0f * dt; // gravity, in squares per second squared
        particle.vx *= 1.0f - 1.8f * dt;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                    [](const Particle &q) { return q.age >= q.life; }),
                     particles_.end());
    for (Ripple &r : ripples_)
        r.age += dt;
    ripples_.erase(std::remove_if(ripples_.begin(), ripples_.end(),
                                  [](const Ripple &r) { return r.age >= r.life; }),
                   ripples_.end());
    if (overlay.cursor != chess::kNoSquare)
    {
        float col = 0.0f;
        float row = 0.0f;
        column_row(orientation_, overlay.cursor, &col, &row);
        if (!cursor_placed_)
        {
            cursor_x_.snap(col);
            cursor_y_.snap(row);
            cursor_placed_ = true;
        }
        cursor_x_.target = col;
        cursor_y_.target = row;
        cursor_alpha_.target = 1.0f;
    }
    else
    {
        cursor_alpha_.target = 0.0f;
    }
    cursor_x_.update(dt, 22.0f);
    cursor_y_.update(dt, 22.0f);
    cursor_alpha_.update(dt, 14.0f);
    if (overlay.selected != last_selected_)
    {
        last_selected_ = overlay.selected;
        lift_.snap(0.0f);
    }
    lift_.target = overlay.selected != chess::kNoSquare ? 1.0f : 0.0f;
    lift_.update(dt, 18.0f);
}

gfx::Rect BoardView::square_rect(const gfx::Rect &area, Square square) const
{
    float col = 0.0f;
    float row = 0.0f;
    column_row(orientation_, square, &col, &row);
    const float size = area.w / 8.0f;
    return {area.x + col * size, area.y + row * size, size, size};
}

void BoardView::draw(gfx::DrawList &list, const ui::Fonts &fonts, const PieceAtlas &atlas,
                     const gfx::Rect &area, const Overlay &overlay, const BoardTheme &theme,
                     float time) const
{
    const float size = area.w / 8.0f;
    const Color light = Color::rgb(theme.light);
    const Color dark = Color::rgb(theme.dark);

    // A flip spins the whole board through a quick shrink-and-fade.
    float flip_scale = 1.0f;
    float flip_alpha = 1.0f;
    if (flip_timer_.running)
    {
        const float p = flip_timer_.progress();
        const float dip = std::sin(p * 3.14159265f);
        flip_scale = 1.0f - 0.08f * dip;
        flip_alpha = 1.0f - 0.55f * dip;
    }
    list.push_transform(flip_scale, area.x + area.w * 0.5f, area.y + area.h * 0.5f, 0.0f, 0.0f);

    // Frame: a deep bevelled border with a soft floor shadow.
    const float frame = size * 0.16f;
    list.shadow({area.x - frame + 10, area.y - frame + 30, area.w + 2 * frame - 20,
                 area.h + 2 * frame - 10},
                22, 46, Color::rgb(0x000000, 0.6f));
    list.gradient_rect({area.x - frame, area.y - frame, area.w + 2 * frame, area.h + 2 * frame},
                       frame * 0.9f, Color::rgb(theme.frame_top), Color::rgb(theme.frame_bottom));
    list.bordered_rect({area.x - frame, area.y - frame, area.w + 2 * frame, area.h + 2 * frame},
                       frame * 0.9f, Color::rgb(0x000000, 0.0f), 1.5f, Color::rgb(0xffffff, 0.12f));
    list.push_opacity(flip_alpha);
    list.board(area, light, dark, theme.style, static_cast<float>(theme.light % 97));
    // Inner shadow line where the squares meet the frame.
    list.bordered_rect({area.x - 1, area.y - 1, area.w + 2, area.h + 2}, 3,
                       Color::rgb(0x000000, 0.0f), 2.0f, Color::rgb(0x000000, 0.35f));

    const auto fill_square = [&](Square square, Color color)
    {
        if (square == chess::kNoSquare)
            return;
        list.rounded_rect(square_rect(area, square), 0.0f, color);
    };

    if (overlay.last_move.valid())
    {
        fill_square(overlay.last_move.from, kLastMove);
        fill_square(overlay.last_move.to, kLastMove);
    }
    if (overlay.premove.valid())
    {
        fill_square(overlay.premove.from, kPremove);
        fill_square(overlay.premove.to, kPremove);
    }
    fill_square(overlay.selected, kSelected);
    if (overlay.feedback > 0.0f)
    {
        fill_square(overlay.good, Color::rgb(0x2fbf5a, 0.55f * overlay.feedback));
        fill_square(overlay.bad, Color::rgb(0xe0413a, 0.6f * overlay.feedback));
    }

    // Check: a red radial glow under the king.
    if (position_.in_check() && !overlay.blindfold)
    {
        const gfx::Rect k = square_rect(area, position_.king_square(position_.turn()));
        const float pulse = 0.85f + 0.15f * std::sin(time * 6.0f);
        list.push_clip(k);
        list.shadow({k.x + k.w * 0.18f, k.y + k.h * 0.18f, k.w * 0.64f, k.h * 0.64f}, k.w * 0.32f,
                    k.w * 0.3f, kCheck.with_alpha(0.9f * pulse));
        list.pop_clip();
    }

    // Coordinates in the edge squares, drawn in the opposite square colour.
    if (overlay.coordinates)
    {
        const float text = size * 0.17f;
        for (int i = 0; i < 8; ++i)
        {
            const int file = orientation_ == chess::Color::white ? i : 7 - i;
            const int rank = orientation_ == chess::Color::white ? 7 - i : i;
            const bool bottom_light = (i + 7) % 2 == 0;
            const char file_name[2] = {static_cast<char>('a' + file), 0};
            ui::text(list, fonts.semibold, file_name,
                     area.x + (static_cast<float>(i) + 1.0f) * size - size * 0.07f,
                     area.y + area.h - size * 0.07f, text, bottom_light ? dark : light,
                     gfx::Align::right);
            const bool left_light = i % 2 == 0;
            const char rank_name[2] = {static_cast<char>('1' + rank), 0};
            ui::text(list, fonts.semibold, rank_name, area.x + size * 0.06f,
                     area.y + static_cast<float>(i) * size + text * 1.05f, text,
                     left_light ? dark : light);
        }
    }

    // Pieces. Squares taking part in a running animation are drawn after.
    const float p = move_timer_.running ? tween::cubic_out(move_timer_.progress()) : 1.0f;
    const auto is_moving_to = [&](Square square)
    {
        if (!move_timer_.running)
            return false;
        for (int m = 0; m < moving_count_; ++m)
            if (moving_[static_cast<std::size_t>(m)].to == square)
                return true;
        return false;
    };
    const auto piece_rect = [&](const gfx::Rect &r, float scale)
    {
        const float inset = r.w * (1.0f - scale) * 0.5f;
        return gfx::Rect{r.x + inset, r.y + inset, r.w * scale, r.h * scale};
    };
    const auto shadow_under = [&](const gfx::Rect &r, float strength)
    {
        list.shadow({r.x + r.w * 0.2f, r.y + r.h * 0.74f, r.w * 0.6f, r.h * 0.16f}, r.h * 0.08f,
                    r.w * 0.08f, Color::rgb(0x000000, 0.18f * strength));
    };
    if (!overlay.blindfold)
    {
        for (Square square = 0; square < 64; ++square)
        {
            const auto piece = position_.piece_at(square);
            if (!piece || is_moving_to(square) || square == overlay.selected)
                continue;
            gfx::Rect r = square_rect(area, square);
            if (shake_timer_.running && square == shake_square_)
            {
                const float s = shake_timer_.progress();
                r.x += std::sin(s * 40.0f) * size * 0.07f * (1.0f - s);
            }
            if (deal_timer_.running)
            {
                // Pieces arrive rank by rank, dropping in with a small bounce.
                float col = 0.0f;
                float row = 0.0f;
                column_row(orientation_, square, &col, &row);
                const float delay = (7.0f - row) * 0.045f + col * 0.012f;
                const float local = tween::clamp01((deal_timer_.elapsed - delay) / 0.32f);
                if (local <= 0.0f)
                    continue;
                const float e = tween::back_out(local);
                r.y -= size * 0.45f * (1.0f - e);
                shadow_under(r, local);
                atlas.draw(list, *piece, piece_rect(r, 0.95f), Color{1, 1, 1, local});
                continue;
            }
            shadow_under(r, 1.0f);
            atlas.draw(list, *piece, piece_rect(r, 0.95f));
        }
        if (has_captured_ && move_timer_.running)
        {
            const gfx::Rect r = square_rect(area, captured_.to);
            const float fade = 1.0f - p;
            atlas.draw(list, captured_.piece, piece_rect(r, 0.95f * (0.6f + 0.4f * fade)),
                       Color{1, 1, 1, fade});
        }
        if (move_timer_.running)
        {
            for (int m = 0; m < moving_count_; ++m)
            {
                const Sprite &s = moving_[static_cast<std::size_t>(m)];
                const gfx::Rect a = square_rect(area, s.from);
                const gfx::Rect b = square_rect(area, s.to);
                const float arc = std::sin(p * 3.14159265f) * 0.06f;
                // A soft trail from the origin square fades as the piece arrives.
                list.line(a.x + a.w * 0.5f, a.y + a.h * 0.5f, tween::lerp(a.x, b.x, p) + a.w * 0.5f,
                          tween::lerp(a.y, b.y, p) + a.h * 0.5f, size * 0.2f,
                          Color::rgb(0xffffff, 0.16f * (1.0f - p)));
                gfx::Rect r{tween::lerp(a.x, b.x, p), tween::lerp(a.y, b.y, p), a.w, a.h};
                shadow_under(r, 1.0f + 2.0f * arc);
                atlas.draw(list, s.piece, piece_rect(r, 0.95f + arc));
            }
        }
    }

    // Legal destinations: dots on empty squares, rings on captures.
    if (overlay.show_dests)
    {
        for (Square square : overlay.dests)
        {
            const gfx::Rect r = square_rect(area, square);
            const float cx = r.x + r.w * 0.5f;
            const float cy = r.y + r.h * 0.5f;
            if (position_.piece_at(square))
                list.ring(cx, cy, r.w * 0.44f, r.w * 0.09f, kDest);
            else
                list.circle(cx, cy, r.w * 0.14f, kDest);
        }
    }

    // The selected piece floats above the board.
    if (overlay.selected != chess::kNoSquare && !overlay.blindfold)
    {
        if (const auto piece = position_.piece_at(overlay.selected))
        {
            const float lift = lift_.value;
            gfx::Rect r = square_rect(area, overlay.selected);
            const float bob = std::sin(time * 4.0f) * size * 0.012f * lift;
            list.shadow({r.x + r.w * 0.16f, r.y + r.h * 0.76f, r.w * 0.68f, r.h * 0.18f},
                        r.h * 0.09f, r.w * (0.08f + 0.06f * lift),
                        Color::rgb(0x000000, 0.18f + 0.22f * lift));
            r.y -= size * 0.08f * lift + bob;
            atlas.draw(list, *piece, piece_rect(r, 0.95f + 0.1f * lift));
        }
    }

    for (const Arrow &arrow : overlay.arrows)
    {
        if (arrow.from == chess::kNoSquare || arrow.to == chess::kNoSquare)
            continue;
        const gfx::Rect a = square_rect(area, arrow.from);
        const gfx::Rect b = square_rect(area, arrow.to);
        list.arrow(a.x + a.w * 0.5f, a.y + a.h * 0.5f, b.x + b.w * 0.5f, b.y + b.h * 0.5f,
                   size * 0.16f, size * 0.42f, arrow.color);
    }

    for (const Ripple &r : ripples_)
    {
        const float q = tween::clamp01(r.age / r.life);
        const float radius = size * (0.4f + r.reach * tween::cubic_out(q));
        list.push_clip(area);
        list.ring(area.x + r.x * size, area.y + r.y * size, radius, size * 0.07f * (1.0f - q),
                  r.color.with_alpha(1.0f - q));
        list.pop_clip();
    }
    for (const Particle &particle : particles_)
    {
        const float q = tween::clamp01(particle.age / particle.life);
        list.circle(area.x + particle.x * size, area.y + particle.y * size,
                    size * particle.size * (1.0f - 0.5f * q), particle.color.with_alpha(1.0f - q));
    }

    // Controller cursor: a glowing rounded frame that glides between squares.
    if (cursor_alpha_.value > 0.01f)
    {
        const float a = cursor_alpha_.value;
        const float pulse = 0.75f + 0.25f * std::sin(time * 5.0f);
        const gfx::Rect r{area.x + cursor_x_.value * size, area.y + cursor_y_.value * size, size,
                          size};
        // A lit square under a ring with a dark hairline either side of it, so
        // the ring reads on the light squares as well as on the dark ones.
        const Color c = overlay.cursor_color;
        const float width = std::max(size * 0.06f, 3.5f);
        const float edge = std::max(size * 0.018f, 1.5f);
        list.rounded_rect(r, 0.0f, c.with_alpha(0.3f * a * pulse));
        list.bordered_rect(r.inset(-edge), 3.0f, Color::rgb(0x000000, 0.0f), width + 2.0f * edge,
                           Color::rgb(0x1a1206, 0.7f * a));
        list.bordered_rect(r, 2.0f, c.with_alpha(0.0f), width, c.with_alpha(a));
    }
    list.pop_opacity();
    list.pop_transform();
}

} // namespace pch::board
