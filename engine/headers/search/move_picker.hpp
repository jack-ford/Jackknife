#pragma once

#include <array>
#include <functional>
#include <optional>

#include "core/board.hpp"
#include "movegen/movegen.hpp"

namespace search {

struct ScoredMove {
    chess::move_t move;
    int score;
    int exchange;
};

// The caller supplies a validated hash move. Each remaining stage is generated
// only when needed; SEE is evaluated only for selected tactical candidates.
// A supplied legalMoves list must outlive the picker; generator state is copied.
class MovePicker final {
    public:
        MovePicker(const chess::Board& board, chess::move_t hashMove,
            std::function<int(chess::move_t)> moveScore, bool tacticalOnly = false, const chess::MoveGenerator* generator = nullptr,
            const chess::MoveList* legalMoves = nullptr);
        std::optional<ScoredMove> next();

    private:
        enum class Stage { Hash, GenerateTactical, Tactical, GenerateQuiet, Quiet, Losing, Done };
        void generate(bool tactical);
        ScoredMove select();

        const chess::Board& m_board;
        const chess::MoveList* m_legalMoves;
        chess::move_t m_hashMove;
        std::function<int(chess::move_t)> m_moveScore;
        chess::MoveGenerator m_generator;
        std::array<ScoredMove, chess::MaxMoves> m_moves;
        int m_count = 0;
        int m_losingCount = 0;
        bool m_tacticalOnly;
        Stage m_stage = Stage::Hash;
};

}
