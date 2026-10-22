#include "core/board.hpp"

#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>

#include "movegen/attacks.hpp"
#include "movegen/movegen.hpp"
#include "utils/fen_utility.hpp"
#include "core/zobrist.hpp"

namespace chess {

bool GameState::has_kingside_castle_right(PieceColor color) const {
    return (castlingRights & (color == PieceColor::White ? 1 : 4)) != 0;
}

bool GameState::has_queenside_castle_right(PieceColor color) const {
    return (castlingRights & (color == PieceColor::White ? 2 : 8)) != 0;
}

Board::Board() {
    load_start_position();
}

Board::Board(std::string_view fen) {
    load_position(fen);
}

void Board::make_move(move_t move, bool inSearch) {
    square_t startSquare = Move::from(move);
    square_t targetSquare = Move::to(move);

    Move::Type moveFlag = Move::type(move);

    piece_t movedPiece = m_squares[startSquare];
    PieceType movedPieceType = Piece::type(movedPiece);

    bool isCastling = moveFlag == Move::Type::Castling;
    bool isEnPassant = moveFlag == Move::Type::EnPassant;
    bool isPromotion = moveFlag == Move::Type::Promotion;
    
    PieceColor moveColour = move_colour();
    PieceColor opponentColour = opponent_colour();

    assert(!Piece::isEmpty(movedPiece) && Piece::color(movedPiece) == moveColour);
    assert(startSquare != targetSquare);
    assert(m_plyCount < std::numeric_limits<int>::max());
    assert(m_currentGameState.fiftyMoveCounter < std::numeric_limits<int>::max());

    square_t captureSquare = isEnPassant ? Square::ep_square(targetSquare) : targetSquare;
    piece_t capturedPiece = isCastling ? Piece::create_empty() : m_squares[captureSquare];

    assert(Piece::isEmpty(capturedPiece) ||
           (Piece::color(capturedPiece) == opponentColour && Piece::type(capturedPiece) != PieceType::King));

    assert(!isEnPassant || (movedPieceType == PieceType::Pawn &&
           targetSquare == m_currentGameState.enPassantSquare &&
           Piece::type(capturedPiece) == PieceType::Pawn && Piece::isEmpty(m_squares[targetSquare])));

    m_gameStateHistory.push_back({ m_currentGameState, move, m_repetitionStart, inSearch, false });

    uint64_t newZobristKey = m_currentGameState.zobristKey;
    uint8_t prevCastleState = m_currentGameState.castlingRights;
    if (m_currentGameState.enPassantSquare != Square::None)
        newZobristKey ^= Zobrist::en_passant(Square::file(m_currentGameState.enPassantSquare));

    m_currentGameState.enPassantSquare = Square::None;
    m_currentGameState.fiftyMoveCounter++;
    m_plyCount++;

    if (!Piece::isEmpty(capturedPiece)) {
        remove_piece(capturedPiece, captureSquare);
        newZobristKey ^= Zobrist::piece(capturedPiece, captureSquare);
        m_currentGameState.fiftyMoveCounter = 0;

        if (Piece::type(capturedPiece) == PieceType::Rook)
            clear_rook_castle_right(captureSquare, opponentColour);
    }

    if (movedPieceType == PieceType::King)
        m_currentGameState.castlingRights &= moveColour == PieceColor::White ? 12 : 3;
    else if (movedPieceType == PieceType::Rook)
        clear_rook_castle_right(startSquare, moveColour);
    else if (movedPieceType == PieceType::Pawn) {
        m_currentGameState.fiftyMoveCounter = 0;

        if (Square::value_distance(startSquare, targetSquare) == 16) {
            square_t enPassantSquare = Square::ep_square(targetSquare);
            bitboard_t enemyPawns = m_pieceBitboards[Piece::create(PieceType::Pawn, opponentColour)];
            
            if (Attacks::get_pawn_attacks(enPassantSquare, moveColour) & enemyPawns) {
                m_currentGameState.enPassantSquare = enPassantSquare;
                newZobristKey ^= Zobrist::en_passant(Square::file(enPassantSquare));
            }
        }
    }

    if (isCastling) {
        bool kingside = targetSquare > startSquare;

        square_t kingTo = Square::castling_king_square(kingside, moveColour);
        square_t rookTo = Square::castling_rook_square(kingside, moveColour);

        piece_t rookPiece = Piece::create(PieceType::Rook, moveColour);
        assert(movedPieceType == PieceType::King && m_squares[targetSquare] == rookPiece);

        remove_piece(movedPiece, startSquare);
        remove_piece(rookPiece, targetSquare);
        place_piece(movedPiece, kingTo);
        place_piece(rookPiece, rookTo);

        newZobristKey ^= Zobrist::piece(movedPiece, startSquare) ^ Zobrist::piece(movedPiece, kingTo);
        newZobristKey ^= Zobrist::piece(rookPiece, targetSquare) ^ Zobrist::piece(rookPiece, rookTo);
    } else if (isPromotion) {
        assert(movedPieceType == PieceType::Pawn);
        piece_t promotionPiece = Piece::create(Move::promotion_piece(move), moveColour);
        remove_piece(movedPiece, startSquare);
        place_piece(promotionPiece, targetSquare);
        newZobristKey ^= Zobrist::piece(movedPiece, startSquare) ^ Zobrist::piece(promotionPiece, targetSquare);
    } else {
        move_piece(movedPiece, startSquare, targetSquare);
        newZobristKey ^= Zobrist::piece(movedPiece, startSquare) ^ Zobrist::piece(movedPiece, targetSquare);
    }

    newZobristKey ^= Zobrist::castling(prevCastleState);
    newZobristKey ^= Zobrist::castling(m_currentGameState.castlingRights);
    newZobristKey ^= Zobrist::side_to_move();

    m_currentGameState.capturedPieceType = Piece::type(capturedPiece);
    m_currentGameState.zobristKey = newZobristKey;

    m_isWhiteToMove = !m_isWhiteToMove;

    update_slider_bitboards();
    m_hasCachedInCheckValue = false;

    if (!inSearch) {
        if (movedPieceType == PieceType::Pawn || !Piece::isEmpty(capturedPiece))
            m_repetitionStart = m_repetitionPositionHistory.size();

        m_repetitionPositionHistory.push_back(newZobristKey);
        m_allGameMoves.push_back(move);
    }
}

void Board::unmake_move(move_t move, bool inSearch) {
    assert(!m_gameStateHistory.empty());
    const HistoryEntry previous = m_gameStateHistory.back();
    assert(!previous.isNullMove && previous.move == move && previous.inSearch == inSearch);

    m_isWhiteToMove = !m_isWhiteToMove;
    m_plyCount--;

    square_t movedFrom = Move::from(move);
    square_t movedTo = Move::to(move);

    Move::Type moveFlag = Move::type(move);
    piece_t capturedPiece = Piece::create(m_currentGameState.capturedPieceType, opponent_colour());

    if (moveFlag == Move::Type::Castling) {
        bool kingside = movedTo > movedFrom;

        square_t kingTo = Square::castling_king_square(kingside, move_colour());
        square_t rookTo = Square::castling_rook_square(kingside, move_colour());
        piece_t kingPiece = Piece::create(PieceType::King, move_colour());
        piece_t rookPiece = Piece::create(PieceType::Rook, move_colour());

        remove_piece(kingPiece, kingTo);
        remove_piece(rookPiece, rookTo);
        place_piece(kingPiece, movedFrom);
        place_piece(rookPiece, movedTo);
    } else {
        piece_t movedPiece = m_squares[movedTo];

        if (moveFlag == Move::Type::Promotion) {
            remove_piece(movedPiece, movedTo);
            place_piece(Piece::create(PieceType::Pawn, move_colour()), movedFrom);
        } else {
            move_piece(movedPiece, movedTo, movedFrom);
        }

        if (!Piece::isEmpty(capturedPiece)) {
            square_t captureSquare = moveFlag == Move::Type::EnPassant ? Square::ep_square(movedTo) : movedTo;
            place_piece(capturedPiece, captureSquare);
        }
    }

    m_currentGameState = previous.gameState;
    m_gameStateHistory.pop_back();
    if (!inSearch) {
        assert(!m_allGameMoves.empty() && !m_repetitionPositionHistory.empty());

        m_allGameMoves.pop_back();
        m_repetitionPositionHistory.pop_back();
        m_repetitionStart = previous.repetitionStart;
    }

    update_slider_bitboards();
    m_hasCachedInCheckValue = false;
}

void Board::unmake_move() {
    assert(!m_gameStateHistory.empty() && !m_gameStateHistory.back().isNullMove);
    unmake_move(m_gameStateHistory.back().move, m_gameStateHistory.back().inSearch);
}

void Board::make_null_move() {
    assert(!is_in_check());
    assert(m_plyCount < std::numeric_limits<int>::max());

    m_gameStateHistory.push_back({ m_currentGameState, 0, m_repetitionStart, true, true });
    m_currentGameState.zobristKey ^= Zobrist::side_to_move();

    if (m_currentGameState.enPassantSquare != Square::None)
        m_currentGameState.zobristKey ^= Zobrist::en_passant(Square::file(m_currentGameState.enPassantSquare));

    m_currentGameState.enPassantSquare = Square::None;
    m_currentGameState.capturedPieceType = PieceType::None;
   
    m_isWhiteToMove = !m_isWhiteToMove;
    m_plyCount++;

    update_slider_bitboards();
    m_hasCachedInCheckValue = false;
}

void Board::unmake_null_move() {
    assert(!m_gameStateHistory.empty() && m_gameStateHistory.back().isNullMove);

    m_currentGameState = m_gameStateHistory.back().gameState;
    m_gameStateHistory.pop_back();

    m_isWhiteToMove = !m_isWhiteToMove;
    m_plyCount--;

    update_slider_bitboards();
    m_hasCachedInCheckValue = false;
}

bool Board::is_in_check() const {
    if (!m_hasCachedInCheckValue) {
        m_cachedInCheckValue = calculate_in_check_state();
        m_hasCachedInCheckValue = true;
    }

    return m_cachedInCheckValue;
}

bool Board::calculate_in_check_state() const {
    square_t kingSquare = m_kingSquare[move_colour_index()];
    assert(Square::is_valid(kingSquare));

    PieceColor enemyColour = opponent_colour();
    if (Attacks::get_pawn_attacks(kingSquare, move_colour()) & m_pieceBitboards[Piece::create(PieceType::Pawn, enemyColour)])
        return true;
    if (Attacks::get_knight_attacks(kingSquare) & m_pieceBitboards[Piece::create(PieceType::Knight, enemyColour)])
        return true;
    if (Attacks::get_king_attacks(kingSquare) & m_pieceBitboards[Piece::create(PieceType::King, enemyColour)])
        return true;
    if (Attacks::get_bishop_attacks(kingSquare, m_allPiecesBitboard) & m_enemyDiagonalSliders)
        return true;

    return (Attacks::get_rook_attacks(kingSquare, m_allPiecesBitboard) & m_enemyOrthogonalSliders) != 0;
}

void Board::load_start_position() {
    load_position(FenUtility::startPositionFen);
}

void Board::load_position(std::string_view fen) {
    load_position(FenUtility::position_from_fen(fen));
}

void Board::load_position(const FenUtility::PositionInfo& positionInfo) {
    if (positionInfo.m_epFile < 0 || positionInfo.m_epFile > 8 ||
        positionInfo.m_fiftyMovePlyCount < 0 ||
        positionInfo.m_fiftyMovePlyCount == std::numeric_limits<int>::max() ||
        positionInfo.m_moveCount < 1 ||
        positionInfo.m_moveCount > std::numeric_limits<int>::max() / 2)
        throw std::invalid_argument("Invalid position counters or en-passant file");

    initialize();

    m_gameStartFen = positionInfo.m_fen;
    for (int index = 0; index < Square::Count; index++) {
        piece_t piece = positionInfo.m_squares[index];
        if (!Piece::isEmpty(piece)) {
            if (piece < 2 || piece >= m_pieceBitboards.size())
                throw std::invalid_argument("Invalid piece code");
            place_piece(piece, static_cast<square_t>(index));
            m_currentGameState.zobristKey ^= Zobrist::piece(piece, static_cast<square_t>(index));
        }
    }

    m_isWhiteToMove = positionInfo.m_whiteToMove;
    m_plyCount = (positionInfo.m_moveCount - 1) * 2 + (m_isWhiteToMove ? 0 : 1);
    m_currentGameState.fiftyMoveCounter = positionInfo.m_fiftyMovePlyCount;

    m_currentGameState.castlingRights =
        (positionInfo.m_whiteCastleKingside ? 1 : 0) |
        (positionInfo.m_whiteCastleQueenside ? 2 : 0) |
        (positionInfo.m_blackCastleKingside ? 4 : 0) |
        (positionInfo.m_blackCastleQueenside ? 8 : 0);

    update_slider_bitboards();

    for (PieceColor color : { PieceColor::White, PieceColor::Black }) {
        square_t kingFrom = Square::relative_square(Square::E1, color);

        if (m_squares[kingFrom] != Piece::create(PieceType::King, color)) {
            m_currentGameState.castlingRights &= color == PieceColor::White ? 12 : 3;
        } else {
            for (square_t corner : { Square::A1, Square::H1 }) {
                square_t rookSquare = Square::relative_square(corner, color);
                if (m_squares[rookSquare] != Piece::create(PieceType::Rook, color))
                    clear_rook_castle_right(rookSquare, color);
            }
        }
    }

    if (positionInfo.m_epFile != 0) {
        square_t enPassantSquare = Square::index(m_isWhiteToMove ? 5 : 2, positionInfo.m_epFile - 1);
        if (is_en_passant_square_valid(enPassantSquare)) {
            m_currentGameState.enPassantSquare = enPassantSquare;
            m_currentGameState.zobristKey ^= Zobrist::en_passant(Square::file(enPassantSquare));
        }
    }

    if (m_isWhiteToMove)
        m_currentGameState.zobristKey ^= Zobrist::side_to_move();
    
    m_currentGameState.zobristKey ^= Zobrist::castling(m_currentGameState.castlingRights);
    m_repetitionPositionHistory.push_back(m_currentGameState.zobristKey);
}

bool Board::is_en_passant_square_valid(square_t targetSquare) const {
    square_t capturedSquare = Square::ep_square(targetSquare);
    PieceColor enemyColour = opponent_colour();

    if (!Piece::isEmpty(m_squares[targetSquare]) ||  m_squares[capturedSquare] != Piece::create(PieceType::Pawn, enemyColour))
        return false;
    
    bitboard_t candidates = Attacks::get_pawn_attacks(targetSquare, enemyColour) &
        m_pieceBitboards[Piece::create(PieceType::Pawn, move_colour())];

    square_t kingSquare = m_kingSquare[move_colour_index()];
    if (!Square::is_valid(kingSquare))
        return false;

    bitboard_t enemyPawns = m_pieceBitboards[Piece::create(PieceType::Pawn, enemyColour)] & ~Square::bitboard(capturedSquare);

    while (candidates != 0) {
        square_t startSquare = Bitboard::pop(candidates);
        bitboard_t occupied = (m_allPiecesBitboard &
            ~(Square::bitboard(startSquare) | Square::bitboard(capturedSquare))) | Square::bitboard(targetSquare);

        if ((Attacks::get_pawn_attacks(kingSquare, move_colour()) & enemyPawns) ||
            (Attacks::get_knight_attacks(kingSquare) & m_pieceBitboards[Piece::create(PieceType::Knight, enemyColour)]) ||
            (Attacks::get_king_attacks(kingSquare) & m_pieceBitboards[Piece::create(PieceType::King, enemyColour)]) ||
            (Attacks::get_bishop_attacks(kingSquare, occupied) & m_enemyDiagonalSliders) ||
            (Attacks::get_rook_attacks(kingSquare, occupied) & m_enemyOrthogonalSliders))
            continue;

        return true;
    }
    return false;
}

void Board::place_piece(piece_t piece, square_t square) {
    assert(Square::is_valid(square) && Piece::isEmpty(m_squares[square]));
    assert(piece >= 2 && piece < m_pieceBitboards.size());

    int colourIndex = static_cast<int>(Piece::color(piece));
    m_allPieceLists[piece].push_back(square);

    Bitboard::set(m_pieceBitboards[piece], square);
    Bitboard::set(m_colourBitboards[colourIndex], square);
    m_squares[square] = piece;

    PieceType type = Piece::type(piece);

    if (type == PieceType::King)
        m_kingSquare[colourIndex] = square;
    else if (type != PieceType::Pawn)
        m_totalPieceCountWithoutPawnsAndKings++;
}

void Board::remove_piece(piece_t piece, square_t square) {
    assert(Square::is_valid(square) && m_squares[square] == piece && !Piece::isEmpty(piece));

    int colourIndex = static_cast<int>(Piece::color(piece));
    auto& pieceList = m_allPieceLists[piece];

    auto entry = std::find(pieceList.begin(), pieceList.end(), square);
    assert(entry != pieceList.end());

    *entry = pieceList.back();
    pieceList.pop_back();

    Bitboard::clear(m_pieceBitboards[piece], square);
    Bitboard::clear(m_colourBitboards[colourIndex], square);

    m_squares[square] = Piece::create_empty();
    PieceType type = Piece::type(piece);
    if (type == PieceType::King)
        m_kingSquare[colourIndex] = Square::None;
    else if (type != PieceType::Pawn)
        m_totalPieceCountWithoutPawnsAndKings--;
}

void Board::move_piece(piece_t piece, square_t startSquare, square_t targetSquare) {
    remove_piece(piece, startSquare);
    place_piece(piece, targetSquare);
}

void Board::clear_rook_castle_right(square_t square, PieceColor color) {
    if (square == Square::relative_square(Square::H1, color))
        m_currentGameState.castlingRights &= color == PieceColor::White ? 14 : 11;
    else if (square == Square::relative_square(Square::A1, color))
        m_currentGameState.castlingRights &= color == PieceColor::White ? 13 : 7;
}

void Board::update_slider_bitboards() {
    PieceColor friendlyColour = move_colour();
    PieceColor enemyColour = opponent_colour();
    m_allPiecesBitboard = m_colourBitboards[whiteIndex] | m_colourBitboards[blackIndex];

    m_friendlyOrthogonalSliders = m_pieceBitboards[Piece::create(PieceType::Rook, friendlyColour)] |
        m_pieceBitboards[Piece::create(PieceType::Queen, friendlyColour)];

    m_friendlyDiagonalSliders = m_pieceBitboards[Piece::create(PieceType::Bishop, friendlyColour)] |
        m_pieceBitboards[Piece::create(PieceType::Queen, friendlyColour)];

    m_enemyOrthogonalSliders = m_pieceBitboards[Piece::create(PieceType::Rook, enemyColour)] |
        m_pieceBitboards[Piece::create(PieceType::Queen, enemyColour)];

    m_enemyDiagonalSliders = m_pieceBitboards[Piece::create(PieceType::Bishop, enemyColour)] |
        m_pieceBitboards[Piece::create(PieceType::Queen, enemyColour)];
}

void Board::initialize() {
    m_squares.fill(Piece::create_empty());
    m_kingSquare.fill(Square::None);
    m_pieceBitboards.fill(0);
    m_colourBitboards.fill(0);

    for (auto& pieceList : m_allPieceLists) {
        pieceList.clear();
        pieceList.reserve(16);
    }
    
    m_totalPieceCountWithoutPawnsAndKings = 0;
    m_isWhiteToMove = true;
    m_plyCount = 0;

    m_currentGameState = {};
    m_gameStateHistory.clear();
    m_gameStateHistory.reserve(256);
    m_repetitionPositionHistory.clear();
    m_repetitionPositionHistory.reserve(256);

    m_repetitionStart = 0;

    m_allGameMoves.clear();
    m_gameStartFen.clear();

    m_cachedInCheckValue = false;
    m_hasCachedInCheckValue = false;
    
    update_slider_bitboards();
}

std::pair<GameResult, GameResultReason> Board::isGameOver() const {
    MoveGenerator generator;
    if (!generator.has_legal_moves(*this)) {
        if (generator.in_check())
            return { GameResult::Lose, GameResultReason::Checkmate };
        return { GameResult::Draw, GameResultReason::Stalemate };
    }

    if (GameResultUtility::isInsufficientMaterial(*this))
        return { GameResult::Draw, GameResultReason::Insufficient_Material };
    if (GameResultUtility::isRepetition(*this, 3))
        return { GameResult::Draw, GameResultReason::Threefold_Repetition };
    if (GameResultUtility::isHalfMoveDraw(*this))
        return { GameResult::Draw, GameResultReason::Fifty_Move_Rule };

    return { GameResult::None, GameResultReason::None };
}

piece_t Board::get_square(square_t square) const {
    assert(Square::is_valid(square));
    return m_squares[square];
}

const std::array<piece_t, Square::Count>& Board::squares() const { return m_squares; }
square_t Board::king_square(PieceColor color) const { return m_kingSquare[static_cast<int>(color)]; }
const std::array<bitboard_t, 14>& Board::piece_bitboards() const { return m_pieceBitboards; }
const std::array<bitboard_t, 2>& Board::colour_bitboards() const { return m_colourBitboards; }
bitboard_t Board::all_pieces_bitboard() const { return m_allPiecesBitboard; }
bitboard_t Board::friendly_orthogonal_sliders() const { return m_friendlyOrthogonalSliders; }
bitboard_t Board::friendly_diagonal_sliders() const { return m_friendlyDiagonalSliders; }
bitboard_t Board::enemy_orthogonal_sliders() const { return m_enemyOrthogonalSliders; }
bitboard_t Board::enemy_diagonal_sliders() const { return m_enemyDiagonalSliders; }
int Board::total_piece_count_without_pawns_and_kings() const { return m_totalPieceCountWithoutPawnsAndKings; }

std::span<const square_t> Board::piece_list(PieceType type, PieceColor color) const {
    return m_allPieceLists[Piece::create(type, color)];
}

std::span<const square_t> Board::rooks(PieceColor color) const { return piece_list(PieceType::Rook, color); }
std::span<const square_t> Board::bishops(PieceColor color) const { return piece_list(PieceType::Bishop, color); }
std::span<const square_t> Board::queens(PieceColor color) const { return piece_list(PieceType::Queen, color); }
std::span<const square_t> Board::knights(PieceColor color) const { return piece_list(PieceType::Knight, color); }
std::span<const square_t> Board::pawns(PieceColor color) const { return piece_list(PieceType::Pawn, color); }

bool Board::is_white_to_move() const { return m_isWhiteToMove; }
PieceColor Board::move_colour() const { return m_isWhiteToMove ? PieceColor::White : PieceColor::Black; }
PieceColor Board::opponent_colour() const { return m_isWhiteToMove ? PieceColor::Black : PieceColor::White; }
int Board::move_colour_index() const { return static_cast<int>(move_colour()); }
int Board::opponent_colour_index() const { return static_cast<int>(opponent_colour()); }
int Board::ply_count() const { return m_plyCount; }
int Board::fifty_move_counter() const { return m_currentGameState.fiftyMoveCounter; }
uint64_t Board::zobrist_key() const { return m_currentGameState.zobristKey; }
const GameState& Board::current_game_state() const { return m_currentGameState; }
std::span<const uint64_t> Board::repetition_position_history() const {
    return std::span<const uint64_t>(m_repetitionPositionHistory).subspan(m_repetitionStart);
}
std::span<const move_t> Board::all_game_moves() const { return m_allGameMoves; }
std::string Board::current_fen() const { return FenUtility::current_fen(*this); }
const std::string& Board::game_start_fen() const { return m_gameStartFen; }

std::ostream& operator<<(std::ostream& output, const Board& board) {
    return output << board.current_fen();
}

}
