#pragma once

#include <algorithm>
#include <bit>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <string>

#include "directions.hpp"

namespace chess {

using bitboard_t = uint64_t;

namespace Bitboard {

    template <Direction direction>
    constexpr bitboard_t shift(bitboard_t bitboard) {
        constexpr bitboard_t FileAMask = 0x0101010101010101ULL;
        constexpr bitboard_t FileHMask = 0x8080808080808080ULL;

        if constexpr (direction == Direction::North)
            return bitboard << 8;
        else if constexpr (direction == Direction::South)
            return bitboard >> 8;
        else if constexpr (direction == Direction::NorthWest)
            return (bitboard & ~FileAMask) << 7;
        else if constexpr (direction == Direction::West)
            return (bitboard & ~FileAMask) >> 1;
        else if constexpr (direction == Direction::SouthWest)
            return (bitboard & ~FileAMask) >> 9;
        else if constexpr (direction == Direction::NorthEast)
            return (bitboard & ~FileHMask) << 9;
        else if constexpr (direction == Direction::East)
            return (bitboard & ~FileHMask) << 1;
        else if constexpr (direction == Direction::SouthEast)
            return (bitboard & ~FileHMask) >> 7;
        else
            return 0;
    }

    constexpr bitboard_t create(uint64_t bits = 0) {
        return bits;
    }

    constexpr bitboard_t from_square(int index) {
        assert(index >= 0 && index < 64);
        return 1ULL << index;
    }

    constexpr bitboard_t& set(bitboard_t& bitboard, int index) {
        assert(index >= 0 && index < 64);
        bitboard |= 1ULL << index;
        return bitboard;
    }

    constexpr bool check(bitboard_t bitboard, int index) {
        assert(index >= 0 && index < 64);
        return bitboard & (1ULL << index);
    }

    constexpr bitboard_t& clear(bitboard_t& bitboard, int index) {
        assert(index >= 0 && index < 64);
        bitboard &= ~(1ULL << index);
        return bitboard;
    }

    constexpr bitboard_t& clear(bitboard_t& bitboard) {
        bitboard = 0;
        return bitboard;
    }

    constexpr bool is_empty(bitboard_t bitboard) {
        return bitboard == 0;
    }

    constexpr int lsb(bitboard_t bitboard) {
        assert(bitboard != 0);
        return std::countr_zero(bitboard);
    }

    constexpr int msb(bitboard_t bitboard) {
        assert(bitboard != 0);
        return 63 - std::countl_zero(bitboard);
    }

    constexpr int count(bitboard_t bitboard) {
        return std::popcount(bitboard);
    }

    constexpr uint8_t pop(bitboard_t& bitboard) {
        assert(bitboard != 0);

        uint8_t index = static_cast<uint8_t>(lsb(bitboard));
        bitboard &= bitboard - 1;
        return index;
    }

    constexpr uint64_t bits(bitboard_t bitboard) {
        return bitboard;
    }

    inline std::string to_string(bitboard_t bitboard) {
        std::string result;
        std::string binary = std::bitset<64>(bitboard).to_string();

        for (int i = 0; i < 64; i += 8) {
            std::string rank = binary.substr(i, 8);
            std::reverse(rank.begin(), rank.end());
            result += rank + '\n';
        }

        return result;
    }

}

}
