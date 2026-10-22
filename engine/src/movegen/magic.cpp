#include "movegen/magic.hpp"

namespace chess {

std::vector<bitboard_t> Magic::create_all_bitboard_blockers(bitboard_t movementMask) {
    std::size_t patternCount = std::size_t { 1 } << Bitboard::count(movementMask);

    std::vector<bitboard_t> blockerBitboards;
    blockerBitboards.reserve(patternCount);

    bitboard_t blockers = 0;

    do {
        blockerBitboards.push_back(blockers);
        blockers = (blockers - movementMask) & movementMask;
    }
    while (blockers != 0);

    return blockerBitboards;
}

bitboard_t Magic::create_movement_mask(square_t index, bool ortho) {
    assert(Square::is_valid(index));

    bitboard_t movementMask = 0;

    const auto& directions = ortho ? rookDirections : bishopDirections;

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

    const auto& directions = ortho ? rookDirections : bishopDirections;

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
        bitboard_t magic = rook ? rookMagics[square] : bishopMagics[square];

        int shift = rook ? rookShifts[square] : bishopShifts[square];

        int relevantBits = 64 - shift;
        std::size_t tableSize = 1ULL << relevantBits;

        tables[square].resize(tableSize);

        auto blockerPatterns = create_all_bitboard_blockers(masks[square]);

        for (bitboard_t blockers : blockerPatterns) {
            std::size_t index = static_cast<std::size_t>((blockers * magic) >> shift);
            tables[square][index] = legalmove_bitboard_from_blockers(square, blockers, rook);
        }
    }

    return tables;
}

const mask_table_t Magic::rookMasks = create_masks(true);
const mask_table_t Magic::bishopMasks = create_masks(false);
const attack_table_t Magic::rookAttacks = create_attack_tables(true, rookMasks);
const attack_table_t Magic::bishopAttacks = create_attack_tables(false, bishopMasks);

bitboard_t Magic::get_slider_attacks(square_t square, bitboard_t blockers, bool ortho) {
    return ortho ? get_rook_attacks(square, blockers) : get_bishop_attacks(square, blockers);
}

bitboard_t Magic::get_rook_attacks(square_t square, bitboard_t blockers) {
    uint64_t key = ((blockers & rookMasks[square]) * rookMagics[square]) >> rookShifts[square];
    return rookAttacks[square][key];
}

bitboard_t Magic::get_bishop_attacks(square_t square, bitboard_t blockers) {
    uint64_t key = ((blockers & bishopMasks[square]) * bishopMagics[square]) >> bishopShifts[square];
    return bishopAttacks[square][key];
}

}
