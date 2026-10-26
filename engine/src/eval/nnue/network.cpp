#include "eval/nnue/architecture.hpp"
#include "eval/nnue/network.hpp"

#include <bit>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace eval::nnue {

extern const unsigned char EmbeddedNetwork[sizeof(Network)];

namespace {
struct NetworkState {
    std::mutex mutex;
    std::unique_ptr<Network> weights;
    bool frozen = false;
};

NetworkState& network_state() {
    static NetworkState state;
    return state;
}
}

void load_network(const std::filesystem::path& path) {
    auto& state = network_state();
    std::lock_guard lock(state.mutex);
    if (state.frozen)
        throw std::logic_error("Configure NNUE before constructing boards or accessing weights");
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() != static_cast<std::streamoff>(sizeof(Network)))
        throw std::invalid_argument("NNUE file has an incompatible size or cannot be opened");
    auto weights = std::make_unique_for_overwrite<Network>();
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(weights.get()), sizeof(Network)))
        throw std::runtime_error("Unable to read NNUE weights");
    state.weights = std::move(weights);
}

const Network& network() {
    static_assert(std::endian::native == std::endian::little, "Casanchess network requires little-endian storage");
    static const Network* weights = [] {
        auto& state = network_state();
        std::lock_guard lock(state.mutex);
        if (!state.weights) {
            state.weights = std::make_unique_for_overwrite<Network>();
            std::memcpy(state.weights.get(), EmbeddedNetwork, sizeof(Network));
        }
        state.frozen = true;
        return state.weights.get();
    }();
    return *weights;
}

}
