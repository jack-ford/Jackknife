cmake_minimum_required(VERSION 3.24)

project(Jackknife VERSION 2.1.0 LANGUAGES CXX)

option(CHESS_BUILD_GUI "Build the SDL3 desktop GUI" ON)
option(CHESS_AVX2 "Build for AVX2-capable CPUs: AVX2 NNUE kernels and native bit-manipulation instructions" ON)
option(CHESS_ENABLE_LTO "Use link-time optimization for Release builds when supported" ON)

if(CHESS_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT CHESS_LTO_SUPPORTED OUTPUT CHESS_LTO_ERROR)
    if(CHESS_LTO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    else()
        message(WARNING "Link-time optimization is unavailable: ${CHESS_LTO_ERROR}")
    endif()
endif()

# Reserve room for MaxPly plies on every thread
set(CHESS_STACK_BYTES 8388608)
if(MSVC)
    add_link_options(/STACK:${CHESS_STACK_BYTES})
endif()

set(NNUE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/data/trained_network.nnue")
file(SHA256 "${NNUE_FILE}" NNUE_HASH)
if(NOT NNUE_HASH STREQUAL "b17d5a73bb312345c442c72d7f87474cfa3b20e9c42889efc8184b17c211560f")
    message(FATAL_ERROR "NNUE asset checksum mismatch")
endif()
set(NNUE_EMBEDDED "${CMAKE_CURRENT_BINARY_DIR}/nnue_embedded.cpp")
add_custom_command(
    OUTPUT "${NNUE_EMBEDDED}"
    COMMAND "${CMAKE_COMMAND}" "-DINPUT=${NNUE_FILE}" "-DOUTPUT=${NNUE_EMBEDDED}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedNetwork.cmake"
    DEPENDS "${NNUE_FILE}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedNetwork.cmake"
    VERBATIM
)

add_library(chess_engine
    engine/src/core/board.cpp
    engine/src/utils/fen_utility.cpp
    engine/src/utils/result_utility.cpp
    engine/src/utils/move_utility.cpp
    engine/src/eval/nnue/nnue.cpp
    engine/src/eval/nnue/kernels.cpp
    engine/src/eval/nnue/network.cpp
    "${NNUE_EMBEDDED}"
    engine/src/eval/evaluation.cpp
    engine/src/search/pvs.cpp
    engine/src/search/aspiration.cpp
    engine/src/eval/static_exchange.cpp
    engine/src/movegen/movegen.cpp
    engine/src/search/opening_book.cpp
    engine/src/search/syzygy.cpp
    engine/src/search/controller.cpp
    engine/src/search/search.cpp
    engine/src/search/probcut.cpp
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

add_library(fathom STATIC third_party/fathom/tbprobe.c)
set_source_files_properties(third_party/fathom/tbprobe.c PROPERTIES LANGUAGE CXX)
target_include_directories(fathom PUBLIC third_party/fathom)
target_compile_features(fathom PRIVATE cxx_std_20)
if(WIN32)
    target_compile_definitions(fathom PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(fathom PRIVATE Threads::Threads)
endif()
target_link_libraries(chess_engine PRIVATE fathom)

# Public so that inline engine code compiles identically in every consumer
if(CHESS_AVX2)
    if(MSVC)
        target_compile_options(chess_engine PUBLIC /arch:AVX2)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i.86)$")
        target_compile_options(chess_engine PUBLIC -march=x86-64-v3)
    else()
        message(FATAL_ERROR "AVX2 is unavailable on this target; configure with -DCHESS_AVX2=OFF")
    endif()
endif()

add_library(chess_uci_lib
    uci/uci.cpp
)
target_include_directories(chess_uci_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/uci")
target_link_libraries(chess_uci_lib PUBLIC chess_engine)

set(CHESS_BOOK_FILE "${CMAKE_CURRENT_SOURCE_DIR}/data/books/8moves_v3.pgn")
function(copy_opening_book target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/data/books"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CHESS_BOOK_FILE}" "$<TARGET_FILE_DIR:${target}>/data/books/8moves_v3.pgn"
    )
endfunction()

add_executable(chess_uci uci/main.cpp)
target_link_libraries(chess_uci PRIVATE chess_uci_lib)
copy_opening_book(chess_uci)

if(CHESS_BUILD_GUI)
    find_package(SDL3 CONFIG REQUIRED)
    find_package(SDL3_image CONFIG REQUIRED)

    add_executable(chess_gui WIN32
        gui/main.cpp
    )

    target_link_libraries(chess_gui PRIVATE chess_engine SDL3::SDL3 SDL3_image::SDL3_image)

    add_custom_command(TARGET chess_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:chess_gui>/data"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/data/chess_pieces.png"
            "$<TARGET_FILE_DIR:chess_gui>/data/chess_pieces.png"
    )
    copy_opening_book(chess_gui)
endif()

include(CTest)