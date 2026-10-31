#include "ui/button.hpp"

#include "ui/draw.hpp"
#include "ui/theme.hpp"

namespace GUI {

void Button::set_bounds(const SDL_FRect& bounds) {
    m_bounds = bounds;
}

void Button::set_label(const std::string& label) {
    if (label != m_label)
        m_label = label;
}

void Button::set_font(Font font, SDL_Color color) {
    m_font = font;
    m_color = color;
}

void Button::set_enabled(bool enabled) {
    m_enabled = enabled;

    if (!enabled)
        m_pressed = false;
}

void Button::set_selected(bool selected) {
    m_selected = selected;
}

bool Button::press(SDL_FPoint point) {
    m_pressed = m_enabled && Draw::contains(m_bounds, point);
    return m_pressed;
}

bool Button::release(SDL_FPoint point) {
    bool clicked = m_pressed && m_enabled && Draw::contains(m_bounds, point);
    m_pressed = false;
    return clicked;
}

void Button::render(SDL_Renderer* renderer, Text& text, SDL_FPoint mouse) const {
    bool hovered = m_enabled && Draw::contains(m_bounds, mouse);

    SDL_Color fill = Theme::Button;
    SDL_Color label = m_enabled ? m_color : Theme::DimText;

    if (m_selected)
        fill = Theme::Selected;
    else if (m_pressed && hovered)
        fill = Theme::ButtonPressed;
    else if (hovered)
        fill = Theme::ButtonHover;

    Draw::fill_rect(renderer, m_bounds, fill);
    text.draw_centered(m_selected ? Font::ButtonStrong : m_font, m_label, m_bounds, label);
}

}
