#include "search/controller.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace search {

TimeBudget allocate_time(const Clock& clock) {
    if (clock.remaining.count() < 0 || clock.increment.count() < 0 || clock.overhead.count() < 0 || clock.movesToGo < 0)
        throw std::invalid_argument("Invalid clock");
    using Duration = std::chrono::milliseconds;
    auto available = std::max(Duration {1}, clock.remaining - clock.overhead);
    int horizon = clock.movesToGo == 0 ? 30 : std::clamp(clock.movesToGo, 1, 100);
    // Cap before addition to avoid overflow with caller-supplied durations.
    auto base = available / horizon;
    auto increment = std::min(clock.increment, available) - std::min(clock.increment, available) / 4;
    auto soft = std::max(Duration {1}, base + std::min(increment, available - base));
    auto hard = soft + std::min(available - soft, soft);
    hard += std::min(available - hard, soft);
    return {soft, hard};
}

std::chrono::milliseconds stable_budget(TimeBudget budget, int stableIterations, int scoreDrop) {
    if (scoreDrop >= 50)
        return budget.soft + (budget.hard - budget.soft) / 2;
    if (stableIterations >= 4)
        return std::max(std::chrono::milliseconds {1}, budget.soft / 2);
    if (stableIterations >= 2)
        return std::max(std::chrono::milliseconds {1}, budget.soft - budget.soft / 4);
    return budget.soft;
}

Controller::Controller(std::size_t hashMegabytes, const Options& options): m_search(hashMegabytes, options) {}

void Controller::set_integrations(Integrations integrations) {
    m_search.set_integrations(std::move(integrations));
}

void Controller::clear() { m_search.clear(); }

Result Controller::search(chess::Board board, const Clock& clock, Limits limits,
    std::stop_token stopToken, const std::function<void(const Result&)>& onIteration) {
    limits.validate();
    TimeBudget budget = allocate_time(clock);
    limits.time = budget.hard;
    limits.softTime = std::chrono::milliseconds {0};
    chess::MoveGenerator generator;
    if (generator.generate_moves(board).size() == 1)
        limits.depth = std::min(limits.depth, 1);

    std::stop_source stopSource;
    std::stop_callback forwardStop(stopToken, [&] { stopSource.request_stop(); });
    chess::move_t previousMove = 0;
    std::vector<chess::move_t> previousPv;
    int previousScore = 0;
    int stableIterations = 0;
    bool softStop = false;
    Result result = m_search.iterative_deepening(std::move(board), limits, stopSource.get_token(), [&](const Result& iteration) {
        const auto& pv = iteration.variations.front().moves;
        std::size_t sharedLength = std::min<std::size_t>({3, pv.size(), previousPv.size()});
        bool stable = sharedLength != 0 && std::equal(pv.begin(), pv.begin() + sharedLength, previousPv.begin());
        stableIterations = stable ? stableIterations + 1 : 0;
        previousPv = pv;
        int scoreDrop = previousMove == 0 ? 0 : previousScore - iteration.score;
        previousMove = iteration.bestMove;
        previousScore = iteration.score;
        if (onIteration)
            onIteration(iteration);
        if (iteration.elapsed >= stable_budget(budget, stableIterations, scoreDrop)) {
            softStop = true;
            stopSource.request_stop();
        }
    });
    if (softStop && !stopToken.stop_requested() && result.elapsed < budget.hard &&
        (limits.nodes == 0 || result.nodes < limits.nodes))
        result.stopped = false;
    return result;
}

}
