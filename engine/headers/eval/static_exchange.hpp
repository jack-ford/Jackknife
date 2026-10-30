#pragma once

#include "core/board.hpp"

namespace eval {

inline constexpr std::array<chess::PieceType, 6> ExchangeOrder {
    chess::PieceType::Pawn, chess::PieceType::Knight, chess::PieceType::Bishop,
    chess::PieceType::Rook, chess::PieceType::Queen, chess::PieceType::King
};

class ExchangePosition {
    public:
        explicit ExchangePosition(const chess::Board& board);
        int score(chess::move_t move, chess::PieceColor color);

    private:
        chess::bitboard_t attackers(chess::square_t square, chess::PieceColor color) const;

        void remove(chess::piece_t piece, chess::square_t square);
        void place(chess::piece_t piece, chess::square_t square);
        
        std::array<chess::bitboard_t, 14> m_pieces;
        chess::bitboard_t m_occupied;
        std::array<chess::square_t, 2> m_kings;
};

int static_exchange(const chess::Board& board, chess::move_t move);

}
