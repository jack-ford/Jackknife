#include "ui/viewport.hpp"

#include <algorithm>
#include <cmath>

#include "ui/theme.hpp"

namespace GUI::Viewport {

namespace {

constexpr const char* ScaleProperty = "GUI.viewport.scale";
constexpr const char* OriginXProperty = "GUI.viewport.x";
constexpr const char* OriginYProperty = "GUI.viewport.y";

SDL_FPoint origin(SDL_Renderer* renderer) {
    SDL_PropertiesID properties = SDL_GetRendererProperties(renderer);
    return {SDL_GetFloatProperty(properties, OriginXProperty, 0.0f), SDL_GetFloatProperty(properties, OriginYProperty, 0.0f)};
}

}

void fit(SDL_Renderer* renderer, int pixelWidth, int pixelHeight) {
    float width = static_cast<float>(pixelWidth);
    float height = static_cast<float>(pixelHeight);
    float fitted = std::min(width / Theme::DesignWidth, height / Theme::DesignHeight);
    float scale = std::max(fitted, Theme::MinScale);

    SDL_PropertiesID properties = SDL_GetRendererProperties(renderer);
    SDL_SetFloatProperty(properties, ScaleProperty, scale);
    SDL_SetFloatProperty(properties, OriginXProperty, std::floor((width - Theme::DesignWidth * scale) / 2.0f));
    SDL_SetFloatProperty(properties, OriginYProperty, std::floor((height - Theme::DesignHeight * scale) / 2.0f));
}

float scale(SDL_Renderer* renderer) {
    return SDL_GetFloatProperty(SDL_GetRendererProperties(renderer), ScaleProperty, 1.0f);
}

SDL_FPoint to_pixels(SDL_Renderer* renderer, SDL_FPoint point) {
    SDL_FPoint offset = origin(renderer);
    float factor = scale(renderer);
    return {offset.x + point.x * factor, offset.y + point.y * factor};
}

SDL_FRect to_pixels(SDL_Renderer* renderer, const SDL_FRect& rect) {
    SDL_FPoint topLeft = to_pixels(renderer, SDL_FPoint {rect.x, rect.y});
    SDL_FPoint bottomRight = to_pixels(renderer, SDL_FPoint {rect.x + rect.w, rect.y + rect.h});

    float left = std::round(topLeft.x);
    float top = std::round(topLeft.y);
    return {left, top, std::round(bottomRight.x) - left, std::round(bottomRight.y) - top};
}

SDL_FPoint to_design(SDL_Renderer* renderer, SDL_FPoint pixels) {
    SDL_FPoint offset = origin(renderer);
    float factor = scale(renderer);
    return {(pixels.x - offset.x) / factor, (pixels.y - offset.y) / factor};
}

}
