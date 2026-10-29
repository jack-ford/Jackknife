#include <array>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <random>
#include <stop_token>
#include <string>

#include "constants.hpp"
#include "core/board.hpp"
#include "movegen/movegen.hpp"
#include "search/opening_book.hpp"
#include "search/search.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

using namespace chess;

namespace gui {

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

class EnginePlayer final {
    public:
        ~EnginePlayer() {
            cancel();
        }

        void start(const Board& board, const search::Limits& limits) {
            m_stopSource = std::stop_source {};
            {
                std::lock_guard lock(m_mutex);
                m_progress = {};
            }
            m_future = std::async(std::launch::async, [this, board, limits] {
                return m_search->iterative_deepening(board, limits, m_stopSource.get_token(),
                    [this](const search::Result& result) {
                        std::lock_guard lock(m_mutex);
                        m_progress = result;
                    });
            });
        }

        bool active() const {
            return m_future.valid();
        }

        bool ready() const {
            return active() && m_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
        }

        search::Result finish() {
            return m_future.get();
        }

        void stop() {
            m_stopSource.request_stop();
        }

        void cancel() {
            stop();
            if (m_future.valid()) {
                m_future.wait();
                try {
                    m_future.get();
                }
                catch (...) {
                }
            }
        }

        void clear() {
            cancel();
            m_search->clear();
            std::lock_guard lock(m_mutex);
            m_progress = {};
        }

        search::Result progress() const {
            std::lock_guard lock(m_mutex);
            return m_progress;
        }

        // Book moves are chosen at random by weight
        void set_book(std::shared_ptr<const search::OpeningBook> book) {
            cancel();
            search::Integrations integrations;
            if (book)
                integrations.probeBook = [book, random = std::mt19937_64(std::random_device {}())](
                    const Board& position) mutable {
                    return book->probe(position, random());
                };
            m_search->set_integrations(std::move(integrations));
        }

