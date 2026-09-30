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
    # earcut.hpp is header-only. Do not add the upstream meta-project: its
    # CMakeLists unconditionally declares FetchContent fixtures and benchmark
    # dependencies, which would violate the no-network configure contract.
    if(NOT TARGET genomes_earcut)
        add_library(genomes_earcut INTERFACE)
        target_include_directories(genomes_earcut INTERFACE
            "${GENOMES_EARCUT_SOURCE_DIR}/include")
        add_library(genomes::earcut ALIAS genomes_earcut)
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

set(GENOMES_FASTGLTF_SOURCE_DIR "${CMAKE_SOURCE_DIR}/external/fastgltf"
    CACHE PATH "Pinned fastgltf source directory")
set(GENOMES_FASTGLTF_PIN "0d1b67a28c4950ea2deb796702006dcbe31e02b3")
set(GENOMES_SIMDJSON_SOURCE_DIR "${CMAKE_SOURCE_DIR}/external/simdjson"
    CACHE PATH "Pinned simdjson source directory")
set(GENOMES_SIMDJSON_PIN "7382dc2be88e53fbc35cb50369b831855656f0fd")

function(genomes_configure_simdjson)
    if(TARGET simdjson::simdjson)
        return()
    endif()
    if(NOT EXISTS "${GENOMES_SIMDJSON_SOURCE_DIR}/singleheader/simdjson.h"
       OR NOT EXISTS "${GENOMES_SIMDJSON_SOURCE_DIR}/singleheader/simdjson.cpp")
        message(FATAL_ERROR "simdjson single-header sources are not initialized at '${GENOMES_SIMDJSON_SOURCE_DIR}'")
    endif()
    find_package(Git REQUIRED)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${GENOMES_SIMDJSON_SOURCE_DIR}" rev-parse HEAD
        RESULT_VARIABLE pin_result OUTPUT_VARIABLE actual_pin OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NOT pin_result EQUAL 0 OR NOT actual_pin STREQUAL GENOMES_SIMDJSON_PIN)
        message(FATAL_ERROR "simdjson must be pinned to ${GENOMES_SIMDJSON_PIN}; found '${actual_pin}'")
    endif()
    add_library(genomes_simdjson STATIC
        "${GENOMES_SIMDJSON_SOURCE_DIR}/singleheader/simdjson.cpp")
    target_include_directories(genomes_simdjson PUBLIC
        "${GENOMES_SIMDJSON_SOURCE_DIR}/singleheader")
    target_compile_features(genomes_simdjson PUBLIC cxx_std_17)
    set_target_properties(genomes_simdjson PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(simdjson::simdjson ALIAS genomes_simdjson)
endfunction()

function(genomes_configure_fastgltf)
    if(NOT GENOMES_ENABLE_ASSETS)
        return()
    endif()
    if(NOT EXISTS "${GENOMES_FASTGLTF_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "fastgltf is not initialized at '${GENOMES_FASTGLTF_SOURCE_DIR}'")
    endif()
    find_package(Git REQUIRED)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${GENOMES_FASTGLTF_SOURCE_DIR}" rev-parse HEAD
        RESULT_VARIABLE pin_result OUTPUT_VARIABLE actual_pin OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NOT pin_result EQUAL 0 OR NOT actual_pin STREQUAL GENOMES_FASTGLTF_PIN)
        message(FATAL_ERROR "fastgltf must be pinned to ${GENOMES_FASTGLTF_PIN}; found '${actual_pin}'")
    endif()
    set(FASTGLTF_ENABLE_TESTS OFF CACHE BOOL "Disable fastgltf upstream tests" FORCE)
    set(FASTGLTF_ENABLE_EXAMPLES OFF CACHE BOOL "Disable fastgltf examples" FORCE)
    set(FASTGLTF_ENABLE_DOCS OFF CACHE BOOL "Disable fastgltf docs" FORCE)
    set(FASTGLTF_ENABLE_GLTF_RS OFF CACHE BOOL "Disable fastgltf benchmarks" FORCE)
    set(FASTGLTF_ENABLE_ASSIMP OFF CACHE BOOL "Disable fastgltf assimp benchmark" FORCE)
    set(FASTGLTF_ENABLE_CPP_MODULES OFF CACHE BOOL "Disable fastgltf modules" FORCE)
    genomes_configure_simdjson()
    add_subdirectory("${GENOMES_FASTGLTF_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/_deps/fastgltf-build" EXCLUDE_FROM_ALL)
    if(TARGET fastgltf AND NOT TARGET genomes::fastgltf)
        add_library(genomes::fastgltf ALIAS fastgltf)
    endif()
endfunction()

set(GENOMES_MIKKTSPACE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/external/mikktspace"
    CACHE PATH "Pinned MikkTSpace source directory")
set(GENOMES_MIKKTSPACE_PIN "3e895b49d05ea07e4c2133156cfa94369e19e409")

function(genomes_configure_mikktspace)
    if(NOT GENOMES_ENABLE_MIKKTSPACE)
        return()
    endif()
    if(NOT EXISTS "${GENOMES_MIKKTSPACE_SOURCE_DIR}/mikktspace.c")
        message(FATAL_ERROR "MikkTSpace is not initialized at '${GENOMES_MIKKTSPACE_SOURCE_DIR}'")
    endif()
    find_package(Git REQUIRED)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${GENOMES_MIKKTSPACE_SOURCE_DIR}" rev-parse HEAD
        RESULT_VARIABLE pin_result OUTPUT_VARIABLE actual_pin OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT pin_result EQUAL 0 OR NOT actual_pin STREQUAL GENOMES_MIKKTSPACE_PIN)
        message(FATAL_ERROR "MikkTSpace must be pinned to ${GENOMES_MIKKTSPACE_PIN}; found '${actual_pin}'")
    endif()
    add_library(genomes_mikktspace STATIC "${GENOMES_MIKKTSPACE_SOURCE_DIR}/mikktspace.c")
    target_include_directories(genomes_mikktspace PUBLIC "${GENOMES_MIKKTSPACE_SOURCE_DIR}")
    set_target_properties(genomes_mikktspace PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(genomes::mikktspace ALIAS genomes_mikktspace)
endfunction()
