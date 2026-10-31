#include "ui/draw.hpp"

#include <cmath>

#include "ui/viewport.hpp"

namespace GUI::Draw {

namespace {

void set_color(SDL_Renderer* renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

}

bool contains(const SDL_FRect& rect, SDL_FPoint point) {
    return point.x >= rect.x && point.x < rect.x + rect.w && point.y >= rect.y && point.y < rect.y + rect.h;
}

void fill_rect(SDL_Renderer* renderer, const SDL_FRect& rect, SDL_Color color) {
    SDL_FRect pixels = Viewport::to_pixels(renderer, rect);
    set_color(renderer, color);
    SDL_RenderFillRect(renderer, &pixels);
}

void fill_circle(SDL_Renderer* renderer, SDL_FPoint center, float radius, SDL_Color color) {
    set_color(renderer, color);

    SDL_FPoint position = Viewport::to_pixels(renderer, center);
    float x = position.x;
    float y = position.y;
    float pixels = radius * Viewport::scale(renderer);
    int extent = static_cast<int>(pixels);

    for (int row = -extent; row <= extent; row++) {
        float width = std::sqrt(pixels * pixels - static_cast<float>(row * row));
        SDL_RenderLine(renderer, x - width, y + static_cast<float>(row), x + width, y + static_cast<float>(row));
    }
}

void texture(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect& source, const SDL_FRect& destination) {
    SDL_FRect pixels = Viewport::to_pixels(renderer, destination);
    SDL_RenderTexture(renderer, texture, &source, &pixels);
}

}
