#include "search/transposition_table.hpp"

#include <algorithm>
#include <bit>
#include "utils/search_utility.hpp"
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

    m_clusters.resize(megabytes * BytesPerMegabyte / sizeof(Cluster));
    if (std::has_single_bit(m_clusters.size()))
        m_indexMask = m_clusters.size() - 1;
}

std::size_t TranspositionTable::cluster_index(uint64_t key) const {
    return m_indexMask != 0 ? key & m_indexMask : key % m_clusters.size();
}

void TranspositionTable::clear() {
    std::fill(m_clusters.begin(), m_clusters.end(), Cluster {});
    m_generation = 0;
}

void TranspositionTable::new_search() {
    m_generation = (m_generation + 1) & 31;
}

std::size_t TranspositionTable::capacity() const {
    return m_clusters.size() * HashClusterSize;
}

std::size_t TranspositionTable::allocated_bytes() const {
    return m_clusters.size() * sizeof(Cluster);
}

namespace {
uint32_t fingerprint(uint64_t key) {
    uint64_t mixed = mix_key(key);
    return static_cast<uint32_t>(mixed ^ (mixed >> 32));
}
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
    const auto& cluster = m_clusters[cluster_index(key)];
    HashProbe result;
    uint32_t positionTag = fingerprint(key);
    uint32_t contextTag = fingerprint(context);

    for (const Entry& entry : cluster.entries) {
        if (entry.bound() == Bound::None || entry.key != positionTag)
            continue;

        if (result.hit && entry.context != contextTag && entry.depth <= result.depth)
            continue;

        result = {entry.move, score_from_table(entry.score, ply), entry.staticScore,
            entry.depth, entry.bound(), true, entry.context == contextTag, entry.was_pv()};

        if (result.contextMatch)
            break;
    }
    return result;
}

int TranspositionTable::replacement_quality(const Entry& entry) const {
    int age = ((m_generation - entry.generation()) & 31);
    return entry.depth + (entry.bound() == Bound::Exact ? HashExactBonus : 0) - HashAgePenalty * age;
}

void TranspositionTable::store(uint64_t key, uint64_t context, chess::move_t move, int score,
    int staticScore, int depth, int ply, Bound bound, bool pvNode) {
    assert(bound != Bound::None && depth >= 0 && depth < MaxPly + MaxExtensions);
    assert(score > -Infinity && score < Infinity);
    auto& cluster = m_clusters[cluster_index(key)];
    Entry* replacement = nullptr;
    uint32_t positionTag = fingerprint(key);
    uint32_t contextTag = fingerprint(context);

    for (Entry& entry : cluster.entries) {
        if (entry.bound() != Bound::None && entry.key == positionTag && entry.context == contextTag) {
            pvNode = pvNode || entry.was_pv();

            if (entry.depth > depth || (entry.depth == depth && entry.bound() == Bound::Exact && bound != Bound::Exact)) {
                entry.refresh(m_generation, pvNode);
                if (entry.move == 0)
                    entry.move = move;

                return;
            }
            if (move == 0)
                move = entry.move;

            replacement = &entry;

            break;
        }
        if (replacement == nullptr || (replacement->bound() != Bound::None &&
            (entry.bound() == Bound::None || replacement_quality(entry) < replacement_quality(*replacement))))
            replacement = &entry;
    }
    
    *replacement = {positionTag, contextTag, static_cast<int16_t>(score_to_table(score, ply)),
        static_cast<int16_t>(staticScore), move, static_cast<uint8_t>(depth), static_cast<uint8_t>(bound)};
    replacement->refresh(m_generation, pvNode);
}

}
