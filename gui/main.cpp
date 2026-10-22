#include <array>
#include <cmath>
#include <string>

#include "core/board.hpp"
#include "movegen/movegen.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

using namespace chess;

namespace {

constexpr int WindowWidth = 1920;
constexpr int WindowHeight = 1080;
constexpr float Scale = WindowWidth / 1280.0f;
constexpr float SquareSize = 88.0f * Scale;
constexpr float PieceSize = 88.0f * Scale;
constexpr float BoardSize = SquareSize * 8.0f;
constexpr float BoardX = (WindowWidth - BoardSize) / 2.0f;
constexpr float BoardY = (WindowHeight - BoardSize) / 2.0f;
constexpr float DragDistance = 5.0f * Scale;
constexpr float PromotionSize = 104.0f * Scale;
constexpr float PromotionGap = 8.0f * Scale;
constexpr float PromotionX = BoardX + BoardSize + (WindowWidth - BoardX - BoardSize - PromotionSize) / 2.0f;
constexpr float PromotionY = (WindowHeight - PromotionSize * 4.0f - PromotionGap * 3.0f) / 2.0f;

constexpr SDL_Color Background = {255, 255, 255, 255};
constexpr SDL_Color LightSquare = {240, 217, 181, 255};
constexpr SDL_Color DarkSquare = {181, 136, 99, 255};
constexpr SDL_Color SelectedSquare = {246, 246, 105, 255};
constexpr SDL_Color LastMoveLight = {205, 210, 106, 255};
constexpr SDL_Color LastMoveDark = {170, 162, 86, 255};
constexpr SDL_Color CheckSquare = {220, 82, 82, 255};
constexpr SDL_Color LegalMove = {45, 55, 45, 105};
constexpr SDL_Color PromotionBackground = {225, 225, 225, 255};
constexpr SDL_Color PromotionBorder = {75, 75, 75, 255};

constexpr std::array<PieceType, 4> PromotionPieces = {
    PieceType::Queen,
    PieceType::Rook,
    PieceType::Bishop,
    PieceType::Knight
};

struct Position {
    float x;
    float y;
};

struct SpriteSheet {
    SDL_Texture* texture;
    float width;
    float height;
};

struct PendingPromotion {
    MoveList moves;
    PieceColor color = PieceColor::White;
    bool active = false;
};

void set_color(SDL_Renderer* renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

square_t square_at(Position position) {
    if (position.x < BoardX || position.x >= BoardX + BoardSize ||
        position.y < BoardY || position.y >= BoardY + BoardSize)
        return Square::None;

    int file = static_cast<int>((position.x - BoardX) / SquareSize);
    int rank = 7 - static_cast<int>((position.y - BoardY) / SquareSize);
    return Square::index(rank, file);
}

Position square_position(square_t square) {
    return {
        BoardX + Square::file(square) * SquareSize,
        BoardY + (7 - Square::rank(square)) * SquareSize
    };
}

int piece_column(piece_t piece) {
    switch (Piece::type(piece)) {
        case PieceType::King: return 0;
        case PieceType::Queen: return 1;
        case PieceType::Bishop: return 2;
        case PieceType::Knight: return 3;
        case PieceType::Rook: return 4;
        case PieceType::Pawn: return 5;
        default: return 0;
    }
}

void draw_piece(SDL_Renderer* renderer, const SpriteSheet& sprites,
    piece_t piece, SDL_FRect destination) {
    float spriteWidth = sprites.width / 6.0f;
    float spriteHeight = sprites.height / 2.0f;
    int row = Piece::color(piece) == PieceColor::White ? 0 : 1;
    SDL_FRect source = {
        piece_column(piece) * spriteWidth,
        row * spriteHeight,
        spriteWidth,
        spriteHeight
    };

    SDL_RenderTexture(renderer, sprites.texture, &source, &destination);
}

square_t displayed_destination(move_t move, PieceColor color) {
    if (Move::type(move) != Move::Type::Castling)
        return Move::to(move);

    return Square::castling_king_square(Move::to(move) > Move::from(move), color);
}

MoveList moves_from(const MoveList& legalMoves, square_t square) {
    MoveList moves;

    for (move_t move : legalMoves) {
        if (Move::from(move) == square)
            moves.add(move);
    }

    return moves;
}

MoveList moves_to(const MoveList& selectedMoves, square_t square, PieceColor color) {
    MoveList moves;

    for (move_t move : selectedMoves) {
        if (displayed_destination(move, color) == square)
            moves.add(move);
    }

    return moves;
}

bool can_select(const Board& board, square_t square) {
    if (square == Square::None)
        return false;

    piece_t piece = board.get_square(square);
    return !Piece::isEmpty(piece) && Piece::color(piece) == board.move_colour();
}

bool is_capture(const Board& board, move_t move) {
    if (Move::type(move) == Move::Type::EnPassant)
        return true;
    if (Move::type(move) == Move::Type::Castling)
        return false;
    return !Piece::isEmpty(board.get_square(Move::to(move)));
}

SDL_Color square_color(square_t square, square_t selected,
    square_t lastFrom, square_t lastTo, square_t checkedKing) {
    bool light = Square::is_light(square);

    if (square == checkedKing)
        return CheckSquare;
    if (square == selected)
        return SelectedSquare;
    if (square == lastFrom || square == lastTo)
        return light ? LastMoveLight : LastMoveDark;
    return light ? LightSquare : DarkSquare;
}

void draw_filled_circle(SDL_Renderer* renderer, float centerX, float centerY, float radius) {
    int extent = static_cast<int>(radius);

    for (int y = -extent; y <= extent; y++) {
        float width = std::sqrt(radius * radius - static_cast<float>(y * y));
        SDL_RenderLine(renderer, centerX - width, centerY + y, centerX + width, centerY + y);
    }
}

void draw_legal_moves(SDL_Renderer* renderer, const Board& board,
    const MoveList& selectedMoves, PieceColor color) {
    uint64_t drawn = 0;
    set_color(renderer, LegalMove);

    for (move_t move : selectedMoves) {
        square_t square = displayed_destination(move, color);
        uint64_t mask = Square::bitboard(square);

        if ((drawn & mask) != 0)
            continue;

        drawn |= mask;
        Position position = square_position(square);

        if (is_capture(board, move)) {
            float inset = SquareSize * 0.06f;
            float thickness = SquareSize * 0.08f;
            SDL_FRect top = {position.x + inset, position.y + inset, SquareSize - inset * 2.0f, thickness};
            SDL_FRect bottom = {position.x + inset, position.y + SquareSize - inset - thickness, SquareSize - inset * 2.0f, thickness};
            SDL_FRect left = {position.x + inset, position.y + inset, thickness, SquareSize - inset * 2.0f};
            SDL_FRect right = {position.x + SquareSize - inset - thickness, position.y + inset, thickness, SquareSize - inset * 2.0f};
            SDL_RenderFillRect(renderer, &top);
            SDL_RenderFillRect(renderer, &bottom);
            SDL_RenderFillRect(renderer, &left);
            SDL_RenderFillRect(renderer, &right);
        }
        else {
            draw_filled_circle(renderer, position.x + SquareSize / 2.0f,
                position.y + SquareSize / 2.0f, SquareSize * 0.13f);
        }
    }
}

void draw_board(SDL_Renderer* renderer, const SpriteSheet& sprites,
    const Board& board, const MoveList& selectedMoves, square_t selected,
    square_t lastFrom, square_t lastTo, bool dragging, Position mouse) {
    square_t checkedKing = board.is_in_check() ? board.king_square(board.move_colour()) : Square::None;

    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            square_t square = Square::index(rank, file);
            Position position = square_position(square);
            SDL_FRect rectangle = {position.x, position.y, SquareSize, SquareSize};

            set_color(renderer, square_color(square, selected, lastFrom, lastTo, checkedKing));
            SDL_RenderFillRect(renderer, &rectangle);
        }
    }

