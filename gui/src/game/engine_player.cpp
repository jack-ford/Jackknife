#include "game/engine_player.hpp"

#include <chrono>
#include <random>

namespace GUI {

EnginePlayer::~EnginePlayer() {
    cancel();
}

void EnginePlayer::start(const chess::Board& board, const search::Clock& clock, const search::Limits& limits) {
    m_stopSource = std::stop_source {};

    {
        std::lock_guard lock(m_mutex);
        m_progress = {};
    }

    m_future = std::async(std::launch::async, [this, board, clock, limits] {
        return m_controller->search(board, clock, limits, m_stopSource.get_token(),
            [this](const search::Result& result) {
                std::lock_guard lock(m_mutex);
                m_progress = result;
            });
    });
}

bool EnginePlayer::active() const {
    return m_future.valid();
}

bool EnginePlayer::ready() const {
    return active() && m_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
}

search::Result EnginePlayer::finish() {
    return m_future.get();
}

search::Result EnginePlayer::progress() const {
    std::lock_guard lock(m_mutex);
    return m_progress;
}

void EnginePlayer::stop() {
    m_stopSource.request_stop();
}

void EnginePlayer::cancel() {
    stop();

    if (!m_future.valid())
        return;

    m_future.wait();

    try {
        m_future.get();
    }
    catch (...) {
    }
}

void EnginePlayer::clear() {
    cancel();
    m_controller->clear();

    std::lock_guard lock(m_mutex);
    m_progress = {};
}

void EnginePlayer::set_book(std::shared_ptr<const search::OpeningBook> book) {
    cancel();

    search::Integrations integrations;

    if (book) {
        integrations.probeBook = [book, random = std::mt19937_64(std::random_device {}())](
            const chess::Board& position) mutable {
            return book->probe(position, random());
        };
    }

    m_controller->set_integrations(std::move(integrations));
}

}
