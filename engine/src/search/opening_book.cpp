#include "search/opening_book.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include "utils/move_utility.hpp"

namespace search {
namespace {
std::string_view trim(std::string_view value) {
    auto start = value.find_first_not_of(" \t\r");

    if (start == std::string_view::npos)
        return {};

    return value.substr(start, value.find_last_not_of(" \t\r") - start + 1);
}

bool is_result(std::string_view token) {
    return token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*";
}
}

void OpeningBook::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);

    if (!input)
        throw std::invalid_argument("Cannot open opening book");

    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });

    load(input, extension == ".pgn" ? Format::Pgn : Format::Native);
}

void OpeningBook::load(std::istream& input, Format format) {
    auto positions = format == Format::Pgn ? read_pgn(input) : read_native(input);
    m_positions.swap(positions);
}

void OpeningBook::add(Positions& positions, const chess::Board& board, chess::move_t move, uint32_t weight) {
    auto& entries = positions[board.zobrist_key()];
    auto existing = std::find_if(entries.begin(), entries.end(), [move](const Entry& entry) { return entry.move == move; });

    if (existing == entries.end())
        entries.push_back({move, weight});
    else
        existing->weight += weight;
}

OpeningBook::Positions OpeningBook::read_native(std::istream& input) {
    Positions positions;
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

        add(positions, board, *move, weight);
    }

    if (input.bad() || (!input.eof() && input.fail()))
        throw std::runtime_error("Unable to read opening book");

    return positions;
}

OpeningBook::Positions OpeningBook::read_pgn(std::istream& input) {
    std::string text {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (input.bad())
        throw std::runtime_error("Unable to read opening book");

    Positions positions;
    chess::Board board;
    std::size_t game = 1;
    bool inMoves = false;

    auto finish_game = [&] {
        if (inMoves)
            game++;

        inMoves = false;
        board.load_start_position();
    };

    for (std::size_t index = 0; index < text.size();) {
        char character = text[index];
        if (std::isspace(static_cast<unsigned char>(character))) {
            index++;
        }
        else if (character == '[') {
            std::size_t end = text.find(']', index);
            if (end == std::string::npos)
                throw std::invalid_argument("Unterminated PGN tag in game " + std::to_string(game));

            if (inMoves)
                finish_game();

            std::string_view tag(text.data() + index + 1, end - index - 1);
            if (tag.starts_with("FEN ")) {
                auto open = tag.find('"');
                auto close = tag.rfind('"');

                if (open == std::string_view::npos || close == open)
                    throw std::invalid_argument("Invalid PGN FEN tag in game " + std::to_string(game));

                board.load_position(tag.substr(open + 1, close - open - 1));
            }

            index = end + 1;
        }
        else if (character == '{') {
            std::size_t end = text.find('}', index);
            index = end == std::string::npos ? text.size() : end + 1;
        }
        else if (character == ';') {
            std::size_t end = text.find('\n', index);
            index = end == std::string::npos ? text.size() : end + 1;
        }
        else if (character == '(') {
            int depth = 0;

            for (; index < text.size(); index++) {
                if (text[index] == '{') {
                    index = text.find('}', index);
                    if (index == std::string::npos)
                        break;
                }
                else if (text[index] == '(')
                    depth++;
                else if (text[index] == ')' && --depth == 0)
                    break;
            }

            if (index >= text.size())
                throw std::invalid_argument("Unterminated PGN variation in game " + std::to_string(game));

            index++;
        }
        else {
            std::size_t end = index;
            while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end])) &&
                std::string_view("[]{}();").find(text[end]) == std::string_view::npos)
                end++;

            std::string_view token(text.data() + index, std::max(end, index + 1) - index);
            index = std::max(end, index + 1);

            if (is_result(token)) {
                finish_game();
                continue;
            }

            if (token.front() == '$')
                continue;

            // Move numbers may be attached to the move
            auto number = token.find_first_not_of("0123456789");

            if (number != std::string_view::npos && token[number] == '.') {
                auto start = token.find_first_not_of('.', number);
                token = start == std::string_view::npos ? std::string_view {} : token.substr(start);
            }

            if (token.empty())
                continue;

            auto move = chess::parse_san_move(board, token);
            if (!move)
                throw std::invalid_argument("Invalid PGN move " + std::string(token) + " in game " + std::to_string(game));

            add(positions, board, *move, 1);
            board.make_move(*move);
            inMoves = true;
        }
    }
    return positions;
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
