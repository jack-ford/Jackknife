#pragma once

#include <array>
#include <chrono>
#include <functional>
#include <optional>
#include <stop_token>
#include <vector>

#include "core/board.hpp"
#include "movegen/movegen.hpp"
#include "eval/evaluation.hpp"
#include "utils/search_utility.hpp"
#include "search/transposition_table.hpp"
#include "eval/static_exchange.hpp"

namespace search {

struct Options {
    bool transpositionTable = true;
    bool aspirationWindows = true;
    bool principalVariationSearch = true;
    bool nullMovePruning = true;
    bool reverseFutilityPruning = true;
    bool futilityPruning = true;
    bool checkExtensions = true;
    bool recaptureExtensions = true;
    bool internalIterativeReduction = true;
    bool lateMoveReductions = true;
    bool historyHeuristic = true;
    bool killerMoves = true;
    bool seePruning = true;
    bool deltaPruning = true;
};

struct Statistics {
    uint64_t hashHits = 0;
    uint64_t hashCutoffs = 0;
    uint64_t aspirationResearches = 0;
    uint64_t pvsResearches = 0;
    uint64_t nullAttempts = 0;
    uint64_t nullCutoffs = 0;
    uint64_t nullVerifications = 0;
    uint64_t reverseFutilityCutoffs = 0;
    uint64_t futilityPrunes = 0;
    uint64_t checkExtensions = 0;
    uint64_t recaptureExtensions = 0;
    uint64_t internalReductions = 0;
    uint64_t lateReductions = 0;
    uint64_t lateResearches = 0;
    uint64_t seePrunes = 0;
    uint64_t deltaPrunes = 0;
    uint64_t tablebaseHits = 0;
};

struct Limits {
    int depth = 64;
    int multiPv = 1;
    uint64_t nodes = 0;
    std::chrono::milliseconds time {1000};
    std::chrono::milliseconds softTime {0};
};

enum class TablebaseOutcome { Loss, Draw, Win };

struct Integrations {
    std::function<std::optional<TablebaseOutcome>(const chess::Board&)> probeTablebase;
};

struct Variation {
    int score = 0;
    std::vector<chess::move_t> moves;
};

struct ScoredMove {
    move_t move;
    int score;
    int exchange;
};

struct Result {
    chess::move_t bestMove = 0;
    int score = 0;
    int depth = 0;
    int selectiveDepth = 0;
    chess::move_t ponderMove = 0;
    uint64_t nodes = 0;
    std::chrono::milliseconds elapsed {0};
    bool stopped = false;
    Statistics statistics;
    std::vector<Variation> variations;
};

class Search final {
    public:
        explicit Search(std::size_t hashMegabytes = DefaultHashMegabytes, const Options& options = {});
        Result iterative_deepening(chess::Board board, const Limits& limits = {},
            std::stop_token stopToken = {}, const std::function<void(const Result&)>& onIteration = {});
        void clear();
        void set_integrations(Integrations integrations);

    private:
        bool should_stop();
        bool is_draw(const chess::Board& board, bool nullSearch) const;
        int aspiration_window(chess::Board& board, int depth, int center,
            const std::vector<chess::move_t>& excluded, chess::move_t preferred);
        int pvs(chess::Board& board, int depth, int alpha, int beta, int ply,
            int extensions, chess::square_t recaptureSquare, bool pvNode, bool nullSearch,
            bool allowNull, bool firstMove, int reduction);
        int root_max(chess::Board& board, int depth, int alpha, int beta,
            const std::vector<chess::move_t>& excluded, chess::move_t preferred);
        int nega_max(chess::Board& board, int depth, int alpha, int beta, int ply,
            int extensions, chess::square_t recaptureSquare, bool pvNode, bool nullSearch, bool allowNull = true);
        int quiescence_search(chess::Board& board, int alpha, int beta, int ply, bool nullSearch);
        void order_moves(const chess::Board& board, chess::MoveList& moves, chess::move_t hashMove, int ply);
        void update_pv(int ply, chess::move_t move);
        void reward_quiet(chess::PieceColor color, chess::move_t move, int depth, int ply,
            const chess::MoveList& quietMoves);
        void advance_history(const chess::Board& board, int ply);
        uint64_t context_key(const chess::Board& board, int ply, int extensions,
            chess::square_t recaptureSquare, bool nullSearch, bool allowNull = true) const;
        HashProbe probe_hash(const chess::Board& board, uint64_t context, int ply);
        void store_hash(const chess::Board& board, uint64_t context, chess::move_t move,
            int score, int staticScore, int depth, int ply, int originalAlpha, int beta);
        int futility_margin(int depth, bool quiet, int quality) const;
        int late_reduction(int depth, int searched, int quality, bool pvNode, bool improving,
            int evaluationDelta, bool quiet) const;

        TranspositionTable m_hash;
        Options m_options;
        Integrations m_integrations;
        Statistics m_statistics;
        std::array<uint64_t, MaxPly> m_historyKeys {};
        std::array<int, MaxPly> m_staticScores {};
        std::array<std::array<int, chess::MaxMoves>, MaxPly> m_exchangeScores {};
        std::array<std::array<int, chess::MaxMoves + 1>, MaxPly> m_reductions {};
        std::array<std::array<std::array<int, 64>, 64>, 2> m_history {};
        std::array<std::array<chess::move_t, 2>, MaxPly> m_killers {};
        std::array<std::array<chess::move_t, MaxPly>, MaxPly> m_pv {};
        std::array<int, MaxPly> m_pvLength {};
        chess::MoveGenerator m_generator;
        Limits m_limits;
        std::stop_token m_stopToken;
        std::chrono::steady_clock::time_point m_start;
        uint64_t m_nodes = 0;
        int m_selectiveDepth = 0;
        bool m_stopped = false;
};

}
