#include "utils/fen_utility.hpp"

#include <charconv>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "core/board.hpp"

namespace chess::FenUtility {

PositionInfo::PositionInfo(std::string_view fen) : m_fen(fen) {
    std::istringstream input(m_fen);

    std::array<std::string, 6> sections;
    int sectionCount = 0;

    std::string section;
    while (input >> section) {
        if (sectionCount == 6)
            throw std::invalid_argument("FEN has too many fields");

        sections[sectionCount++] = section;
    }
    if (sectionCount < 3)
        throw std::invalid_argument("FEN requires placement, side to move, and castling rights");

    int rank = 7;
    int file = 0;
    for (char symbol : sections[0]) {
        if (symbol == '/') {
            if (file != 8 || rank == 0)
                throw std::invalid_argument("Invalid FEN rank");
            rank--;
            file = 0;
        } else if (symbol >= '1' && symbol <= '8') {
            file += symbol - '0';
            if (file > 8)
                throw std::invalid_argument("FEN rank exceeds eight squares");
        } else {
            if (file >= 8)
                throw std::invalid_argument("FEN rank exceeds eight squares");
            m_squares[Square::index(rank, file++)] = Piece::create(symbol);
        }
    }
    if (rank != 0 || file != 8)
        throw std::invalid_argument("FEN must describe eight complete ranks");

    if (sections[1] != "w" && sections[1] != "b")
        throw std::invalid_argument("Invalid FEN side to move");

    m_whiteToMove = sections[1] == "w";

    if (sections[2] != "-") {
        for (char symbol : sections[2]) {
            bool* right = nullptr;
            switch (symbol) {
                case 'K': right = &m_whiteCastleKingside; break;
                case 'Q': right = &m_whiteCastleQueenside; break;
                case 'k': right = &m_blackCastleKingside; break;
                case 'q': right = &m_blackCastleQueenside; break;
                default: throw std::invalid_argument("Invalid FEN castling rights");
            }
            if (*right)
                throw std::invalid_argument("Duplicate FEN castling right");

            *right = true;
        }
    }

    if (sectionCount > 3 && sections[3] != "-") {
        if (!Square::is_valid_string(sections[3]) || sections[3][1] != (m_whiteToMove ? '6' : '3'))
            throw std::invalid_argument("Invalid FEN en-passant square");

        m_epFile = sections[3][0] - 'a' + 1;
    }

    for (int index = 4; index < sectionCount; index++) {
        int value = 0;
        const std::string& counter = sections[index];

        auto result = std::from_chars(counter.data(), counter.data() + counter.size(), value);

        int minimum = index == 4 ? 0 : 1;
        int maximum = index == 4 ? std::numeric_limits<int>::max() - 1 : std::numeric_limits<int>::max() / 2;

        if (result.ec != std::errc() || result.ptr != counter.data() + counter.size() ||
            value < minimum || value > maximum)
            throw std::invalid_argument("Invalid FEN move counter");
        if (index == 4)
            m_fiftyMovePlyCount = value;
        else
            m_moveCount = value;
    }
}

PositionInfo position_from_fen(std::string_view fen) {
    return PositionInfo(fen);
}

std::string current_fen(const Board& board, bool alwaysIncludeEPSquare) {
    std::string fen;
    fen.reserve(90);

    for (int rank = 7; rank >= 0; rank--) {
        int numEmptyFiles = 0;
        for (int file = 0; file < 8; file++) {
            piece_t piece = board.get_square(Square::index(rank, file));
            if (Piece::isEmpty(piece)) {
                numEmptyFiles++;
            } else {
                if (numEmptyFiles != 0) {
                    fen += static_cast<char>('0' + numEmptyFiles);
                    numEmptyFiles = 0;
                }
                fen += Piece::symbol(piece);
            }
        }

        if (numEmptyFiles != 0)
            fen += static_cast<char>('0' + numEmptyFiles);
        if (rank != 0)
            fen += '/';
    }

    fen += board.is_white_to_move() ? " w " : " b ";

    const GameState& state = board.current_game_state();
    if (state.has_kingside_castle_right(PieceColor::White)) fen += 'K';
    if (state.has_queenside_castle_right(PieceColor::White)) fen += 'Q';
    if (state.has_kingside_castle_right(PieceColor::Black)) fen += 'k';
    if (state.has_queenside_castle_right(PieceColor::Black)) fen += 'q';
    if (state.castlingRights == 0) fen += '-';

    fen += ' ';

    if (state.enPassantSquare != Square::None &&
        (alwaysIncludeEPSquare || en_passant_can_be_captured(board)))
        fen += Square::to_string(state.enPassantSquare);
    else
        fen += '-';

    fen += " " + std::to_string(board.fifty_move_counter());
    fen += " " + std::to_string(board.ply_count() / 2 + 1);

    return fen;
}

bool en_passant_can_be_captured(const Board& board) {
    square_t targetSquare = board.current_game_state().enPassantSquare;
    return targetSquare != Square::None && board.is_en_passant_square_valid(targetSquare);
}

std::string flip_fen(std::string_view fen) {
    PositionInfo position = position_from_fen(fen);
    std::string flippedFen;

    for (int rank = 0; rank < 8; rank++) {
        int numEmptyFiles = 0;

        for (int file = 0; file < 8; file++) {
            piece_t piece = position.m_squares[Square::index(rank, file)];
            if (Piece::isEmpty(piece)) {
                numEmptyFiles++;
            } else {
                if (numEmptyFiles != 0) {
                    flippedFen += static_cast<char>('0' + numEmptyFiles);
                    numEmptyFiles = 0;
                }
                PieceColor colour = Piece::color(piece) == PieceColor::White ? PieceColor::Black : PieceColor::White;
                flippedFen += Piece::symbol(Piece::create(Piece::type(piece), colour));
            }
        }

        if (numEmptyFiles != 0)
            flippedFen += static_cast<char>('0' + numEmptyFiles);
        if (rank != 7)
            flippedFen += '/';
    }
    flippedFen += position.m_whiteToMove ? " b " : " w ";
    
    std::string castlingRights;
    if (position.m_blackCastleKingside) castlingRights += 'K';
    if (position.m_blackCastleQueenside) castlingRights += 'Q';
    if (position.m_whiteCastleKingside) castlingRights += 'k';
    if (position.m_whiteCastleQueenside) castlingRights += 'q';
    flippedFen += castlingRights.empty() ? "-" : castlingRights;

    flippedFen += ' ';

    if (position.m_epFile != 0)
        flippedFen += Square::to_string(Square::index(position.m_whiteToMove ? 2 : 5, position.m_epFile - 1));
    else
        flippedFen += '-';

    flippedFen += " " + std::to_string(position.m_fiftyMovePlyCount);
    flippedFen += " " + std::to_string(position.m_moveCount);

    return flippedFen;
}

}
