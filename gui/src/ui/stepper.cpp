#include "ui/stepper.hpp"

#include "ui/draw.hpp"
#include "ui/theme.hpp"

namespace GUI {

namespace {

constexpr const char* LeftArrow = "\xE2\x80\xB9";
constexpr const char* RightArrow = "\xE2\x80\xBA";

}

void Stepper::set_bounds(const SDL_FRect& bounds) {
    m_bounds = bounds;
}

void Stepper::set_value(const std::string& value) {
    if (value != m_value)
        m_value = value;
}

void Stepper::set_enabled(bool enabled) {
    m_enabled = enabled;

    if (!enabled)
        m_pressed = 0;
}

SDL_FRect Stepper::arrow(int direction) const {
    float x = direction < 0 ? m_bounds.x : m_bounds.x + m_bounds.w - ArrowWidth;
    return {x, m_bounds.y, ArrowWidth, m_bounds.h};
}

int Stepper::arrow_at(SDL_FPoint point) const {
    if (Draw::contains(arrow(-1), point))
        return -1;

    if (Draw::contains(arrow(1), point))
        return 1;

    return 0;
}

bool Stepper::press(SDL_FPoint point) {
    m_pressed = m_enabled ? arrow_at(point) : 0;
    return m_pressed != 0;
}

int Stepper::release(SDL_FPoint point) {
    int clicked = m_pressed != 0 && m_enabled && arrow_at(point) == m_pressed ? m_pressed : 0;
    m_pressed = 0;
    return clicked;
}

void Stepper::render(SDL_Renderer* renderer, Text& text, SDL_FPoint mouse) const {
    Draw::fill_rect(renderer, m_bounds, Theme::Button);

    for (int direction : {-1, 1}) {
        SDL_FRect area = arrow(direction);
        bool hovered = m_enabled && Draw::contains(area, mouse);

        if (hovered)
            Draw::fill_rect(renderer, area, m_pressed == direction ? Theme::ButtonPressed : Theme::ButtonHover);

        SDL_Color color = hovered ? Theme::Text : Theme::DimText;
        text.draw_centered(Font::Button, direction < 0 ? LeftArrow : RightArrow, area, m_enabled ? color : Theme::Button);
    }

    text.draw_centered(Font::Button, m_value, m_bounds, m_enabled ? Theme::Text : Theme::DimText);
}

}
