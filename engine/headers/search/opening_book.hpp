#pragma once

#include <filesystem>
#include <istream>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/board.hpp"

namespace search {

// Native text format: full FEN | coordinate move | positive integer weight.
// Reloading is transactional. This is not the Polyglot binary format.
class OpeningBook final {
    public:
        void load(const std::filesystem::path& path);
        void load(std::istream& input);
        std::optional<chess::move_t> probe(const chess::Board& board, uint64_t choice = 0) const;
        std::size_t size() const;

    private:
        struct Entry {
            chess::move_t move;
            uint32_t weight;
        };
        std::unordered_map<uint64_t, std::vector<Entry>> m_positions;
};

}
