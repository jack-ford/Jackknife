#pragma once

#include "search/search.hpp"

#include <algorithm>
#include <stdexcept>

namespace search_tests {

using namespace chess;

inline void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

inline bool is_tactical(const Board& board, move_t move) {
    return Move::type(move) == Move::Type::Promotion || Move::type(move) == Move::Type::EnPassant ||
        (Move::type(move) != Move::Type::Castling && !Piece::is_empty(board.get_square(Move::to(move))));
}

inline int reference_quiescence(Board& board, int ply = 1) {
    MoveGenerator generator;
    MoveList moves = generator.generate_moves(board);
    bool inCheck = board.is_in_check();
    if (moves.is_empty())
        return inCheck ? -search::MateScore + ply : 0;
    if (board.is_game_over().first == GameResult::Draw)
        return 0;
    require(ply < 32, "reference quiescence unexpectedly deep");
    int bestScore = inCheck ? -search::Infinity : eval::evaluate(board);
    for (move_t move : moves) {
        if (!inCheck && !is_tactical(board, move))
            continue;
        board.make_move(move, true);
        int score = -reference_quiescence(board, ply + 1);
        board.unmake_move(move, true);
        bestScore = std::max(bestScore, score);
    }
    return bestScore;
}

inline void verify_pv(const Board& original, const search::Result& result) {
    MoveGenerator generator;
    std::vector<move_t> roots;
    int previousScore = search::Infinity;
    for (const auto& variation : result.variations) {
        require(!variation.moves.empty(), "empty principal variation");
        require(variation.score <= previousScore, "MultiPV scores not sorted");
        previousScore = variation.score;
        require(std::find(roots.begin(), roots.end(), variation.moves[0]) == roots.end(), "duplicate MultiPV root");
        roots.push_back(variation.moves[0]);
        Board board = original;
        for (move_t move : variation.moves) {
            require(board.is_game_over().first == GameResult::None, "PV continues beyond terminal position");
            require(generator.is_legal(board, move), "illegal PV move");
            board.make_move(move, true);
        }
    }
    if (!result.variations.empty()) {
        require(result.bestMove == result.variations[0].moves[0], "best move disagrees with PV");
        require(result.score == result.variations[0].score, "score disagrees with PV");
    }
}

}
