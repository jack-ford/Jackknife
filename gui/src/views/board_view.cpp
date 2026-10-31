#include "views/board_view.hpp"

#include <cstdint>

#include <SDL3_image/SDL_image.h>

#include "ui/draw.hpp"
#include "ui/theme.hpp"

namespace GUI {

namespace {

constexpr float SpriteColumns = 6.0f;
constexpr float SpriteRows = 2.0f;

int sprite_column(chess::PieceType type) {
    switch (type) {
        case chess::PieceType::Queen: return 1;
        case chess::PieceType::Bishop: return 2;
        case chess::PieceType::Knight: return 3;
        case chess::PieceType::Rook: return 4;
        case chess::PieceType::Pawn: return 5;
        default: return 0;
    }
}

bool is_capture(const chess::Board& board, chess::move_t move) {
    if (chess::Move::type(move) == chess::Move::Type::EnPassant)
        return true;

    if (chess::Move::type(move) == chess::Move::Type::Castling)
        return false;

    return !chess::Piece::is_empty(board.get_square(chess::Move::to(move)));
}

}

bool BoardView::load(SDL_Renderer* renderer, const std::string& basePath, std::string& error) {
    std::string path = basePath + "data/chess_pieces.png";
    m_sprites.reset(IMG_LoadTexture(renderer, path.c_str()));

    if (!m_sprites) {
        error = "Could not load " + path + ": " + SDL_GetError();
        return false;
    }

    SDL_GetTextureSize(m_sprites.get(), &m_spriteWidth, &m_spriteHeight);
    SDL_SetTextureScaleMode(m_sprites.get(), SDL_SCALEMODE_LINEAR);
    return true;
}

void BoardView::set_bounds(const SDL_FRect& bounds) {
    m_bounds = bounds;
}

const SDL_FRect& BoardView::bounds() const {
    return m_bounds;
}

float BoardView::square_size() const {
    return m_bounds.w / 8.0f;
}

chess::square_t BoardView::square_at(SDL_FPoint point) const {
    if (!Draw::contains(m_bounds, point))
        return chess::Square::None;

    int column = static_cast<int>((point.x - m_bounds.x) / square_size());
    int row = static_cast<int>((point.y - m_bounds.y) / square_size());

    if (column > 7 || row > 7)
        return chess::Square::None;

    int file = m_flipped ? 7 - column : column;
    int rank = m_flipped ? row : 7 - row;
    return chess::Square::index(rank, file);
}

SDL_FRect BoardView::square_rect(chess::square_t square) const {
    int file = chess::Square::file(square);
    int rank = chess::Square::rank(square);
    float column = static_cast<float>(m_flipped ? 7 - file : file);
    float row = static_cast<float>(m_flipped ? rank : 7 - rank);
    float size = square_size();

    return {m_bounds.x + column * size, m_bounds.y + row * size, size, size};
}

SDL_FRect BoardView::promotion_rect(int index) const {
    float size = square_size();
    float left = m_bounds.x + (m_bounds.w - size * 4.0f) / 2.0f;
    float top = m_bounds.y + (m_bounds.h - size) / 2.0f;

    return {left + static_cast<float>(index) * size, top, size, size};
}

int BoardView::promotion_at(SDL_FPoint point) const {
    for (int index = 0; index < static_cast<int>(PromotionPieces.size()); index++) {
        if (Draw::contains(promotion_rect(index), point))
            return index;
    }

    return -1;
}

bool BoardView::can_select(const Game& game, chess::square_t square) const {
    if (square == chess::Square::None)
        return false;

    chess::piece_t piece = game.board().get_square(square);
    return !chess::Piece::is_empty(piece) && chess::Piece::color(piece) == game.board().move_color();
}

void BoardView::select(const Game& game, chess::square_t square) {
    m_selected = square;
    m_selectedMoves.clear();

    for (chess::move_t move : game.legal_moves()) {
        if (chess::Move::from(move) == square)
            m_selectedMoves.add(move);
    }
}

void BoardView::clear_selection() {
    m_selected = chess::Square::None;
    m_selectedMoves.clear();
}

void BoardView::reset() {
    clear_selection();
    m_promotionMoves.clear();
    m_promoting = false;
    m_mouseDown = false;
    m_dragging = false;
    m_selectedBeforePress = false;
    m_pressSquare = chess::Square::None;
}

void BoardView::flip() {
    m_flipped = !m_flipped;
}

bool BoardView::flipped() const {
    return m_flipped;
}

void BoardView::commit(Game& game, chess::move_t move) {
    game.play_move(move);
    clear_selection();
}

bool BoardView::try_move(Game& game, chess::square_t destination) {
    chess::PieceColor color = game.board().move_color();
    chess::MoveList candidates;

    for (chess::move_t move : m_selectedMoves) {
        if (displayed_destination(move, color) == destination)
            candidates.add(move);
    }

    if (candidates.is_empty())
        return false;

    if (candidates.size() == 1) {
        commit(game, candidates[0]);
    }
    else {
        m_promotionMoves = candidates;
        m_promotionColor = color;
        m_promoting = true;
    }

    return true;
}

void BoardView::press(SDL_FPoint point, const Game& game) {
    m_mouse = point;

    if (!game.human_to_move())
        return;

    m_pressPoint = point;
    m_mouseDown = true;
    m_dragging = false;

    if (m_promoting) {
        m_pressSquare = chess::Square::None;
        m_selectedBeforePress = false;
        return;
    }

    m_pressSquare = square_at(point);
    m_selectedBeforePress = m_selected != chess::Square::None;

    if (m_selected == chess::Square::None && can_select(game, m_pressSquare))
        select(game, m_pressSquare);
}

void BoardView::move(SDL_FPoint point) {
    m_mouse = point;

    if (m_promoting || !m_mouseDown || m_pressSquare != m_selected || m_selected == chess::Square::None)
        return;

    float dx = m_mouse.x - m_pressPoint.x;
    float dy = m_mouse.y - m_pressPoint.y;
    float distance = Theme::DragDistance * square_size();
    m_dragging = dx * dx + dy * dy >= distance * distance;
}

void BoardView::release(SDL_FPoint point, Game& game) {
    m_mouse = point;

    if (!m_mouseDown)
        return;

    if (m_promoting) {
        int choice = promotion_at(point);

        for (chess::move_t move : m_promotionMoves) {
            if (choice >= 0 && chess::Move::promotion_piece(move) == PromotionPieces[choice]) {
                m_promoting = false;
                commit(game, move);
                break;
            }
        }

        m_mouseDown = false;
        m_dragging = false;
        return;
    }

    chess::square_t destination = square_at(point);
    bool handled = false;

    if (m_selected != chess::Square::None && m_dragging && m_pressSquare == m_selected) {
        if (destination != m_selected)
            handled = try_move(game, destination);

        if (!handled)
            clear_selection();
    }
    else if (m_selected != chess::Square::None && m_selectedBeforePress && m_pressSquare == destination) {
        if (destination == m_selected) {
            clear_selection();
            handled = true;
        }
        else {
            handled = try_move(game, destination);

            if (!handled && can_select(game, destination)) {
                select(game, destination);
                handled = true;
            }
        }
    }

    if (!handled && m_selectedBeforePress && m_pressSquare == destination && destination != chess::Square::None)
        clear_selection();

    m_mouseDown = false;
    m_dragging = false;
    m_selectedBeforePress = false;
    m_pressSquare = chess::Square::None;
}

void BoardView::draw_piece(SDL_Renderer* renderer, chess::piece_t piece, const SDL_FRect& destination) const {
    float width = m_spriteWidth / SpriteColumns;
    float height = m_spriteHeight / SpriteRows;
    float row = chess::Piece::color(piece) == chess::PieceColor::White ? 0.0f : 1.0f;
    SDL_FRect source = {static_cast<float>(sprite_column(chess::Piece::type(piece))) * width, row * height, width, height};

    Draw::texture(renderer, m_sprites.get(), source, destination);
}

void BoardView::render(SDL_Renderer* renderer, const Game& game) const {
    render_squares(renderer, game);

    if (!game.started())
        return;

    render_legal_moves(renderer, game);
    render_pieces(renderer, game);
    render_promotion(renderer);
}

void BoardView::render_squares(SDL_Renderer* renderer, const Game& game) const {
    const chess::Board& board = game.board();
    bool showState = game.started();
    chess::square_t checkedKing = showState && board.is_in_check() ? board.king_square(board.move_color()) : chess::Square::None;

    for (int index = 0; index < chess::Square::Count; index++) {
        chess::square_t square = static_cast<chess::square_t>(index);
        bool light = chess::Square::is_light(square);
        SDL_Color color = light ? Theme::LightSquare : Theme::DarkSquare;

        if (square == checkedKing)
            color = Theme::CheckSquare;
        else if (square == m_selected)
            color = Theme::SelectedSquare;
        else if (showState && (square == game.last_from() || square == game.last_to()))
            color = light ? Theme::LastMoveLight : Theme::LastMoveDark;

        Draw::fill_rect(renderer, square_rect(square), color);
    }
}

void BoardView::render_legal_moves(SDL_Renderer* renderer, const Game& game) const {
    std::uint64_t drawn = 0;
    float size = square_size();

    for (chess::move_t move : m_selectedMoves) {
        chess::square_t square = displayed_destination(move, game.board().move_color());
        std::uint64_t mask = chess::Square::bitboard(square);

        if ((drawn & mask) != 0)
            continue;

        drawn |= mask;
        SDL_FRect rect = square_rect(square);

        if (is_capture(game.board(), move)) {
            float inset = size * Theme::CaptureInset;
            float thickness = size * Theme::CaptureThickness;
            float span = size - inset * 2.0f;

            Draw::fill_rect(renderer, {rect.x + inset, rect.y + inset, span, thickness}, Theme::LegalMove);
            Draw::fill_rect(renderer, {rect.x + inset, rect.y + size - inset - thickness, span, thickness}, Theme::LegalMove);
            Draw::fill_rect(renderer, {rect.x + inset, rect.y + inset, thickness, span}, Theme::LegalMove);
            Draw::fill_rect(renderer, {rect.x + size - inset - thickness, rect.y + inset, thickness, span}, Theme::LegalMove);
        }
        else {
            SDL_FPoint center = {rect.x + size / 2.0f, rect.y + size / 2.0f};
            Draw::fill_circle(renderer, center, size * Theme::LegalDotRadius, Theme::LegalMove);
        }
    }
}

void BoardView::render_pieces(SDL_Renderer* renderer, const Game& game) const {
    for (int index = 0; index < chess::Square::Count; index++) {
        chess::square_t square = static_cast<chess::square_t>(index);
        chess::piece_t piece = game.board().get_square(square);

        if (chess::Piece::is_empty(piece) || (m_dragging && square == m_selected))
            continue;

        draw_piece(renderer, piece, square_rect(square));
    }

    if (m_dragging && m_selected != chess::Square::None) {
        float size = square_size();
        SDL_FRect destination = {m_mouse.x - size / 2.0f, m_mouse.y - size / 2.0f, size, size};
        draw_piece(renderer, game.board().get_square(m_selected), destination);
    }
}

void BoardView::render_promotion(SDL_Renderer* renderer) const {
    if (!m_promoting)
        return;

    Draw::fill_rect(renderer, m_bounds, Theme::Overlay);

    for (int index = 0; index < static_cast<int>(PromotionPieces.size()); index++) {
        SDL_FRect rect = promotion_rect(index);
        bool hovered = Draw::contains(rect, m_mouse);
        float inset = rect.w * Theme::PromotionInset;
        SDL_FRect piece = {rect.x + inset, rect.y + inset, rect.w - inset * 2.0f, rect.h - inset * 2.0f};

        Draw::fill_rect(renderer, rect, hovered ? Theme::ButtonHover : Theme::Button);
        draw_piece(renderer, chess::Piece::create(PromotionPieces[index], m_promotionColor), piece);
    }
}

}
