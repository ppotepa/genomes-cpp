include_guard(GLOBAL)

set(GENOMES_MESHOPTIMIZER_PIN "9e1f07b159d3cb777f1c67ed31fc11fd117986f4")
set(GENOMES_MESHOPTIMIZER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/external/meshoptimizer"
    CACHE PATH "Pinned meshoptimizer 1.3 source directory")

function(genomes_configure_meshoptimizer)
    if(NOT GENOMES_ENABLE_MESHOPTIMIZER)
        return()
    endif()
    if(NOT EXISTS "${GENOMES_MESHOPTIMIZER_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "meshoptimizer is not initialized. Run: git submodule update --init external/meshoptimizer")
    endif()
    find_package(Git REQUIRED)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${GENOMES_MESHOPTIMIZER_SOURCE_DIR}" rev-parse HEAD
        RESULT_VARIABLE pin_result OUTPUT_VARIABLE actual_pin
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT pin_result EQUAL 0 OR NOT actual_pin STREQUAL GENOMES_MESHOPTIMIZER_PIN)
        message(FATAL_ERROR "meshoptimizer must be pinned to ${GENOMES_MESHOPTIMIZER_PIN}; found '${actual_pin}'")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${GENOMES_MESHOPTIMIZER_SOURCE_DIR}" diff --quiet HEAD --
        RESULT_VARIABLE dirty_result ERROR_QUIET)
    if(NOT dirty_result EQUAL 0)
        message(FATAL_ERROR "meshoptimizer has tracked local changes; preserve them, then restore or repin deliberately")
    endif()
    # No GPU, windowing, codecs or gltfpack dependencies are configured here.
    # Keep the upstream project CPU-only and hermetic. Cache entries are forced
    # because meshoptimizer is an embedded subdirectory and must not inherit a
    # developer's prior standalone configuration.
    set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "Disable meshoptimizer demo" FORCE)
    set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "Disable meshoptimizer gltfpack" FORCE)
    set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "Build meshoptimizer statically" FORCE)
    set(MESHOPT_WERROR OFF CACHE BOOL "Do not promote upstream warnings" FORCE)
    set(MESHOPT_INSTALL OFF CACHE BOOL "Disable meshoptimizer install rules" FORCE)
    add_subdirectory("${GENOMES_MESHOPTIMIZER_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/_deps/meshoptimizer-build" EXCLUDE_FROM_ALL)
    genomes_mark_third_party(meshoptimizer)
    set_property(TARGET meshoptimizer APPEND PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
        "$<BUILD_INTERFACE:${GENOMES_MESHOPTIMIZER_SOURCE_DIR}/src>")
    add_library(genomes::meshoptimizer ALIAS meshoptimizer)
    message(STATUS "Genomes meshoptimizer 1.3: ${GENOMES_MESHOPTIMIZER_PIN}; CPU index-order preparation")
endfunction()
