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

set(GENOMES_MANIFOLD_SOURCE_DIR "${CMAKE_SOURCE_DIR}/external/manifold"
    CACHE PATH "Pinned Manifold source directory")
set(GENOMES_MANIFOLD_PIN "0edd9d54876f3135e431575214dd6d8a72866fee")

function(genomes_configure_manifold)
    if(NOT GENOMES_ENABLE_CSG)
        return()
    endif()
    if(NOT EXISTS "${GENOMES_MANIFOLD_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "Manifold is not initialized at '${GENOMES_MANIFOLD_SOURCE_DIR}'")
    endif()
    find_package(Git REQUIRED)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${GENOMES_MANIFOLD_SOURCE_DIR}" rev-parse HEAD
        RESULT_VARIABLE pin_result OUTPUT_VARIABLE actual_pin OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NOT pin_result EQUAL 0 OR NOT actual_pin STREQUAL GENOMES_MANIFOLD_PIN)
        message(FATAL_ERROR "Manifold must be pinned to ${GENOMES_MANIFOLD_PIN}; found '${actual_pin}'")
    endif()
    set(MANIFOLD_DOWNLOADS OFF CACHE BOOL "Disallow Manifold dependency downloads" FORCE)
    set(MANIFOLD_TEST OFF CACHE BOOL "Disable Manifold upstream tests" FORCE)
    set(MANIFOLD_PYBIND OFF CACHE BOOL "Disable Manifold Python bindings" FORCE)
    set(MANIFOLD_CBIND OFF CACHE BOOL "Disable Manifold C bindings" FORCE)
    set(MANIFOLD_JSBIND OFF CACHE BOOL "Disable Manifold JS bindings" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build Manifold statically" FORCE)
    add_subdirectory("${GENOMES_MANIFOLD_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/_deps/manifold-build" EXCLUDE_FROM_ALL)
    if(TARGET manifold AND NOT TARGET genomes::manifold)
        add_library(genomes::manifold ALIAS manifold)
    elseif(TARGET manifold::manifold AND NOT TARGET genomes::manifold)
        add_library(genomes::manifold ALIAS manifold::manifold)
    endif()
endfunction()
