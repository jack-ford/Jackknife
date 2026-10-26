#pragma once

#include "core/board.hpp"

namespace eval {

inline constexpr std::array<int, 7> PieceValues {0, 0, 100, 320, 330, 500, 900};

int evaluate(const chess::Board& board);
int piece_value(chess::PieceType type);

}
