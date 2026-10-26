#include "search_test_utils.hpp"
#include "search/controller.hpp"
#include "search/syzygy.hpp"
#include "search/opening_book.hpp"
#include "utils/move_utility.hpp"
#include <sstream>

#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <set>

namespace search_tests {
using namespace std::chrono_literals;

int reference_recaptures(Board& board, square_t target) {
    MoveGenerator generator;
    auto moves = generator.generate_moves(board, MoveGenerator::GenerationMode::CapturesOnly);
    move_t selected = 0;
    int bestRank = 100;
    for (move_t move : moves) {
        if (Move::to(move) != target || (Move::type(move) == Move::Type::Promotion &&
            Move::promotion_piece(move) != PieceType::Queen)) continue;
        PieceType piece = Piece::type(board.get_square(Move::from(move)));
        int rank = piece == PieceType::King ? 6 : static_cast<int>(piece) - 2;
        if (rank < bestRank || (rank == bestRank && Move::from(move) < Move::from(selected))) {
            selected = move;
            bestRank = rank;
        }
    }
    if (selected == 0) return 0;
    int gain = eval::piece_value(Piece::type(board.get_square(target)));
    if (Move::type(selected) == Move::Type::Promotion)
        gain += eval::piece_value(Move::promotion_piece(selected)) - 100;
    board.make_move(selected, true);
    int score = std::max(0, gain - reference_recaptures(board, target));
    board.unmake_move(selected, true);
    return score;
}

int reference_exchange(Board board, move_t move) {
    int gain = Move::type(move) == Move::Type::EnPassant ? 100 : eval::piece_value(Piece::type(board.get_square(Move::to(move))));
    if (Move::type(move) == Move::Type::Promotion)
        gain += eval::piece_value(Move::promotion_piece(move)) - 100;
    board.make_move(move, true);
    return gain - reference_recaptures(board, Move::to(move));
}

void test_book() {
    Board board;
    search::OpeningBook book;
    std::istringstream input(std::string(FenUtility::StartPositionFen) + " | e2e4 | 3\n" +
        std::string(FenUtility::StartPositionFen) + " | d2d4 | 1\n");
    book.load(input);
    require(book.size() == 1, "book position count");
    require(book.probe(board, 0) == parse_coordinate_move(board, "e2e4"), "book first weight");
    require(book.probe(board, 2) == parse_coordinate_move(board, "e2e4"), "book weight boundary");
    require(book.probe(board, 3) == parse_coordinate_move(board, "d2d4"), "book second weight");
    require(book.probe(board, 4) == book.probe(board, 0), "book choice wrap");
    for (const char* record : {"e2e5 | 1", "e2e4 | 0", "e2e4 | -1", "e2e4 | 1junk", "e2e4 | "}) {
        std::istringstream invalid(std::string(FenUtility::StartPositionFen) + " | " + record);
        bool rejected = false;
        try { book.load(invalid); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected && book.size() == 1 && book.probe(board).has_value(), "book reload not transactional");
    }
    auto engine = std::make_unique<search::Search>(1);
    search::Integrations integrations;
    integrations.probeBook = [&book](const Board& position) { return book.probe(position); };
    engine->set_integrations(integrations);
    auto result = engine->iterative_deepening(board);
    require(result.nodes == 0 && result.bestMove == *book.probe(board), "book search integration");
    verify_pv(board, result);
    board.make_move(result.bestMove);
    require(!book.probe(board), "book miss");
}

void test_search_features() {
    search::Limits limits;
    limits.time = 0ms;
    limits.depth = 7;
    limits.nodes = 300000;
    search::Options options;
    options.continuationHistory = true;
    options.counterMoves = true;
    options.captureHistory = true;
    options.pawnHistory = true;
    options.singularExtensions = true;
    options.probCut = true;
    options.razoring = true;
    options.correctionHistory = true;
    auto engine = std::make_unique<search::Search>(1, options);
    search::Statistics totals;
    for (const char* fen : {
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "4k3/8/8/3q4/8/8/3R4/4K3 w - - 0 1",
        "6k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1"}) {
        Board board(fen);
        engine->clear();
        auto result = engine->iterative_deepening(board, limits);
        verify_pv(board, result);
        require(result.depth > 0 && result.nodes <= limits.nodes, "experimental search lost completed result or node limit");
        totals.singularExtensions += result.statistics.singularExtensions;
        totals.probCutCutoffs += result.statistics.probCutCutoffs;
        totals.razorCutoffs += result.statistics.razorCutoffs;
        totals.correctionUpdates += result.statistics.correctionUpdates;
        totals.continuationUpdates += result.statistics.continuationUpdates;
        totals.counterMoveUpdates += result.statistics.counterMoveUpdates;
        totals.captureHistoryUpdates += result.statistics.captureHistoryUpdates;
        totals.pawnHistoryUpdates += result.statistics.pawnHistoryUpdates;
    }
    require(totals.continuationUpdates > 0 && totals.counterMoveUpdates > 0 &&
        totals.captureHistoryUpdates > 0 && totals.pawnHistoryUpdates > 0, "history feature not exercised");
    require(totals.correctionUpdates > 0, "correction history not exercised");
    require(totals.singularExtensions > 0, "singular extension not exercised");
    require(totals.probCutCutoffs > 0, "ProbCut not exercised");
    require(totals.razorCutoffs > 0, "razoring not exercised");
    for (auto feature : {&search::Options::singularExtensions, &search::Options::probCut,
        &search::Options::razoring, &search::Options::correctionHistory}) {
        search::Options isolated;
        isolated.*feature = true;
        auto isolatedEngine = std::make_unique<search::Search>(1, isolated);
        search::Limits tacticalLimits;
        tacticalLimits.time = 0ms;
        tacticalLimits.depth = 4;
        Board mate("6k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1");
        auto tacticalResult = isolatedEngine->iterative_deepening(mate, tacticalLimits);
        require(tacticalResult.bestMove == *parse_coordinate_move(mate, "e1e8") &&
            tacticalResult.score >= search::MateThreshold, "isolated feature lost forced mate");
        verify_pv(mate, tacticalResult);
    }
    options = {};
    options.continuationHistory = false;
    options.counterMoves = false;
    options.captureHistory = false;
    options.pawnHistory = false;
    engine = std::make_unique<search::Search>(1, options);
    limits.depth = 4;
    auto result = engine->iterative_deepening(Board {}, limits);
    require(result.statistics.continuationUpdates == 0 && result.statistics.counterMoveUpdates == 0 &&
        result.statistics.captureHistoryUpdates == 0 && result.statistics.pawnHistoryUpdates == 0 &&
        result.statistics.correctionUpdates == 0 && result.statistics.singularExtensions == 0 &&
        result.statistics.probCutCutoffs == 0 && result.statistics.razorCutoffs == 0, "disabled feature ran");
    engine->clear();
    auto repeated = engine->iterative_deepening(Board {}, limits);
    require(result.bestMove == repeated.bestMove && result.score == repeated.score && result.nodes == repeated.nodes,
        "clear did not reset search state");
}

void test_clock() {
    auto budget = search::allocate_time({30000ms, 1000ms, 20ms, 0});
    require(budget.soft == 1749ms && budget.hard == 5247ms, "clock allocation");
    budget = search::allocate_time({100ms, 1000ms, 20ms, 1});
    require(budget.soft == 80ms && budget.hard == 80ms, "increment must not exceed available clock");
    budget = search::allocate_time({0ms, 0ms, 20ms, 0});
    require(budget.soft == 1ms && budget.hard == 1ms, "zero clock became unlimited");
    budget = search::allocate_time({std::chrono::milliseconds::max(), std::chrono::milliseconds::max(), 0ms, 1});
    require(budget.hard == std::chrono::milliseconds::max(), "clock overflow");
    require(search::stable_budget({100ms, 300ms}, 4, 0) == 50ms, "stable PV budget");
    require(search::stable_budget({100ms, 300ms}, 4, 100) == 200ms, "score drop budget");
    bool rejected = false;
    try { search::allocate_time({-1ms}); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "negative clock accepted");
    auto controller = std::make_unique<search::Controller>(1);
    Board forced("7k/8/5K2/5Q2/8/8/8/8 b - - 0 1");
    MoveGenerator generator;
    require(generator.generate_moves(forced).size() == 1, "forced move fixture");
    search::Limits limits;
    limits.depth = 8;
    auto result = controller->search(forced, {30000ms}, limits);
    require(result.depth == 1 && result.bestMove != 0, "forced move searched too long");
    verify_pv(forced, result);
    auto invalidLimits = limits;
    invalidLimits.depth = search::MaxPly;
    rejected = false;
    try { controller->search(forced, {30000ms}, invalidLimits); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "forced move bypassed limit validation");
    std::stop_source stop;
    stop.request_stop();
    result = controller->search(Board {}, {30000ms}, limits, stop.get_token());
    require(result.stopped && result.depth == 0, "controller lost external cancellation");
}

void check_picker(const Board& board, std::mt19937& random) {
    MoveGenerator generator;
    auto moves = generator.generate_moves(board);
    if (moves.is_empty())
        return;
    move_t hashMove = moves[random() % moves.size()];
    search::MovePicker picker(board, hashMove, [](move_t move) { return static_cast<int>(move % 31); });
    std::set<move_t> selected;
    auto first = picker.next();
    require(first && first->move == hashMove, "hash move not first");
    selected.insert(first->move);
    while (auto candidate = picker.next()) {
        require(selected.insert(candidate->move).second, "duplicate staged move");
        require(candidate->exchange == (is_tactical(board, candidate->move) ? eval::static_exchange(board, candidate->move) : 0),
            "staged SEE mismatch");
    }
    require(selected == std::set<move_t>(moves.begin(), moves.end()), "staged move coverage");
    auto tactical = generator.generate_moves(board, MoveGenerator::GenerationMode::Tactical);
    for (move_t move : tactical)
        require(eval::static_exchange(board, move) == reference_exchange(board, move), "SEE differs from legal-board reference");
    std::set<move_t> expected;
    for (move_t move : moves)
        if (is_tactical(board, move)) expected.insert(move);
    require(expected == std::set<move_t>(tactical.begin(), tactical.end()), "tactical generation mismatch");
    search::MovePicker tacticalPicker(board, 0, {}, true);
    selected.clear();
    while (auto candidate = tacticalPicker.next()) selected.insert(candidate->move);
    require(selected == expected, "tactical picker mismatch");
}

void test_differential_walks() {
    std::mt19937 random(731);
    MoveGenerator generator;
    for (std::string_view fen : std::initializer_list<std::string_view> {FenUtility::StartPositionFen,
        "1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1",
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"}) {
        Board board(fen);
        for (int ply = 0; ply < 160; ++ply) {
            check_picker(board, random);
            std::string savedFen = board.current_fen();
            uint64_t savedKey = board.zobrist_key();
            Board parsed(savedFen);
            require(parsed.zobrist_key() == savedKey && parsed.current_fen() == savedFen, "FEN round trip");
            auto moves = generator.generate_moves(board);
            if (moves.is_empty()) break;
            move_t move = moves[random() % moves.size()];
            auto accumulator = board.nnue_accumulator();
            board.make_move(move, true);
            board.unmake_move(move, true);
            require(board.current_fen() == savedFen && board.zobrist_key() == savedKey &&
                board.nnue_accumulator() == accumulator, "random undo failed");
            board.make_move(move, true);
        }
    }
}

void test_syzygy() {
    search::Syzygy backend(SYZYGY_TEST_PATH);
    require(backend.max_pieces() == 3, "real tablebase files not loaded");
    Board win("7k/8/5K2/5Q2/8/8/8/8 w - - 0 1");
    Board loss("7k/8/5K2/5Q2/8/8/8/8 b - - 0 1");
    require(backend.probe_wdl(win) == search::TablebaseOutcome::Win, "WDL win");
    require(backend.probe_wdl(loss) == search::TablebaseOutcome::Loss, "WDL loss");
    Board stale("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    require(backend.probe_wdl(stale) == search::TablebaseOutcome::Draw, "WDL stalemate");
    require(!backend.probe_wdl(Board {}), "unsupported material accepted");
    Board late("7k/8/5K2/5Q2/8/8/8/8 b - - 99 1");
    require(!backend.probe_wdl(late), "WDL ignored rule50");
    auto root = backend.probe_root(win);
    require(root && root->outcome == search::TablebaseOutcome::Win, "DTZ root probe");
    MoveGenerator generator;
    require(generator.is_legal(win, root->move), "DTZ move illegal");
    root = backend.probe_root(late);
    require(root && root->outcome == search::TablebaseOutcome::Draw, "DTZ ignored rule50");
    auto engine = std::make_unique<search::Search>(1);
    engine->set_integrations(backend.integrations());
    auto result = engine->iterative_deepening(win);
    require(result.statistics.tablebaseHits == 1 && result.bestMove != 0, "root integration missing");
    verify_pv(win, result);
    win.make_move(*parse_coordinate_move(win, "f5f4"));
    require(!backend.probe_root(win), "root probe ignored repetition history");
    search::Syzygy shared(SYZYGY_TEST_PATH);
    require(shared.max_pieces() == 3, "shared tablebase lifetime");
    bool rejected = false;
    try { search::Syzygy conflict("another-path"); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "conflicting tablebase mappings accepted");
}
}

int main() {
    try {
        search_tests::test_book();
        search_tests::test_search_features();
        search_tests::test_clock();
        search_tests::test_differential_walks();
        search_tests::test_syzygy();
        search::Syzygy missing("missing-syzygy-directory");
        search_tests::require(missing.max_pieces() == 0 && !missing.probe_wdl(chess::Board {}), "missing tables did not fall back");
        std::cout << "Search upgrade tests passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
