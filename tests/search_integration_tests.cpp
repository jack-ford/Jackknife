#include "search_test_utils.hpp"

#include <iostream>
#include <memory>

namespace search_tests {

void check_exchange(const char* fen, move_t move, int expected) {
    Board board(fen);
    MoveGenerator generator;
    require(generator.is_legal(board, move), "illegal SEE fixture");
    require(eval::static_exchange(board, move) == expected, "incorrect static exchange score");
    require(board.current_fen() == fen, "SEE mutated board");
}

void test_exchange() {
    check_exchange("4k3/8/8/3q4/8/8/3R4/4K3 w - - 0 1", Move::create(11, 35), 900);
    check_exchange("7k/8/4p3/3p4/8/8/3Q4/K7 w - - 0 1", Move::create(11, 35), -800);
    check_exchange("4k3/8/4p3/3p4/8/8/3Q4/K3R3 w - - 0 1", Move::create(11, 35), 100);
    check_exchange("8/8/4k3/3p4/8/8/3R2B1/K7 w - - 0 1", Move::create(11, 35), 100);
    Board promotion("7k/P7/8/8/8/8/8/7K w - - 0 1");
    Board ep("3r3k/8/8/3pP3/8/8/8/K7 w - d6 0 1");
    MoveGenerator generator;
    bool sawPromotion = false;
    bool sawEp = false;
    for (move_t move : generator.generate_moves(promotion)) {
        if (Move::type(move) == Move::Type::Promotion) {
            require(eval::static_exchange(promotion, move) == eval::piece_value(Move::promotion_piece(move)) - 100,
                "promotion SEE incorrect");
            sawPromotion = true;
        }
    }
    for (move_t move : generator.generate_moves(ep)) {
        if (Move::type(move) == Move::Type::EnPassant) {
            require(eval::static_exchange(ep, move) == 0, "en passant x-ray SEE incorrect");
            sawEp = true;
        }
    }
    require(sawPromotion && sawEp, "missing special-move fixtures");
}

void test_integrations() {
    auto engine = std::make_unique<search::Search>(1);
    search::Limits limits;
    limits.time = std::chrono::milliseconds(0);
    limits.depth = 4;
    int calls = 0;
    search::Integrations integrations;
    integrations.probeTablebase = [&](const Board&) -> std::optional<search::TablebaseOutcome> {
        ++calls;
        return search::TablebaseOutcome::Draw;
    };
    engine->set_integrations(std::move(integrations));
    Board board;
    auto result = engine->iterative_deepening(board, limits);
    require(calls > 0 && result.statistics.tablebaseHits > 0, "tablebase adapter was not used");
    verify_pv(board, result);
    require(result.selectiveDepth > 0, "selective depth missing");
    require(result.ponderMove == (result.variations[0].moves.size() > 1 ? result.variations[0].moves[1] : 0),
        "ponder move differs from PV");
    engine->set_integrations({});
    limits.depth = 64;
    limits.softTime = std::chrono::milliseconds(1);
    result = engine->iterative_deepening(board, limits);
    require(result.depth > 0 && result.depth < limits.depth && !result.stopped, "soft budget did not stop between iterations");
    verify_pv(board, result);
}

}

int main() {
    try {
        search_tests::test_exchange();
        search_tests::test_integrations();
        std::cout << "Search integration tests passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