    draw_legal_moves(renderer, board, selectedMoves, board.move_colour());

    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            square_t square = Square::index(rank, file);
            piece_t piece = board.get_square(square);

            if (Piece::isEmpty(piece) || (dragging && square == selected))
                continue;

            Position position = square_position(square);
            SDL_FRect destination = {
                position.x + (SquareSize - PieceSize) / 2.0f,
                position.y + (SquareSize - PieceSize) / 2.0f,
                PieceSize,
                PieceSize
            };
            draw_piece(renderer, sprites, piece, destination);
        }
    }

    if (dragging && selected != Square::None) {
        piece_t piece = board.get_square(selected);
        SDL_FRect destination = {
            mouse.x - PieceSize / 2.0f,
            mouse.y - PieceSize / 2.0f,
            PieceSize,
            PieceSize
        };
        draw_piece(renderer, sprites, piece, destination);
    }
}

SDL_FRect promotion_rectangle(int index) {
    return {
        PromotionX,
        PromotionY + index * (PromotionSize + PromotionGap),
        PromotionSize,
        PromotionSize
    };
}

void draw_promotion_choices(SDL_Renderer* renderer, const SpriteSheet& sprites,
    const PendingPromotion& promotion) {
    if (!promotion.active)
        return;

    for (int index = 0; index < static_cast<int>(PromotionPieces.size()); index++) {
        SDL_FRect rectangle = promotion_rectangle(index);
        set_color(renderer, PromotionBorder);
        SDL_RenderFillRect(renderer, &rectangle);

        float inset = 3.0f * Scale;
        SDL_FRect inside = {
            rectangle.x + inset,
            rectangle.y + inset,
            rectangle.w - inset * 2.0f,
            rectangle.h - inset * 2.0f
        };
        set_color(renderer, PromotionBackground);
        SDL_RenderFillRect(renderer, &inside);

        float pieceInset = 8.0f * Scale;
        SDL_FRect pieceRectangle = {
            rectangle.x + pieceInset,
            rectangle.y + pieceInset,
            rectangle.w - pieceInset * 2.0f,
            rectangle.h - pieceInset * 2.0f
        };
        draw_piece(renderer, sprites, Piece::create(PromotionPieces[index], promotion.color), pieceRectangle);
    }
}

