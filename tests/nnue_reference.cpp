// Optional differential test against the unmodified, pinned Casanchess evaluator.
#include "NNUE.h"
#include "core/board.hpp"
#include "eval/nnue/nnue.hpp"

#include <stdexcept>

void verify_casanchess(const chess::Board& board) {
    static const bool loaded = NNUE::LoadFile();
    if (!loaded) throw std::runtime_error("could not load Casanchess reference network");
    static thread_local NNUE reference;
    PieceBitboards pieces {};
    for (int square = 0; square < 64; ++square) {
        auto piece = board.get_square(static_cast<chess::square_t>(square));
        if (chess::Piece::is_empty(piece)) continue;
        int color = chess::Piece::color(piece) == chess::PieceColor::White ? WHITE : BLACK;
        int type = chess::Piece::type(piece) == chess::PieceType::King ? KING :
            static_cast<int>(chess::Piece::type(piece)) - 1;
        pieces[color][type] |= uint64_t{1} << square;
    }
    reference.Inputs_FullUpdate(0, pieces);
    for (auto color : { chess::PieceColor::White, chess::PieceColor::Black }) {
        auto expected = reference.EvaluateOutputs(color == chess::PieceColor::White ? WHITE : BLACK, 0);
        auto actual = eval::nnue::evaluate_outputs(board.nnue_accumulator(), color);
        if (actual.eval != expected.eval || actual.drawishness != expected.drawishness)
            throw std::runtime_error("Casanchess score mismatch: " + board.current_fen());
    }
}
