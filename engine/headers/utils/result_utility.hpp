#pragma once

namespace chess {

class Board;
struct MoveList;

enum class GameResult { Win, Lose, Draw, None };

enum class GameResultReason {
    Checkmate,
    Stalemate,
    InsufficientMaterial,
    FiftyMoveRule,
    ThreefoldRepetition,
    None
};

namespace GameResultUtility {

bool is_repetition(const Board& board, int count = 2);
bool is_half_move_draw(const Board& board);
bool is_insufficient_material(const Board& board);
bool is_mate(const MoveList& legalMoves, bool inCheck);

}

}
