#pragma once

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ui/handles.hpp"

namespace GUI {

enum class Font { Heading, Label, Button, ButtonStrong, Start, Text, Small, Count };

class Text final {
    public:
        bool load(SDL_Renderer* renderer, const std::string& basePath, std::string& error);

        float width(Font font, std::string_view text);
        float height(Font font) const;

        void draw(Font font, std::string_view text, float x, float y, SDL_Color color);
        void draw_right(Font font, std::string_view text, float right, float y, SDL_Color color);
        void draw_centered(Font font, std::string_view text, const SDL_FRect& bounds, SDL_Color color);

    private:
        static constexpr std::size_t FontCount = static_cast<std::size_t>(Font::Count);
        static constexpr std::size_t MaxCachedTexts = 512;

        TTF_Text* get(Font font, std::string_view text);

        TextEnginePtr m_engine;
        std::array<FontPtr, FontCount> m_fonts;
        std::array<std::unordered_map<std::string, TextPtr>, FontCount> m_cache;
};

}
