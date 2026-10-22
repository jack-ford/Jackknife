#pragma once

#include "square.hpp"
#include "piece.hpp"

namespace chess {

using move_t = uint16_t;

namespace Move {

    enum class Type : uint8_t {
        Normal = 0,
        Promotion = 1,
        EnPassant = 2,
        Castling = 3
    };

    template <Type type = Type::Normal>
    constexpr move_t create(square_t from, square_t to, PieceType promotion = PieceType::Knight) {
        move_t promotionValue = static_cast<move_t>(promotion) - static_cast<move_t>(PieceType::Knight);
        return (static_cast<move_t>(type) << 14) | (promotionValue << 12)
            | (static_cast<move_t>(from) << 6) | static_cast<move_t>(to);

    }

    constexpr square_t from(move_t move) {
        return static_cast<square_t>((move >> 6) & 0x3F);
    }

    constexpr square_t to(move_t move) {
        return static_cast<square_t>(move & 0x3F);
    }

    constexpr Type type(move_t move) {
        return static_cast<Type>((move >> 14) & 3);
    }

    constexpr PieceType promotion_piece(move_t move) {
        assert(type(move) == Type::Promotion);

        return static_cast<PieceType>(((move >> 12) & 0x3) + static_cast<uint8_t>(PieceType::Knight));
    }

}

}
