#include "core/board.hpp"
#include "utils/fen_utility.hpp"
#include "core/zobrist.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace chess;

static void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

static void verify(const Board& board) {
    std::array<bitboard_t, 14> pieceBoards {};
    std::array<bitboard_t, 2> colourBoards {};
    int materialCount = 0;
    uint64_t key = Zobrist::castling(board.current_game_state().castlingRights);
    if (board.is_white_to_move()) key ^= Zobrist::side_to_move();
    square_t ep = board.current_game_state().enPassantSquare;
    if (ep != Square::None) key ^= Zobrist::en_passant(Square::file(ep));
    for (int i = 0; i < 64; ++i) {
        square_t square = static_cast<square_t>(i);
        piece_t piece = board.get_square(square);
        if (Piece::isEmpty(piece)) continue;
        pieceBoards[piece] |= Square::bitboard(square);
        colourBoards[static_cast<int>(Piece::color(piece))] |= Square::bitboard(square);
        key ^= Zobrist::piece(piece, square);
        if (Piece::type(piece) != PieceType::King && Piece::type(piece) != PieceType::Pawn)
            ++materialCount;
        if (Piece::type(piece) == PieceType::King)
            require(board.king_square(Piece::color(piece)) == square, "king square mismatch");
    }
    require(pieceBoards == board.piece_bitboards(), "piece bitboards mismatch");
    require(colourBoards == board.colour_bitboards(), "colour bitboards mismatch");
    require((colourBoards[0] | colourBoards[1]) == board.all_pieces_bitboard(), "occupancy mismatch");
    require(key == board.zobrist_key(), "incremental hash mismatch");
    require(materialCount == board.total_piece_count_without_pawns_and_kings(), "material count mismatch");
    for (PieceColor color : { PieceColor::Black, PieceColor::White }) {
        std::array<std::span<const square_t>, 5> lists {
            board.pawns(color), board.knights(color), board.bishops(color), board.rooks(color), board.queens(color)
        };
        std::array<PieceType, 5> types {
            PieceType::Pawn, PieceType::Knight, PieceType::Bishop, PieceType::Rook, PieceType::Queen
        };
        for (std::size_t i = 0; i < lists.size(); ++i) {
            bitboard_t mask = 0;
            for (square_t square : lists[i]) {
                require(!(mask & Square::bitboard(square)), "duplicate piece list entry");
                mask |= Square::bitboard(square);
            }
            require(mask == pieceBoards[Piece::create(types[i], color)], "piece list mismatch");
        }
    }
    bitboard_t friendlyQueens = pieceBoards[Piece::create(PieceType::Queen, board.move_colour())];
    bitboard_t enemyQueens = pieceBoards[Piece::create(PieceType::Queen, board.opponent_colour())];
    require(board.friendly_orthogonal_sliders() ==
        (friendlyQueens | pieceBoards[Piece::create(PieceType::Rook, board.move_colour())]), "friendly rook mask");
    require(board.friendly_diagonal_sliders() ==
        (friendlyQueens | pieceBoards[Piece::create(PieceType::Bishop, board.move_colour())]), "friendly bishop mask");
    require(board.enemy_orthogonal_sliders() ==
        (enemyQueens | pieceBoards[Piece::create(PieceType::Rook, board.opponent_colour())]), "enemy rook mask");
    require(board.enemy_diagonal_sliders() ==
        (enemyQueens | pieceBoards[Piece::create(PieceType::Bishop, board.opponent_colour())]), "enemy bishop mask");
}

static void round_trip(Board& board, move_t move, bool inSearch = false) {
    std::string before = board.current_fen();
    uint64_t key = board.zobrist_key();
    std::vector<uint64_t> history(board.repetition_position_history().begin(), board.repetition_position_history().end());
    std::vector<move_t> gameMoves(board.all_game_moves().begin(), board.all_game_moves().end());
    bool check = board.is_in_check();
    board.make_move(move, inSearch);
    verify(board);
    require(board.is_in_check() == board.calculate_in_check_state(), "stale check cache");
    if (inSearch)
        require(std::ranges::equal(gameMoves, board.all_game_moves()), "search recorded a game move");
    board.unmake_move(move, inSearch);
    verify(board);
    require(before == board.current_fen() && key == board.zobrist_key(), "undo did not restore state");
    require(std::ranges::equal(history, board.repetition_position_history()), "undo lost repetition history");
    require(std::ranges::equal(gameMoves, board.all_game_moves()), "undo changed game history");
    require(check == board.is_in_check(), "undo check cache mismatch");
}

