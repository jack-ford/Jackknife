#include "ui/draw.hpp"

#include <cmath>

#include "ui/theme.hpp"

namespace GUI::Draw {

namespace {

void set_color(SDL_Renderer* renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

}

SDL_FRect scaled(const SDL_FRect& rect) {
    return {rect.x * Theme::Scale, rect.y * Theme::Scale, rect.w * Theme::Scale, rect.h * Theme::Scale};
}

bool contains(const SDL_FRect& rect, SDL_FPoint point) {
    return point.x >= rect.x && point.x < rect.x + rect.w && point.y >= rect.y && point.y < rect.y + rect.h;
}

void fill_rect(SDL_Renderer* renderer, const SDL_FRect& rect, SDL_Color color) {
    SDL_FRect pixels = scaled(rect);
    set_color(renderer, color);
    SDL_RenderFillRect(renderer, &pixels);
}

void fill_circle(SDL_Renderer* renderer, SDL_FPoint center, float radius, SDL_Color color) {
    set_color(renderer, color);

    float x = center.x * Theme::Scale;
    float y = center.y * Theme::Scale;
    float pixels = radius * Theme::Scale;
    int extent = static_cast<int>(pixels);

    for (int row = -extent; row <= extent; row++) {
        float width = std::sqrt(pixels * pixels - static_cast<float>(row * row));
        SDL_RenderLine(renderer, x - width, y + static_cast<float>(row), x + width, y + static_cast<float>(row));
    }
}

void texture(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect& source, const SDL_FRect& destination) {
    SDL_FRect pixels = scaled(destination);
    SDL_RenderTexture(renderer, texture, &source, &pixels);
}

}
