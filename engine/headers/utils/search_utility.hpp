#pragma once

#include "core/board.hpp"
#include "search/constants.hpp"

namespace search {

bool is_capture(const chess::Board& board, chess::move_t move);
bool is_quiet(const chess::Board& board, chess::move_t move);
bool is_advanced_pawn(const chess::Board& board, chess::move_t move);
bool has_null_material(const chess::Board& board);
uint64_t mix_key(uint64_t key);

}
