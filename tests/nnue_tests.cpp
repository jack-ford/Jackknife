#include "core/board.hpp"
#include "eval/evaluation.hpp"
#include "eval/nnue/nnue.hpp"
#include "eval/nnue/kernels.hpp"
#include "movegen/movegen.hpp"

#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

#ifdef CHESS_NNUE_REFERENCE
void verify_casanchess(const chess::Board& board);
#endif

namespace {

using namespace chess;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void verify(const Board& board) {
    eval::nnue::Accumulator fresh;
    eval::nnue::refresh(fresh, board);
    require(fresh == board.nnue_accumulator(), "incremental accumulator differs from full refresh");
    auto output = eval::nnue::evaluate_outputs(fresh, board.move_color());
    require(output.eval == eval::evaluate(board), "evaluation entry points disagree");
#ifdef CHESS_NNUE_REFERENCE
    verify_casanchess(board);
#endif
}

void test_kernels() {
    using namespace eval::nnue;
    alignas(32) int16_t input[AccumulatorSize], output[AccumulatorSize];
    // Every int16 input, including the 256 endpoint that overflows a 16-bit square.
    for (int start = -32768; start <= 32767; start += AccumulatorSize) {
        for (int i = 0; i < AccumulatorSize; ++i) input[i] = static_cast<int16_t>(start + i);
        kernels::activate(input, output);
        for (int i = 0; i < AccumulatorSize; ++i)
            require(output[i] == kernels::screlu(input[i]), "SIMD activation differs from scalar");
    }
    std::mt19937 random(20260910);
    alignas(32) int16_t values[256], weights[256];
    for (int trial = 0; trial < 100; ++trial) {
        for (int i = 0; i < 256; ++i) {
            values[i] = static_cast<int16_t>(random() % 257);
            weights[i] = static_cast<int16_t>(static_cast<int>(random() % 65536) - 32768);
        }
        for (int size : { 1, 15, 16, 17, 48, 128, 256 }) {
            int64_t expected = 0;
            for (int i = 0; i < size; ++i) expected += values[i] * weights[i];
            require(kernels::dot_product(values, weights, size) == expected, "SIMD dot product differs from scalar");
        }
    }
    constexpr int InputSize = 2 * AccumulatorSize;
    alignas(32) int16_t activations[InputSize];
    std::vector<int16_t> rows(HiddenSize * InputSize);
    alignas(32) int32_t sums[HiddenSize];
    for (int trial = 0; trial < 20; ++trial) {
        for (int16_t& value : activations) value = static_cast<int16_t>(random() % (ActivationScale + 1));
        for (int16_t& weight : rows) weight = static_cast<int16_t>(static_cast<int>(random() % 65536) - 32768);
        kernels::hidden_layer(activations, rows.data(), sums);
        for (int row = 0; row < HiddenSize; ++row) {
            int64_t expected = 0;
            for (int i = 0; i < InputSize; ++i) expected += activations[i] * rows[row * InputSize + i];
            require(sums[row] == expected, "SIMD hidden layer differs from scalar");
        }
    }
}

void test_moves() {
    MoveGenerator generator;
    int promotions = 0, castles = 0, enPassant = 0;
    for (const char* fen : {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1",
        "1r5k/P7/8/8/8/8/8/7K w - - 0 1",
        "7k/8/8/8/8/8/p7/1R5K b - - 0 1",
        "7k/8/8/3pP3/8/8/8/7K w - d6 0 1",
        "7k/8/8/8/3Pp3/8/8/7K b - d3 0 1",
        "7k/8/8/8/8/2p5/1K6/R7 w - - 0 1",
        "7k/8/8/8/8/3p4/2K5/R7 w - - 0 1",
        "7k/8/8/8/8/8/R7/K7 w - - 0 100000"
    }) {
        Board board(fen);
        verify(board);
        auto original = board.nnue_accumulator();
        for (move_t move : generator.generate_moves(board)) {
            promotions += Move::type(move) == Move::Type::Promotion;
            castles += Move::type(move) == Move::Type::Castling;
            enPassant += Move::type(move) == Move::Type::EnPassant;
            board.make_move(move, true);
            verify(board);
            // Copy isolation and null moves, including a real move below the null.
            Board copy = board;
            if (!copy.is_in_check()) {
                copy.make_null_move();
                verify(copy);
                auto replies = generator.generate_moves(copy);
                if (!replies.is_empty()) {
                    copy.make_move(replies[0], true);
                    verify(copy);
                    copy.unmake_move();
                }
                copy.unmake_null_move();
                verify(copy);
                require(copy.nnue_accumulator() == board.nnue_accumulator(), "copy/null undo changed accumulator");
            }
            board.unmake_move(move, true);
            require(board.nnue_accumulator() == original, "unmake did not restore accumulator");
        }
    }
    require(promotions >= 16 && castles >= 4 && enPassant >= 2, "special-move fixtures were not exercised");
}

void test_playouts() {
    std::mt19937 random(144);
    MoveGenerator generator;
    for (int game = 0; game < 12; ++game) {
        Board board;
        auto initial = board.nnue_accumulator();
        int movesMade = 0;
        for (; movesMade < 300; ++movesMade) {
            auto moves = generator.generate_moves(board);
            if (moves.is_empty()) break;
            board.make_move(moves[random() % moves.size()], true);
            verify(board);
            if (movesMade == 150) {
                Board copy = board;
                require(copy.nnue_accumulator() == board.nnue_accumulator(), "board copy lost accumulator");
                copy.load_start_position();
                verify(copy);
                verify(board);
            }
        }
        while (movesMade-- > 0) {
            board.unmake_move();
            verify(board);
        }
        require(board.nnue_accumulator() == initial, "playout undo failed");
        auto before = board.nnue_accumulator();
        try { board.load_position("invalid fen"); } catch (const std::exception&) {}
        require(board.nnue_accumulator() == before, "failed FEN load changed accumulator");
    }
}

}

int main() {
    try {
        test_kernels();
        test_moves();
        test_playouts();
        std::cout << "NNUE tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
