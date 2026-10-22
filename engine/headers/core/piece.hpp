#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>

namespace chess {

using piece_t = uint8_t;

enum class PieceType : piece_t {
    None   = 0,
    King   = 1,
    Pawn   = 2,
    Knight = 3,
    Bishop = 4,
    Rook   = 5,
    Queen  = 6
};

enum class PieceColor : bool {
    White = true,
    Black = false
};

namespace Piece {

    constexpr piece_t create(PieceType type, PieceColor color) {
        if (type == PieceType::None)
            return 0;

        return (static_cast<piece_t>(type) << 1) | static_cast<piece_t>(color);
    }

    constexpr piece_t create(piece_t type, bool color) {
        return create(static_cast<PieceType>(type), static_cast<PieceColor>(color));
    }

    constexpr PieceType type(piece_t piece) {
        return static_cast<PieceType>(piece >> 1);
    }

    constexpr PieceColor color(piece_t piece) {
        return static_cast<PieceColor>(piece & 1);
    }

    constexpr bool isEmpty(piece_t piece) {
        return piece == 0;
    }

    inline constexpr std::array<char, 14> symbols = {
        '.', '.',
        'k', 'K',
        'p', 'P',
        'n', 'N',
        'b', 'B',
        'r', 'R',
        'q', 'Q'
    };

    constexpr char symbol(piece_t piece) {
        return piece < symbols.size() ? symbols[piece] : '?';
    }

    constexpr piece_t create(char symbol) {
        switch (symbol) {
            case ' ': return 0;
            case 'k': return 2;
            case 'K': return 3;
            case 'p': return 4;
            case 'P': return 5;
            case 'n': return 6;
            case 'N': return 7;
            case 'b': return 8;
            case 'B': return 9;
            case 'r': return 10;
            case 'R': return 11;
            case 'q': return 12;
            case 'Q': return 13;
            default:
                throw std::invalid_argument("Invalid piece symbol");
        }
    }

    constexpr piece_t create_empty() {
        return 0;
    }
}

}
