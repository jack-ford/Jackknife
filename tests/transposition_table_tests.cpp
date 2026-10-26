#include "search/transposition_table.hpp"

#include <iostream>
#include <stdexcept>

namespace search_tests {

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

void test_table() {
    search::TranspositionTable oddSize(3);
    require(oddSize.allocated_bytes() == 3 * search::BytesPerMegabyte, "non-power-of-two allocation truncated");
    require(oddSize.capacity() == oddSize.allocated_bytes() / 16, "compact entry capacity");
    oddSize.store(0xffffffffffffffffULL, 0xabcdef1234567890ULL, 123, 42, 12, 8, 0, search::Bound::Exact);
    require(oddSize.probe(0xffffffffffffffffULL, 0xabcdef1234567890ULL, 0).contextMatch, "arbitrary-size indexing");
    search::TranspositionTable table(1);
    require(table.capacity() > 0, "empty table allocation");
    require(!table.probe(0, 0, 0).hit, "empty table matched zero key");
    table.store(0, 0, 123, 42, 12, 6, 0, search::Bound::Exact, true);
    auto probe = table.probe(0, 0, 0);
    require(probe.hit && probe.contextMatch && probe.move == 123 && probe.staticScore == 12, "entry fields not retained");
    require(probe.wasPv, "PV provenance not retained");
    require(probe.can_cutoff(6, -100, 100) && !probe.can_cutoff(7, -100, 100), "exact depth test failed");
    probe = table.probe(0, 1, 0);
    require(probe.hit && probe.move == 123 && !probe.can_cutoff(1, -100, 100), "incompatible history reused score");
    table.store(1, 0, 124, 100, 15, 5, 0, search::Bound::Lower);
    probe = table.probe(1, 0, 0);
    require(probe.can_cutoff(5, 0, 100) && !probe.can_cutoff(5, 0, 101), "lower bound cutoff incorrect");
    table.store(2, 0, 125, -100, -15, 5, 0, search::Bound::Upper);
    probe = table.probe(2, 0, 0);
    require(probe.can_cutoff(5, -100, 0) && !probe.can_cutoff(5, -101, 0), "upper bound cutoff incorrect");

    table.store(3, 0, 126, search::MateScore - 9, 0, 6, 4, search::Bound::Exact);
    require(table.probe(3, 0, 2).score == search::MateScore - 7, "winning mate was not normalized");
    table.store(4, 0, 127, -search::MateScore + 9, 0, 6, 4, search::Bound::Exact);
    require(table.probe(4, 0, 2).score == -search::MateScore + 7, "losing mate was not normalized");
    table.store(0, 0, 128, 200, 50, 2, 0, search::Bound::Lower);
    require(table.probe(0, 0, 0).score == 42, "shallow result overwrote deeper exact entry");
    table.store(0, 0, 0, 55, 12, 7, 0, search::Bound::Exact);
    require(table.probe(0, 0, 0).move == 123, "score-only update discarded hash move");
    require(table.probe(0, 0, 0).wasPv, "PV provenance lost during replacement");

    table.clear();
    uint64_t stride = table.capacity() / search::HashClusterSize;
    for (uint64_t index = 0; index < search::HashClusterSize; ++index)
        table.store(index * stride, 0, 123, 10, 0, static_cast<int>(index) + 1, 0, search::Bound::Exact);
    table.store(search::HashClusterSize * stride, 0, 123, 20, 0, 8, 0, search::Bound::Exact);
    require(!table.probe(0, 0, 0).hit, "cluster did not replace shallowest entry");
    for (uint64_t index = 1; index <= search::HashClusterSize; ++index)
        require(table.probe(index * stride, 0, 0).hit, "cluster collision corrupted another entry");
    table.new_search();
    table.store(stride, 0, 123, 10, 0, 2, 0, search::Bound::Exact);
    table.store((search::HashClusterSize + 1) * stride, 0, 123, 20, 0, 8, 0, search::Bound::Exact);
    require(table.probe(stride, 0, 0).hit && !table.probe(2 * stride, 0, 0).hit, "replacement did not account for generation age");
    for (int generation = 0; generation < 256; ++generation)
        table.new_search();
    require(table.probe(stride, 0, 0).hit, "generation wrap lost entry");
    table.clear();
    require(!table.probe(stride, 0, 0).hit, "clear retained entry");

    bool rejected = false;
    try {
        search::TranspositionTable invalid(0);
    }
    catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "invalid allocation accepted");
}

}

int main() {
    try {
        search_tests::test_table();
        std::cout << "Transposition table tests passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
