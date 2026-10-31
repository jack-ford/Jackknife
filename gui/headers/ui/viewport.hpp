#pragma once

#include <SDL3/SDL.h>

namespace GUI::Viewport {

void fit(SDL_Renderer* renderer, int pixelWidth, int pixelHeight);

float scale(SDL_Renderer* renderer);
SDL_FPoint to_pixels(SDL_Renderer* renderer, SDL_FPoint point);
SDL_FRect to_pixels(SDL_Renderer* renderer, const SDL_FRect& rect);
SDL_FPoint to_design(SDL_Renderer* renderer, SDL_FPoint pixels);

}
