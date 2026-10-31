#include "app.hpp"

#include "engine_info.hpp"
#include "ui/theme.hpp"
#include "ui/viewport.hpp"
#include "views/clocks.hpp"
#include "views/stats_panel.hpp"

namespace GUI {

namespace {

constexpr SDL_FRect BoardBounds {328.0f, 48.0f, 624.0f, 624.0f};

}

SdlSession::~SdlSession() {
    if (m_ttf)
        TTF_Quit();

    if (m_sdl)
        SDL_Quit();
}

bool SdlSession::start() {
    m_sdl = SDL_Init(SDL_INIT_VIDEO);
    m_ttf = m_sdl && TTF_Init();
    return m_ttf;
}

App::~App() {
    if (m_watching)
        SDL_RemoveEventWatch(on_event_watch, this);

    m_game.stop();
}

std::string App::title() {
    return std::string(chess::EngineInfo::FullName);
}

bool App::initialize(std::string& error) {
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d11");

    if (!m_session.start()) {
        error = std::string("Could not start SDL: ") + SDL_GetError();
        return false;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    if (!SDL_CreateWindowAndRenderer(title().c_str(), Theme::WindowWidth, Theme::WindowHeight, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        error = std::string("Could not create the window: ") + SDL_GetError();
        return false;
    }

    m_window.reset(window);
    m_renderer.reset(renderer);

    SDL_SetWindowMinimumSize(window, Theme::MinWindowWidth, Theme::MinWindowHeight);
    SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    fit_viewport();

    const char* base = SDL_GetBasePath();
    std::string basePath = base != nullptr ? base : "";

    if (!m_text.load(renderer, basePath, error))
        return false;

    if (!m_board.load(renderer, basePath, error))
        return false;

    m_board.set_bounds(BoardBounds);
    m_game.load_book(basePath);
    m_watching = SDL_AddEventWatch(on_event_watch, this);
    return true;
}

void App::run() {
    while (m_running) {
        SDL_Event event;

        while (SDL_PollEvent(&event))
            handle_event(event);

        if (!m_running)
            break;

        if (m_game.update())
            m_board.clear_selection();

        fit_viewport();
        m_board.set_orientation(bottom_color());
        m_controls.update(m_game);
        render();
    }
}

bool SDLCALL App::on_event_watch(void* userdata, SDL_Event* event) {
    if (event->type == SDL_EVENT_WINDOW_EXPOSED) {
        App* app = static_cast<App*>(userdata);
        app->fit_viewport();
        app->render();
    }

    return true;
}

void App::fit_viewport() {
    int width = 0;
    int height = 0;
    SDL_GetCurrentRenderOutputSize(m_renderer.get(), &width, &height);

    if (width == m_pixelWidth && height == m_pixelHeight)
        return;

    float previous = Viewport::scale(m_renderer.get());

    m_pixelWidth = width;
    m_pixelHeight = height;
    Viewport::fit(m_renderer.get(), width, height);

    if (m_text.loaded() && Viewport::scale(m_renderer.get()) != previous)
        m_text.rescale();
}

chess::PieceColor App::bottom_color() const {
    switch (m_game.settings().mode) {
        case Mode::PlayBlack:
            return chess::PieceColor::Black;

        case Mode::TwoPlayer:
            return m_game.board().move_color();

        default:
            return chess::PieceColor::White;
    }
}

SDL_FPoint App::to_design(float x, float y) const {
    return Viewport::to_design(m_renderer.get(), SDL_FPoint {x, y});
}

void App::handle_event(const SDL_Event& source) {
    SDL_Event event = source;
    SDL_ConvertEventToRenderCoordinates(m_renderer.get(), &event);

    switch (event.type) {
        case SDL_EVENT_QUIT:
            m_running = false;
            break;

        case SDL_EVENT_KEY_DOWN:
            if (!event.key.repeat)
                handle_key(event.key.key);
            break;

        case SDL_EVENT_MOUSE_MOTION:
            m_mouse = to_design(event.motion.x, event.motion.y);
            m_board.move(m_mouse);
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button != SDL_BUTTON_LEFT)
                break;

            m_mouse = to_design(event.button.x, event.button.y);

            if (!m_controls.press(m_mouse))
                m_board.press(m_mouse, m_game);
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button != SDL_BUTTON_LEFT)
                break;

            m_mouse = to_design(event.button.x, event.button.y);
            m_controls.release(m_mouse, m_game, m_board);
            m_board.release(m_mouse, m_game);
            break;

        default:
            break;
    }
}

void App::handle_key(SDL_Keycode key) {
    switch (key) {
        case SDLK_SPACE:
            m_game.play_now();
            break;

        case SDLK_F:
            m_board.flip();
            break;

        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            m_controls.toggle_game(m_game, m_board);
            break;

        default:
            break;
    }
}

void App::render() {
    SDL_Renderer* renderer = m_renderer.get();

    SDL_SetRenderDrawColor(renderer, Theme::Background.r, Theme::Background.g, Theme::Background.b, Theme::Background.a);
    SDL_RenderClear(renderer);

    m_board.render(renderer, m_game);
    Clocks::render(renderer, m_text, m_game, m_board.flipped());
    m_controls.render(renderer, m_text, m_game, m_mouse);
    StatsPanel::render(renderer, m_text, m_game, m_board.flipped());

    SDL_RenderPresent(renderer);
}

}
