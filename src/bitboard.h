/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef BITBOARD_H_INCLUDED
#define BITBOARD_H_INCLUDED

#include <algorithm>
#include <array>
#include <cassert>
#include <string>

#include <xstd/bits/bit/bit_convert.hpp>
#include <xstd/bits/bit_algorithm/bit_for_each.hpp>
#include <xstd/bits/bit_ranges/from_blocks.hpp>

#include "types.h"
#include "misc.h"

namespace Stockfish {

namespace Bitboards {

std::string pretty(Bitboard b);

}  // namespace Stockfish::Bitboards

#ifdef USE_AVX512
// clang-format off
inline const __m512i AllSquares = _mm512_set_epi8(
    63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41,
    40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18,
    17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
// clang-format on
#endif

constexpr Bitboard FileABB(xstd::from_blocks, 0x0101010101010101ULL);
constexpr Bitboard FileBBB = FileABB << 1;
constexpr Bitboard FileCBB = FileABB << 2;
constexpr Bitboard FileDBB = FileABB << 3;
constexpr Bitboard FileEBB = FileABB << 4;
constexpr Bitboard FileFBB = FileABB << 5;
constexpr Bitboard FileGBB = FileABB << 6;
constexpr Bitboard FileHBB = FileABB << 7;

constexpr Bitboard Rank1BB(xstd::from_blocks, 0xFFULL);
constexpr Bitboard Rank2BB = Rank1BB << (8 * 1);
constexpr Bitboard Rank3BB = Rank1BB << (8 * 2);
constexpr Bitboard Rank4BB = Rank1BB << (8 * 3);
constexpr Bitboard Rank5BB = Rank1BB << (8 * 4);
constexpr Bitboard Rank6BB = Rank1BB << (8 * 5);
constexpr Bitboard Rank7BB = Rank1BB << (8 * 6);
constexpr Bitboard Rank8BB = Rank1BB << (8 * 7);

// A Square is a one-square Bitboard on either side of &, |, ^ and -, so only
// the union of two squares needs an operator of its own.

constexpr Bitboard operator|(Square s1, Square s2) { return Bitboard{s1, s2}; }

constexpr bool more_than_one(Bitboard b) { return b.size() > 1; }


// rank_bb() and file_bb() return a bitboard representing all the squares on
// the given file or rank.

constexpr Bitboard rank_bb(Rank r) { return Rank1BB << (8 * r); }

constexpr Bitboard rank_bb(Square s) { return rank_bb(rank_of(s)); }

constexpr Bitboard file_bb(File f) { return FileABB << f; }

constexpr Bitboard file_bb(Square s) { return file_bb(file_of(s)); }

// Moves a bitboard one or two steps as specified by the direction D
inline constexpr Bitboard shift(Bitboard b, Direction dir) {
    return dir == NORTH         ? b << 8
         : dir == SOUTH         ? b >> 8
         : dir == NORTH + NORTH ? b << 16
         : dir == SOUTH + SOUTH ? b >> 16
         : dir == EAST          ? (b - FileHBB) << 1
         : dir == WEST          ? (b - FileABB) >> 1
         : dir == NORTH_EAST    ? (b - FileHBB) << 9
         : dir == NORTH_WEST    ? (b - FileABB) << 7
         : dir == SOUTH_EAST    ? (b - FileHBB) >> 7
         : dir == SOUTH_WEST    ? (b - FileABB) >> 9
                                : Bitboard{};
}


// Returns the squares attacked by pawns of the given color
// from the squares in the given bitboard.
template<Color C>
constexpr Bitboard pawn_attacks_bb(Bitboard b) {
    return C == WHITE ? shift(b, NORTH_WEST) | shift(b, NORTH_EAST)
                      : shift(b, SOUTH_WEST) | shift(b, SOUTH_EAST);
}

constexpr Bitboard pawn_single_push_bb(Color c, Bitboard b) {
    return shift(b, c == WHITE ? NORTH : SOUTH);
}

inline constexpr auto PawnPairBB = []() {
    std::array<Bitboard, SQUARE_NB> result{};
    for (Square s = SQ_A1; s <= SQ_H8; ++s)
    {
        Bitboard file  = file_bb(s);
        Bitboard files = file | shift(file, EAST) | shift(file, WEST);
        result[s]      = files - (Rank1BB | Rank8BB) - s;
    }
    return result;
}();

// Returns the squares that can host a pawn forming a "pawn pair" with a pawn
// on s: own file plus adjacent files, restricted to ranks 2-7, excluding s.
// The geometry is color-independent.
constexpr Bitboard pawn_pair_bb(Square s) { return PawnPairBB[s]; }

constexpr int edge_distance(File f) { return std::min(f, File(FILE_H - f)); }

// The integer bit utilities below serve masks that are not bitboards, such as
// the NNUE sparse input. A Bitboard counts and finds its squares itself.

// Counts the number of non-zero bits in a mask.
inline int popcount(u64 b) {

#ifdef _MSC_VER

    return int(_mm_popcnt_u64(b));

#else  // Assumed gcc or compatible compiler

    return __builtin_popcountll(b);

#endif
}

// Returns the least significant bit in a non-zero mask.
inline Square lsb(u64 b) {
    assert(b);

#if defined(__GNUC__)  // GCC, Clang, ICX

    return Square(__builtin_ctzll(b));

#elif defined(_MSC_VER)
    #ifdef _WIN64  // MSVC, WIN64

    unsigned long idx;
    _BitScanForward64(&idx, b);
    return Square(idx);

    #else  // MSVC, WIN32
    unsigned long idx;

    if (b & 0xffffffff)
    {
        _BitScanForward(&idx, i32(b));
        return Square(idx);
    }
    else
    {
        _BitScanForward(&idx, i32(b >> 32));
        return Square(idx + 32);
    }
    #endif
#else  // Compiler is neither GCC nor MSVC compatible
    #error "Compiler not supported."
#endif
}

// Returns the most significant bit in a non-zero mask.
inline Square msb(u64 b) {
    assert(b);

#if defined(__GNUC__)  // GCC, Clang, ICX

    return Square(63 ^ __builtin_clzll(b));

#elif defined(_MSC_VER)
    #ifdef _WIN64  // MSVC, WIN64

    unsigned long idx;
    _BitScanReverse64(&idx, b);
    return Square(idx);

    #else  // MSVC, WIN32

    unsigned long idx;

    if (b >> 32)
    {
        _BitScanReverse(&idx, i32(b >> 32));
        return Square(idx + 32);
    }
    else
    {
        _BitScanReverse(&idx, i32(b));
        return Square(idx);
    }
    #endif
#else  // Compiler is neither GCC nor MSVC compatible
    #error "Compiler not supported."
#endif
}

// Finds and clears the least significant bit in a non-zero mask.
inline Square pop_lsb(u64& b) {
    assert(b);
    const Square s = lsb(b);
    b &= b - 1;
    return s;
}

// Finds and removes the lowest square of a non-empty bitboard.
inline Square pop_lsb(Bitboard& b) {
    assert(!b.empty());
    const Square s = b.front();
    b.erase(b.begin());
    return s;
}

}  // namespace Stockfish

#endif  // #ifndef BITBOARD_H_INCLUDED
