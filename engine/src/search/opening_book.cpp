#include "search/opening_book.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <stdexcept>

#include "utils/move_utility.hpp"

namespace search {
namespace {
std::string_view trim(std::string_view value) {
    auto start = value.find_first_not_of(" \t\r");
    if (start == std::string_view::npos)
        return {};
    return value.substr(start, value.find_last_not_of(" \t\r") - start + 1);
}
}

void OpeningBook::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input)
        throw std::invalid_argument("Cannot open opening book");
    load(input);
}

void OpeningBook::load(std::istream& input) {
    decltype(m_positions) positions;
    std::string line;
    while (std::getline(input, line)) {
        auto record = trim(line);
        if (record.empty() || record.front() == '#')
            continue;
        auto first = record.find('|');
        auto second = first == std::string_view::npos ? first : record.find('|', first + 1);
        if (first == std::string_view::npos || second == std::string_view::npos)
            throw std::invalid_argument("Invalid opening book record");
        chess::Board board(trim(record.substr(0, first)));
        auto move = chess::parse_coordinate_move(board, trim(record.substr(first + 1, second - first - 1)));
        auto weightText = trim(record.substr(second + 1));
        if (weightText.empty())
            throw std::invalid_argument("Missing opening book weight");
        uint32_t weight = 0;
        auto [end, error] = std::from_chars(weightText.data(), weightText.data() + weightText.size(), weight);
        if (!move || error != std::errc {} || end != weightText.data() + weightText.size() || weight == 0)
            throw std::invalid_argument("Invalid opening book move or weight");
        positions[board.zobrist_key()].push_back({*move, weight});
    }
    if (input.bad() || (!input.eof() && input.fail()))
        throw std::runtime_error("Unable to read opening book");
    m_positions.swap(positions);
}

std::optional<chess::move_t> OpeningBook::probe(const chess::Board& board, uint64_t choice) const {
    auto found = m_positions.find(board.zobrist_key());
    if (found == m_positions.end() || board.is_game_over().first != chess::GameResult::None)
        return std::nullopt;
    auto history = board.repetition_position_history();
    if (std::count(history.begin(), history.end(), board.zobrist_key()) > 1)
        return std::nullopt;
    chess::MoveGenerator generator;
    auto legal = generator.generate_moves(board);
    uint64_t total = 0;
    for (const auto& entry : found->second)
        if (std::find(legal.begin(), legal.end(), entry.move) != legal.end())
            total += entry.weight;
    if (total == 0)
        return std::nullopt;
    choice %= total;
    for (const auto& entry : found->second) {
        if (std::find(legal.begin(), legal.end(), entry.move) == legal.end())
            continue;
        if (choice < entry.weight)
            return entry.move;
        choice -= entry.weight;
    }
    return std::nullopt;
}

std::size_t OpeningBook::size() const { return m_positions.size(); }

}
