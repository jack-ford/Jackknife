#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "movegen/movegen.hpp"

namespace chess {

inline std::string coordinate_move(move_t move) {
    std::string result = Square::to_string(Move::from(move)) + Square::to_string(Move::to(move));
    if (Move::type(move) == Move::Type::Promotion) {
        constexpr char Pieces[] = "   nbrq";
        result += Pieces[static_cast<int>(Move::promotion_piece(move))];
    }
    return result;
}

inline std::optional<move_t> parse_coordinate_move(const Board& board, std::string_view text) {
    MoveGenerator generator;
    for (move_t move : generator.generate_moves(board))
        if (coordinate_move(move) == text)
            return move;
    return std::nullopt;
}

}
