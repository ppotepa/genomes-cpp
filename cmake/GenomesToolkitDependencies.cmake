include_guard(GLOBAL)

set(GENOMES_EARCUT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/external/earcut"
    CACHE PATH "Pinned earcut.hpp source directory")
set(GENOMES_EARCUT_PIN "c68c8835ccff2b7532d31d8fa8dfcf398f629498")

function(genomes_configure_earcut)
    if(NOT GENOMES_ENABLE_EARCUT)
        return()
    endif()
    if(NOT EXISTS "${GENOMES_EARCUT_SOURCE_DIR}/include/mapbox/earcut.hpp")
        message(FATAL_ERROR "earcut.hpp is not initialized at '${GENOMES_EARCUT_SOURCE_DIR}'")
    endif()
    find_package(Git REQUIRED)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${GENOMES_EARCUT_SOURCE_DIR}" rev-parse HEAD
        RESULT_VARIABLE pin_result OUTPUT_VARIABLE actual_pin OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NOT pin_result EQUAL 0 OR NOT actual_pin STREQUAL GENOMES_EARCUT_PIN)
        message(FATAL_ERROR "earcut.hpp must be pinned to ${GENOMES_EARCUT_PIN}; found '${actual_pin}'")
    endif()
    set(EARCUT_BUILD_TESTS OFF CACHE BOOL "Disable earcut upstream tests" FORCE)
    set(EARCUT_BUILD_BENCH OFF CACHE BOOL "Disable earcut upstream benchmarks" FORCE)
    set(EARCUT_BUILD_VIZ OFF CACHE BOOL "Disable earcut upstream visualizer" FORCE)
    add_subdirectory("${GENOMES_EARCUT_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/_deps/earcut-build" EXCLUDE_FROM_ALL)
    if(TARGET earcut_hpp AND NOT TARGET genomes::earcut)
        add_library(genomes::earcut ALIAS earcut_hpp)
    endif()
endfunction()
