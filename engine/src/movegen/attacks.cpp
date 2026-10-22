#include "movegen/attacks.hpp"
#include "movegen/magic.hpp"

namespace chess {

constexpr std::size_t Attacks::color_index(PieceColor color) {
    return static_cast<std::size_t>(color);
}

template <std::size_t directionCount>
constexpr bitboard_t Attacks::create_leaper_attacks(square_t square,
    const std::array<DirectionOffset, directionCount>& directions) {
    bitboard_t attacks = 0;

    int startRank = Square::rank(square);
    int startFile = Square::file(square);

    for (const DirectionOffset& direction : directions) {
        int rank = startRank + direction.rank;
        int file = startFile + direction.file;

        if (Square::is_valid(rank, file))
            Bitboard::set(attacks, Square::index(rank, file));
    }

    return attacks;
}

constexpr leaper_attack_table_t Attacks::create_knight_attack_table() {
    leaper_attack_table_t table {};

    for (int square = 0; square < Square::Count; ++square) {
        table[square] = create_leaper_attacks(
            static_cast<square_t>(square),
            knightDirections
        );
    }

    return table;
}

constexpr leaper_attack_table_t Attacks::create_king_attack_table() {
    leaper_attack_table_t table {};

    for (int square = 0; square < Square::Count; ++square) {
        table[square] = create_leaper_attacks(
            static_cast<square_t>(square),
            kingDirections
        );
    }

    return table;
}

constexpr pawn_attack_table_t Attacks::create_pawn_attack_table() {
    pawn_attack_table_t table {};

    for (int square = 0; square < Square::Count; ++square) {
        square_t index = static_cast<square_t>(square);

        table[square][color_index(PieceColor::White)] = create_leaper_attacks(index, whitePawnAttackDirections);

        table[square][color_index(PieceColor::Black)] = create_leaper_attacks(index, blackPawnAttackDirections);
    }

    return table;
}

constexpr distance_table_t Attacks::create_orthogonal_distance_table() {
    distance_table_t table {};

    for (int first = 0; first < Square::Count; ++first) {
        for (int second = 0; second < Square::Count; ++second) {
            int rankDistance = Square::abs(Square::rank(first) - Square::rank(second));

            int fileDistance = Square::abs(Square::file(first) - Square::file(second));

            table[first][second] = static_cast<square_t>(rankDistance + fileDistance);
        }
    }

    return table;
}

constexpr distance_table_t Attacks::create_king_distance_table() {
    distance_table_t table {};

    for (int first = 0; first < Square::Count; ++first) {
        for (int second = 0; second < Square::Count; ++second) {
            table[first][second] = static_cast<square_t>(Square::distance(
                static_cast<square_t>(first),
                static_cast<square_t>(second)
            ));
        }
    }

    return table;
}

const leaper_attack_table_t Attacks::knightAttacks = create_knight_attack_table();
const leaper_attack_table_t Attacks::kingAttacks = create_king_attack_table();
const pawn_attack_table_t Attacks::pawnAttacks = create_pawn_attack_table();

const distance_table_t Attacks::orthogonalDistance = create_orthogonal_distance_table();
const distance_table_t Attacks::kingDistance = create_king_distance_table();

bitboard_t Attacks::get_pawn_attacks(square_t square, PieceColor color) {
    assert(Square::is_valid(square));
    return pawnAttacks[square][color_index(color)];
}

bitboard_t Attacks::get_left_pawn_attacks(bitboard_t pawns, PieceColor color) {
    return color == PieceColor::White ? Bitboard::shift<Direction::NorthWest>(pawns)
        : Bitboard::shift<Direction::SouthEast>(pawns);
}

bitboard_t Attacks::get_right_pawn_attacks(bitboard_t pawns, PieceColor color) {
    return color == PieceColor::White ? Bitboard::shift<Direction::NorthEast>(pawns)
        : Bitboard::shift<Direction::SouthWest>(pawns);
}

bitboard_t Attacks::get_pawn_attacks(bitboard_t pawns, PieceColor color) {
    return get_left_pawn_attacks(pawns, color) | get_right_pawn_attacks(pawns, color);
}

bitboard_t Attacks::get_knight_attacks(square_t square) {
    assert(Square::is_valid(square));
    return knightAttacks[square];
}

bitboard_t Attacks::get_bishop_attacks(square_t square, bitboard_t occupied) {
    assert(Square::is_valid(square));
    return Magic::get_bishop_attacks(square, occupied);
}

bitboard_t Attacks::get_rook_attacks(square_t square, bitboard_t occupied) {
    assert(Square::is_valid(square));
    return Magic::get_rook_attacks(square, occupied);
}

bitboard_t Attacks::get_queen_attacks(square_t square, bitboard_t occupied) {
    assert(Square::is_valid(square));
    return get_bishop_attacks(square, occupied) | get_rook_attacks(square, occupied);
}

bitboard_t Attacks::get_king_attacks(square_t square) {
    assert(Square::is_valid(square));
    return kingAttacks[square];
}

}
