#pragma once

#include <array>
#include <vector>

#include "core/move.hpp"
#include "search/constants.hpp"

namespace search {

enum class Bound : uint8_t { None, Exact, Lower, Upper };

struct HashProbe {
    chess::move_t move = 0;
    int score = 0;
    int staticScore = 0;
    int depth = -1;
    Bound bound = Bound::None;
    bool hit = false;
    bool contextMatch = false;
    bool wasPv = false;

    bool can_cutoff(int requiredDepth, int alpha, int beta) const;
};

class TranspositionTable final {
    public:
        explicit TranspositionTable(std::size_t megabytes = DefaultHashMegabytes);
        void clear();
        void new_search();
        HashProbe probe(uint64_t key, uint64_t context, int ply) const;
        void store(uint64_t key, uint64_t context, chess::move_t move, int score,
            int staticScore, int depth, int ply, Bound bound, bool pvNode = false);
        std::size_t capacity() const;
        static int score_to_table(int score, int ply);
        static int score_from_table(int score, int ply);

    private:
        struct Entry {
            uint64_t key = 0;
            uint64_t context = 0;
            int16_t score = 0;
            int16_t staticScore = 0;
            int16_t depth = -1;
            chess::move_t move = 0;
            uint8_t generation = 0;
            Bound bound = Bound::None;
            bool wasPv = false;
        };

        struct alignas(CacheLineSize) Cluster {
            std::array<Entry, HashClusterSize> entries;
        };

        int replacement_quality(const Entry& entry) const;
        std::vector<Cluster> m_clusters;
        uint8_t m_generation = 0;
};

}
