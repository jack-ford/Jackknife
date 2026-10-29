#include "search_test_utils.hpp"

#include <iostream>
#include <memory>

namespace search_tests {

search::Options exact_options(bool useTable) {
    search::Options options;
    options.transpositionTable = useTable;
    options.nullMovePruning = false;
    options.reverseFutilityPruning = false;
    options.futilityPruning = false;
    options.checkExtensions = false;
    options.recaptureExtensions = false;
    options.internalIterativeReduction = false;
    options.lateMoveReductions = false;
    options.seePruning = false;
    options.deltaPruning = false;
    return options;
}

int reference_search(Board& board, int depth, int ply) {
    if (depth <= 0)
        return reference_quiescence(board, ply);
    MoveGenerator generator;
    MoveList moves = generator.generate_moves(board);
    if (moves.is_empty())
        return board.is_in_check() ? -search::MateScore + ply : 0;
    if (board.is_game_over().first == GameResult::Draw)
        return 0;
    int score = -search::Infinity;
    for (move_t move : moves) {
        board.make_move(move, true);
        score = std::max(score, -reference_search(board, depth - 1, ply + 1));
        board.unmake_move(move, true);
    }
    return score;
}

void test_exact_search() {
    search::Limits limits;
    limits.time = std::chrono::milliseconds(0);
    limits.depth = 3;
    limits.multiPv = 3;
    auto cached = std::make_unique<search::Search>(1, exact_options(true));
    auto uncached = std::make_unique<search::Search>(1, exact_options(false));
    search::Options plainOptions = exact_options(false);
    plainOptions.principalVariationSearch = false;
    plainOptions.aspirationWindows = false;
    auto plain = std::make_unique<search::Search>(1, plainOptions);
    uint64_t cutoffs = 0;
    for (const char* fen : {
        "4k3/8/8/3q4/8/8/3R4/4K3 w - - 0 1",
        "7k/P7/8/8/8/8/8/7K w - - 0 1",
        "7k/8/8/3pP3/8/8/8/7K w - d6 0 1",
        "7k/8/5KQ1/8/8/8/8/8 w - - 0 1",
        "k3r3/8/8/8/8/8/8/R3K3 w - - 0 1"
    }) {
        Board board(fen);
        cached->clear();
        auto expected = uncached->iterative_deepening(board, limits);
        auto baseline = plain->iterative_deepening(board, limits);
        require(expected.score == baseline.score, "PVS differs from full-window alpha-beta");
        require(expected.score == reference_search(board, limits.depth, 0), "alpha-beta differs from exhaustive reference");
        for (int repeat = 0; repeat < 2; ++repeat) {
            auto result = cached->iterative_deepening(board, limits);
            require(result.score == expected.score, "TT changed exact search score");
            verify_pv(board, result);
            require(result.variations.size() == expected.variations.size(), "TT changed MultiPV size");
            for (std::size_t index = 0; index < result.variations.size(); ++index)
                require(result.variations[index].score == expected.variations[index].score, "TT contaminated excluded-root MultiPV score");
            cutoffs += result.statistics.hashCutoffs;
        }
    }
    require(cutoffs > 0, "table never produced an actual search cutoff");

    for (int clock : {0, 98, 0, 98}) {
        Board board("7k/8/8/8/8/8/R7/K7 w - - " + std::to_string(clock) + " 1");
        auto expected = uncached->iterative_deepening(board, limits);
        auto result = cached->iterative_deepening(board, limits);
        require(result.score == expected.score, "TT crossed fifty-move histories");
        require(clock == 0 ? result.score > 0 : result.score == 0, "fifty-move test did not distinguish histories");
    }
    Board history;
    for (auto [from, to] : {std::pair{6, 21}, {62, 45}, {21, 6}, {45, 62}})
        history.make_move(Move::create(static_cast<square_t>(from), static_cast<square_t>(to)));
    Board fresh(history.current_fen());
    require(history.zobrist_key() == fresh.zobrist_key(), "history fixture has different position key");
    cached->iterative_deepening(fresh, limits);
    auto result = cached->iterative_deepening(history, limits);
    auto expected = uncached->iterative_deepening(history, limits);
    require(result.score == expected.score, "TT crossed repetition histories");
    verify_pv(history, result);
}

void test_selective_search() {
    auto engine = std::make_unique<search::Search>(4);
    search::Limits limits;
    limits.time = std::chrono::milliseconds(0);
    limits.depth = 8;
    limits.nodes = 300000;
    search::Statistics totals;
    for (const char* fen : {
        FenUtility::StartPositionFen.data(),
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r1bq1rk1/ppp2ppp/2np1n2/4p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w - - 0 7"
    }) {
        Board board(fen);
        auto result = engine->iterative_deepening(board, limits);
        verify_pv(board, result);
        auto stats = result.statistics;
        totals.hashCutoffs += stats.hashCutoffs;
        totals.aspirationResearches += stats.aspirationResearches;
        totals.pvsResearches += stats.pvsResearches;
        totals.nullAttempts += stats.nullAttempts;
        totals.nullCutoffs += stats.nullCutoffs;
        totals.nullVerifications += stats.nullVerifications;
        totals.reverseFutilityCutoffs += stats.reverseFutilityCutoffs;
        totals.futilityPrunes += stats.futilityPrunes;
        totals.checkExtensions += stats.checkExtensions;
        totals.recaptureExtensions += stats.recaptureExtensions;
        totals.internalReductions += stats.internalReductions;
        totals.lateReductions += stats.lateReductions;
        totals.lateResearches += stats.lateResearches;
        totals.seePrunes += stats.seePrunes;
        totals.deltaPrunes += stats.deltaPrunes;
    }
    std::cout << "TT " << totals.hashCutoffs << ", AW " << totals.aspirationResearches
        << ", PVS " << totals.pvsResearches << ", NMP " << totals.nullAttempts << '/' << totals.nullCutoffs
        << ", verification " << totals.nullVerifications << ", RFP " << totals.reverseFutilityCutoffs
        << ", FP " << totals.futilityPrunes << ", check " << totals.checkExtensions
        << ", recapture " << totals.recaptureExtensions << ", IIR " << totals.internalReductions
        << ", LMR " << totals.lateReductions << '/' << totals.lateResearches << '\n';
    require(totals.hashCutoffs > 0 && totals.pvsResearches > 0, "TT/PVS branches unexercised");
    require(totals.nullAttempts > 0 && totals.nullCutoffs > 0, "null pruning unexercised");
    require(totals.reverseFutilityCutoffs > 0 && totals.futilityPrunes > 0, "futility pruning unexercised");
    require(totals.checkExtensions > 0 && totals.recaptureExtensions > 0, "extensions unexercised");
    require(totals.internalReductions > 0 && totals.lateReductions > 0 && totals.lateResearches > 0, "reductions/re-search unexercised");
    require(totals.aspirationResearches > 0, "aspiration retry unexercised");
    require(totals.seePrunes > 0 && totals.deltaPrunes > 0, "quiescence pruning unexercised");
}

}

int main() {
    try {
        search_tests::test_exact_search();
        search_tests::test_selective_search();
        std::cout << "Search algorithm tests passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

