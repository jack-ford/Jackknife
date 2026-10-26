#pragma once

#include <array>
#include <SDL3/SDL.h>
#include "core/piece.hpp"

namespace gui {

inline constexpr int WindowWidth = 1920;
inline constexpr int WindowHeight = 1080;
inline constexpr float Scale = WindowWidth / 1280.0f;
inline constexpr float SquareSize = 88.0f * Scale;
inline constexpr float PieceSize = 88.0f * Scale;
inline constexpr float BoardSize = SquareSize * 8.0f;
inline constexpr float BoardX = (WindowWidth - BoardSize) / 2.0f;
inline constexpr float BoardY = (WindowHeight - BoardSize) / 2.0f;
inline constexpr float DragDistance = 5.0f * Scale;
inline constexpr float PromotionSize = 104.0f * Scale;
inline constexpr float PromotionGap = 8.0f * Scale;
inline constexpr float PromotionX = BoardX + BoardSize + (WindowWidth - BoardX - BoardSize - PromotionSize) / 2.0f;
inline constexpr float PromotionY = (WindowHeight - PromotionSize * 4.0f - PromotionGap * 3.0f) / 2.0f;

inline constexpr SDL_Color Background = {255, 255, 255, 255};
inline constexpr SDL_Color LightSquare = {240, 217, 181, 255};
inline constexpr SDL_Color DarkSquare = {181, 136, 99, 255};
inline constexpr SDL_Color SelectedSquare = {246, 246, 105, 255};
inline constexpr SDL_Color LastMoveLight = {205, 210, 106, 255};
inline constexpr SDL_Color LastMoveDark = {170, 162, 86, 255};
inline constexpr SDL_Color CheckSquare = {220, 82, 82, 255};
inline constexpr SDL_Color LegalMove = {45, 55, 45, 105};
inline constexpr SDL_Color PromotionBackground = {225, 225, 225, 255};
inline constexpr SDL_Color PromotionBorder = {75, 75, 75, 255};

inline constexpr std::array<chess::PieceType, 4> PromotionPieces = {
    chess::PieceType::Queen,
    chess::PieceType::Rook,
    chess::PieceType::Bishop,
    chess::PieceType::Knight
};

inline constexpr std::array<const char*, 7> ControlLabels {
    "W: New game as White", "B: New game as Black", "L: Local two-player game",
    "N: New game", "U: Undo turn", "Space: Play current best move", "M: Cycle MultiPV (1-3)"
};
inline constexpr std::array<SDL_Keycode, 7> ControlKeys {
    SDLK_W, SDLK_B, SDLK_L, SDLK_N, SDLK_U, SDLK_SPACE, SDLK_M
};

inline constexpr float TextScale = 1.5f;

}
