#pragma once

#include <string>

#include <SDL3/SDL.h>

#include "ui/text.hpp"

namespace GUI {

class Stepper final {
    public:
        void set_bounds(const SDL_FRect& bounds);
        void set_value(const std::string& value);
        void set_enabled(bool enabled);

        bool press(SDL_FPoint point);
        int release(SDL_FPoint point);

        void render(SDL_Renderer* renderer, Text& text, SDL_FPoint mouse) const;

    private:
        static constexpr float ArrowWidth = 30.0f;

        SDL_FRect arrow(int direction) const;
        int arrow_at(SDL_FPoint point) const;

        SDL_FRect m_bounds {};
        std::string m_value;
        bool m_enabled = true;
        int m_pressed = 0;
};

}
