#pragma once

#include <array>
#include <cstdint>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

#include "bitboard.hpp"
#include "move.hpp"
#include "piece.hpp"
#include "square.hpp"
#include "utils/fen_utility.hpp"
#include "utils/result_utility.hpp"

namespace chess {

struct GameState {
    PieceType capturedPieceType = PieceType::None;
    square_t enPassantSquare = Square::None;
    uint8_t castlingRights = 0; // White kingside/queenside, black kingside/queenside.
    int fiftyMoveCounter = 0;
    uint64_t zobristKey = 0;

    bool has_kingside_castle_right(PieceColor color) const;
    bool has_queenside_castle_right(PieceColor color) const;
};

struct HistoryEntry {
    GameState gameState;
    move_t move = 0;
    std::size_t repetitionStart = 0;
    bool inSearch = false;
    bool isNullMove = false;
};

class Board final {
    public:
        // Black = 0, White = 1.
        static constexpr int whiteIndex = static_cast<int>(PieceColor::White);
        static constexpr int blackIndex = static_cast<int>(PieceColor::Black);

        // Construct the standard starting position.
        Board();
        Board(std::string_view fen);

        void make_move(move_t move, bool inSearch = false);
        void unmake_move(move_t move, bool inSearch = false);
        void unmake_move();
        void make_null_move();
        void unmake_null_move();

        bool is_in_check() const;
        bool calculate_in_check_state() const;

        void load_start_position();
        void load_position(std::string_view fen);
        void load_position(const FenUtility::PositionInfo& positionInfo);

        // Result is relative to the side to move. Claimable draws are treated as claimed.
        std::pair<GameResult, GameResultReason> isGameOver() const;

        piece_t get_square(square_t square) const;
        const std::array<piece_t, Square::Count>& squares() const;
        square_t king_square(PieceColor color) const;
        const std::array<bitboard_t, 14>& piece_bitboards() const;
        const std::array<bitboard_t, 2>& colour_bitboards() const;
        bitboard_t all_pieces_bitboard() const;
        bitboard_t friendly_orthogonal_sliders() const;
        bitboard_t friendly_diagonal_sliders() const;
        bitboard_t enemy_orthogonal_sliders() const;
        bitboard_t enemy_diagonal_sliders() const;
        int total_piece_count_without_pawns_and_kings() const;

        std::span<const square_t> rooks(PieceColor color) const;
        std::span<const square_t> bishops(PieceColor color) const;
        std::span<const square_t> queens(PieceColor color) const;
        std::span<const square_t> knights(PieceColor color) const;
        std::span<const square_t> pawns(PieceColor color) const;

        bool is_white_to_move() const;
        PieceColor move_colour() const;
        PieceColor opponent_colour() const;
        int move_colour_index() const;
        int opponent_colour_index() const;
        int ply_count() const;
        int fifty_move_counter() const;
        uint64_t zobrist_key() const;
        const GameState& current_game_state() const;
        std::span<const uint64_t> repetition_position_history() const;
        std::span<const move_t> all_game_moves() const;
        std::string current_fen() const;
        const std::string& game_start_fen() const;

        friend std::ostream& operator<<(std::ostream& output, const Board& board);
        friend bool FenUtility::en_passant_can_be_captured(const Board& board);

    private:
    
        void initialize();
        void place_piece(piece_t piece, square_t square);
        void remove_piece(piece_t piece, square_t square);
        void move_piece(piece_t piece, square_t startSquare, square_t targetSquare);
        void update_slider_bitboards();
        void clear_rook_castle_right(square_t square, PieceColor color);
        bool is_en_passant_square_valid(square_t targetSquare) const;
        std::span<const square_t> piece_list(PieceType type, PieceColor color) const;

        std::array<piece_t, Square::Count> m_squares {};
        std::array<square_t, 2> m_kingSquare { Square::None, Square::None };
        std::array<bitboard_t, 14> m_pieceBitboards {};
        std::array<bitboard_t, 2> m_colourBitboards {};
        bitboard_t m_allPiecesBitboard = 0;
        bitboard_t m_friendlyOrthogonalSliders = 0;
        bitboard_t m_friendlyDiagonalSliders = 0;
        bitboard_t m_enemyOrthogonalSliders = 0;
        bitboard_t m_enemyDiagonalSliders = 0;
        int m_totalPieceCountWithoutPawnsAndKings = 0;
        std::array<std::vector<square_t>, 14> m_allPieceLists;

        bool m_isWhiteToMove = true;
        int m_plyCount = 0;
        GameState m_currentGameState;
        std::vector<HistoryEntry> m_gameStateHistory;
        std::vector<uint64_t> m_repetitionPositionHistory;
        // Retain the inactive prefix so undoing a capture/pawn move restores history.
        std::size_t m_repetitionStart = 0;
        std::vector<move_t> m_allGameMoves;
        std::string m_gameStartFen;
        mutable bool m_cachedInCheckValue = false;
        mutable bool m_hasCachedInCheckValue = false;
};

}
