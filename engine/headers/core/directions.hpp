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

inline constexpr std::array<DirectionOffset, 2> WhitePawnAttackDirections = {{
    { 1, -1 },
    { 1,  1 }
}};

inline constexpr std::array<DirectionOffset, 2> BlackPawnAttackDirections = {{
    { -1, -1 },
    { -1,  1 }
}};

inline constexpr std::array<DirectionOffset, 4> RookDirections = {{
    {  1,  0 },
    { -1,  0 },
    {  0,  1 },
    {  0, -1 }
}};

inline constexpr std::array<DirectionOffset, 4> BishopDirections = {{
    {  1,  1 },
    {  1, -1 },
    { -1,  1 },
    { -1, -1 }
}};

inline constexpr std::array<DirectionOffset, 8> KnightDirections = {{
    {  2,  1 },
    { -2,  1 },
    {  2, -1 },
    { -2, -1 },
    {  1,  2 },
    { -1,  2 },
    {  1, -2 },
    { -1, -2 }
}};

inline constexpr std::array<DirectionOffset, 8> KingDirections = {{
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
