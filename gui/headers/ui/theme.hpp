#pragma once

#include <SDL3/SDL_pixels.h>

namespace GUI::Theme {

inline constexpr float DesignWidth = 1280.0f;
inline constexpr float DesignHeight = 720.0f;
inline constexpr float MinScale = 0.5f;
inline constexpr int WindowWidth = 1980;
inline constexpr int WindowHeight = 1080;
inline constexpr int MinWindowWidth = 640;
inline constexpr int MinWindowHeight = 360;

inline constexpr SDL_Color Background {28, 28, 28, 255};
inline constexpr SDL_Color Button {44, 44, 44, 255};
inline constexpr SDL_Color ButtonHover {54, 54, 54, 255};
inline constexpr SDL_Color ButtonPressed {36, 36, 36, 255};
inline constexpr SDL_Color Selected {71, 71, 71, 255};
inline constexpr SDL_Color Text {232, 232, 232, 255};
inline constexpr SDL_Color DimText {145, 145, 145, 255};
inline constexpr SDL_Color Accent {91, 155, 248, 255};
inline constexpr SDL_Color Line {48, 48, 48, 255};
inline constexpr SDL_Color White {255, 255, 255, 255};
inline constexpr SDL_Color Overlay {0, 0, 0, 140};

inline constexpr SDL_Color LightSquare {240, 217, 181, 255};
inline constexpr SDL_Color DarkSquare {181, 136, 99, 255};
inline constexpr SDL_Color SelectedSquare {246, 246, 105, 255};
inline constexpr SDL_Color LastMoveLight {205, 210, 106, 255};
inline constexpr SDL_Color LastMoveDark {170, 162, 86, 255};
inline constexpr SDL_Color CheckSquare {220, 82, 82, 255};
inline constexpr SDL_Color LegalMove {45, 55, 45, 105};

inline constexpr float HeadingSize = 34.0f;
inline constexpr float LabelSize = 20.0f;
inline constexpr float SmallSize = 15.0f;
inline constexpr float StartSize = 26.0f;

inline constexpr float DragDistance = 5.0f / 88.0f;
inline constexpr float LegalDotRadius = 0.13f;
inline constexpr float CaptureInset = 0.06f;
inline constexpr float CaptureThickness = 0.08f;
inline constexpr float PromotionInset = 8.0f / 104.0f;

}
