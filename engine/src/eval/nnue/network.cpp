#include "eval/nnue/architecture.hpp"

#include <bit>
#include <cstring>
#include <memory>

namespace eval::nnue {

extern const unsigned char EmbeddedNetwork[sizeof(Network)];

const Network& network() {
    static_assert(std::endian::native == std::endian::little, "Casanchess network requires little-endian storage");
    
    static const auto weights = [] {
        auto result = std::make_unique_for_overwrite<Network>();
        std::memcpy(result.get(), EmbeddedNetwork, sizeof(Network));
        return result;
    }();
    
    return *weights;
}

}
