cmake_minimum_required(VERSION 3.24)

project(ChessEngineV2 VERSION 0.1.0 LANGUAGES CXX)

option(CHESS_BUILD_GUI "Build the SDL3 desktop GUI" ON)

add_library(chess_engine
    engine/src/core/board.cpp
    engine/src/utils/fen_utility.cpp
    engine/src/utils/result_utility.cpp
    engine/src/eval/nnue.cpp
    engine/src/movegen/movegen.cpp
    engine/src/search/search.cpp
    engine/headers/movegen/magic.hpp
    engine/src/movegen/magic.cpp
    engine/src/movegen/attacks.cpp
    engine/headers/utils/result_utility.hpp
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
