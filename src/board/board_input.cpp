// ProsperoLichess - Controller input on the board: cursor, selection, premoves, promotion.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "board/board_input.hpp"

#include <algorithm>

namespace pch::board
{

namespace
{

using chess::Square;

constexpr chess::Role kPromotionRoles[4] = {chess::Role::queen, chess::Role::knight,
                                            chess::Role::rook, chess::Role::bishop};

// The same position with the other side to move (en passant cleared): lets
// premoves list the targets a piece will have after the opponent's reply.
bool flipped(const chess::Position &position, chess::Position *out)
{
    std::string fen = position.fen();
    std::size_t space = fen.find(' ');
    if (space == std::string::npos || space + 1 >= fen.size())
        return false;
    fen[space + 1] = position.turn() == chess::Color::white ? 'b' : 'w';
    // Fields: placement turn castling ep halfmove fullmove.
    std::size_t castling = fen.find(' ', space + 1);
    std::size_t ep =
        castling == std::string::npos ? std::string::npos : fen.find(' ', castling + 1);
    if (ep != std::string::npos)
    {
        const std::size_t end = fen.find(' ', ep + 1);
        fen.replace(ep + 1, (end == std::string::npos ? fen.size() : end) - ep - 1, "-");
    }
    return chess::Position::from_fen(fen, out);
}

int screen_index(Square square, chess::Color orientation)
{
    const int file = chess::file_of(square);
    const int rank = chess::rank_of(square);
    const int col = orientation == chess::Color::white ? file : 7 - file;
    const int row = orientation == chess::Color::white ? 7 - rank : rank;
    return row * 8 + col;
}

} // namespace

void BoardInput::place_cursor(Square square)
{
    if (square != chess::kNoSquare)
        cursor_ = square;
}

void BoardInput::clear_selection()
{
    selected_ = chess::kNoSquare;
    dests_.clear();
    moves_.clear();
    promotion_ = {};
}

bool BoardInput::take_premove(const chess::Position &position, chess::Move *move)
{
    if (!premove_.valid())
        return false;
    chess::Move candidate = premove_;
    premove_ = {};
    if (const auto piece = position.piece_at(candidate.from);
        piece && piece->role == chess::Role::pawn &&
        (chess::rank_of(candidate.to) == 7 || chess::rank_of(candidate.to) == 0) &&
        !candidate.promotion)
        candidate.promotion = chess::Role::queen;
    if (!position.is_legal(candidate))
        return false;
    *move = candidate;
    return true;
}

void BoardInput::targets(const chess::Position &position, Square from, bool premove,
                         std::vector<Square> *out, std::vector<chess::Move> *moves) const
{
    out->clear();
    moves->clear();
    chess::MoveList list;
    if (premove)
    {
        chess::Position other;
        if (!flipped(position, &other))
            return;
        other.legal_moves_from(from, list);
    }
    else
    {
        position.legal_moves_from(from, list);
    }
    for (const chess::Move &m : list)
    {
        moves->push_back(m);
        if (std::find(out->begin(), out->end(), m.to) == out->end())
            out->push_back(m.to);
    }
}

bool BoardInput::can_control(const chess::Position &position, Square square,
                             const InputRules &rules, bool *premove) const
{
    const auto piece = position.piece_at(square);
    if (!piece || rules.locked)
        return false;
    const bool mine = piece->color == chess::Color::white ? rules.white : rules.black;
    if (!mine)
        return false;
    *premove = piece->color != position.turn();
    return !*premove || rules.premoves;
}

void BoardInput::cycle(const chess::Position &position, chess::Color orientation,
                       const InputRules &rules, int step, ui::Feedback &feedback)
{
    std::vector<Square> candidates;
    if (selected_ != chess::kNoSquare)
    {
        candidates = dests_;
    }
    else
    {
        std::vector<Square> scratch;
        std::vector<chess::Move> scratch_moves;
        for (Square square = 0; square < 64; ++square)
        {
            bool premove = false;
            if (!can_control(position, square, rules, &premove))
                continue;
            targets(position, square, premove, &scratch, &scratch_moves);
            if (!scratch.empty())
                candidates.push_back(square);
        }
    }
    if (candidates.empty())
        return;
    std::sort(candidates.begin(), candidates.end(), [&](Square a, Square b)
              { return screen_index(a, orientation) < screen_index(b, orientation); });
    const int here = screen_index(cursor_, orientation);
    std::size_t next = 0;
    if (step > 0)
    {
        next = 0;
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            if (screen_index(candidates[i], orientation) > here)
            {
                next = i;
                break;
            }
        }
    }
    else
    {
        next = candidates.size() - 1;
        for (std::size_t i = candidates.size(); i-- > 0;)
        {
            if (screen_index(candidates[i], orientation) < here)
            {
                next = i;
                break;
            }
        }
    }
    cursor_ = candidates[next];
    feedback.play(audio::Cue::cursor);
}

