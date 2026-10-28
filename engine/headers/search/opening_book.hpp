#pragma once

#include <filesystem>
#include <istream>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/board.hpp"

namespace search {

// Bundled Stockfish book (official-stockfish/books), relative to the executable.
inline constexpr std::string_view DefaultBookPath = "data/books/8moves_v3.pgn";

// Native text format: full FEN | coordinate move | positive integer weight.
// PGN format: every mainline move of every game adds one to that move's weight,
// as used by the PGN books in official-stockfish/books. Comments, variations,
// NAGs and FEN tags are accepted. Position-only EPD books carry no moves.
// Reloading is transactional. This is not the Polyglot binary format.
class OpeningBook final {
    public:
        enum class Format { Native, Pgn };

        // Files ending in .pgn load as PGN; any other file loads as native text.
        void load(const std::filesystem::path& path);
        void load(std::istream& input, Format format = Format::Native);
        std::optional<chess::move_t> probe(const chess::Board& board, uint64_t choice = 0) const;
        std::size_t size() const;

    private:
        struct Entry {
            chess::move_t move;
            uint32_t weight;
        };
        using Positions = std::unordered_map<uint64_t, std::vector<Entry>>;

        static Positions read_native(std::istream& input);
        static Positions read_pgn(std::istream& input);
        static void add(Positions& positions, const chess::Board& board, chess::move_t move, uint32_t weight);

        Positions m_positions;
};

}
