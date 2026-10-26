#include "core/board.hpp"
#include "eval/evaluation.hpp"
#include "eval/nnue/network.hpp"

#include <iostream>
#include <chrono>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace {
struct TemporaryNetwork {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("chess-network-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".nnue");
    ~TemporaryNetwork() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};
}

int main() {
    try {
        bool rejected = false;
        try { eval::nnue::load_network("missing-network.nnue"); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("missing network accepted");
        rejected = false;
        try { eval::nnue::load_network(INVALID_NETWORK_PATH); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("wrong-sized network accepted");
        auto weights = std::make_unique<eval::nnue::Network>();
        std::ifstream source(TEST_NETWORK_PATH, std::ios::binary);
        if (!source.read(reinterpret_cast<char*>(weights.get()), sizeof(*weights)))
            throw std::runtime_error("cannot read network fixture");
        weights->outputBias += 64;
        TemporaryNetwork custom;
        {
            std::ofstream output(custom.path, std::ios::binary);
            if (!output.write(reinterpret_cast<const char*>(weights.get()), sizeof(*weights)))
                throw std::runtime_error("cannot write network fixture");
        }
        eval::nnue::load_network(custom.path);
        try { eval::nnue::load_network(INVALID_NETWORK_PATH); }
        catch (const std::invalid_argument&) {}
        if (eval::nnue::network().outputBias != weights->outputBias)
            throw std::runtime_error("custom network was not loaded or failed reload discarded it");
        chess::Board board;
        auto accumulator = board.nnue_accumulator();
        int score = eval::evaluate(board);
        rejected = false;
        try { eval::nnue::load_network(TEST_NETWORK_PATH); }
        catch (const std::logic_error&) { rejected = true; }
        if (!rejected || eval::evaluate(board) != score || board.nnue_accumulator() != accumulator)
            throw std::runtime_error("live network replacement invalidated board");
        std::cout << "Network loading tests passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
