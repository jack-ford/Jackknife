#include "search/search.hpp"

#include <algorithm>
#include <cmath>

namespace search {

int Search::aspiration_window(chess::Board& board, int depth, int center, const std::vector<chess::move_t>& excluded, chess::move_t preferred) {
    if (!m_options.aspirationWindows || depth < AspirationDepth || std::abs(center) >= MateThreshold)
        return root_max(board, depth, -Infinity, Infinity, excluded, preferred);

    int window = AspirationWindow + m_scoreVolatility;
    int alpha = std::max(-Infinity, center - window);
    int beta = std::min(Infinity, center + window);

    for (int retry = 0; ; retry++) {
        int score = root_max(board, depth, alpha, beta, excluded, preferred);

        if (m_stopped || (score > alpha && score < beta))
            return score;

        m_statistics.aspirationResearches++;
        if (retry >= AspirationRetries || std::abs(score) >= MateThreshold)
            return root_max(board, depth, -Infinity, Infinity, excluded, preferred);

        window *= 2;

        if (score <= alpha)
            alpha = std::max(-Infinity, score - window);
        else
            beta = std::min(Infinity, score + window);
    }
}

}
