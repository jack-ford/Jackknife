#include "search/transposition_table.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <stdexcept>

namespace search {

bool HashProbe::can_cutoff(int requiredDepth, int alpha, int beta) const {
    return contextMatch && depth >= requiredDepth &&
        (bound == Bound::Exact || (bound == Bound::Lower && score >= beta) ||
            (bound == Bound::Upper && score <= alpha));
}

TranspositionTable::TranspositionTable(std::size_t megabytes) {
    if (megabytes == 0 || megabytes > MaxHashMegabytes)
        throw std::invalid_argument("Hash size must be between 1 and 1024 MiB");

    m_clusters.resize(std::bit_floor(megabytes * BytesPerMegabyte / sizeof(Cluster)));
}

void TranspositionTable::clear() {
    std::fill(m_clusters.begin(), m_clusters.end(), Cluster {});
    m_generation = 0;
}

void TranspositionTable::new_search() {
    m_generation++;
}

std::size_t TranspositionTable::capacity() const {
    return m_clusters.size() * HashClusterSize;
}

int TranspositionTable::score_to_table(int score, int ply) {
    if (score >= MateThreshold)
        return score + ply;

    if (score <= -MateThreshold)
        return score - ply;

    return score;
}

int TranspositionTable::score_from_table(int score, int ply) {
    if (score >= MateThreshold)
        return score - ply;

    if (score <= -MateThreshold)
        return score + ply;

    return score;
}

HashProbe TranspositionTable::probe(uint64_t key, uint64_t context, int ply) const {
    const auto& cluster = m_clusters[key & (m_clusters.size() - 1)];
    HashProbe result;

    for (const Entry& entry : cluster.entries) {
        if (entry.bound == Bound::None || entry.key != key)
            continue;

        if (result.hit && entry.context != context && entry.depth <= result.depth)
            continue;

        result = {entry.move, score_from_table(entry.score, ply), entry.staticScore,
            entry.depth, entry.bound, true, entry.context == context, entry.wasPv};

        if (result.contextMatch)
            break;
    }
    return result;
}

int TranspositionTable::replacement_quality(const Entry& entry) const {
    int age = static_cast<uint8_t>(m_generation - entry.generation);
    return entry.depth + (entry.bound == Bound::Exact ? HashExactBonus : 0) - HashAgePenalty * age;
}

void TranspositionTable::store(uint64_t key, uint64_t context, chess::move_t move, int score,
    int staticScore, int depth, int ply, Bound bound, bool pvNode) {
    assert(bound != Bound::None && depth >= 0 && depth < MaxPly + MaxExtensions);
    assert(score > -Infinity && score < Infinity);
    auto& cluster = m_clusters[key & (m_clusters.size() - 1)];
    Entry* replacement = nullptr;

    for (Entry& entry : cluster.entries) {
        if (entry.bound != Bound::None && entry.key == key && entry.context == context) {
            pvNode = pvNode || entry.wasPv;

            if (entry.depth > depth || (entry.depth == depth && entry.bound == Bound::Exact && bound != Bound::Exact)) {
                entry.generation = m_generation;
                entry.wasPv = pvNode;
                if (entry.move == 0)
                    entry.move = move;

                return;
            }
            if (move == 0)
                move = entry.move;

            replacement = &entry;

            break;
        }
        if (replacement == nullptr || (replacement->bound != Bound::None &&
            (entry.bound == Bound::None || replacement_quality(entry) < replacement_quality(*replacement))))
            replacement = &entry;
    }
    
    *replacement = {key, context, static_cast<int16_t>(score_to_table(score, ply)),
        static_cast<int16_t>(staticScore), static_cast<int16_t>(depth), move, m_generation, bound, pvNode};
}

}
