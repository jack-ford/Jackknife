#pragma once

#include <string>

#include <SDL3/SDL.h>

#include "game/game.hpp"
#include "ui/handles.hpp"
#include "ui/text.hpp"
#include "views/board_view.hpp"
#include "views/control_panel.hpp"

namespace GUI {

class SdlSession final {
    public:
        ~SdlSession();
        bool start();

    private:
        bool m_sdl = false;
        bool m_ttf = false;
};

class App final {
    public:
        ~App();

        bool initialize(std::string& error);
        void run();

        static std::string title();

    private:
        SDL_FPoint to_design(float x, float y) const;
        void handle_event(const SDL_Event& event);
        void handle_key(SDL_Keycode key);
        void render();

        SdlSession m_session;
        WindowPtr m_window;
        RendererPtr m_renderer;
        Text m_text;
        Game m_game;
        BoardView m_board;
        ControlPanel m_controls;

        SDL_FPoint m_mouse {};
        bool m_running = true;
};

}
