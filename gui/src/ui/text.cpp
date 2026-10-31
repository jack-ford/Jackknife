#include "ui/text.hpp"

#include <cmath>

#include "ui/theme.hpp"

namespace GUI {

namespace {

using FontFiles = std::array<const char*, 3>;

constexpr FontFiles DisplaySemibold {"Poppins-Medium.ttf", "seguisb.ttf", "arialbd.ttf"};
constexpr FontFiles DisplayRegular {"Poppins-Regular.ttf", "segoeui.ttf", "arial.ttf"};
constexpr FontFiles Body {"arial.ttf", "segoeui.ttf", "tahoma.ttf"};

struct FontSpec {
    FontFiles files;
    float size;
};

constexpr std::array<FontSpec, 7> FontSpecs {{
    {DisplaySemibold, Theme::HeadingSize},
    {DisplaySemibold, Theme::LabelSize},
    {DisplayRegular, Theme::SmallSize},
    {DisplaySemibold, Theme::SmallSize},
    {DisplaySemibold, Theme::StartSize},
    {Body, Theme::LabelSize},
    {Body, Theme::SmallSize}
}};

std::string system_font_directory() {
    const char* windows = SDL_getenv("WINDIR");
    return std::string(windows != nullptr ? windows : "C:\\Windows") + "\\Fonts\\";
}

FontPtr open_font(const std::string& basePath, const FontSpec& spec) {
    for (const char* file : spec.files) {
        for (const std::string& directory : {basePath + "data/fonts/", system_font_directory()}) {
            FontPtr font(TTF_OpenFont((directory + file).c_str(), spec.size * Theme::Scale));

            if (font)
                return font;
        }
    }

    return nullptr;
}

}

bool Text::load(SDL_Renderer* renderer, const std::string& basePath, std::string& error) {
    m_engine.reset(TTF_CreateRendererTextEngine(renderer));

    if (!m_engine) {
        error = std::string("Could not create the text engine: ") + SDL_GetError();
        return false;
    }

    for (std::size_t index = 0; index < FontCount; index++) {
        m_fonts[index] = open_font(basePath, FontSpecs[index]);

        if (!m_fonts[index]) {
            error = "Could not load a font. Place Poppins or Arial in data/fonts.";
            return false;
        }
    }

    return true;
}

TTF_Text* Text::get(Font font, std::string_view text) {
    auto& cache = m_cache[static_cast<std::size_t>(font)];
    std::string key(text);

    auto found = cache.find(key);

    if (found != cache.end())
        return found->second.get();

    if (cache.size() >= MaxCachedTexts)
        cache.clear();

    TextPtr created(TTF_CreateText(m_engine.get(), m_fonts[static_cast<std::size_t>(font)].get(), key.c_str(), key.size()));
    TTF_Text* result = created.get();
    cache.emplace(std::move(key), std::move(created));
    return result;
}

float Text::width(Font font, std::string_view text) {
    if (text.empty())
        return 0.0f;

    int width = 0;
    int height = 0;
    TTF_GetTextSize(get(font, text), &width, &height);
    return static_cast<float>(width) / Theme::Scale;
}

float Text::height(Font font) const {
    return static_cast<float>(TTF_GetFontHeight(m_fonts[static_cast<std::size_t>(font)].get())) / Theme::Scale;
}

void Text::draw(Font font, std::string_view text, float x, float y, SDL_Color color) {
    if (text.empty())
        return;

    TTF_Text* shaped = get(font, text);

    if (shaped == nullptr)
        return;

    TTF_SetTextColor(shaped, color.r, color.g, color.b, color.a);
    TTF_DrawRendererText(shaped, std::round(x * Theme::Scale), std::round(y * Theme::Scale));
}

void Text::draw_right(Font font, std::string_view text, float right, float y, SDL_Color color) {
    draw(font, text, right - width(font, text), y, color);
}

void Text::draw_centered(Font font, std::string_view text, const SDL_FRect& bounds, SDL_Color color) {
    float x = bounds.x + (bounds.w - width(font, text)) / 2.0f;
    float y = bounds.y + (bounds.h - height(font)) / 2.0f;
    draw(font, text, x, y, color);
}

}
