#include "movegen/movegen.hpp"

#include <algorithm>
#include <cassert>

#include "core/directions.hpp"
#include "movegen/attacks.hpp"

namespace chess {

constexpr MoveGenerator::between_table_t MoveGenerator::create_squares_between() {
    between_table_t table {};
    for (int start = 0; start < Square::Count; start++) {
        for (int target = 0; target < Square::Count; target++) {
            table[start][target] = Square::bitboard(static_cast<square_t>(target));

            int rankDistance = Square::rank(target) - Square::rank(start);
            int fileDistance = Square::file(target) - Square::file(start);

            if (rankDistance != 0 && fileDistance != 0 &&
                Square::abs(rankDistance) != Square::abs(fileDistance))
                continue;

            int rankStep = (rankDistance > 0) - (rankDistance < 0);
            int fileStep = (fileDistance > 0) - (fileDistance < 0);
            int rank = Square::rank(start) + rankStep;
            int file = Square::file(start) + fileStep;

            while (rank != Square::rank(target) || file != Square::file(target)) {
                table[start][target] |= Square::bitboard(Square::index(rank, file));
                rank += rankStep;
                file += fileStep;
            }
        }
    }
    return table;
}

const MoveGenerator::between_table_t MoveGenerator::SquaresBetween = create_squares_between();

bitboard_t MoveGenerator::squares_between(square_t startSquare, square_t targetSquare) {
    assert(Square::is_valid(startSquare) && Square::is_valid(targetSquare));
    return SquaresBetween[startSquare][targetSquare];
}

MoveList MoveGenerator::generate_moves(const Board& board, GenerationMode mode, uint8_t pieceMask) {
    MoveList moves;
    generate_moves(board, moves, mode, pieceMask);
    return moves;
}

void MoveGenerator::generate_moves(const Board& board, MoveList& moves, GenerationMode mode, uint8_t pieceMask) {
    moves.clear();
    initialize(board, mode, false);
    generate_all_moves(moves, pieceMask);
}

MoveList MoveGenerator::generate_moves(const Board& board, bool capturesOnly) {
    return generate_moves(board, capturesOnly ? GenerationMode::CapturesOnly : GenerationMode::All);
}

bool MoveGenerator::has_legal_moves(const Board& board, GenerationMode mode, uint8_t pieceMask) {
    MoveList moves;
    initialize(board, mode, true);
    generate_all_moves(moves, pieceMask);
    return !moves.is_empty();
}

bool MoveGenerator::is_legal(const Board& board, move_t move) {
    // Legality is independent of an optional promotion selection for search.
    initialize(board, GenerationMode::All, false);
    MoveList moves;
    PromotionMode savedMode = m_promotionMode;
    m_promotionMode = PromotionMode::All;
    uint8_t pieceMask = static_cast<uint8_t>(1 << static_cast<int>(Piece::type(board.get_square(Move::from(move)))));
    generate_all_moves(moves, pieceMask);
    m_promotionMode = savedMode;
    return std::find(moves.begin(), moves.end(), move) != moves.end();
}

bool MoveGenerator::is_en_passant_square_valid(const Board& board, square_t targetSquare) {
    initialize(board, GenerationMode::CapturesOnly, true);
    MoveList moves;
    generate_en_passant_moves(moves, targetSquare);
    return !moves.is_empty();
}

void MoveGenerator::set_promotion_mode(PromotionMode mode) { m_promotionMode = mode; }
bool MoveGenerator::in_check() const { return m_checkCount != 0; }
bool MoveGenerator::in_double_check() const { return m_checkCount > 1; }
bitboard_t MoveGenerator::opponent_attack_map() const { return m_opponentAttackMap; }
bool MoveGenerator::finished(const MoveList& moves) const { return m_oneMoveOnly && !moves.is_empty(); }

bitboard_t MoveGenerator::friendly_piece_board(PieceType type) const {
    return m_board->piece_bitboards()[Piece::create(type, m_friendlyColor)];
}

bitboard_t MoveGenerator::enemy_piece_board(PieceType type) const {
    return m_board->piece_bitboards()[Piece::create(type, m_opponentColor)];
}

void MoveGenerator::initialize(const Board& board, GenerationMode mode, bool oneMoveOnly) {
    bool attacksReady = m_board != nullptr && m_positionKey == board.zobrist_key() &&
        m_friendlyColor == board.move_color() && m_positionPieces == board.piece_bitboards();
    m_board = &board;
    m_generationMode = mode;
    m_oneMoveOnly = oneMoveOnly;

    m_friendlyColor = board.move_color();
    m_opponentColor = board.opponent_color();

    m_friendlyKingSquare = board.king_square(m_friendlyColor);
    assert(Square::is_valid(m_friendlyKingSquare));

    m_friendlyPieces = board.color_bitboards()[board.move_color_index()];
    m_enemyPieces = board.color_bitboards()[board.opponent_color_index()];
    m_allPieces = board.all_pieces_bitboard();

    m_moveMask = (mode == GenerationMode::CapturesOnly || mode == GenerationMode::Tactical) ? m_enemyPieces :
        mode == GenerationMode::QuietOnly ? ~m_allPieces : ~m_friendlyPieces;

    m_moveMask &= ~enemy_piece_board(PieceType::King);

    if (!attacksReady) {
        calculate_attack_data();
        m_positionKey = board.zobrist_key();
        m_positionPieces = board.piece_bitboards();
    }
}

void MoveGenerator::calculate_attack_data() {
    bitboard_t checkers = Attacks::get_knight_attacks(m_friendlyKingSquare) & enemy_piece_board(PieceType::Knight);
    checkers |= Attacks::get_pawn_attacks(m_friendlyKingSquare, m_friendlyColor) & enemy_piece_board(PieceType::Pawn);
    checkers |= Attacks::get_king_attacks(m_friendlyKingSquare) & enemy_piece_board(PieceType::King);

    bitboard_t slidingCheckers =
        (Attacks::get_rook_attacks(m_friendlyKingSquare, m_allPieces) & m_board->enemy_orthogonal_sliders()) |
        (Attacks::get_bishop_attacks(m_friendlyKingSquare, m_allPieces) & m_board->enemy_diagonal_sliders());

    m_checkCount = Bitboard::count(checkers | slidingCheckers);
    m_checkRayBitmask = checkers;

    while (slidingCheckers != 0)
        m_checkRayBitmask |= squares_between(m_friendlyKingSquare, Bitboard::pop(slidingCheckers));

    if (m_checkCount == 0)
        m_checkRayBitmask = ~bitboard_t{0};

    m_orthogonalPinRays = calculate_pin_rays(true);
    m_diagonalPinRays = calculate_pin_rays(false);

    calculate_opponent_attack_map();
}

bitboard_t MoveGenerator::calculate_pin_rays(bool orthogonal) const {
    bitboard_t possiblePinners = orthogonal
        ? Attacks::get_rook_attacks(m_friendlyKingSquare, m_enemyPieces) & m_board->enemy_orthogonal_sliders()
        : Attacks::get_bishop_attacks(m_friendlyKingSquare, m_enemyPieces) & m_board->enemy_diagonal_sliders();

    bitboard_t pinRays = 0;
    while (possiblePinners != 0) {
        bitboard_t ray = squares_between(m_friendlyKingSquare, Bitboard::pop(possiblePinners));
        if (Bitboard::count(ray & m_friendlyPieces) == 1) pinRays |= ray;
    }

    return pinRays;
}

void MoveGenerator::calculate_opponent_attack_map() {
    bitboard_t occupied = m_allPieces & ~Square::bitboard(m_friendlyKingSquare);

    m_opponentAttackMap = Attacks::get_pawn_attacks(enemy_piece_board(PieceType::Pawn), m_opponentColor);

    bitboard_t knights = enemy_piece_board(PieceType::Knight);
    bitboard_t kings = enemy_piece_board(PieceType::King);
    bitboard_t diagonalSliders = m_board->enemy_diagonal_sliders();
    bitboard_t orthogonalSliders = m_board->enemy_orthogonal_sliders();

    while (knights != 0)
        m_opponentAttackMap |= Attacks::get_knight_attacks(Bitboard::pop(knights));
    while (kings != 0)
        m_opponentAttackMap |= Attacks::get_king_attacks(Bitboard::pop(kings));
    while (diagonalSliders != 0)
        m_opponentAttackMap |= Attacks::get_bishop_attacks(Bitboard::pop(diagonalSliders), occupied);
    while (orthogonalSliders != 0)
        m_opponentAttackMap |= Attacks::get_rook_attacks(Bitboard::pop(orthogonalSliders), occupied);
}

void MoveGenerator::generate_all_moves(MoveList& moves, uint8_t pieceMask) {
    if (pieceMask & King) {
        generate_king_moves(moves);
        if (finished(moves)) return;

        if (!in_check() && m_generationMode != GenerationMode::CapturesOnly && m_generationMode != GenerationMode::Tactical)
            generate_castling_moves(moves);
        if (finished(moves)) return;
    }

    if (in_double_check()) return;

    if (pieceMask & Pawn) generate_pawn_moves(moves);
    if (finished(moves)) return;

    if (pieceMask & Knight) generate_knight_moves(moves);
    if (finished(moves)) return;

    for (PieceType type : { PieceType::Bishop, PieceType::Rook, PieceType::Queen }) {
        if (pieceMask & (1 << static_cast<int>(type)))
            generate_sliding_moves(moves, type);

        if (finished(moves)) return;
    }
}

void MoveGenerator::generate_king_moves(MoveList& moves) {
    bitboard_t targets = Attacks::get_king_attacks(m_friendlyKingSquare) & m_moveMask & ~m_opponentAttackMap;

    while (targets != 0) {
        moves.add(Move::create(m_friendlyKingSquare, Bitboard::pop(targets)));
        if (finished(moves)) return;
    }
}

void MoveGenerator::generate_castling_moves(MoveList& moves) {
    if (m_friendlyKingSquare != Square::relative_square(Square::E1, m_friendlyColor)) return;

    const GameState& state = m_board->current_game_state();
    for (bool kingside : { true, false }) {
        if (!(kingside ? state.has_kingside_castle_right(m_friendlyColor) :
            state.has_queenside_castle_right(m_friendlyColor))) continue;

        square_t rookSquare = Square::relative_square(kingside ? Square::H1 : Square::A1, m_friendlyColor);
        if (m_board->get_square(rookSquare) != Piece::create(PieceType::Rook, m_friendlyColor)) continue;

        bitboard_t emptyPath = squares_between(m_friendlyKingSquare, rookSquare) & ~Square::bitboard(rookSquare);
        if (emptyPath & m_allPieces) continue;

        square_t kingTarget = Square::castling_king_square(kingside, m_friendlyColor);
        if (squares_between(m_friendlyKingSquare, kingTarget) & m_opponentAttackMap) continue;

        moves.add(Move::create<Move::Type::Castling>(m_friendlyKingSquare, rookSquare));
        if (finished(moves)) return;
    }
}

void MoveGenerator::generate_knight_moves(MoveList& moves) {
    bitboard_t knights = friendly_piece_board(PieceType::Knight) & ~(m_diagonalPinRays | m_orthogonalPinRays);

    while (knights != 0) {
        square_t startSquare = Bitboard::pop(knights);
        bitboard_t targets = Attacks::get_knight_attacks(startSquare) & m_moveMask & m_checkRayBitmask;

        while (targets != 0) {
            moves.add(Move::create(startSquare, Bitboard::pop(targets)));
            if (finished(moves)) return;
        }
    }
}

void MoveGenerator::generate_sliding_moves(MoveList& moves, PieceType pieceType) {
    bitboard_t pieces = friendly_piece_board(pieceType);
    while (pieces != 0) {
        square_t startSquare = Bitboard::pop(pieces);
        bitboard_t startBit = Square::bitboard(startSquare);

        bitboard_t targets = 0;
        if (pieceType != PieceType::Rook && !(startBit & m_orthogonalPinRays)) {
            bitboard_t diagonalMoves = Attacks::get_bishop_attacks(startSquare, m_allPieces);
            if (startBit & m_diagonalPinRays) diagonalMoves &= m_diagonalPinRays;
            targets |= diagonalMoves;
        }

        if (pieceType != PieceType::Bishop && !(startBit & m_diagonalPinRays)) {
            bitboard_t orthogonalMoves = Attacks::get_rook_attacks(startSquare, m_allPieces);
            if (startBit & m_orthogonalPinRays) orthogonalMoves &= m_orthogonalPinRays;
            targets |= orthogonalMoves;
        }

        targets &= m_moveMask & m_checkRayBitmask;
        while (targets != 0) {
            moves.add(Move::create(startSquare, Bitboard::pop(targets)));
            if (finished(moves)) return;
        }
    }
}

void MoveGenerator::generate_promotions(MoveList& moves, square_t startSquare, square_t targetSquare) {
    for (PieceType type : { PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight }) {
        if (type != PieceType::Queen && (m_promotionMode == PromotionMode::QueenOnly ||
            (m_promotionMode == PromotionMode::QueenAndKnight && type != PieceType::Knight))) continue;

        moves.add(Move::create<Move::Type::Promotion>(startSquare, targetSquare, type));

        if (finished(moves)) return;
    }
}

void MoveGenerator::generate_pawn_moves(MoveList& moves) {
    int pushOffset = m_friendlyColor == PieceColor::White ? 8 : -8;
    int promotionRank = m_friendlyColor == PieceColor::White ? 7 : 0;
    int startingRank = m_friendlyColor == PieceColor::White ? 1 : 6;

    bitboard_t pawns = friendly_piece_board(PieceType::Pawn);
    while (pawns != 0) {
        square_t startSquare = Bitboard::pop(pawns);
        bitboard_t startBit = Square::bitboard(startSquare);

        if (m_generationMode != GenerationMode::QuietOnly && !(startBit & m_orthogonalPinRays)) {
            bitboard_t captures = Attacks::get_pawn_attacks(startSquare, m_friendlyColor) &
                m_enemyPieces & ~enemy_piece_board(PieceType::King) & m_checkRayBitmask;

            if (startBit & m_diagonalPinRays) captures &= m_diagonalPinRays;

            while (captures != 0) {
                square_t targetSquare = Bitboard::pop(captures);
                if (Square::rank(targetSquare) == promotionRank)
                    generate_promotions(moves, startSquare, targetSquare);
                else
                    moves.add(Move::create(startSquare, targetSquare));

                if (finished(moves)) return;
            }
        }
        if (m_generationMode == GenerationMode::CapturesOnly || (startBit & m_diagonalPinRays)) continue;

        int singleTarget = startSquare + pushOffset;
        if (!Square::is_valid(singleTarget) || !Piece::is_empty(m_board->get_square(static_cast<square_t>(singleTarget))))
            continue;

        square_t targetSquare = static_cast<square_t>(singleTarget);
        bitboard_t targetBit = Square::bitboard(targetSquare);

        if (m_generationMode == GenerationMode::Tactical && Square::rank(targetSquare) != promotionRank) continue;

        if ((startBit & m_orthogonalPinRays) && !(targetBit & m_orthogonalPinRays)) continue;

        if (targetBit & m_checkRayBitmask) {
            if (Square::rank(targetSquare) == promotionRank)
                generate_promotions(moves, startSquare, targetSquare);
            else
                moves.add(Move::create(startSquare, targetSquare));

            if (finished(moves)) return;
        }
        if (Square::rank(startSquare) == startingRank) {
            square_t doubleTarget = static_cast<square_t>(singleTarget + pushOffset);

            if (Piece::is_empty(m_board->get_square(doubleTarget)) &&
                (Square::bitboard(doubleTarget) & m_checkRayBitmask)) {
                moves.add(Move::create(startSquare, doubleTarget));

                if (finished(moves)) return;
            }
        }
    }
    if (m_generationMode != GenerationMode::QuietOnly)
        generate_en_passant_moves(moves, m_board->current_game_state().enPassantSquare);
}

void MoveGenerator::generate_en_passant_moves(MoveList& moves, square_t targetSquare) {
    if (!Square::is_valid(targetSquare) ||
        Square::rank(targetSquare) != (m_friendlyColor == PieceColor::White ? 5 : 2) ||
        !Piece::is_empty(m_board->get_square(targetSquare)) || in_double_check()) return;

    square_t capturedSquare = Square::ep_square(targetSquare);

    if (m_board->get_square(capturedSquare) != Piece::create(PieceType::Pawn, m_opponentColor)) return;
    if (!(m_checkRayBitmask & (Square::bitboard(capturedSquare) | Square::bitboard(targetSquare)))) return;

    bitboard_t candidates = Attacks::get_pawn_attacks(targetSquare, m_opponentColor) &
        friendly_piece_board(PieceType::Pawn) & ~m_orthogonalPinRays;

    while (candidates != 0) {
        square_t startSquare = Bitboard::pop(candidates);
        if ((Square::bitboard(startSquare) & m_diagonalPinRays) &&
            !(Square::bitboard(targetSquare) & m_diagonalPinRays)) continue;

        if (is_en_passant_legal(startSquare, targetSquare)) {
            moves.add(Move::create<Move::Type::EnPassant>(startSquare, targetSquare));
            if (finished(moves)) return;
        }
    }
}

bool MoveGenerator::is_en_passant_legal(square_t startSquare, square_t targetSquare) const {
    square_t capturedSquare = Square::ep_square(targetSquare);
    bitboard_t occupied = (m_allPieces &
        ~(Square::bitboard(startSquare) | Square::bitboard(capturedSquare))) | Square::bitboard(targetSquare);

    bitboard_t enemyPawns = enemy_piece_board(PieceType::Pawn) & ~Square::bitboard(capturedSquare);

    return !(Attacks::get_pawn_attacks(m_friendlyKingSquare, m_friendlyColor) & enemyPawns) &&
        !(Attacks::get_knight_attacks(m_friendlyKingSquare) & enemy_piece_board(PieceType::Knight)) &&
        !(Attacks::get_king_attacks(m_friendlyKingSquare) & enemy_piece_board(PieceType::King)) &&
        !(Attacks::get_rook_attacks(m_friendlyKingSquare, occupied) & m_board->enemy_orthogonal_sliders()) &&
        !(Attacks::get_bishop_attacks(m_friendlyKingSquare, occupied) & m_board->enemy_diagonal_sliders());
}

}
