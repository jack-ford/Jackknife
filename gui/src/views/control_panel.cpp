#include "views/control_panel.hpp"

#include <algorithm>
#include <string>

#include "engine_info.hpp"
#include "ui/draw.hpp"
#include "ui/theme.hpp"

namespace GUI {

namespace {

constexpr float Left = 20.0f;
constexpr float Width = 240.0f;
constexpr float ControlX = 122.0f;
constexpr float ControlWidth = 138.0f;
constexpr float ControlHeight = 30.0f;
constexpr float ModeWidth = 74.0f;
constexpr float ModeHeight = 40.0f;
constexpr float ModeY = 162.0f;
constexpr float ModeSpacing = 83.0f;

constexpr std::array<const char*, 6> RowLabels {"Clock", "Increment", "Max think", "Lines", "Book", "Board"};
constexpr std::array<float, 6> RowY {282.0f, 322.0f, 362.0f, 402.0f, 442.0f, 482.0f};
constexpr std::array<float, 3> DividerY {106.0f, 222.0f, 534.0f};

SDL_FRect control_rect(std::size_t row) {
    return {ControlX, RowY[row] - 3.0f, ControlWidth, ControlHeight};
}

}

ControlPanel::ControlPanel() {
    m_buttons[PlayWhite].set_bounds({Left, ModeY, ModeWidth, ModeHeight});
    m_buttons[PlayBlack].set_bounds({Left + ModeSpacing, ModeY, ModeWidth, ModeHeight});
    m_buttons[TwoPlayer].set_bounds({Left + ModeSpacing * 2.0f, ModeY, ModeWidth, ModeHeight});
    m_buttons[Book].set_bounds(control_rect(4));
    m_buttons[Flip].set_bounds(control_rect(5));
    m_buttons[StartStop].set_bounds({Left, 652.0f, Width, 48.0f});

    m_buttons[PlayWhite].set_label("White");
    m_buttons[PlayBlack].set_label("Black");
    m_buttons[TwoPlayer].set_label("2 Player");
    m_buttons[Flip].set_label("Flip");

    for (Button& button : m_buttons)
        button.set_font(Font::Button, Theme::Text);

    m_buttons[StartStop].set_font(Font::Start, Theme::Accent);

    for (std::size_t index = 0; index < StepperCount; index++)
        m_steppers[index].set_bounds(control_rect(index));
}

bool ControlPanel::press(SDL_FPoint point) {
    bool pressed = false;

    for (Button& button : m_buttons)
        pressed = button.press(point) || pressed;

    for (Stepper& stepper : m_steppers)
        pressed = stepper.press(point) || pressed;

    return pressed;
}

void ControlPanel::release(SDL_FPoint point, Game& game, BoardView& board) {
    Settings& settings = game.settings();
    int clockCount = static_cast<int>(ClockMinutes.size());
    int incrementCount = static_cast<int>(IncrementSeconds.size());
    int maxThinkCount = static_cast<int>(MaxThinkMilliseconds.size());

    if (m_buttons[PlayWhite].release(point))
        settings.mode = Mode::PlayWhite;

    if (m_buttons[PlayBlack].release(point))
        settings.mode = Mode::PlayBlack;

    if (m_buttons[TwoPlayer].release(point))
        settings.mode = Mode::TwoPlayer;

    if (m_buttons[Book].release(point))
        settings.book = !settings.book;

    if (m_buttons[Flip].release(point))
        board.flip();

    if (m_buttons[StartStop].release(point))
        toggle_game(game, board);

    settings.clockIndex = std::clamp(settings.clockIndex + m_steppers[ClockStepper].release(point), 0, clockCount - 1);
    settings.incrementIndex = std::clamp(settings.incrementIndex + m_steppers[IncrementStepper].release(point), 0, incrementCount - 1);
    settings.maxThinkIndex = std::clamp(settings.maxThinkIndex + m_steppers[MaxThinkStepper].release(point), 0, maxThinkCount - 1);
    settings.lines = std::clamp(settings.lines + m_steppers[LinesStepper].release(point), 1, MaxLines);
}

void ControlPanel::toggle_game(Game& game, BoardView& board) {
    if (game.running())
        game.stop();
    else
        game.start();

    board.reset();
}

void ControlPanel::update(const Game& game) {
    const Settings& settings = game.settings();
    bool editable = !game.running();

    m_buttons[PlayWhite].set_selected(settings.mode == Mode::PlayWhite);
    m_buttons[PlayBlack].set_selected(settings.mode == Mode::PlayBlack);
    m_buttons[TwoPlayer].set_selected(settings.mode == Mode::TwoPlayer);
    m_buttons[Book].set_selected(settings.book);
    m_buttons[Book].set_label(settings.book ? "On" : "Off");
    m_buttons[StartStop].set_label(game.running() ? "stop" : "start");

    for (ButtonId id : {PlayWhite, PlayBlack, TwoPlayer})
        m_buttons[id].set_enabled(editable);

    m_buttons[Book].set_enabled(editable && game.book_available());

    m_steppers[ClockStepper].set_value(std::to_string(ClockMinutes[settings.clockIndex]) + " min");
    m_steppers[IncrementStepper].set_value(std::to_string(IncrementSeconds[settings.incrementIndex]) + " s");
    m_steppers[MaxThinkStepper].set_value(MaxThinkLabels[settings.maxThinkIndex]);
    m_steppers[LinesStepper].set_value(std::to_string(settings.lines));

    for (Stepper& stepper : m_steppers)
        stepper.set_enabled(editable);
}

void ControlPanel::render(SDL_Renderer* renderer, Text& text, const Game& game, SDL_FPoint mouse) {
    text.draw(Font::Heading, chess::EngineInfo::Name, Left, 20.0f, Theme::Accent);
    text.draw(Font::Small, "Version " + std::string(chess::EngineInfo::Version), Left + 1.0f, 70.0f, Theme::DimText);

    for (float y : DividerY)
        Draw::fill_rect(renderer, {Left, y, Width, 1.0f}, Theme::Line);

    text.draw(Font::Label, "Play As", Left, 124.0f, Theme::Text);
    text.draw(Font::Label, "Settings", Left, 240.0f, Theme::Text);
    text.draw(Font::Label, "Status", Left, 552.0f, Theme::Text);

    for (std::size_t row = 0; row < RowLabels.size(); row++)
        text.draw(Font::Text, RowLabels[row], Left, RowY[row], Theme::DimText);

    for (const Button& button : m_buttons)
        button.render(renderer, text, mouse);

    for (const Stepper& stepper : m_steppers)
        stepper.render(renderer, text, mouse);

    text.draw(Font::Text, game.status(), Left, 590.0f, Theme::DimText);
}

}
