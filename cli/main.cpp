#include "core/board.hpp"
#include <iostream>

using namespace chess;

int main() {

    Board board(FenUtility::startPositionFen);
    std::cout << board;

    return 0;
}
