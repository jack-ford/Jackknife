#include "eval/static_exchange.hpp"

#include <algorithm>
#include "eval/evaluation.hpp"
#include "movegen/attacks.hpp"

namespace eval {

using namespace chess;

ExchangePosition::ExchangePosition(const Board& board)
    : m_pieces(board.piece_bitboards()), m_occupied(board.all_pieces_bitboard()),
      m_kings {board.king_square(PieceColor::Black), board.king_square(PieceColor::White)} {}

void ExchangePosition::remove(piece_t piece, square_t square) {
    m_pieces[piece] &= ~Square::bitboard(square);
    m_occupied &= ~Square::bitboard(square);
}

void ExchangePosition::place(piece_t piece, square_t square) {
    m_pieces[piece] |= Square::bitboard(square);
    m_occupied |= Square::bitboard(square);
}

bitboard_t ExchangePosition::attackers(square_t square, PieceColor color) const {
    auto pieces = [&](PieceType type) { return m_pieces[Piece::create(type, color)]; };
    PieceColor opponent = static_cast<PieceColor>(!static_cast<bool>(color));
    return (Attacks::get_pawn_attacks(square, opponent) & pieces(PieceType::Pawn)) |
        (Attacks::get_knight_attacks(square) & pieces(PieceType::Knight)) |
        (Attacks::get_bishop_attacks(square, m_occupied) & (pieces(PieceType::Bishop) | pieces(PieceType::Queen))) |
        (Attacks::get_rook_attacks(square, m_occupied) & (pieces(PieceType::Rook) | pieces(PieceType::Queen))) |
        (Attacks::get_king_attacks(square) & pieces(PieceType::King));
}

int ExchangePosition::score(move_t move, PieceColor color) {
    if (Move::type(move) == Move::Type::Castling)
        return 0;

    square_t from = Move::from(move);
    square_t target = Move::to(move);
    square_t victimSquare = Move::type(move) == Move::Type::EnPassant ? Square::ep_square(target) : target;
    piece_t moving = 0;
    piece_t victim = 0;

    for (piece_t piece = 2; piece < m_pieces.size(); ++piece) {
        if (m_pieces[piece] & Square::bitboard(from))
            moving = piece;
        if (m_pieces[piece] & Square::bitboard(victimSquare))
            victim = piece;
    }

    std::array<int, Square::Count> gains {};
    gains[0] = eval::piece_value(Piece::type(victim));

    remove(moving, from);

    if (victim != 0)
        remove(victim, victimSquare);

    if (Move::type(move) == Move::Type::Promotion) {
        moving = Piece::create(Move::promotion_piece(move), color);
        gains[0] += eval::piece_value(Piece::type(moving)) - eval::piece_value(PieceType::Pawn);
    }
    place(moving, target);

    if (Piece::type(moving) == PieceType::King)
        m_kings[static_cast<int>(color)] = target;

    int depth = 0;
    while (Piece::type(moving) != PieceType::King && depth < Square::Count - 1) {
        color = static_cast<PieceColor>(!static_cast<bool>(color));
        PieceColor opponent = static_cast<PieceColor>(!static_cast<bool>(color));
        bitboard_t candidates = attackers(target, color);
        bool found = false;
        for (PieceType type : ExchangeOrder) {
            piece_t piece = Piece::create(type, color);
            bitboard_t origins = candidates & m_pieces[piece];

            while (origins != 0) {
                square_t origin = Bitboard::pop(origins);
                piece_t promoted = type == PieceType::Pawn && (Square::rank(target) == 0 || Square::rank(target) == 7)
                    ? Piece::create(PieceType::Queen, color) : piece;

                square_t king = m_kings[static_cast<int>(color)];
                remove(moving, target);
                remove(piece, origin);
                place(promoted, target);

                if (type == PieceType::King)
                    m_kings[static_cast<int>(color)] = target;

                if (attackers(m_kings[static_cast<int>(color)], opponent) == 0) {
                    depth++;

                    gains[depth] = eval::piece_value(Piece::type(moving)) - gains[depth - 1] +
                        eval::piece_value(Piece::type(promoted)) - eval::piece_value(type);

                    moving = promoted;
                    found = true;

                    break;
                }

                m_kings[static_cast<int>(color)] = king;
                remove(promoted, target);
                place(piece, origin);
                place(moving, target);
            }

            if (found)
                break;
        }
        if (!found)
            break;
    }

    while (depth > 0) {
        gains[depth - 1] = std::min(gains[depth - 1], -gains[depth]);
        depth--;
    }

    return gains[0];
}

int static_exchange(const Board& board, move_t move) {
    return ExchangePosition(board).score(move, board.move_color());
}

}
