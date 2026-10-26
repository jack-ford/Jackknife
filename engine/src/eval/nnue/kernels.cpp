#include "eval/nnue/kernels.hpp"

#include <bit>
#if defined(__AVX2__)
#include <immintrin.h>
#endif

namespace eval::nnue::kernels {

namespace {

template<bool Add, bool Remove, bool Capture>
void update_rows(int16_t* accumulator, const int16_t* added, const int16_t* removed, const int16_t* captured) {
    int i = 0;
#if defined(__AVX2__)
    for (; i < AccumulatorSize; i += 16) {
        __m256i value = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(accumulator + i));
        if constexpr (Add)
            value = _mm256_add_epi16(value, _mm256_loadu_si256(reinterpret_cast<const __m256i*>(added + i)));
        if constexpr (Remove)
            value = _mm256_sub_epi16(value, _mm256_loadu_si256(reinterpret_cast<const __m256i*>(removed + i)));
        if constexpr (Capture)
            value = _mm256_sub_epi16(value, _mm256_loadu_si256(reinterpret_cast<const __m256i*>(captured + i)));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(accumulator + i), value);
    }
#endif
    for (; i < AccumulatorSize; ++i) {
        int value = accumulator[i];
        if constexpr (Add) value += added[i];
        if constexpr (Remove) value -= removed[i];
        if constexpr (Capture) value -= captured[i];
        // Match SIMD's wrapping arithmetic even for intermediate refresh sums.
        accumulator[i] = std::bit_cast<int16_t>(static_cast<uint16_t>(value));
    }
}

}

void update(int16_t* accumulator, const int16_t* added, const int16_t* removed, const int16_t* captured) {
    if (added && removed) {
        if (captured) update_rows<true, true, true>(accumulator, added, removed, captured);
        else update_rows<true, true, false>(accumulator, added, removed, captured);
    } else if (added) {
        update_rows<true, false, false>(accumulator, added, removed, captured);
    } else if (captured) {
        update_rows<false, false, true>(accumulator, added, removed, captured);
    }
}

void activate(const int16_t* input, int16_t* output) {
    int i = 0;
#if defined(__AVX2__)
    const __m256i zero = _mm256_setzero_si256();
    const __m256i maximum = _mm256_set1_epi16(ActivationScale);
    const __m256i rounding = _mm256_set1_epi16(ActivationScale / 2);
    for (; i < AccumulatorSize; i += 16) {
        __m256i value = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + i));
        value = _mm256_min_epi16(_mm256_max_epi16(value, zero), maximum);
        const __m256i atMaximum = _mm256_cmpeq_epi16(value, maximum);
        value = _mm256_mullo_epi16(value, value);
        value = _mm256_srli_epi16(_mm256_add_epi16(value, rounding), 8);
        // 256 squared wraps to zero in 16 bits; restore this endpoint.
        value = _mm256_blendv_epi8(value, maximum, atMaximum);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(output + i), value);
    }
#endif
    for (; i < AccumulatorSize; ++i)
        output[i] = screlu(input[i]);
}

int32_t dot_product(const int16_t* values, const int16_t* weights, int size) {
    int32_t sum = 0;
    int i = 0;
#if defined(__AVX2__)
    __m256i dot = _mm256_setzero_si256();
    for (; i + 16 <= size; i += 16) {
        __m256i value = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(values + i));
        __m256i weight = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(weights + i));
        dot = _mm256_add_epi32(dot, _mm256_madd_epi16(value, weight));
    }
    __m128i halves = _mm_add_epi32(_mm256_castsi256_si128(dot), _mm256_extracti128_si256(dot, 1));
    halves = _mm_add_epi32(halves, _mm_srli_si128(halves, 8));
    halves = _mm_add_epi32(halves, _mm_srli_si128(halves, 4));
    sum = _mm_cvtsi128_si32(halves);
#endif
    for (; i < size; ++i)
        sum += values[i] * weights[i];
    return sum;
}

}
