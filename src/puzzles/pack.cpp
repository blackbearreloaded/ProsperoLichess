// ProsperoLichess - Offline Lichess puzzle pack reader.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "puzzles/pack.hpp"

#include "core/save_file.hpp"
#include "core/strings.hpp"

#include <utility>

namespace pch::puzzles
{

namespace
{

constexpr char kMagic[4] = {'P', 'C', 'H', 'P'};
constexpr std::uint16_t kVersion = 1;
constexpr std::size_t kHeaderSize = 76;
constexpr std::size_t kRecordFixed = 5 + 2 + 1 + 1 + 1 + 1 + 2 + 8; // up to the nibbles
constexpr char kPieces[] = "PNBRQK";
constexpr char kPromotions[] = " nbrq";

std::uint64_t get_le(std::string_view data, std::size_t offset, std::size_t bytes)
{
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < bytes; ++i)
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(data[offset + i]))
                 << (8 * i);
    return value;
}

bool fail(std::string *error, const char *message)
{
    if (error)
        *error = message;
    return false;
}

bool is_alnum(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool valid_piece(unsigned code)
{
    return (code >= 1 && code <= 6) || (code >= 9 && code <= 14);
}

std::string square_name(unsigned square)
{
    return std::string{static_cast<char>('a' + square % 8), static_cast<char>('1' + square / 8)};
}

int popcount64(std::uint64_t value)
{
    int count = 0;
    for (; value; value &= value - 1)
        ++count;
    return count;
}

std::uint64_t next_random(std::uint64_t *state)
{
    std::uint64_t x = *state ? *state : 0x9e3779b97f4a7c15ull;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

// Checks one record of exactly `length` bytes against the format rules.
bool valid_record(std::string_view record, std::size_t mask_bytes, int theme_count)
{
    if (record.size() < kRecordFixed + 1 + mask_bytes)
        return false;
    for (std::size_t i = 0; i < 5; ++i)
        if (!is_alnum(record[i]))
            return false;
    const unsigned flags = static_cast<unsigned char>(record[8]);
    const unsigned ep = static_cast<unsigned char>(record[9]);
    if (flags & ~0x1fu)
        return false;
    if (ep != 0xff && !((ep >= 16 && ep < 24) || (ep >= 40 && ep < 48)))
        return false;
    const std::uint64_t occupancy = get_le(record, 13, 8);
    const int pieces = popcount64(occupancy);
    const std::size_t nibble_bytes = static_cast<std::size_t>(pieces + 1) / 2;
    std::size_t position = kRecordFixed;
    if (record.size() < position + nibble_bytes + 1 + mask_bytes)
        return false;
    for (int i = 0; i < pieces; ++i)
    {
        const unsigned byte = static_cast<unsigned char>(record[position + i / 2]);
        if (!valid_piece((i & 1) ? byte >> 4 : byte & 15))
            return false;
    }
    if ((pieces & 1) && (static_cast<unsigned char>(record[position + nibble_bytes - 1]) >> 4))
        return false;
    position += nibble_bytes;
    const std::size_t moves = static_cast<unsigned char>(record[position++]);
    if (moves < 2 || record.size() != position + 2 * moves + mask_bytes)
        return false;
    for (std::size_t i = 0; i < moves; ++i)
    {
        const auto move = static_cast<unsigned>(get_le(record, position + 2 * i, 2));
        if ((move >> 12) > 4 || (move & 63) == ((move >> 6) & 63))
            return false;
    }
    position += 2 * moves;
    for (std::size_t bit = static_cast<std::size_t>(theme_count); bit < mask_bytes * 8; ++bit)
        if ((static_cast<unsigned char>(record[position + bit / 8]) >> (bit % 8)) & 1)
            return false;
    return true;
}

} // namespace

bool Pack::load(std::string data, std::string *error)
{
    *this = Pack{};
    const std::string_view view(data);
    if (view.size() < kHeaderSize + 4)
        return fail(error, "puzzle pack: truncated header");
    if (view.substr(0, 4) != std::string_view(kMagic, 4))
        return fail(error, "puzzle pack: bad magic");
    if (get_le(view, 4, 2) != kVersion)
        return fail(error, "puzzle pack: unsupported version");
    if (get_le(view, 6, 2) != kHeaderSize || get_le(view, 72, 4) != view.size())
        return fail(error, "puzzle pack: size mismatch");
    const std::size_t size = view.size();
    if (pch::save::crc32(view.substr(0, size - 4)) != get_le(view, size - 4, 4))
        return fail(error, "puzzle pack: checksum mismatch");

    const std::size_t count = get_le(view, 8, 4);
    const int theme_count = static_cast<int>(get_le(view, 12, 2));
    const std::size_t mask_bytes = get_le(view, 14, 1);
    const int base = static_cast<int>(get_le(view, 16, 2));
    const int step = static_cast<int>(get_le(view, 18, 2));
    const std::size_t buckets = get_le(view, 20, 2);
    if (theme_count > kMaxThemes || mask_bytes != static_cast<std::size_t>(theme_count + 7) / 8 ||
        step <= 0 || buckets == 0 || get_le(view, 15, 1) != 0 || get_le(view, 22, 2) != 0)
        return fail(error, "puzzle pack: bad header fields");
    for (std::size_t i = 24; i < 34; ++i)
        if (view[i] == '\0')
            return fail(error, "puzzle pack: bad source date");

    const std::size_t end = size - 4;
    std::size_t position = kHeaderSize;
    std::vector<std::string> themes;
    for (int i = 0; i < theme_count; ++i)
    {
        if (position >= end)
            return fail(error, "puzzle pack: truncated theme table");
        const std::size_t length = static_cast<unsigned char>(view[position++]);
        if (length == 0 || length > end - position)
            return fail(error, "puzzle pack: bad theme name");
        std::string name(view.substr(position, length));
        for (char c : name)
            if (!is_alnum(c))
                return fail(error, "puzzle pack: bad theme name");
        for (const std::string &other : themes)
            if (other == name)
                return fail(error, "puzzle pack: duplicate theme name");
        themes.push_back(std::move(name));
        position += length;
    }

    const std::size_t index_at = position;
    if ((end - position) / 4 < buckets + 1)
        return fail(error, "puzzle pack: truncated rating index");
    position += 4 * (buckets + 1);
    const std::size_t offsets_at = position;
    if (count == 0 || (end - position) / 4 < count + 1)
        return fail(error, "puzzle pack: truncated offsets");
    position += 4 * (count + 1);
    const std::size_t records_at = position;
    const std::size_t blob = end - records_at;
    if (get_le(view, offsets_at, 4) != 0 || get_le(view, offsets_at + 4 * count, 4) != blob)
        return fail(error, "puzzle pack: bad record blob size");

    std::vector<std::size_t> theme_counts(static_cast<std::size_t>(theme_count), 0);
    int previous_rating = -1;
    std::size_t bucket = 0; // next rating index entry to check
    for (std::size_t i = 0; i <= count; ++i)
    {
        int rating = 1 << 30;
        std::string_view record;
        if (i < count)
        {
            const std::size_t begin = get_le(view, offsets_at + 4 * i, 4);
            const std::size_t next = get_le(view, offsets_at + 4 * (i + 1), 4);
            if (next <= begin || next > blob)
                return fail(error, "puzzle pack: bad record offsets");
            record = view.substr(records_at + begin, next - begin);
            if (!valid_record(record, mask_bytes, theme_count))
                return fail(error, "puzzle pack: corrupt record");
            rating = static_cast<int>(get_le(record, 5, 2));
        }
        // Every bucket boundary at or below this rating must point at it.
        while (bucket <= buckets &&
               (i == count || base + static_cast<int>(bucket) * step <= rating))
        {
            if (get_le(view, index_at + 4 * bucket, 4) != i)
                return fail(error, "puzzle pack: bad rating index");
            ++bucket;
        }
        if (i == count)
            break;
        if (rating < previous_rating || rating < base)
            return fail(error, "puzzle pack: records not sorted by rating");
        previous_rating = rating;
        const std::size_t mask_at = record.size() - mask_bytes;
        for (int t = 0; t < theme_count; ++t)
            if ((static_cast<unsigned char>(record[mask_at + t / 8]) >> (t % 8)) & 1)
                ++theme_counts[static_cast<std::size_t>(t)];
    }
    if (bucket != buckets + 1)
        return fail(error, "puzzle pack: rating index does not cover every puzzle");

    data_ = std::move(data);
    count_ = count;
    mask_bytes_ = mask_bytes;
    rating_base_ = base;
    rating_step_ = step;
    rating_buckets_ = buckets;
    rating_index_ = index_at;
    offsets_ = offsets_at;
    records_ = records_at;
    themes_ = std::move(themes);
    theme_counts_ = std::move(theme_counts);
    if (error)
        error->clear();
    return true;
}

std::size_t Pack::size() const
{
    return count_;
}

std::size_t Pack::record(std::size_t index) const
{
    return records_ + get_le(data_, offsets_ + 4 * index, 4);
}

int Pack::rating(std::size_t index) const
{
    return index < count_ ? static_cast<int>(get_le(data_, record(index) + 5, 2)) : 0;
}

bool Pack::get(std::size_t index, PackPuzzle *out) const
{
    if (index >= count_ || !out)
        return false;
    const std::string_view view(data_);
    const std::size_t at = record(index);
    PackPuzzle puzzle;
    puzzle.id.assign(view.substr(at, 5));
    puzzle.rating = static_cast<int>(get_le(view, at + 5, 2));
    puzzle.popularity = static_cast<int>(static_cast<std::int8_t>(get_le(view, at + 7, 1)));
    const unsigned flags = static_cast<unsigned>(get_le(view, at + 8, 1));
    const unsigned ep = static_cast<unsigned>(get_le(view, at + 9, 1));
    const unsigned halfmove = static_cast<unsigned>(get_le(view, at + 10, 1));
    const unsigned fullmove = static_cast<unsigned>(get_le(view, at + 11, 2));
    const std::uint64_t occupancy = get_le(view, at + 13, 8);

    unsigned board[64] = {};
    std::size_t position = at + kRecordFixed;
    int piece = 0;
    for (unsigned square = 0; square < 64; ++square)
    {
        if (!((occupancy >> square) & 1))
            continue;
        const unsigned byte = static_cast<unsigned char>(view[position + piece / 2]);
        board[square] = (piece & 1) ? byte >> 4 : byte & 15;
        ++piece;
    }
    position += static_cast<std::size_t>(piece + 1) / 2;

    std::string &fen = puzzle.fen;
    for (int rank = 7; rank >= 0; --rank)
    {
        int empty = 0;
        for (int file = 0; file < 8; ++file)
        {
            const unsigned code = board[rank * 8 + file];
            if (!code)
            {
                ++empty;
                continue;
            }
            if (empty)
                fen.push_back(static_cast<char>('0' + empty));
            empty = 0;
            const char letter = kPieces[(code & 7) - 1];
            fen.push_back((code & 8) ? static_cast<char>(letter - 'A' + 'a') : letter);
        }
        if (empty)
            fen.push_back(static_cast<char>('0' + empty));
        if (rank)
            fen.push_back('/');
    }
    fen += (flags & 1) ? " b " : " w ";
    const std::size_t castling_at = fen.size();
    for (int bit = 0; bit < 4; ++bit)
        if (flags & (2u << bit))
            fen.push_back("KQkq"[bit]);
    if (fen.size() == castling_at)
        fen.push_back('-');
    fen += ' ';
    fen += ep == 0xff ? std::string("-") : square_name(ep);
    fen += ' ' + std::to_string(halfmove) + ' ' + std::to_string(fullmove);

    const std::size_t moves = static_cast<unsigned char>(view[position++]);
    for (std::size_t i = 0; i < moves; ++i)
    {
        const auto move = static_cast<unsigned>(get_le(view, position + 2 * i, 2));
        std::string uci = square_name(move & 63) + square_name((move >> 6) & 63);
        if (move >> 12)
            uci.push_back(kPromotions[move >> 12]);
        puzzle.moves.push_back(std::move(uci));
    }
    position += 2 * moves;
    for (std::size_t t = 0; t < themes_.size(); ++t)
        if ((static_cast<unsigned char>(view[position + t / 8]) >> (t % 8)) & 1)
            puzzle.themes.set(static_cast<int>(t));
    *out = std::move(puzzle);
    return true;
}

int Pack::theme_count() const
{
    return static_cast<int>(themes_.size());
}

const std::string &Pack::theme_name(int index) const
{
    static const std::string empty;
    return index >= 0 && index < theme_count() ? themes_[static_cast<std::size_t>(index)] : empty;
}

int Pack::theme_index(std::string_view name) const
{
    for (std::size_t i = 0; i < themes_.size(); ++i)
        if (themes_[i] == name)
            return static_cast<int>(i);
    return -1;
}

std::size_t Pack::theme_puzzle_count(int theme) const
{
    return theme >= 0 && theme < theme_count() ? theme_counts_[static_cast<std::size_t>(theme)] : 0;
}

bool Pack::has_theme(std::size_t index, int theme) const
{
    if (theme < 0)
        return true;
    if (theme >= theme_count())
        return false;
    const std::size_t mask_at = record(index + 1) - mask_bytes_; // offsets[N] = blob size
    return (static_cast<unsigned char>(data_[mask_at + static_cast<std::size_t>(theme) / 8]) >>
            (theme % 8)) &
           1;
}

std::size_t Pack::lower_bound(int value) const
{
    if (count_ == 0 || value <= rating_base_)
        return 0;
    const std::size_t bucket = static_cast<std::size_t>((value - rating_base_) / rating_step_);
    if (bucket >= rating_buckets_)
        return count_;
    std::size_t low = get_le(data_, rating_index_ + 4 * bucket, 4);
    std::size_t high = get_le(data_, rating_index_ + 4 * (bucket + 1), 4);
    while (low < high)
    {
        const std::size_t mid = low + (high - low) / 2;
        if (rating(mid) < value)
            low = mid + 1;
        else
            high = mid;
    }
    return low;
}

bool Pack::pick(int min_rating, int max_rating, int theme, std::uint64_t *rng,
                const std::function<bool(std::size_t)> &seen, std::size_t *index) const
{
    if (!rng || !index || min_rating > max_rating || theme >= theme_count())
        return false;
    const std::size_t low = lower_bound(min_rating);
    const std::size_t high = max_rating >= 65535 ? count_ : lower_bound(max_rating + 1);
    if (low >= high)
        return false;
    const std::size_t span = high - low;
    const auto eligible = [&](std::size_t candidate)
    { return has_theme(candidate, theme) && !(seen && seen(candidate)); };
    // Random probes first; they stay uniform while most of the range is fresh.
    for (int attempt = 0; attempt < 32; ++attempt)
    {
        const std::size_t candidate = low + static_cast<std::size_t>(next_random(rng) % span);
        if (eligible(candidate))
        {
            *index = candidate;
            return true;
        }
    }
    // Then an exhaustive scan from a random start, wrapping around.
    const std::size_t start = static_cast<std::size_t>(next_random(rng) % span);
    for (std::size_t step = 0; step < span; ++step)
    {
        const std::size_t candidate = low + (start + step) % span;
        if (eligible(candidate))
        {
            *index = candidate;
            return true;
        }
    }
    return false;
}

std::string Pack::source_date() const
{
    if (data_.size() < kHeaderSize)
        return {};
    std::string date(data_.data() + 24, 16);
    date.resize(date.find('\0') == std::string::npos ? 16 : date.find('\0'));
    return date;
}

std::string Pack::source_sha256() const
{
    if (data_.size() < kHeaderSize)
        return {};
    static const char digits[] = "0123456789abcdef";
    std::string hex;
    for (std::size_t i = 40; i < 72; ++i)
    {
        const auto byte = static_cast<unsigned char>(data_[i]);
        hex.push_back(digits[byte >> 4]);
        hex.push_back(digits[byte & 15]);
    }
    return hex;
}

std::string theme_label(std::string_view name)
{
    // The themes of the Lichess puzzle database, by their keys, with the name
    // each is shown under. They are chess terms every language has its own
    // word for, so each name is a text of its own for the catalogs, kept
    // apart from the same word used elsewhere ("Long", "Mate", "Opening").
    struct Known
    {
        const char *name;
        const char *label;
    };
    static const Known known[] = {
        {"advancedPawn", TRC("puzzle theme", "Advanced pawn")},
        {"advantage", TRC("puzzle theme", "Advantage")},
        {"anastasiaMate", TRC("puzzle theme", "Anastasia's mate")},
        {"arabianMate", TRC("puzzle theme", "Arabian mate")},
        {"attackingF2F7", TRC("puzzle theme", "Attacking f2 or f7")},
        {"attraction", TRC("puzzle theme", "Attraction")},
        {"backRankMate", TRC("puzzle theme", "Back rank mate")},
        {"balestraMate", TRC("puzzle theme", "Balestra mate")},
        {"bishopEndgame", TRC("puzzle theme", "Bishop endgame")},
        {"blindSwineMate", TRC("puzzle theme", "Blind swine mate")},
        {"bodenMate", TRC("puzzle theme", "Boden's mate")},
        {"capturingDefender", TRC("puzzle theme", "Capturing defender")},
        {"castling", TRC("puzzle theme", "Castling")},
        {"clearance", TRC("puzzle theme", "Clearance")},
        {"collinearMove", TRC("puzzle theme", "Collinear move")},
        {"cornerMate", TRC("puzzle theme", "Corner mate")},
        {"crushing", TRC("puzzle theme", "Crushing")},
        {"defensiveMove", TRC("puzzle theme", "Defensive move")},
        {"deflection", TRC("puzzle theme", "Deflection")},
        {"discoveredAttack", TRC("puzzle theme", "Discovered attack")},
        {"discoveredCheck", TRC("puzzle theme", "Discovered check")},
        {"doubleBishopMate", TRC("puzzle theme", "Double bishop mate")},
        {"doubleCheck", TRC("puzzle theme", "Double check")},
        {"dovetailMate", TRC("puzzle theme", "Dovetail mate")},
        {"enPassant", TRC("puzzle theme", "En passant")},
        {"endgame", TRC("puzzle theme", "Endgame")},
        {"epauletteMate", TRC("puzzle theme", "Epaulette mate")},
        {"equality", TRC("puzzle theme", "Equality")},
        {"exposedKing", TRC("puzzle theme", "Exposed king")},
        {"fork", TRC("puzzle theme", "Fork")},
        {"hangingPiece", TRC("puzzle theme", "Hanging piece")},
        {"hookMate", TRC("puzzle theme", "Hook mate")},
        {"interference", TRC("puzzle theme", "Interference")},
        {"intermezzo", TRC("puzzle theme", "Intermezzo")},
        {"killBoxMate", TRC("puzzle theme", "Kill box mate")},
        {"kingsideAttack", TRC("puzzle theme", "Kingside attack")},
        {"knightEndgame", TRC("puzzle theme", "Knight endgame")},
        {"long", TRC("puzzle theme", "Long")},
        {"master", TRC("puzzle theme", "Master")},
        {"masterVsMaster", TRC("puzzle theme", "Master vs master")},
        {"mate", TRC("puzzle theme", "Mate")},
        {"mateIn1", TRC("puzzle theme", "Mate in 1")},
        {"mateIn2", TRC("puzzle theme", "Mate in 2")},
        {"mateIn3", TRC("puzzle theme", "Mate in 3")},
        {"mateIn4", TRC("puzzle theme", "Mate in 4")},
        {"mateIn5", TRC("puzzle theme", "Mate in 5 or more")},
        {"middlegame", TRC("puzzle theme", "Middlegame")},
        {"morphysMate", TRC("puzzle theme", "Morphy's mate")},
        {"oneMove", TRC("puzzle theme", "One move")},
        {"opening", TRC("puzzle theme", "Opening")},
        {"operaMate", TRC("puzzle theme", "Opera mate")},
        {"pawnEndgame", TRC("puzzle theme", "Pawn endgame")},
        {"pillsburysMate", TRC("puzzle theme", "Pillsbury's mate")},
        {"pin", TRC("puzzle theme", "Pin")},
        {"promotion", TRC("puzzle theme", "Promotion")},
        {"queenEndgame", TRC("puzzle theme", "Queen endgame")},
        {"queenRookEndgame", TRC("puzzle theme", "Queen rook endgame")},
        {"queensideAttack", TRC("puzzle theme", "Queenside attack")},
        {"quietMove", TRC("puzzle theme", "Quiet move")},
        {"rookEndgame", TRC("puzzle theme", "Rook endgame")},
        {"sacrifice", TRC("puzzle theme", "Sacrifice")},
        {"short", TRC("puzzle theme", "Short")},
        {"skewer", TRC("puzzle theme", "Skewer")},
        {"smotheredMate", TRC("puzzle theme", "Smothered mate")},
        {"superGM", TRC("puzzle theme", "Super GM")},
        {"swallowstailMate", TRC("puzzle theme", "Swallow's tail mate")},
        {"trappedPiece", TRC("puzzle theme", "Trapped piece")},
        {"triangleMate", TRC("puzzle theme", "Triangle mate")},
        {"underPromotion", TRC("puzzle theme", "Under promotion")},
        {"veryLong", TRC("puzzle theme", "Very long")},
        {"vukovicMate", TRC("puzzle theme", "Vukovic mate")},
        {"xRayAttack", TRC("puzzle theme", "X-ray attack")},
        {"zugzwang", TRC("puzzle theme", "Zugzwang")},
    };
    for (const Known &entry : known)
        if (name == entry.name)
            return trc("puzzle theme", entry.label);

    // A theme the table does not know (Lichess adds one now and then) keeps
    // its key, made readable. Split camelCase into words: lowercase runs,
    // Capitalized runs, ACRONYM runs and digit runs; keep acronyms upper-case,
    // lower-case the rest.
    std::string label;
    std::size_t i = 0;
    const auto upper = [](char c) { return c >= 'A' && c <= 'Z'; };
    const auto lower = [](char c) { return c >= 'a' && c <= 'z'; };
    const auto digit = [](char c) { return c >= '0' && c <= '9'; };
    while (i < name.size())
    {
        std::size_t j = i;
        bool acronym = false;
        if (digit(name[i]))
        {
            while (j < name.size() && digit(name[j]))
                ++j;
        }
        else if (upper(name[i]) && i + 1 < name.size() && upper(name[i + 1]))
        {
            while (j < name.size() && upper(name[j]) &&
                   !(j + 1 < name.size() && lower(name[j + 1]) && j > i))
                ++j;
            acronym = j - i > 1;
        }
        else
        {
            ++j;
            while (j < name.size() && lower(name[j]))
                ++j;
        }
        if (!label.empty())
            label.push_back(' ');
        for (std::size_t k = i; k < j; ++k)
        {
            char c = name[k];
            if (!acronym && upper(c))
                c = static_cast<char>(c - 'A' + 'a');
            label.push_back(c);
        }
        i = j;
    }
    if (!label.empty() && lower(label[0]))
        label[0] = static_cast<char>(label[0] - 'a' + 'A');
    return label;
}

} // namespace pch::puzzles
