#include <string>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "app.hpp"

int main(int, char**) {
    std::string error;

    {
        GUI::App app;

        if (app.initialize(error)) {
            app.run();
            return 0;
        }
    }

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GUI::App::title().c_str(), error.c_str(), nullptr);
    return 1;
}
