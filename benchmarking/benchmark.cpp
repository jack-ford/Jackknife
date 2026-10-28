#include "eval/evaluation.hpp"
#include "movegen/movegen.hpp"
#include "search/search.hpp"
#include "utils/fen_utility.hpp"
#include "utils/move_utility.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace chess;

namespace {

using Clock = std::chrono::steady_clock;

struct Position {
    std::string_view name;
    std::string_view fen;
};

struct PerftPosition : Position {
    int depth;
    uint64_t expected;
};

constexpr std::array Positions {
    Position {"start", FenUtility::StartPositionFen},
    Position {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"},
    Position {"rook_endgame", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"},
    Position {"promotions_castling", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"},
    Position {"tactical", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"},
    Position {"middlegame", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"}
};

constexpr std::array PerftPositions {
    PerftPosition {{"start", FenUtility::StartPositionFen}, 5, 4865609},
    PerftPosition {{"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"}, 4, 4085603},
    PerftPosition {{"rook_endgame", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"}, 5, 674624},
    PerftPosition {{"promotions_castling", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"}, 4, 422333},
    PerftPosition {{"tactical", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"}, 4, 2103487},
    PerftPosition {{"middlegame", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"}, 4, 3894594}
};

struct SearchRun {
    uint64_t nodes = 0;
    double seconds = 0;
    int completedDepth = 0;
    int score = 0;
    move_t bestMove = 0;
    search::Statistics statistics;
};

struct Feature {
    const char* name;
    search::Options options;
};

double seconds_since(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

uint64_t perft(Board& board, int depth) {
    MoveGenerator generator;
    MoveList moves = generator.generate_moves(board);
    if (depth == 1)
        return moves.size();

    uint64_t nodes = 0;
    for (move_t move : moves) {
        board.make_move(move, true);
        nodes += perft(board, depth - 1);
        board.unmake_move(move, true);
    }
    return nodes;
}

int parse_positive(std::string_view argument, const char* name) {
    int value = 0;
    auto [end, error] = std::from_chars(argument.data(), argument.data() + argument.size(), value);
    if (error != std::errc {} || end != argument.data() + argument.size() || value < 1)
        throw std::invalid_argument(std::string("Invalid ") + name);
    return value;
}

void print_environment(int depth, int repetitions) {
    std::cout << "benchmark,configuration,value\n";
    std::cout << "environment,search_depth," << depth << '\n';
    std::cout << "environment,repetitions," << repetitions << '\n';
#if defined(CHESS_AVX2_ENABLED)
    std::cout << "environment,nnue_kernel,avx2\n";
#else
    std::cout << "environment,nnue_kernel,scalar\n";
#endif
#if defined(NDEBUG)
    std::cout << "environment,build,release\n";
#else
    std::cout << "environment,build,debug\n";
#endif
}

void benchmark_movegen(int repetitions) {
    constexpr int GenerationIterations = 200000;
    std::cout << "\nmovegen,position,iterations,legal_moves,seconds,positions_per_second,moves_per_second\n";
    for (const auto& position : Positions) {
        Board board(position.fen);
        MoveGenerator generator;
        MoveList warmup = generator.generate_moves(board);
        uint64_t moveCount = 0;
        auto start = Clock::now();
        for (int repeat = 0; repeat < repetitions; ++repeat) {
            for (int iteration = 0; iteration < GenerationIterations; ++iteration)
                moveCount += generator.generate_moves(board).size();
        }
        double seconds = seconds_since(start);
        uint64_t iterations = static_cast<uint64_t>(GenerationIterations) * repetitions;
        std::cout << "movegen," << position.name << ',' << iterations << ',' << warmup.size() << ',' << seconds << ','
            << static_cast<uint64_t>(iterations / seconds) << ',' << static_cast<uint64_t>(moveCount / seconds) << '\n';
    }

    std::cout << "\nperft,position,depth,nodes,expected,seconds,nodes_per_second,passed\n";
    for (const auto& position : PerftPositions) {
        uint64_t nodes = 0;
        double seconds = 0;
        for (int repeat = 0; repeat < repetitions; ++repeat) {
            Board board(position.fen);
            auto start = Clock::now();
            nodes = perft(board, position.depth);
            seconds += seconds_since(start);
        }
        std::cout << "perft," << position.name << ',' << position.depth << ',' << nodes << ',' << position.expected << ','
            << seconds / repetitions << ',' << static_cast<uint64_t>(nodes * repetitions / seconds) << ','
            << (nodes == position.expected) << '\n';
        if (nodes != position.expected)
            throw std::runtime_error("Perft mismatch for " + std::string(position.name));
    }
}

void benchmark_evaluation(int repetitions) {
    constexpr int EvaluationIterations = 1000000;
    std::cout << "\nevaluation,position,iterations,seconds,evaluations_per_second,checksum\n";
    for (const auto& position : Positions) {
        Board board(position.fen);
        int64_t checksum = 0;
        auto start = Clock::now();
        for (int repeat = 0; repeat < repetitions; ++repeat) {
            for (int iteration = 0; iteration < EvaluationIterations; ++iteration)
                checksum += eval::evaluate(board);
        }
        double seconds = seconds_since(start);
        uint64_t iterations = static_cast<uint64_t>(EvaluationIterations) * repetitions;
        std::cout << "evaluation," << position.name << ',' << iterations << ',' << seconds << ','
            << static_cast<uint64_t>(iterations / seconds) << ',' << checksum << '\n';
    }
}

SearchRun run_search(const Position& position, int depth, int repetitions, const search::Options& options) {
    SearchRun aggregate;
    search::Limits limits;
    limits.depth = depth;
    limits.time = std::chrono::milliseconds {0};
    search::Search engine(16, options);

    for (int repeat = 0; repeat < repetitions; ++repeat) {
        Board board(position.fen);
        engine.clear();
        auto start = Clock::now();
        search::Result result = engine.iterative_deepening(board, limits);
        aggregate.seconds += seconds_since(start);
        aggregate.nodes += result.nodes;
        aggregate.completedDepth = result.depth;
        aggregate.score = result.score;
        aggregate.bestMove = result.bestMove;
        aggregate.statistics.hashHits += result.statistics.hashHits;
        aggregate.statistics.hashCutoffs += result.statistics.hashCutoffs;
        aggregate.statistics.aspirationResearches += result.statistics.aspirationResearches;
        aggregate.statistics.pvsResearches += result.statistics.pvsResearches;
        aggregate.statistics.nullAttempts += result.statistics.nullAttempts;
        aggregate.statistics.nullCutoffs += result.statistics.nullCutoffs;
        aggregate.statistics.lateReductions += result.statistics.lateReductions;
        aggregate.statistics.lateResearches += result.statistics.lateResearches;
        aggregate.statistics.futilityPrunes += result.statistics.futilityPrunes;
        aggregate.statistics.seePrunes += result.statistics.seePrunes;
        aggregate.statistics.deltaPrunes += result.statistics.deltaPrunes;
    }
    return aggregate;
}

void benchmark_search(int depth, int repetitions) {
    std::cout << "\nsearch,position,depth,nodes,seconds,nps,score,bestmove,hash_hits,hash_cutoffs,aspiration_researches,pvs_researches,null_attempts,null_cutoffs,lmr,lmr_researches,futility_prunes,see_prunes,delta_prunes\n";
    uint64_t totalNodes = 0;
    double totalSeconds = 0;
    for (const auto& position : Positions) {
        SearchRun result = run_search(position, depth, repetitions, {});
        totalNodes += result.nodes;
        totalSeconds += result.seconds;
        const auto& stats = result.statistics;
        std::cout << "search," << position.name << ',' << result.completedDepth << ',' << result.nodes << ',' << result.seconds << ','
            << static_cast<uint64_t>(result.nodes / result.seconds) << ',' << result.score << ',' << coordinate_move(result.bestMove) << ','
            << stats.hashHits << ',' << stats.hashCutoffs << ',' << stats.aspirationResearches << ',' << stats.pvsResearches << ','
            << stats.nullAttempts << ',' << stats.nullCutoffs << ',' << stats.lateReductions << ',' << stats.lateResearches << ','
            << stats.futilityPrunes << ',' << stats.seePrunes << ',' << stats.deltaPrunes << '\n';
    }
    std::cout << "search,total," << depth << ',' << totalNodes << ',' << totalSeconds << ','
        << static_cast<uint64_t>(totalNodes / totalSeconds) << ",0,-,0,0,0,0,0,0,0,0,0,0,0\n";
}

std::vector<Feature> feature_configurations() {
    search::Options baseline;
    std::vector<Feature> features {{"baseline", baseline}};
    const auto disabled = [&](const char* name, auto member) {
        search::Options options = baseline;
        options.*member = false;
        features.push_back({name, options});
    };
    const auto enabled = [&](const char* name, auto member) {
        search::Options options = baseline;
        options.*member = true;
        features.push_back({name, options});
    };

    disabled("no_transposition_table", &search::Options::transpositionTable);
    disabled("no_aspiration_windows", &search::Options::aspirationWindows);
    disabled("no_principal_variation_search", &search::Options::principalVariationSearch);
    disabled("no_null_move_pruning", &search::Options::nullMovePruning);
    disabled("no_reverse_futility_pruning", &search::Options::reverseFutilityPruning);
    disabled("no_futility_pruning", &search::Options::futilityPruning);
    disabled("no_check_extensions", &search::Options::checkExtensions);
    disabled("no_recapture_extensions", &search::Options::recaptureExtensions);
    disabled("no_internal_iterative_reduction", &search::Options::internalIterativeReduction);
    disabled("no_late_move_reductions", &search::Options::lateMoveReductions);
    disabled("no_history_heuristic", &search::Options::historyHeuristic);
    disabled("no_killer_moves", &search::Options::killerMoves);
    disabled("no_see_pruning", &search::Options::seePruning);
    disabled("no_delta_pruning", &search::Options::deltaPruning);
    disabled("no_razoring", &search::Options::razoring);
    disabled("no_continuation_history", &search::Options::continuationHistory);
    disabled("no_singular_extensions", &search::Options::singularExtensions);
    disabled("no_correction_history", &search::Options::correctionHistory);
    enabled("counter_moves", &search::Options::counterMoves);
    enabled("capture_history", &search::Options::captureHistory);
    enabled("pawn_history", &search::Options::pawnHistory);
    enabled("probcut", &search::Options::probCut);
    return features;
}

void benchmark_features(int depth, int repetitions) {
    auto features = feature_configurations();
    std::vector<SearchRun> totals(features.size());
    std::vector<std::array<move_t, Positions.size()>> bestMoves(features.size());

    for (std::size_t feature = 0; feature < features.size(); ++feature) {
        for (std::size_t position = 0; position < Positions.size(); ++position) {
            SearchRun run = run_search(Positions[position], depth, repetitions, features[feature].options);
            totals[feature].nodes += run.nodes;
            totals[feature].seconds += run.seconds;
            bestMoves[feature][position] = run.bestMove;
        }
    }

    const SearchRun& baseline = totals[0];
    std::cout << "\nfeatures,configuration,depth,nodes,seconds,nps,nodes_ratio,time_ratio,nps_ratio,bestmove_agreement\n";
    for (std::size_t feature = 0; feature < features.size(); ++feature) {
        const SearchRun& run = totals[feature];
        int agreement = 0;
        for (std::size_t position = 0; position < Positions.size(); ++position)
            agreement += bestMoves[feature][position] == bestMoves[0][position];
        std::cout << "features," << features[feature].name << ',' << depth << ',' << run.nodes << ',' << run.seconds << ','
            << static_cast<uint64_t>(run.nodes / run.seconds) << ',' << static_cast<double>(run.nodes) / baseline.nodes << ','
            << run.seconds / baseline.seconds << ',' << (run.nodes / run.seconds) / (baseline.nodes / baseline.seconds) << ','
            << agreement << '/' << Positions.size() << '\n';
    }
}

}

int main(int argc, char** argv) {
    try {
        if (argc > 4)
            throw std::invalid_argument("Usage: chess_benchmark [all|movegen|evaluation|search|features] [depth] [repetitions]");

        std::string mode = argc > 1 ? argv[1] : "all";
        int depth = argc > 2 ? parse_positive(argv[2], "depth") : 7;
        int repetitions = argc > 3 ? parse_positive(argv[3], "repetitions") : 1;
        if (depth >= search::MaxPly)
            throw std::invalid_argument("Depth exceeds search limit");
        if (mode != "all" && mode != "movegen" && mode != "evaluation" && mode != "search" && mode != "features")
            throw std::invalid_argument("Unknown benchmark mode");

        std::cout << std::fixed << std::setprecision(6);
        print_environment(depth, repetitions);
        if (mode == "all" || mode == "movegen") benchmark_movegen(repetitions);
        if (mode == "all" || mode == "evaluation") benchmark_evaluation(repetitions);
        if (mode == "all" || mode == "search") benchmark_search(depth, repetitions);
        if (mode == "all" || mode == "features") benchmark_features(depth, repetitions);
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
