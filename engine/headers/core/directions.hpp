#pragma once

#include <array>
#include <cstdint>

namespace chess {

enum class Direction : int8_t {
    North     =  8,
    South     = -8,
    East      =  1,
    West      = -1,
    NorthEast =  9,
    NorthWest =  7,
    SouthEast = -7,
    SouthWest = -9
};

struct DirectionOffset {
    int rank;
    int file;
};

inline constexpr std::array<DirectionOffset, 2> whitePawnAttackDirections = {{
    { 1, -1 },
    { 1,  1 }
}};

inline constexpr std::array<DirectionOffset, 2> blackPawnAttackDirections = {{
    { -1, -1 },
    { -1,  1 }
}};

inline constexpr std::array<DirectionOffset, 4> rookDirections = {{
    {  1,  0 },
    { -1,  0 },
    {  0,  1 },
    {  0, -1 }
}};

inline constexpr std::array<DirectionOffset, 4> bishopDirections = {{
    {  1,  1 },
    {  1, -1 },
    { -1,  1 },
    { -1, -1 }
}};

inline constexpr std::array<DirectionOffset, 8> knightDirections = {{
    {  2,  1 },
    { -2,  1 },
    {  2, -1 },
    { -2, -1 },
    {  1,  2 },
    { -1,  2 },
    {  1, -2 },
    { -1, -2 }
}};

inline constexpr std::array<DirectionOffset, 8> kingDirections = {{
    {  1,  1 },
    { -1,  1 },
    {  1, -1 },
    { -1, -1 },
    {  0,  1 },
    {  0, -1 },
    {  1,  0 },
    { -1,  0 }
}};

}
