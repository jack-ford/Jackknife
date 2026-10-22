#pragma once

#include <array>
#include <cassert>
#include <cstddef>

#include "core/move.hpp"

namespace chess {

constexpr int MaxMoves = 256;

struct MoveList {
    std::array<move_t, MaxMoves> moves;
    int count = 0;

    constexpr void add(move_t move) {
        assert(count < MaxMoves);
        moves[count++] = move;
    }

    constexpr void clear() {
        count = 0;
    }

    constexpr bool is_empty() const {
        return count == 0;
    }

    constexpr int size() const {
        return count;
    }

    constexpr move_t& operator[](int index) {
        assert(index >= 0 && index < count);
        return moves[index];
    }

    constexpr const move_t& operator[](int index) const {
        assert(index >= 0 && index < count);
        return moves[index];
    }

    constexpr move_t* begin() {
        return moves.data();
    }

    constexpr const move_t* begin() const {
        return moves.data();
    }

    constexpr move_t* end() {
        return moves.data() + count;
    }

    constexpr const move_t* end() const {
        return moves.data() + count;
    }
};

}
