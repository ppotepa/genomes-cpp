add_executable(genomes_game main.cpp GameApplication.cpp)
target_link_libraries(genomes_game
PRIVATE
genomes::runtime
genomes::simulation
genomes::platform)

if(GENOMES_RENDER_BACKEND STREQUAL "DILIGENT")
target_link_libraries(genomes_game PRIVATE
    genomes::render_diligent
    genomes::camera
    genomes::math
    genomes::geometry)
else()
    message(FATAL_ERROR "genomes_game requires a presentation backend")
endif()

if(GENOMES_ENABLE_RMLUI)
    target_link_libraries(genomes_game PRIVATE genomes::ui_rml)
    target_compile_definitions(genomes_game PRIVATE GENOMES_HAS_RMLUI=1)
endif()
genomes_set_project_warnings(genomes_game)

find_package(Git QUIET)
set(_genomes_build_sha "unknown")
set(_genomes_build_state "unknown")
if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE _genomes_build_sha OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _genomes_git_sha_result ERROR_QUIET)
    if(NOT _genomes_git_sha_result EQUAL 0)
        set(_genomes_build_sha "unknown")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" diff --quiet HEAD --
        RESULT_VARIABLE _genomes_git_dirty_result ERROR_QUIET)
    if(_genomes_git_dirty_result EQUAL 0)
        set(_genomes_build_state "clean")
    else()
        set(_genomes_build_state "dirty")
    endif()
endif()
target_compile_definitions(genomes_game PRIVATE
    GENOMES_BUILD_GIT_SHA="${_genomes_build_sha}"
    GENOMES_BUILD_STATE="${_genomes_build_state}"
    GENOMES_BUILD_RENDER_PROFILE="${GENOMES_RENDER_BACKEND}")
unset(_genomes_build_sha)
unset(_genomes_build_state)
unset(_genomes_git_sha_result)
unset(_genomes_git_dirty_result)
