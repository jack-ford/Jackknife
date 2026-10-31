#pragma once

#include <memory>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

namespace GUI {

struct WindowDeleter {
    void operator()(SDL_Window* window) const { SDL_DestroyWindow(window); }
};

struct RendererDeleter {
    void operator()(SDL_Renderer* renderer) const { SDL_DestroyRenderer(renderer); }
};

struct TextureDeleter {
    void operator()(SDL_Texture* texture) const { SDL_DestroyTexture(texture); }
};

struct SurfaceDeleter {
    void operator()(SDL_Surface* surface) const { SDL_DestroySurface(surface); }
};

struct FontDeleter {
    void operator()(TTF_Font* font) const { TTF_CloseFont(font); }
};

struct TextEngineDeleter {
    void operator()(TTF_TextEngine* engine) const { TTF_DestroyRendererTextEngine(engine); }
};

struct TextDeleter {
    void operator()(TTF_Text* text) const { TTF_DestroyText(text); }
};

using WindowPtr = std::unique_ptr<SDL_Window, WindowDeleter>;
using RendererPtr = std::unique_ptr<SDL_Renderer, RendererDeleter>;
using TexturePtr = std::unique_ptr<SDL_Texture, TextureDeleter>;
using SurfacePtr = std::unique_ptr<SDL_Surface, SurfaceDeleter>;
using FontPtr = std::unique_ptr<TTF_Font, FontDeleter>;
using TextEnginePtr = std::unique_ptr<TTF_TextEngine, TextEngineDeleter>;
using TextPtr = std::unique_ptr<TTF_Text, TextDeleter>;

}
