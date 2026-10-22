#pragma once

#include <array>
#include <cassert>

#include "core/bitboard.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "core/directions.hpp"

namespace chess {

using leaper_attack_table_t = std::array<bitboard_t, Square::Count>;
using pawn_attack_table_t = std::array<std::array<bitboard_t, 2>, Square::Count>;
using distance_table_t = std::array<std::array<square_t, Square::Count>, Square::Count>;

class Attacks {
    public:
        static const leaper_attack_table_t KnightAttacks;
        static const leaper_attack_table_t KingAttacks;
        static const pawn_attack_table_t PawnAttacks;

        static const distance_table_t OrthogonalDistance;
        static const distance_table_t KingDistance;

        static bitboard_t get_pawn_attacks(square_t square, PieceColor color);
        static bitboard_t get_left_pawn_attacks(bitboard_t pawns, PieceColor color);
        static bitboard_t get_right_pawn_attacks(bitboard_t pawns, PieceColor color);
        static bitboard_t get_pawn_attacks(bitboard_t pawns, PieceColor color);
        static bitboard_t get_knight_attacks(square_t square);
        static bitboard_t get_bishop_attacks(square_t square, bitboard_t occupied);
        static bitboard_t get_rook_attacks(square_t square, bitboard_t occupied);
        static bitboard_t get_queen_attacks(square_t square, bitboard_t occupied);
        static bitboard_t get_king_attacks(square_t square);

    private:
        Attacks() = delete;

        static constexpr std::size_t color_index(PieceColor color);

        template <std::size_t directionCount>
        static constexpr bitboard_t create_leaper_attacks(
            square_t square,
            const std::array<DirectionOffset, directionCount>& directions
        );

        static constexpr leaper_attack_table_t create_knight_attack_table();
        static constexpr leaper_attack_table_t create_king_attack_table();
        static constexpr pawn_attack_table_t create_pawn_attack_table();
        static constexpr distance_table_t create_orthogonal_distance_table();
        static constexpr distance_table_t create_king_distance_table();
};

}
