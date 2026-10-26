#include "search_test_utils.hpp"

#include <algorithm>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace chess;

namespace search_tests {

void test_search() {
    auto engine = std::make_unique<search::Search>(1);
    search::Limits limits;
    limits.time = std::chrono::milliseconds(0);
    limits.depth = 4;
    limits.multiPv = 3;
    Board board;
    std::string originalFen = board.current_fen();
    uint64_t originalKey = board.zobrist_key();
    int iterations = 0;
    auto result = engine->iterative_deepening(board, limits, {}, [&](const search::Result& progress) {
        require(progress.depth == ++iterations, "iteration depths skipped");
        require(progress.variations.size() == 3, "partial MultiPV iteration published");
        verify_pv(board, progress);
    });
    require(result.depth == 4 && !result.stopped, "fixed-depth search incomplete");
    require(result.variations.size() == 3, "MultiPV count incorrect");
    require(board.current_fen() == originalFen && board.zobrist_key() == originalKey &&
        board.all_game_moves().empty(), "search changed caller's board");
    verify_pv(board, result);

    limits.multiPv = 1;
    limits.depth = 3;
    Board mate("7k/8/5KQ1/8/8/8/8/8 w - - 0 1");
    result = engine->iterative_deepening(mate, limits);
    require(result.score == search::MateScore - 1, "mate in one score incorrect");
    mate.make_move(result.bestMove);
    require(mate.is_game_over().second == GameResultReason::Checkmate, "missed mate in one");
    result = engine->iterative_deepening(Board("7k/6Q1/5K2/8/8/8/8/8 b - - 100 1"), limits);
    require(result.bestMove == 0 && result.score == -search::MateScore, "mate must precede fifty-move draw");
    result = engine->iterative_deepening(Board("7k/5K2/6Q1/8/8/8/8/8 b - - 0 1"), limits);
    require(result.bestMove == 0 && result.score == 0, "stalemate scored incorrectly");
    for (const char* fen : {
        "7k/8/8/8/8/8/8/K7 w - - 0 1",
        "7k/8/8/8/8/8/R7/K7 w - - 100 1"
    }) {
        result = engine->iterative_deepening(Board(fen), limits);
        require(result.bestMove == 0 && result.score == 0, "draw root returned move");
    }

    Board repetition;
    for (int cycle = 0; cycle < 2; ++cycle) {
        for (auto [from, to] : {std::pair{6, 21}, {62, 45}, {21, 6}, {45, 62}})
            repetition.make_move(Move::create(static_cast<square_t>(from), static_cast<square_t>(to)));
    }
    result = engine->iterative_deepening(repetition, limits);
    require(result.bestMove == 0 && result.score == 0, "threefold draw missed");

    search::Options referenceOptions;
    referenceOptions.checkExtensions = false;
    referenceOptions.seePruning = false;
    referenceOptions.deltaPruning = false;
    auto referenceEngine = std::make_unique<search::Search>(1, referenceOptions);
    limits.depth = 1;
    limits.multiPv = MaxMoves;
    for (const char* fen : {
        "4k3/8/8/3q4/8/8/3R4/4K3 w - - 0 1",
        "7k/P7/8/8/8/8/8/7K w - - 0 1",
        "7k/8/8/3pP3/8/8/8/7K w - d6 0 1",
        "4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1",
        "7k/6p1/8/8/8/3q4/3R4/K7 w - - 0 1"
    }) {
        Board position(fen);
        MoveGenerator generator;
        result = referenceEngine->iterative_deepening(position, limits);
        require(result.variations.size() == static_cast<std::size_t>(generator.generate_moves(position).size()), "MultiPV did not include all legal roots");
        verify_pv(position, result);
        for (const auto& variation : result.variations) {
            position.make_move(variation.moves[0], true);
            int expected = -reference_quiescence(position);
            position.unmake_move(variation.moves[0], true);
            require(variation.score == expected, "depth-one score differs from exhaustive quiescence");
        }
    }

    limits.multiPv = 3;
    limits.depth = 64;
    limits.nodes = 1000;
    search::Result completed;
    result = engine->iterative_deepening(board, limits, {}, [&](const search::Result& progress) {
        completed = progress;
    });
    require(result.depth == completed.depth && result.bestMove == completed.bestMove &&
        result.score == completed.score, "interrupted iteration replaced completed result");
    require(result.stopped && result.nodes == limits.nodes && result.bestMove != 0, "node limit not respected");
    verify_pv(board, result);
    std::stop_source stopSource;
    stopSource.request_stop();
    result = engine->iterative_deepening(board, limits, stopSource.get_token());
    require(result.stopped && result.depth == 0 && result.nodes == 0 && result.bestMove != 0, "pre-cancelled search has no legal fallback");

    limits.nodes = 0;
    limits.time = std::chrono::milliseconds(20);
    result = engine->iterative_deepening(board, limits);
    require(result.stopped && result.elapsed < std::chrono::seconds(2), "time limit not respected");
    verify_pv(board, result);
    limits.time = std::chrono::milliseconds(0);
    stopSource = std::stop_source {};
    auto future = std::async(std::launch::async, [&] {
        return engine->iterative_deepening(board, limits, stopSource.get_token());
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    stopSource.request_stop();
    require(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "search cancellation blocked");
    result = future.get();
    require(result.stopped, "asynchronous cancellation ignored");
    verify_pv(board, result);

    engine->clear();
    limits.depth = 8;
    limits.multiPv = 2;
    limits.nodes = 150000;
    for (const char* fen : {
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "k3r3/8/8/8/8/8/8/R3K3 w - - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"
    }) {
        Board position(fen);
        result = engine->iterative_deepening(position, limits);
        require(result.depth > 0 && result.bestMove != 0, "complex search failed to complete an iteration");
        verify_pv(position, result);
    }
    limits.nodes = 0;
    limits.depth = 1;
    limits.multiPv = MaxMoves;
    result = engine->iterative_deepening(Board("7k/8/8/8/8/8/R7/K7 w - - 99 1"), limits);
    for (const auto& variation : result.variations)
        require(variation.score == 0, "search missed fifty-move draw at child");

    limits.depth = 5;
    limits.multiPv = 1;
    result = engine->iterative_deepening(board, limits);
    require(result.depth == 5 && !result.stopped, "engine not reusable after cancellation");
    verify_pv(board, result);
    std::cout << "Start position depth " << result.depth << ": " << result.nodes << " nodes in "
        << result.elapsed.count() << " ms\n";
}

}

int main() {
    try {
        search_tests::test_search();
        std::cout << "Search tests passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
