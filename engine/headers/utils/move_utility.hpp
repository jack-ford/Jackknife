#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "movegen/movegen.hpp"

namespace chess {

std::string coordinate_move(move_t move);
std::optional<move_t> parse_coordinate_move(const Board& board, std::string_view text);

std::optional<move_t> parse_san_move(const Board& board, std::string_view text);

}
