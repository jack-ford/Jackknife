#include "utils/result_utility.hpp"

#include <algorithm>
#include "core/board.hpp"
#include "movegen/movelist.hpp"

namespace chess::GameResultUtility {

bool isRepetition(const Board& board, int count) {
    auto history = board.repetition_position_history();
    auto occurrences = std::count(history.begin(), history.end(), board.zobrist_key());
    return occurrences >= count;
}

bool isHalfMoveDraw(const Board& board) {
    return board.fifty_move_counter() >= 100;
}

bool isInsufficientMaterial(const Board& board) {
    const int count = Bitboard::count(board.all_pieces_bitboard());

    if (count == 2) return true;

    const auto& pieces = board.piece_bitboards();

    const bitboard_t bishops =
        pieces[Piece::create(PieceType::Bishop, PieceColor::White)] |
        pieces[Piece::create(PieceType::Bishop, PieceColor::Black)];

    const bitboard_t knights =
        pieces[Piece::create(PieceType::Knight, PieceColor::White)] |
        pieces[Piece::create(PieceType::Knight, PieceColor::Black)];

    // Two kings and one bishop or knight
    if (count == 3)
        return (bishops | knights) != 0;

    // Every non-king piece is a bishop
    if (bishops != 0 && Bitboard::count(bishops) == count - 2) {
        bitboard_t remaining = bishops;
        const square_t first = Bitboard::pop(remaining);

        while (remaining != 0) {
            if (!Square::same_color(first, Bitboard::pop(remaining)))
                return false;
        }

        return true;
    }

    return false;
}

bool isMate(const MoveList& legalMoves, bool inCheck) {
    return legalMoves.is_empty() && inCheck;
}

}
