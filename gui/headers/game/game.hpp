#pragma once

#include <array>
#include <chrono>
#include <memory>
#include <string>

#include "core/board.hpp"
#include "game/engine_player.hpp"
#include "movegen/movegen.hpp"
#include "search/opening_book.hpp"
#include "search/search.hpp"

namespace GUI {

enum class Mode { PlayWhite, PlayBlack, TwoPlayer };

inline constexpr std::array<int, 7> ClockMinutes {1, 3, 5, 10, 15, 30, 60};
inline constexpr std::array<int, 6> IncrementSeconds {0, 1, 2, 3, 5, 10};
inline constexpr std::array<int, 5> MaxThinkMilliseconds {250, 500, 1000, 2000, 5000};
inline constexpr std::array<const char*, 5> MaxThinkLabels {"0.25 s", "0.5 s", "1 s", "2 s", "5 s"};
inline constexpr int MaxLines = 3;

struct Settings {
    Mode mode = Mode::PlayWhite;
    int clockIndex = 2;
    int incrementIndex = 3;
    int maxThinkIndex = 2;
    int lines = 1;
    bool book = true;
};

chess::square_t displayed_destination(chess::move_t move, chess::PieceColor color);

class Game final {
    public:
        Game();

        void load_book(const std::string& basePath);
        void start();
        void stop();
        bool update();
        void play_move(chess::move_t move);
        void play_now();

        Settings& settings();
        const Settings& settings() const;

        bool running() const;
        bool started() const;
        bool thinking() const;
        bool human_to_move() const;
        bool book_available() const;

        const chess::Board& board() const;
        const chess::MoveList& legal_moves() const;
        chess::square_t last_from() const;
        chess::square_t last_to() const;
        std::chrono::milliseconds remaining(chess::PieceColor color) const;

        const search::Result& result() const;
        chess::PieceColor result_color() const;
        std::string status() const;

    private:
        bool engine_to_move() const;
        void freeze_clock();
        void commit(chess::move_t move);
        void check_game_over();
        void finish(const std::string& status);
        void start_search();

        chess::Board m_board;
        chess::MoveGenerator m_generator;
        chess::MoveList m_legalMoves;
        EnginePlayer m_engine;
        std::shared_ptr<const search::OpeningBook> m_book;
        Settings m_settings;

        std::array<std::chrono::milliseconds, 2> m_remaining {};
        std::chrono::steady_clock::time_point m_turnStart;
        std::chrono::steady_clock::time_point m_searchStart;

        search::Result m_result;
        chess::PieceColor m_resultColor = chess::PieceColor::White;
        chess::square_t m_lastFrom = chess::Square::None;
        chess::square_t m_lastTo = chess::Square::None;

        std::string m_status = "Press start";
        bool m_running = false;
        bool m_started = false;
};

}
