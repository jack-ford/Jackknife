#pragma once

#include <future>
#include <memory>
#include <mutex>
#include <stop_token>

#include "core/board.hpp"
#include "search/controller.hpp"
#include "search/opening_book.hpp"

namespace GUI {

class EnginePlayer final {
    public:
        ~EnginePlayer();

        void start(const chess::Board& board, const search::Clock& clock, const search::Limits& limits);
        bool active() const;
        bool ready() const;
        search::Result finish();
        search::Result progress() const;

        void stop();
        void cancel();
        void clear();
        void set_book(std::shared_ptr<const search::OpeningBook> book);

    private:
        std::unique_ptr<search::Controller> m_controller = std::make_unique<search::Controller>();
        std::future<search::Result> m_future;
        std::stop_source m_stopSource;
        mutable std::mutex m_mutex;
        search::Result m_progress;
};

}
