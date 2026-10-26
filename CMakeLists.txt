cmake_minimum_required(VERSION 3.24)

project(ChessEngineV2 VERSION 0.1.0 LANGUAGES CXX)

option(CHESS_BUILD_GUI "Build the SDL3 desktop GUI" ON)

add_library(chess_engine
    engine/src/core/board.cpp
    engine/src/utils/fen_utility.cpp
    engine/src/utils/result_utility.cpp
    engine/src/eval/nnue.cpp
    engine/src/eval/evaluation.cpp
    engine/src/search/pvs.cpp
    engine/src/search/aspiration.cpp
    engine/src/eval/static_exchange.cpp
    engine/src/movegen/movegen.cpp
    engine/src/search/search.cpp
    engine/src/search/negamax.cpp
    engine/src/search/quiescence.cpp
    engine/src/search/move_ordering.cpp
    engine/src/utils/search_utility.cpp
    engine/src/search/transposition_table.cpp
    engine/headers/movegen/magic.hpp
    engine/src/movegen/magic.cpp
    engine/src/movegen/attacks.cpp
    engine/headers/utils/result_utility.hpp
    engine/headers/search/search.hpp
)

target_include_directories(chess_engine
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/engine/headers"
)
target_compile_features(chess_engine PUBLIC cxx_std_20)

add_executable(chess_cli
    cli/main.cpp
)

target_link_libraries(chess_cli PRIVATE chess_engine)

if(CHESS_BUILD_GUI)
    find_package(SDL3 CONFIG REQUIRED)
    find_package(SDL3_image CONFIG REQUIRED)

    add_executable(chess_gui WIN32
        gui/main.cpp
    )

    target_link_libraries(chess_gui PRIVATE chess_engine SDL3::SDL3 SDL3_image::SDL3_image)

    add_custom_command(TARGET chess_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/gui/chess_pieces.png"
            "$<TARGET_FILE_DIR:chess_gui>/chess_pieces.png"
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(board_tests tests/board/board_tests.cpp)
    target_link_libraries(board_tests PRIVATE chess_engine)
    add_test(NAME board_state_transitions COMMAND board_tests)

    add_executable(movegen_tests tests/board/movegen_tests.cpp)
    target_link_libraries(movegen_tests PRIVATE chess_engine)
    add_test(NAME legal_move_generation COMMAND movegen_tests)

    add_executable(search_tests tests/search_tests.cpp)
    target_link_libraries(search_tests PRIVATE chess_engine)
    add_test(NAME search_behavior COMMAND search_tests)

    add_executable(transposition_table_tests tests/transposition_table_tests.cpp)
    target_link_libraries(transposition_table_tests PRIVATE chess_engine)
    add_test(NAME transposition_table COMMAND transposition_table_tests)

    add_executable(search_algorithm_tests tests/search_algorithm_tests.cpp)
    target_link_libraries(search_algorithm_tests PRIVATE chess_engine)
    add_test(NAME search_algorithms COMMAND search_algorithm_tests)
    add_executable(search_integration_tests tests/search_integration_tests.cpp)
    target_link_libraries(search_integration_tests PRIVATE chess_engine)
    add_test(NAME search_integrations COMMAND search_integration_tests)
endif()
