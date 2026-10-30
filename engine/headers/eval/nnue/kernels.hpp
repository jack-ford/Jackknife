#pragma once

#include <algorithm>
#include <cstdint>

#include "eval/nnue/architecture.hpp"

namespace eval::nnue::kernels {

constexpr int16_t screlu(int32_t value) {
    value = std::clamp(value, 0, ActivationScale);
    return static_cast<int16_t>((value * value + ActivationScale / 2) / ActivationScale);
}

void update(int16_t* accumulator, const int16_t* added, const int16_t* removed, const int16_t* captured);

void activate(const int16_t* input, int16_t* output);

int32_t dot_product(const int16_t* values, const int16_t* weights, int size);

void hidden_layer(const int16_t* input, const int16_t* weights, int32_t* output);

}
