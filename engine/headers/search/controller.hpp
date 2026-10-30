#pragma once

#include "search/search.hpp"

namespace search {

struct Clock {
    std::chrono::milliseconds remaining {0};
    std::chrono::milliseconds increment {0};
    std::chrono::milliseconds overhead {20};
    int movesToGo = 0;
};

struct TimeBudget {
    std::chrono::milliseconds soft {0};
    std::chrono::milliseconds hard {0};
};

TimeBudget allocate_time(const Clock& clock);
std::chrono::milliseconds stable_budget(TimeBudget budget, int stableIterations, int scoreDrop);

class Controller final {
    public:
        explicit Controller(std::size_t hashMegabytes = DefaultHashMegabytes, const Options& options = {});
        Result search(chess::Board board, const Clock& clock, Limits limits = {},
            std::stop_token stopToken = {}, const std::function<void(const Result&)>& onIteration = {});
        void set_integrations(Integrations integrations);
        void clear();

    private:
        Search m_search;
};

}
