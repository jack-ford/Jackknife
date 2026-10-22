#include "movegen/movegen.hpp"
#include "utils/fen_utility.hpp"

#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace chess;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static uint64_t perft(Board& board, int depth) {
    MoveGenerator generator;
    MoveList moves = generator.generate_moves(board);
    if (depth == 1) return moves.size();
    uint64_t nodes = 0;
    for (move_t move : moves) {
        board.make_move(move, true);
        nodes += perft(board, depth - 1);
        board.unmake_move(move, true);
    }
    return nodes;
}

static void check_perft(std::string_view fen, int depth, uint64_t expected) {
    Board board(fen);
    uint64_t originalKey = board.zobrist_key();
    std::string originalFen = board.current_fen();
    uint64_t actual = perft(board, depth);
    if (actual != expected) {
        std::cerr << fen << " depth " << depth << ": expected " << expected << ", got " << actual << '\n';
        throw std::runtime_error("Perft mismatch");
    }
    require(board.zobrist_key() == originalKey && board.current_fen() == originalFen, "perft modified board");
}

int main() {
    try {
        const auto expectResult = [](const Board& board, GameResult result, GameResultReason reason) {
            require(board.isGameOver() == std::pair{result, reason}, "game result mismatch");
        };
        Board ongoing;
        expectResult(ongoing, GameResult::None, GameResultReason::None);
        Board checkOnly("4k3/8/8/8/8/8/4r3/4K3 w - - 0 1");
        expectResult(checkOnly, GameResult::None, GameResultReason::None);
        Board checkmate("7k/6Q1/6K1/8/8/8/8/8 b - - 100 1");
        expectResult(checkmate, GameResult::Lose, GameResultReason::Checkmate);
        Board drawnStalemate("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
        expectResult(drawnStalemate, GameResult::Draw, GameResultReason::Stalemate);
        Board bareKings("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
        expectResult(bareKings, GameResult::Draw, GameResultReason::Insufficient_Material);
        Board belowLimit("4k3/8/8/8/8/8/8/R3K3 w - - 99 1");
        expectResult(belowLimit, GameResult::None, GameResultReason::None);
        belowLimit.make_move(Move::create(Square::A1, Square::A2));
        expectResult(belowLimit, GameResult::Draw, GameResultReason::Fifty_Move_Rule);
        for (int cycle = 0; cycle < 2; ++cycle) {
            ongoing.make_move(Move::create(Square::G1, Square::F3));
            ongoing.make_move(Move::create(Square::G8, Square::F6));
            ongoing.make_move(Move::create(Square::F3, Square::G1));
            ongoing.make_move(Move::create(Square::F6, Square::G8));
            expectResult(ongoing, cycle == 0 ? GameResult::None : GameResult::Draw,
                cycle == 0 ? GameResultReason::None : GameResultReason::Threefold_Repetition);
        }
        MoveGenerator resultGenerator;
        require(GameResultUtility::isMate(resultGenerator.generate_moves(checkmate), checkmate.is_in_check()), "mate helper");
        require(!GameResultUtility::isMate(resultGenerator.generate_moves(drawnStalemate), drawnStalemate.is_in_check()), "stalemate is not mate");
        check_perft(FenUtility::startPositionFen, 4, 197281);
        check_perft("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862);
        check_perft("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238);
        check_perft("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467);
        check_perft("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379);

        MoveGenerator generator;
        for (const char* fen : {
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
            "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
            "1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1"
        }) {
            Board board(fen);
            auto all = generator.generate_moves(board);
            auto captures = generator.generate_moves(board, MoveGenerator::GenerationMode::CapturesOnly);
            auto quiet = generator.generate_moves(board, MoveGenerator::GenerationMode::QuietOnly);
            std::set<move_t> combined(captures.begin(), captures.end());
            for (move_t move : quiet)
                require(combined.insert(move).second, "capture and quiet sets overlap");
            require(combined == std::set<move_t>(all.begin(), all.end()), "move modes do not partition moves");
            require(combined.size() == static_cast<std::size_t>(all.size()), "duplicate move");
            require(generator.has_legal_moves(board), "early exit missed legal move");
            for (move_t move : all)
                require(generator.is_legal(board, move), "legality disagrees with generation");
            for (move_t move : captures)
                require(Move::type(move) == Move::Type::EnPassant ||
                    !Piece::isEmpty(board.get_square(Move::to(move))), "quiet move in captures");
            auto knights = generator.generate_moves(board, MoveGenerator::GenerationMode::All, MoveGenerator::Knight);
            for (move_t move : knights)
                require(Piece::type(board.get_square(Move::from(move))) == PieceType::Knight, "piece filter");
        }

        Board start;
        require(start.king_square(PieceColor::White) == Square::E1, "default white king");
        require(start.king_square(PieceColor::Black) == Square::E8, "default black king");
        require(start.current_fen() == FenUtility::startPositionFen, "default starting position");
        require(generator.generate_moves(start).size() == 20, "default opening moves");
        require(!generator.is_legal(start, Move::create(Square::E2, Square::E5)), "illegal pawn leap");
        require(!generator.is_legal(start, Move::create<Move::Type::Promotion>(Square::G1, Square::F3)),
            "nonpawn promotion accepted");
        require(generator.generate_moves(start, true).is_empty(), "initial capture moves");

        Board mate("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
        require(!generator.has_legal_moves(mate) && generator.in_check(), "mate");
        Board stalemate("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
        require(!generator.has_legal_moves(stalemate) && !generator.in_check(), "stalemate");
        Board doubleCheck("4r1k1/8/8/8/1b6/8/8/R3K3 w - - 0 1");
        auto evasions = generator.generate_moves(doubleCheck);
        require(generator.in_double_check(), "double check detection");
        for (move_t move : evasions)
            require(Move::from(move) == Square::E1, "nonking double-check evasion");

        Board promotions("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
        generator.set_promotion_mode(MoveGenerator::PromotionMode::QueenOnly);
        require(generator.generate_moves(promotions, MoveGenerator::GenerationMode::All, MoveGenerator::Pawn).size() == 1,
            "queen-only promotions");
        require(generator.is_legal(promotions, Move::create<Move::Type::Promotion>(
            Square::A7, Square::A8, PieceType::Knight)), "legality depended on promotion setting");
        generator.set_promotion_mode(MoveGenerator::PromotionMode::QueenAndKnight);
        require(generator.generate_moves(promotions, MoveGenerator::GenerationMode::All, MoveGenerator::Pawn).size() == 2,
            "queen-and-knight promotions");

        Board pinnedEp("4k3/5p2/8/r5PK/8/8/8/8 b - - 0 1");
        pinnedEp.make_move(Move::create(Square::F7, Square::F5));
        require(!generator.is_en_passant_square_valid(pinnedEp, Square::F6), "pinned EP allowed");
        Board validEp("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
        require(generator.is_en_passant_square_valid(validEp, Square::D6), "valid EP rejected");
        require(!generator.is_en_passant_square_valid(validEp, Square::None), "invalid EP square accepted");

        require(MoveGenerator::squares_between(Square::A1, Square::D1) ==
            (Square::bitboard(Square::B1) | Square::bitboard(Square::C1) | Square::bitboard(Square::D1)),
            "between-square mask");
        std::cout << "Move generation tests and five perft positions passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
