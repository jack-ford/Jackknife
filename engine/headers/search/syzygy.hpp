#pragma once

#include <memory>
#include <string>

#include "search/search.hpp"

namespace search {

class Syzygy final {
    public:
        explicit Syzygy(const std::string& path);
        unsigned max_pieces() const;
        std::optional<TablebaseOutcome> probe_wdl(const chess::Board& board) const;
        std::optional<TablebaseRoot> probe_root(const chess::Board& board) const;
        Integrations integrations() const;

    private:
        struct State;
        std::shared_ptr<State> m_state;
};

}
