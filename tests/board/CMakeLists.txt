cmake_minimum_required(VERSION 3.24)
project(BoardTests LANGUAGES CXX)
enable_testing()
add_executable(board_tests
    board_tests.cpp
    ../../engine/src/utils/result_utility.cpp
    ../../engine/src/movegen/movegen.cpp
    ../../engine/src/core/board.cpp
    ../../engine/src/utils/fen_utility.cpp
    ../../engine/src/movegen/attacks.cpp
    ../../engine/src/movegen/magic.cpp
)
target_compile_features(board_tests PRIVATE cxx_std_20)
target_include_directories(board_tests PRIVATE ../../engine/headers)
add_test(NAME board_state_transitions COMMAND board_tests)

add_executable(movegen_tests
    movegen_tests.cpp
    ../../engine/src/utils/result_utility.cpp
    ../../engine/src/core/board.cpp
    ../../engine/src/utils/fen_utility.cpp
    ../../engine/src/movegen/attacks.cpp
    ../../engine/src/movegen/magic.cpp
    ../../engine/src/movegen/movegen.cpp
)
target_compile_features(movegen_tests PRIVATE cxx_std_20)
target_include_directories(movegen_tests PRIVATE ../../engine/headers)
add_test(NAME legal_move_generation COMMAND movegen_tests)
