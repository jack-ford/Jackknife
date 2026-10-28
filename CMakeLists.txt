cmake_minimum_required(VERSION 3.24)

project(ChessEngineV2 VERSION 0.1.0 LANGUAGES CXX)

option(CHESS_BUILD_GUI "Build the SDL3 desktop GUI" ON)
option(CHESS_AVX2 "Build for AVX2-capable CPUs: AVX2 NNUE kernels and native bit-manipulation instructions" ON)
option(CHESS_BUILD_BENCHMARKS "Build the core engine benchmark suite" ON)
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

# Search keeps per-ply move lists on the stack; reserve room for MaxPly plies on
# every thread. Other platforms' default 8 MiB thread stacks already suffice.
set(CHESS_STACK_BYTES 8388608)
if(MSVC)
    add_link_options(/STACK:${CHESS_STACK_BYTES})
endif()

set(NNUE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/data/trained_network.nnue")
file(SHA256 "${NNUE_FILE}" NNUE_HASH)
if(NOT NNUE_HASH STREQUAL "b17d5a73bb312345c442c72d7f87474cfa3b20e9c42889efc8184b17c211560f")
    message(FATAL_ERROR "Casanchess NNUE asset checksum mismatch")
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

# Public so that inline engine code compiles identically in every consumer.
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

if(CHESS_BUILD_BENCHMARKS)
    add_executable(chess_benchmark benchmarking/benchmark.cpp)
    target_link_libraries(chess_benchmark PRIVATE chess_engine)
    if(CHESS_AVX2)
        target_compile_definitions(chess_benchmark PRIVATE CHESS_AVX2_ENABLED)
    endif()
endif()

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
# The tactical suite runner serves both the test suite and benchmarking.
if(BUILD_TESTING OR CHESS_BUILD_BENCHMARKS)
    add_executable(chess_bench benchmarking/search_bench.cpp)
    target_link_libraries(chess_bench PRIVATE chess_engine)
endif()

if(BUILD_TESTING)
    add_test(NAME tactical_suite COMMAND chess_bench 4 "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/tactics.txt")
    add_executable(network_loading_tests tests/network_loading_tests.cpp)
    target_link_libraries(network_loading_tests PRIVATE chess_engine)
    target_compile_definitions(network_loading_tests PRIVATE TEST_NETWORK_PATH="${NNUE_FILE}"
        INVALID_NETWORK_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/syzygy/KQvK.rtbw")
    add_test(NAME network_loading COMMAND network_loading_tests)

    add_executable(nnue_tests tests/nnue_tests.cpp)
    target_link_libraries(nnue_tests PRIVATE chess_engine)
    add_test(NAME nnue_incremental COMMAND nnue_tests)

    set(CHESS_CASANCHESS_REFERENCE "" CACHE PATH "Optional Casanchess checkout for differential NNUE tests")
    if(CHESS_CASANCHESS_REFERENCE)
        add_executable(nnue_reference_tests tests/nnue_tests.cpp tests/nnue_reference.cpp
            "${CHESS_CASANCHESS_REFERENCE}/src/NNUE.cpp")
        set(REFERENCE_INCLUDE "${CHESS_CASANCHESS_REFERENCE}/include")
        if(MSVC)
            # MSVC 19.44 rejects upstream's constexpr score helpers calling abs.
            # Shadow only the headers, replacing that expression equivalently.
            # The NNUE.cpp being tested remains the original upstream source.
            set(REFERENCE_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/casanchess_headers")
            file(MAKE_DIRECTORY "${REFERENCE_INCLUDE}")
            foreach(header NNUE.h NNUE_Architecture.h BitboardUtils.h Constants.h)
                file(READ "${CHESS_CASANCHESS_REFERENCE}/include/${header}" content)
                if(header STREQUAL "Constants.h")
                    string(REPLACE "abs(score)" "(score < 0 ? -score : score)" content "${content}")
                endif()
                file(WRITE "${REFERENCE_INCLUDE}/${header}" "${content}")
            endforeach()
        endif()
        target_include_directories(nnue_reference_tests PRIVATE "${REFERENCE_INCLUDE}")
        target_compile_features(nnue_reference_tests PRIVATE cxx_std_23)
        target_compile_definitions(nnue_reference_tests PRIVATE CHESS_NNUE_REFERENCE
            NNUE_FILENAME="${NNUE_FILE}")
        target_link_libraries(nnue_reference_tests PRIVATE chess_engine)
        add_test(NAME nnue_casanchess_parity COMMAND nnue_reference_tests)
    endif()
    add_executable(board_tests tests/board/board_tests.cpp)
    target_link_libraries(board_tests PRIVATE chess_engine)
    add_test(NAME board_state_transitions COMMAND board_tests)

    add_executable(movegen_tests tests/board/movegen_tests.cpp)
    target_link_libraries(movegen_tests PRIVATE chess_engine)
    add_test(NAME legal_move_generation COMMAND movegen_tests)

    add_executable(search_upgrade_tests tests/search_upgrade_tests.cpp)
    target_link_libraries(search_upgrade_tests PRIVATE chess_engine)
    target_compile_definitions(search_upgrade_tests PRIVATE SYZYGY_TEST_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/syzygy"
        BUNDLED_BOOK_PATH="${CHESS_BOOK_FILE}")
    add_test(NAME search_upgrades COMMAND search_upgrade_tests)

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

    add_executable(uci_tests tests/uci_tests.cpp)
    target_link_libraries(uci_tests PRIVATE chess_uci_lib)
    add_test(NAME uci_protocol COMMAND uci_tests)
endif()
