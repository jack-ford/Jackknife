#include "views/clocks.hpp"

#include <cstdio>
#include <string>

#include "ui/draw.hpp"
#include "ui/theme.hpp"

namespace GUI::Clocks {

namespace {

constexpr SDL_FRect TopClock {840.0f, 8.0f, 112.0f, 32.0f};
constexpr SDL_FRect BottomClock {328.0f, 680.0f, 112.0f, 32.0f};
constexpr float Padding = 12.0f;
constexpr long long TenthsThreshold = 20000;

std::string time_text(std::chrono::milliseconds time) {
    long long total = time.count();
    char buffer[32];

    if (total < TenthsThreshold)
        std::snprintf(buffer, sizeof(buffer), "%lld:%02lld.%lld", total / 60000, total / 1000 % 60, total / 100 % 10);
    else
        std::snprintf(buffer, sizeof(buffer), "%lld:%02lld", total / 60000, total / 1000 % 60);

    return buffer;
}

void render_clock(SDL_Renderer* renderer, Text& text, const Game& game, chess::PieceColor color, const SDL_FRect& bounds) {
    bool active = game.running() && game.board().move_color() == color;
    std::chrono::milliseconds time = game.started() ? game.remaining(color) :
        std::chrono::minutes(ClockMinutes[game.settings().clockIndex]);
    float y = bounds.y + (bounds.h - text.height(Font::ButtonStrong)) / 2.0f;

    Draw::fill_rect(renderer, bounds, active ? Theme::Text : Theme::Button);
    text.draw(Font::ButtonStrong, color == chess::PieceColor::White ? "White" : "Black", bounds.x + Padding, y,
        active ? Theme::Background : Theme::DimText);
    text.draw_right(Font::ButtonStrong, time_text(time), bounds.x + bounds.w - Padding, y,
        active ? Theme::Background : Theme::Text);
}

}

void render(SDL_Renderer* renderer, Text& text, const Game& game, bool flipped) {
    chess::PieceColor top = flipped ? chess::PieceColor::White : chess::PieceColor::Black;
    chess::PieceColor bottom = flipped ? chess::PieceColor::Black : chess::PieceColor::White;

    render_clock(renderer, text, game, top, TopClock);
    render_clock(renderer, text, game, bottom, BottomClock);
}

}
