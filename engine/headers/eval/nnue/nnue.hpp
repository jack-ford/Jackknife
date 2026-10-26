#pragma once

#include "core/move.hpp"
#include "eval/nnue/architecture.hpp"

namespace chess { class Board; }

namespace eval::nnue {

struct PieceSquare {
    chess::piece_t piece = 0;
    chess::square_t square = chess::Square::None;
};

struct Changes {
    std::array<chess::square_t, 2> previousKings;
    PieceSquare removed;
    PieceSquare captured;
    PieceSquare added;
};

// Describe before making the move; update after the board is in its new state.
Changes describe_move(const chess::Board& board, chess::move_t move);
void update(Accumulator& accumulator, const chess::Board& board, const Changes& changes);
void refresh(Accumulator& accumulator, const chess::Board& board);
int evaluate(const Accumulator& accumulator, chess::PieceColor sideToMove);
Output evaluate_outputs(const Accumulator& accumulator, chess::PieceColor sideToMove);

}
