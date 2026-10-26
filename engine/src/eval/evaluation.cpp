#include "eval/evaluation.hpp"
#include "eval/nnue/nnue.hpp"

namespace eval {

using namespace chess;

int piece_value(PieceType type) {
    return PieceValues[static_cast<int>(type)];
}

int evaluate(const Board& board) {
    return nnue::evaluate(board.nnue_accumulator(), board.move_colour());
}

}
