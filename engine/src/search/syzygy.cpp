#include "search/syzygy.hpp"

#include <bit>
#include <mutex>
#include <stdexcept>

#include "tbprobe.h"

namespace search {
namespace {
std::mutex tablebaseMutex;

TablebaseOutcome outcome(unsigned wdl) {
    if (wdl == TB_WIN)
        return TablebaseOutcome::Win;

    if (wdl == TB_LOSS)
        return TablebaseOutcome::Loss;

    return TablebaseOutcome::Draw;
}

struct Position {
    uint64_t white;
    uint64_t black;
    std::array<uint64_t, 7> pieces {};
    unsigned ep;

    explicit Position(const chess::Board& board):
        white(board.color_bitboards()[chess::Board::WhiteIndex]),
        black(board.color_bitboards()[chess::Board::BlackIndex]),
        ep(board.current_game_state().enPassantHashed ? board.current_game_state().enPassantSquare : 0) {
        const auto& bitboards = board.piece_bitboards();

        for (int type = 1; type <= 6; type++)
            pieces[type] = bitboards[type * 2] | bitboards[type * 2 + 1];

    }
};

bool eligible(const chess::Board& board, unsigned largest) {
    return board.current_game_state().castlingRights == 0 &&
        std::popcount(board.all_pieces_bitboard()) <= static_cast<int>(largest);
}
}

struct Syzygy::State {
    std::string path;
    unsigned largest = 0;
    ~State() { tb_free(); }
};

Syzygy::Syzygy(const std::string& path) {
    if (path.empty())
        throw std::invalid_argument("Syzygy path is empty");
    
    static std::shared_ptr<State> activeState;
    std::lock_guard lock(tablebaseMutex);

    if (activeState && activeState->path != path) {
        if (activeState.use_count() != 1)
            throw std::invalid_argument("Another Syzygy path is still in use");
        activeState.reset();
    }

    if (!activeState) {
        auto state = std::make_shared<State>();
        state->path = path;
        if (!tb_init(path.c_str()))
            throw std::runtime_error("Unable to initialize Syzygy tables");
        state->largest = TB_LARGEST;
        activeState = std::move(state);
    }

    m_state = activeState;
}

unsigned Syzygy::max_pieces() const { return m_state->largest; }

std::optional<TablebaseOutcome> Syzygy::probe_wdl(const chess::Board& board) const {
    if (!eligible(board, max_pieces()) || board.fifty_move_counter() != 0)
        return std::nullopt;

    Position position(board);
    std::lock_guard lock(tablebaseMutex);
    unsigned result = tb_probe_wdl(position.white, position.black, position.pieces[1], position.pieces[6],
        position.pieces[5], position.pieces[4], position.pieces[3], position.pieces[2],
        0, 0, position.ep, board.is_white_to_move());
        
    return result == TB_RESULT_FAILED ? std::nullopt : std::optional {outcome(result)};
}

std::optional<TablebaseRoot> Syzygy::probe_root(const chess::Board& board) const {
    // handle repetition history
    if (!eligible(board, max_pieces()) || board.repetition_position_history().size() > 1 ||
        board.fifty_move_counter() >= 100)
        return std::nullopt;

    Position position(board);
    unsigned result;

    {
        std::lock_guard lock(tablebaseMutex);
        result = tb_probe_root(position.white, position.black, position.pieces[1], position.pieces[6],
            position.pieces[5], position.pieces[4], position.pieces[3], position.pieces[2],
            board.fifty_move_counter(), 0, position.ep, board.is_white_to_move(), nullptr);
    }

    if (result == TB_RESULT_FAILED || result == TB_RESULT_CHECKMATE || result == TB_RESULT_STALEMATE)
        return std::nullopt;

    chess::MoveGenerator generator;
    for (chess::move_t move : generator.generate_moves(board)) {
        if (chess::Move::from(move) != TB_GET_FROM(result) || chess::Move::to(move) != TB_GET_TO(result))
            continue;

        unsigned promotion = chess::Move::type(move) == chess::Move::Type::Promotion ?
            7 - static_cast<unsigned>(chess::Move::promotion_piece(move)) : 0;

        if (promotion == TB_GET_PROMOTES(result))
            return TablebaseRoot {outcome(TB_GET_WDL(result)), move, static_cast<int>(TB_GET_DTZ(result))};
    }
    return std::nullopt;
}

Integrations Syzygy::integrations() const {
    return {[backend = *this](const chess::Board& board) { return backend.probe_wdl(board); },
        [backend = *this](const chess::Board& board) { return backend.probe_root(board); }};
}

}
