#include "search/search.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace search {

using namespace chess;

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
}

void Search::advance_history(const Board& board, int ply) {
    m_historyKeys[ply + 1] = board.repetition_position_history().size() <= 1 ? 0 : m_historyKeys[ply];
    m_historyKeys[ply + 1] += mix_key(board.zobrist_key() + HistorySeed);
}

uint64_t Search::context_key(const Board& board, int ply, int extensions, square_t recaptureSquare, bool nullSearch, bool allowNull) const {
    uint64_t context = nullSearch ? NullContextSeed :
        m_historyKeys[ply] ^ mix_key(static_cast<uint64_t>(board.fifty_move_counter()) + HistorySeed);

    context ^= mix_key(static_cast<uint64_t>(extensions) * (Square::Count + 1) + recaptureSquare);
    return allowNull ? context : context ^ VerificationContextSeed;
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
        m_stopped = std::chrono::steady_clock::now() - m_start >= m_limits.time;

    return m_stopped;
}

bool Search::is_draw(const Board& board, bool nullSearch) const {
    return GameResultUtility::is_insufficient_material(board) || (!nullSearch &&
        (GameResultUtility::is_half_move_draw(board) || GameResultUtility::is_repetition(board, 3)));
}

void Search::update_pv(int ply, move_t move) {
    m_pv[ply][ply] = move;
    m_pvLength[ply] = m_pvLength[ply + 1];

    std::copy(m_pv[ply + 1].begin() + ply + 1,
        m_pv[ply + 1].begin() + m_pvLength[ply + 1], m_pv[ply].begin() + ply + 1);
}

int Search::root_max(Board& board, int depth, int alpha, int beta, const std::vector<move_t>& excluded, move_t preferred) {
    m_pvLength[0] = 0;

    MoveList moves = m_generator.generate_moves(board);
    order_moves(board, moves, preferred, 0);

    int bestScore = -Infinity;
    int searched = 0;

    int extension = m_options.checkExtensions && board.is_in_check() ? 1 : 0;
    m_statistics.checkExtensions += extension;

    int originalAlpha = alpha;

    move_t bestMove = 0;
    m_staticScores[0] = eval::evaluate(board);
    uint64_t context = context_key(board, 0, extension, Square::None, false);

    for (move_t move : moves) {
        if (std::find(excluded.begin(), excluded.end(), move) != excluded.end())
            continue;

        if (should_stop())
            return 0;

        bool capture = is_capture(board, move);
        board.make_move(move, true);

        advance_history(board, 0);
        square_t recaptureSquare = capture ? Move::to(move) : Square::None;

        int score = pvs(board, depth - 1 + extension, alpha, beta, 1,
            extension, recaptureSquare, true, false, true, searched == 0, 0);

        board.unmake_move(move, true);
        if (m_stopped)
            return 0;

        ++searched;

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
    if (limits.depth < 1 || limits.depth >= MaxPly || limits.multiPv < 1 || limits.time.count() < 0 || limits.softTime.count() < 0)
        throw std::invalid_argument("Invalid search limits");

    m_limits = limits;
    m_stopToken = stopToken;

    m_start = std::chrono::steady_clock::now();

    m_nodes = 0;
    m_selectiveDepth = 0;
    m_statistics = {};

    m_hash.new_search();
    m_staticScores.fill(Infinity);
    m_historyKeys[0] = 0;

    for (uint64_t key : board.repetition_position_history())
        m_historyKeys[0] += mix_key(key + HistorySeed);

    m_stopped = false;
    m_killers = {};
    m_pvLength = {};

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
    if (is_draw(board, false))
        return result;

    result.bestMove = moves[0];
    result.score = eval::evaluate(board);

    int count = std::min(limits.multiPv, moves.size());
    for (int depth = 1; depth <= limits.depth; depth++) {

        std::vector<Variation> variations;
        std::vector<move_t> excluded;
        for (int index = 0; index < count; index++) {
            int center = index < static_cast<int>(result.variations.size()) ? result.variations[index].score : result.score;
            move_t preferred = index < static_cast<int>(result.variations.size()) ? result.variations[index].moves[0] : probe_hash(board, context_key(board, 0, 0, Square::None, false), 0).move;

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
