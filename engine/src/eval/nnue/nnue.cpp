#include "eval/nnue/nnue.hpp"

#include "core/board.hpp"
#include "eval/nnue/kernels.hpp"

namespace eval::nnue {

using namespace chess;

namespace {

void refresh_perspective(Accumulator& accumulator, const Board& board, PieceColor perspective) {
    const auto& weights = network();

    int color = static_cast<int>(perspective);
    auto& values = accumulator.values[color];
    std::copy_n(weights.inputBiases, AccumulatorSize, values.begin());

    accumulator.linear[color] = 0;
    square_t king = board.king_square(perspective);
    bitboard_t occupied = board.all_pieces_bitboard();

    while (occupied) {
        square_t square = Bitboard::pop(occupied);
        piece_t piece = board.get_square(square);

        if (Piece::type(piece) == PieceType::King) continue;

        int feature = feature_index(piece, square, king, perspective);
        kernels::update(values.data(), weights.inputWeights[feature], nullptr, nullptr);
        accumulator.linear[color] += weights.linearWeights[feature];
    }
}

void activate(const Accumulator& accumulator, PieceColor sideToMove, int16_t* output) {
    int us = static_cast<int>(sideToMove);
    kernels::activate(accumulator.values[us].data(), output);
    kernels::activate(accumulator.values[1 - us].data(), output + AccumulatorSize);
}

int evaluate_activated(const Accumulator& accumulator, PieceColor sideToMove, const int16_t* activated) {
    const auto& weights = network();
    alignas(32) int32_t sums[HiddenSize];
    alignas(32) int16_t hidden[HiddenSize];

    kernels::hidden_layer(activated, &weights.hiddenWeights[0][0], sums);
    for (int i = 0; i < HiddenSize; ++i)
        hidden[i] = kernels::screlu(std::max(0, sums[i] + weights.hiddenBiases[i]) / WeightScale);

    int us = static_cast<int>(sideToMove);

    int32_t linear = accumulator.linear[us] - accumulator.linear[1 - us];
    int64_t output = weights.outputBias + kernels::dot_product(hidden, weights.outputWeights, HiddenSize);

    output += static_cast<int64_t>(linear) * WeightScale;
    return static_cast<int>(output * 100 / BiasScale);
}

}

Changes describe_move(const Board& board, move_t move) {
    square_t from = Move::from(move);
    square_t to = Move::to(move);
    piece_t piece = board.get_square(from);
    Changes changes {{ board.king_square(PieceColor::Black), board.king_square(PieceColor::White) }};

    if (Move::type(move) == Move::Type::Castling) {
        // The engine encodes castling as king-from -> rook-from.
        piece_t rook = Piece::create(PieceType::Rook, board.move_color());
        changes.removed = { rook, to };
        changes.added = { rook, Square::castling_rook_square(to > from, board.move_color()) };
        return changes;
    }

    if (Piece::type(piece) != PieceType::King) {
        changes.removed = { piece, from };
        changes.added = { Move::type(move) == Move::Type::Promotion ?
            Piece::create(Move::promotion_piece(move), board.move_color()) : piece, to };
    }

    square_t capturedSquare = Move::type(move) == Move::Type::EnPassant ? Square::ep_square(to) : to;
    changes.captured = { board.get_square(capturedSquare), capturedSquare };

    return changes;
}

void update(Accumulator& accumulator, const Board& board, const Changes& changes) {
    const auto& weights = network();

    for (PieceColor perspective : { PieceColor::Black, PieceColor::White }) {
        int color = static_cast<int>(perspective);
        square_t king = board.king_square(perspective);

        if (king_bucket(king, perspective) != king_bucket(changes.previousKings[color], perspective)) {
            refresh_perspective(accumulator, board, perspective);
            continue;
        }

        auto index = [&](const PieceSquare& entry) {
            return entry.piece ? feature_index(entry.piece, entry.square, king, perspective) : -1;
        };

        int added = index(changes.added);
        int removed = index(changes.removed);
        int captured = index(changes.captured);

        kernels::update(accumulator.values[color].data(),
            added >= 0 ? weights.inputWeights[added] : nullptr,
            removed >= 0 ? weights.inputWeights[removed] : nullptr,
            captured >= 0 ? weights.inputWeights[captured] : nullptr);
            
        if (added >= 0) accumulator.linear[color] += weights.linearWeights[added];
        if (removed >= 0) accumulator.linear[color] -= weights.linearWeights[removed];
        if (captured >= 0) accumulator.linear[color] -= weights.linearWeights[captured];
    }
}

void refresh(Accumulator& accumulator, const Board& board) {
    refresh_perspective(accumulator, board, PieceColor::White);
    refresh_perspective(accumulator, board, PieceColor::Black);
}

int evaluate(const Accumulator& accumulator, PieceColor sideToMove) {
    alignas(32) int16_t activated[2 * AccumulatorSize];
    activate(accumulator, sideToMove, activated);
    return evaluate_activated(accumulator, sideToMove, activated);
}

Output evaluate_outputs(const Accumulator& accumulator, PieceColor sideToMove) {
    alignas(32) int16_t activated[2 * AccumulatorSize];
    activate(accumulator, sideToMove, activated);
    const auto& weights = network();
    int64_t residual = weights.drawBias + kernels::dot_product(activated, weights.drawWeights, 2 * AccumulatorSize);
    return { evaluate_activated(accumulator, sideToMove, activated), static_cast<int>(residual * 100 / BiasScale) };
}

}
