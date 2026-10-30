#pragma once

#include <array>
#include <string>
#include <string_view>

#include "core/piece.hpp"
#include "core/square.hpp"

namespace chess {

class Board;

namespace FenUtility {

    inline constexpr std::string_view StartPositionFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    struct PositionInfo {
        explicit PositionInfo(std::string_view fenText);

        std::string fen;
        std::array<piece_t, Square::Count> squares {};
        bool whiteCastleKingside = false;
        bool whiteCastleQueenside = false;
        bool blackCastleKingside = false;
        bool blackCastleQueenside = false;
        int epFile = 0;
        bool whiteToMove = true;
        int fiftyMovePlyCount = 0;
        int moveCount = 1;
    };


    PositionInfo position_from_fen(std::string_view fen);

    std::string current_fen(const Board& board, bool alwaysIncludeEpSquare = true);
    bool en_passant_can_be_captured(const Board& board);
    std::string flip_fen(std::string_view fen);

}

}
