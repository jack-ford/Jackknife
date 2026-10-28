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
#include "search/move_picker.hpp"

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
    bool continuationHistory = true;
    bool counterMoves = false;
    bool captureHistory = false;
    bool pawnHistory = false;
    bool singularExtensions = true;
    bool probCut = false;
    bool razoring = true;
    bool correctionHistory = true;
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
    uint64_t singularExtensions = 0;
    uint64_t probCutCutoffs = 0;
    uint64_t razorCutoffs = 0;
    uint64_t continuationUpdates = 0;
    uint64_t pawnHistoryUpdates = 0;
    uint64_t captureHistoryUpdates = 0;
    uint64_t counterMoveUpdates = 0;
    uint64_t correctionUpdates = 0;
};

struct Limits {
    int depth = 64;
    int multiPv = 1;
    uint64_t nodes = 0;
    std::chrono::milliseconds time {1000};
    std::chrono::milliseconds softTime {0};
    // Empty means all legal root moves. Callers must supply legal moves from
    // the root position; unknown entries are ignored defensively.
    std::vector<chess::move_t> searchMoves;

    void validate() const;
};

// Per-node search state passed from a parent to a child.
struct NodeContext {
    int ply = 0;
    int extensions = 0;
    chess::square_t recaptureSquare = chess::Square::None;
    bool pvNode = false;
    // Below a null move: repetition and fifty-move draws are not claimed.
    bool nullSearch = false;
    // False inside null-move verification and singular searches.
    bool allowNull = true;
    // Singular-extension verification searches every move except this one.
    chess::move_t excludedMove = 0;
};

enum class TablebaseOutcome { Loss, Draw, Win };

struct TablebaseRoot {
    TablebaseOutcome outcome;
    chess::move_t move = 0;
    int distanceToZero = 0;
};

struct Integrations {
    std::function<std::optional<TablebaseOutcome>(const chess::Board&)> probeTablebase;
    std::function<std::optional<TablebaseRoot>(const chess::Board&)> probeRoot;
    std::function<std::optional<chess::move_t>(const chess::Board&)> probeBook;
};

struct Variation {
    int score = 0;
    std::vector<chess::move_t> moves;
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
        bool is_draw(const chess::Board& board, bool nullSearch);
        int aspiration_window(chess::Board& board, int depth, int center,
            const std::vector<chess::move_t>& excluded, chess::move_t preferred);
        int pvs(chess::Board& board, int depth, int alpha, int beta, const NodeContext& child,
            bool firstMove, int reduction);
        int root_max(chess::Board& board, int depth, int alpha, int beta,
            const std::vector<chess::move_t>& excluded, chess::move_t preferred);
        int nega_max(chess::Board& board, int depth, int alpha, int beta, NodeContext node);
        std::optional<int> prob_cut(chess::Board& board, int depth, int beta, const NodeContext& node);
        int quiescence_search(chess::Board& board, int alpha, int beta, int ply, bool nullSearch);
        int quiet_score(const chess::Board& board, chess::move_t move, int ply) const;
        void update_pv(int ply, chess::move_t move);
        void reward_quiet(const chess::Board& board, chess::move_t move, int depth, int ply,
            const chess::MoveList& quietMoves);
        void advance_history(const chess::Board& board, int ply, chess::move_t move);
        int history_score(const chess::Board& board, chess::move_t move, int ply) const;
        std::size_t continuation_index(const chess::Board& board, chess::move_t move, int ply) const;
        std::size_t pawn_index(const chess::Board& board, chess::move_t move) const;
        int capture_score(const chess::Board& board, chess::move_t move) const;
        void reward_capture(const chess::Board& board, chess::move_t move, int depth, const chess::MoveList& captures);
        int corrected_score(const chess::Board& board, int score) const;
        void update_correction(const chess::Board& board, int depth, int score, int staticScore);
        uint64_t context_key(const NodeContext& node) const;
        bool is_repetition_draw(const chess::Board& board) const;
        HashProbe probe_hash(const chess::Board& board, uint64_t context, int ply);
        void store_hash(const chess::Board& board, uint64_t context, chess::move_t move,
            int score, int staticScore, int depth, int ply, int originalAlpha, int beta);
        int futility_margin(int depth, bool quiet, int quality) const;
        int late_reduction(int depth, int searched, int quality, bool pvNode, bool improving,
            int evaluationDelta, bool quiet) const;

        struct RootMove {
            chess::move_t move;
            int score = -Infinity;
            uint64_t nodes = 0;
        };
        std::vector<RootMove> m_rootMoves;
        int m_scoreVolatility = 0;
        TranspositionTable m_hash;
        Options m_options;
        Integrations m_integrations;
        Statistics m_statistics;
        std::array<int, MaxPly> m_staticScores {};
        std::array<std::array<int, chess::MaxMoves + 1>, MaxPly> m_reductions {};
        std::array<std::array<std::array<int, 64>, 64>, 2> m_history {};
        std::array<std::array<chess::move_t, 2>, MaxPly> m_killers {};
        std::vector<int> m_continuationHistory = std::vector<int>(ContinuationHistorySize);
        std::vector<int> m_pawnHistory = std::vector<int>(HistoryTableSize);
        std::array<std::array<std::array<int, 7>, 64>, 14> m_captureHistory {};
        std::array<std::array<chess::move_t, 64>, 14> m_counterMoves {};
        std::array<chess::move_t, MaxPly> m_previousMoves {};
        std::array<chess::piece_t, MaxPly> m_previousPieces {};
        std::array<std::vector<int>, 2> m_correctionHistory {
            std::vector<int>(HistoryTableSize), std::vector<int>(HistoryTableSize)};
        std::array<std::array<chess::move_t, MaxPly>, MaxPly> m_pv {};
        std::array<int, MaxPly> m_pvLength {};
        chess::MoveGenerator m_generator;
        Limits m_limits;
        std::stop_token m_stopToken;
        std::chrono::steady_clock::time_point m_start;
        uint64_t m_nodes = 0;
        int m_rootPly = 0;
        int m_selectiveDepth = 0;
        bool m_stopped = false;
};

}
