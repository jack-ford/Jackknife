#pragma once

#include <memory>
#include <string>

#include "search/search.hpp"

namespace search {

// Fathom owns process-wide mappings. Instances may share the same path; a
// different path requires releasing all instances and their integrations first.
// Mappings stay cached until the path changes or the process exits.
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
