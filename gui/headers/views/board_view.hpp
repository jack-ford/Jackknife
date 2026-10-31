#pragma once

#include <array>
#include <string>

#include <SDL3/SDL.h>

#include "game/game.hpp"
#include "ui/handles.hpp"

namespace GUI {

class BoardView final {
    public:
        bool load(SDL_Renderer* renderer, const std::string& basePath, std::string& error);
        void set_bounds(const SDL_FRect& bounds);
        const SDL_FRect& bounds() const;

        void press(SDL_FPoint point, const Game& game);
        void move(SDL_FPoint point);
        void release(SDL_FPoint point, Game& game);

        void reset();
        void clear_selection();
        void flip();
        void reset_flip();
        void set_orientation(chess::PieceColor bottom);
        bool flipped() const;

        void render(SDL_Renderer* renderer, const Game& game) const;

    private:
        static constexpr std::array<chess::PieceType, 4> PromotionPieces {
            chess::PieceType::Queen, chess::PieceType::Rook, chess::PieceType::Bishop, chess::PieceType::Knight
        };

        float square_size() const;
        chess::square_t square_at(SDL_FPoint point) const;
        SDL_FRect square_rect(chess::square_t square) const;
        SDL_FRect promotion_rect(int index) const;
        int promotion_at(SDL_FPoint point) const;

        bool can_select(const Game& game, chess::square_t square) const;
        void select(const Game& game, chess::square_t square);
        bool try_move(Game& game, chess::square_t destination);
        void commit(Game& game, chess::move_t move);

        void draw_piece(SDL_Renderer* renderer, chess::piece_t piece, const SDL_FRect& destination) const;
        void render_squares(SDL_Renderer* renderer, const Game& game) const;
        void render_legal_moves(SDL_Renderer* renderer, const Game& game) const;
        void render_pieces(SDL_Renderer* renderer, const Game& game) const;
        void render_promotion(SDL_Renderer* renderer) const;

        TexturePtr m_sprites;
        float m_spriteWidth = 0.0f;
        float m_spriteHeight = 0.0f;
        SDL_FRect m_bounds {};
        bool m_flipped = false;
        bool m_blackAtBottom = false;

        chess::square_t m_selected = chess::Square::None;
        chess::MoveList m_selectedMoves;
        chess::MoveList m_promotionMoves;
        chess::PieceColor m_promotionColor = chess::PieceColor::White;
        bool m_promoting = false;

        SDL_FPoint m_mouse {};
        SDL_FPoint m_pressPoint {};
        chess::square_t m_pressSquare = chess::Square::None;
        bool m_mouseDown = false;
        bool m_dragging = false;
        bool m_selectedBeforePress = false;
};

}
