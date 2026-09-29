include_guard(GLOBAL)
include(GenomesThirdParty)

# This is a reviewed source revision, not a claim of GPU validation.
set(GENOMES_THREEPP_PIN "ad9571cbcbb5e27c4dd582d4810fb0b235534f60")
set(GENOMES_THREEPP_SOURCE_DIR "${CMAKE_SOURCE_DIR}/external/threepp"
    CACHE PATH "Pinned threepp checkout (no automatic download)")

function(genomes_configure_threepp)
    if(NOT GENOMES_ENABLE_THREEPP)
        return()
    endif()
    if(CMAKE_VERSION VERSION_LESS 3.21)
        message(FATAL_ERROR "The pinned threepp integration requires CMake >= 3.21")
    endif()
    if(NOT GENOMES_ENABLE_SDL)
        message(FATAL_ERROR "GENOMES_ENABLE_THREEPP requires GENOMES_ENABLE_SDL=ON")
    endif()
    if(EMSCRIPTEN OR ANDROID OR IOS)
        message(FATAL_ERROR "This migration slice supports desktop SDL3/OpenGL only")
    endif()
    if(TARGET threepp)
        message(FATAL_ERROR "threepp was already configured outside GenomesThreepp.cmake")
    endif()
    foreach(required IN ITEMS CMakeLists.txt src/CMakeLists.txt
            src/threepp/utils/LoadGlad.hpp src/external/glad/glad/glad.h LICENSE)
        if(NOT EXISTS "${GENOMES_THREEPP_SOURCE_DIR}/${required}")
            message(FATAL_ERROR
                "Missing threepp file ${required}. Run: git submodule update --init --recursive external/threepp")
        endif()
    endforeach()
    find_package(Git REQUIRED)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${GENOMES_THREEPP_SOURCE_DIR}" rev-parse HEAD
        RESULT_VARIABLE revision_result OUTPUT_VARIABLE revision
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_VARIABLE revision_error)
    if(NOT revision_result EQUAL 0 OR NOT revision STREQUAL GENOMES_THREEPP_PIN)
        message(FATAL_ERROR
            "Unexpected threepp revision '${revision}'; required ${GENOMES_THREEPP_PIN}. ${revision_error}")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${GENOMES_THREEPP_SOURCE_DIR}" diff --quiet HEAD --
        RESULT_VARIABLE dirty_result)
    if(NOT dirty_result EQUAL 0)
        message(FATAL_ERROR "The pinned threepp checkout has tracked changes or could not be inspected")
    endif()

    # Local variables intentionally stay in this function's scope. In particular,
    # do not force BUILD_SHARED_LIBS or output directories on unrelated modules.
    set(BUILD_SHARED_LIBS OFF)
    set(THREEPP_BUILD_EXAMPLES OFF)
    set(THREEPP_BUILD_EXAMPLE_PROJECTS OFF)
    set(THREEPP_BUILD_TESTS OFF)
    set(THREEPP_BUILD_EDITOR OFF)
    set(THREEPP_WITH_AUDIO OFF)
    set(THREEPP_WITH_GLFW OFF)
    set(THREEPP_USE_EXTERNAL_GLFW OFF)
    set(THREEPP_WITH_VULKAN OFF)
    set(THREEPP_WITH_PYTHON OFF)
    set(THREEPP_WITH_FBX OFF)
    set(THREEPP_WITH_USD OFF)
    set(THREEPP_WITH_RLTOOLS OFF)
    set(THREEPP_WITH_FSR OFF)
    set(THREEPP_WITH_DLSS OFF)
    set(THREEPP_WITH_INFERENCE OFF)
    set(THREEPP_TREAT_WARNINGS_AS_ERRORS OFF)
    add_subdirectory("${GENOMES_THREEPP_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/_deps/threepp-build" EXCLUDE_FROM_ALL)
    if(NOT TARGET threepp::threepp)
        message(FATAL_ERROR "Pinned threepp did not provide threepp::threepp")
    endif()
    genomes_mark_third_party(threepp)
    # Headers are third-party too; keep Werror on Genomes, not upstream headers.
    set_property(TARGET threepp APPEND PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
        "$<BUILD_INTERFACE:${GENOMES_THREEPP_SOURCE_DIR}/include>")
    message(STATUS "Genomes threepp: ${GENOMES_THREEPP_PIN}; static, SDL host, OpenGL, no GLFW/editor/Vulkan")
endfunction()
