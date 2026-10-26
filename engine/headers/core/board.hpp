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
#include "eval/nnue/architecture.hpp"
#include "utils/fen_utility.hpp"
#include "utils/result_utility.hpp"

namespace chess {

namespace CastlingRights {
    inline constexpr uint8_t WhiteKingside = 1;
    inline constexpr uint8_t WhiteQueenside = 2;
    inline constexpr uint8_t BlackKingside = 4;
    inline constexpr uint8_t BlackQueenside = 8;

    constexpr uint8_t kingside(PieceColor color) {
        return color == PieceColor::White ? WhiteKingside : BlackKingside;
    }

    constexpr uint8_t queenside(PieceColor color) {
        return color == PieceColor::White ? WhiteQueenside : BlackQueenside;
    }

    constexpr uint8_t both(PieceColor color) {
        return kingside(color) | queenside(color);
    }
}


struct GameState {
    PieceType capturedPieceType = PieceType::None;
    // Raw FEN target; only legally capturable targets contribute to zobristKey.
    square_t enPassantSquare = Square::None;
    bool enPassantHashed = false;
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
    eval::nnue::Accumulator accumulator;
};

class Board final {
    public:
        // Black = 0, White = 1.
        static constexpr int WhiteIndex = static_cast<int>(PieceColor::White);
        static constexpr int BlackIndex = static_cast<int>(PieceColor::Black);

        // Construct the standard starting position.
        Board();
        explicit Board(std::string_view fen);

        // Requires a legal move. Search moves enter repetition history, but not game moves.
        void make_move(move_t move, bool inSearch = false);
        void unmake_move(move_t move, bool inSearch = false);
        void unmake_move();
        void make_null_move();
        void unmake_null_move();

        bool is_in_check() const;
        bool calculate_in_check_state() const;

        void load_start_position();
        // Rejects invalid piece/king counts, adjacent kings, back-rank pawns, and invalid EP geometry.
        // Rejected loads leave the board unchanged; historical reachability is not checked.
        void load_position(std::string_view fen);
        void load_position(const FenUtility::PositionInfo& positionInfo);

        // Result is relative to the side to move. Claimable draws are treated as claimed.
        std::pair<GameResult, GameResultReason> is_game_over() const;

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

        // Views are valid until mutation; piece-list order is unspecified.
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
        // Canonical position key: en passant is hashed only when a legal capture exists.
        // Not a Polyglot book key when the only candidate pawn is pinned.
        uint64_t zobrist_key() const;
        const GameState& current_game_state() const;
        // Active real/search positions since the last irreversible or null move.
        // Null positions themselves are excluded.
        std::span<const uint64_t> repetition_position_history() const;
        std::span<const move_t> all_game_moves() const;
        std::string current_fen() const;
        const std::string& game_start_fen() const;
        const eval::nnue::Accumulator& nnue_accumulator() const { return m_accumulator; }

        friend std::ostream& operator<<(std::ostream& output, const Board& board);
        friend bool FenUtility::en_passant_can_be_captured(const Board& board);

    private:

        explicit Board(std::nullptr_t);
        void initialize();
        void load_validated_position(const FenUtility::PositionInfo& positionInfo);
        void update_en_passant_hash();
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
        // At most 16 pieces of either color in a structurally valid position.
        std::array<std::array<square_t, 16>, 14> m_allPieceLists {};
        std::array<uint8_t, 14> m_pieceListCounts {};
        std::array<uint8_t, Square::Count> m_pieceListIndices {};

        bool m_isWhiteToMove = true;
        int m_plyCount = 0;
        GameState m_currentGameState;
        eval::nnue::Accumulator m_accumulator;
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
