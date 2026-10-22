#include "core/board.hpp"
#include <iostream>

using namespace chess;

int main() {

    Board board(FenUtility::StartPositionFen);
    std::cout << board;

    return 0;
}
