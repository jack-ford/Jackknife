#pragma once

#include <SDL3/SDL.h>

#include "game/game.hpp"
#include "ui/text.hpp"

namespace GUI::StatsPanel {

void render(SDL_Renderer* renderer, Text& text, const Game& game, bool flipped);

}
