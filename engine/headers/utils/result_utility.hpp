#pragma once

namespace chess {

class Board;
struct MoveList;

enum class GameResult { Win, Lose, Draw, None };

enum class GameResultReason {
    Checkmate,
    Stalemate,
    Insufficient_Material,
    Fifty_Move_Rule,
    Threefold_Repetition,
    None
};

namespace GameResultUtility {

bool isRepetition(const Board& board, int count = 2);
bool isHalfMoveDraw(const Board& board);
bool isInsufficientMaterial(const Board& board);
bool isMate(const MoveList& legalMoves, bool inCheck);

}

}
