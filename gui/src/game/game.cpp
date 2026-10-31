#include "game/game.hpp"

#include <algorithm>
#include <exception>

namespace GUI {

namespace {

using Milliseconds = std::chrono::milliseconds;
using SteadyClock = std::chrono::steady_clock;

int color_index(chess::PieceColor color) {
    return static_cast<int>(color);
}

std::string color_name(chess::PieceColor color) {
    return color == chess::PieceColor::White ? "White" : "Black";
}

chess::PieceColor opponent(chess::PieceColor color) {
    return color == chess::PieceColor::White ? chess::PieceColor::Black : chess::PieceColor::White;
}

}

chess::square_t displayed_destination(chess::move_t move, chess::PieceColor color) {
    if (chess::Move::type(move) != chess::Move::Type::Castling)
        return chess::Move::to(move);

    return chess::Square::castling_king_square(chess::Move::to(move) > chess::Move::from(move), color);
}

Game::Game() {
    m_legalMoves = m_generator.generate_moves(m_board);
}

void Game::load_book(const std::string& basePath) {
    try {
        auto book = std::make_shared<search::OpeningBook>();
        book->load(basePath + std::string(search::DefaultBookPath));
        m_book = std::move(book);
    }
    catch (const std::exception&) {
        m_book = nullptr;
        m_settings.book = false;
    }
}

void Game::start() {
    m_engine.clear();
    m_engine.set_book(m_settings.book ? m_book : nullptr);

    m_board.load_start_position();
    m_legalMoves = m_generator.generate_moves(m_board);

    Milliseconds clock = std::chrono::minutes(ClockMinutes[m_settings.clockIndex]);
    m_remaining = {clock, clock};
    m_turnStart = SteadyClock::now();

    m_result = {};
    m_lastFrom = chess::Square::None;
    m_lastTo = chess::Square::None;
    m_status.clear();
    m_running = true;
    m_started = true;
}

void Game::stop() {
    finish("Game stopped");
}

bool Game::update() {
    if (!m_running)
        return false;

    bool moved = false;

    if (m_engine.ready()) {
        try {
            m_result = m_engine.finish();

            if (m_result.bestMove != 0 && m_generator.is_legal(m_board, m_result.bestMove)) {
                commit(m_result.bestMove);
                moved = true;
            }
        }
        catch (const std::exception& error) {
            finish(std::string("Engine error: ") + error.what());
            return moved;
        }
    }

    if (!m_running)
        return moved;

    chess::PieceColor side = m_board.move_color();

    if (remaining(side) <= Milliseconds(0)) {
        freeze_clock();
        finish(color_name(side) + " lost on time");
        return moved;
    }

    if (!m_engine.active() && engine_to_move())
        start_search();

    Milliseconds maxThink(MaxThinkMilliseconds[m_settings.maxThinkIndex]);

    if (m_engine.active() && SteadyClock::now() - m_searchStart >= maxThink)
        m_engine.stop();

    if (m_engine.active()) {
        search::Result progress = m_engine.progress();

        if (!progress.variations.empty())
            m_result = std::move(progress);
    }

    return moved;
}

void Game::play_move(chess::move_t move) {
    if (human_to_move() && m_generator.is_legal(m_board, move))
        commit(move);
}

void Game::play_now() {
    m_engine.stop();
}

void Game::start_search() {
    search::Clock clock;
    clock.remaining = remaining(m_board.move_color());
    clock.increment = std::chrono::seconds(IncrementSeconds[m_settings.incrementIndex]);

    search::Limits limits;
    limits.multiPv = m_settings.lines;

    m_resultColor = m_board.move_color();
    m_searchStart = SteadyClock::now();

    try {
        m_engine.start(m_board, clock, limits);
    }
    catch (const std::exception& error) {
        finish(std::string("Engine error: ") + error.what());
    }
}

void Game::freeze_clock() {
    chess::PieceColor side = m_board.move_color();
    m_remaining[color_index(side)] = remaining(side);
    m_turnStart = SteadyClock::now();
}

void Game::commit(chess::move_t move) {
    chess::PieceColor mover = m_board.move_color();
    int index = color_index(mover);

    freeze_clock();

    if (m_remaining[index] <= Milliseconds(0)) {
        finish(color_name(mover) + " lost on time");
        return;
    }

    m_remaining[index] += std::chrono::seconds(IncrementSeconds[m_settings.incrementIndex]);

    m_lastFrom = chess::Move::from(move);
    m_lastTo = displayed_destination(move, mover);
    m_board.make_move(move);
    m_legalMoves = m_generator.generate_moves(m_board);

    check_game_over();
}

void Game::check_game_over() {
    auto [result, reason] = m_board.is_game_over();

    if (result == chess::GameResult::None)
        return;

    if (result != chess::GameResult::Draw) {
        finish("Checkmate, " + color_name(opponent(m_board.move_color())) + " wins");
        return;
    }

    switch (reason) {
        case chess::GameResultReason::Stalemate: finish("Draw by stalemate"); break;
        case chess::GameResultReason::FiftyMoveRule: finish("Draw by fifty-move rule"); break;
        case chess::GameResultReason::ThreefoldRepetition: finish("Draw by repetition"); break;
        default: finish("Draw by insufficient material"); break;
    }
}

void Game::finish(const std::string& status) {
    if (!m_running)
        return;

    freeze_clock();
    m_engine.cancel();
    m_running = false;
    m_status = status;
}

Settings& Game::settings() {
    return m_settings;
}

const Settings& Game::settings() const {
    return m_settings;
}

bool Game::running() const {
    return m_running;
}

bool Game::started() const {
    return m_started;
}

bool Game::thinking() const {
    return m_engine.active();
}

bool Game::engine_to_move() const {
    if (!m_running || m_settings.mode == Mode::TwoPlayer)
        return false;

    chess::PieceColor engine = m_settings.mode == Mode::PlayWhite ? chess::PieceColor::Black : chess::PieceColor::White;
    return m_board.move_color() == engine;
}

bool Game::human_to_move() const {
    return m_running && !engine_to_move();
}

bool Game::book_available() const {
    return m_book != nullptr;
}

const chess::Board& Game::board() const {
    return m_board;
}

const chess::MoveList& Game::legal_moves() const {
    return m_legalMoves;
}

chess::square_t Game::last_from() const {
    return m_lastFrom;
}

chess::square_t Game::last_to() const {
    return m_lastTo;
}

std::chrono::milliseconds Game::remaining(chess::PieceColor color) const {
    Milliseconds time = m_remaining[color_index(color)];

    if (m_running && color == m_board.move_color())
        time -= std::chrono::duration_cast<Milliseconds>(SteadyClock::now() - m_turnStart);

    return std::max(time, Milliseconds(0));
}

const search::Result& Game::result() const {
    return m_result;
}

chess::PieceColor Game::result_color() const {
    return m_resultColor;
}

std::string Game::status() const {
    if (!m_running)
        return m_status;

    std::string text;

    if (m_settings.mode == Mode::TwoPlayer)
        text = color_name(m_board.move_color()) + " to move";
    else
        text = human_to_move() ? "Your move" : "Engine thinking";

    if (m_board.is_in_check())
        text += ", check";

    return text;
}

}
