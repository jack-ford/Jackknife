#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "core/square.hpp"

namespace eval::nnue {

inline constexpr int AccumulatorSize = 128;
inline constexpr int HiddenSize = 48;
inline constexpr int FeatureCount = 26 * 64 * 5 * 2;
inline constexpr int ActivationScale = 256;
inline constexpr int WeightScale = 64;
inline constexpr int BiasScale = ActivationScale * WeightScale;
inline constexpr std::array<uint8_t, 64> KingBuckets {
     0, 1, 2, 3, 4, 5, 6, 7,
     8, 8, 9,10,11,12,13,13,
    14,14,15,15,16,16,17,17,
    14,14,18,18,19,19,17,17,
    20,20,21,21,22,22,23,23,
    20,20,21,21,22,22,23,23,
    24,24,24,24,25,25,25,25,
    24,24,24,24,25,25,25,25
};

constexpr int king_bucket(chess::square_t square, chess::PieceColor perspective) {
    return KingBuckets[chess::Square::relative_square(square, perspective)];
}

constexpr int feature_index(chess::piece_t piece, chess::square_t square,
                           chess::square_t kingSquare, chess::PieceColor perspective) {
    // Explicity map values to model
    int type = static_cast<int>(chess::Piece::type(piece)) - static_cast<int>(chess::PieceType::Pawn);
    int enemy = chess::Piece::color(piece) != perspective;
    return 640 * king_bucket(kingSquare, perspective) + 64 * (2 * type + enemy) +
        chess::Square::relative_square(square, perspective);
}

struct alignas(32) Network {
    int16_t inputWeights[FeatureCount][AccumulatorSize];
    int16_t inputBiases[AccumulatorSize];
    int16_t linearWeights[FeatureCount];
    int16_t hiddenWeights[HiddenSize][2 * AccumulatorSize];
    int32_t hiddenBiases[HiddenSize];
    int16_t outputWeights[HiddenSize];
    int32_t outputBias;
    int16_t drawWeights[2 * AccumulatorSize];
    int32_t drawBias;
};

static_assert(std::is_trivially_copyable_v<Network>);
static_assert(sizeof(Network) == 4318784);
static_assert(offsetof(Network, drawBias) == 4318756);

struct alignas(32) Accumulator {
    std::array<std::array<int16_t, AccumulatorSize>, 2> values {};
    std::array<int32_t, 2> linear {};
    bool operator==(const Accumulator&) const = default;
};

struct Output {
    int eval;
    int drawishness; // Signed residual in centilogits
};

const Network& network();

}