    private:
        std::unique_ptr<search::Search> m_search = std::make_unique<search::Search>();
        std::future<search::Result> m_future;
        std::stop_source m_stopSource;
        mutable std::mutex m_mutex;
        search::Result m_progress;
};

std::string move_text(move_t move) {
    square_t target = Move::to(move);
    if (Move::type(move) == Move::Type::Castling)
        target = Square::castling_king_square(target > Move::from(move),
            Square::rank(Move::from(move)) == 0 ? PieceColor::White : PieceColor::Black);
    std::string text;
    for (square_t square : {Move::from(move), target}) {
        text += static_cast<char>('a' + Square::file(square));
        text += static_cast<char>('1' + Square::rank(square));
    }
    if (Move::type(move) == Move::Type::Promotion)
        text += Piece::symbol(Piece::create(Move::promotion_piece(move), PieceColor::Black));
    return text;
}

std::string score_text(int score) {
    if (std::abs(score) >= search::MateThreshold) {
        int moves = (search::MateScore - std::abs(score) + 1) / 2;
        return std::string(score < 0 ? "-M" : "M") + std::to_string(moves);
    }
    return std::to_string(score) + " cp";
}

SDL_Keycode control_at(Position position) {
    if (position.x < 24 || position.x >= BoardX - 24 || position.y < 100)
        return 0;
    if (std::fmod(position.y - 100, 44.0f) >= 36)
        return 0;
    int index = static_cast<int>((position.y - 100) / 44);
    return index >= 0 && index < static_cast<int>(ControlKeys.size()) ? ControlKeys[index] : 0;
}

void draw_text(SDL_Renderer* renderer, float x, float y, const char* text) {
    float scaleX;
    float scaleY;
    SDL_GetRenderScale(renderer, &scaleX, &scaleY);
    SDL_SetRenderScale(renderer, scaleX * TextScale, scaleY * TextScale);
    SDL_RenderDebugText(renderer, x / TextScale, y / TextScale, text);
    SDL_SetRenderScale(renderer, scaleX, scaleY);
}

void draw_controls(SDL_Renderer* renderer, const Board& board, PieceColor humanColor, bool localGame,
    const search::Limits& limits, const search::Result& result, bool thinking, const std::string& bookStatus,
    const std::string& error) {
    SDL_SetRenderDrawColor(renderer, 35, 35, 35, 255);
    std::string mode = localGame ? "Local two-player" :
        humanColor == PieceColor::White ? "You: White | Engine: Black" : "You: Black | Engine: White";
    draw_text(renderer, 24, 40, mode.c_str());
    for (int index = 0; index < static_cast<int>(ControlLabels.size()); ++index) {
        SDL_FRect rectangle {24, 100.0f + index * 44, BoardX - 48, 36};
        SDL_RenderRect(renderer, &rectangle);
        draw_text(renderer, 34, rectangle.y + 14, ControlLabels[index]);
    }
    std::string settings = "-/+: " + std::to_string(limits.time.count()) + " ms | MultiPV " + std::to_string(limits.multiPv);
    draw_text(renderer, 24, 474, settings.c_str());
    draw_text(renderer, 24, 498, bookStatus.c_str());
    auto [gameResult, reason] = board.is_game_over();
    std::string status = thinking ? "Engine thinking..." : "Your move";
    if (localGame)
        status = board.is_white_to_move() ? "White to move" : "Black to move";
    if (gameResult == GameResult::Lose)
        status = board.is_white_to_move() ? "Checkmate: Black wins" : "Checkmate: White wins";
    else if (gameResult == GameResult::Draw) {
        switch (reason) {
            case GameResultReason::Stalemate: status = "Draw: stalemate"; break;
            case GameResultReason::FiftyMoveRule: status = "Draw: fifty-move rule"; break;
            case GameResultReason::ThreefoldRepetition: status = "Draw: repetition"; break;
            default: status = "Draw: insufficient material"; break;
        }
    }
    draw_text(renderer, 24, 534, status.c_str());
    // Book hits return a one-move variation without searching any nodes
    bool bookMove = result.depth == 0 && result.nodes == 0 && !result.variations.empty();
    std::string statistics = bookMove ? "Book move" : "Depth " + std::to_string(result.depth) + "/" +
        std::to_string(result.selectiveDepth) + " | Nodes " + std::to_string(result.nodes);
    draw_text(renderer, 24, 574, statistics.c_str());
    std::string elapsed = std::to_string(result.elapsed.count()) + " ms | Score: search side";
    draw_text(renderer, 24, 598, elapsed.c_str());
    for (int index = 0; index < static_cast<int>(result.variations.size()); ++index) {
        const auto& variation = result.variations[index];
        std::string text = std::to_string(index + 1) + ". " + score_text(variation.score);
        draw_text(renderer, 24, 632.0f + index * 56, text.c_str());
        text.clear();
        for (std::size_t ply = 0; ply < std::min<std::size_t>(5, variation.moves.size()); ++ply)
            text += move_text(variation.moves[ply]) + " ";
        draw_text(renderer, 24, 652.0f + index * 56, text.c_str());
    }
    if (!error.empty())
        draw_text(renderer, 24, 800, error.c_str());
}

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
    return !Piece::is_empty(piece) && Piece::color(piece) == board.move_color();
}

bool is_capture(const Board& board, move_t move) {
    if (Move::type(move) == Move::Type::EnPassant)
        return true;
    if (Move::type(move) == Move::Type::Castling)
        return false;
    return !Piece::is_empty(board.get_square(Move::to(move)));
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
    square_t checkedKing = board.is_in_check() ? board.king_square(board.move_color()) : Square::None;

    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            square_t square = Square::index(rank, file);
            Position position = square_position(square);
            SDL_FRect rectangle = {position.x, position.y, SquareSize, SquareSize};

            set_color(renderer, square_color(square, selected, lastFrom, lastTo, checkedKing));
            SDL_RenderFillRect(renderer, &rectangle);
        }
    }

    draw_legal_moves(renderer, board, selectedMoves, board.move_color());

    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            square_t square = Square::index(rank, file);
            piece_t piece = board.get_square(square);

            if (Piece::is_empty(piece) || (dragging && square == selected))
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
    else if (board.is_game_over().first == GameResult::Draw) {
        title += "Draw";
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
    PieceColor color = board.move_color();
    lastFrom = Move::from(move);
    lastTo = displayed_destination(move, color);
    board.make_move(move);
    legalMoves = generator.generate_moves(board);
    clear_selection(selected, selectedMoves);
}

bool try_move(Board& board, MoveGenerator& generator, square_t destination,
    MoveList& legalMoves, square_t& selected, MoveList& selectedMoves,
    square_t& lastFrom, square_t& lastTo, PendingPromotion& promotion) {
    MoveList candidates = moves_to(selectedMoves, destination, board.move_color());

    if (candidates.is_empty())
        return false;

    if (candidates.size() == 1) {
        commit_move(board, generator, candidates[0], legalMoves, selected,
            selectedMoves, lastFrom, lastTo);
    }
    else {
        promotion.moves = candidates;
        promotion.color = board.move_color();
        promotion.active = true;
    }

    return true;
}

}

