#pragma once

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include "bitboard.hpp"
#include "directions.hpp"
#include "piece.hpp"

namespace chess {

using square_t = uint8_t;

namespace Square {

    constexpr square_t None = 64;
    constexpr int Count = 64;

    enum : square_t {
        A1, B1, C1, D1, E1, F1, G1, H1,
        A2, B2, C2, D2, E2, F2, G2, H2,
        A3, B3, C3, D3, E3, F3, G3, H3,
        A4, B4, C4, D4, E4, F4, G4, H4,
        A5, B5, C5, D5, E5, F5, G5, H5,
        A6, B6, C6, D6, E6, F6, G6, H6,
        A7, B7, C7, D7, E7, F7, G7, H7,
        A8, B8, C8, D8, E8, F8, G8, H8
    };

    constexpr square_t index(int rank, int file) {
        assert(rank >= 0 && rank < 8);
        assert(file >= 0 && file < 8);

        return static_cast<square_t>(rank * 8 + file);
    }

    constexpr int rank(square_t sq) {
        return sq >> 3;
    }

    constexpr int file(square_t sq) {
        return sq & 7;
    }

    constexpr bool is_valid(int sq) {
        return sq >= 0 && sq < 64;
    }

    constexpr bool is_valid(int rank, int file) {
        return rank >= 0 && rank < 8 &&
               file >= 0 && file < 8;
    }

    constexpr bool is_valid_string(std::string_view str) {
        return str.size() == 2 &&
               str[0] >= 'a' && str[0] <= 'h' &&
               str[1] >= '1' && str[1] <= '8';
    }

    constexpr square_t from_string(std::string_view str) {
        if (!is_valid_string(str))
            throw std::invalid_argument("Invalid square");

        int f = str[0] - 'a';
        int r = str[1] - '1';

        return index(r, f);
    }

    inline std::string to_string(square_t sq) {
        if (!is_valid(sq))
            throw std::invalid_argument("Invalid square");

        return {
            static_cast<char>('a' + file(sq)),
            static_cast<char>('1' + rank(sq))
        };
    }

    constexpr bool is_light(square_t sq) {
        return (rank(sq) + file(sq)) & 1;
    }

    constexpr bool is_dark(square_t sq) {
        return !is_light(sq);
    }

    constexpr bool same_color(square_t a, square_t b) {
        return is_light(a) == is_light(b);
    }

    constexpr int abs(int x)  {
        return x < 0 ? -x : x;
    }

    constexpr int distance(square_t a, square_t b) {
        int dr = abs(rank(a) - rank(b));
        int df = abs(file(a) - file(b));

        return dr > df ? dr : df;
    }

    constexpr int value_distance(square_t a, square_t b) {
        return abs(static_cast<int>(a) - static_cast<int>(b));
    }

    constexpr bool back_rank(square_t sq, PieceColor color) {
        return rank(sq) == (color == PieceColor::White ? 0 : 7);
    }

    constexpr int relative_rank(int r, PieceColor color) {
        return color == PieceColor::White ? r : 7 - r;
    }

    constexpr int diagonal_of(square_t sq) {
        return 7 + rank(sq) - file(sq);
    }

    constexpr int antidiagonal_of(square_t sq) {
        return rank(sq) + file(sq);
    }

    constexpr square_t flip(square_t sq) {
        return sq ^ 56;
    }

    constexpr square_t relative_square(square_t sq, PieceColor color) {
        return color == PieceColor::White ? sq : flip(sq);
    }

    constexpr square_t ep_square(square_t sq) {
        assert(rank(sq) >= 2 && rank(sq) <= 5);
        return sq ^ 8;
    }

    constexpr square_t castling_king_square(bool kingSide, PieceColor color) {
        return relative_square(kingSide ? G1 : C1, color);
    }

    constexpr square_t castling_rook_square(bool kingSide, PieceColor color) {
        return relative_square(kingSide ? F1 : D1, color);
    }

    constexpr Direction relative_direction(Direction dir, PieceColor color) {
        return color == PieceColor::White ? dir : static_cast<Direction>(-static_cast<int>(dir));
    }

    constexpr square_t step(square_t sq, Direction dir) {
        if (!is_valid(sq))
            return None;

        int dr = 0;
        int df = 0;

        switch (dir) {
            case Direction::North:     dr =  1; break;
            case Direction::South:     dr = -1; break;
            case Direction::East:      df =  1; break;
            case Direction::West:      df = -1; break;
            case Direction::NorthEast: dr =  1; df =  1; break;
            case Direction::NorthWest: dr =  1; df = -1; break;
            case Direction::SouthEast: dr = -1; df =  1; break;
            case Direction::SouthWest: dr = -1; df = -1; break;
            default: return None;
        }

        int r = rank(sq) + dr;
        int f = file(sq) + df;

        return is_valid(r, f) ? index(r, f) : None;
    }

    constexpr bitboard_t bitboard(square_t sq) {
        return is_valid(sq) ? (1ULL << sq) : 0ULL;
    }

    constexpr bitboard_t rank_mask(int r) {
        return r >= 0 && r < 8 ? (0xFFULL << (r * 8)): 0ULL;
    }

    constexpr bitboard_t file_mask(int f) {
        return f >= 0 && f < 8 ? (0x0101010101010101ULL << f) : 0ULL;
    }

}

}
