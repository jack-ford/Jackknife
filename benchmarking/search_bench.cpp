#include "search/search.hpp"
#include "utils/move_utility.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    try {
        if (argc > 4 || (argc > 3 && std::string(argv[3]) != "experimental"))
            throw std::invalid_argument("Usage: chess_bench [depth] [suite.txt or -] [experimental]");
        search::Limits limits;
        limits.depth = 6;
        limits.time = std::chrono::milliseconds {0};
        if (argc > 1) {
            std::string argument = argv[1];
            auto [end, error] = std::from_chars(argument.data(), argument.data() + argument.size(), limits.depth);
            if (error != std::errc {} || end != argument.data() + argument.size() ||
                limits.depth < 1 || limits.depth >= search::MaxPly)
                throw std::invalid_argument("Usage: chess_bench [depth] [suite.txt] [experimental]");
        }
        search::Options options;
        if (argc > 3 && std::string(argv[3]) == "experimental") {
            options.continuationHistory = true;
            options.counterMoves = true;
            options.captureHistory = true;
            options.pawnHistory = true;
            options.singularExtensions = true;
            options.probCut = true;
            options.razoring = true;
            options.correctionHistory = true;
        }
        std::vector<std::string> positions {
            std::string(chess::FenUtility::StartPositionFen),
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
            "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
            "4k3/8/8/3q4/8/8/3R4/4K3 w - - 0 1|d2d5",
            "7k/P7/8/8/8/8/8/7K w - - 0 1|a7a8q a7a8r"
        };
        if (argc > 2 && std::string(argv[2]) != "-") {
            std::ifstream input(argv[2]);
            if (!input)
                throw std::invalid_argument("Cannot open benchmark suite");
            positions.clear();
            std::string line;
            while (std::getline(input, line)) {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (!line.empty() && line[0] != '#')
                    positions.push_back(line);
            }
            if (positions.empty())
                throw std::invalid_argument("Empty benchmark suite");
        }
        auto engine = std::make_unique<search::Search>(16, options);
        uint64_t totalNodes = 0;
        double totalSeconds = 0;
        int failures = 0;
        std::cout << "position,depth,nodes,seconds,nps,score,bestmove,passed\n";
        for (std::size_t index = 0; index < positions.size(); ++index) {
            const auto& line = positions[index];
            auto delimiter = line.find('|');
            chess::Board board(line.substr(0, delimiter));
            engine->clear();
            auto start = std::chrono::steady_clock::now();
            auto result = engine->iterative_deepening(board, limits);
            double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            std::string best = chess::coordinate_move(result.bestMove);
            bool passed = delimiter == std::string::npos;
            if (!passed) {
                std::istringstream expected(line.substr(delimiter + 1));
                std::string move;
                while (expected >> move)
                    passed = passed || move == best;
            }
            failures += !passed;
            totalNodes += result.nodes;
            totalSeconds += seconds;
            std::cout << index + 1 << ',' << result.depth << ',' << result.nodes << ',' << seconds << ','
                << static_cast<uint64_t>(result.nodes / std::max(seconds, 0.000001)) << ',' << result.score << ','
                << best << ',' << passed << '\n';
        }
        std::cout << "total,0," << totalNodes << ',' << totalSeconds << ','
            << static_cast<uint64_t>(totalNodes / std::max(totalSeconds, 0.000001)) << ",0,-,"
            << (failures == 0) << '\n';
        return failures == 0 ? 0 : 1;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
