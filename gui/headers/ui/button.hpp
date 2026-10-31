#pragma once

#include <string>

#include <SDL3/SDL.h>

#include "ui/text.hpp"

namespace GUI {

class Button final {
    public:
        void set_bounds(const SDL_FRect& bounds);
        void set_label(const std::string& label);
        void set_font(Font font, SDL_Color color);
        void set_enabled(bool enabled);
        void set_selected(bool selected);

        bool press(SDL_FPoint point);
        bool release(SDL_FPoint point);

        void render(SDL_Renderer* renderer, Text& text, SDL_FPoint mouse) const;

    private:
        SDL_FRect m_bounds {};
        std::string m_label;
        Font m_font = Font::Button;
        SDL_Color m_color {255, 255, 255, 255};
        bool m_enabled = true;
        bool m_selected = false;
        bool m_pressed = false;
};

}
