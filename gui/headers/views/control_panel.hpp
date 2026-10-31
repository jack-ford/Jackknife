#pragma once

#include <array>
#include <cstddef>

#include <SDL3/SDL.h>

#include "game/game.hpp"
#include "ui/button.hpp"
#include "ui/stepper.hpp"
#include "ui/text.hpp"
#include "views/board_view.hpp"

namespace GUI {

class ControlPanel final {
    public:
        ControlPanel();

        bool press(SDL_FPoint point);
        void release(SDL_FPoint point, Game& game, BoardView& board);
        void toggle_game(Game& game, BoardView& board);

        void update(const Game& game);
        void render(SDL_Renderer* renderer, Text& text, const Game& game, SDL_FPoint mouse);

    private:
        enum ButtonId : std::size_t { PlayWhite, PlayBlack, TwoPlayer, Book, Flip, StartStop, ButtonCount };
        enum StepperId : std::size_t { ClockStepper, IncrementStepper, MaxThinkStepper, LinesStepper, StepperCount };

        std::array<Button, ButtonCount> m_buttons;
        std::array<Stepper, StepperCount> m_steppers;
};

}
