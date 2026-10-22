#pragma once

#include <array>
#include <cstdint>

#include "core/board.hpp"
#include "movegen/movelist.hpp"

namespace chess {

class MoveGenerator {
    public:
        enum class GenerationMode { All, CapturesOnly, QuietOnly };
        enum class PromotionMode { All, QueenOnly, QueenAndKnight };

        enum PieceFilter : uint8_t {
            King = 1 << static_cast<int>(PieceType::King),
            Pawn = 1 << static_cast<int>(PieceType::Pawn),
            Knight = 1 << static_cast<int>(PieceType::Knight),
            Bishop = 1 << static_cast<int>(PieceType::Bishop),
            Rook = 1 << static_cast<int>(PieceType::Rook),
            Queen = 1 << static_cast<int>(PieceType::Queen),
            AllPieces = King | Pawn | Knight | Bishop | Rook | Queen
        };

        MoveList generate_moves(const Board& board, GenerationMode mode = GenerationMode::All, uint8_t pieceMask = AllPieces);
        void generate_moves(const Board& board, MoveList& moves, GenerationMode mode = GenerationMode::All, uint8_t pieceMask = AllPieces);
        MoveList generate_moves(const Board& board, bool capturesOnly);
        bool has_legal_moves(const Board& board, GenerationMode mode = GenerationMode::All, uint8_t pieceMask = AllPieces);
        bool is_legal(const Board& board, move_t move);
        bool is_en_passant_square_valid(const Board& board, square_t targetSquare);

        void set_promotion_mode(PromotionMode mode);
        // These describe the position from the most recent generation/query.
        bool in_check() const;
        bool in_double_check() const;
        bitboard_t opponent_attack_map() const;
        // Excludes startSquare, includes targetSquare (also for unaligned pairs).
        static bitboard_t squares_between(square_t startSquare, square_t targetSquare);

    private:
        using between_table_t = std::array<std::array<bitboard_t, 64>, 64>;
        static constexpr between_table_t create_squares_between();
        static const between_table_t SquaresBetween;

        void initialize(const Board& board, GenerationMode mode, bool oneMoveOnly);
        void calculate_attack_data();
        bitboard_t calculate_pin_rays(bool orthogonal) const;
        void calculate_opponent_attack_map();
        void generate_all_moves(MoveList& moves, uint8_t pieceMask);
        void generate_king_moves(MoveList& moves);
        void generate_castling_moves(MoveList& moves);
        void generate_pawn_moves(MoveList& moves);
        void generate_knight_moves(MoveList& moves);
        void generate_sliding_moves(MoveList& moves, PieceType pieceType);
        void generate_promotions(MoveList& moves, square_t startSquare, square_t targetSquare);
        void generate_en_passant_moves(MoveList& moves, square_t targetSquare);
        bool is_en_passant_legal(square_t startSquare, square_t targetSquare) const;
        bool finished(const MoveList& moves) const;
        bitboard_t friendly_piece_board(PieceType type) const;
        bitboard_t enemy_piece_board(PieceType type) const;

        const Board* m_board = nullptr;
        GenerationMode m_generationMode = GenerationMode::All;
        PromotionMode m_promotionMode = PromotionMode::All;
        bool m_oneMoveOnly = false;
        PieceColor m_friendlyColour = PieceColor::White;
        PieceColor m_opponentColour = PieceColor::Black;
        square_t m_friendlyKingSquare = Square::None;
        int m_checkCount = 0;
        bitboard_t m_checkRayBitmask = 0;
        bitboard_t m_orthogonalPinRays = 0;
        bitboard_t m_diagonalPinRays = 0;
        bitboard_t m_opponentAttackMap = 0;
        bitboard_t m_friendlyPieces = 0;
        bitboard_t m_enemyPieces = 0;
        bitboard_t m_allPieces = 0;
        bitboard_t m_moveMask = 0;
};

}
