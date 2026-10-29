#include "search/search.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace search {

using namespace chess;

void Limits::validate() const {
    if (depth < 1 || depth >= MaxPly || multiPv < 1 || time.count() < 0 || softTime.count() < 0)
        throw std::invalid_argument("Invalid search limits");
}

Search::Search(std::size_t hashMegabytes, const Options& options): m_hash(hashMegabytes), m_options(options) {
    for (int depth = 1; depth < MaxPly; ++depth) {
        for (int move = 1; move <= MaxMoves; ++move) {
            m_reductions[depth][move] = static_cast<int>(
                LateReductionBase +
                std::log(static_cast<double>(depth)) * 
                std::log(static_cast<double>(move)) / LateReductionDivisor);
        }
    }
}

void Search::set_integrations(Integrations integrations) {
    m_integrations = std::move(integrations);
    m_hash.clear();
}

void Search::clear() {
    m_hash.clear();
    m_history = {};
    m_killers = {};
    std::fill(m_continuationHistory.begin(), m_continuationHistory.end(), 0);
    std::fill(m_pawnHistory.begin(), m_pawnHistory.end(), 0);
    m_captureHistory = {};
    m_counterMoves = {};
    for (auto& color : m_correctionHistory)
        std::fill(color.begin(), color.end(), 0);
}

void Search::advance_history(const Board& board, int ply, move_t move) {
    m_previousMoves[ply + 1] = move;
    m_previousPieces[ply + 1] = board.get_square(Move::to(move));
}

uint64_t Search::context_key(const NodeContext& node) const {
    // Entries are shared across paths; only searches that exclude a move need their own
    return node.excludedMove == 0 ? 0 : mix_key(node.excludedMove + ExcludedMoveSeed);
}

bool Search::is_repetition_draw(const Board& board) const {
    // History entries are consecutive plies since the last irreversible or null move.
    auto history = board.repetition_position_history();
    int last = static_cast<int>(history.size()) - 1;
    int occurrences = 0;
    for (int distance = 2; distance <= last; distance += 2) {
        if (history[last - distance] != board.zobrist_key())
            continue;
        // One repetition inside the search tree is enough; game history needs threefold.
        if (board.ply_count() - distance > m_rootPly || ++occurrences == 2)
            return true;
    }
    return false;
}

HashProbe Search::probe_hash(const Board& board, uint64_t context, int ply) {
    if (!m_options.transpositionTable)
        return {};

    HashProbe probe = m_hash.probe(board.zobrist_key(), context, ply);
    m_statistics.hashHits += probe.hit;

    return probe;
}

void Search::store_hash(const Board& board, uint64_t context, move_t move, int score,
    int staticScore, int depth, int ply, int originalAlpha, int beta) {
    if (!m_options.transpositionTable || m_stopped)
        return;

    Bound bound = score >= beta ? Bound::Lower : score <= originalAlpha ? Bound::Upper : Bound::Exact;
    m_hash.store(board.zobrist_key(), context, move, score, staticScore, depth, ply, bound, beta - originalAlpha > 1);
}

bool Search::should_stop() {
    if (m_stopped)
        return true;

    m_stopped = m_stopToken.stop_requested() || (m_limits.nodes != 0 && m_nodes >= m_limits.nodes);

    if (!m_stopped && m_limits.time.count() > 0 && (m_nodes & ClockCheckMask) == 0)
        m_stopped = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_start) >= m_limits.time;

    return m_stopped;
}

bool Search::is_draw(const Board& board, bool nullSearch) {
    if (GameResultUtility::is_insufficient_material(board))
        return true;
    if (nullSearch)
        return false;
    if (is_repetition_draw(board))
        return true;
    // Checkmate delivered on the move that reaches the fifty-move limit takes precedence.
    return GameResultUtility::is_half_move_draw(board) &&
        (!board.is_in_check() || m_generator.has_legal_moves(board));
}

void Search::update_pv(int ply, move_t move) {
    m_pv[ply][ply] = move;
    m_pvLength[ply] = m_pvLength[ply + 1];

    std::copy(m_pv[ply + 1].begin() + ply + 1,
        m_pv[ply + 1].begin() + m_pvLength[ply + 1], m_pv[ply].begin() + ply + 1);
}

int Search::root_max(Board& board, int depth, int alpha, int beta, const std::vector<move_t>& excluded, move_t preferred) {
    m_pvLength[0] = 0;

    std::stable_sort(m_rootMoves.begin(), m_rootMoves.end(), [preferred](const RootMove& left, const RootMove& right) {
        if ((left.move == preferred) != (right.move == preferred))
            return left.move == preferred;
        return left.score != right.score ? left.score > right.score : left.nodes > right.nodes;
    });

    int bestScore = -Infinity;
    int searched = 0;

    int extension = m_options.checkExtensions && board.is_in_check() ? 1 : 0;
    m_statistics.checkExtensions += extension;

    int originalAlpha = alpha;

    move_t bestMove = 0;
    m_staticScores[0] = eval::evaluate(board);
    uint64_t context = context_key({});

    for (auto& rootMove : m_rootMoves) {
        move_t move = rootMove.move;
        uint64_t startNodes = m_nodes;
        if (std::find(excluded.begin(), excluded.end(), move) != excluded.end())
            continue;

        if (should_stop())
            return 0;

        bool capture = is_capture(board, move);
        board.make_move(move, true);

        advance_history(board, 0, move);
        square_t recaptureSquare = capture ? Move::to(move) : Square::None;

        NodeContext child {.ply = 1, .extensions = extension, .recaptureSquare = recaptureSquare, .pvNode = true};
        int score = pvs(board, depth - 1 + extension, alpha, beta, child, searched == 0, 0);

        board.unmake_move(move, true);
        if (m_stopped)
            return 0;

        ++searched;
        rootMove.score = score;
        rootMove.nodes = m_nodes - startNodes;

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
            update_pv(0, move);
        }

        alpha = std::max(alpha, score);
        if (score >= beta)
            break;
    }

    if (excluded.empty())
        store_hash(board, context, bestMove, bestScore, m_staticScores[0], depth + extension, 0, originalAlpha, beta);

    return bestScore;
}