int main(int, char**) {
    using namespace gui;
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

    std::string spritePath = std::string(SDL_GetBasePath()) + "data/chess_pieces.png";
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
    bool localGame = false;
    PieceColor humanColor = PieceColor::White;
    search::Limits limits;
    EnginePlayer engine;
    search::Result searchResult;
    std::string engineError;
    std::shared_ptr<const search::OpeningBook> book;
    bool bookEnabled = false;
    try {
        auto loaded = std::make_shared<search::OpeningBook>();
        loaded->load(std::string(SDL_GetBasePath()) + std::string(search::DefaultBookPath));
        book = std::move(loaded);
        bookEnabled = true;
        engine.set_book(book);
    }
    catch (const std::exception& error) {
        engineError = std::string("Opening book: ") + error.what();
    }
    auto book_status = [&] {
        if (!book)
            return std::string("Book: unavailable");
        return std::string(bookEnabled ? "Book: on" : "Book: off") + " (" + std::to_string(book->size()) + " positions)";
    };

    auto handle_control = [&](SDL_Keycode key) {
        if (key == SDLK_SPACE) {
            engine.stop();
            return;
        }
        if (key == SDLK_O) {
            if (book) {
                bookEnabled = !bookEnabled;
                engine.set_book(bookEnabled ? book : nullptr);
            }
            return;
        }
        if (key == SDLK_M) {
            limits.multiPv = limits.multiPv % 3 + 1;
            return;
        }
        if (key == SDLK_MINUS || key == SDLK_EQUALS || key == SDLK_PLUS) {
            int change = key == SDLK_MINUS ? -250 : 250;
            limits.time = std::chrono::milliseconds(std::clamp<int>(static_cast<int>(limits.time.count()) + change, 250, 10000));
            return;
        }
        if (key != SDLK_W && key != SDLK_B && key != SDLK_L && key != SDLK_N && key != SDLK_U)
            return;
        engine.clear();
        engineError.clear();
        searchResult = {};
        if (key == SDLK_U) {
            if (!board.all_game_moves().empty()) {
                board.unmake_move();
                if (!localGame && board.move_color() != humanColor && !board.all_game_moves().empty())
                    board.unmake_move();
            }
        }
        else {
            if (key == SDLK_W || key == SDLK_B) {
                humanColor = key == SDLK_W ? PieceColor::White : PieceColor::Black;
                localGame = false;
            }
            if (key == SDLK_L)
                localGame = true;
            board.load_start_position();
        }
        legalMoves = generator.generate_moves(board);
        clear_selection(selected, selectedMoves);
        promotion = {};
        mouseDown = false;
        dragging = false;
        selectedBeforePress = false;
        pressSquare = Square::None;
        lastFrom = Square::None;
        lastTo = Square::None;
        update_title(window, board, legalMoves, false);
    };

    update_title(window, board, legalMoves, false);

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                handle_control(event.key.key);
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
                SDL_Keycode control = control_at(mouse);
                if (control != 0) {
                    handle_control(control);
                    continue;
                }
                if ((!localGame && board.move_color() != humanColor) || board.is_game_over().first != GameResult::None)
                    continue;
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

                if (!mouseDown)
                    continue;
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

        if (!running)
            break;
        if (engine.ready()) {
            try {
                searchResult = engine.finish();
                if (searchResult.bestMove != 0 && generator.is_legal(board, searchResult.bestMove)) {
                    commit_move(board, generator, searchResult.bestMove, legalMoves, selected,
                        selectedMoves, lastFrom, lastTo);
                    update_title(window, board, legalMoves, false);
                }
            }
            catch (const std::exception& error) {
                engineError = error.what();
                localGame = true;
            }
        }
        if (!engine.active() && !localGame && board.move_color() != humanColor &&
            board.is_game_over().first == GameResult::None) {
            try {
                engine.start(board, limits);
            }
            catch (const std::exception& error) {
                engineError = error.what();
                localGame = true;
            }
        }
        if (engine.active())
            searchResult = engine.progress();

        set_color(renderer, Background);
        SDL_RenderClear(renderer);
        draw_board(renderer, sprites, board, selectedMoves, selected, lastFrom,
            lastTo, dragging, mouse);
        draw_promotion_choices(renderer, sprites, promotion);
        draw_controls(renderer, board, humanColor, localGame, limits, searchResult, engine.active(),
            book_status(), engineError);
        SDL_RenderPresent(renderer);
    }

    engine.cancel();
    SDL_DestroyTexture(sprites.texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
