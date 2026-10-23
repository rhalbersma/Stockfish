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

#include "attacks.h"

#include <array>

#include "misc.h"

namespace Stockfish::Attacks {

Bitboard LineBB[SQUARE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
Bitboard inverse_RayPassBB[SQUARE_NB][SQUARE_NB];
Bitboard DiagBB[SQUARE_NB];
Bitboard AntiDiagBB[SQUARE_NB];

namespace {

#ifndef USE_DUAL_HYPERBOLA_QUINT
alignas(64) Magic Magics[SQUARE_NB][2];
#endif

}

[[maybe_unused]] static constexpr Bitboard line_mask(Square sq, Direction d1, Direction d2) {
    Bitboard mask;
    for (Direction d : {d1, d2})
    {
        Square s = sq;
        while (!safe_destination(s, d).empty())
        {
            s += d;
            mask |= s;
        }
    }
    return mask;
}

#ifdef USE_HYPERBOLA_QUINT
static void init_magics(Magic magics[][2]) {
    for (Square s = SQ_A1; s <= SQ_H8; ++s)
    {
        Magic& rook = magics[s][ROOK - BISHOP];
        rook.mask1  = line_mask(s, NORTH, SOUTH);
        rook.mask2  = line_mask(s, EAST, WEST);

        Magic& bishop = magics[s][BISHOP - BISHOP];
        bishop.mask1  = line_mask(s, NORTH_EAST, SOUTH_WEST);
        bishop.mask2  = line_mask(s, NORTH_WEST, SOUTH_EAST);
    }
}

#elif defined(USE_DUAL_HYPERBOLA_QUINT)

// Sliding attacks within a rank, indexed by the slider's file and the
// 6 inner bits of the rank occupancy (edge squares never affect the
// attack set), yielding the 8-bit attack set on that rank
alignas(64) constexpr auto RankAttacks = []() {
    std::array<std::array<u8, 64>, FILE_NB> table{};
    for (int file = 0; file < 8; ++file)
        for (int occ6 = 0; occ6 < 64; ++occ6)
            table[file][occ6] = u8(xstd::bit_convert<u64>(
              sliding_attack(ROOK, Square(file), Bitboard(xstd::from_blocks, u64(occ6) << 1))));
    return table;
}();

static constexpr auto make_dual_magics() {
    std::array<DualMagic, SQUARE_NB> magics{};
    for (Square s = SQ_A1; s <= SQ_H8; ++s)
    {
        DualMagic& m        = magics[s];
        m.maskFile          = line_mask(s, NORTH, SOUTH);
        m.maskDiag          = line_mask(s, NORTH_EAST, SOUTH_WEST);
        m.maskNone          = Bitboard{};
        m.maskAntidiag      = line_mask(s, NORTH_WEST, SOUTH_EAST);
        m.r                 = xstd::bit_convert<u64>(Bitboard{s} << 1);
        m.rr                = xstd::bit_convert<u64>(Bitboard{Square(63 - int(s))} << 1);
        m.rankAttacksLookup = RankAttacks[int(file_of(s))].data();
        m.shift             = 8 * int(rank_of(s));
    }
    return magics;
}

alignas(64) extern constexpr std::array<DualMagic, SQUARE_NB> DualMagics = make_dual_magics();

#else

namespace {
void init_magics(PieceType pt, Bitboard table[], Magic magics[][2]) {

    int seeds[][RANK_NB] = {{8977, 44560, 54343, 38998, 5731, 95205, 104912, 17020},
                            {728, 10316, 55013, 32803, 12281, 15100, 16645, 255}};

    Bitboard occupancy[4096];
    int      epoch[4096] = {}, cnt = 0;
    Bitboard reference[4096] = {};
    int      size            = 0;

    for (Square s = SQ_A1; s <= SQ_H8; ++s)
    {
        Bitboard edges = ((Rank1BB | Rank8BB) - rank_bb(s)) | ((FileABB | FileHBB) - file_bb(s));

        Magic&   m       = magics[s][pt - BISHOP];
        Bitboard attacks = sliding_attack(pt, s, Bitboard{});
        m.mask           = attacks - edges;
        m.shift          = (Is64Bit ? 64 : 32) - int(m.mask.size());
        m.attacks        = s == SQ_A1 ? table : magics[s - 1][pt - BISHOP].attacks + size;
        size             = 0;

        // Carry-Rippler: enumerate every subset of the mask, which needs the
        // integer subtraction to carry across squares.
        u64 mask = xstd::bit_convert<u64>(m.mask);
        u64 b    = 0;
        do
        {
            occupancy[size] = Bitboard(xstd::from_blocks, b);
            reference[size] = sliding_attack(pt, s, occupancy[size]);

            size++;
            b = (b - mask) & mask;
        } while (b);

        PRNG rng(seeds[Is64Bit][rank_of(s)]);

        for (int i = 0; i < size;)
        {
            for (m.magic = 0; popcount((m.magic * mask) >> 56) < 6;)
                m.magic = rng.sparse_rand<u64>();

            for (++cnt, i = 0; i < size; ++i)
            {
                unsigned idx = m.index(occupancy[i]);

                if (epoch[idx] < cnt)
                {
                    epoch[idx]     = cnt;
                    m.attacks[idx] = reference[i];
                }
                else if (m.attacks[idx] != reference[i])
                    break;
            }
        }
    }
}

    #if !defined(USE_DUAL_HYPERBOLA_QUINT) && !defined(USE_HYPERBOLA_QUINT)
static std::array<Bitboard, 0x19000> RookTable;
static std::array<Bitboard, 0x1480>  BishopTable;
    #endif
}

#endif

void init() {

#ifdef USE_HYPERBOLA_QUINT
    init_magics(Magics);
#elif !defined(USE_DUAL_HYPERBOLA_QUINT)
    init_magics(ROOK, RookTable.data(), Magics);
    init_magics(BISHOP, BishopTable.data(), Magics);
#endif

    for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
    {
        for (PieceType pt : {BISHOP, ROOK})
            for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2)
            {
                if (PseudoAttacks[pt][s1].contains(s2))
                {
                    LineBB[s1][s2] =
                      (attacks_bb(pt, s1, Bitboard{}) & attacks_bb(pt, s2, Bitboard{})) | s1 | s2;
                    BetweenBB[s1][s2] =
                      (attacks_bb(pt, s1, Bitboard{s2}) & attacks_bb(pt, s2, Bitboard{s1}));
                    inverse_RayPassBB[s2][s1] =
                      attacks_bb(pt, s1, Bitboard{}) & (attacks_bb(pt, s2, Bitboard{s1}) | s2);
                }
                BetweenBB[s1][s2] |= s2;
            }

        DiagBB[s1]     = line_mask(s1, NORTH_EAST, SOUTH_WEST) | s1;
        AntiDiagBB[s1] = line_mask(s1, NORTH_WEST, SOUTH_EAST) | s1;
    }
}

#ifndef USE_DUAL_HYPERBOLA_QUINT
const Magic& magic(Square s, PieceType pt) {
    assert((pt == BISHOP || pt == ROOK) && is_ok(s));
    return Magics[s][pt - BISHOP];
}
#endif

}  // namespace Stockfish::Attacks