int main() {
    try {
        Board board = Board::create_board();
        verify(board);
        require(board.zobrist_key() == 0x463B96181691FC9CULL, "starting-position Polyglot key");
        board.make_move(Move::create(Square::E2, Square::E4));
        require(board.zobrist_key() == 0x823C9B50FD114196ULL, "e4 Polyglot key");
        board.unmake_move();
        require(Zobrist::piece(Piece::create(PieceType::Pawn, PieceColor::Black), Square::A1) ==
            0x9D39247E33776D41ULL, "black pawn piece mapping");
        require(Zobrist::side_to_move() == 0xF8D626AAAF278509ULL, "side key");
        for (int rights = 0; rights < 16; ++rights) {
            uint64_t key = 0;
            for (int index = 0; index < 4; ++index)
                if (rights & (1 << index)) key ^= Zobrist::castling_index(index);
            require(key == Zobrist::castling(static_cast<uint8_t>(rights)), "castling combination");
        }
        require(board.total_piece_count_without_pawns_and_kings() == 14, "initial material");
        require(!board.is_in_check(), "initial check");
        round_trip(board, Move::create(Square::E2, Square::E4));
        round_trip(board, Move::create(Square::G1, Square::F3), true);

        // Nested reversible history, then a pawn move which starts a new repetition segment.
        board.make_move(Move::create(Square::G1, Square::F3));
        board.make_move(Move::create(Square::G8, Square::F6));
        round_trip(board, Move::create(Square::E2, Square::E4));
        Board copy = Board::create_board(board);
        require(copy.current_fen() == board.current_fen(), "game copy mismatch");
        board.make_move(Move::create(Square::B1, Square::C3), true);
        Board gameCopy = Board::create_board(board);
        require(gameCopy.current_fen() == copy.current_fen(), "game copy included search move");
        board.unmake_move();
        board.unmake_move();
        board.unmake_move();
        require(board.current_fen() == FenUtility::startPositionFen, "nested undo mismatch");

        for (PieceColor color : { PieceColor::White, PieceColor::Black }) {
            std::string side = color == PieceColor::White ? "w" : "b";
            Board castles("r3k2r/8/8/8/8/8/8/R3K2R " + side + " KQkq - 7 12");
            for (square_t rook : { Square::A1, Square::H1 })
                round_trip(castles, Move::create<Move::Type::Castling>(
                    Square::relative_square(Square::E1, color), Square::relative_square(rook, color)));
            square_t from = Square::relative_square(Square::A1, color);
            square_t to = Square::relative_square(Square::A8, color);
            round_trip(castles, Move::create(from, to));
            castles.make_move(Move::create(from, to));
            require(castles.current_game_state().castlingRights == 5, "rook capture castling rights");
            verify(castles);
        }

        for (PieceType type : { PieceType::Knight, PieceType::Bishop, PieceType::Rook, PieceType::Queen }) {
            Board white("1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1");
            round_trip(white, Move::create<Move::Type::Promotion>(Square::A7, Square::A8, type));
            round_trip(white, Move::create<Move::Type::Promotion>(Square::A7, Square::B8, type));
            Board black("4k3/8/8/8/8/8/p7/1R2K3 b - - 0 1");
            round_trip(black, Move::create<Move::Type::Promotion>(Square::A2, Square::A1, type));
            round_trip(black, Move::create<Move::Type::Promotion>(Square::A2, Square::B1, type));
        }

        Board epWhite("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
        require(epWhite.current_game_state().enPassantSquare == Square::D6, "white EP load");
        round_trip(epWhite, Move::create<Move::Type::EnPassant>(Square::E5, Square::D6));
        Board epBlack("4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1");
        round_trip(epBlack, Move::create<Move::Type::EnPassant>(Square::E4, Square::D3));

        Board pinned("4k3/8/8/r4pPK/8/8/8/8 w - f6 0 1");
        require(pinned.current_game_state().enPassantSquare == Square::None, "illegal EP survived load");
        Board doublePush("4k3/3p4/8/4P3/8/8/8/4K3 b - - 0 1");
        doublePush.make_move(Move::create(Square::D7, Square::D5));
        require(doublePush.current_game_state().enPassantSquare == Square::D6, "double push EP missing");
        verify(doublePush);
        round_trip(doublePush, Move::create<Move::Type::EnPassant>(Square::E5, Square::D6));
        doublePush.unmake_move();
        verify(doublePush);

        for (Board* current : { &epWhite, &epBlack, &board }) {
            std::string before = current->current_fen();
            uint64_t key = current->zobrist_key();
            int counter = current->fifty_move_counter();
            current->make_null_move();
            verify(*current);
            require(current->fifty_move_counter() == counter, "null changed fifty-move counter");
            require(current->current_game_state().enPassantSquare == Square::None, "null kept EP");
            current->unmake_null_move();
            verify(*current);
            require(current->current_fen() == before && current->zobrist_key() == key, "null undo mismatch");
        }

        for (const char* fen : {
            "4r1k1/8/8/8/8/8/8/4K3 w - - 0 1",
            "6k1/8/8/8/1b6/8/8/4K3 w - - 0 1",
            "6k1/8/8/8/8/5n2/8/4K3 w - - 0 1",
            "6k1/8/8/8/8/8/3p4/4K3 w - - 0 1",
            "8/8/8/8/8/8/4k3/4K3 w - - 0 1"
        }) {
            Board checkBoard(fen);
            require(checkBoard.is_in_check(), "missed check");
        }
        Board cache("6k1/8/8/8/8/8/8/R3K3 w - - 0 1");
        require(!cache.is_in_check(), "unexpected check");
        cache.make_move(Move::create(Square::A1, Square::G1));
        require(cache.is_in_check(), "check cache was not invalidated");
        cache.unmake_move();
        require(!cache.is_in_check(), "undo cache was not invalidated");
        cache.load_start_position();
        verify(cache);
        require(cache.all_game_moves().empty() && cache.repetition_position_history().size() == 1, "load retained history");


        auto partial = FenUtility::position_from_fen("8/8/8/8/8/8/8/8 w -");
        require(partial.m_fiftyMovePlyCount == 0 && partial.m_moveCount == 1, "FEN counter defaults");
        require(FenUtility::flip_fen(FenUtility::startPositionFen) ==
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1", "flip start");
        std::string asymmetric = "4k3/8/8/3pP3/8/8/8/4K2R w K d6 17 42";
        require(FenUtility::flip_fen(FenUtility::flip_fen(asymmetric)) == asymmetric, "flip twice");
        for (const char* invalid : {
            "", "8/8/8/8/8/8/8 w -", "9/8/8/8/8/8/8/8 w -",
            "8/8/8/8/8/8/8/7 w -", "8/8/8/8/8/8/8/8/8 w -",
            "x7/8/8/8/8/8/8/8 w -", "8/8/8/8/8/8/8/8 x -",
            "8/8/8/8/8/8/8/8 w KK", "8/8/8/8/8/8/8/8 w X",
            "8/8/8/8/8/8/8/8 w - d3", "8/8/8/8/8/8/8/8 w - z6",
            "8/8/8/8/8/8/8/8 w - - -1 1", "8/8/8/8/8/8/8/8 w - - 0 0",
            "8/8/8/8/8/8/8/8 w - - 0 1 extra", "8/8/8/8/8/8/8/8 w - - 12x 1",
            "8/8/8/8/8/8/8/8 w - - 99999999999999999999999 1"
        }) {
            bool rejected = false;
            try { FenUtility::position_from_fen(invalid); }
            catch (const std::invalid_argument&) { rejected = true; }
            require(rejected, "invalid FEN was accepted");
        }
        require(FenUtility::current_fen(epWhite, false).find(" d6 ") != std::string::npos, "legal EP omitted");
        // Default board move handling keeps this pseudo-legal EP square, although the pawn is pinned.
        Board epFilter("4k3/5p2/8/r5PK/8/8/8/8 b - - 0 1");
        epFilter.make_move(Move::create(Square::F7, Square::F5));
        std::string beforeFilter = epFilter.current_fen();
        uint64_t beforeKey = epFilter.zobrist_key();
        std::vector<uint64_t> beforeHistory(epFilter.repetition_position_history().begin(), epFilter.repetition_position_history().end());
        require(beforeFilter.find(" f6 ") != std::string::npos, "stored EP missing");
        require(FenUtility::current_fen(epFilter, false).find(" - 0 2") != std::string::npos, "illegal EP emitted");
        require(epFilter.current_fen() == beforeFilter && epFilter.zobrist_key() == beforeKey &&
            std::ranges::equal(beforeHistory, epFilter.repetition_position_history()), "FEN formatting mutated board");

        std::cout << "Board, FEN, and Zobrist tests passed using production implementations.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