move_t promotion_at(Position position, const PendingPromotion& promotion) {
    if (!promotion.active)
        return 0;

    for (int index = 0; index < static_cast<int>(PromotionPieces.size()); index++) {
        SDL_FRect rectangle = promotion_rectangle(index);

        if (position.x < rectangle.x || position.x >= rectangle.x + rectangle.w ||
            position.y < rectangle.y || position.y >= rectangle.y + rectangle.h)
            continue;

        for (move_t move : promotion.moves) {
            if (Move::promotion_piece(move) == PromotionPieces[index])
                return move;
        }
    }

    return 0;
}

void update_title(SDL_Window* window, const Board& board, const MoveList& legalMoves,
    bool choosingPromotion) {
    std::string title = "Chess Engine Debug Board - ";

    if (choosingPromotion) {
        title += "Choose a promotion piece";
    }
    else if (legalMoves.is_empty()) {
        if (board.is_in_check())
            title += board.is_white_to_move() ? "Checkmate - Black wins" : "Checkmate - White wins";
        else
            title += "Stalemate";
    }
    else {
        title += board.is_white_to_move() ? "White to move" : "Black to move";

        if (board.is_in_check())
            title += " - Check";
    }

    SDL_SetWindowTitle(window, title.c_str());
}

void clear_selection(square_t& selected, MoveList& selectedMoves) {
    selected = Square::None;
    selectedMoves.clear();
}

void select_square(const MoveList& legalMoves, square_t square,
    square_t& selected, MoveList& selectedMoves) {
    selected = square;
    selectedMoves = moves_from(legalMoves, square);
}

void commit_move(Board& board, MoveGenerator& generator, move_t move,
    MoveList& legalMoves, square_t& selected, MoveList& selectedMoves,
    square_t& lastFrom, square_t& lastTo) {
    PieceColor color = board.move_colour();
    lastFrom = Move::from(move);
    lastTo = displayed_destination(move, color);
    board.make_move(move);
    legalMoves = generator.generate_moves(board);
    clear_selection(selected, selectedMoves);
}

bool try_move(Board& board, MoveGenerator& generator, square_t destination,
    MoveList& legalMoves, square_t& selected, MoveList& selectedMoves,
    square_t& lastFrom, square_t& lastTo, PendingPromotion& promotion) {
    MoveList candidates = moves_to(selectedMoves, destination, board.move_colour());

    if (candidates.is_empty())
        return false;

    if (candidates.size() == 1) {
        commit_move(board, generator, candidates[0], legalMoves, selected,
            selectedMoves, lastFrom, lastTo);
    }
    else {
        promotion.moves = candidates;
        promotion.color = board.move_colour();
        promotion.active = true;
    }

    return true;
}

}

