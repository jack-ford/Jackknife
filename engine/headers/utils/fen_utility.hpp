#pragma once

#include <array>
#include <string>
#include <string_view>

#include "core/piece.hpp"
#include "core/square.hpp"

namespace chess {

class Board;

namespace FenUtility {

    inline constexpr std::string_view startPositionFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    struct PositionInfo {
        explicit PositionInfo(std::string_view fen);

        std::string m_fen;
        std::array<piece_t, Square::Count> m_squares {};
        bool m_whiteCastleKingside = false;
        bool m_whiteCastleQueenside = false;
        bool m_blackCastleKingside = false;
        bool m_blackCastleQueenside = false;
        int m_epFile = 0;
        bool m_whiteToMove = true;
        int m_fiftyMovePlyCount = 0;
        int m_moveCount = 1;
    };

    
    PositionInfo position_from_fen(std::string_view fen);
   
    std::string current_fen(const Board& board, bool alwaysIncludeEPSquare = true);
    bool en_passant_can_be_captured(const Board& board);
    std::string flip_fen(std::string_view fen);

}

}