BoardInput::Result BoardInput::update(const InputFrame &input, float dt,
                                      const chess::Position &position, chess::Color orientation,
                                      const InputRules &rules, ui::Feedback &feedback,
                                      chess::Move *move)
{
    picker_.target = promotion_.valid() ? 1.0f : 0.0f;
    picker_.update(dt, 20.0f);

    // Promotion picker: up/down choose, Cross confirms, Circle cancels.
    if (promotion_.valid())
    {
        if (input.nav == Direction::up || input.nav == Direction::left)
        {
            promotion_choice_ = (promotion_choice_ + 3) % 4;
            feedback.play(audio::Cue::focus);
        }
        else if (input.nav == Direction::down || input.nav == Direction::right)
        {
            promotion_choice_ = (promotion_choice_ + 1) % 4;
            feedback.play(audio::Cue::focus);
        }
        if (input.is_pressed(Action::confirm))
        {
            chess::Move chosen = promotion_;
            chosen.promotion = kPromotionRoles[promotion_choice_];
            const bool premove = selected_is_premove_;
            clear_selection();
            if (premove)
            {
                premove_ = chosen;
                feedback.play(audio::Cue::premove);
                return Result::premove;
            }
            *move = chosen;
            return Result::move;
        }
        if (input.is_pressed(Action::back))
        {
            promotion_ = {};
            feedback.play(audio::Cue::back);
        }
        return Result::none;
    }

    // Selection made stale by the position changing under it.
    if (selected_ != chess::kNoSquare)
    {
        bool premove = false;
        if (!can_control(position, selected_, rules, &premove) || premove != selected_is_premove_)
            clear_selection();
        else
            targets(position, selected_, premove, &dests_, &moves_);
    }

    if (input.nav != Direction::none)
    {
        int df = 0;
        int dr = 0;
        switch (input.nav)
        {
        case Direction::up:
            dr = 1;
            break;
        case Direction::down:
            dr = -1;
            break;
        case Direction::left:
            df = -1;
            break;
        case Direction::right:
            df = 1;
            break;
        case Direction::none:
            break;
        }
        if (orientation == chess::Color::black)
        {
            df = -df;
            dr = -dr;
        }
        const int file = std::clamp(chess::file_of(cursor_) + df, 0, 7);
        const int rank = std::clamp(chess::rank_of(cursor_) + dr, 0, 7);
        const Square next = chess::make_square(file, rank);
        if (next != cursor_)
        {
            cursor_ = next;
            feedback.play(audio::Cue::cursor);
        }
    }
    if (input.is_pressed(Action::page_next))
        cycle(position, orientation, rules, 1, feedback);
    if (input.is_pressed(Action::page_prev))
        cycle(position, orientation, rules, -1, feedback);

    if (input.is_pressed(Action::back))
    {
        if (selected_ != chess::kNoSquare)
        {
            clear_selection();
            feedback.play(audio::Cue::back);
            return Result::none;
        }
        if (premove_.valid())
        {
            premove_ = {};
            feedback.play(audio::Cue::back);
            return Result::none;
        }
        return Result::back;
    }

    if (!input.is_pressed(Action::confirm))
        return Result::none;

    if (selected_ != chess::kNoSquare &&
        std::find(dests_.begin(), dests_.end(), cursor_) != dests_.end())
    {
        chess::Move chosen;
        bool needs_promotion = false;
        for (const chess::Move &m : moves_)
        {
            if (m.to != cursor_)
                continue;
            if (m.promotion)
                needs_promotion = true;
            if (!chosen.valid() || (m.promotion && *m.promotion == chess::Role::queen))
                chosen = m;
        }
        if (needs_promotion && !rules.auto_queen)
        {
            promotion_ = chosen;
            promotion_.promotion.reset();
            promotion_choice_ = 0;
            feedback.play(audio::Cue::modal_open);
            return Result::none;
        }
        const bool premove = selected_is_premove_;
        clear_selection();
        if (premove)
        {
            premove_ = chosen;
            feedback.play(audio::Cue::premove);
            return Result::premove;
        }
        *move = chosen;
        return Result::move;
    }

    bool premove = false;
    if (can_control(position, cursor_, rules, &premove))
    {
        if (selected_ == cursor_)
        {
            clear_selection();
            feedback.play(audio::Cue::back);
            return Result::none;
        }
        std::vector<Square> dests;
        std::vector<chess::Move> moves;
        targets(position, cursor_, premove, &dests, &moves);
        if (dests.empty())
        {
            feedback.play(audio::Cue::illegal);
            return Result::none;
        }
        selected_ = cursor_;
        selected_is_premove_ = premove;
        dests_ = std::move(dests);
        moves_ = std::move(moves);
        feedback.play(audio::Cue::pickup);
        return Result::none;
    }
    feedback.play(audio::Cue::illegal);
    return Result::none;
}

