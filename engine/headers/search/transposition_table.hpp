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
        std::size_t allocated_bytes() const;
        static int score_to_table(int score, int ply);
        static int score_from_table(int score, int ply);

    private:
        struct Entry {
            uint32_t key = 0;
            uint32_t context = 0;
            int16_t score = 0;
            int16_t staticScore = 0;
            chess::move_t move = 0;
            uint8_t depth = 0;
            uint8_t metadata = 0;

            Bound bound() const { return static_cast<Bound>(metadata & 3); }
            bool was_pv() const { return (metadata & 4) != 0; }
            uint8_t generation() const { return metadata >> 3; }
            void refresh(uint8_t generation, bool pvNode) {
                metadata = static_cast<uint8_t>((generation << 3) | (pvNode ? 4 : 0) | static_cast<uint8_t>(bound()));
            }
        };

        struct alignas(CacheLineSize) Cluster {
            std::array<Entry, HashClusterSize> entries;
        };

        static_assert(sizeof(Entry) == 16);
        static_assert(sizeof(Cluster) == CacheLineSize);

        std::size_t cluster_index(uint64_t key) const;
        std::size_t m_indexMask = 0;
        int replacement_quality(const Entry& entry) const;
        std::vector<Cluster> m_clusters;
        uint8_t m_generation = 0;
};

}