Result Search::iterative_deepening(Board board, const Limits& limits, std::stop_token stopToken, const std::function<void(const Result&)>& onIteration) {
    limits.validate();

    m_limits = limits;
    m_stopToken = stopToken;

    m_start = std::chrono::steady_clock::now();

    m_nodes = 0;
    m_selectiveDepth = 0;
    m_statistics = {};

    m_hash.new_search();
    m_staticScores.fill(Infinity);
    m_rootPly = board.ply_count();

    m_stopped = false;
    m_killers = {};
    m_pvLength = {};
    m_previousMoves = {};
    m_previousPieces = {};
    for (int& value : m_continuationHistory) value /= 2;
    for (int& value : m_pawnHistory) value /= 2;
    for (auto& piece : m_captureHistory)
        for (auto& target : piece)
            for (int& value : target) value /= 2;

    for (auto& color : m_history)
        for (auto& from : color)
            for (int& value : from)
                value /= 2;

    Result result;
    MoveList moves = m_generator.generate_moves(board);

    if (moves.is_empty()) {
        result.score = board.is_in_check() ? -MateScore : 0;
        return result;
    }

    if (!limits.searchMoves.empty()) {
        MoveList restricted;
        for (move_t move : moves)
            if (std::find(limits.searchMoves.begin(), limits.searchMoves.end(), move) != limits.searchMoves.end())
                restricted.add(move);
        if (restricted.is_empty())
            throw std::invalid_argument("Search move list contains no legal root move");
        moves = restricted;
    }
    if (is_draw(board, false))
        return result;

    result.bestMove = moves[0];
    result.score = eval::evaluate(board);

    if (limits.multiPv == 1 && !should_stop() && m_integrations.probeRoot) {
        auto root = m_integrations.probeRoot(board);
        if (root && !should_stop() && std::find(moves.begin(), moves.end(), root->move) != moves.end()) {
            result.bestMove = root->move;
            result.score = root->outcome == TablebaseOutcome::Win ? TablebaseScore :
                root->outcome == TablebaseOutcome::Loss ? -TablebaseScore : 0;
            result.variations = {{result.score, {root->move}}};
            result.statistics.tablebaseHits = 1;
            result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_start);
            if (onIteration)
                onIteration(result);
            return result;
        }
    }

    if (limits.multiPv == 1 && !should_stop() && m_integrations.probeBook) {
        auto move = m_integrations.probeBook(board);
        if (move && !should_stop() && std::find(moves.begin(), moves.end(), *move) != moves.end()) {
            result.bestMove = *move;
            result.variations = {{result.score, {*move}}};
            result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_start);
            if (onIteration)
                onIteration(result);
            return result;
        }
    }

    m_rootMoves.clear();
    for (move_t move : moves)
        m_rootMoves.push_back({move});
    m_scoreVolatility = 0;
    int count = std::min(limits.multiPv, moves.size());
    for (int depth = 1; depth <= limits.depth; depth++) {

        std::vector<Variation> variations;
        std::vector<move_t> excluded;
        for (int index = 0; index < count; index++) {
            int center = index < static_cast<int>(result.variations.size()) ? result.variations[index].score : result.score;
            move_t preferred = index < static_cast<int>(result.variations.size()) ? result.variations[index].moves[0] : probe_hash(board, context_key({}), 0).move;

            int score = aspiration_window(board, depth, center, excluded, preferred);
            if (m_stopped)
                break;

            Variation variation;
            variation.score = score;
            variation.moves.assign(m_pv[0].begin(), m_pv[0].begin() + m_pvLength[0]);
            excluded.push_back(variation.moves[0]);
            variations.push_back(std::move(variation));
        }

        if (m_stopped)
            break;

        std::stable_sort(variations.begin(), variations.end(), [](const Variation& left, const Variation& right) {
            return left.score > right.score;
        });

        m_scoreVolatility = std::min(MaxScoreVolatility, (m_scoreVolatility + std::abs(variations[0].score - result.score)) / 2);
        result.variations = std::move(variations);
        result.bestMove = result.variations[0].moves[0];
        result.score = result.variations[0].score;
        result.depth = depth;
        result.selectiveDepth = m_selectiveDepth;
        result.ponderMove = result.variations[0].moves.size() > 1 ? result.variations[0].moves[1] : 0;
        result.nodes = m_nodes;
        result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_start);
        result.statistics = m_statistics;

        if (onIteration)
            onIteration(result);

        if (should_stop() || (m_limits.softTime.count() > 0 && result.elapsed >= m_limits.softTime))
            break;
    }

    result.nodes = m_nodes;
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_start);
    result.stopped = m_stopped;
    result.statistics = m_statistics;
    
    return result;
}

}