void BoardInput::fill_overlay(Overlay &overlay) const
{
    overlay.selected = selected_;
    overlay.dests = dests_;
    overlay.premove = premove_;
    overlay.cursor = cursor_;
}

void BoardInput::draw_promotion(gfx::DrawList &list, const PieceAtlas &atlas, const BoardView &view,
                                const gfx::Rect &area, const chess::Position &position,
                                gfx::Color focus_color) const
{
    const float t = picker_.value;
    if (t < 0.01f || !promotion_.valid())
        return;
    const auto piece = position.piece_at(promotion_.from);
    const chess::Color color = piece ? piece->color : position.turn();
    list.rounded_rect(area, 0, gfx::Color::rgb(0x000000, 0.45f * t));
    const gfx::Rect target = view.square_rect(area, promotion_.to);
    // The column grows away from the edge the pawn promotes on.
    const bool downward = target.y < area.y + area.h * 0.5f;
    for (int i = 0; i < 4; ++i)
    {
        const float offset = static_cast<float>(i) * target.h * tween::cubic_out(t);
        gfx::Rect r = target;
        r.y = downward ? target.y + offset : target.y - offset;
        const bool focused = i == promotion_choice_;
        list.shadow({r.x + 6, r.y + 8, r.w - 12, r.h - 8}, r.w * 0.5f, 18,
                    gfx::Color::rgb(0x000000, 0.5f * t));
        list.circle(r.x + r.w * 0.5f, r.y + r.h * 0.5f, r.w * 0.48f,
                    focused ? focus_color.with_alpha(t) : gfx::Color::rgb(0xe8e3d8, t));
        const float inset = r.w * (focused ? 0.04f : 0.12f);
        atlas.draw(list, {color, kPromotionRoles[i]},
                   {r.x + inset, r.y + inset, r.w - 2 * inset, r.h - 2 * inset},
                   gfx::Color{1, 1, 1, t});
    }
}

} // namespace pch::board
