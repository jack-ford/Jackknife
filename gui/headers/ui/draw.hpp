#pragma once

#include <SDL3/SDL.h>

namespace GUI::Draw {

SDL_FRect scaled(const SDL_FRect& rect);
bool contains(const SDL_FRect& rect, SDL_FPoint point);

void fill_rect(SDL_Renderer* renderer, const SDL_FRect& rect, SDL_Color color);
void fill_circle(SDL_Renderer* renderer, SDL_FPoint center, float radius, SDL_Color color);
void texture(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect& source, const SDL_FRect& destination);

}
