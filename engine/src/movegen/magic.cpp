#include "movegen/magic.hpp"

namespace chess {

bitboard_t Magic::create_movement_mask(square_t index, bool ortho) {
    assert(Square::is_valid(index));

    bitboard_t movementMask = 0;

    const auto& directions = ortho ? RookDirections : BishopDirections;

    int startRank = Square::rank(index);
    int startFile = Square::file(index);

    for (const DirectionOffset& direction : directions) {
        int rank = startRank + direction.rank;
        int file = startFile + direction.file;

        while (Square::is_valid(rank + direction.rank, file + direction.file)) {
            square_t square = Square::index(rank, file);
            Bitboard::set(movementMask, square);

            rank += direction.rank;
            file += direction.file;
        }
    }

    return movementMask;
}

bitboard_t Magic::legalmove_bitboard_from_blockers(square_t start, bitboard_t blockers, bool ortho) {
    assert(Square::is_valid(start));

    bitboard_t attacks = 0;

    const auto& directions = ortho ? RookDirections : BishopDirections;

    int startRank = Square::rank(start);
    int startFile = Square::file(start);

    for (const DirectionOffset& direction : directions) {
        int rank = startRank + direction.rank;
        int file = startFile + direction.file;

        while (Square::is_valid(rank, file)) {
            square_t square = Square::index(rank, file);
            Bitboard::set(attacks, square);

            if (Bitboard::check(blockers, square))
                break;

            rank += direction.rank;
            file += direction.file;
        }
    }

    return attacks;
}

mask_table_t Magic::create_masks(bool rook) {
    mask_table_t masks {};

    for (int square = 0; square < Square::Count; square++) {
        masks[square] = create_movement_mask(square, rook);
    }

    return masks;
}

attack_table_t Magic::create_attack_tables(bool rook, const mask_table_t& masks) {
    attack_table_t tables;

    for (int square = 0; square < Square::Count; ++square) {
        bitboard_t magic = rook ? RookMagics[square] : BishopMagics[square];

        int shift = rook ? RookShifts[square] : BishopShifts[square];

        int relevantBits = 64 - shift;
        std::size_t tableSize = 1ULL << relevantBits;

        tables[square].resize(tableSize);

        bitboard_t blockers = 0;
        do {
            std::size_t index = static_cast<std::size_t>((blockers * magic) >> shift);
            bitboard_t attacks = legalmove_bitboard_from_blockers(square, blockers, rook);
            assert(tables[square][index] == 0 || tables[square][index] == attacks);
            tables[square][index] = attacks;
            blockers = (blockers - masks[square]) & masks[square];
        } while (blockers != 0);
    }

    return tables;
}

const Magic::Tables& Magic::tables() {
    // Initialized on first use, including calls made by other global initializers.
    static const Tables LookupTables = [] {
        Tables result;
        result.rookMasks = create_masks(true);
        result.bishopMasks = create_masks(false);
        result.rookAttacks = create_attack_tables(true, result.rookMasks);
        result.bishopAttacks = create_attack_tables(false, result.bishopMasks);
        return result;
    }();
    return LookupTables;
}

const mask_table_t& Magic::RookMasks = tables().rookMasks;
const mask_table_t& Magic::BishopMasks = tables().bishopMasks;
const attack_table_t& Magic::RookAttacks = tables().rookAttacks;
const attack_table_t& Magic::BishopAttacks = tables().bishopAttacks;

bitboard_t Magic::get_slider_attacks(square_t square, bitboard_t blockers, bool ortho) {
    return ortho ? get_rook_attacks(square, blockers) : get_bishop_attacks(square, blockers);
}

bitboard_t Magic::get_rook_attacks(square_t square, bitboard_t blockers) {
    assert(Square::is_valid(square));
    const auto& data = tables();
    uint64_t key = ((blockers & data.rookMasks[square]) * RookMagics[square]) >> RookShifts[square];
    return data.rookAttacks[square][key];
}

bitboard_t Magic::get_bishop_attacks(square_t square, bitboard_t blockers) {
    assert(Square::is_valid(square));
    const auto& data = tables();
    uint64_t key = ((blockers & data.bishopMasks[square]) * BishopMagics[square]) >> BishopShifts[square];
    return data.bishopAttacks[square][key];
}

}