int main(int, char**) {
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d11");

    if (!SDL_Init(SDL_INIT_VIDEO))
        return 1;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    if (!SDL_CreateWindowAndRenderer("Chess Engine Debug Board", WindowWidth, WindowHeight,
        0, &window, &renderer)) {
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    std::string spritePath = std::string(SDL_GetBasePath()) + "chess_pieces.png";
    SpriteSheet sprites = {IMG_LoadTexture(renderer, spritePath.c_str()), 0, 0};

    if (sprites.texture == nullptr) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Chess Engine Debug Board",
            SDL_GetError(), window);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GetTextureSize(sprites.texture, &sprites.width, &sprites.height);
    SDL_SetTextureScaleMode(sprites.texture, SDL_SCALEMODE_LINEAR);

    Board board = Board();
    MoveGenerator generator;
    MoveList legalMoves = generator.generate_moves(board);
    MoveList selectedMoves;
    PendingPromotion promotion;
    square_t selected = Square::None;
    square_t lastFrom = Square::None;
    square_t lastTo = Square::None;
    Position mouse = {};
    Position pressPosition = {};
    square_t pressSquare = Square::None;
    bool mouseDown = false;
    bool dragging = false;
    bool selectedBeforePress = false;
    bool running = true;

    update_title(window, board, legalMoves, false);

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                mouse = {event.motion.x, event.motion.y};

                if (!promotion.active && mouseDown && pressSquare == selected && selected != Square::None) {
                    float dx = mouse.x - pressPosition.x;
                    float dy = mouse.y - pressPosition.y;
                    dragging = dx * dx + dy * dy >= DragDistance * DragDistance;
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_LEFT) {
                mouse = {event.button.x, event.button.y};
                pressPosition = mouse;
                mouseDown = true;
                dragging = false;

                if (promotion.active) {
                    pressSquare = Square::None;
                    selectedBeforePress = false;
                    continue;
                }

                pressSquare = square_at(mouse);
                selectedBeforePress = selected != Square::None;

                if (selected == Square::None && can_select(board, pressSquare))
                    select_square(legalMoves, pressSquare, selected, selectedMoves);
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                event.button.button == SDL_BUTTON_LEFT) {
                mouse = {event.button.x, event.button.y};

                if (promotion.active) {
                    move_t move = promotion_at(mouse, promotion);

                    if (move != 0) {
                        promotion.active = false;
                        commit_move(board, generator, move, legalMoves, selected,
                            selectedMoves, lastFrom, lastTo);
                        update_title(window, board, legalMoves, false);
                    }

                    mouseDown = false;
                    dragging = false;
                    continue;
                }

                square_t destination = square_at(mouse);
                bool handled = false;

                if (selected != Square::None && dragging && pressSquare == selected) {
                    if (destination != selected)
                        handled = try_move(board, generator, destination, legalMoves,
                            selected, selectedMoves, lastFrom, lastTo, promotion);

                    if (!handled)
                        clear_selection(selected, selectedMoves);
                }
                else if (selected != Square::None && selectedBeforePress &&
                    pressSquare == destination) {
                    if (destination == selected) {
                        clear_selection(selected, selectedMoves);
                        handled = true;
                    }
                    else {
                        handled = try_move(board, generator, destination, legalMoves,
                            selected, selectedMoves, lastFrom, lastTo, promotion);

                        if (!handled && can_select(board, destination)) {
                            select_square(legalMoves, destination, selected, selectedMoves);
                            handled = true;
                        }
                    }
                }

                if (!handled && selectedBeforePress && pressSquare == destination &&
                    destination != Square::None)
                    clear_selection(selected, selectedMoves);

                update_title(window, board, legalMoves, promotion.active);
                mouseDown = false;
                dragging = false;
                selectedBeforePress = false;
                pressSquare = Square::None;
            }
        }

        set_color(renderer, Background);
        SDL_RenderClear(renderer);
        draw_board(renderer, sprites, board, selectedMoves, selected, lastFrom,
            lastTo, dragging, mouse);
        draw_promotion_choices(renderer, sprites, promotion);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(sprites.texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
