#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

#include "core/board.hpp"
#include "search/opening_book.hpp"
#include "search/search.hpp"
#include "search/syzygy.hpp"

namespace uci {

class Application final {
    public:
        // A non-empty default book becomes the Book File option's default.
        explicit Application(std::string defaultBookFile = {});
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        int run(std::istream& input, std::ostream& output);

    private:
        struct Settings {
            std::size_t hashMegabytes = search::DefaultHashMegabytes;
            int multiPv = 1;
            std::chrono::milliseconds moveOverhead {20};
            bool ponder = false;
            bool ownBook = false;
            bool analyseMode = false;
            std::string bookFile;
            std::string syzygyPath;
            search::Options searchOptions;
        };

        struct GoCommand {
            search::Limits limits;
            search::Limits ponderHitLimits;
            bool ponder = false;
        };

        void handle_command(std::string_view line);
        void handle_position(std::string_view arguments);
        void handle_setoption(std::string_view arguments);
        void handle_go(std::string_view arguments);
        void handle_perft(std::string_view arguments);
        void handle_bench(std::string_view arguments);
        void start_search(const search::Limits& limits, bool ponder);
        void stop_search(bool emitBestMove);
        void rebuild_search();
        void refresh_integrations();
        void configure_book();
        void configure_syzygy();
        void print_identification();
        void print_info(const search::Result& result);
        void print_result(const search::Result& result);
        void send(std::string_view line);
        void send_error(std::string_view message);

        static uint64_t perft(chess::Board& board, int depth);

        Settings m_settings;
        std::string m_defaultBookFile;
        chess::Board m_board;
        std::unique_ptr<search::Search> m_search;
        std::shared_ptr<search::OpeningBook> m_book;
        std::shared_ptr<search::Syzygy> m_syzygy;
        std::jthread m_worker;
        std::stop_source m_stopSource;
        std::atomic<bool> m_emitBestMove {false};
        bool m_debug = false;
        bool m_pondering = false;
        search::Limits m_ponderHitLimits;
        std::ostream* m_output = nullptr;
        std::mutex m_outputMutex;
};

}
