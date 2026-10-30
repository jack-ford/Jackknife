#include "move_utility.hpp"

namespace chess {

std::string coordinate_move(move_t move) {
    square_t target = Move::to(move);

    if (Move::type(move) == Move::Type::Castling) {
        PieceColor color = Square::rank(Move::from(move)) == 0 ? PieceColor::White : PieceColor::Black;
        target = Square::castling_king_square(target > Move::from(move), color);
    }

    std::string result = Square::to_string(Move::from(move)) + Square::to_string(target);

    if (Move::type(move) == Move::Type::Promotion) {
        constexpr char Pieces[] = "   nbrq";
        result += Pieces[static_cast<int>(Move::promotion_piece(move))];
    }

    return result;
}

std::optional<move_t> parse_coordinate_move(const Board& board, std::string_view text) {
    MoveGenerator generator;
    for (move_t move : generator.generate_moves(board))
        if (coordinate_move(move) == text)
            return move;

    return std::nullopt;
}

std::optional<move_t> parse_san_move(const Board& board, std::string_view text) {
    while (!text.empty() && std::string_view("+#!?").find(text.back()) != std::string_view::npos)
        text.remove_suffix(1);

    MoveGenerator generator;
    MoveList legal = generator.generate_moves(board);

    if (text == "O-O" || text == "0-0" || text == "O-O-O" || text == "0-0-0") {
        bool kingside = text.size() == 3;
        for (move_t move : legal)
            if (Move::type(move) == Move::Type::Castling && (Move::to(move) > Move::from(move)) == kingside)
                return move;
        return std::nullopt;
    }

    auto piece_type = [](char symbol) {
        switch (symbol) {
            case 'N': return PieceType::Knight;
            case 'B': return PieceType::Bishop;
            case 'R': return PieceType::Rook;
            case 'Q': return PieceType::Queen;
            case 'K': return PieceType::King;
            default: return PieceType::None;
        }
    };

    PieceType piece = text.empty() ? PieceType::None : piece_type(text.front());
    if (piece != PieceType::None)
        text.remove_prefix(1);
    else
        piece = PieceType::Pawn;

    PieceType promotion = text.empty() ? PieceType::None : piece_type(text.back());
    if (promotion != PieceType::None) {
        text.remove_suffix(1);
        if (!text.empty() && text.back() == '=')
            text.remove_suffix(1);
    }

    if (text.size() < 2 || !Square::is_valid_string(text.substr(text.size() - 2)))
        return std::nullopt;

    square_t target = Square::from_string(text.substr(text.size() - 2));

    int fileHint = -1;
    int rankHint = -1;
    for (char symbol : text.substr(0, text.size() - 2)) {
        if (symbol >= 'a' && symbol <= 'h')
            fileHint = symbol - 'a';
        else if (symbol >= '1' && symbol <= '8')
            rankHint = symbol - '1';
        else if (symbol != 'x')
            return std::nullopt;
    }

    std::optional<move_t> found;
    for (move_t move : legal) {
        square_t from = Move::from(move);
        bool promotes = Move::type(move) == Move::Type::Promotion;
        
        if (Move::type(move) == Move::Type::Castling || Move::to(move) != target ||
            Piece::type(board.get_square(from)) != piece ||
            (fileHint >= 0 && Square::file(from) != fileHint) || (rankHint >= 0 && Square::rank(from) != rankHint) ||
            promotes != (promotion != PieceType::None) || (promotes && Move::promotion_piece(move) != promotion))
            continue;

        if (found)
            return std::nullopt;

        found = move;
    }
    return found;
}

}
